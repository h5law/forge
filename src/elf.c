#include <elf_parser.h>

#include <elf.h>

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int read_header(FILE *file, Elf64_Ehdr *header)
{
    if (fread(header, sizeof(*header), 1, file) != 1) {
        fprintf(stderr, "failed to read ELF header: %s\n",
                feof(file) ? "unexpected end of file" : strerror(errno));

        return -1;
    }

    return 0;
}

static int validate_header(const Elf64_Ehdr *header)
{
    if (header->e_version != EV_CURRENT) {
        fprintf(stderr, "unsupported ELF version: %u\n", header->e_version);

        return -1;
    }

    if (header->e_ehsize != sizeof(Elf64_Ehdr)) {
        fprintf(stderr, "unsupported ELF header size: %u\n", header->e_ehsize);

        return -1;
    }

    if (header->e_phnum != 0 && header->e_phentsize != sizeof(Elf64_Phdr)) {
        fprintf(stderr, "unsupported ELF program header size: %u\n",
                header->e_phentsize);

        return -1;
    }

    return 0;
}

static int read_program_headers(FILE *file, const Elf64_Ehdr *header,
                                Elf64_Phdr **program_headers)
{
    if (header->e_phnum == 0) {
        *program_headers = NULL;
        return 0;
    }

    if (header->e_phentsize != sizeof(Elf64_Phdr)) {
        fprintf(stderr, "unsupported ELF program header size: %u\n",
                header->e_phentsize);

        return -1;
    }

    Elf64_Phdr *headers = malloc(( size_t )header->e_phnum * sizeof(*headers));

    if (headers == NULL) {
        fprintf(stderr, "failed to allocate ELF program headers\n");

        return -1;
    }

    if (( Elf64_Off )( long )header->e_phoff != header->e_phoff ||
        fseek(file, ( long )header->e_phoff, SEEK_SET) < 0) {
        fprintf(stderr, "failed to seek to program headers: %s\n",
                strerror(errno));

        free(headers);
        return -1;
    }

    if (fread(headers, sizeof(*headers), header->e_phnum, file) !=
        header->e_phnum) {
        fprintf(stderr, "failed to read ELF program headers\n");

        free(headers);
        return -1;
    }

    *program_headers = headers;

    return 0;
}

static int has_program_header(const Elf64_Phdr *program_headers,
                              size_t program_header_count, Elf64_Word type)
{
    for (size_t i = 0; i < program_header_count; ++i) {
        if (program_headers[i].p_type == type)
            return 1;
    }

    return 0;
}

static int extract_interpreter(FILE *file, const Elf64_Phdr *program_headers,
                               size_t            program_header_count,
                               struct forge_elf *elf)
{
    for (size_t i = 0; i < program_header_count; ++i) {
        const Elf64_Phdr *header = &program_headers[i];

        if (header->p_type != PT_INTERP)
            continue;

        if (header->p_filesz == 0) {
            fprintf(stderr, "ELF PT_INTERP segment is empty\n");

            return -1;
        }

        if (header->p_filesz > SIZE_MAX - 1) {
            fprintf(stderr, "ELF PT_INTERP segment is too large\n");

            return -1;
        }

        size_t size       = ( size_t )header->p_filesz;

        char *interpreter = malloc(size + 1);

        if (interpreter == NULL) {
            fprintf(stderr, "failed to allocate ELF interpreter\n");

            return -1;
        }

        if (( Elf64_Off )( long )header->p_offset != header->p_offset ||
            fseek(file, ( long )header->p_offset, SEEK_SET) < 0) {
            fprintf(stderr, "failed to seek to ELF interpreter: %s\n",
                    strerror(errno));

            free(interpreter);
            return -1;
        }

        if (fread(interpreter, size, 1, file) != 1) {
            fprintf(stderr, "failed to read ELF interpreter\n");

            free(interpreter);
            return -1;
        }

        if (memchr(interpreter, '\0', size) == NULL) {
            fprintf(stderr, "ELF PT_INTERP segment is not null terminated\n");

            free(interpreter);
            return -1;
        }

        interpreter[size] = '\0';
        elf->interpreter  = interpreter;

        return 0;
    }

    return 0;
}

static int virtual_to_file_offset(const Elf64_Phdr *program_headers,
                                  size_t            program_header_count,
                                  Elf64_Addr address, Elf64_Off *offset)
{
    for (size_t i = 0; i < program_header_count; ++i) {
        const Elf64_Phdr *header = &program_headers[i];

        if (header->p_type != PT_LOAD)
            continue;

        if (address < header->p_vaddr)
            continue;

        Elf64_Addr relative = address - header->p_vaddr;

        if (relative >= header->p_filesz)
            continue;

        if (relative > UINT64_MAX - header->p_offset)
            return -1;

        *offset = header->p_offset + relative;

        return 0;
    }

    return -1;
}

