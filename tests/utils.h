#ifndef FORGE_TEST_UTILS_H
#define FORGE_TEST_UTILS_H

void test_begin(const char *name);
void test_pass(void);
void test_fail(const char *message);

int test_run(void);

#endif
