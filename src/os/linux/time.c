#include "core/time.h"

#include <time.h>
#include <stdint.h>

int64_t jk_now() {
    struct timespec ts;
    // Use CLOCK_REALTIME for wall-clock, CLOCK_MONOTONIC for monotonic
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return -1; // error
    }
    return (int64_t)ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL;
}
