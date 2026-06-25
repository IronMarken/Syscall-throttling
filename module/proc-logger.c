#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/rhashtable.h>
#include <linux/seq_file.h>

#include "common.h"
#include "lifecycle.h"
#include "monitoring_data.h"
#include "throttling.h"

static struct proc_dir_entry *root_dir;

// Show for status
static int status_show(struct seq_file *sf, void *v) {
    int en = atomic_read(&enabled);
    int mr = atomic_read(&max_rate);
    seq_printf(sf, "(-) Throttler status:\n\t(*) Enabled: %d\n\t(*) Max Rate: %d\n", en, mr);
    return 0;
}

static int status_open(struct inode *inode, struct file *file) {
    return single_open(file, status_show, NULL);
}

static const struct proc_ops status_ops = {
    .proc_open = status_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};


// Show for registered euid
static int euid_show(struct seq_file *sf, void *v) {
    struct str_node *entry;
    struct rhashtable_iter iterator;

    seq_printf(sf, "(-) Registered EUIDs:\n");

    // Doesnt need explicit rcu read lock
    rhashtable_walk_enter(&euid_data.hashtable, &iterator);
    rhashtable_walk_start(&iterator);

    while((entry = rhashtable_walk_next(&iterator))) {
        // Ignore entry error
        if(IS_ERR(entry))
            continue;

        seq_printf(sf, "\t(*) %s\n", entry->str_key);
    }

    rhashtable_walk_stop(&iterator);
    rhashtable_walk_exit(&iterator);

    return 0;
}

static int euid_open(struct inode *inode, struct file *file) {
    return single_open(file, euid_show, NULL);
}

static const struct proc_ops euid_ops = {
    .proc_open = euid_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};


// Show for registered program names
static int comm_show(struct seq_file *sf, void *v) {
    struct str_node *entry;
    struct rhashtable_iter iterator;

    seq_printf(sf, "(-) Registered program names:\n");

    // Doesnt need explicit rcu read lock
    rhashtable_walk_enter(&comm_data.hashtable, &iterator);
    rhashtable_walk_start(&iterator);

    while((entry = rhashtable_walk_next(&iterator))) {
        // Ignore entry error
        if(IS_ERR(entry))
            continue;

        seq_printf(sf, "\t(*) %s\n", entry->str_key);
    }

    rhashtable_walk_stop(&iterator);
    rhashtable_walk_exit(&iterator);

    return 0;
}

static int comm_open(struct inode *inode, struct file *file) {
    return single_open(file, comm_show, NULL);
}

static const struct proc_ops comm_ops = {
    .proc_open = comm_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

// Show for registered syscall numbers
static int sn_show(struct seq_file *sf, void *v) {
    struct int_node *entry;
    struct rhashtable_iter iterator;

    seq_printf(sf, "(-) Registered syscall numbers:\n");

    // Doesnt need explicit rcu read lock
    rhashtable_walk_enter(&sn_data.hashtable, &iterator);
    rhashtable_walk_start(&iterator);

    while((entry = rhashtable_walk_next(&iterator))) {
        // Ignore entry error
        if(IS_ERR(entry))
            continue;

        seq_printf(sf, "\t(*) %d\n", entry->int_key);
    }

    rhashtable_walk_stop(&iterator);
    rhashtable_walk_exit(&iterator);

    return 0;
}

static int sn_open(struct inode *inode, struct file *file) {
    return single_open(file, sn_show, NULL);
}

static const struct proc_ops sn_ops = {
    .proc_open = sn_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
}; 


int setup_logger(void) {
    // Create root dir
    root_dir = proc_mkdir(MODNAME, NULL);
    if (!root_dir) {
        printk(KERN_ERR "[%s]: Error generating proc root dir\n", MODNAME);
        return -ENOMEM;
    }

    // Create status file
    if (!proc_create("status", 0444, root_dir, &status_ops)) {
        printk(KERN_ERR "[%s]: Error generating status file\n", MODNAME);
        goto err;
    }

    // Create registered data subdir
    struct proc_dir_entry *registered_data_dir;
    registered_data_dir = proc_mkdir("registered-data", root_dir);
    if (!registered_data_dir) {
        printk(KERN_ERR "[%s]: Error generating registered-data subdir\n", MODNAME);
        goto err;
    }

    // Create file for euid
    if (!proc_create("euid", 0444, registered_data_dir, &euid_ops)) {
        printk(KERN_ERR "[%s]: Error generating euid file\n", MODNAME);
        goto err;
    }

    // Create file for pn
    if (!proc_create("pn", 0444, registered_data_dir, &comm_ops)) {
        printk(KERN_ERR "[%s]: Error generating program name file\n", MODNAME);
        goto err;
    }

    // Create file for sn
    if (!proc_create("sn", 0444, registered_data_dir, &sn_ops)) {
        printk(KERN_ERR "[%s]: Error generating syscall number file\n", MODNAME);
        goto err;
    }

    return 0;

err:
    remove_proc_subtree(MODNAME, NULL);
    return -ENOMEM;
}

void cleanup_logger(void) {
    remove_proc_subtree(MODNAME, NULL);
}