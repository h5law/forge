#include <resolver.h>

#include "utils.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_resolve_dependencies(void)
{
    test_begin("resolve recursive dependencies");

    struct forge_dependency_tree tree;

    assert(forge_resolve_dependencies("/bin/ls", &tree) == 0);
    assert(tree.count > 0);

    forge_dependency_tree_free(&tree);

    test_pass();
}

static void test_resolve_interpreter(void)
{
    test_begin("resolve dynamic linker");

    struct forge_dependency_tree tree;

    assert(forge_resolve_dependencies("/bin/ls", &tree) == 0);
    assert(tree.interpreter != NULL);
    assert(tree.interpreter[0] == '/');

    forge_dependency_tree_free(&tree);

    test_pass();
}

static void test_interpreter_not_dependency(void)
{
    test_begin("keep dynamic linker separate");

    struct forge_dependency_tree tree;

    assert(forge_resolve_dependencies("/bin/ls", &tree) == 0);
    assert(tree.interpreter != NULL);

    for (size_t i = 0; i < tree.count; ++i) {
        assert(strcmp(tree.dependencies[i]->name, "ld-linux-x86-64.so.2") != 0);

        assert(strcmp(tree.dependencies[i]->path, tree.interpreter) != 0);
    }

    forge_dependency_tree_free(&tree);

    test_pass();
}

static void test_dependencies_have_paths(void)
{
    test_begin("dependencies have resolved paths");

    struct forge_dependency_tree tree;

    assert(forge_resolve_dependencies("/bin/ls", &tree) == 0);
    assert(tree.count > 0);

    for (size_t i = 0; i < tree.count; ++i) {
        assert(tree.dependencies[i] != NULL);
        assert(tree.dependencies[i]->name != NULL);
        assert(tree.dependencies[i]->path != NULL);
        assert(tree.dependencies[i]->path[0] == '/');
    }

    forge_dependency_tree_free(&tree);

    test_pass();
}

static void test_recursive_dependencies(void)
{
    test_begin("resolve nested dependencies");

    struct forge_dependency_tree tree;

    assert(forge_resolve_dependencies("/bin/sh", &tree) == 0);

    int has_children = 0;

    for (size_t i = 0; i < tree.count; ++i) {
        if (tree.dependencies[i]->child_count > 0) {
            has_children = 1;
            break;
        }
    }

    assert(has_children);

    forge_dependency_tree_free(&tree);

    test_pass();
}

static void test_resolve_missing_binary(void)
{
    test_begin("reject missing binary");

    struct forge_dependency_tree tree;

    assert(forge_resolve_dependencies("/does/not/exist", &tree) < 0);

    test_pass();
}

static void test_resolve_null_binary(void)
{
    test_begin("reject null binary");

    struct forge_dependency_tree tree;

    assert(forge_resolve_dependencies(NULL, &tree) < 0);

    test_pass();
}

static void test_resolve_null_tree(void)
{
    test_begin("reject null tree");

    assert(forge_resolve_dependencies("/bin/ls", NULL) < 0);

    test_pass();
}

int main(void)
{
    puts("forge resolver tests");
    puts("====================");
    puts("");

    test_resolve_dependencies();
    test_resolve_interpreter();
    test_interpreter_not_dependency();
    test_dependencies_have_paths();
    test_recursive_dependencies();
    test_resolve_missing_binary();
    test_resolve_null_binary();
    test_resolve_null_tree();

    puts("");

    return test_run();
}
