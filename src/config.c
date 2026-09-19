#include <config.h>

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

enum section {
    SECTION_NONE,
    SECTION_BASE,
    SECTION_ROOTFS,
    SECTION_BINARIES,
};

struct parser {
    FILE       *file;
    const char *path;

    size_t line;
    size_t column;

    int current;
    int has_current;

    enum section section;
};

static void parser_error(const struct parser *parser, const char *message)
{
    fprintf(stderr, "%s:%zu:%zu: %s\n", parser->path, parser->line,
            parser->column, message);
}

static int parser_get(struct parser *parser)
{
    int c = fgetc(parser->file);

    if (c == EOF) {
        parser->has_current = 0;
        return EOF;
    }

    parser->current     = c;
    parser->has_current = 1;

    if (c == '\n') {
        ++parser->line;
        parser->column = 0;
    } else {
        ++parser->column;
    }

    return c;
}

static int parser_peek(struct parser *parser)
{
    if (!parser->has_current)
        return parser_get(parser);

    return parser->current;
}

static int parser_consume(struct parser *parser)
{
    int c               = parser_peek(parser);

    parser->has_current = 0;

    return c;
}

static void skip_spaces(struct parser *parser)
{
    while (parser_peek(parser) == ' ' || parser_peek(parser) == '\t' ||
           parser_peek(parser) == '\r') {
        parser_consume(parser);
    }
}

static void skip_comment(struct parser *parser)
{
    if (parser_peek(parser) != '#')
        return;

    while (parser_peek(parser) != EOF && parser_peek(parser) != '\n') {
        parser_consume(parser);
    }
}

static void skip_line_ending(struct parser *parser)
{
    skip_spaces(parser);

    if (parser_peek(parser) == '#')
        skip_comment(parser);

    if (parser_peek(parser) == '\n')
        parser_consume(parser);
}

static void skip_whitespace_and_comments(struct parser *parser)
{
    for (;;) {
        skip_spaces(parser);

        if (parser_peek(parser) == '#') {
            skip_comment(parser);
            continue;
        }

        if (parser_peek(parser) == '\n') {
            parser_consume(parser);
            continue;
        }

        return;
    }
}

static int is_key_char(int c)
{
    return isalnum(( unsigned char )c) || c == '_' || c == '-';
}

static char *parse_key(struct parser *parser)
{
    skip_spaces(parser);

    size_t capacity = 16;
    size_t length   = 0;

    char *key       = malloc(capacity);

    if (key == NULL) {
        perror("malloc");
        return NULL;
    }

    while (is_key_char(parser_peek(parser))) {
        int c = parser_consume(parser);

        if (length + 1 >= capacity) {
            capacity      *= 2;

            char *new_key  = realloc(key, capacity);

            if (new_key == NULL) {
                perror("realloc");
                free(key);
                return NULL;
            }

            key = new_key;
        }

        key[length] = ( char )c;
        ++length;
    }

    if (length == 0) {
        parser_error(parser, "expected key");
        free(key);
        return NULL;
    }

    key[length] = '\0';

    return key;
}

static int append_char(char **buffer, size_t *length, size_t *capacity, char c)
{
    if (*length + 1 >= *capacity) {
        size_t new_capacity = *capacity * 2;

        char *new_buffer    = realloc(*buffer, new_capacity);

        if (new_buffer == NULL) {
            perror("realloc");
            return -1;
        }

        *buffer   = new_buffer;
        *capacity = new_capacity;
    }

    (*buffer)[*length] = c;
    ++(*length);

    return 0;
}

static int parse_escape(struct parser *parser, char *output)
{
    int c = parser_consume(parser);

    switch (c) {
    case 'b':
        *output = '\b';
        return 0;

    case 't':
        *output = '\t';
        return 0;

    case 'n':
        *output = '\n';
        return 0;

    case 'f':
        *output = '\f';
        return 0;

    case 'r':
        *output = '\r';
        return 0;

    case '"':
        *output = '"';
        return 0;

    case '\\':
        *output = '\\';
        return 0;

    default:
        parser_error(parser, "unsupported escape sequence");

        return -1;
    }
}

static char *parse_string(struct parser *parser)
{
    if (parser_consume(parser) != '"') {
        parser_error(parser, "expected '\"'");
        return NULL;
    }

    size_t capacity = 32;
    size_t length   = 0;

    char *value     = malloc(capacity);

    if (value == NULL) {
        perror("malloc");
        return NULL;
    }

    for (;;) {
        int c = parser_peek(parser);

        if (c == EOF || c == '\n') {
            parser_error(parser, "unterminated string");

            free(value);
            return NULL;
        }

        if (c == '"') {
            parser_consume(parser);
            break;
        }

        if (c == '\\') {
            parser_consume(parser);

            char escaped;

            if (parse_escape(parser, &escaped) < 0) {
                free(value);
                return NULL;
            }

            if (append_char(&value, &length, &capacity, escaped) < 0) {
                free(value);
                return NULL;
            }

            continue;
        }

        parser_consume(parser);

        if (append_char(&value, &length, &capacity, ( char )c) < 0) {
            free(value);
            return NULL;
        }
    }

    value[length] = '\0';

    return value;
}

