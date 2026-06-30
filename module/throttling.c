#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/kprobes.h>
#include <linux/module.h>
#include <linux/timer.h>
#include <linux/smp.h>

#include "common.h"
#include "lifecycle.h"
#include "monitoring_data.h"
#include "throttling.h"

// Lookup probe trigger
#define setup_target_func "dummy_trigger"
#define throttling_target_func "x64_sys_call"

// Blocking probe
DEFINE_PER_CPU(unsigned long, brute_search);
DEFINE_PER_CPU(unsigned long *, kprobe_ctx_pointer);

static struct kprobe setup_kprobe;
static struct kprobe throttle_kprobe;
static atomic_t lookup_counter = ATOMIC_INIT(0);
static atomic_t presence_counter = ATOMIC_INIT(0);

// Sleep-Awake data
static struct wait_queue_head sleeping_queue;
static struct timer_list time_window;
static atomic_t limit_counter; 

atomic_t enabled = ATOMIC_INIT(0);
atomic_t max_rate = ATOMIC_INIT(DEFAULT_MAX_RATE);

static unsigned long *reference_offset = 0x0;


static int __kprobes lookup(struct kprobe *p, struct pt_regs *regs) {
    unsigned long *temp = this_cpu_ptr(&brute_search);

    while ((unsigned long) temp>0) {
        temp -=1;

        if (*temp == (unsigned long) p) {
            atomic_inc(&lookup_counter);
            printk(KERN_INFO "[%s]: FOUND - Target found on CPU %d. Offset: %p \n", MODNAME, smp_processor_id(), temp);
            reference_offset = temp;
            break;
        }
        if ((unsigned long) temp<=0) return 1;
    }

    this_cpu_write(kprobe_ctx_pointer, temp);
    
    return 0;
}

static void dummy_trigger(void *param) {
    printk(KERN_INFO "[%s]: SETUP - Lookup trigger on CPU %d\n", MODNAME, smp_processor_id());
    return;
}


// Throttling kprobe
static int __kprobes throttle(struct kprobe *p, struct pt_regs *regs) {
    // Check if enabled
    if (!atomic_read(&enabled)) {
        return 0;
    }

    // Check if critical
    char comm[MAX_LEN_STR];
    char euid_s[MAX_LEN_STR];
    int syscall_nr = regs->si;
    kuid_t euid = current_euid();
    
    strcpy(comm, current->comm);
    snprintf(euid_s, MAX_LEN_STR, "%u", __kuid_val(euid));

    if (is_critical(syscall_nr, comm, euid_s)) {
        
        // Check if max rate is exceeded in atomic way
        if (atomic_dec_return(&limit_counter) < 0) {
        
            printk(KERN_INFO "[%s]: BLOCKED - id=%d euid=%s comm=%s exceeded the max rate\n", MODNAME, syscall_nr, euid_s, comm);      
        
            unsigned long *kprobe_ctx_ptr = this_cpu_read(kprobe_ctx_pointer);

            // Presence counter for module cleanup
            atomic_inc(&presence_counter);
        
            // Blocking probe ctx
            *kprobe_ctx_ptr = (unsigned long)NULL;
            preempt_enable();

            // Wake up condition: throttling disabled or max rate not exceeded 
            wait_event_interruptible(sleeping_queue, !atomic_read(&enabled) || atomic_dec_return(&limit_counter) >= 0);

            // Restore probe ctx
            preempt_disable();
            kprobe_ctx_ptr = this_cpu_read(kprobe_ctx_pointer);
            *kprobe_ctx_ptr = (unsigned long) p;

            atomic_dec(&presence_counter);
        }
    }
    return 0;
}

static void window_callback(struct timer_list *t) {
    // Reset limit
    atomic_set(&limit_counter, atomic_read(&max_rate));

    // Wake up all
    wake_up_interruptible(&sleeping_queue);

    // Restart timer
    mod_timer(&time_window, jiffies + msecs_to_jiffies(PERIOD_MS));
}

int setup_throttling(void) {
    int ret;

    // Setup wait queue
    init_waitqueue_head(&sleeping_queue);

    // Setup limit
    atomic_set(&limit_counter, atomic_read(&max_rate));

    // Setup kprobe init
    setup_kprobe.symbol_name = setup_target_func;
    setup_kprobe.pre_handler = lookup;
    
    ret = register_kprobe(&setup_kprobe);
    if (ret < 0) {
        printk(KERN_ERR "[%s]: Error during setup kprobe register\n", MODNAME);
        return ret;
    }

    // Preemption disabled
    get_cpu();

    on_each_cpu(dummy_trigger, NULL, 1);

    if (atomic_read(&lookup_counter)!= (num_online_cpus())) {
        printk(KERN_ERR "[%s]: Lookup error\n",MODNAME);
        put_cpu();
        unregister_kprobe(&setup_kprobe);
        return -1;
    }

    put_cpu();


    // Setup kprobe throttle
    throttle_kprobe.symbol_name = throttling_target_func;
    throttle_kprobe.pre_handler = throttle;
    
    ret = register_kprobe(&throttle_kprobe);
    if (ret < 0) {
        printk(KERN_ERR "[%s]: Error during throttle kprobe register\n", MODNAME);
        return ret;
    }

    // Setup timer
    timer_setup(&time_window, window_callback, 0);
    
    // Absolute expiration time (jiffies + period)
    mod_timer(&time_window, jiffies + msecs_to_jiffies(PERIOD_MS));

    return 0;
}


void cleanup_throttling(void) {
    // Disable throttling
    atomic_set(&enabled, 0);
    
    // Disable and remove timer
    timer_delete_sync(&time_window);

    // Wake up condition met with enabled=0 but no more max rate 
    // All blocked probes execute
    wake_up_interruptible(&sleeping_queue);

    // Wait all sleeping threads
    while (atomic_read(&presence_counter) != 0) {
        // Sleep 50 ms
        msleep(50);
    } 

    // Remove kprobes
    unregister_kprobe(&setup_kprobe);
    unregister_kprobe(&throttle_kprobe);
}