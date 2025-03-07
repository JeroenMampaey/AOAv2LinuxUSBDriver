#include "touch.h"
#include "../usb.h"

// Input is a two-tuple: ([0, 255],[0, 255]) => 2 bytes
#define ACCEPTED_WRITE_SIZE 2

/*
    Forward declarations for private functions for this touch.c file
*/
static ssize_t touch_write(struct file* File, const char* user_buffer, size_t count, loff_t* offs);
static int driver_open(struct inode* device_file, struct file* instance);
static int driver_close(struct inode* device_file, struct file* instance);

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = driver_open,
    .release = driver_close,
    .write = touch_write
};

static dev_t touch_device_nr;
static struct cdev touch_device;
static struct class* touch_device_class;

static unsigned int file_is_open[NUM_POSSIBLE_ACCESSORY_MODE_DEVICES];
static char buffer[NUM_POSSIBLE_ACCESSORY_MODE_DEVICES][ACCEPTED_WRITE_SIZE];
static char* touch_hid_events[NUM_POSSIBLE_ACCESSORY_MODE_DEVICES];

int setup_touch(void){
    for(int i=0; i<NUM_POSSIBLE_ACCESSORY_MODE_DEVICES; i++){
        touch_hid_events[i] = NULL;
        file_is_open[i] = 0;
    }

    for(int i=0; i<NUM_POSSIBLE_ACCESSORY_MODE_DEVICES; i++){
        touch_hid_events[i] = kmalloc(6, GFP_KERNEL);
        if(!touch_hid_events[i]){
            goto setup_touch_error0;
        }
    }

    if(alloc_chrdev_region(&touch_device_nr, 0, NUM_POSSIBLE_ACCESSORY_MODE_DEVICES, "android_touches") < 0){
		printk("aoa_hid_driver - touch_device_nr could not be allocated\n");
		goto setup_touch_error0;
	}

    if(!(touch_device_class = class_create("android_touch"))){
        printk("aoa_hid_driver - Error creating class for android touch");
        goto setup_touch_error1;
    }

    cdev_init(&touch_device, &fops);
    if(cdev_add(&touch_device, touch_device_nr, NUM_POSSIBLE_ACCESSORY_MODE_DEVICES)){
        printk("aoa_hid_driver - Error adding touch device\n");
        goto setup_touch_error2;
    }

    return 0;

setup_touch_error2:
    class_destroy(touch_device_class);

setup_touch_error1:
    unregister_chrdev_region(touch_device_nr, NUM_POSSIBLE_ACCESSORY_MODE_DEVICES);

setup_touch_error0:
    for(int i=0; i<NUM_POSSIBLE_ACCESSORY_MODE_DEVICES; i++){
        if(touch_hid_events[i]){
            kfree(touch_hid_events[i]);
        }
    }

    return -1;
}

void cleanup_touch(void){
    cdev_del(&touch_device);
    class_destroy(touch_device_class);
    unregister_chrdev_region(touch_device_nr, NUM_POSSIBLE_ACCESSORY_MODE_DEVICES);
    for(int i=0; i<NUM_POSSIBLE_ACCESSORY_MODE_DEVICES; i++){
        kfree(touch_hid_events[i]);
    }
}

int add_touch_device(int minor){
    if(device_create(touch_device_class, NULL, touch_device_nr + minor, NULL, "android_touch%d", minor)==NULL){
		printk("aoa_hid_driver - Can not create device file for minor %d\n", minor);
		goto add_touch_device_error0;
	}

    return 0;

add_touch_device_error0:
    return -1;
}

void remove_touch_device(int minor){
    device_destroy(touch_device_class, touch_device_nr + minor);
}

static ssize_t touch_write(struct file* File, const char* user_buffer, size_t count, loff_t* offs){
    if(count != ACCEPTED_WRITE_SIZE){
        printk("aoa_hid_driver - Error writing to touch device, a single write to the touch can handle %d characters but attempted to write %d characters instead\n", ACCEPTED_WRITE_SIZE, (int)count);
        return -EINVAL;
    }

    int minor = iminor(file_inode(File));
    int not_copied = copy_from_user(buffer[minor], user_buffer, count);

    unsigned char x_rel = buffer[minor][0];
    unsigned char y_rel = buffer[minor][1];

    unsigned short x = (32767*((unsigned int)x_rel))/0xFF;
    unsigned short y = (32767*((unsigned int)y_rel))/0xFF;

    touch_hid_events[minor][0] = 0x04;
    touch_hid_events[minor][1] = 0b11;
    touch_hid_events[minor][2] = x & 0xFF;
    touch_hid_events[minor][3] = (x >> 8) & 0xFF;
    touch_hid_events[minor][4] = y & 0xFF;
    touch_hid_events[minor][5] = (y >> 8) & 0xFF;

    usb_control_msg(get_usb_device(minor), usb_sndctrlpipe(get_usb_device(minor), 0), ACCESSORY_SEND_HID_EVENT, USB_DIR_OUT | USB_TYPE_VENDOR, 1, 0, touch_hid_events[minor], 6, 1000);

    touch_hid_events[minor][0] = 0x04;
    touch_hid_events[minor][1] = 0x00;
    touch_hid_events[minor][2] = x & 0xFF;
    touch_hid_events[minor][3] = (x >> 8) & 0xFF;
    touch_hid_events[minor][4] = y & 0xFF;
    touch_hid_events[minor][5] = (y >> 8) & 0xFF;
    usb_control_msg(get_usb_device(minor), usb_sndctrlpipe(get_usb_device(minor), 0), ACCESSORY_SEND_HID_EVENT, USB_DIR_OUT | USB_TYPE_VENDOR, 1, 0, touch_hid_events[minor], 6, 1000);
    
    int delta = count - not_copied;
    return delta;
}

static int driver_open(struct inode* device_file, struct file* instance){
    int minor = iminor(device_file);

    if(atomic_cmpxchg((atomic_t*)&file_is_open[minor], 0, 1)){
        printk("aoa_hid_driver - Error opening touch device, device is already open\n");
        return -EBUSY;
    }

    return 0;
}

static int driver_close(struct inode* device_file, struct file* instance){
    int minor = iminor(device_file);

    if(!atomic_cmpxchg((atomic_t*)&file_is_open[minor], 1, 0)){
        printk("aoa_hid_driver - Error closing touch device, device is already closed\n");
        return -EBUSY;
    }

    return 0;
}