static int add_binary(struct parser *parser, struct forge_config *config,
                      char *path)
{
    if (path[0] != '/') {
        parser_error(parser, "binary path must be absolute");

        free(path);
        return -1;
    }

    char **paths = realloc(config->binaries.paths,
                           sizeof(*paths) * (config->binaries.count + 1));

    if (paths == NULL) {
        perror("realloc");
        free(path);
        return -1;
    }

    config->binaries.paths                         = paths;
    config->binaries.paths[config->binaries.count] = path;

    ++config->binaries.count;

    return 0;
}

static int parse_binary_array(struct parser       *parser,
                              struct forge_config *config)
{
    if (parser_consume(parser) != '[') {
        parser_error(parser, "expected '['");
        return -1;
    }

    for (;;) {
        skip_whitespace_and_comments(parser);

        if (parser_peek(parser) == ']') {
            parser_consume(parser);
            return 0;
        }

        if (parser_peek(parser) != '"') {
            parser_error(parser, "expected string in array");

            return -1;
        }

        char *path = parse_string(parser);

        if (path == NULL)
            return -1;

        if (add_binary(parser, config, path) < 0)
            return -1;

        skip_spaces(parser);

        if (parser_peek(parser) == '#') {
            skip_comment(parser);
            continue;
        }

        if (parser_peek(parser) == ']') {
            parser_consume(parser);
            return 0;
        }

        if (parser_peek(parser) != ',') {
            parser_error(parser, "expected ',' or ']'");

            return -1;
        }

        parser_consume(parser);
    }
}

static int parse_section(struct parser *parser)
{
    if (parser_consume(parser) != '[') {
        parser_error(parser, "expected '['");
        return -1;
    }

    skip_spaces(parser);

    char *name = parse_key(parser);

    if (name == NULL)
        return -1;

    skip_spaces(parser);

    if (parser_consume(parser) != ']') {
        parser_error(parser, "expected ']'");
        free(name);
        return -1;
    }

    skip_line_ending(parser);

    if (strcmp(name, "base") == 0)
        parser->section = SECTION_BASE;
    else if (strcmp(name, "rootfs") == 0)
        parser->section = SECTION_ROOTFS;
    else if (strcmp(name, "binaries") == 0)
        parser->section = SECTION_BINARIES;
    else {
        parser_error(parser, "unknown section");
        free(name);
        return -1;
    }

    free(name);

    return 0;
}

static int parse_base_assignment(struct parser       *parser,
                                 struct forge_config *config, const char *key)
{
    char **destination;

    if (strcmp(key, "distribution") == 0)
        destination = &config->base.distribution;
    else if (strcmp(key, "version") == 0)
        destination = &config->base.version;
    else if (strcmp(key, "architecture") == 0)
        destination = &config->base.architecture;
    else {
        parser_error(parser, "unknown key in [base]");

        return -1;
    }

    if (*destination != NULL) {
        parser_error(parser, "duplicate key");
        return -1;
    }

    if (parser_consume(parser) != '=') {
        parser_error(parser, "expected '='");
        return -1;
    }

    skip_spaces(parser);

    if (parser_peek(parser) != '"') {
        parser_error(parser, "expected string value");

        return -1;
    }

    *destination = parse_string(parser);

    return *destination == NULL ? -1 : 0;
}

static int parse_rootfs_assignment(struct parser       *parser,
                                   struct forge_config *config, const char *key)
{
    if (strcmp(key, "output") != 0) {
        parser_error(parser, "unknown key in [rootfs]");

        return -1;
    }

    if (config->rootfs.output != NULL) {
        parser_error(parser, "duplicate key");
        return -1;
    }

    if (parser_consume(parser) != '=') {
        parser_error(parser, "expected '='");
        return -1;
    }

    skip_spaces(parser);

    if (parser_peek(parser) != '"') {
        parser_error(parser, "expected string value");

        return -1;
    }

    config->rootfs.output = parse_string(parser);

    return config->rootfs.output == NULL ? -1 : 0;
}

static int parse_binaries_assignment(struct parser       *parser,
                                     struct forge_config *config,
                                     const char          *key)
{
    if (strcmp(key, "paths") != 0) {
        parser_error(parser, "unknown key in [binaries]");

        return -1;
    }

    if (config->binaries.count != 0) {
        parser_error(parser, "duplicate key");
        return -1;
    }

    if (parser_consume(parser) != '=') {
        parser_error(parser, "expected '='");
        return -1;
    }

    skip_spaces(parser);

    return parse_binary_array(parser, config);
}

