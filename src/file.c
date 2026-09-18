#include <file.h>

#include <errno.h>
#include <stdio.h>
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
