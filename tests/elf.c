#include <elf_parser.h>

#include "utils.h"

#include <assert.h>
#include <elf.h>
#include <stdio.h>
#include <string.h>

static void test_parse_elf(void)
{
    test_begin("parse ELF binary");

    struct forge_elf elf;

    assert(forge_elf_parse("/bin/sh", &elf) == 0);
    assert(elf.machine == EM_X86_64);
    assert(elf.elf_class == ELFCLASS64);

    forge_elf_free(&elf);

    test_pass();
}

static void test_parse_interpreter(void)
{
    test_begin("extract ELF interpreter");

    struct forge_elf elf;

    assert(forge_elf_parse("/bin/sh", &elf) == 0);
    assert(elf.interpreter != NULL);
    assert(elf.interpreter[0] == '/');

    forge_elf_free(&elf);

    test_pass();
}

static void test_reject_non_elf(void)
{
    test_begin("reject non-ELF file");

    struct forge_elf elf;

    assert(forge_elf_parse("/etc/passwd", &elf) != 0);

    test_pass();
}

static void test_reject_missing_file(void)
{
    test_begin("reject missing file");

    struct forge_elf elf;

    assert(forge_elf_parse("/does/not/exist", &elf) != 0);

    test_pass();
}

int main(void)
{
    puts("forge ELF tests");
    puts("===============");
    puts("");

    test_parse_elf();
    test_parse_interpreter();
    test_reject_non_elf();
    test_reject_missing_file();

    puts("");

    return test_run();
}
