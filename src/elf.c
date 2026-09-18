#include <elf_parser.h>

#include <errno.h>
#include <elf.h>
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

    if (header.e_machine != EM_X86_64) {
        fprintf(stderr, "%s: unsupported ELF architecture: %u\n", path,
                header.e_machine);

        fclose(file);
        return -1;
    }

    elf->machine   = header.e_machine;
    elf->elf_class = header.e_ident[EI_CLASS];

    if (fclose(file) != 0) {
        fprintf(stderr, "failed to close %s: %s\n", path, strerror(errno));

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
