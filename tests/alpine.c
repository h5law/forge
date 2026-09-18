#include <alpine.h>

#include "utils.h"

#include <assert.h>
#include <errno.h>
#include <unistd.h>

static void test_parse_valid_version(void)
{
    test_begin("parse valid Alpine version");

    unsigned int major;
    unsigned int minor;

    if (forge_alpine_parse_version("3.24.2", &major, &minor) < 0) {
        test_fail("failed to parse valid version");
        return;
    }

    if (major != 3 || minor != 24) {
        test_fail("parsed version does not match expected value");
        return;
    }

    test_pass();
}

static void test_parse_rejects_missing_patch(void)
{
    test_begin("reject version without patch");

    unsigned int major;
    unsigned int minor;

    if (forge_alpine_parse_version("3.24", &major, &minor) == 0) {
        test_fail("accepted version without patch");
        return;
    }

    test_pass();
}

static void test_parse_rejects_extra_components(void)
{
    test_begin("reject version with extra components");

    unsigned int major;
    unsigned int minor;

    if (forge_alpine_parse_version("3.24.2.1", &major, &minor) == 0) {
        test_fail("accepted version with extra components");
        return;
    }

    test_pass();
}

static void test_parse_rejects_non_numeric_version(void)
{
    test_begin("reject non-numeric version");

    unsigned int major;
    unsigned int minor;

    if (forge_alpine_parse_version("3.x.2", &major, &minor) == 0) {
        test_fail("accepted non-numeric version");
        return;
    }

    test_pass();
}

static void test_parse_rejects_empty_version(void)
{
    test_begin("reject empty version");

    unsigned int major;
    unsigned int minor;

    if (forge_alpine_parse_version("", &major, &minor) == 0) {
        test_fail("accepted empty version");
        return;
    }

    test_pass();
}

static void test_cleanup_on_download_failure(void)
{
    const char *output = "tests/test-rootfs";

    test_begin("remove rootfs after download failure");

    assert(rmdir(output) != 0 || errno == ENOENT);

    assert(forge_alpine_prepare("invalid-version", "x86_64", output) != 0);

    assert(access(output, F_OK) != 0);
    assert(errno == ENOENT);

    test_pass();
}

int main(void)
{
    test_parse_valid_version();
    test_parse_rejects_missing_patch();
    test_parse_rejects_extra_components();
    test_parse_rejects_non_numeric_version();
    test_parse_rejects_empty_version();
    test_cleanup_on_download_failure();

    return test_run();
}
