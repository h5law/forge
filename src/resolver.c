#include <resolver.h>
#include <elf_parser.h>

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *library_paths[] = {
        "/lib",
        "/lib64",
        "/usr/lib",
        "/usr/lib64",
};

static int elf_is_compatible(const struct forge_elf *elf,
                             unsigned int elf_class, unsigned int machine)
{
    return elf->elf_class == elf_class && elf->machine == machine;
}

static char *resolve_library(const char *name, unsigned int elf_class,
                             unsigned int machine)
{
    if (name == NULL || name[0] == '\0')
        return NULL;

    size_t library_path_count =
            sizeof(library_paths) / sizeof(library_paths[0]);

    size_t name_length = strlen(name);

    for (size_t i = 0; i < library_path_count; ++i) {
        const char *directory   = library_paths[i];

        size_t directory_length = strlen(directory);

        if (directory_length > SIZE_MAX - name_length - 2)
            return NULL;

        size_t length = directory_length + 1 + name_length + 1;

        char *path    = malloc(length);

        if (path == NULL) {
            fprintf(stderr, "failed to allocate library path: %s\n",
                    strerror(errno));
            return NULL;
        }

        snprintf(path, length, "%s/%s", directory, name);

        struct forge_elf elf;

        /*
         * Candidate probing is deliberately quiet. A file can exist
         * but be an incompatible ELF, so parser failure is not itself
         * an error during library lookup.
         */
        if (forge_elf_parse_quiet(path, &elf) < 0) {
            free(path);
            continue;
        }

        int compatible = elf_is_compatible(&elf, elf_class, machine);

        forge_elf_free(&elf);

        if (compatible)
            return path;

        free(path);
    }

    return NULL;
}

static struct forge_dependency *dependency_create(const char *name,
                                                  const char *path)
{
    struct forge_dependency *dependency = calloc(1, sizeof(*dependency));

    if (dependency == NULL) {
        fprintf(stderr, "failed to allocate dependency: %s\n", strerror(errno));
        return NULL;
    }

    dependency->name = strdup(name);

    if (dependency->name == NULL) {
        fprintf(stderr, "failed to allocate dependency name: %s\n",
                strerror(errno));
        free(dependency);
        return NULL;
    }

    dependency->path = strdup(path);

    if (dependency->path == NULL) {
        fprintf(stderr, "failed to allocate dependency path: %s\n",
                strerror(errno));
        free(dependency->name);
        free(dependency);
        return NULL;
    }

    dependency->state = FORGE_DEPENDENCY_UNRESOLVED;

    return dependency;
}

static void dependency_free(struct forge_dependency *dependency)
{
    if (dependency == NULL)
        return;

    free(dependency->children);
    free(dependency->name);
    free(dependency->path);
    free(dependency);
}

static int dependency_add_child(struct forge_dependency *dependency,
                                struct forge_dependency *child)
{
    for (size_t i = 0; i < dependency->child_count; ++i) {
        if (dependency->children[i] == child)
            return 0;
    }

    size_t new_count = dependency->child_count + 1;

    if (new_count > SIZE_MAX / sizeof(*dependency->children))
        return -1;

    struct forge_dependency **children =
            realloc(dependency->children, new_count * sizeof(*children));

    if (children == NULL)
        return -1;

    dependency->children                          = children;
    dependency->children[dependency->child_count] = child;
    dependency->child_count                       = new_count;

    return 0;
}

static int tree_add_dependency(struct forge_dependency_tree *tree,
                               struct forge_dependency      *dependency)
{
    size_t new_count = tree->count + 1;

    if (new_count > SIZE_MAX / sizeof(*tree->dependencies))
        return -1;

    struct forge_dependency **dependencies =
            realloc(tree->dependencies, new_count * sizeof(*dependencies));

    if (dependencies == NULL)
        return -1;

    tree->dependencies              = dependencies;
    tree->dependencies[tree->count] = dependency;
    tree->count                     = new_count;

    return 0;
}

static struct forge_dependency *
tree_find_dependency(const struct forge_dependency_tree *tree, const char *path)
{
    for (size_t i = 0; i < tree->count; ++i) {
        if (strcmp(tree->dependencies[i]->path, path) == 0)
            return tree->dependencies[i];
    }

    return NULL;
}

static int resolve_dependency(struct forge_dependency *dependency,
                              unsigned int elf_class, unsigned int machine,
                              const char                   *interpreter,
                              struct forge_dependency_tree *tree)
{
    if (dependency->state == FORGE_DEPENDENCY_RESOLVED)
        return 0;

    /*
     * Encountering a currently resolving node means we found a cycle.
     * The edge already exists in the graph, so stop traversing here.
     */
    if (dependency->state == FORGE_DEPENDENCY_RESOLVING)
        return 0;

    dependency->state = FORGE_DEPENDENCY_RESOLVING;

    struct forge_elf elf;

    if (forge_elf_parse(dependency->path, &elf) < 0) {
        dependency->state = FORGE_DEPENDENCY_UNRESOLVED;
        return -1;
    }

    if (!elf_is_compatible(&elf, elf_class, machine)) {
        fprintf(stderr, "dependency '%s' has incompatible ELF architecture\n",
                dependency->path);

        forge_elf_free(&elf);
        dependency->state = FORGE_DEPENDENCY_UNRESOLVED;
        return -1;
    }

