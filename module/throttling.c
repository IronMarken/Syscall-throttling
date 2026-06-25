#include <linux/module.h>

#include "common.h"
#include "throttling.h"

atomic_t enabled = ATOMIC_INIT(0);