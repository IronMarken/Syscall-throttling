#include <linux/module.h>

#include "common.h"
#include "lifecycle.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Marco Ferri");
MODULE_DESCRIPTION("LKM for syscall throttling");


static int __init lkm_init(void) {
    int ret;

    // Setup monitoring data
    ret = setup_monitoring_data();
    if (ret == 0) {
        printk(KERN_INFO "[%s]: Monitoring data setup completed\n", MODNAME);
    }
    
    // Setup driver
    ret = setup_driver();
    if (ret == 0) {
        printk(KERN_INFO "[%s]: Driver setup completed\n", MODNAME);
    }

    printk(KERN_INFO "[%s]: Module setup completed\n", MODNAME);

    return 0;
}


static void __exit lkm_exit(void) {
    // Cleanup monitoring data
    cleanup_monitoring_data();
    printk(KERN_INFO "[%s]: Monitoring data cleanup completed\n", MODNAME);
    
    // Cleanup driver
    cleanup_driver();
    printk(KERN_INFO "[%s]: Driver cleanup completed\n", MODNAME);

    printk(KERN_INFO "[%s]: Module cleanup completed\n", MODNAME); 
}


module_init(lkm_init);
module_exit(lkm_exit);