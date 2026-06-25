#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h> 

#include "common.h"
#include "driver_commands.h"
#include "lifecycle.h"
#include "monitoring_data.h" 
#include "throttling.h"

static dev_t dev_numbers;
static struct cdev chr_dev;
static struct class *dev_class;


static int open_dev(struct inode *inode, struct file *f) {
    printk(KERN_INFO "[%s]: Device '%s' (MAJOR: %d, MINOR: %d) opened\n", MODNAME, DEV_NAME, MAJOR(dev_numbers), MINOR(dev_numbers));
    return 0;
}

static int release_dev(struct inode *inode, struct file *f) {
    printk(KERN_INFO "[%s]: Device '%s' (MAJOR: %d, MINOR: %d) released\n", MODNAME, DEV_NAME, MAJOR(dev_numbers), MINOR(dev_numbers));
    return 0;
}

static long ioctl_op(struct file *file, unsigned int cmd, unsigned long arg) {
    // Only update op
    // Check euid
    if (!uid_eq(current_euid(), GLOBAL_ROOT_UID)) {
        printk(KERN_ERR "[%s]: eUID operation error\n", MODNAME);
        return -EPERM;
    }

    // Check mode
    if (!(file->f_mode & FMODE_WRITE)) {
        printk(KERN_ERR "[%s]: Mode error\n", MODNAME);
        return -EBADF;
    }

    int ret;
    int uval;
    char ubuff[MAX_LEN_STR];

    // Check op
    switch (cmd) {
        case IOCTL_ENABLE_THROTTLING:
            atomic_set(&enabled, 1);
            printk(KERN_INFO "[%s]: Throttling enabled\n", MODNAME);
            break;

        case IOCTL_DISABLE_THROTTLING:
            atomic_set(&enabled, 0);
            printk(KERN_INFO "[%s]: Throttling disabled\n", MODNAME);
            break;

        case IOCTL_REGISTER_SN:
            // Read input
            if (copy_from_user(&uval, (int __user *)arg, sizeof(uval))) {
                printk(KERN_ERR "[%s]: Input reading error\n", MODNAME);
                return -EFAULT;
            }

            ret = add_critical_sn(uval);
            // Check error
            if (ret)
                return ret;
            break;

        case IOCTL_UNREGISTER_SN:
            // Read input
            if (copy_from_user(&uval, (int __user *)arg, sizeof(uval))) {
                printk(KERN_ERR "[%s]: Input reading error\n", MODNAME);
                return -EFAULT;
            }

            ret = remove_critical_sn(uval);
            // Check error
            if (ret)
                return ret;
            break;

        case IOCTL_REGISTER_EUID:
            // Read input with MAXLEN
            if (copy_from_user(ubuff, (char __user *)arg, MAX_LEN_STR)) {
                printk(KERN_ERR "[%s]: Input reading error\n", MODNAME);
                return -EFAULT;
            }

            ret = add_critical_euid(ubuff);
            // Check error
            if (ret)
                return ret;
            break;

        case IOCTL_UNREGISTER_EUID:
            // Read input with MAXLEN
            if (copy_from_user(ubuff, (char __user *)arg, MAX_LEN_STR)) {
                printk(KERN_ERR "[%s]: Input reading error\n", MODNAME);
                return -EFAULT;
            }

            ret = remove_critical_euid(ubuff);
            // Check error
            if (ret)
                return ret;
            break;
        
        case IOCTL_REGISTER_PN:
            // Read input with MAXLEN
            if (copy_from_user(ubuff, (char __user *)arg, MAX_LEN_STR)) {
                printk(KERN_ERR "[%s]: Input reading error\n", MODNAME);
                return -EFAULT;
            }

            ret = add_critical_pn(ubuff);
            // Check error
            if (ret)
                return ret;
            break;

        case IOCTL_UNREGISTER_PN:
            // Read input with MAXLEN
            if (copy_from_user(ubuff, (char __user *)arg, MAX_LEN_STR)) {
                printk(KERN_ERR "[%s]: Input reading error\n", MODNAME);
                return -EFAULT;
            }

            ret = remove_critical_pn(ubuff);
            // Check error
            if (ret)
                return ret;
            break;
        case IOCTL_SET_MAX_RATE:
            // Read input
            if (copy_from_user(&uval, (int __user *)arg, sizeof(uval))) {
                printk(KERN_ERR "[%s]: Input reading error\n", MODNAME);
                return -EFAULT;
            }

            atomic_set(&max_rate, uval);
            printk(KERN_INFO "[%s]: Max rate updated\n", MODNAME);
            break;
            
        default:
            return -EINVAL;
    }

    printk(KERN_INFO "[%s]: IOCTL operation completed\n", MODNAME);
    return 0;
}

static const struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = open_dev,
    .release = release_dev,
    .unlocked_ioctl = ioctl_op,
};


int setup_driver(void) {
    int ret;
    struct device *dev;

    // Reserve a Major and Minor=0
    ret = alloc_chrdev_region(&dev_numbers, 0, 1, DEV_NAME);
    if (ret) {
        printk(KERN_ERR "[%s]: Error reserving Major and Minor\n", MODNAME);
        return ret;
    }

    // Bind fops to the device
    cdev_init(&chr_dev, &fops);

    // Register the device
    cdev_add(&chr_dev, dev_numbers, 1);

    // Generate class
    dev_class = class_create(DEV_NAME);
    if (IS_ERR(dev_class)) {
        printk(KERN_ERR "[%s]: Error generating class\n", MODNAME);
        cdev_del(&chr_dev);
        unregister_chrdev_region(dev_numbers, 1);
        return PTR_ERR(&chr_dev);
    }

    // Create device
    dev = device_create(dev_class, NULL, dev_numbers, NULL, DEV_NAME);
    if (IS_ERR(dev)) {
        printk(KERN_ERR "[%s]: Error generating device\n", MODNAME);
        class_destroy(dev_class);
        cdev_del(&chr_dev);
        unregister_chrdev_region(dev_numbers, 1);
        return PTR_ERR(&dev);
    }

    return 0;
}

void cleanup_driver(void) {
    device_destroy(dev_class, dev_numbers);
    class_destroy(dev_class);
    cdev_del(&chr_dev);
    unregister_chrdev_region(dev_numbers, 1);
}