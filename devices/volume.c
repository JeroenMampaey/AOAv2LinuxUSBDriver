#include "volume.h"
#include "../usb.h"

// Input is a 1 byte: 0xFF for volume down, 0x01 for volume up
#define ACCEPTED_WRITE_SIZE 1

/*
    Forward declarations for private functions for this volume.c file
*/
static ssize_t volume_write(struct file* File, const char* user_buffer, size_t count, loff_t* offs);
static int driver_open(struct inode* device_file, struct file* instance);
static int driver_close(struct inode* device_file, struct file* instance);

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = driver_open,
    .release = driver_close,
    .write = volume_write
};

static dev_t volume_device_nr;
static struct cdev volume_device;
static struct class* volume_device_class;

static unsigned int file_is_open[NUM_POSSIBLE_ACCESSORY_MODE_DEVICES];
static char buffer[NUM_POSSIBLE_ACCESSORY_MODE_DEVICES][ACCEPTED_WRITE_SIZE];
static char* volume_hid_events[NUM_POSSIBLE_ACCESSORY_MODE_DEVICES];

int setup_volume(void){
    for(int i=0; i<NUM_POSSIBLE_ACCESSORY_MODE_DEVICES; i++){
        volume_hid_events[i] = NULL;
        file_is_open[i] = 0;
    }

    for(int i=0; i<NUM_POSSIBLE_ACCESSORY_MODE_DEVICES; i++){
        volume_hid_events[i] = kmalloc(2, GFP_KERNEL);
        if(!volume_hid_events[i]){
            goto setup_volume_error0;
        }
    }

    if(alloc_chrdev_region(&volume_device_nr, 0, NUM_POSSIBLE_ACCESSORY_MODE_DEVICES, "android_volumes") < 0){
		printk("aoa_hid_driver - volume_device_nr could not be allocated\n");
		goto setup_volume_error0;
	}

    if(!(volume_device_class = class_create("android_volume"))){
        printk("aoa_hid_driver - Error creating class for android volume");
        goto setup_volume_error1;
    }

    cdev_init(&volume_device, &fops);
    if(cdev_add(&volume_device, volume_device_nr, NUM_POSSIBLE_ACCESSORY_MODE_DEVICES)){
        printk("aoa_hid_driver - Error adding volume device\n");
        goto setup_volume_error2;
    }

    return 0;

setup_volume_error2:
    class_destroy(volume_device_class);

setup_volume_error1:
    unregister_chrdev_region(volume_device_nr, NUM_POSSIBLE_ACCESSORY_MODE_DEVICES);

setup_volume_error0:
    for(int i=0; i<NUM_POSSIBLE_ACCESSORY_MODE_DEVICES; i++){
        if(volume_hid_events[i]){
            kfree(volume_hid_events[i]);
        }
    }

    return -1;
}

void cleanup_volume(void){
    cdev_del(&volume_device);
    class_destroy(volume_device_class);
    unregister_chrdev_region(volume_device_nr, NUM_POSSIBLE_ACCESSORY_MODE_DEVICES);
    for(int i=0; i<NUM_POSSIBLE_ACCESSORY_MODE_DEVICES; i++){
        kfree(volume_hid_events[i]);
    }
}

int add_volume_device(int minor){
    if(device_create(volume_device_class, NULL, volume_device_nr + minor, NULL, "android_volume%d", minor)==NULL){
		printk("aoa_hid_driver - Can not create device file for minor %d\n", minor);
		goto add_volume_device_error0;
	}

    return 0;

add_volume_device_error0:
    return -1;
}

void remove_volume_device(int minor){
    device_destroy(volume_device_class, volume_device_nr + minor);
}

static ssize_t volume_write(struct file* File, const char* user_buffer, size_t count, loff_t* offs){
    if(count != ACCEPTED_WRITE_SIZE){
        printk("aoa_hid_driver - Error writing to volume device, a single write to the volume can handle %d characters but attempted to write %d characters instead\n", ACCEPTED_WRITE_SIZE, (int)count);
        return -EINVAL;
    }

    int minor = iminor(file_inode(File));
    int not_copied = copy_from_user(buffer[minor], user_buffer, count);

    if(buffer[minor][0] != 0xFF && buffer[minor][0] != 0x01){
        printk("aoa_hid_driver - Error writing to volume device, the fourth byte of the write must be either 0xFF or 0x01\n");
        return -EINVAL;
    }

    volume_hid_events[minor][0] = 0x03;
    volume_hid_events[minor][1] = (buffer[minor][0] == 0xFF) ? 0xEA : 0xE9;
    usb_control_msg(get_usb_device(minor), usb_sndctrlpipe(get_usb_device(minor), 0), ACCESSORY_SEND_HID_EVENT, USB_DIR_OUT | USB_TYPE_VENDOR, 1, 0, volume_hid_events[minor], 2, 1000);

    msleep(100);
    volume_hid_events[minor][0] = 0x03;
    volume_hid_events[minor][1] = 0x00;
    usb_control_msg(get_usb_device(minor), usb_sndctrlpipe(get_usb_device(minor), 0), ACCESSORY_SEND_HID_EVENT, USB_DIR_OUT | USB_TYPE_VENDOR, 1, 0, volume_hid_events[minor], 2, 1000);

    int delta = count - not_copied;
    return delta;
}

static int driver_open(struct inode* device_file, struct file* instance){
    int minor = iminor(device_file);

    if(atomic_cmpxchg((atomic_t*)&file_is_open[minor], 0, 1)){
        printk("aoa_hid_driver - Error opening volume device, device is already open\n");
        return -EBUSY;
    }

    return 0;
}

static int driver_close(struct inode* device_file, struct file* instance){
    int minor = iminor(device_file);

    if(!atomic_cmpxchg((atomic_t*)&file_is_open[minor], 1, 0)){
        printk("aoa_hid_driver - Error closing volume device, device is already closed\n");
        return -EBUSY;
    }

    return 0;
}