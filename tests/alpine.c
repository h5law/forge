#include "alpine.h"

#include "utils.h"

#include <assert.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>

static void remove_test_rootfs(void) { rmdir("tests/test-rootfs"); }

static void test_parse_version(void)
{
    test_begin("parse Alpine version");

    unsigned int major;
    unsigned int minor;

    assert(forge_alpine_parse_version("3.24.2", &major, &minor) == 0);
    assert(major == 3);
    assert(minor == 24);

    test_pass();
}

static void test_reject_invalid_version(void)
{
    test_begin("reject invalid Alpine version");

    unsigned int major;
    unsigned int minor;

    assert(forge_alpine_parse_version("invalid-version", &major, &minor) < 0);

    test_pass();
}

static void test_reject_invalid_architecture(void)
{
    test_begin("reject unsupported Alpine architecture");

    assert(forge_alpine_prepare("3.24.2", "invalid",
                                "tests/test-rootfs-invalid-architecture") != 0);

    test_pass();
}

static void test_reject_existing_rootfs(void)
{
    const char *output = "tests/test-rootfs";

    test_begin("reject existing rootfs");

    remove_test_rootfs();

    assert(mkdir(output, 0755) == 0);

    assert(forge_alpine_prepare("3.24.2", "x86_64", output) != 0);

    assert(access(output, F_OK) == 0);

    assert(rmdir(output) == 0);

    test_pass();
}

static void test_cleanup_on_prepare_failure(void)
{
    const char *output = "tests/test-rootfs-failure";

    test_begin("remove rootfs after preparation failure");

    rmdir(output);

    assert(forge_alpine_prepare("invalid-version", "x86_64", output) != 0);

    assert(access(output, F_OK) != 0);
    assert(errno == ENOENT);

    test_pass();
}

static void test_null_arguments(void)
{
    const char *output = "tests/test-rootfs";

    test_begin("reject null arguments");

    remove_test_rootfs();

    assert(forge_alpine_prepare(NULL, "x86_64", output) != 0);
    assert(forge_alpine_prepare("3.24.2", NULL, output) != 0);
    assert(forge_alpine_prepare("3.24.2", "x86_64", NULL) != 0);

    assert(access(output, F_OK) != 0);

    test_pass();
}

int main(void)
{
    test_parse_version();
    test_reject_invalid_version();
    test_reject_invalid_architecture();
    test_reject_existing_rootfs();
    test_cleanup_on_prepare_failure();
    test_null_arguments();

    return test_run();
}
