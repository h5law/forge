#include <alpine.h>
#include <config.h>
#include <elf_parser.h>
#include <resolver.h>
#include <rootfs.h>

#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FORGE_VERSION "0.2.0-alpha"

enum command {
    COMMAND_NONE,
    COMMAND_BUILD,
    COMMAND_VALIDATE,
    COMMAND_DEPS,
};

static void usage(FILE *stream, const char *program)
{
    fprintf(stream,
            "usage: %s <command> [options] [config]\n"
            "\n"
            "Build and manage minimal Linux root filesystems.\n"
            "\n"
            "Commands:\n"
            "  build      Build a rootfs from a configuration\n"
            "  validate   Parse and validate a configuration\n"
            "  deps       Show ELF dependencies for configured binaries\n"
            "\n"
            "Options:\n"
            "  -h, --help       Show this help message\n"
            "  -V, --version    Show version information\n",
            program);
}

static void command_usage(FILE *stream, const char *program,
                          enum command command)
{
    switch (command) {
    case COMMAND_BUILD:
        fprintf(stream,
                "usage: %s build [options] <config>\n"
                "\n"
                "Build a rootfs from a configuration file.\n"
                "\n"
                "Options:\n"
                "  -h, --help       Show this help message\n"
                "  -V, --version    Show version information\n",
                program);
        break;

    case COMMAND_VALIDATE:
        fprintf(stream,
                "usage: %s validate [options] <config>\n"
                "\n"
                "Parse and validate a configuration file.\n"
                "\n"
                "Options:\n"
                "  -h, --help       Show this help message\n"
                "  -V, --version    Show version information\n",
                program);
        break;

    case COMMAND_DEPS:
        fprintf(stream,
                "usage: %s deps [options] <config>\n"
                "\n"
                "Show ELF dependencies for configured binaries.\n"
                "\n"
                "Options:\n"
                "  -h, --help       Show this help message\n"
                "  -V, --version    Show version information\n",
                program);
        break;

    case COMMAND_NONE:
        usage(stream, program);
        break;
    }
}

static void version(void) { printf("forge %s\n", FORGE_VERSION); }

static enum command parse_command(const char *command)
{
    if (strcmp(command, "build") == 0)
        return COMMAND_BUILD;

    if (strcmp(command, "validate") == 0)
        return COMMAND_VALIDATE;

    if (strcmp(command, "deps") == 0)
        return COMMAND_DEPS;

    return COMMAND_NONE;
}

static int run_validate(const char *path)
{
    struct forge_config config;

    if (forge_config_parse(path, &config) < 0)
        return EXIT_FAILURE;

    forge_config_free(&config);

    printf("configuration is valid: %s\n", path);

    return EXIT_SUCCESS;
}

static int run_build(const char *path)
{
    struct forge_config config;

    if (forge_config_parse(path, &config) < 0)
        return EXIT_FAILURE;

    struct forge_rootfs rootfs;

    if (forge_alpine_prepare(config.base.version, config.base.architecture,
                             config.rootfs.output) < 0) {
        forge_config_free(&config);
        return EXIT_FAILURE;
    }

    if (forge_rootfs_init(&rootfs, config.rootfs.output) < 0) {
        fprintf(stderr, "failed to initialise rootfs: %s\n", strerror(errno));
        forge_config_free(&config);
        return EXIT_FAILURE;
    }

    int result = EXIT_SUCCESS;

    for (size_t i = 0; i < config.binaries.count; ++i) {
        const char *binary = config.binaries.paths[i];

        struct forge_dependency_tree tree;

        if (forge_resolve_dependencies(binary, &tree) < 0) {
            result = EXIT_FAILURE;
            break;
        }

        if (forge_rootfs_copy(&rootfs, binary) < 0) {
            forge_dependency_tree_free(&tree);
            result = EXIT_FAILURE;
            break;
        }

        for (size_t j = 0; j < tree.count; ++j) {
            if (forge_rootfs_copy(&rootfs, tree.dependencies[j]->path) < 0) {
                result = EXIT_FAILURE;
                break;
            }
        }

        forge_dependency_tree_free(&tree);

        if (result != EXIT_SUCCESS)
            break;
    }

    forge_rootfs_free(&rootfs);
    forge_config_free(&config);

    return result;
}

