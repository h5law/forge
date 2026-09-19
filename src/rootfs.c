#include <rootfs.h>

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define FORGE_ROOTFS_SYMLINK_MAX_DEPTH 40

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

    /*
     * open() applies the process umask to newly created files.
     * Restore the source permission bits explicitly so the rootfs
     * preserves the source file's mode.
     */
    if (fchmod(destination_fd, mode) < 0) {
        fprintf(stderr, "failed to set permissions on %s: %s\n", destination,
                strerror(errno));
        close(destination_fd);
        close(source_fd);
        unlink(destination);
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

static int read_symlink_target(const char *source, char **target)
{
    size_t capacity = 256;

    if (target == NULL) {
        errno = EINVAL;
        return -1;
    }

    *target = NULL;

    for (;;) {
        char *buffer = malloc(capacity);

        if (buffer == NULL)
            return -1;

        ssize_t length = readlink(source, buffer, capacity);

        if (length < 0) {
            fprintf(stderr, "failed to read symlink %s: %s\n", source,
                    strerror(errno));
            free(buffer);
            return -1;
        }

        if (( size_t )length < capacity) {
            buffer[length] = '\0';
            *target        = buffer;
            return 0;
        }

        free(buffer);

        if (capacity > SIZE_MAX / 2) {
            errno = ENAMETOOLONG;
            return -1;
        }

        capacity *= 2;
    }
}

static char *symlink_target_path(const char *source, const char *target)
{
    if (source == NULL || target == NULL) {
        errno = EINVAL;
        return NULL;
    }

    /*
     * Absolute symlink targets are already expressed as paths in the
     * root filesystem.
     */
    if (target[0] == '/')
        return strdup(target);

    /*
     * Relative symlink targets are interpreted relative to the directory
     * containing the symlink.
     */
    const char *slash = strrchr(source, '/');

    if (slash == NULL)
        return strdup(target);

    size_t directory_length = ( size_t )(slash - source);

    if (directory_length > SIZE_MAX - strlen(target) - 2) {
        errno = ENAMETOOLONG;
        return NULL;
    }

    char *path = malloc(directory_length + strlen(target) + 2);

    if (path == NULL)
        return NULL;

    memcpy(path, source, directory_length);
    path[directory_length] = '/';
    strcpy(path + directory_length + 1, target);

    return path;
}

static int copy_entry(const char *source, const char *destination,
                      unsigned int depth);

static int copy_symlink(const char *source, const char *destination,
                        unsigned int depth)
{
    if (depth >= FORGE_ROOTFS_SYMLINK_MAX_DEPTH) {
        errno = ELOOP;

        fprintf(stderr, "too many symlink levels while copying %s\n", source);

        return -1;
    }

    char *target = NULL;

    if (read_symlink_target(source, &target) < 0)
        return -1;

    if (symlink(target, destination) < 0) {
        fprintf(stderr, "failed to create symlink %s -> %s: %s\n", destination,
                target, strerror(errno));
        free(target);
        return -1;
    }

    char *target_path = symlink_target_path(source, target);

    if (target_path == NULL) {
        unlink(destination);
        free(target);
        return -1;
    }

    /*
     * The target must exist in the generated rootfs at the same logical
     * path referenced by the symlink. For example:
     *
     *     /bin/sh -> bash
     *
     * requires:
     *
     *     /bin/bash
     *
     * to exist in the rootfs.
     */
    struct stat target_status;

    if (lstat(target_path, &target_status) < 0) {
        fprintf(stderr, "failed to stat symlink target %s: %s\n", target_path,
                strerror(errno));
        unlink(destination);
        free(target_path);
        free(target);
        return -1;
    }

    char *target_destination = NULL;

    /*
     * forge_rootfs_path() is not used here because target_path is already
     * an absolute path in the source filesystem namespace.
     */
    size_t rootfs_length     = 0;

    char *destination_root   = strdup(destination);

    if (destination_root == NULL) {
        unlink(destination);
        free(target_path);
        free(target);
        return -1;
    }

    char *destination_slash = strrchr(destination_root, '/');

    if (destination_slash == NULL) {
        free(destination_root);
        unlink(destination);
        free(target_path);
        free(target);
        errno = EINVAL;
        return -1;
    }

    /*
     * Find the rootfs prefix by using the destination path and source
     * path lengths. The destination has the form:
     *
     *     rootfs + source
     */
    rootfs_length = strlen(destination) - strlen(source);

    free(destination_root);

    if (rootfs_length == 0) {
        unlink(destination);
        free(target_path);
        free(target);
        errno = EINVAL;
        return -1;
    }

    size_t target_length = strlen(target_path);

    if (rootfs_length > SIZE_MAX - target_length - 1) {
        unlink(destination);
        free(target_path);
        free(target);
        errno = ENAMETOOLONG;
        return -1;
    }

    target_destination = malloc(rootfs_length + target_length + 1);

    if (target_destination == NULL) {
        unlink(destination);
        free(target_path);
        free(target);
        return -1;
    }

    memcpy(target_destination, destination, rootfs_length);
    memcpy(target_destination + rootfs_length, target_path, target_length);
    target_destination[rootfs_length + target_length] = '\0';

    int result = copy_entry(target_path, target_destination, depth + 1);

    free(target_destination);
    free(target_path);
    free(target);

    if (result < 0)
        unlink(destination);

    return result;
}

static int copy_entry(const char *source, const char *destination,
                      unsigned int depth)
{
    struct stat status;

    if (lstat(source, &status) < 0) {
        fprintf(stderr, "failed to stat %s: %s\n", source, strerror(errno));
        return -1;
    }

    if (!S_ISREG(status.st_mode) && !S_ISLNK(status.st_mode)) {
        fprintf(stderr, "source is not a regular file or symlink: %s\n",
                source);
        errno = EINVAL;
        return -1;
    }

    if (create_parent_directories(destination) < 0) {
        fprintf(stderr, "failed to create parent directories for %s: %s\n",
                destination, strerror(errno));
        return -1;
    }

    if (unlink(destination) < 0 && errno != ENOENT) {
        fprintf(stderr, "failed to remove existing %s: %s\n", destination,
                strerror(errno));
        return -1;
    }

    printf("Copying %s -> %s\n", source, destination);

    if (S_ISLNK(status.st_mode))
        return copy_symlink(source, destination, depth);

    return copy_file(source, destination, status.st_mode & 07777);
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

static const char *rootfs_directories[] = {
        "bin",  "etc", "home", "lib", "lib64", "mnt",
        "root", "run", "sbin", "usr", "var",
};

int forge_rootfs_prepare(struct forge_rootfs *rootfs)
{
    if (rootfs == NULL || rootfs->path == NULL) {
        errno = EINVAL;
        return -1;
    }

    if (mkdir(rootfs->path, 0755) < 0) {
        if (errno != EEXIST)
            return -1;

        struct stat status;

        if (stat(rootfs->path, &status) < 0)
            return -1;

        if (!S_ISDIR(status.st_mode)) {
            errno = ENOTDIR;
            return -1;
        }
    }

    for (size_t i = 0;
         i < sizeof(rootfs_directories) / sizeof(rootfs_directories[0]); ++i) {
        size_t rootfs_length = strlen(rootfs->path);
        size_t name_length   = strlen(rootfs_directories[i]);

        if (rootfs_length > SIZE_MAX - name_length - 2) {
            errno = ENAMETOOLONG;
            return -1;
        }

        char *path = malloc(rootfs_length + name_length + 2);

        if (path == NULL)
            return -1;

        snprintf(path, rootfs_length + name_length + 2, "%s/%s", rootfs->path,
                 rootfs_directories[i]);

        if (create_directory(path, 0755) < 0) {
            free(path);
            return -1;
        }

        free(path);
    }

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

    char *destination = NULL;

    if (forge_rootfs_path(rootfs, source, &destination) < 0)
        return -1;

    int result = copy_entry(source, destination, 0);

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
