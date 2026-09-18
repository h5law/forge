#include <resolver.h>

#include "utils.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void test_resolve_existing_library(void)
{
    test_begin("resolve existing library");

    char *path = forge_resolve_library("libc.so.6");

    assert(path != NULL);
    assert(path[0] == '/');

    free(path);

    test_pass();
}

static void test_resolve_missing_library(void)
{
    test_begin("reject missing library");

    char *path = forge_resolve_library("libdoesnotexist.so");

    assert(path == NULL);

    test_pass();
}

static void test_resolve_null(void)
{
    test_begin("reject null library name");

    assert(forge_resolve_library(NULL) == NULL);

    test_pass();
}

static void test_resolve_empty_name(void)
{
    test_begin("reject empty library name");

    assert(forge_resolve_library("") == NULL);

    test_pass();
}

int main(void)
{
    puts("forge resolver tests");
    puts("====================");
    puts("");

    test_resolve_existing_library();
    test_resolve_missing_library();
    test_resolve_null();
    test_resolve_empty_name();

    puts("");

    return test_run();
}
