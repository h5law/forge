#include <forge/config.h>

#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static unsigned tests_run;
static unsigned tests_passed;

static void test_begin(const char *name)
{
    printf("  %-50s ", name);
    fflush(stdout);

    ++tests_run;
}

static void test_pass(void)
{
    puts("[PASS]");
    ++tests_passed;
}

static void write_fixture(const char *path, const char *contents)
{
    FILE *file = fopen(path, "w");

    assert(file != NULL);
    assert(fputs(contents, file) >= 0);
    assert(fclose(file) == 0);
}

static void test_basic_config(void)
{
    const char *path = "tests/fixtures.toml";

    test_begin("parse basic configuration");

    write_fixture(path, "[base]\n"
                        "distribution = \"alpine\"\n"
                        "version = \"3.22\"\n"
                        "architecture = \"x86_64\"\n"
                        "\n"
                        "[rootfs]\n"
                        "output = \"./rootfs\"\n"
                        "\n"
                        "[binaries]\n"
                        "paths = [\n"
                        "    \"/bin/sh\",\n"
                        "    \"/usr/bin/curl\",\n"
                        "    \"/usr/bin/git\",\n"
                        "]\n");

    struct forge_config config;

    assert(forge_config_parse(path, &config) == 0);

    assert(strcmp(config.base.distribution, "alpine") == 0);
    assert(strcmp(config.base.version, "3.22") == 0);
    assert(strcmp(config.base.architecture, "x86_64") == 0);

    assert(strcmp(config.rootfs.output, "./rootfs") == 0);

    assert(config.binaries.count == 3);
    assert(strcmp(config.binaries.paths[0], "/bin/sh") == 0);
    assert(strcmp(config.binaries.paths[1], "/usr/bin/curl") == 0);
    assert(strcmp(config.binaries.paths[2], "/usr/bin/git") == 0);

    forge_config_free(&config);
    assert(remove(path) == 0);

    test_pass();
}

static void test_trailing_comma(void)
{
    const char *path = "tests/fixtures.toml";

    test_begin("parse trailing comma in array");

    write_fixture(path, "[base]\n"
                        "distribution = \"alpine\"\n"
                        "version = \"3.22\"\n"
                        "architecture = \"x86_64\"\n"
                        "\n"
                        "[rootfs]\n"
                        "output = \"./rootfs\"\n"
                        "\n"
                        "[binaries]\n"
                        "paths = [\n"
                        "    \"/bin/sh\",\n"
                        "    \"/usr/bin/curl\",\n"
                        "]\n");

    struct forge_config config;

    assert(forge_config_parse(path, &config) == 0);

    assert(config.binaries.count == 2);
    assert(strcmp(config.binaries.paths[0], "/bin/sh") == 0);
    assert(strcmp(config.binaries.paths[1], "/usr/bin/curl") == 0);

    forge_config_free(&config);
    assert(remove(path) == 0);

    test_pass();
}

static void test_inline_array(void)
{
    const char *path = "tests/fixtures.toml";

    test_begin("parse inline array");

    write_fixture(path, "[base]\n"
                        "distribution = \"alpine\"\n"
                        "version = \"3.22\"\n"
                        "architecture = \"x86_64\"\n"
                        "\n"
                        "[rootfs]\n"
                        "output = \"./rootfs\"\n"
                        "\n"
                        "[binaries]\n"
                        "paths = [\"/bin/sh\", \"/bin/ls\",]\n");

    struct forge_config config;

    assert(forge_config_parse(path, &config) == 0);

    assert(config.binaries.count == 2);
    assert(strcmp(config.binaries.paths[0], "/bin/sh") == 0);
    assert(strcmp(config.binaries.paths[1], "/bin/ls") == 0);

    forge_config_free(&config);
    assert(remove(path) == 0);

    test_pass();
}

static void test_comments(void)
{
    const char *path = "tests/fixtures.toml";

    test_begin("parse comments and whitespace");

    write_fixture(path, "# forge configuration\n"
                        "\n"
                        "[base] # base image\n"
                        "distribution = \"alpine\" # distribution\n"
                        "version = \"3.22\"\n"
                        "architecture = \"x86_64\"\n"
                        "\n"
                        "[rootfs]\n"
                        "output = \"./rootfs\"\n"
                        "\n"
                        "[binaries]\n"
                        "paths = [\n"
                        "    \"/bin/sh\", # shell\n"
                        "    \"/bin/ls\", # ls\n"
                        "]\n");

    struct forge_config config;

    assert(forge_config_parse(path, &config) == 0);

    assert(strcmp(config.base.distribution, "alpine") == 0);
    assert(strcmp(config.rootfs.output, "./rootfs") == 0);

    assert(config.binaries.count == 2);
    assert(strcmp(config.binaries.paths[0], "/bin/sh") == 0);
    assert(strcmp(config.binaries.paths[1], "/bin/ls") == 0);

    forge_config_free(&config);
    assert(remove(path) == 0);

    test_pass();
}

