#include <linux/module.h>
#include <linux/jhash.h>
#include <linux/rcupdate.h>
#include <linux/rhashtable.h>
#include <linux/spinlock.h>

#include "common.h"
#include "lifecycle.h" 
#include "monitoring_data.h"

// Syscall hash struct
struct hash_struct sn_data;
// EUID hash struct
struct hash_struct euid_data;
// Program name hash struct
struct hash_struct comm_data;

static struct rhashtable_params int_params = {
    .head_offset = offsetof(struct int_node, node),
    .key_offset = offsetof(struct int_node, int_key),
    .key_len = sizeof(int),
    .automatic_shrinking = true,
};

// Hash function for string
static u32 str_hashfn(const void *data, u32 len, u32 seed) {
    const char *str_data = data;
    return jhash(str_data, strlen(str_data), seed);
}

static int str_cmpfn(struct rhashtable_compare_arg *arg, const void *obj) {
    return strcmp((char *)arg->key, ((struct str_node *) obj)->str_key);
}

static struct rhashtable_params str_params = {
    .head_offset = offsetof(struct str_node, node),
    .key_offset = offsetof(struct str_node, str_key),
    .key_len = MAX_LEN_STR,
    .hashfn = str_hashfn,
    .obj_cmpfn = str_cmpfn,
    .automatic_shrinking = true,
};


int setup_monitoring_data(void) {
    int ret;

    // Setup syscall hash struct
    spin_lock_init(&sn_data.lock);
    ret = rhashtable_init(&sn_data.hashtable, &int_params);

    if (ret < 0) {
        printk(KERN_ERR "[%s]: Error in syscall hashtable init\n", MODNAME);
        return ret; 
    }

    // Setup EUID hash struct
    spin_lock_init(&euid_data.lock);
    ret = rhashtable_init(&euid_data.hashtable, &str_params);

    if (ret < 0) {
        printk(KERN_ERR "[%s]: Error in EUID hashtable init\n", MODNAME);
        return ret; 
    }

    // Setup program name hash struct
    spin_lock_init(&comm_data.lock);
    ret = rhashtable_init(&comm_data.hashtable, &str_params);

    if (ret < 0) {
        printk(KERN_ERR "[%s]: Error in program name hashtable init\n", MODNAME);
        return ret; 
    }

    return 0;
}

// Free callback for free and destroy
static void free_cb(void *obj, void *arg) {
    kfree(obj);
}

void cleanup_monitoring_data(void) {
    // Cleanup syscall hash struct
    rhashtable_free_and_destroy(&sn_data.hashtable, free_cb, NULL);

    // Cleanup EUID hash struct
    rhashtable_free_and_destroy(&euid_data.hashtable, free_cb, NULL);

    // Cleanup program name struct
    rhashtable_free_and_destroy(&comm_data.hashtable, free_cb, NULL);
}

static void free_int_rcu_cb(struct rcu_head *rcu) {
    struct int_node *obj = container_of(rcu, struct int_node, rcu);
    kfree(obj);
}

static void free_str_rcu_cb(struct rcu_head *rcu) {
    struct str_node *obj = container_of(rcu, struct str_node, rcu);
    kfree(obj);
}

// Add critical syscall number
int add_critical_sn(int sc_number) {
    int ret;

    // Writer lock
    spin_lock(&sn_data.lock);

    // Check if data already exists
    struct int_node *entry;

    entry = rhashtable_lookup_fast(&sn_data.hashtable, &sc_number, int_params);

    // Data already present in hashtable
    if (entry) {
        printk(KERN_ERR "[%s]: Data already exists\n", MODNAME);
        spin_unlock(&sn_data.lock);
        return 1;
    }

    // Generate node and add
    entry = kmalloc(sizeof(struct int_node), GFP_KERNEL | __GFP_ZERO);
    if (!entry) {
        printk(KERN_ERR "[%s]: Error allocating memory\n", MODNAME);
        spin_unlock(&sn_data.lock);
        return -ENOMEM;
    }

    entry->int_key = sc_number;
    ret = rhashtable_insert_fast(&sn_data.hashtable, &entry->node, int_params);

    if (ret) {
        printk(KERN_ERR "[%s]: Insert error\n", MODNAME);
        kfree(entry);
        spin_unlock(&sn_data.lock);
        return ret;
    }

    // Insert completed
    spin_unlock(&sn_data.lock);
    return 0;
}

