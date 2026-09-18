#include "rootfs.h"

#include "utils.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
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

    assert(stat("/usr/bin/env", &source_status) == 0);
    assert(S_ISREG(source_status.st_mode));

    assert(forge_rootfs_init(&rootfs, output) == 0);

    assert(forge_rootfs_copy(&rootfs, "/usr/bin/env") == 0);

    assert(stat("tests/rootfs-copy/usr/bin/env", &destination_status) == 0);
    assert(S_ISREG(destination_status.st_mode));

    assert((destination_status.st_mode & 07777) ==
           (source_status.st_mode & 07777));

    forge_rootfs_free(&rootfs);
    remove_rootfs(output);

    test_pass();
}

static void test_preserve_permissions(void)
{
    const char *source = "/tmp/forge-rootfs-source";
    const char *output = "tests/rootfs-copy";

    struct forge_rootfs rootfs;
    struct stat         source_status;
    struct stat         destination_status;

    test_begin("preserve file permissions");

    remove_rootfs(output);
    unlink(source);

    assert(creat(source, 0601) >= 0);
    assert(chmod(source, 0601) == 0);
    assert(stat(source, &source_status) == 0);

    assert(forge_rootfs_init(&rootfs, output) == 0);
    assert(forge_rootfs_copy(&rootfs, source) == 0);

    assert(stat("tests/rootfs-copy/tmp/forge-rootfs-source",
                &destination_status) == 0);

    assert((destination_status.st_mode & 07777) ==
           (source_status.st_mode & 07777));

    forge_rootfs_free(&rootfs);

    unlink(source);
    remove_rootfs(output);

    test_pass();
}

static void test_copy_symlink(void)
{
    const char *source = "/tmp/forge-rootfs-symlink";
    const char *target = "/tmp/forge-rootfs-target";
    const char *output = "tests/rootfs-copy";

    struct forge_rootfs rootfs;
    struct stat         destination_status;

    test_begin("copy symlink into rootfs");

    remove_rootfs(output);
    unlink(source);
    unlink(target);

    int target_fd = creat(target, 0644);

    assert(target_fd >= 0);
    assert(close(target_fd) == 0);
    assert(symlink(target, source) == 0);

    assert(lstat(source, &destination_status) == 0);
    assert(S_ISLNK(destination_status.st_mode));

    assert(forge_rootfs_init(&rootfs, output) == 0);
    assert(forge_rootfs_copy(&rootfs, source) == 0);

    assert(lstat("tests/rootfs-copy/tmp/forge-rootfs-symlink",
                 &destination_status) == 0);
    assert(S_ISLNK(destination_status.st_mode));

    char buffer[256];

    ssize_t length = readlink("tests/rootfs-copy/tmp/forge-rootfs-symlink",
                              buffer, sizeof(buffer) - 1);

    assert(length >= 0);

    buffer[length] = '\0';

    assert(strcmp(buffer, target) == 0);

    forge_rootfs_free(&rootfs);

    unlink(source);
    unlink(target);
    remove_rootfs(output);

    test_pass();
}

static void test_copy_parent_directories(void)
{
    const char *source = "/tmp/forge-rootfs-source";
    const char *output = "tests/rootfs-copy";

    struct forge_rootfs rootfs;
    struct stat         status;

    test_begin("create required parent directories");

    remove_rootfs(output);
    unlink(source);

    int source_fd = creat(source, 0644);

    assert(source_fd >= 0);
    assert(close(source_fd) == 0);

    assert(forge_rootfs_init(&rootfs, output) == 0);
    assert(forge_rootfs_copy(&rootfs, source) == 0);

    assert(stat("tests/rootfs-copy/tmp", &status) == 0);
    assert(S_ISDIR(status.st_mode));

    assert(stat("tests/rootfs-copy/tmp/forge-rootfs-source", &status) == 0);
    assert(S_ISREG(status.st_mode));

    forge_rootfs_free(&rootfs);

    unlink(source);
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
    test_preserve_permissions();
    test_copy_symlink();
    test_copy_parent_directories();
    test_copy_missing_source();
    test_copy_non_regular();
    test_copy_interpreter();
    test_invalid_source();
    test_null_arguments();

    return test_run();
}
