#include <linux/module.h>
#include <linux/init.h>
#include "sys_files.h"
#include "usb.h"

static int aoa_hid_driver_module_init(void){
	printk("aoa_hid_driver - aoa_hid_driver_module_init\n");

	if(setup_sysfs()){
		goto module_init_error0;
	}

	if(setup_usb()){
		goto module_init_error1;
	}

	return 0;

module_init_error1:
	cleanup_sysfs();

module_init_error0:
	printk("aoa_hid_driver - aoa_hid_driver_module_init exited with an error\n");
	return -1;
}

static void aoa_hid_driver_module_exit(void){
	printk("aoa_hid_driver - aoa_hid_driver_module_exit\n");

	cleanup_sysfs();
	cleanup_usb();
}

module_init(aoa_hid_driver_module_init);
module_exit(aoa_hid_driver_module_exit);

MODULE_LICENSE("GPL");
