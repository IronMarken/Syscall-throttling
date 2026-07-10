# Syscall-throttling
## 1. Description
Linux Kernel Module (LKM) that implements a system call throttling mechanism. Registered system calls are subject to a rate limit of MAX invocations per one-second time window when invoked by a registered effective user-ID and/or program-name. If the number of invocations exceeds MAX during a given window, the excess invocations are deferred to subsequent windows, ensuring that the rate limit is always enforced. The invocation counter is reset at the beginning of each new one-second window.<br><br>
A user with an effective user ID (EUID) of 0 can configure the module through a dedicated character device exposed as ```/dev/throttling```. Specifically, the user can enable or disable the throttling mechanism, register or deregister the system call numbers to be monitored, register or deregister effective user IDs (EUIDs) and program names and configure the MAX invocation limit enforced for each one-second time window.<br><br>
The module exposes runtime information through the proc virtual file system, specifically under ```/proc/SYS-THROTTLING```. This interface provides the module's current status, the registered configuration (system call numbers, effective user IDs (EUIDs) and program names), as well as statistics related to threads whose  system call invocations have been deferred by the throttling mechanism.<br><br>
A [user-space controller](user/user-controller/controller.c) is also provided to configure the module through a simple and transparent interface, together with a [test application](user/test/test.c) used to validate the module's behavior.

## 2. Usage
### 2.1. [Module](module)
```bash
# Compile the module
sudo make compile

# Load the module
sudo make mount

# Unload the module
sudo make umount

# Clean compile files
sudo make clean
```
**WARNING:** Upon module loading, the throttling mechanism is disabled by default. The user must explicity enable it before it becomes active.

---

### 2.2. [Controller](user/user-controller/controller.c)
```bash
# Compile the controller
gcc -o controller ./controller.c
```
**Controller options:**
```text
./controller -h
Usage:
    -h      Show this help message
    -e      Enable throttling
    -d      Disable throttling
    -s NUM  Register syscall number NUM
    -z NUM  Unregister syscall number NUM
    -i EUID Register euid EUID
    -l EUID Unregister euid EUID
    -p PN   Register program name PN
    -b PN   Unregister program name PN
    -r LIM  Set max rate LIM

Options e, d and r can be used once each
Options e and d can't be used together
```
**INFO:** The ```-s```, ```-z```, ```-i```, ```-l```, ```-p``` and ```-b``` options can be specified multiple times within the same command, allowing multiple system call numbers, EUIDs and program names to be registered or deregistered without requiring multiple invocations of the controller application. 

---

### 2.3. [Test](user/test/test.c)

```bash
# Compile the test
gcc -o test ./test.c

# Run test
sudo ./test
```

## 3. Module implementation
### 3.1. [Registered data](module/monitoring-data.c)
One of the key design aspects of the module is the choice of the data structures used to store the registered system call numbers, EUIDs and program names. An analysis of the module's usage patterns shows that insertion and removals are relatively infrequent while the dominant operation is performing point lookups to determine if a specific element is registered. Traversing all registered entries is also required, but it occours much less frequently. The data structure that best matches these requirements is a hash table, as it provides efficient lookup operations while maintaining acceptable insertion and removal performance. For this reason the module uses a separate hash table for each registered category: system call numbers, EUID and program names. Each hash table is accessed concurrently. Read operations are performed to verify if a given entry is registered while write operations are limited to registrations and deregistrations. Considering this expected strongly read-oriented workload Read Copy Update (R.C.U.) is the synchronization mechanism that best fits the module requirements. 
<br><br>
At the implementation level the Linux kernel provides the ```rhashtable``` API, a hash table implementation that supports dynamic resizing of the buckets, provides RCU compatible mechanism for safe and efficient concurrent access and automatically handles hash collissions.
Each hash table is represented by an ```hash_struct``` structure which contains the corresponding ```rhashtable``` instance and the ```spinlock``` used to synchronize RCU writers. The ```rhashtable``` storing system call numbers uses the system call number itself as the key of each node. The hash tabes storing EUIDs and program names, instead, require string based keys. The ```rhashtable``` API supports string keys by allowing the definition of custom hash (```str_hashfn()```) and comparison (```str_cmpfn()```) functions. These functions are used to compute the hash value associated with a string key and to verify key equality during lookup operations. The module prevents the insertion of duplicate entries and the removal of not existing entries. A system call invocation is considered **critical** when its system call number is present in the corresponding hash table and the invoking process matches an EUID registered in the EUID hash table and/or a program name registered in the program name hash table.


