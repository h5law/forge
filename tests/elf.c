#include <elf_parser.h>

#include "utils.h"

#include <assert.h>
#include <elf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const char *fixture_path = "tests/test-elf-fixture";

static void write_fixture(const void *data, size_t size)
{
    FILE *file = fopen(fixture_path, "wb");

    assert(file != NULL);
    assert(fwrite(data, 1, size, file) == size);
    assert(fclose(file) == 0);
}

static void remove_fixture(void) { assert(unlink(fixture_path) == 0); }

static void test_parse_binary(void)
{
    test_begin("parse ELF binary");

    struct forge_elf elf;

    assert(forge_elf_parse("/bin/ls", &elf) == 0);
    assert(elf.machine == EM_X86_64);
    assert(elf.elf_class == ELFCLASS64);

    forge_elf_free(&elf);

    test_pass();
}

static void test_extract_interpreter(void)
{
    test_begin("extract ELF interpreter");

    struct forge_elf elf;

    assert(forge_elf_parse("/bin/ls", &elf) == 0);
    assert(elf.interpreter != NULL);
    assert(elf.interpreter[0] == '/');

    forge_elf_free(&elf);

    test_pass();
}

static void test_extract_needed(void)
{
    test_begin("extract DT_NEEDED entries");

    struct forge_elf elf;

    assert(forge_elf_parse("/bin/ls", &elf) == 0);
    assert(elf.needed_count > 0);

    for (size_t i = 0; i < elf.needed_count; ++i)
        assert(elf.needed[i] != NULL);

    forge_elf_free(&elf);

    test_pass();
}

static void test_extract_search_paths(void)
{
    test_begin("extract ELF library search paths");

    struct forge_elf elf;

    assert(forge_elf_parse("/bin/ls", &elf) == 0);

    if (elf.rpath != NULL)
        assert(elf.rpath[0] != '\0');

    if (elf.runpath != NULL)
        assert(elf.runpath[0] != '\0');

    forge_elf_free(&elf);

    test_pass();
}

static void test_dynamic(void)
{
    test_begin("detect dynamic ELF");

    struct forge_elf elf;

    assert(forge_elf_parse("/bin/ls", &elf) == 0);
    assert(elf.dynamic == 1);

    forge_elf_free(&elf);

    test_pass();
}

static void test_static(void)
{
    test_begin("detect static ELF");

    /*
     * Static ELF fixture will be added once the test suite has
     * deterministic ELF fixtures.
     */

    test_skip();

    test_pass();
}

static void test_truncated_header(void)
{
    test_begin("reject truncated ELF header");

    unsigned char data[sizeof(Elf64_Ehdr)];

    memset(data, 0, sizeof(data));

    write_fixture(data, sizeof(data) - 1);

    struct forge_elf elf;

    assert(forge_elf_parse(fixture_path, &elf) < 0);

    remove_fixture();

    test_pass();
}

static void test_invalid_magic(void)
{
    test_begin("reject invalid ELF magic");

    Elf64_Ehdr header;

    memset(&header, 0, sizeof(header));

    header.e_ident[EI_CLASS] = ELFCLASS64;
    header.e_ident[EI_DATA]  = ELFDATA2LSB;

    write_fixture(&header, sizeof(header));

    struct forge_elf elf;

    assert(forge_elf_parse(fixture_path, &elf) < 0);

    remove_fixture();

    test_pass();
}

static void test_unsupported_class(void)
{
    test_begin("reject unsupported ELF class");

    Elf64_Ehdr header;

    memset(&header, 0, sizeof(header));

    memcpy(header.e_ident, ELFMAG, SELFMAG);

    header.e_ident[EI_CLASS] = ELFCLASS32;
    header.e_ident[EI_DATA]  = ELFDATA2LSB;

    write_fixture(&header, sizeof(header));

    struct forge_elf elf;

    assert(forge_elf_parse(fixture_path, &elf) < 0);

    remove_fixture();

    test_pass();
}

static void test_unsupported_data(void)
{
    test_begin("reject unsupported ELF byte order");

    Elf64_Ehdr header;

    memset(&header, 0, sizeof(header));

    memcpy(header.e_ident, ELFMAG, SELFMAG);

    header.e_ident[EI_CLASS] = ELFCLASS64;
    header.e_ident[EI_DATA]  = ELFDATA2MSB;

    write_fixture(&header, sizeof(header));

    struct forge_elf elf;

    assert(forge_elf_parse(fixture_path, &elf) < 0);

    remove_fixture();

    test_pass();
}

