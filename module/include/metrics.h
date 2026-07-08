#ifndef METRICS_H
#define METRICS_H

struct max_sleep {
    s64 sleep_time;
    int sn;
    char pn[MAX_LEN_STR];
    char euid[MAX_LEN_STR];
};

extern int avg_blocked_x1000;
extern int max_num_blocked;
extern spinlock_t metrics_lock;

DECLARE_PER_CPU(struct max_sleep, per_cpu_max);
// Sync counter
DECLARE_PER_CPU(seqcount_t, seq_number);

void update_metrics(void);

#endif