### 3.2. [Throttling mechanism](module/throttling.c)
The throttling mechanism represents the core functionality of the module.
The design of this mechanism can be divided into two main aspects: the implementation of system call invocation interception and the implementation of the actual delay mechanism. For the first aspect, the main design principle is to define a single interception point for system call invocations. This approach provides the significant advantage of managing a single access point, independent of the number of system call numbers being monitored, rather than requiring an interception mechanism that scales linearly with the number of monitored system calls. The trade-off of this approach is the introduction of an additional overhead, although minimal, on every system call invocation, incuding those that are not subject to monitoring. From an implementation perspective, ```krpobes``` provide the "simplest" mechanism while still being suitable for the requirements of this design. In particular, by exploiting the ```pre_handler``` callback, system call invocations can be intercepted before entering the actual system call implementation. This allows system calls to be handled uniformly, independently of whether they are blocking or non-blocking. Regarding the identification of a single access function for all system call invocations ```do_syscall_64``` was initially considered. However this function is included in the list of symbols blacklisted by ```kprobes```, preventing probe registration on it. A deeper analysis identified ```x64_sys_call``` as a suitable alternative interception point. The delay mechanism is implemented through a wait queue where threads exceeding the configured MAX rate are placed into a waiting state. Threads are awakened when throttling is disabled or when an atomic decrement of the ```limit_counter``` using ```atomic_dec_return(&limit_counter)``` indicates that a new invocation is allowed. A ```timer_list``` timer periodically expires every one second, resetting the counter for the new time window and waking up all sleeping threads, which then re evaluate their wake up condition. The wake up condition includes ```atomic_dec_return(&limit_counter)>= 0```, ensuring that the configured MAX rate is respected across concurrent thread wakes up.
<br><br>
The two implementation choices appear to be incompatible, since a kprobe handler cannot perform blocking operations and therefore cannot directly place the current task into a waiting queue. To overcome this limitation, a preliminary setup phase is introduced. During this phase, an additional kprobe is used to identify, through per-CPU variables, the address of the kprobe execution context. The context is then explicitly managed in the throttling probe pre handler to allow the required blocking operations before and after controlling preemption through ```preempt_disable()``` and ```preempt_enable()```. Care must be taken not to unregister the kprobe while tasks are still waiting in the wait queue. To ensure safe removal, an atomic ```presence_counter``` is used to track the number of tasks currently managed by the throttling mechanism. The kprobe can only be unregistered when ```presence_counter``` reaches zero.


### 3.3. [Driver](module/driver.c)
The driver creates the ```/dev/throttling``` device node without requiring the user to manually create it through ```mknod``` command. All configuration operations are exposed through ```ioctl``` interface. The ```ioctl``` command definitions are provided in a dedicated [header file](/module/include/driver_commands.h), allowing them to be easily imported and used by user space applications interacting with the module.
For ```ioctl``` commands requiring input from user space, the provided data is safetly retrieved using ```copy_from_user()```. Since all supported ```ioctl``` commands modify the module state, the driver verifies that the device has been opened with write permissions and that the caller has an effective user ID (EUID) equal to 0  before executing any operation.

