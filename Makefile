.PHONY: install uninstall

obj-m += aoa_hid_driver.o
aoa_hid_driver-objs := module.o sys_files.o usb.o hid_descriptor.o devices/keyboard.o devices/mouse.o

all: module

module:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

install: module
	sudo insmod aoa_hid_driver.ko

uninstall:
	sudo rmmod aoa_hid_driver