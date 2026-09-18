#include "rootfs.h"

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int create_directory(const char *path, mode_t mode)
{
    if (mkdir(path, mode) == 0)
        return 0;

    if (errno == EEXIST) {
        struct stat status;

        if (stat(path, &status) == 0 && S_ISDIR(status.st_mode))
            return 0;
    }

    return -1;
}

static int create_parent_directories(const char *path)
{
    char *directory = strdup(path);

    if (directory == NULL)
        return -1;

    char *slash = strrchr(directory, '/');

    if (slash == NULL) {
        free(directory);
        return 0;
    }

    if (slash == directory) {
        free(directory);
        return 0;
    }

    *slash = '\0';

    for (char *current = directory + 1; *current != '\0'; ++current) {
        if (*current != '/')
            continue;

        *current = '\0';

        if (create_directory(directory, 0755) < 0) {
            free(directory);
            return -1;
        }

        *current = '/';
    }

    if (create_directory(directory, 0755) < 0) {
        free(directory);
        return -1;
    }

    free(directory);

    return 0;
}

static int copy_file(const char *source, const char *destination, mode_t mode)
{
    int source_fd = open(source, O_RDONLY);

    if (source_fd < 0) {
        fprintf(stderr, "failed to open %s: %s\n", source, strerror(errno));
        return -1;
    }

    int destination_fd = open(destination, O_WRONLY | O_CREAT | O_TRUNC, mode);

    if (destination_fd < 0) {
        fprintf(stderr, "failed to create %s: %s\n", destination,
                strerror(errno));
        close(source_fd);
        return -1;
    }

    char buffer[65536];

    int result = 0;

    for (;;) {
        ssize_t bytes_read = read(source_fd, buffer, sizeof(buffer));

        if (bytes_read == 0)
            break;

        if (bytes_read < 0) {
            if (errno == EINTR)
                continue;

            fprintf(stderr, "failed to read %s: %s\n", source, strerror(errno));

            result = -1;
            break;
        }

        ssize_t offset = 0;

        while (offset < bytes_read) {
            ssize_t bytes_written = write(destination_fd, buffer + offset,
                                          ( size_t )(bytes_read - offset));

            if (bytes_written < 0) {
                if (errno == EINTR)
                    continue;

                fprintf(stderr, "failed to write %s: %s\n", destination,
                        strerror(errno));

                result = -1;
                break;
            }

            if (bytes_written == 0) {
                errno = EIO;

                fprintf(stderr, "failed to write %s: %s\n", destination,
                        strerror(errno));

                result = -1;
                break;
            }

            offset += bytes_written;
        }

        if (result < 0)
            break;
    }

    if (close(destination_fd) < 0 && result == 0) {
        fprintf(stderr, "failed to close %s: %s\n", destination,
                strerror(errno));
        result = -1;
    }

    if (close(source_fd) < 0 && result == 0) {
        fprintf(stderr, "failed to close %s: %s\n", source, strerror(errno));
        result = -1;
    }

    if (result < 0)
        unlink(destination);

    return result;
}

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

int forge_rootfs_copy(struct forge_rootfs *rootfs, const char *source)
{
    if (rootfs == NULL || rootfs->path == NULL || source == NULL ||
        source[0] != '/') {
        errno = EINVAL;
        return -1;
    }

    struct stat status;

    if (stat(source, &status) < 0) {
        fprintf(stderr, "failed to stat %s: %s\n", source, strerror(errno));
        return -1;
    }

    if (!S_ISREG(status.st_mode)) {
        fprintf(stderr, "source is not a regular file: %s\n", source);
        errno = EINVAL;
        return -1;
    }

    char *destination = NULL;

    if (forge_rootfs_path(rootfs, source, &destination) < 0)
        return -1;

    if (create_parent_directories(destination) < 0) {
        fprintf(stderr, "failed to create parent directories for %s: %s\n",
                destination, strerror(errno));
        free(destination);
        return -1;
    }

    if (unlink(destination) < 0 && errno != ENOENT) {
        fprintf(stderr, "failed to remove existing %s: %s\n", destination,
                strerror(errno));
        free(destination);
        return -1;
    }

    printf("Copying %s -> %s\n", source, destination);

    int result = copy_file(source, destination, status.st_mode & 07777);

    free(destination);

    return result;
}

void forge_rootfs_free(struct forge_rootfs *rootfs)
{
    if (rootfs == NULL)
        return;

    free(rootfs->path);
    memset(rootfs, 0, sizeof(*rootfs));
}
