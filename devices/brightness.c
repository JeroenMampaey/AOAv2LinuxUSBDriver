#include "brightness.h"
#include "../usb.h"

// Input is a 1 byte: 0xFF for brightness down, 0x01 for brightness up
#define ACCEPTED_WRITE_SIZE 1

/*
    Forward declarations for private functions for this brightness.c file
*/
static ssize_t brightness_write(struct file* File, const char* user_buffer, size_t count, loff_t* offs);
static int driver_open(struct inode* device_file, struct file* instance);
static int driver_close(struct inode* device_file, struct file* instance);

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = driver_open,
    .release = driver_close,
    .write = brightness_write
};

static dev_t brightness_device_nr;
static struct cdev brightness_device;
static struct class* brightness_device_class;

static unsigned int file_is_open[NUM_POSSIBLE_ACCESSORY_MODE_DEVICES];
static char buffer[NUM_POSSIBLE_ACCESSORY_MODE_DEVICES][ACCEPTED_WRITE_SIZE];
static char* brightness_hid_events[NUM_POSSIBLE_ACCESSORY_MODE_DEVICES];

int setup_brightness(void){
    for(int i=0; i<NUM_POSSIBLE_ACCESSORY_MODE_DEVICES; i++){
        brightness_hid_events[i] = NULL;
        file_is_open[i] = 0;
    }

    for(int i=0; i<NUM_POSSIBLE_ACCESSORY_MODE_DEVICES; i++){
        brightness_hid_events[i] = kmalloc(2, GFP_KERNEL);
        if(!brightness_hid_events[i]){
            goto setup_brightness_error0;
        }
    }

    if(alloc_chrdev_region(&brightness_device_nr, 0, NUM_POSSIBLE_ACCESSORY_MODE_DEVICES, "android_brightnesss") < 0){
		printk("aoa_hid_driver - brightness_device_nr could not be allocated\n");
		goto setup_brightness_error0;
	}

    if(!(brightness_device_class = class_create("android_brightness"))){
        printk("aoa_hid_driver - Error creating class for android brightness");
        goto setup_brightness_error1;
    }

    cdev_init(&brightness_device, &fops);
    if(cdev_add(&brightness_device, brightness_device_nr, NUM_POSSIBLE_ACCESSORY_MODE_DEVICES)){
        printk("aoa_hid_driver - Error adding brightness device\n");
        goto setup_brightness_error2;
    }

    return 0;

setup_brightness_error2:
    class_destroy(brightness_device_class);

setup_brightness_error1:
    unregister_chrdev_region(brightness_device_nr, NUM_POSSIBLE_ACCESSORY_MODE_DEVICES);

setup_brightness_error0:
    for(int i=0; i<NUM_POSSIBLE_ACCESSORY_MODE_DEVICES; i++){
        if(brightness_hid_events[i]){
            kfree(brightness_hid_events[i]);
        }
    }

    return -1;
}

void cleanup_brightness(void){
    cdev_del(&brightness_device);
    class_destroy(brightness_device_class);
    unregister_chrdev_region(brightness_device_nr, NUM_POSSIBLE_ACCESSORY_MODE_DEVICES);
    for(int i=0; i<NUM_POSSIBLE_ACCESSORY_MODE_DEVICES; i++){
        kfree(brightness_hid_events[i]);
    }
}

int add_brightness_device(int minor){
    if(device_create(brightness_device_class, NULL, brightness_device_nr + minor, NULL, "android_brightness%d", minor)==NULL){
		printk("aoa_hid_driver - Can not create device file for minor %d\n", minor);
		goto add_brightness_device_error0;
	}

    return 0;

add_brightness_device_error0:
    return -1;
}

void remove_brightness_device(int minor){
    device_destroy(brightness_device_class, brightness_device_nr + minor);
}

static ssize_t brightness_write(struct file* File, const char* user_buffer, size_t count, loff_t* offs){
    if(count != ACCEPTED_WRITE_SIZE){
        printk("aoa_hid_driver - Error writing to brightness device, a single write to the brightness can handle %d characters but attempted to write %d characters instead\n", ACCEPTED_WRITE_SIZE, (int)count);
        return -EINVAL;
    }

    int minor = iminor(file_inode(File));
    int not_copied = copy_from_user(buffer[minor], user_buffer, count);

    if(buffer[minor][0] != 0xFF && buffer[minor][0] != 0x01){
        printk("aoa_hid_driver - Error writing to brightness device, the fourth byte of the write must be either 0xFF or 0x01\n");
        return -EINVAL;
    }

    brightness_hid_events[minor][0] = 0x03;
    brightness_hid_events[minor][1] = (buffer[minor][0] == 0xFF) ? 0x70 : 0x6F;
    usb_control_msg(get_usb_device(minor), usb_sndctrlpipe(get_usb_device(minor), 0), ACCESSORY_SEND_HID_EVENT, USB_DIR_OUT | USB_TYPE_VENDOR, 1, 0, brightness_hid_events[minor], 2, 1000);

    msleep(100);
    brightness_hid_events[minor][0] = 0x03;
    brightness_hid_events[minor][1] = 0x00;
    usb_control_msg(get_usb_device(minor), usb_sndctrlpipe(get_usb_device(minor), 0), ACCESSORY_SEND_HID_EVENT, USB_DIR_OUT | USB_TYPE_VENDOR, 1, 0, brightness_hid_events[minor], 2, 1000);

    int delta = count - not_copied;
    return delta;
}

static int driver_open(struct inode* device_file, struct file* instance){
    int minor = iminor(device_file);

    if(atomic_cmpxchg((atomic_t*)&file_is_open[minor], 0, 1)){
        printk("aoa_hid_driver - Error opening brightness device, device is already open\n");
        return -EBUSY;
    }

    return 0;
}

static int driver_close(struct inode* device_file, struct file* instance){
    int minor = iminor(device_file);

    if(!atomic_cmpxchg((atomic_t*)&file_is_open[minor], 1, 0)){
        printk("aoa_hid_driver - Error closing brightness device, device is already closed\n");
        return -EBUSY;
    }

    return 0;
}