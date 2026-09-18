#include "rootfs.h"

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int forge_rootfs_init(struct forge_rootfs *rootfs, const char *path)
{
    if (rootfs == NULL || path == NULL || path[0] == '\0') {
        errno = EINVAL;
        return -1;
    }

    memset(rootfs, 0, sizeof(*rootfs));

    rootfs->path = strdup(path);
    if (rootfs->path == NULL)
        return -1;

    return 0;
}

int forge_rootfs_path(const struct forge_rootfs *rootfs, const char *source,
                      char **destination)
{
    size_t rootfs_length;
    size_t source_length;
    char  *path;

    if (rootfs == NULL || rootfs->path == NULL || source == NULL ||
        destination == NULL || source[0] != '/') {
        errno = EINVAL;
        return -1;
    }

    rootfs_length = strlen(rootfs->path);
    source_length = strlen(source);

    if (rootfs_length > SIZE_MAX - source_length - 1) {
        errno = ENAMETOOLONG;
        return -1;
    }

    path = malloc(rootfs_length + source_length + 1);
    if (path == NULL)
        return -1;

    memcpy(path, rootfs->path, rootfs_length);
    memcpy(path + rootfs_length, source, source_length);
    path[rootfs_length + source_length] = '\0';

    *destination                        = path;

    return 0;
}

void forge_rootfs_free(struct forge_rootfs *rootfs)
{
    if (rootfs == NULL)
        return;

    free(rootfs->path);
    memset(rootfs, 0, sizeof(*rootfs));
}
