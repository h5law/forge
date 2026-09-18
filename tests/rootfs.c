#include "rootfs.h"
#include "utils.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

static void test_init(void)
{
    struct forge_rootfs rootfs;

    test_begin("initialise rootfs");

    assert(forge_rootfs_init(&rootfs, "/tmp/forge-rootfs") == 0);
    assert(rootfs.path != NULL);
    assert(strcmp(rootfs.path, "/tmp/forge-rootfs") == 0);

    forge_rootfs_free(&rootfs);

    test_pass();
}

static void test_path(void)
{
    struct forge_rootfs rootfs;
    char               *destination = NULL;

    test_begin("map host path into rootfs");

    assert(forge_rootfs_init(&rootfs, "/tmp/forge-rootfs") == 0);

    assert(forge_rootfs_path(&rootfs, "/bin/sh", &destination) == 0);

    assert(strcmp(destination, "/tmp/forge-rootfs/bin/sh") == 0);

    free(destination);
    forge_rootfs_free(&rootfs);

    test_pass();
}

static void test_nested_path(void)
{
    struct forge_rootfs rootfs;
    char               *destination = NULL;

    test_begin("map nested host path into rootfs");

    assert(forge_rootfs_init(&rootfs, "/tmp/forge-rootfs") == 0);

    assert(forge_rootfs_path(&rootfs, "/usr/lib/x86_64-linux-gnu/libc.so.6",
                             &destination) == 0);

    assert(strcmp(destination,
                  "/tmp/forge-rootfs/usr/lib/x86_64-linux-gnu/libc.so.6") == 0);

    free(destination);
    forge_rootfs_free(&rootfs);

    test_pass();
}

static void test_invalid_source(void)
{
    struct forge_rootfs rootfs;
    char               *destination = NULL;

    test_begin("reject relative source path");

    assert(forge_rootfs_init(&rootfs, "/tmp/forge-rootfs") == 0);

    assert(forge_rootfs_path(&rootfs, "bin/sh", &destination) < 0);

    assert(destination == NULL);

    forge_rootfs_free(&rootfs);

    test_pass();
}

static void test_null_arguments(void)
{
    struct forge_rootfs rootfs;
    char               *destination = NULL;

    test_begin("reject null arguments");

    assert(forge_rootfs_init(NULL, "/tmp/forge-rootfs") < 0);
    assert(forge_rootfs_init(&rootfs, NULL) < 0);

    assert(forge_rootfs_path(NULL, "/bin/sh", &destination) < 0);
    assert(forge_rootfs_path(&rootfs, "/bin/sh", NULL) < 0);

    forge_rootfs_free(NULL);

    test_pass();
}

int main(void)
{
    test_init();
    test_path();
    test_nested_path();
    test_invalid_source();
    test_null_arguments();

    return test_run();
}
