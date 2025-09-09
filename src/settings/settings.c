#include "settings.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define BOOL_TO_S(arg) ((arg) ? "true" : "false")

void init_settings(settings_t *s) {
    s->port = 0;
    s->proxy_mode = false;
}

static int64_t handle_port(struct settings_s *s, const char *val) {
    if (!val) {
        fprintf(stderr, "port setting requires a value\n");
        return -1;
    }
    s->port = strtoll(val, NULL, 10);

    return 0;
}

static int64_t handle_proxy(struct settings_s *s, const char *val) {
    (void)val; // unused
    s->proxy_mode = true;

    return 0;
}

typedef enum {
    OPT_NONE,
    OPT_REQUIRED,
    OPT_OPTIONAL
} opt_arg_type;

typedef struct {
    const char *long_name;
    char short_name;
    opt_arg_type arg_type;
    int64_t (*handler)(struct settings_s *s, const char *val);
} option_t;

static option_t options[] = {
    {"port",  'p', OPT_REQUIRED, handle_port},
    {"proxy",  0 , OPT_NONE, handle_proxy},
    {0, 0, OPT_NONE, 0} // terminator
};

int64_t parse_args(int argc, char *argv[], settings_t* settings) {
    if (settings == NULL) {
        fprintf(stderr, "parse_args: settings is NULL\n");
        exit(1);
    }

    for (int i = 1; i < argc; i++) {
        char *arg = argv[i];

        if (strncmp(arg, "--", 2) == 0) {
            char *eq = strchr(arg, '=');
            const char *name = arg + 2;
            const char *val = NULL;

            if (eq) {
                *eq = '\0';
                val = eq + 1;
            } else {
                if (i + 1 < argc && argv[i+1][0] != '-') {
                    val = argv[++i];
                }
            }

            int matched = 0;
            for (option_t *opt = options; opt->long_name; opt++) {
                if (strcmp(name, opt->long_name) == 0) {
                    if (opt->arg_type == OPT_REQUIRED && !val) {
                        fprintf(stderr, "--%s requires a value\n", opt->long_name);
                        return -1;
                    }
                    if (opt->handler(settings, val) == -1) {
                        fprintf(stderr, "parsing aborted\n");
                        return -1;
                    }
                    matched = 1;
                    break;
                }
            }
            if (!matched) {
                fprintf(stderr, "Unknown option: --%s\n", name);
                return -1;
            }
        } else {
            fprintf(stderr, "Bad format: %s\n", arg);
            return -1;
        }
    }

    return 0;
}

void dump_settings(FILE *f, settings_t *s) {
    char buf[1024];

    if (setvbuf(f, buf, _IOFBF, sizeof(buf)) != 0) {
        // warning
        perror("dump_settings.setvbuf");
    }

    int max_len = (int)strlen("proxy_mode");

    fprintf(f, "settings:\n");
    fprintf(f, "%-*s : %u\n", max_len, "port", s->port);
    fprintf(f, "%-*s : %s\n",  max_len, "proxy_mode", BOOL_TO_S(s->proxy_mode));
    fflush(f);

    setvbuf(f, NULL, _IOLBF, 0);
}
