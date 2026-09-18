#include <file.h>

#include "utils.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const char *fixture_path = "tests/test-script-fixture";

static void write_fixture(const char *contents)
{
    FILE *file = fopen(fixture_path, "wb");

    assert(file != NULL);
    assert(fputs(contents, file) >= 0);
    assert(fclose(file) == 0);
}

static void remove_fixture(void) { assert(unlink(fixture_path) == 0); }

static void test_script(void)
{
    test_begin("detect script");

    write_fixture("#!/bin/sh\nprintf 'hello\\n'\n");

    assert(forge_file_is_script(fixture_path) == 1);

    remove_fixture();

    test_pass();
}

static void test_script_interpreter(void)
{
    test_begin("detect script interpreter");

    write_fixture("#!/bin/sh\nprintf 'hello\\n'\n");

    char *interpreter = NULL;

    assert(forge_file_get_script_interpreter(fixture_path, &interpreter) == 0);

    assert(interpreter != NULL);
    assert(strcmp(interpreter, "/bin/sh") == 0);

    free(interpreter);
    remove_fixture();

    test_pass();
}

static void test_script_interpreter_with_argument(void)
{
    test_begin("detect script interpreter with argument");

    write_fixture("#!/bin/sh -e\nprintf 'hello\\n'\n");

    char *interpreter = NULL;

    assert(forge_file_get_script_interpreter(fixture_path, &interpreter) == 0);

    assert(interpreter != NULL);
    assert(strcmp(interpreter, "/bin/sh") == 0);

    free(interpreter);
    remove_fixture();

    test_pass();
}

static void test_malformed_script(void)
{
    test_begin("reject script without interpreter");

    write_fixture("#!\nprintf 'hello\\n'\n");

    char *interpreter = NULL;

    assert(forge_file_get_script_interpreter(fixture_path, &interpreter) < 0);

    assert(interpreter == NULL);

    remove_fixture();

    test_pass();
}

static void test_non_script(void)
{
    test_begin("reject non-script");

    write_fixture("printf 'hello\\n'\n");

    assert(forge_file_is_script(fixture_path) == 0);

    remove_fixture();

    test_pass();
}

static void test_non_script_interpreter(void)
{
    test_begin("reject interpreter from non-script");

    write_fixture("printf 'hello\\n'\n");

    char *interpreter = NULL;

    assert(forge_file_get_script_interpreter(fixture_path, &interpreter) < 0);

    assert(interpreter == NULL);

    remove_fixture();

    test_pass();
}

static void test_empty_file(void)
{
    test_begin("reject empty file");

    write_fixture("");

    assert(forge_file_is_script(fixture_path) == 0);

    remove_fixture();

    test_pass();
}

static void test_short_file(void)
{
    test_begin("reject short file");

    write_fixture("#");

    assert(forge_file_is_script(fixture_path) == 0);

    remove_fixture();

    test_pass();
}

static void test_missing_file(void)
{
    test_begin("reject missing file");

    assert(forge_file_is_script("/does/not/exist") < 0);

    test_pass();
}

static void test_null_path(void)
{
    test_begin("reject null path");

    assert(forge_file_is_script(NULL) < 0);

    test_pass();
}

static void test_null_interpreter(void)
{
    test_begin("reject null interpreter");

    write_fixture("#!/bin/sh\n");

    assert(forge_file_get_script_interpreter(fixture_path, NULL) < 0);

    remove_fixture();

    test_pass();
}

int main(void)
{
    puts("forge file tests");
    puts("================");
    puts("");

    test_script();
    test_script_interpreter();
    test_script_interpreter_with_argument();
    test_malformed_script();
    test_non_script();
    test_non_script_interpreter();
    test_empty_file();
    test_short_file();
    test_missing_file();
    test_null_path();
    test_null_interpreter();

    puts("");

    return test_run();
}
