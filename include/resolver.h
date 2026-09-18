#ifndef FORGE_RESOLVER_H
#define FORGE_RESOLVER_H

#include <stddef.h>

enum forge_dependency_state {
    FORGE_DEPENDENCY_UNRESOLVED,
    FORGE_DEPENDENCY_RESOLVING,
    FORGE_DEPENDENCY_RESOLVED,
};

struct forge_dependency {
    char *name;
    char *path;

    struct forge_dependency **children;
    size_t                    child_count;

    enum forge_dependency_state state;
};

struct forge_dependency_tree {
    char *interpreter;

    struct forge_dependency **dependencies;
    size_t                    count;
};

int forge_resolve_dependencies(const char                   *binary,
                               struct forge_dependency_tree *tree);

void forge_dependency_tree_free(struct forge_dependency_tree *tree);

#endif
