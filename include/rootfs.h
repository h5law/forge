#ifndef FORGE_ROOTFS_H
#define FORGE_ROOTFS_H

struct forge_rootfs {
    char *path;
};

int forge_rootfs_init(struct forge_rootfs *rootfs, const char *path);

int forge_rootfs_path(const struct forge_rootfs *rootfs, const char *source,
                      char **destination);

int forge_rootfs_copy(struct forge_rootfs *rootfs, const char *source);

void forge_rootfs_free(struct forge_rootfs *rootfs);

#endif
