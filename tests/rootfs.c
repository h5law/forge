#include "rootfs.h"

#include "utils.h"

#include <assert.h>
#include <errno.h>
#include <ftw.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int remove_entry(const char *path, const struct stat *status, int type,
                        struct FTW *buffer)
{
    ( void )status;
    ( void )type;
    ( void )buffer;

    return remove(path);
}

static void remove_rootfs(const char *path)
{
    nftw(path, remove_entry, 64, FTW_DEPTH | FTW_PHYS);
}

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

    assert(forge_rootfs_path(&rootfs, "/usr/lib/libc.so.6", &destination) == 0);

    assert(strcmp(destination, "/tmp/forge-rootfs/usr/lib/libc.so.6") == 0);

    free(destination);
    forge_rootfs_free(&rootfs);

    test_pass();
}

static void test_copy_binary(void)
{
    const char *output = "tests/rootfs-copy";

    struct forge_rootfs rootfs;
    struct stat         source_status;
    struct stat         destination_status;

    test_begin("copy binary into rootfs");

    remove_rootfs(output);

    assert(stat("/bin/sh", &source_status) == 0);

    assert(forge_rootfs_init(&rootfs, output) == 0);

    assert(forge_rootfs_copy(&rootfs, "/bin/sh") == 0);

    assert(stat("tests/rootfs-copy/bin/sh", &destination_status) == 0);
    assert(S_ISREG(destination_status.st_mode));

    assert((destination_status.st_mode & 07777) ==
           (source_status.st_mode & 07777));

    forge_rootfs_free(&rootfs);
    remove_rootfs(output);

    test_pass();
}

static void test_copy_nested_binary(void)
{
    const char *output = "tests/rootfs-copy";

    struct forge_rootfs rootfs;

    test_begin("copy binary with parent directories");

    remove_rootfs(output);

    assert(forge_rootfs_init(&rootfs, output) == 0);

    assert(forge_rootfs_copy(&rootfs, "/usr/bin/env") == 0);

    assert(access("tests/rootfs-copy/usr/bin/env", F_OK) == 0);

    forge_rootfs_free(&rootfs);
    remove_rootfs(output);

    test_pass();
}

static void test_copy_missing_source(void)
{
    struct forge_rootfs rootfs;

    test_begin("reject missing source");

    assert(forge_rootfs_init(&rootfs, "tests/rootfs-copy") == 0);

    assert(forge_rootfs_copy(&rootfs, "/does/not/exist") < 0);

    forge_rootfs_free(&rootfs);
    remove_rootfs("tests/rootfs-copy");

    test_pass();
}

static void test_copy_non_regular(void)
{
    struct forge_rootfs rootfs;

    test_begin("reject non-regular source");

    assert(forge_rootfs_init(&rootfs, "tests/rootfs-copy") == 0);

    assert(forge_rootfs_copy(&rootfs, "/tmp") < 0);

    forge_rootfs_free(&rootfs);
    remove_rootfs("tests/rootfs-copy");

    test_pass();
}

static void test_copy_interpreter(void)
{
    struct forge_rootfs rootfs;
    const char         *interpreter = "/lib64/ld-linux-x86-64.so.2";

    test_begin("copy dynamic linker into rootfs");

    if (access(interpreter, F_OK) != 0) {
        test_skip();
        return;
    }

    assert(forge_rootfs_init(&rootfs, "tests/rootfs-copy") == 0);

    assert(forge_rootfs_copy(&rootfs, interpreter) == 0);
    assert(access("tests/rootfs-copy/lib64/ld-linux-x86-64.so.2", F_OK) == 0);

    forge_rootfs_free(&rootfs);

    test_pass();
}

static void test_invalid_source(void)
{
    struct forge_rootfs rootfs;

    test_begin("reject relative source path");

    assert(forge_rootfs_init(&rootfs, "tests/rootfs-copy") == 0);

    errno = 0;

    assert(forge_rootfs_copy(&rootfs, "bin/sh") < 0);
    assert(errno == EINVAL);

    forge_rootfs_free(&rootfs);
    remove_rootfs("tests/rootfs-copy");

    test_pass();
}

static void test_null_arguments(void)
{
    struct forge_rootfs rootfs;

    test_begin("reject null arguments");

    assert(forge_rootfs_init(NULL, "/tmp/forge-rootfs") < 0);
    assert(forge_rootfs_init(&rootfs, NULL) < 0);

    assert(forge_rootfs_path(NULL, "/bin/sh", NULL) < 0);
    assert(forge_rootfs_copy(NULL, "/bin/sh") < 0);

    forge_rootfs_free(NULL);

    test_pass();
}

int main(void)
{
    test_init();
    test_path();
    test_nested_path();
    test_copy_binary();
    test_copy_nested_binary();
    test_copy_missing_source();
    test_copy_non_regular();
    test_copy_interpreter();
    test_invalid_source();
    test_null_arguments();

    return test_run();
}
