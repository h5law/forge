#include <alpine.h>
#include <config.h>

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FORGE_VERSION "0.2.0-alpha"

enum command {
    COMMAND_NONE,
    COMMAND_BUILD,
    COMMAND_VALIDATE,
};

static void usage(FILE *stream, const char *program)
{
    fprintf(stream,
            "usage: %s <command> [options] [config]\n"
            "\n"
            "Build and manage minimal Linux root filesystems.\n"
            "\n"
            "Commands:\n"
            "  build      Build a rootfs from a configuration\n"
            "  validate   Parse and validate a configuration\n"
            "\n"
            "Options:\n"
            "  -h, --help       Show this help message\n"
            "  -V, --version    Show version information\n",
            program);
}

static void command_usage(FILE *stream, const char *program,
                          enum command command)
{
    switch (command) {
    case COMMAND_BUILD:
        fprintf(stream,
                "usage: %s build [options] <config>\n"
                "\n"
                "Build a rootfs from a configuration file.\n"
                "\n"
                "Options:\n"
                "  -h, --help       Show this help message\n"
                "  -V, --version    Show version information\n",
                program);
        break;

    case COMMAND_VALIDATE:
        fprintf(stream,
                "usage: %s validate [options] <config>\n"
                "\n"
                "Parse and validate a configuration file.\n"
                "\n"
                "Options:\n"
                "  -h, --help       Show this help message\n"
                "  -V, --version    Show version information\n",
                program);
        break;

    case COMMAND_NONE:
        usage(stream, program);
        break;
    }
}

static void version(void) { printf("forge %s\n", FORGE_VERSION); }

static enum command parse_command(const char *command)
{
    if (strcmp(command, "build") == 0)
        return COMMAND_BUILD;

    if (strcmp(command, "validate") == 0)
        return COMMAND_VALIDATE;

    return COMMAND_NONE;
}

static int run_validate(const char *path)
{
    struct forge_config config;

    if (forge_config_parse(path, &config) < 0)
        return EXIT_FAILURE;

    forge_config_free(&config);

    printf("configuration is valid: %s\n", path);

    return EXIT_SUCCESS;
}

static int run_build(const char *path)
{
    struct forge_config config;

    if (forge_config_parse(path, &config) < 0)
        return EXIT_FAILURE;

    int result =
            forge_alpine_prepare(config.base.version, config.base.architecture,
                                 config.rootfs.output);

    forge_config_free(&config);

    return result < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}

int main(int argc, char **argv)
{
    const char *program                  = argv[0];

    static const struct option options[] = {
            {
             .name    = "help",
             .has_arg = no_argument,
             .flag    = NULL,
             .val     = 'h',
             },
            {
             .name    = "version",
             .has_arg = no_argument,
             .flag    = NULL,
             .val     = 'V',
             },
            {
             .name    = NULL,
             .has_arg = 0,
             .flag    = NULL,
             .val     = 0,
             },
    };

    if (argc < 2) {
        usage(stderr, argv[0]);
        return EXIT_FAILURE;
    }

    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        usage(stdout, argv[0]);
        return EXIT_SUCCESS;
    }

    if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-V") == 0) {
        version();
        return EXIT_SUCCESS;
    }

    enum command command = parse_command(argv[1]);

    if (command == COMMAND_NONE) {
        fprintf(stderr, "%s: unknown command: %s\n\n", argv[0], argv[1]);

        usage(stderr, argv[0]);

        return EXIT_FAILURE;
    }

    argc   -= 1;
    argv   += 1;

    optind  = 1;

    int option;

    while ((option = getopt_long(argc, argv, "hV", options, NULL)) != -1) {
        switch (option) {
        case 'h':
            command_usage(stdout, program, command);
            return EXIT_SUCCESS;

        case 'V':
            version();
            return EXIT_SUCCESS;

        default:
            command_usage(stderr, argv[-1], command);
            return EXIT_FAILURE;
        }
    }

    if (argc - optind != 1) {
        fprintf(stderr, "error: expected exactly one configuration file\n\n");

        command_usage(stderr, argv[-1], command);

        return EXIT_FAILURE;
    }

    const char *config_path = argv[optind];

    switch (command) {
    case COMMAND_BUILD:
        return run_build(config_path);

    case COMMAND_VALIDATE:
        return run_validate(config_path);

    case COMMAND_NONE:
        break;
    }

    return EXIT_FAILURE;
}
