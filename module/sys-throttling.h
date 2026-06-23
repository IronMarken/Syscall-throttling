#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h> 
#include <linux/jhash.h>
#include <linux/module.h>
#include <linux/rcupdate.h>
#include <linux/rhashtable.h>
#include <linux/spinlock.h>

#include "driver-commands.h"

#define MAX_LEN_STR 64

#define MODNAME "SYS-THROTTLING"

// Monitoring data
int setup_monitoring_data(void);
void cleanup_monitoring_data(void);
int add_critical_sn(int);
int remove_critical_sn(int);
int add_critical_euid(char*);
int remove_critical_euid(char*);
int add_critical_pn(char*);
int remove_critical_pn(char*);
int is_critical(int, char*, char*);

// Driver
int setup_driver(void);
void cleanup_driver(void);

// Throttling
void enable_throttling(void);
void disable_throttling(void);
int is_throttling_enabled(void);