    for (size_t i = 0; i < elf.needed_count; ++i) {
        const char *name = elf.needed[i];

        char *resolved   = resolve_library(name, elf_class, machine);

        if (resolved == NULL) {
            fprintf(stderr, "failed to resolve dependency '%s' of '%s'\n", name,
                    dependency->path);

            forge_elf_free(&elf);
            dependency->state = FORGE_DEPENDENCY_UNRESOLVED;
            return -1;
        }

        /*
         * PT_INTERP is represented separately from DT_NEEDED.
         * Do not add the interpreter to the dependency graph even if
         * an object happens to list it in DT_NEEDED.
         */
        if (interpreter != NULL && strcmp(resolved, interpreter) == 0) {
            free(resolved);
            continue;
        }

        struct forge_dependency *child = tree_find_dependency(tree, resolved);

        if (child == NULL) {
            child = dependency_create(name, resolved);

            if (child == NULL) {
                free(resolved);
                forge_elf_free(&elf);
                dependency->state = FORGE_DEPENDENCY_UNRESOLVED;
                return -1;
            }

            if (tree_add_dependency(tree, child) < 0) {
                dependency_free(child);
                free(resolved);
                forge_elf_free(&elf);
                dependency->state = FORGE_DEPENDENCY_UNRESOLVED;
                return -1;
            }
        }

        free(resolved);

        if (dependency_add_child(dependency, child) < 0) {
            forge_elf_free(&elf);
            dependency->state = FORGE_DEPENDENCY_UNRESOLVED;
            return -1;
        }

        if (resolve_dependency(child, elf_class, machine, interpreter, tree) <
            0) {
            forge_elf_free(&elf);
            dependency->state = FORGE_DEPENDENCY_UNRESOLVED;
            return -1;
        }
    }

    forge_elf_free(&elf);

    dependency->state = FORGE_DEPENDENCY_RESOLVED;

    return 0;
}

static int resolve_interpreter(const struct forge_elf       *elf,
                               struct forge_dependency_tree *tree)
{
    if (elf->interpreter == NULL)
        return 0;

    struct forge_elf interpreter;

    if (forge_elf_parse(elf->interpreter, &interpreter) < 0) {
        fprintf(stderr, "failed to parse dynamic linker '%s'\n",
                elf->interpreter);
        return -1;
    }

    if (!elf_is_compatible(&interpreter, elf->elf_class, elf->machine)) {
        fprintf(stderr,
                "dynamic linker '%s' has incompatible ELF architecture\n",
                elf->interpreter);

        forge_elf_free(&interpreter);
        return -1;
    }

    forge_elf_free(&interpreter);

    tree->interpreter = strdup(elf->interpreter);

    if (tree->interpreter == NULL) {
        fprintf(stderr, "failed to allocate dynamic linker path: %s\n",
                strerror(errno));
        return -1;
    }

    return 0;
}

int forge_resolve_dependencies(const char                   *binary,
                               struct forge_dependency_tree *tree)
{
    if (binary == NULL || tree == NULL)
        return -1;

    memset(tree, 0, sizeof(*tree));

    struct forge_elf elf;

    if (forge_elf_parse(binary, &elf) < 0)
        return -1;

    unsigned int elf_class = elf.elf_class;
    unsigned int machine   = elf.machine;

    if (!elf.dynamic) {
        forge_elf_free(&elf);
        return 0;
    }

    if (resolve_interpreter(&elf, tree) < 0) {
        forge_elf_free(&elf);
        forge_dependency_tree_free(tree);
        return -1;
    }

    for (size_t i = 0; i < elf.needed_count; ++i) {
        const char *name = elf.needed[i];

        char *resolved   = resolve_library(name, elf_class, machine);

        if (resolved == NULL) {
            fprintf(stderr, "failed to resolve dependency '%s' of '%s'\n", name,
                    binary);

            forge_elf_free(&elf);
            forge_dependency_tree_free(tree);
            return -1;
        }

        /*
         * Keep PT_INTERP separate from the dependency graph.
         */
        if (tree->interpreter != NULL &&
            strcmp(resolved, tree->interpreter) == 0) {
            free(resolved);
            continue;
        }

        struct forge_dependency *dependency =
                tree_find_dependency(tree, resolved);

        if (dependency == NULL) {
            dependency = dependency_create(name, resolved);

            if (dependency == NULL) {
                free(resolved);
                forge_elf_free(&elf);
                forge_dependency_tree_free(tree);
                return -1;
            }

            if (tree_add_dependency(tree, dependency) < 0) {
                dependency_free(dependency);
                free(resolved);
                forge_elf_free(&elf);
                forge_dependency_tree_free(tree);
                return -1;
            }
        }

        free(resolved);

        if (resolve_dependency(dependency, elf_class, machine,
                               tree->interpreter, tree) < 0) {
            forge_elf_free(&elf);
            forge_dependency_tree_free(tree);
            return -1;
        }
    }

    forge_elf_free(&elf);

    return 0;
}

void forge_dependency_tree_free(struct forge_dependency_tree *tree)
{
    if (tree == NULL)
        return;

    free(tree->interpreter);

    for (size_t i = 0; i < tree->count; ++i)
        dependency_free(tree->dependencies[i]);

    free(tree->dependencies);

    tree->interpreter  = NULL;
    tree->dependencies = NULL;
    tree->count        = 0;
}
