#ifndef FORGE_ELF_H
#define FORGE_ELF_H

#include <stddef.h>

struct forge_elf {
    unsigned int machine;
    unsigned int elf_class;

    char *interpreter;

    char **needed;
    size_t needed_count;

    char *rpath;
    char *runpath;

    int dynamic;
};

int forge_elf_parse(const char *path, struct forge_elf *elf);

int forge_elf_parse_quiet(const char *path, struct forge_elf *elf);

void forge_elf_free(struct forge_elf *elf);

#endif
