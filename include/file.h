#ifndef FORGE_FILE_H
#define FORGE_FILE_H

int forge_file_is_script(const char *path);

int forge_file_get_script_interpreter(const char *path, char **interpreter);

#endif
