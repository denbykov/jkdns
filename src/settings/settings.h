#pragma once

#include <core/decl.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

struct settings_s {
    uint16_t port;
    bool proxy_mode;
};

void init_settings(settings_t* s);
void dump_settings(FILE *f, settings_t* s);
int64_t parse_args(int argc, char *argv[], settings_t* settings);
