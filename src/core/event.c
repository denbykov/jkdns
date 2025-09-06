#include "event.h"
#include <stdbool.h>

void init_event(event_t *ev) {
    ev->owner.tag = EV_OWNER_NONE;
    ev->owner.ptr = NULL;
    ev->write = false;
    ev->enabled = false;
    ev->handler = NULL;
}
