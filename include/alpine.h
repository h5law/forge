#ifndef FORGE_ALPINE_H
#define FORGE_ALPINE_H

#include <stddef.h>

int forge_alpine_parse_version(const char *version, unsigned int *major,
                               unsigned int *minor);

int forge_alpine_prepare(const char *version, const char *architecture,
                         const char *output);

#endif