// Remove critical syscall number
int remove_critical_sn(int sc_number) {
    int ret;

    // Writer lock
    spin_lock(&sn_data.lock);

    // Check if sn already exists
    struct int_node *entry;

    entry = rhashtable_lookup_fast(&sn_data.hashtable, &sc_number, int_params);

    // Data to remove is not in hashtable
    if (!entry) {
        printk(KERN_ERR "[%s]: Data to remove doesn't exists\n", MODNAME);
        spin_unlock(&sn_data.lock);
        return 1;
    }

    // Remove data
    ret = rhashtable_remove_fast(&sn_data.hashtable, &entry->node, int_params);
    if (ret) {
        printk(KERN_ERR "[%s]: Remove error\n", MODNAME);
        spin_unlock(&sn_data.lock);
        return ret;
    }

    spin_unlock(&sn_data.lock);

    // RCU callback free
    call_rcu(&entry->rcu, free_int_rcu_cb);
    return 0;
}

// Add critical string
static int add_critical_str(struct hash_struct table, char *value) {
    int ret;

    // Writer lock
    spin_lock(&table.lock);

    // Check if data already exists
    struct str_node *entry;

    entry = rhashtable_lookup_fast(&table.hashtable, value, str_params);

    // Data already present in hashtable
    if (entry) {
        printk(KERN_ERR "[%s]: Data already exists\n", MODNAME);
        spin_unlock(&table.lock);
        return 1;
    }

    // Generate node and add
    entry = kmalloc(sizeof(struct str_node), GFP_KERNEL | __GFP_ZERO);
    if (!entry) {
        printk(KERN_ERR "[%s]: Error allocating memory\n", MODNAME);
        spin_unlock(&table.lock);
        return -ENOMEM;
    }

    strscpy(entry->str_key, value, MAX_LEN_STR);
    ret = rhashtable_insert_fast(&table.hashtable, &entry->node, str_params);

    if (ret) {
        printk(KERN_ERR "[%s]: Insert error\n", MODNAME);
        kfree(entry);
        spin_unlock(&table.lock);
        return ret;
    }

    // Insert completed
    spin_unlock(&table.lock);
    return 0;
}

// Add critical EUID
int add_critical_euid(char *euid) {
    return add_critical_str(euid_data, euid);
}

// Add critical program name
int add_critical_pn(char *comm) {
    return add_critical_str(comm_data, comm);
}

// Remove critical string
static int remove_critical_string(struct hash_struct table, char *value) {
    int ret;

    // Writer lock
    spin_lock(&table.lock);

    // Check if sn already exists
    struct str_node *entry;

    entry = rhashtable_lookup_fast(&table.hashtable, value, str_params);

    // Data to remove is not in hashtable
    if (!entry) {
        printk(KERN_ERR "[%s]: Data to remove doesn't exists\n", MODNAME);
        spin_unlock(&table.lock);
        return 1;
    }

    // Remove data
    ret = rhashtable_remove_fast(&table.hashtable, &entry->node, str_params);
    if (ret) {
        printk(KERN_ERR "[%s]: Remove error\n", MODNAME);
        spin_unlock(&table.lock);
        return ret;
    }

    spin_unlock(&table.lock);

    // RCU callback free
    call_rcu(&entry->rcu, free_str_rcu_cb);
    return 0;
}

// Remove critical EUID
int remove_critical_euid(char *euid) {
    return remove_critical_string(euid_data, euid);
}

// Remove critical program name
int remove_critical_pn(char *comm) {
    return remove_critical_string(comm_data, comm);
}

// Check if invocation is critical
int is_critical(int sc_number, char *comm, char *euid) {
    struct int_node *ret_sn;
    struct str_node *ret_comm, *ret_euid;
    int res_sn, res_comm, res_euid;

    rcu_read_lock();
    ret_sn = rhashtable_lookup(&sn_data.hashtable, &sc_number, int_params);
    ret_comm = rhashtable_lookup(&comm_data.hashtable, comm, str_params);
    ret_euid = rhashtable_lookup(&euid_data.hashtable, euid, str_params);
    rcu_read_unlock();

    res_sn = (ret_sn) ? 1:0;
    res_comm = (ret_comm) ? 1:0;
    res_euid = (ret_euid) ? 1:0;

    return (ret_sn && (ret_comm || ret_euid));
}