#include <file.h>

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int forge_file_is_script(const char *path)
{
    if (path == NULL) {
        errno = EINVAL;
        return -1;
    }

    FILE *file = fopen(path, "rb");

    if (file == NULL)
        return -1;

    unsigned char magic[2];

    size_t bytes_read = fread(magic, 1, sizeof(magic), file);

    if (ferror(file)) {
        fprintf(stderr, "failed to read %s: %s\n", path, strerror(errno));
        fclose(file);
        return -1;
    }

    if (fclose(file) != 0) {
        fprintf(stderr, "failed to close %s: %s\n", path, strerror(errno));
        return -1;
    }

    if (bytes_read != sizeof(magic))
        return 0;

    return magic[0] == '#' && magic[1] == '!';
}

int forge_file_get_script_interpreter(const char *path, char **interpreter)
{
    if (path == NULL || interpreter == NULL) {
        errno = EINVAL;
        return -1;
    }

    *interpreter = NULL;

    FILE *file   = fopen(path, "rb");

    if (file == NULL)
        return -1;

    char line[4096];

    if (fgets(line, sizeof(line), file) == NULL) {
        if (ferror(file)) {
            fprintf(stderr, "failed to read %s: %s\n", path, strerror(errno));
        } else {
            errno = ENOEXEC;
        }

        fclose(file);

        return -1;
    }

    if (fclose(file) != 0) {
        fprintf(stderr, "failed to close %s: %s\n", path, strerror(errno));
        return -1;
    }

    if (line[0] != '#' || line[1] != '!') {
        errno = ENOEXEC;
        return -1;
    }

    char *start = line + 2;

    while (*start != '\0' && isspace(( unsigned char )*start)) {
        ++start;
    }

    if (*start == '\0') {
        errno = ENOEXEC;
        return -1;
    }

    char *end = start;

    while (*end != '\0' && !isspace(( unsigned char )*end)) {
        ++end;
    }

    if (end == start) {
        errno = ENOEXEC;
        return -1;
    }

    size_t length = ( size_t )(end - start);

    char *result  = malloc(length + 1);

    if (result == NULL) {
        fprintf(stderr, "failed to allocate script interpreter: %s\n",
                strerror(errno));
        return -1;
    }

    memcpy(result, start, length);
    result[length] = '\0';

    *interpreter   = result;

    return 0;
}
