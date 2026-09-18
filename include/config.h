#ifndef FORGE_CONFIG_H
#define FORGE_CONFIG_H

#include <stddef.h>

struct forge_config {
    struct {
        char *distribution;
        char *version;
        char *architecture;
    } base;

    struct {
        char *output;
    } rootfs;

    struct {
        char **paths;
        size_t count;
    } binaries;
};

int forge_config_parse(const char *path, struct forge_config *config);

int forge_config_validate(const struct forge_config *config);

void forge_config_free(struct forge_config *config);

#endif
