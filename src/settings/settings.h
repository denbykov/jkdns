#pragma once

#include "core/decl.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

struct settings_s {
    const char* log_file;
    const char* log_level;

    uint16_t port;
    
    bool        proxy_mode;
    const char* remote_ip;
    uint16_t    remote_port;
    bool        remote_use_udp;
};

extern settings_t *current_settings;

void init_settings(settings_t* s);
int64_t validate_settings(settings_t* s);
void dump_settings(FILE *f, settings_t* s);
int64_t parse_args(int argc, char *argv[], settings_t* settings);
