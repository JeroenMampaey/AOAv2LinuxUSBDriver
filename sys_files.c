#include "sys_files.h"
#include <linux/fs.h>
#include <linux/sysfs.h>
#include <linux/device.h>
#include <linux/atomic.h>
#include <linux/kobject.h>

#define MAX_ANDROID_DEVICE_IDS 25

/*
	Forward declarations for private functions for this sys_files.c file
*/
static ssize_t add_known_device_store(struct kobject* kobj, struct kobj_attribute *attr, const char* buffer, size_t count);
static ssize_t remove_known_device_store(struct kobject* kobj, struct kobj_attribute *attr, const char* buffer, size_t count);
static ssize_t show_known_devices_show(struct kobject* kobj, struct kobj_attribute *attr, char* buffer);

static u32 known_device_ids[MAX_ANDROID_DEVICE_IDS];
static int num_known_device_ids = 0;
static struct kobject *android_usb_kobj;

static struct kobj_attribute add_known_device_attr = __ATTR(add_known_device, 0660, NULL, add_known_device_store);
static struct kobj_attribute remove_known_device_attr = __ATTR(remove_known_device, 0660, NULL, remove_known_device_store);
static struct kobj_attribute show_known_devices_attr = __ATTR(show_known_devices, 0660, show_known_devices_show, NULL);

int setup_sysfs(void){
	if(!(android_usb_kobj = kobject_create_and_add("android_usb", kernel_kobj))){
		printk("aoa_hid_driver - Error creating /sys/kernel/android_usb\n");
		goto setup_sysfs_error0;
	}

	if(sysfs_create_file(android_usb_kobj, &add_known_device_attr.attr)){
		printk("aoa_hid_driver - Error creating /sys/kernel/android_usb/add_known_device\n");
		goto setup_sysfs_error1;
	}

	if(sysfs_create_file(android_usb_kobj, &remove_known_device_attr.attr)){
		printk("aoa_hid_driver - Error creating /sys/kernel/android_usb/remove_known_device\n");
		goto setup_sysfs_error2;
	}

	if(sysfs_create_file(android_usb_kobj, &show_known_devices_attr.attr)){
		printk("aoa_hid_driver - Error creating /sys/kernel/android_usb/show_known_devices\n");
		goto setup_sysfs_error3;
	}

	return 0;

setup_sysfs_error3:
	sysfs_remove_file(android_usb_kobj, &remove_known_device_attr.attr);

setup_sysfs_error2:
	sysfs_remove_file(android_usb_kobj, &add_known_device_attr.attr);

setup_sysfs_error1:
	kobject_put(android_usb_kobj);

setup_sysfs_error0:
	return -1;
}

void cleanup_sysfs(void){
	sysfs_remove_file(android_usb_kobj, &show_known_devices_attr.attr);
	sysfs_remove_file(android_usb_kobj, &remove_known_device_attr.attr);
	sysfs_remove_file(android_usb_kobj, &add_known_device_attr.attr);
	kobject_put(android_usb_kobj);
}

static ssize_t add_known_device_store(struct kobject* kobj, struct kobj_attribute *attr, const char* buffer, size_t count){
	unsigned short id_vendor, id_product;
	
	int ret = sscanf(buffer, "%hx %hx", &id_vendor, &id_product);
	if(ret != 2){
		printk("aoa_hid_driver - Invalid input \"%s\" for add_known_device\n", buffer);
		return -EINVAL;
	}

	if(num_known_device_ids >= MAX_ANDROID_DEVICE_IDS){
		printk("aoa_hid_driver - No more space for additional known devices\n");
		return -ENOMEM;
	}

	known_device_ids[num_known_device_ids] = (((u32)id_vendor) << 16) | ((u32)id_product);
	atomic_inc((atomic_t*)&num_known_device_ids);

	return count;
}

static ssize_t remove_known_device_store(struct kobject* kobj, struct kobj_attribute *attr, const char* buffer, size_t count){
	unsigned short id_vendor, id_product;
	
	int ret = sscanf(buffer, "%hx %hx", &id_vendor, &id_product);
	if(ret != 2){
		printk("aoa_hid_driver - Invalid input \"%s\" for remove_known_device\n", buffer);
		return -EINVAL;
	}

	u32 id = (((u32)id_vendor) << 16) | ((u32)id_product);
	int i;
	for(i = 0; i < num_known_device_ids; i++){
		if(known_device_ids[i] == id){
			break;
		}
	}

	if(i == num_known_device_ids){
		printk("aoa_hid_driver - Device %04x:%04x not found in known devices\n", id_vendor, id_product);
		return -EINVAL;
	}

	atomic_set((atomic_t*)&known_device_ids[i], known_device_ids[num_known_device_ids - 1]);
	atomic_set((atomic_t*)&known_device_ids[num_known_device_ids - 1], 0);
	atomic_dec((atomic_t*)&num_known_device_ids);

	return count;
}

static ssize_t show_known_devices_show(struct kobject* kobj, struct kobj_attribute *attr, char* buffer){
	int offset = 0;
	for(int i = 0; i < num_known_device_ids; i++){
		// Since num_known_device_ids is max 25 and each entry has 10 bytes, we can safely assume that the buffer is large enough
		offset += sprintf(buffer + offset, "%04x:%04x\n", (unsigned short)(known_device_ids[i] >> 16), (unsigned short)known_device_ids[i]);
	}

	return offset;
}

bool is_android_device(u16 id_vendor, u16 id_product){
	u32 id = (((u32)id_vendor) << 16) | ((u32)id_product);
	for(int i = 0; i < num_known_device_ids; i++){
		unsigned int known_id = known_device_ids[i];

		if(known_id == 0){
			break;
		}
		else if(known_id == id){
			return true;
		}
	}

	return false;
}