static int append_needed(struct forge_elf *elf, const char *name)
{
    if (elf->needed_count == SIZE_MAX / sizeof(*elf->needed) - 1) {
        fprintf(stderr, "too many ELF dependencies\n");

        return -1;
    }

    char **needed =
            realloc(elf->needed, (elf->needed_count + 1) * sizeof(*needed));

    if (needed == NULL) {
        fprintf(stderr, "failed to allocate ELF dependency list\n");

        return -1;
    }

    elf->needed                    = needed;

    elf->needed[elf->needed_count] = strdup(name);

    if (elf->needed[elf->needed_count] == NULL) {
        fprintf(stderr, "failed to allocate ELF dependency name\n");

        return -1;
    }

    ++elf->needed_count;

    return 0;
}

static int extract_needed(FILE *file, const Elf64_Phdr *program_headers,
                          size_t program_header_count, struct forge_elf *elf)
{
    const Elf64_Phdr *dynamic_header = NULL;

    for (size_t i = 0; i < program_header_count; ++i) {
        if (program_headers[i].p_type != PT_DYNAMIC)
            continue;

        dynamic_header = &program_headers[i];
        break;
    }

    if (dynamic_header == NULL)
        return 0;

    if (dynamic_header->p_filesz % sizeof(Elf64_Dyn) != 0) {
        fprintf(stderr, "invalid ELF dynamic section size\n");

        return -1;
    }

    size_t dynamic_count =
            ( size_t )(dynamic_header->p_filesz / sizeof(Elf64_Dyn));

    if (dynamic_count > SIZE_MAX / sizeof(Elf64_Dyn)) {
        fprintf(stderr, "ELF dynamic section is too large\n");

        return -1;
    }

    Elf64_Dyn *dynamic = NULL;

    if (dynamic_count != 0) {
        dynamic = malloc(dynamic_count * sizeof(*dynamic));

        if (dynamic == NULL) {
            fprintf(stderr, "failed to allocate ELF dynamic section\n");

            return -1;
        }
    }

    if (( Elf64_Off )( long )dynamic_header->p_offset !=
                dynamic_header->p_offset ||
        fseek(file, ( long )dynamic_header->p_offset, SEEK_SET) < 0) {
        fprintf(stderr, "failed to seek to ELF dynamic section: %s\n",
                strerror(errno));

        free(dynamic);
        return -1;
    }

    if (dynamic_count != 0 && fread(dynamic, sizeof(*dynamic), dynamic_count,
                                    file) != dynamic_count) {
        fprintf(stderr, "failed to read ELF dynamic section\n");

        free(dynamic);
        return -1;
    }

    Elf64_Addr  string_table_address   = 0;
    Elf64_Xword string_table_size      = 0;
    int         have_string_table      = 0;
    int         have_string_table_size = 0;

    for (size_t i = 0; i < dynamic_count; ++i) {
        if (dynamic[i].d_tag == DT_NULL)
            break;

        if (dynamic[i].d_tag == DT_STRTAB) {
            string_table_address = dynamic[i].d_un.d_ptr;
            have_string_table    = 1;
        } else if (dynamic[i].d_tag == DT_STRSZ) {
            string_table_size      = dynamic[i].d_un.d_val;
            have_string_table_size = 1;
        }
    }

    if (!have_string_table || !have_string_table_size) {
        fprintf(stderr, "ELF dynamic section has no string table\n");

        free(dynamic);
        return -1;
    }

    if (string_table_size > SIZE_MAX) {
        fprintf(stderr, "ELF string table is too large\n");

        free(dynamic);
        return -1;
    }

    Elf64_Off string_table_offset;

    if (virtual_to_file_offset(program_headers, program_header_count,
                               string_table_address,
                               &string_table_offset) < 0) {
        fprintf(stderr, "failed to locate ELF string table\n");

        free(dynamic);
        return -1;
    }

    if (( Elf64_Off )( long )string_table_offset != string_table_offset ||
        fseek(file, ( long )string_table_offset, SEEK_SET) < 0) {
        fprintf(stderr, "failed to seek to ELF string table: %s\n",
                strerror(errno));

        free(dynamic);
        return -1;
    }

    size_t string_table_length = ( size_t )string_table_size;

    char *strings              = NULL;

    if (string_table_length != 0) {
        strings = malloc(string_table_length);

        if (strings == NULL) {
            fprintf(stderr, "failed to allocate ELF string table\n");

            free(dynamic);
            return -1;
        }

        if (fread(strings, string_table_length, 1, file) != 1) {
            fprintf(stderr, "failed to read ELF string table\n");

            free(strings);
            free(dynamic);
            return -1;
        }
    }

    for (size_t i = 0; i < dynamic_count; ++i) {
        if (dynamic[i].d_tag == DT_NULL)
            break;

        if (dynamic[i].d_tag != DT_NEEDED)
            continue;

        Elf64_Xword string_offset = dynamic[i].d_un.d_val;

        if (string_offset >= string_table_size) {
            fprintf(stderr, "ELF DT_NEEDED string offset is out of bounds\n");

            free(strings);
            free(dynamic);
            return -1;
        }

        const char *name = strings + ( size_t )string_offset;

        size_t remaining = string_table_length - ( size_t )string_offset;

        if (memchr(name, '\0', remaining) == NULL) {
            fprintf(stderr, "ELF DT_NEEDED string is not null terminated\n");

            free(strings);
            free(dynamic);
            return -1;
        }

        if (append_needed(elf, name) < 0) {
            free(strings);
            free(dynamic);
            return -1;
        }
    }

    free(strings);
    free(dynamic);

    return 0;
}