### 3.4. [Metrics](module/metrics.c)
The module also mantains runtime metrics. In particular it records the maximum number of threads blocked within a single time window, the average number of blocked threads per time window, considering only the time windows during which the throttling mechanism is enabled and the system call number, program name and EUID associated with the thread that experienced the longest sleep time. The ```presence_counter``` corresponds to the number of blocked threads within a time window. These threads can only be woken up at the end of the time window. It also accounts for threads that remain sleeping when the limit is reset at the end of the time window, in cases where the number of already blocked threads exceeds the limit and not all of them can 
be woken up. The ```update_metrics()``` function is invoked by the timer at the end of each time window only when the throttling mechanism is enabled, preventing inactive periods from affecting the collected statistics. It updates all metrics except those related to the thread with the longest sleep time. The latter metrics are updated directly by the ```kprobe``` pre_handler which computes the time spent sleeping in ms with ```ktime_t``` and updates the recorded values whenever a new maximum sleep time is observed. Since ```float``` and ```double``` types are not used in kernel code, the average number of blocked threads is computed as follows: the accumulated number of blocked threads is multiplied by 1000 before being divided by the number of elapsed time windows, so that the resulting ```integer``` preserves three decimal digits of precision.

### 3.5. [Module output](module/proc-logger.c)
Information exposed by the module is provided through the ```proc``` virtual file system instead of the device driver. In particular the virtual directory hierarchy is organized as follows:

```text
/proc/SYS-THROTTLING/
├── status
├── metrics
└── registered-data/
    ├── euid
    ├── pn
    └── sn
```
-```status``` reports if the throttling mechanism is currently enabled and the configured MAX rate.
<br>
-```euid``` reports all EUIDs registered.
<br>
-```pn``` reports all program names registered.
<br>
-```sn``` reports all system call numbers registered.
<br>
-```metrics``` reports all metrics.
<br><br>
Each procfs entry is associated with a set of file operations. The ```open``` operation initializes the file access mechanism and associates the entry with its corresponding ```show``` function, which is responsible for generating the exported content. The ```show``` function is executed when the virtual file is read by user space (for example through the ```cat``` command). 
<br><br>
All reported metrics, except for the information related to the thread with the longest sleep time, refer to the completed time window rather than the currently active one. This avoids computing partial statistics during an ongoing window, which would incorrectly be considered representative of the entire time interval.
<br><br>
The ```show``` functions require synchronization with the execution contexts that update the exported information. The values reported in the ```status``` entry are stored in atomic variables and can therefore be safely read without additional locking mechanisms.<br>
The information related to registered EUIDs, program names and system call numbers is stored inside ```rhashtable``` structures protected by RCU. To safetly traverse these structures the module uses the ```rhashtable_walk``` interface through the ```rhashtable_iter```.<br>
The metrics updated by the timer when the throttling mechanism is enabled are synchronized through a ```spinlock``` to ensure consistency between concurrent update and read<br>
The information related to the thread with the longest sleep time (system call number, program name, EUID and sleep duration) is updated by the ```kprobe``` pre_handler. This represents a synchronization challenge, since multiple kprobe handlers may update these values concurrently while user accesses them through the procfs interface. Protecting these updates with a global ```spinlock``` would introduce significant performance overhead, as ```kprobes``` running on different  CPUs would unnecessarily contend on the same lock. To avoid this issue ```per-CPU variables``` are used. Since only one ```kprobe``` handler can be active on a given CPU at a time each ```per-CPU instance``` is accessed by a single writer. This removes contention between ```kprobe``` handlers running on different CPUs. The remaining synchronization between the writer and the procfs reader is handled using ```seqcount_t```. This mechanism allows readers to proceed without acquiring a lock and retry the read if a concurrent update is detected. Since each per-CPU instance has a single writer, no additional writer locking is required. As a result each ```per-CPU variable``` stores a local maximum sleep time together with the associated information (system call number, program name and EUID). These local values are updated independently on each CPU and synchronized through their corresponding per-CPU ```seqcount_t```. The procfs ```show``` function iterates over all CPUs and uses the associated ```seqcount_t``` to safetly read each local maximum. After collecting all local values the function compares them to determine the global maximum sleep time, along with the corresponding information, which is then exported.     


