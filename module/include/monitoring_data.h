#ifndef MONITORING_DATA_H
#define MONITORING_DATA_H

// Struct for proc logger
extern struct hash_struct sn_data;
extern struct hash_struct euid_data;
extern struct hash_struct comm_data;

struct int_node {
    int int_key;
    struct rhash_head node;
    struct rcu_head rcu;
};

struct str_node {
    char str_key[MAX_LEN_STR];
    struct rhash_head node;
    struct rcu_head rcu;
};

struct hash_struct {
    struct rhashtable hashtable;
    spinlock_t lock;
};


// Data op
int add_critical_sn(int);
int remove_critical_sn(int);
int add_critical_euid(char*);
int remove_critical_euid(char*);
int add_critical_pn(char*);
int remove_critical_pn(char*);
int is_critical(int, char*, char*);

#endif