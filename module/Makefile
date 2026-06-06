syscall-throttling-objs := monitoring-data.o main.o
obj-m := syscall-throttling.o

PWD := $(shell pwd)

EXTRA_CFLAGS += -I$(PWD)

compile:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

mount:
	insmod syscall-throttling.ko

umount:
	rmmod syscall-throttling