int forge_elf_parse(const char *path, struct forge_elf *elf)
{
    if (path == NULL || elf == NULL) {
        fprintf(stderr, "invalid ELF parser arguments\n");
        return -1;
    }

    memset(elf, 0, sizeof(*elf));

    FILE *file = fopen(path, "rb");

    if (file == NULL) {
        fprintf(stderr, "failed to open %s: %s\n", path, strerror(errno));

        return -1;
    }

    unsigned char ident[EI_NIDENT];

    if (fread(ident, sizeof(ident), 1, file) != 1) {
        fprintf(stderr, "%s: truncated ELF header\n", path);
        fclose(file);
        return -1;
    }

    if (memcmp(ident, ELFMAG, SELFMAG) != 0) {
        fprintf(stderr, "%s: invalid ELF magic\n", path);
        fclose(file);
        return -1;
    }

    if (ident[EI_CLASS] != ELFCLASS64) {
        fprintf(stderr, "%s: unsupported ELF class: %u\n", path,
                ident[EI_CLASS]);

        fclose(file);
        return -1;
    }

    if (ident[EI_DATA] != ELFDATA2LSB) {
        fprintf(stderr, "%s: unsupported ELF byte order: %u\n", path,
                ident[EI_DATA]);

        fclose(file);
        return -1;
    }

    if (fseek(file, 0, SEEK_SET) < 0) {
        fprintf(stderr, "%s: failed to seek: %s\n", path, strerror(errno));

        fclose(file);
        return -1;
    }

    Elf64_Ehdr header;

    if (read_header(file, &header) < 0) {
        fclose(file);
        return -1;
    }

    if (validate_header(&header) < 0) {
        fclose(file);
        return -1;
    }

    if (header.e_machine != EM_X86_64) {
        fprintf(stderr, "%s: unsupported ELF architecture: %u\n", path,
                header.e_machine);

        fclose(file);
        return -1;
    }

    Elf64_Phdr *program_headers;

    if (read_program_headers(file, &header, &program_headers) < 0) {
        fclose(file);
        return -1;
    }

    elf->machine   = header.e_machine;
    elf->elf_class = header.e_ident[EI_CLASS];

    elf->dynamic =
            has_program_header(program_headers, header.e_phnum, PT_DYNAMIC);

    if (extract_interpreter(file, program_headers, header.e_phnum, elf) < 0) {
        free(program_headers);
        forge_elf_free(elf);
        fclose(file);
        return -1;
    }

    if (elf->dynamic &&
        extract_needed(file, program_headers, header.e_phnum, elf) < 0) {
        free(program_headers);
        forge_elf_free(elf);
        fclose(file);
        return -1;
    }

    free(program_headers);

    if (fclose(file) != 0) {
        fprintf(stderr, "failed to close %s: %s\n", path, strerror(errno));

        forge_elf_free(elf);
        return -1;
    }

    return 0;
}

void forge_elf_free(struct forge_elf *elf)
{
    if (elf == NULL)
        return;

    free(elf->interpreter);

    for (size_t i = 0; i < elf->needed_count; ++i)
        free(elf->needed[i]);

    free(elf->needed);

    memset(elf, 0, sizeof(*elf));
}