static void test_unsupported_machine(void)
{
    test_begin("reject unsupported ELF architecture");

    Elf64_Ehdr header;

    memset(&header, 0, sizeof(header));

    memcpy(header.e_ident, ELFMAG, SELFMAG);

    header.e_ident[EI_CLASS] = ELFCLASS64;
    header.e_ident[EI_DATA]  = ELFDATA2LSB;
    header.e_machine         = EM_AARCH64;

    write_fixture(&header, sizeof(header));

    struct forge_elf elf;

    assert(forge_elf_parse(fixture_path, &elf) < 0);

    remove_fixture();

    test_pass();
}

static void test_truncated_program_headers(void)
{
    test_begin("reject truncated program headers");

    Elf64_Ehdr header;

    memset(&header, 0, sizeof(header));

    memcpy(header.e_ident, ELFMAG, SELFMAG);

    header.e_ident[EI_CLASS] = ELFCLASS64;
    header.e_ident[EI_DATA]  = ELFDATA2LSB;

    header.e_version         = EV_CURRENT;
    header.e_ehsize          = sizeof(Elf64_Ehdr);
    header.e_machine         = EM_X86_64;

    header.e_phoff           = sizeof(Elf64_Ehdr);
    header.e_phentsize       = sizeof(Elf64_Phdr);
    header.e_phnum           = 1;

    write_fixture(&header, sizeof(header));

    struct forge_elf elf;

    assert(forge_elf_parse(fixture_path, &elf) < 0);

    remove_fixture();

    test_pass();
}

static void test_invalid_interpreter(void)
{
    test_begin("reject invalid PT_INTERP");

    Elf64_Ehdr header;

    memset(&header, 0, sizeof(header));

    memcpy(header.e_ident, ELFMAG, SELFMAG);

    header.e_ident[EI_CLASS] = ELFCLASS64;
    header.e_ident[EI_DATA]  = ELFDATA2LSB;

    header.e_version         = EV_CURRENT;
    header.e_ehsize          = sizeof(Elf64_Ehdr);
    header.e_machine         = EM_X86_64;

    header.e_phoff           = sizeof(Elf64_Ehdr);
    header.e_phentsize       = sizeof(Elf64_Phdr);
    header.e_phnum           = 1;

    Elf64_Phdr program_header;

    memset(&program_header, 0, sizeof(program_header));

    program_header.p_type   = PT_INTERP;
    program_header.p_offset = sizeof(Elf64_Ehdr) + sizeof(Elf64_Phdr);
    program_header.p_filesz = 4;

    char interpreter[]      = "abcx";

    size_t size = sizeof(header) + sizeof(program_header) + sizeof(interpreter);

    unsigned char *data = calloc(1, size);

    assert(data != NULL);

    memcpy(data, &header, sizeof(header));

    memcpy(data + sizeof(header), &program_header, sizeof(program_header));

    memcpy(data + sizeof(header) + sizeof(program_header), interpreter,
           sizeof(interpreter));

    write_fixture(data, size);

    free(data);

    struct forge_elf elf;

    assert(forge_elf_parse(fixture_path, &elf) < 0);

    remove_fixture();

    test_pass();
}

static void test_free_empty(void)
{
    test_begin("free empty ELF structure");

    struct forge_elf elf;

    memset(&elf, 0, sizeof(elf));

    forge_elf_free(&elf);

    test_pass();
}

static void test_non_elf(void)
{
    test_begin("reject non-ELF file");

    struct forge_elf elf;

    assert(forge_elf_parse("/etc/passwd", &elf) < 0);

    test_pass();
}

static void test_missing_file(void)
{
    test_begin("reject missing file");

    struct forge_elf elf;

    assert(forge_elf_parse("/does/not/exist", &elf) < 0);

    test_pass();
}

int main(void)
{
    puts("forge ELF tests");
    puts("===============");
    puts("");

    test_parse_binary();
    test_extract_interpreter();
    test_extract_needed();
    test_extract_search_paths();
    test_dynamic();
    test_static();
    test_truncated_header();
    test_invalid_magic();
    test_unsupported_class();
    test_unsupported_data();
    test_unsupported_machine();
    test_truncated_program_headers();
    test_invalid_interpreter();
    test_free_empty();
    test_non_elf();
    test_missing_file();

    puts("");

    return test_run();
}
