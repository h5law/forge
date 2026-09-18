#include <resolver.h>

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const char *library_paths[] = {
        "/lib",
        "/lib64",
        "/usr/lib",
        "/usr/lib64",
};

char *forge_resolve_library(const char *name)
{
    if (name == NULL || name[0] == '\0')
        return NULL;

    size_t library_path_count =
            sizeof(library_paths) / sizeof(library_paths[0]);

    for (size_t i = 0; i < library_path_count; ++i) {
        const char *directory   = library_paths[i];

        size_t directory_length = strlen(directory);
        size_t name_length      = strlen(name);

        if (directory_length > SIZE_MAX - name_length - 2)
            return NULL;

        size_t length = directory_length + 1 + name_length + 1;

        char *path    = malloc(length);

        if (path == NULL) {
            fprintf(stderr, "failed to allocate library path: %s\n",
                    strerror(errno));

            return NULL;
        }

        snprintf(path, length, "%s/%s", directory, name);

        if (access(path, F_OK) == 0)
            return path;

        free(path);
    }

    return NULL;
}
