#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "driver_commands.h"

#define MAX_LEN_STR 64

void help() {
    printf("Usage:\n");
    printf("    -h      Show this help message\n");
    printf("    -e      Enable throttling\n");
    printf("    -d      Disable throttling\n");
    printf("    -s NUM  Register syscall number NUM\n");
    printf("    -z NUM  Unregister syscall number NUM\n");
    printf("    -i EUID Register euid EUID\n");
    printf("    -l EUID Unregister euid EUID\n");
    printf("    -p PN   Register program name PN\n");
    printf("    -b PN   Unregister program name PN\n");
    printf("    -r LIM  Set max rate LIM\n");
    printf("\nOptions e, d and r can be used once each\n");
    printf("Options e and d can't be used together\n");
}


/*  Allow multiple registrations/deregistrations but 
    single enabled, disable and set max rate. 
    Additionally can use only disable or enable not both.
    Check unsupported commands.
    Just end if -h  */
int check_args(int argc, char *argv[]) {
    int enable = 0;
    int disable = 0;
    int rate_already_set = 0;
    
    int opt;
    while ((opt = getopt(argc, argv, "heds:z:i:l:p:b:r:")) != -1) {
        switch (opt) {
            case 'h':
                help();
                return 1;
            case 'e':
                // Check if multiple enable
                if (enable) {
                    printf("Option 'e' can be used once\n");
                    return 1;
                }
                
                // Check enable and disable together
                if (disable) {
                    printf("Options 'e' and 'd' can't be used together\n");
                    return 1;
                }

                enable = 1;
                break;
            case 'd':
                // Check if multiple disable
                if (disable) {
                    printf("Option 'd' can be used once\n");
                    return 1;
                }
                
                // Check enable and disable together
                if (enable) {
                    printf("Options 'e' and 'd' can't be used together\n");
                    return 1;
                }

                disable = 1;
                break;
            case 'r':
                // Check if multiple set max rate
                if (rate_already_set) {
                    printf("Option 'r' can be used once\n");
                    return 1;
                }

                rate_already_set = 1;
                break;
            case 's':
            case 'z':
            case 'i':
            case 'l':
            case 'p':
            case 'b':
                break;
            default:
                help();
                return 1;
        }
    }
    return 0;
}

// Exec all the commands
void exec_command(int argc, char *argv[]) {
    // Open device
    char dev_full_name [MAX_LEN_STR] = "/dev/";
    strcat(dev_full_name, DEV_NAME);

    int fd = open(dev_full_name, O_RDWR);
    if (fd == -1) {
        printf("Error opening device: %s\n", strerror(errno));
        return;
    }

    // Exec commands
    int opt;
    char *str_val;
    int int_val;

    while ((opt = getopt(argc, argv, "heds:z:i:l:p:b:r:")) != -1) {
        switch (opt) {
            case 'h':
                // Done in check_args
                break;
            case 'e':
                printf("Enable throttling\n");
                ioctl(fd, IOCTL_ENABLE_THROTTLING);
                break;
            case 'd':
                printf("Disable throttling\n");
                ioctl(fd, IOCTL_DISABLE_THROTTLING);
                break;
            case 's':
                printf("Register syscall number: %s\n", optarg);
                int_val = (int) strtol(optarg, &str_val, 10);
                
                // Check if totally converted or error
                if (*str_val != '\0') {
                    printf("Error converting arg to int. Skipping operation\n");
                    break;
                }

                ioctl(fd, IOCTL_REGISTER_SN, &int_val);
                break;
            case 'z':
                printf("Unregister syscall number: %s\n", optarg);
                int_val = (int) strtol(optarg, &str_val, 10);
                
                // Check if totally converted or error
                if (*str_val != '\0') {
                    printf("Error converting arg to int. Skipping operation\n");
                    break;
                }

                ioctl(fd, IOCTL_UNREGISTER_SN, &int_val);
                break;
            case 'i':
                printf("Register euid: %s\n", optarg);
                ioctl(fd, IOCTL_REGISTER_EUID, optarg);
                break;
            case 'l':
                printf("Unregister euid: %s\n", optarg);
                ioctl(fd, IOCTL_UNREGISTER_EUID, optarg);
                break;
            case 'p':
                printf("Register program name: %s\n", optarg);
                ioctl(fd, IOCTL_REGISTER_PN, optarg);
                break;
            case 'b':
                printf("Unregister program name: %s\n", optarg);
                ioctl(fd, IOCTL_UNREGISTER_PN, optarg);
                break;
            case 'r':
                printf("Set max rate: %s\n", optarg);
                int_val = (int) strtol(optarg, &str_val, 10);
                
                // Check if totally converted or error
                if (*str_val != '\0') {
                    printf("Error converting arg to int. Skipping operation\n");
                    break;
                }
                if (int_val >= 0)
                    ioctl(fd, IOCTL_SET_MAX_RATE, &int_val);
                else
                    printf("Can't set a negative max rate\n");
                break;
            default:
                // Never executed. Args already checked 
                return;
        }
    }
    
    close(fd);
    return;
}

int main(int argc, char *argv[]) {
    // Check args
    int ret;
    ret = check_args(argc, argv);

    if (ret)
        return 0;
    
    // Reset getopt
    optind = 1;

    // Exec commands
    exec_command(argc, argv);
    
    return 0;
}