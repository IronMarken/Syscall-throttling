#ifndef LIFECYCLE_H
#define LIFECYCLE_H

// Monitoring data
int setup_monitoring_data(void);
void cleanup_monitoring_data(void);

// Driver
int setup_driver(void);
void cleanup_driver(void);

// Proc logger
int setup_logger(void);
void cleanup_logger(void);

// Probing
int setup_throttling(void);
void cleanup_throttling(void);

#endif