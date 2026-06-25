#include <linux/module.h>

#include "common.h"
#include "throttling.h"

atomic_t enabled = ATOMIC_INIT(0);
atomic_t max_rate = ATOMIC_INIT(DEFAULT_MAX_RATE);