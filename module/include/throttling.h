#ifndef THROTTLING_H
#define THROTTLING_H

#define PERIOD_MS 1000

extern atomic_t enabled;
extern atomic_t max_rate;

#endif