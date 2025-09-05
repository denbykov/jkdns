#pragma once

#include "decl.h"

struct buffer_s {
    uint8_t* data;
    size_t capacity;
    size_t taken;
};
