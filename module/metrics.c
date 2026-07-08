#include <linux/module.h>
#include <linux/percpu.h>
#include <linux/smp.h>
#include <linux/spinlock.h>

#include "common.h"
#include "lifecycle.h"
#include "metrics.h"
#include "throttling.h"

// Can't use double
int avg_blocked_x1000 = 0;
int max_num_blocked = 0;
spinlock_t metrics_lock;

static long epochs = 0;
static int blocked_sum = 0;

DEFINE_PER_CPU(struct max_sleep, per_cpu_max);
DEFINE_PER_CPU(seqcount_t, seq_number); 

void update_metrics(void) {
    // Update epoch
    epochs += 1;

    /*The presence_counter corresponds to the number of blocked 
    threads within an epoch. These threads can only be woken up 
    at the end of the epoch (or earlier if throttling is disabled). 
    It also accounts for threads that remain sleeping when the limit 
    is reset at the end of the epoch, in cases where the number of 
    already blocked threads exceeds the limit and not all of them can 
    be woken up.*/
    int actual_blocked = atomic_read(&presence_counter);

    // Update sum
    blocked_sum += actual_blocked;

    spin_lock(&metrics_lock);
    // Update max number of blocked
    if (actual_blocked > max_num_blocked) {
        max_num_blocked = actual_blocked;
    }

    // Calculate avgx1000
    avg_blocked_x1000 = blocked_sum *1000 / epochs;
    spin_unlock(&metrics_lock);
}

static void setup_per_cpu(void *param) {
    // Setup starting local max
    struct max_sleep setup_sleep;
    setup_sleep.sleep_time = 0;
    setup_sleep.sn = 0;
    strcpy(setup_sleep.pn, "");
    strcpy(setup_sleep.euid, "");

    struct max_sleep *per_cpu_ptr = this_cpu_ptr(&per_cpu_max);
    *per_cpu_ptr = setup_sleep;

    // Setup sync sequence number
    seqcount_t *seq_number_ptr = this_cpu_ptr(&seq_number);
    seqcount_init(seq_number_ptr);
}

void setup_metrics(void) {
    spin_lock_init(&metrics_lock);

    // Setup on each cpu
    on_each_cpu(setup_per_cpu, NULL, 1);    
}
