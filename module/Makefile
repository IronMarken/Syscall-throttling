syscall-throttling-objs := monitoring-data.o main.o driver.o throttling.o
obj-m := syscall-throttling.o

PWD := $(shell pwd)

ccflags-y += -I$(PWD)/include

compile:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

mount:
	insmod syscall-throttling.ko

umount:
	rmmod syscall-throttling
