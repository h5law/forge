#include <alpine.h>

#include <errno.h>
#include <ftw.h>
#include <limits.h>
#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define ALPINE_BASE_URL      "https://dl-cdn.alpinelinux.org/alpine"
#define ALPINE_RELEASES_PATH "releases"
#define ALPINE_ARCH_X86_64   "x86_64"

static int run_command(char *const argv[], const char *directory)
{
    pid_t pid = fork();

    if (pid < 0) {
        fprintf(stderr, "fork: %s\n", strerror(errno));
        return -1;
    }

    if (pid == 0) {
        if (directory != NULL && chdir(directory) < 0) {
            fprintf(stderr, "chdir %s: %s\n", directory, strerror(errno));
            _exit(127);
        }

        execvp(argv[0], argv);

        fprintf(stderr, "%s: %s\n", argv[0], strerror(errno));

        _exit(127);
    }

    int status;

    if (waitpid(pid, &status, 0) < 0) {
        fprintf(stderr, "waitpid: %s\n", strerror(errno));
        return -1;
    }

    if (!WIFEXITED(status)) {
        fprintf(stderr, "%s terminated abnormally\n", argv[0]);

        return -1;
    }

    if (WEXITSTATUS(status) != 0) {
        fprintf(stderr, "%s exited with status %d\n", argv[0],
                WEXITSTATUS(status));

        return -1;
    }

    return 0;
}

static int directory_exists(const char *path)
{
    struct stat status;

    if (stat(path, &status) < 0)
        return 0;

    return S_ISDIR(status.st_mode);
}

static int create_directory(const char *path)
{
    if (mkdir(path, 0755) == 0)
        return 0;

    if (errno == EEXIST && directory_exists(path))
        return 0;

    fprintf(stderr, "mkdir %s: %s\n", path, strerror(errno));

    return -1;
}

int forge_alpine_parse_version(const char *version, unsigned int *major,
                               unsigned int *minor)
{
    unsigned int patch;
    char         extra;

    if (sscanf(version, "%u.%u.%u%c", major, minor, &patch, &extra) != 3) {
        fprintf(stderr, "invalid Alpine version: %s\n", version);

        return -1;
    }

    return 0;
}

static int download_release(const char *version, const char *architecture,
                            char *archive, size_t archive_size, char *checksum,
                            size_t checksum_size)
{
    unsigned int major;
    unsigned int minor;

    if (forge_alpine_parse_version(version, &major, &minor) < 0)
        return -1;

    char filename[PATH_MAX];

    int length =
            snprintf(filename, sizeof(filename),
                     "alpine-minirootfs-%s-%s.tar.gz", version, architecture);

    if (length < 0 || ( size_t )length >= sizeof(filename)) {
        fprintf(stderr, "Alpine filename is too long\n");
        return -1;
    }

    length = snprintf(archive, archive_size, "/tmp/%s", filename);

    if (length < 0 || ( size_t )length >= archive_size) {
        fprintf(stderr, "archive path is too long\n");
        return -1;
    }

    length = snprintf(checksum, checksum_size, "/tmp/%s.sha256", filename);

    if (length < 0 || ( size_t )length >= checksum_size) {
        fprintf(stderr, "checksum path is too long\n");
        return -1;
    }

    char url[PATH_MAX];

    length = snprintf(url, sizeof(url), "%s/v%u.%u/%s/%s/%s", ALPINE_BASE_URL,
                      major, minor, ALPINE_RELEASES_PATH, architecture,
                      filename);

    if (length < 0 || ( size_t )length >= sizeof(url)) {
        fprintf(stderr, "Alpine URL is too long\n");
        return -1;
    }

    char checksum_url[PATH_MAX];

    length = snprintf(checksum_url, sizeof(checksum_url), "%s.sha256", url);

    if (length < 0 || ( size_t )length >= sizeof(checksum_url)) {
        fprintf(stderr, "Alpine checksum URL is too long\n");
        return -1;
    }

    printf("Downloading %s...\n", filename);

    char *download_archive[] = {
            "curl",     "--fail", "--silent", "--show-error", "--location",
            "--output", archive,  url,        NULL,
    };

    if (run_command(download_archive, NULL) < 0)
        return -1;

    char *download_checksum[] = {
            "curl",     "--fail", "--silent",   "--show-error", "--location",
            "--output", checksum, checksum_url, NULL,
    };

    if (run_command(download_checksum, NULL) < 0)
        return -1;

    return 0;
}

static int verify_release(const char *checksum)
{
    printf("Verifying archive...\n");

    const char *checksum_name = strrchr(checksum, '/');

    if (checksum_name == NULL)
        checksum_name = checksum;
    else
        ++checksum_name;

    char *verify[] = {
            "sha256sum",
            "--check",
            ( char * )checksum_name,
            NULL,
    };

    return run_command(verify, "/tmp");
}

static int extract_release(const char *archive, const char *output)
{
    printf("Extracting rootfs...\n");

    char *extract[] = {
            "tar", "-xzf", ( char * )archive, "-C", ( char * )output, NULL,
    };

    return run_command(extract, NULL);
}

static int remove_rootfs_entry(const char *path, const struct stat *status,
                               int type, struct FTW *buffer)
{
    ( void )status;
    ( void )type;
    ( void )buffer;

    if (remove(path) < 0) {
        fprintf(stderr, "remove %s: %s\n", path, strerror(errno));

        return -1;
    }

    return 0;
}

static int remove_rootfs(const char *path)
{
    return nftw(path, remove_rootfs_entry, 64, FTW_DEPTH | FTW_PHYS);
}

int forge_alpine_prepare(const char *version, const char *architecture,
                         const char *output)
{
    if (version == NULL || architecture == NULL || output == NULL) {
        fprintf(stderr, "invalid Alpine configuration\n");
        return -1;
    }

    if (strcmp(architecture, ALPINE_ARCH_X86_64) != 0) {
        fprintf(stderr, "unsupported Alpine architecture: %s\n", architecture);

        return -1;
    }

    if (directory_exists(output)) {
        fprintf(stderr, "rootfs output already exists: %s\n", output);

        return -1;
    }

    if (create_directory(output) < 0)
        return -1;

    char archive[PATH_MAX];
    char checksum[PATH_MAX];

    if (download_release(version, architecture, archive, sizeof(archive),
                         checksum, sizeof(checksum)) < 0) {
        goto cleanup;
    }

    if (verify_release(checksum) < 0)
        goto cleanup;

    if (extract_release(archive, output) < 0)
        goto cleanup;

    unlink(archive);
    unlink(checksum);

    printf("Rootfs created: %s\n", output);

    return 0;

cleanup:
    unlink(archive);
    unlink(checksum);
    remove_rootfs(output);

    return -1;
}