static void test_string_escapes(void)
{
    const char *path = "tests/fixtures.toml";

    test_begin("parse basic string escapes");

    write_fixture(path, "[base]\n"
                        "distribution = \"alpine\"\n"
                        "version = \"3.22\"\n"
                        "architecture = \"x86_64\"\n"
                        "\n"
                        "[rootfs]\n"
                        "output = \"./root\\tfs\"\n"
                        "\n"
                        "[binaries]\n"
                        "paths = [\"/bin/sh\"]\n");

    struct forge_config config;

    assert(forge_config_parse(path, &config) == 0);
    assert(strcmp(config.rootfs.output, "./root\tfs") == 0);

    forge_config_free(&config);

    assert(remove(path) == 0);

    test_pass();
}

static void test_missing_required_key(void)
{
    const char *path = "tests/fixtures.toml";

    test_begin("reject missing required key");

    write_fixture(path, "[base]\n"
                        "distribution = \"alpine\"\n"
                        "version = \"3.22\"\n"
                        "\n"
                        "[rootfs]\n"
                        "output = \"./rootfs\"\n"
                        "\n"
                        "[binaries]\n"
                        "paths = [\"/bin/sh\"]\n");

    struct forge_config config;

    assert(forge_config_parse(path, &config) != 0);

    forge_config_free(&config);
    assert(remove(path) == 0);

    test_pass();
}

static void test_relative_binary(void)
{
    const char *path = "tests/fixtures.toml";

    test_begin("reject relative binary path");

    write_fixture(path, "[base]\n"
                        "distribution = \"alpine\"\n"
                        "version = \"3.22\"\n"
                        "architecture = \"x86_64\"\n"
                        "\n"
                        "[rootfs]\n"
                        "output = \"./rootfs\"\n"
                        "\n"
                        "[binaries]\n"
                        "paths = [\"bin/sh\"]\n");

    struct forge_config config;

    assert(forge_config_parse(path, &config) != 0);

    forge_config_free(&config);
    assert(remove(path) == 0);

    test_pass();
}

static void test_unknown_section(void)
{
    const char *path = "tests/fixtures.toml";

    test_begin("reject unknown section");

    write_fixture(path, "[unknown]\n"
                        "value = \"test\"\n");

    struct forge_config config;

    assert(forge_config_parse(path, &config) != 0);

    forge_config_free(&config);
    assert(remove(path) == 0);

    test_pass();
}

static void test_duplicate_key(void)
{
    const char *path = "tests/fixtures.toml";

    test_begin("reject duplicate key");

    write_fixture(path, "[base]\n"
                        "distribution = \"alpine\"\n"
                        "distribution = \"debian\"\n"
                        "version = \"3.22\"\n"
                        "architecture = \"x86_64\"\n"
                        "\n"
                        "[rootfs]\n"
                        "output = \"./rootfs\"\n"
                        "\n"
                        "[binaries]\n"
                        "paths = [\"/bin/sh\"]\n");

    struct forge_config config;

    assert(forge_config_parse(path, &config) != 0);

    forge_config_free(&config);
    assert(remove(path) == 0);

    test_pass();
}

static void test_unterminated_array(void)
{
    const char *path = "tests/fixtures.toml";

    test_begin("reject unterminated array");

    write_fixture(path, "[base]\n"
                        "distribution = \"alpine\"\n"
                        "version = \"3.22\"\n"
                        "architecture = \"x86_64\"\n"
                        "\n"
                        "[rootfs]\n"
                        "output = \"./rootfs\"\n"
                        "\n"
                        "[binaries]\n"
                        "paths = [\n"
                        "    \"/bin/sh\",\n");

    struct forge_config config;

    assert(forge_config_parse(path, &config) != 0);

    forge_config_free(&config);
    assert(remove(path) == 0);

    test_pass();
}

static void test_unsupported_distribution(void)
{
    test_begin("reject unsupported distribution");

    struct forge_config config = {
            .base.distribution = "debian",
            .base.version      = "13",
            .base.architecture = "x86_64",
            .rootfs.output     = "./rootfs",
            .binaries.paths    = NULL,
            .binaries.count    = 1,
    };

    char *binary          = "/bin/sh";
    config.binaries.paths = &binary;

    assert(forge_config_validate(&config) != 0);

    test_pass();
}

