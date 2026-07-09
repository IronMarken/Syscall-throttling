#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

#include "driver_commands.h"

#define MAX_LEN_STR 64

// Test params
#define COUNTER 50
#define RATE 5
// Syscall getcpu
#define SYSCALL_NUMBER 309


void *worker(void *arg) {
    unsigned cpu, node;
    syscall(SYS_getcpu, &cpu, &node, NULL);
} 

int main(int argc, char *argv[]) {

    // Program name
    char *pn = strrchr(argv[0], '/');
    if (pn != NULL)
        strcpy(pn, pn + 1);
    else
        strcpy(pn, argv[0]);
    
    
    int sn = SYSCALL_NUMBER;
    int counter = COUNTER;
    int rate = RATE;

    printf("Test parameters:\n");
    printf("    (*) Registered syscall number: %d\n", sn);
    printf("    (*) Registered syscall invocation counter: %d\n", counter);
    printf("    (*) Max rate: %d\n", rate);
    printf("    (*) Registered program name: %s\n", pn);

    // Setup test
    printf("Setup the test\n");

    // Open device
    char dev_full_name [MAX_LEN_STR] = "/dev/";
    strcat(dev_full_name, DEV_NAME);
    int fd = open(dev_full_name, O_RDWR);
    if (fd == -1) {
        printf("Error opening device: %s\n", strerror(errno));
        return 1;
    }
    
    // Register data and enable
    ioctl(fd, IOCTL_REGISTER_PN, pn);
    ioctl(fd, IOCTL_REGISTER_SN, &sn);
    ioctl(fd, IOCTL_SET_MAX_RATE, &rate);
    ioctl(fd, IOCTL_ENABLE_THROTTLING);
    sleep(2);

    // Start test
    pthread_t t[COUNTER];

    struct timespec t1, t2;

    // Generate threads
    printf("Running the test\n");
    clock_gettime(CLOCK_MONOTONIC, &t1);

    for (int i=0; i<counter; i++) {
        pthread_create(&t[i], NULL, worker, NULL);
    }

    // Wait threads
    for (int i=0; i<counter; i++) {
        pthread_join(t[i], NULL);
    }

    // Disable and unregister data
    ioctl(fd, IOCTL_DISABLE_THROTTLING); 
    ioctl(fd, IOCTL_UNREGISTER_PN, pn);
    ioctl(fd, IOCTL_UNREGISTER_SN, &sn);

    clock_gettime(CLOCK_MONOTONIC, &t2);
    double elapsed_ms = (t2.tv_sec - t1.tv_sec) * 1000.0 + (t2.tv_nsec - t1.tv_nsec) / 1e6;

    printf("Test completed. Elapsed time: %.3f ms\n", elapsed_ms);
    
    return 0;
}