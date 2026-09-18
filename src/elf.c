#include <elf_parser.h>

#include <elf.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int supported_machine(unsigned int machine)
{
    switch (machine) {
    case EM_X86_64:
    case EM_AARCH64:
    case EM_RISCV:
        return 1;

    default:
        return 0;
    }
}

static int forge_elf_parse_internal(const char *path, struct forge_elf *elf,
                                    int verbose)
{
    memset(elf, 0, sizeof(*elf));

    FILE *file = fopen(path, "rb");

    if (file == NULL) {
        if (verbose)
            fprintf(stderr, "failed to open %s: %s\n", path, strerror(errno));

        return -1;
    }

    Elf64_Ehdr header;

    if (fread(&header, sizeof(header), 1, file) != 1) {
        if (verbose)
            fprintf(stderr, "%s: failed to read ELF header\n", path);

        fclose(file);
        return -1;
    }

    if (memcmp(header.e_ident, ELFMAG, SELFMAG) != 0) {
        if (verbose)
            fprintf(stderr, "%s: invalid ELF magic\n", path);

        fclose(file);
        return -1;
    }

    if (header.e_ident[EI_CLASS] != ELFCLASS64) {
        if (verbose)
            fprintf(stderr, "%s: unsupported ELF class: %u\n", path,
                    header.e_ident[EI_CLASS]);

        fclose(file);
        return -1;
    }

    if (header.e_ident[EI_DATA] != ELFDATA2LSB) {
        if (verbose)
            fprintf(stderr, "%s: unsupported ELF data encoding: %u\n", path,
                    header.e_ident[EI_DATA]);

        fclose(file);
        return -1;
    }

    if (!supported_machine(header.e_machine)) {
        if (verbose)
            fprintf(stderr, "%s: unsupported ELF architecture: %u\n", path,
                    header.e_machine);

        fclose(file);
        return -1;
    }

    if (header.e_version != EV_CURRENT) {
        if (verbose)
            fprintf(stderr, "%s: unsupported ELF version: %u\n", path,
                    header.e_version);

        fclose(file);
        return -1;
    }

    if (header.e_ehsize != sizeof(Elf64_Ehdr)) {
        if (verbose)
            fprintf(stderr, "%s: invalid ELF header size\n", path);

        fclose(file);
        return -1;
    }

    if (header.e_phnum > 0 && header.e_phentsize != sizeof(Elf64_Phdr)) {
        if (verbose)
            fprintf(stderr, "%s: invalid program header size\n", path);

        fclose(file);
        return -1;
    }

    elf->machine                = header.e_machine;
    elf->elf_class              = header.e_ident[EI_CLASS];

    Elf64_Phdr *program_headers = NULL;

    if (header.e_phnum > 0) {
        program_headers = malloc(header.e_phnum * sizeof(*program_headers));

        if (program_headers == NULL) {
            if (verbose)
                fprintf(stderr, "%s: failed to allocate program headers: %s\n",
                        path, strerror(errno));

            fclose(file);
            return -1;
        }

        if (fseek(file, ( long )header.e_phoff, SEEK_SET) != 0 ||
            fread(program_headers, sizeof(*program_headers), header.e_phnum,
                  file) != header.e_phnum) {
            if (verbose)
                fprintf(stderr, "%s: failed to read program headers\n", path);

            free(program_headers);
            fclose(file);
            return -1;
        }
    }

    Elf64_Phdr *dynamic_header = NULL;

    for (size_t i = 0; i < header.e_phnum; ++i) {
        Elf64_Phdr *program_header = &program_headers[i];

        if (program_header->p_type == PT_INTERP) {
            if (elf->interpreter != NULL) {
                if (verbose)
                    fprintf(stderr, "%s: multiple PT_INTERP segments\n", path);

                free(program_headers);
                fclose(file);
                forge_elf_free(elf);
                return -1;
            }

            if (program_header->p_filesz == 0) {
                if (verbose)
                    fprintf(stderr, "%s: invalid PT_INTERP size\n", path);

                free(program_headers);
                fclose(file);
                forge_elf_free(elf);
                return -1;
            }

            elf->interpreter = malloc(program_header->p_filesz);

            if (elf->interpreter == NULL) {
                if (verbose)
                    fprintf(stderr, "%s: failed to allocate interpreter: %s\n",
                            path, strerror(errno));

                free(program_headers);
                fclose(file);
                forge_elf_free(elf);
                return -1;
            }

            if (fseek(file, ( long )program_header->p_offset, SEEK_SET) != 0 ||
                fread(elf->interpreter, 1, program_header->p_filesz, file) !=
                        program_header->p_filesz) {
                if (verbose)
                    fprintf(stderr, "%s: failed to read interpreter\n", path);

                free(program_headers);
                fclose(file);
                forge_elf_free(elf);
                return -1;
            }

            if (elf->interpreter[program_header->p_filesz - 1] != '\0') {
                if (verbose)
                    fprintf(stderr, "%s: invalid PT_INTERP string\n", path);

                free(program_headers);
                fclose(file);
                forge_elf_free(elf);
                return -1;
            }
        }

        if (program_header->p_type == PT_DYNAMIC) {
            dynamic_header = program_header;
            elf->dynamic   = 1;
        }
    }

    if (!elf->dynamic) {
        free(program_headers);
        fclose(file);
        return 0;
    }

    uint64_t strtab_address = 0;
    uint64_t strtab_size    = 0;

    uint64_t rpath_offset   = 0;
    uint64_t runpath_offset = 0;

    int have_rpath          = 0;
    int have_runpath        = 0;

    Elf64_Dyn dynamic;

    if (fseek(file, ( long )dynamic_header->p_offset, SEEK_SET) != 0) {
        if (verbose)
            fprintf(stderr, "%s: failed to seek to dynamic section\n", path);

        free(program_headers);
        fclose(file);
        forge_elf_free(elf);
        return -1;
    }

    while (fread(&dynamic, sizeof(dynamic), 1, file) == 1) {
        if (dynamic.d_tag == DT_NULL)
            break;

        if (dynamic.d_tag == DT_STRTAB)
            strtab_address = dynamic.d_un.d_ptr;

        if (dynamic.d_tag == DT_STRSZ)
            strtab_size = dynamic.d_un.d_val;

        if (dynamic.d_tag == DT_RPATH) {
            rpath_offset = dynamic.d_un.d_val;
            have_rpath   = 1;
        }

        if (dynamic.d_tag == DT_RUNPATH) {
            runpath_offset = dynamic.d_un.d_val;
            have_runpath   = 1;
        }
    }

    if (strtab_address == 0 || strtab_size == 0) {
        if (verbose)
            fprintf(stderr, "%s: missing dynamic string table\n", path);

        free(program_headers);
        fclose(file);
        forge_elf_free(elf);
        return -1;
    }

    uint64_t strtab_offset = 0;
    int      found_strtab  = 0;

    for (size_t i = 0; i < header.e_phnum; ++i) {
        Elf64_Phdr *program_header = &program_headers[i];

        if (program_header->p_type != PT_LOAD)
            continue;

        uint64_t start = program_header->p_vaddr;
        uint64_t end   = start + program_header->p_memsz;

        if (strtab_address >= start && strtab_address < end) {
            strtab_offset = program_header->p_offset + (strtab_address - start);

            found_strtab  = 1;
            break;
        }
    }

    if (!found_strtab) {
        if (verbose)
            fprintf(stderr, "%s: failed to locate dynamic string table\n",
                    path);

        free(program_headers);
        fclose(file);
        forge_elf_free(elf);
        return -1;
    }

    char *strtab = malloc(strtab_size);

    if (strtab == NULL) {
        if (verbose)
            fprintf(stderr, "%s: failed to allocate string table: %s\n", path,
                    strerror(errno));

        free(program_headers);
        fclose(file);
        forge_elf_free(elf);
        return -1;
    }

    if (fseek(file, ( long )strtab_offset, SEEK_SET) != 0 ||
        fread(strtab, 1, strtab_size, file) != strtab_size) {
        if (verbose)
            fprintf(stderr, "%s: failed to read dynamic string table\n", path);

        free(strtab);
        free(program_headers);
        fclose(file);
        forge_elf_free(elf);
        return -1;
    }

    if (have_rpath) {
        if (rpath_offset >= strtab_size) {
            if (verbose)
                fprintf(stderr, "%s: invalid DT_RPATH string offset\n", path);

            free(strtab);
            free(program_headers);
            fclose(file);
            forge_elf_free(elf);
            return -1;
        }

        const char *rpath   = strtab + rpath_offset;

        size_t rpath_length = strnlen(rpath, strtab_size - rpath_offset);

        if (rpath_length == strtab_size - rpath_offset) {
            if (verbose)
                fprintf(stderr, "%s: unterminated DT_RPATH string\n", path);

            free(strtab);
            free(program_headers);
            fclose(file);
            forge_elf_free(elf);
            return -1;
        }

        elf->rpath = strdup(rpath);

        if (elf->rpath == NULL) {
            if (verbose)
                fprintf(stderr, "%s: failed to allocate DT_RPATH: %s\n", path,
                        strerror(errno));

            free(strtab);
            free(program_headers);
            fclose(file);
            forge_elf_free(elf);
            return -1;
        }
    }

    if (have_runpath) {
        if (runpath_offset >= strtab_size) {
            if (verbose)
                fprintf(stderr, "%s: invalid DT_RUNPATH string offset\n", path);

            free(strtab);
            free(program_headers);
            fclose(file);
            forge_elf_free(elf);
            return -1;
        }

        const char *runpath   = strtab + runpath_offset;

        size_t runpath_length = strnlen(runpath, strtab_size - runpath_offset);

        if (runpath_length == strtab_size - runpath_offset) {
            if (verbose)
                fprintf(stderr, "%s: unterminated DT_RUNPATH string\n", path);

            free(strtab);
            free(program_headers);
            fclose(file);
            forge_elf_free(elf);
            return -1;
        }

        elf->runpath = strdup(runpath);

        if (elf->runpath == NULL) {
            if (verbose)
                fprintf(stderr, "%s: failed to allocate DT_RUNPATH: %s\n", path,
                        strerror(errno));

            free(strtab);
            free(program_headers);
            fclose(file);
            forge_elf_free(elf);
            return -1;
        }
    }

    if (fseek(file, ( long )dynamic_header->p_offset, SEEK_SET) != 0) {
        if (verbose)
            fprintf(stderr, "%s: failed to seek to dynamic section\n", path);

        free(strtab);
        free(program_headers);
        fclose(file);
        forge_elf_free(elf);
        return -1;
    }

    while (fread(&dynamic, sizeof(dynamic), 1, file) == 1) {
        if (dynamic.d_tag == DT_NULL)
            break;

        if (dynamic.d_tag != DT_NEEDED)
            continue;

        uint64_t offset = dynamic.d_un.d_val;

        if (offset >= strtab_size) {
            if (verbose)
                fprintf(stderr, "%s: invalid DT_NEEDED string offset\n", path);

            free(strtab);
            free(program_headers);
            fclose(file);
            forge_elf_free(elf);
            return -1;
        }

        const char *name   = strtab + offset;

        size_t name_length = strnlen(name, strtab_size - offset);

        if (name_length == strtab_size - offset) {
            if (verbose)
                fprintf(stderr, "%s: unterminated DT_NEEDED string\n", path);

            free(strtab);
            free(program_headers);
            fclose(file);
            forge_elf_free(elf);
            return -1;
        }

        size_t new_count = elf->needed_count + 1;

        if (new_count > SIZE_MAX / sizeof(*elf->needed)) {
            if (verbose)
                fprintf(stderr, "%s: too many dependencies\n", path);

            free(strtab);
            free(program_headers);
            fclose(file);
            forge_elf_free(elf);
            return -1;
        }

        char **needed = realloc(elf->needed, new_count * sizeof(*needed));

        if (needed == NULL) {
            if (verbose)
                fprintf(stderr, "%s: failed to allocate dependencies: %s\n",
                        path, strerror(errno));

            free(strtab);
            free(program_headers);
            fclose(file);
            forge_elf_free(elf);
            return -1;
        }

        elf->needed                    = needed;

        elf->needed[elf->needed_count] = strdup(name);

        if (elf->needed[elf->needed_count] == NULL) {
            if (verbose)
                fprintf(stderr, "%s: failed to allocate dependency: %s\n", path,
                        strerror(errno));

            free(strtab);
            free(program_headers);
            fclose(file);
            forge_elf_free(elf);
            return -1;
        }

        ++elf->needed_count;
    }

    free(strtab);
    free(program_headers);
    fclose(file);

    return 0;
}

int forge_elf_parse(const char *path, struct forge_elf *elf)
{
    return forge_elf_parse_internal(path, elf, 1);
}

int forge_elf_parse_quiet(const char *path, struct forge_elf *elf)
{
    return forge_elf_parse_internal(path, elf, 0);
}

void forge_elf_free(struct forge_elf *elf)
{
    if (elf == NULL)
        return;

    free(elf->interpreter);
    free(elf->rpath);
    free(elf->runpath);

    for (size_t i = 0; i < elf->needed_count; ++i)
        free(elf->needed[i]);

    free(elf->needed);

    memset(elf, 0, sizeof(*elf));
}
