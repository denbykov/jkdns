#include "settings.h"
#include "core/decl.h"
#include "core/errors.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "logger/logger.h"

settings_t *current_settings = NULL;

void init_settings(settings_t *s) {
    if (s == NULL) {
        fprintf(stderr, "init_settings: settings is NULL\n");
        exit(1);
    }

    s->log_file  = NULL;
    s->log_level = NULL;

    s->port = 0;
    s->proxy_mode = false;
    s->remote_ip = NULL;
    s->remote_port = 0;
    s->remote_use_udp = false;
}

static int64_t handle_port(struct settings_s *s, const char *val) {
    if (!val) {
        fprintf(stderr, "port setting requires a value\n");
        return JK_ERROR;
    }
    s->port = strtoll(val, NULL, 10);

    return JK_OK;
}

static int64_t handle_proxy(struct settings_s *s, const char *val) {
    (void)val; // unused
    s->proxy_mode = true;

    return JK_OK;
}

static int64_t handle_remote_ip(struct settings_s *s, const char *val) {
    if (val == NULL) {
        fprintf(stderr, "remote_ip setting requires a value\n");
        return JK_ERROR;
    }
    s->remote_ip = val;

    return JK_OK;
}

static int64_t handle_remote_port(struct settings_s *s, const char *val) {
    if (!val) {
        fprintf(stderr, "remote_port setting requires a value\n");
        return JK_ERROR;
    }
    s->remote_port = strtoll(val, NULL, 10);

    return JK_OK;
}

static int64_t handle_remote_use_udp(struct settings_s *s, const char *val) {
    (void)val; // unused
    s->remote_use_udp = true;

    return JK_OK;
}

static int64_t handle_log_file(struct settings_s *s, const char *val) {
    if (!val) {
        fprintf(stderr, "log_file setting requires a value\n");
        return JK_ERROR;
    }
    s->log_file = val;

    return JK_OK;
}

static int64_t handle_log_level(struct settings_s *s, const char *val) {
    if (!val) {
        fprintf(stderr, "log_level setting requires a value\n");
        return JK_ERROR;
    }
    s->log_level = val;

    return JK_OK;
}

typedef enum {
    OPT_NONE,
    OPT_REQUIRED
} opt_arg_type;

typedef struct {
    const char *long_name;
    char short_name;
    opt_arg_type arg_type;
    int64_t (*handler)(struct settings_s *s, const char *val);
} option_t;

static option_t options[] = {
    {"log-file",  'L', OPT_REQUIRED, handle_log_file},
    {"log-level",  'l', OPT_REQUIRED, handle_log_level},
    {"port",  'p', OPT_REQUIRED, handle_port},
    {"proxy",  0 , OPT_NONE, handle_proxy},
    {"remote-ip",  0, OPT_REQUIRED, handle_remote_ip},
    {"remote-port",  0, OPT_REQUIRED, handle_remote_port},
    {"remote-use-udp",  0, OPT_NONE, handle_remote_use_udp},
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
                        return JK_ERROR;
                    }
                    if (opt->handler(settings, val) == JK_ERROR) {
                        fprintf(stderr, "parsing aborted\n");
                        return JK_ERROR;
                    }
                    matched = 1;
                    break;
                }
            }
            if (!matched) {
                fprintf(stderr, "Unknown option: --%s\n", name);
                return JK_ERROR;
            }
        } else {
            fprintf(stderr, "Bad format: %s\n", arg);
            return JK_ERROR;
        }
    }

    return JK_OK;
}

void dump_settings(FILE *f, settings_t *s) {
    // ToDo: changing bufferization mode in this manner seems to break things, look at it later

    // char buf[1024];

    // if (setvbuf(f, buf, _IOFBF, sizeof(buf)) != 0) {
    //     // warning
    //     perror("dump_settings.setvbuf");
    // }

    int max_len = 0;

    for (option_t *opt = options; opt->long_name; opt++) {
        int len = (int)strlen(opt->long_name);
        if (len > max_len) {
            max_len = len;
        }
    }

    fprintf(f, "settings:\n");
    fprintf(f, "%-*s : %s\n",  max_len, "log-file", s->log_file);
    fprintf(f, "%-*s : %s\n",  max_len, "log-level", s->log_level);
    fprintf(f, "%-*s : %s\n",  max_len, "proxy-mode", BOOL_TO_S(s->proxy_mode));
    fprintf(f, "%-*s : %s\n",  max_len, "remote-ip", s->remote_ip);
    fprintf(f, "%-*s : %u\n",  max_len, "remote-port", s->remote_port);
    fprintf(f, "%-*s : %s\n",  max_len, "remote-use-udp", BOOL_TO_S(s->remote_use_udp));
    fflush(f);

    // setvbuf(f, NULL, _IOLBF, 0);
}

int64_t validate_settings(settings_t *s) {
    if (s == NULL) {
        fprintf(stderr, "validate_settings: settings is NULL\n");
        exit(1);
    }

    if (s->port == 0) {
        fprintf(stderr, "port is not initialized\n");
        return JK_ERROR;
    }

    if (s->proxy_mode && s->remote_ip == NULL) {
        fprintf(stderr, "remote_ip is not initialized\n");
        return JK_ERROR;
    }

    if (s->proxy_mode && s->remote_port == 0) {
        fprintf(stderr, "remote_port is not initialized\n");
        return JK_ERROR;
    }

    return JK_OK;
}
