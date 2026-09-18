#include <elf_parser.h>

#include "utils.h"

#include <assert.h>
#include <elf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define TEST_ELF_PATH "tests/test-elf-fixture"

static void write_fixture(const void *data, size_t size)
{
    FILE *file = fopen(TEST_ELF_PATH, "wb");

    assert(file != NULL);
    assert(fwrite(data, size, 1, file) == 1);
    assert(fclose(file) == 0);
}

static void remove_fixture(void) { assert(unlink(TEST_ELF_PATH) == 0); }

static Elf64_Ehdr make_header(void)
{
    Elf64_Ehdr header;

    memset(&header, 0, sizeof(header));

    memcpy(header.e_ident, ELFMAG, SELFMAG);

    header.e_ident[EI_CLASS]   = ELFCLASS64;
    header.e_ident[EI_DATA]    = ELFDATA2LSB;
    header.e_ident[EI_VERSION] = EV_CURRENT;

    header.e_type              = ET_EXEC;
    header.e_machine           = EM_X86_64;
    header.e_version           = EV_CURRENT;

    header.e_ehsize            = sizeof(Elf64_Ehdr);
    header.e_phentsize         = sizeof(Elf64_Phdr);

    return header;
}

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

static void test_parse_needed(void)
{
    test_begin("extract DT_NEEDED entries");

    struct forge_elf elf;

    assert(forge_elf_parse("/bin/sh", &elf) == 0);
    assert(elf.dynamic != 0);
    assert(elf.needed != NULL);
    assert(elf.needed_count > 0);

    for (size_t i = 0; i < elf.needed_count; ++i) {
        assert(elf.needed[i] != NULL);
        assert(strlen(elf.needed[i]) > 0);
    }

    forge_elf_free(&elf);

    test_pass();
}

static void test_detect_dynamic(void)
{
    test_begin("detect dynamic ELF");

    struct forge_elf elf;

    assert(forge_elf_parse("/bin/sh", &elf) == 0);
    assert(elf.dynamic != 0);
    assert(elf.interpreter != NULL);
    assert(elf.needed_count > 0);

    forge_elf_free(&elf);

    test_pass();
}

static void test_detect_static(void)
{
    test_begin("detect static ELF");

    Elf64_Ehdr header = make_header();

    header.e_phnum    = 1;

    Elf64_Phdr program_header;

    memset(&program_header, 0, sizeof(program_header));

    program_header.p_type = PT_LOAD;

    header.e_phoff        = sizeof(header);

    size_t size           = sizeof(header) + sizeof(program_header);

    unsigned char *data   = calloc(1, size);

    assert(data != NULL);

    memcpy(data, &header, sizeof(header));
    memcpy(data + sizeof(header), &program_header, sizeof(program_header));

    write_fixture(data, size);
    free(data);

    struct forge_elf elf;

    assert(forge_elf_parse(TEST_ELF_PATH, &elf) == 0);
    assert(elf.dynamic == 0);
    assert(elf.interpreter == NULL);
    assert(elf.needed == NULL);
    assert(elf.needed_count == 0);

    forge_elf_free(&elf);

    remove_fixture();

    test_pass();
}

static void test_reject_truncated_header(void)
{
    test_begin("reject truncated ELF header");

    unsigned char data[8] = {
            0x7f, 'E', 'L', 'F', ELFCLASS64, ELFDATA2LSB, EV_CURRENT, 0,
    };

    write_fixture(data, sizeof(data));

    struct forge_elf elf;

    assert(forge_elf_parse(TEST_ELF_PATH, &elf) != 0);

    remove_fixture();

    test_pass();
}

static void test_reject_invalid_magic(void)
{
    test_begin("reject invalid ELF magic");

    Elf64_Ehdr header = make_header();

    header.e_ident[0] = 0;

    write_fixture(&header, sizeof(header));

    struct forge_elf elf;

    assert(forge_elf_parse(TEST_ELF_PATH, &elf) != 0);

    remove_fixture();

    test_pass();
}

static void test_reject_unsupported_class(void)
{
    test_begin("reject unsupported ELF class");

    Elf64_Ehdr header        = make_header();

    header.e_ident[EI_CLASS] = ELFCLASS32;

    write_fixture(&header, sizeof(header));

    struct forge_elf elf;

    assert(forge_elf_parse(TEST_ELF_PATH, &elf) != 0);

    remove_fixture();

    test_pass();
}

static void test_reject_unsupported_endianness(void)
{
    test_begin("reject unsupported ELF byte order");

    Elf64_Ehdr header       = make_header();

    header.e_ident[EI_DATA] = ELFDATA2MSB;

    write_fixture(&header, sizeof(header));

    struct forge_elf elf;

    assert(forge_elf_parse(TEST_ELF_PATH, &elf) != 0);

    remove_fixture();

    test_pass();
}

static void test_reject_unsupported_architecture(void)
{
    test_begin("reject unsupported ELF architecture");

    Elf64_Ehdr header = make_header();

    header.e_machine  = EM_AARCH64;

    write_fixture(&header, sizeof(header));

    struct forge_elf elf;

    assert(forge_elf_parse(TEST_ELF_PATH, &elf) != 0);

    remove_fixture();

    test_pass();
}

static void test_reject_truncated_program_headers(void)
{
    test_begin("reject truncated program headers");

    Elf64_Ehdr header = make_header();

    header.e_phoff    = sizeof(header);
    header.e_phnum    = 1;

    write_fixture(&header, sizeof(header));

    struct forge_elf elf;

    assert(forge_elf_parse(TEST_ELF_PATH, &elf) != 0);

    remove_fixture();

    test_pass();
}

static void test_reject_invalid_interpreter(void)
{
    test_begin("reject invalid PT_INTERP");

    Elf64_Ehdr header = make_header();

    header.e_phoff    = sizeof(header);
    header.e_phnum    = 1;

    Elf64_Phdr program_header;

    memset(&program_header, 0, sizeof(program_header));

    program_header.p_type   = PT_INTERP;
    program_header.p_offset = sizeof(header) + sizeof(program_header);
    program_header.p_filesz = 4;

    unsigned char data[sizeof(header) + sizeof(program_header) + 4];

    memset(data, 0, sizeof(data));

    memcpy(data, &header, sizeof(header));

    memcpy(data + sizeof(header), &program_header, sizeof(program_header));

    memcpy(data + sizeof(header) + sizeof(program_header), "test", 4);

    write_fixture(data, sizeof(data));

    struct forge_elf elf;

    assert(forge_elf_parse(TEST_ELF_PATH, &elf) != 0);

    remove_fixture();

    test_pass();
}

static void test_free_empty_elf(void)
{
    test_begin("free empty ELF structure");

    struct forge_elf elf = {0};

    forge_elf_free(&elf);

    assert(elf.interpreter == NULL);
    assert(elf.needed == NULL);
    assert(elf.needed_count == 0);
    assert(elf.dynamic == 0);

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
    test_parse_needed();
    test_detect_dynamic();
    test_detect_static();

    test_reject_truncated_header();
    test_reject_invalid_magic();
    test_reject_unsupported_class();
    test_reject_unsupported_endianness();
    test_reject_unsupported_architecture();
    test_reject_truncated_program_headers();
    test_reject_invalid_interpreter();

    test_free_empty_elf();
    test_reject_non_elf();
    test_reject_missing_file();

    puts("");

    return test_run();
}