## 4. Controller implementation
The implementation is based on two ```getopt()``` parsing passes over the command-line arguments. The first pass is used to validate parameter consistency. In particular, it checks that the ```-e``` and ```-d``` options are mutually exclusive and specified at most once per command invocation. Enabling and disabling the throttling mechanism within the same command is not meaningful, nor is enabling or disabling it multiple times in a single invocation. The first pass also ensures that the ```-r``` option is specified only once. Allowing multiple ```-r``` options within the same command invocation would be meaningless, as each new value would overwrite the previous one and only the last MAX rate would be applied. If the ```-h``` option is specified, the controller displays the help message when the option is processed. Any subsequent  options are ignored and no corresponding operations are executed after the help message has been requested. Options appearing before ```-h``` are still parsed and checked according to the parameter consistency rules described above.<br><br>
The second ```getopt()``` parsing pass performs the actual execution of the requested operations. After obtaining the file descriptor of the ```/dev/throttling``` device through the ```open()``` system call, the controller issues the corresponding ```ioctl()``` commands to configure the kernel module.<br><br>
The separation between the validation phase and the execution phase allows the controller to verify the consistency of all command-line parameters before applying any configuration change. This design also enables the ```-s```, ```-z```, ```-i```, ```-l```, ```-p``` and ```-b``` options to be specified multiple times with the same command invocation, allowing multiple system call numbers, EUIDs and program names to be registered or deregistered without requiring multiple invocation of the controller application. 

## 5. Test implementation
The test is divided into three main phases: setup, execution and cleanup.<br><br>
During the setup phase the test program register its program name, a system call number and a MAX rate then enables the throttling mechanism.<br><br>
During the execution phase the test spawn a predefined number of threads each invoking the registered system call. The main thread waits for all worker threads to complete by calling ```pthread_join()```.
<br><br>
During the cleanup phase the test deregisters the previously registered system call number and program name and disables the throttling mechanism.
<br><br>
The test is self-contained as it performs both the configuration and the cleanup of the kernel module. Since the module requires configuration operations to be performed by a process with an effective user ID (EUID) of 0, the test must be executed with root privileges. For security reasons the test application doesnt register EUID 0 (root). Consequently the test relies on the combination of the registered system call and program name to trigger the throttling mechanism.
<br><br>
The system call used by the test is ```getcpu()``` (system call number 309) as it is rarely used in typical workloads. This minimizes the likelihood of interference from unrelated processes invoking the same system call while sharing the same program name.
<br><br>
The test execution time, measured in milliseconds, is recorded from the beginning of the execution phase until the completion of the cleanup phase. A delay is intetionally introduced between the setup and execution phases. By design of the module a newly configured MAX rate does not take effect immediatly but is applied starting from the next one-second time window. This prevents modifications to the rate limit from interfering with the correct operation of the throttling mechanism within the current time window.
<br><br>
By default the test is configured with MAX rate of 5 and 50 worker threads (50 system call invocations). These parameters can be modified directly in the [test source file](user/test/test.c). The system call used for the test can also be changed. However doing so requires updating both the configured system call number and the implementation of the worker thread to invoke the selected system call.
<br><br>
**TEST EXAMPLE**
<br>   
```text
sudo ./test
Test parameters:
    (*) Registered syscall number: 309
    (*) Registered syscall invocation counter: 50
    (*) Max rate: 5
    (*) Registered program name: test
Setup the test
Running the test
Test completed. Elapsed time: 9158.379 ms
```

```text
cat /proc/SYS-THROTTLING/metrics
(-) Throttler metrics:
        (*) MAX number of blocked threads within an epoch: 45
        (*) AVG number of blocked threads: 20.454
        (*) MAX sleep time is 9156 ms for:
                (**) Syscall number: 309
                (**) Program name: test
                (**) eUID: 0
```