static int parse_assignment(struct parser *parser, struct forge_config *config)
{
    char *key = parse_key(parser);

    if (key == NULL)
        return -1;

    skip_spaces(parser);

    int result;

    switch (parser->section) {
    case SECTION_BASE:
        result = parse_base_assignment(parser, config, key);
        break;

    case SECTION_ROOTFS:
        result = parse_rootfs_assignment(parser, config, key);
        break;

    case SECTION_BINARIES:
        result = parse_binaries_assignment(parser, config, key);
        break;

    case SECTION_NONE:
        parser_error(parser, "assignment outside of a section");

        result = -1;
        break;
    }

    free(key);

    return result;
}

int forge_config_validate(const struct forge_config *config)
{
    if (config == NULL) {
        fprintf(stderr, "configuration is NULL\n");

        return -1;
    }

    if (config->base.distribution == NULL ||
        config->base.distribution[0] == '\0') {
        fprintf(stderr, "missing required key: base.distribution\n");

        return -1;
    }

    if (strcmp(config->base.distribution, "alpine") != 0 &&
        strcmp(config->base.distribution, "none") != 0) {
        fprintf(stderr, "unsupported distribution: %s\n",
                config->base.distribution);

        return -1;
    }

    if (strcmp(config->base.distribution, "alpine") == 0 &&
        (config->base.version == NULL || config->base.version[0] == '\0')) {
        fprintf(stderr, "missing required key: base.version\n");

        return -1;
    }

    if (config->base.architecture == NULL ||
        config->base.architecture[0] == '\0') {
        fprintf(stderr, "missing required key: base.architecture\n");

        return -1;
    }

    if (strcmp(config->base.architecture, "x86_64") != 0 &&
        strcmp(config->base.architecture, "aarch64") != 0 &&
        strcmp(config->base.architecture, "riscv64") != 0) {
        fprintf(stderr, "unsupported architecture: %s\n",
                config->base.architecture);

        return -1;
    }

    if (config->rootfs.output == NULL || config->rootfs.output[0] == '\0') {
        fprintf(stderr, "missing required key: rootfs.output\n");

        return -1;
    }

    if (config->binaries.count == 0) {
        fprintf(stderr, "no binaries specified\n");

        return -1;
    }

    for (size_t i = 0; i < config->binaries.count; ++i) {
        const char *path = config->binaries.paths[i];
        struct stat status;

        if (path == NULL || path[0] == '\0') {
            fprintf(stderr, "binary path %zu is empty\n", i);

            return -1;
        }

        if (path[0] != '/') {
            fprintf(stderr, "binary path must be absolute: %s\n", path);

            return -1;
        }

        if (stat(path, &status) < 0) {
            fprintf(stderr, "binary does not exist: %s: %s\n", path,
                    strerror(errno));

            return -1;
        }

        if (!S_ISREG(status.st_mode)) {
            fprintf(stderr, "binary is not a regular file: %s\n", path);

            return -1;
        }

        if (access(path, X_OK) < 0) {
            fprintf(stderr, "binary is not executable: %s: %s\n", path,
                    strerror(errno));

            return -1;
        }
    }

    return 0;
}

int forge_config_parse(const char *path, struct forge_config *config)
{
    memset(config, 0, sizeof(*config));

    struct parser parser = {
            .path    = path,
            .line    = 1,
            .column  = 0,
            .section = SECTION_NONE,
    };

    parser.file = fopen(path, "r");

    if (parser.file == NULL) {
        fprintf(stderr, "%s: %s\n", path, strerror(errno));

        return -1;
    }

    int result = -1;

    for (;;) {
        skip_whitespace_and_comments(&parser);

        if (parser_peek(&parser) == EOF)
            break;

        if (parser_peek(&parser) == '[') {
            if (parse_section(&parser) < 0)
                goto cleanup;
        } else {
            if (parse_assignment(&parser, config) < 0)
                goto cleanup;
        }

        skip_line_ending(&parser);

        if (parser_peek(&parser) != EOF && parser_peek(&parser) != '\n' &&
            parser_peek(&parser) != '[' && !is_key_char(parser_peek(&parser))) {
            parser_error(&parser, "unexpected characters");

            goto cleanup;
        }
    }

    if (forge_config_validate(config) < 0)
        goto cleanup;

    result = 0;

cleanup:
    fclose(parser.file);

    if (result < 0)
        forge_config_free(config);

    return result;
}

void forge_config_free(struct forge_config *config)
{
    free(config->base.distribution);
    free(config->base.version);
    free(config->base.architecture);

    free(config->rootfs.output);

    for (size_t i = 0; i < config->binaries.count; ++i)
        free(config->binaries.paths[i]);

    free(config->binaries.paths);

    memset(config, 0, sizeof(*config));
}
