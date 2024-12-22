#include "mouse.h"
#include "../usb.h"

// Input is a four-tuple: ([-127, 127],[-127, 127],[-127,127],[0,1]) => 4 bytes
#define ACCEPTED_WRITE_SIZE 4

/*
    Forward declarations for private functions for this mouse.c file
*/
static ssize_t mouse_write(struct file* File, const char* user_buffer, size_t count, loff_t* offs);
static int driver_open(struct inode* device_file, struct file* instance);
static int driver_close(struct inode* device_file, struct file* instance);

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = driver_open,
    .release = driver_close,
    .write = mouse_write
};

static dev_t mouse_device_nr;
static struct cdev mouse_device;
static struct class* mouse_device_class;

static unsigned int file_is_open[NUM_POSSIBLE_ACCESSORY_MODE_DEVICES];
static char buffer[NUM_POSSIBLE_ACCESSORY_MODE_DEVICES][ACCEPTED_WRITE_SIZE];
static char* mouse_hid_events[NUM_POSSIBLE_ACCESSORY_MODE_DEVICES];

int setup_mouse(void){
    for(int i=0; i<NUM_POSSIBLE_ACCESSORY_MODE_DEVICES; i++){
        mouse_hid_events[i] = NULL;
        file_is_open[i] = 0;
    }

    for(int i=0; i<NUM_POSSIBLE_ACCESSORY_MODE_DEVICES; i++){
        mouse_hid_events[i] = kmalloc(5, GFP_KERNEL);
        if(!mouse_hid_events[i]){
            goto setup_mouse_error0;
        }
    }

    if(alloc_chrdev_region(&mouse_device_nr, 0, NUM_POSSIBLE_ACCESSORY_MODE_DEVICES, "android_mouses") < 0){
		printk("aoa_hid_driver - mouse_device_nr could not be allocated\n");
		goto setup_mouse_error0;
	}

    if(!(mouse_device_class = class_create("android_mouse"))){
        printk("aoa_hid_driver - Error creating class for android mouse");
        goto setup_mouse_error1;
    }

    cdev_init(&mouse_device, &fops);
    if(cdev_add(&mouse_device, mouse_device_nr, NUM_POSSIBLE_ACCESSORY_MODE_DEVICES)){
        printk("aoa_hid_driver - Error adding mouse device\n");
        goto setup_mouse_error2;
    }

    return 0;

setup_mouse_error2:
    class_destroy(mouse_device_class);

setup_mouse_error1:
    unregister_chrdev_region(mouse_device_nr, NUM_POSSIBLE_ACCESSORY_MODE_DEVICES);

setup_mouse_error0:
    for(int i=0; i<NUM_POSSIBLE_ACCESSORY_MODE_DEVICES; i++){
        if(mouse_hid_events[i]){
            kfree(mouse_hid_events[i]);
        }
    }

    return -1;
}

void cleanup_mouse(void){
    cdev_del(&mouse_device);
    class_destroy(mouse_device_class);
    unregister_chrdev_region(mouse_device_nr, NUM_POSSIBLE_ACCESSORY_MODE_DEVICES);
    for(int i=0; i<NUM_POSSIBLE_ACCESSORY_MODE_DEVICES; i++){
        kfree(mouse_hid_events[i]);
    }
}

int add_mouse_device(int minor){
    if(device_create(mouse_device_class, NULL, mouse_device_nr + minor, NULL, "android_mouse%d", minor)==NULL){
		printk("aoa_hid_driver - Can not create device file for minor %d\n", minor);
		goto add_mouse_device_error0;
	}

    return 0;

add_mouse_device_error0:
    return -1;
}

void remove_mouse_device(int minor){
    device_destroy(mouse_device_class, mouse_device_nr + minor);
}

static ssize_t mouse_write(struct file* File, const char* user_buffer, size_t count, loff_t* offs){
    if(count != ACCEPTED_WRITE_SIZE){
        printk("aoa_hid_driver - Error writing to mouse device, a single write to the mouse can handle %d characters but attempted to write %d characters instead\n", ACCEPTED_WRITE_SIZE, (int)count);
        return -EINVAL;
    }

    int minor = iminor(file_inode(File));
    int not_copied = copy_from_user(buffer[minor], user_buffer, count);

    mouse_hid_events[minor][0] = 0x02;
    mouse_hid_events[minor][1] = buffer[minor][3];
    mouse_hid_events[minor][2] = buffer[minor][0];
    mouse_hid_events[minor][3] = buffer[minor][1];
    mouse_hid_events[minor][4] = buffer[minor][2];

    usb_control_msg(get_usb_device(minor), usb_sndctrlpipe(get_usb_device(minor), 0), ACCESSORY_SEND_HID_EVENT, USB_DIR_OUT | USB_TYPE_VENDOR, 1, 0, mouse_hid_events[minor], 5, 1000);

    int delta = count - not_copied;
    return delta;
}

static int driver_open(struct inode* device_file, struct file* instance){
    int minor = iminor(device_file);

    if(atomic_cmpxchg((atomic_t*)&file_is_open[minor], 0, 1)){
        printk("aoa_hid_driver - Error opening mouse device, device is already open\n");
        return -EBUSY;
    }

    return 0;
}

static int driver_close(struct inode* device_file, struct file* instance){
    int minor = iminor(device_file);

    if(!atomic_cmpxchg((atomic_t*)&file_is_open[minor], 1, 0)){
        printk("aoa_hid_driver - Error closing mouse device, device is already closed\n");
        return -EBUSY;
    }

    return 0;
}