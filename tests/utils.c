#include "utils.h"

#include <stdio.h>

static unsigned int tests_run;
static unsigned int tests_passed;

void test_begin(const char *name)
{
    printf("  %-50s ", name);
    fflush(stdout);

    ++tests_run;
}

void test_pass(void)
{
    puts("[PASS]");

    ++tests_passed;
}

void test_fail(const char *message)
{
    printf("[FAIL]\n");
    fprintf(stderr, "    %s\n", message);
}

int test_run(void)
{
    printf("\n%d/%d tests passed\n", tests_passed, tests_run);

    return tests_passed == tests_run ? 0 : 1;
}
