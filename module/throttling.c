#include "sys-throttling.h"

atomic_t enabled = ATOMIC_INIT(0);

void enable_throttling(void) {
    atomic_set(&enabled, 1);
    printk(KERN_INFO "[%s]: Throttling enabled\n", MODNAME);
}

void disable_throttling(void) {
    atomic_set(&enabled, 0);
    printk(KERN_INFO "[%s]: Throttling disabled\n", MODNAME);
}

int is_throttling_enabled(void) {
    return atomic_read(&enabled);
}