static void test_unsupported_architecture(void)
{
    test_begin("reject unsupported architecture");

    struct forge_config config = {
            .base.distribution = "alpine",
            .base.version      = "3.22",
            .base.architecture = "aarch64",
            .rootfs.output     = "./rootfs",
            .binaries.paths    = NULL,
            .binaries.count    = 1,
    };

    char *binary          = "/bin/sh";
    config.binaries.paths = &binary;

    assert(forge_config_validate(&config) != 0);

    test_pass();
}

static void test_valid_config(void)
{
    test_begin("validate valid configuration");

    char *binaries[] = {
            "/bin/sh",
            "/usr/bin/curl",
    };

    struct forge_config config = {
            .base =
                    {
                           .distribution = "alpine",
                           .version      = "3.22",
                           .architecture = "x86_64",
                           },
            .rootfs =
                    {
                           .output = "./rootfs",
                           },
            .binaries =
                    {
                           .paths = binaries,
                           .count = 2,
                           },
    };

    assert(forge_config_validate(&config) == 0);

    test_pass();
}

static void test_missing_binary(void)
{
    test_begin("reject missing binary");

    char *binaries[] = {
            "/this/binary/does/not/exist",
    };

    struct forge_config config = {
            .base =
                    {
                           .distribution = "alpine",
                           .version      = "3.22",
                           .architecture = "x86_64",
                           },
            .rootfs =
                    {
                           .output = "./rootfs",
                           },
            .binaries =
                    {
                           .paths = binaries,
                           .count = 1,
                           },
    };

    assert(forge_config_validate(&config) != 0);

    test_pass();
}

static void test_non_regular_binary(void)
{
    test_begin("reject non-regular binary");

    char *binaries[] = {
            "/tmp",
    };

    struct forge_config config = {
            .base =
                    {
                           .distribution = "alpine",
                           .version      = "3.22",
                           .architecture = "x86_64",
                           },
            .rootfs =
                    {
                           .output = "./rootfs",
                           },
            .binaries =
                    {
                           .paths = binaries,
                           .count = 1,
                           },
    };

    assert(forge_config_validate(&config) != 0);

    test_pass();
}

static void test_non_executable_binary(void)
{
    const char *path = "tests/non-executable";

    test_begin("reject non-executable binary");

    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);

    assert(fd >= 0);
    assert(close(fd) == 0);

    char *binaries[] = {
            ( char * )path,
    };

    struct forge_config config = {
            .base =
                    {
                           .distribution = "alpine",
                           .version      = "3.22",
                           .architecture = "x86_64",
                           },
            .rootfs =
                    {
                           .output = "./rootfs",
                           },
            .binaries =
                    {
                           .paths = binaries,
                           .count = 1,
                           },
    };

    assert(forge_config_validate(&config) != 0);

    assert(remove(path) == 0);

    test_pass();
}

static void test_existing_binary(void)
{
    test_begin("accept existing executable binary");

    char *binaries[] = {
            "/bin/sh",
    };

    struct forge_config config = {
            .base =
                    {
                           .distribution = "alpine",
                           .version      = "3.22",
                           .architecture = "x86_64",
                           },
            .rootfs =
                    {
                           .output = "./rootfs",
                           },
            .binaries =
                    {
                           .paths = binaries,
                           .count = 1,
                           },
    };

    assert(forge_config_validate(&config) == 0);

    test_pass();
}

int main(void)
{
    puts("forge config tests");
    puts("==================");
    puts("");

    test_basic_config();
    test_trailing_comma();
    test_inline_array();
    test_comments();
    test_string_escapes();

    puts("");

    test_missing_required_key();
    test_relative_binary();
    test_unknown_section();
    test_duplicate_key();
    test_unterminated_array();

    puts("");

    test_valid_config();
    test_unsupported_distribution();
    test_unsupported_architecture();

    puts("");

    test_valid_config();
    test_unsupported_distribution();
    test_unsupported_architecture();

    puts("");

    test_existing_binary();
    test_missing_binary();
    test_non_regular_binary();
    test_non_executable_binary();

    puts("");

    printf("%u/%u tests passed\n", tests_passed, tests_run);

    if (tests_passed != tests_run) {
        puts("FAILED");
        return EXIT_FAILURE;
    }

    puts("PASSED");

    return EXIT_SUCCESS;
}