static void print_dependency_tree(const struct forge_dependency *dependency,
                                  const char *prefix, int last)
{
    printf("%s%s%s\n", prefix, last ? "└── " : "├── ", dependency->name);

    size_t prefix_length = strlen(prefix);

    char *child_prefix   = malloc(prefix_length + 5);

    if (child_prefix == NULL)
        return;

    memcpy(child_prefix, prefix, prefix_length);

    if (last)
        memcpy(child_prefix + prefix_length, "    ", 4);
    else
        memcpy(child_prefix + prefix_length, "│   ", 4);

    child_prefix[prefix_length + 4] = '\0';

    for (size_t i = 0; i < dependency->child_count; ++i) {
        print_dependency_tree(dependency->children[i], child_prefix,
                              i + 1 == dependency->child_count);
    }

    free(child_prefix);
}

static int dependency_is_child(const struct forge_dependency_tree *tree,
                               const struct forge_dependency      *dependency)
{
    for (size_t i = 0; i < tree->count; ++i) {
        const struct forge_dependency *candidate = tree->dependencies[i];

        for (size_t j = 0; j < candidate->child_count; ++j) {
            if (candidate->children[j] == dependency)
                return 1;
        }
    }

    return 0;
}

static int run_deps(const char *path)
{
    struct forge_config config;

    if (forge_config_parse(path, &config) < 0)
        return EXIT_FAILURE;

    int result = EXIT_SUCCESS;

    for (size_t i = 0; i < config.binaries.count; ++i) {
        const char *binary = config.binaries.paths[i];

        struct forge_dependency_tree tree;

        if (forge_resolve_dependencies(binary, &tree) < 0) {
            result = EXIT_FAILURE;
            continue;
        }

        printf("%s\n", binary);

        if (tree.interpreter != NULL)
            printf("├── interpreter: %s\n", tree.interpreter);

        size_t root_count = 0;

        for (size_t j = 0; j < tree.count; ++j) {
            if (!dependency_is_child(&tree, tree.dependencies[j]))
                ++root_count;
        }

        size_t root_index = 0;

        for (size_t j = 0; j < tree.count; ++j) {
            struct forge_dependency *dependency = tree.dependencies[j];

            if (dependency_is_child(&tree, dependency))
                continue;

            ++root_index;

            print_dependency_tree(dependency, "", root_index == root_count);
        }

        printf("\n");

        forge_dependency_tree_free(&tree);
    }

    forge_config_free(&config);

    return result;
}

int main(int argc, char **argv)
{
    const char *program                  = argv[0];

    static const struct option options[] = {
            {
             .name    = "help",
             .has_arg = no_argument,
             .flag    = NULL,
             .val     = 'h',
             },
            {
             .name    = "version",
             .has_arg = no_argument,
             .flag    = NULL,
             .val     = 'V',
             },
            {
             .name    = NULL,
             .has_arg = 0,
             .flag    = NULL,
             .val     = 0,
             },
    };

    if (argc < 2) {
        usage(stderr, argv[0]);
        return EXIT_FAILURE;
    }

    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        usage(stdout, argv[0]);
        return EXIT_SUCCESS;
    }

    if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-V") == 0) {
        version();
        return EXIT_SUCCESS;
    }

    enum command command = parse_command(argv[1]);

    if (command == COMMAND_NONE) {
        fprintf(stderr, "%s: unknown command: %s\n\n", argv[0], argv[1]);

        usage(stderr, argv[0]);

        return EXIT_FAILURE;
    }

    argc   -= 1;
    argv   += 1;

    optind  = 1;

    int option;

    while ((option = getopt_long(argc, argv, "hV", options, NULL)) != -1) {
        switch (option) {
        case 'h':
            command_usage(stdout, program, command);
            return EXIT_SUCCESS;

        case 'V':
            version();
            return EXIT_SUCCESS;

        default:
            command_usage(stderr, argv[-1], command);
            return EXIT_FAILURE;
        }
    }

    if (argc - optind != 1) {
        fprintf(stderr, "error: expected exactly one configuration file\n\n");

        command_usage(stderr, argv[-1], command);

        return EXIT_FAILURE;
    }

    const char *config_path = argv[optind];

    switch (command) {
    case COMMAND_BUILD:
        return run_build(config_path);

    case COMMAND_VALIDATE:
        return run_validate(config_path);

    case COMMAND_DEPS:
        return run_deps(config_path);

    case COMMAND_NONE:
        break;
    }

    return EXIT_FAILURE;
}
