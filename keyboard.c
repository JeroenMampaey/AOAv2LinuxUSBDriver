#include "keyboard.h"
#include "usb.h"

/*
    Forward declarations for private functions for this keyboard.c file
*/
static ssize_t keyboard_write(struct file *File, const char *user_buffer, size_t count, loff_t *offs);

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .write = keyboard_write
};

static struct cdev keyboard_device;

int setup_keyboard(dev_t dev_nr){
    cdev_init(&keyboard_device, &fops);
    if(cdev_add(&keyboard_device, dev_nr, NUM_POSSIBLE_ACCESSORY_MODE_DEVICES)){
        printk("aoa_hid_driver - Error adding keyboard device\n");
        goto setup_keyboard_error0;
    }

    return 0;

setup_keyboard_error0:
    return -1;
}

void cleanup_keyboard(void){
    cdev_del(&keyboard_device);
}

int add_keyboard_device(struct class* dev_class, dev_t dev_nr){
    printk("aoa_hid_driver - Adding keyboard device for %u\n", (unsigned int)dev_nr);
    if(device_create(dev_class, NULL, dev_nr, NULL, "android_keyboard%d", minor(dev_nr))==NULL){
		printk("aoa_hid_driver - Can not create device file for minor %d\n", minor(dev_nr));
		goto add_keyboard_device_error0;
	}

    return 0;

add_keyboard_device_error0:
    return -1;
}

void remove_keyboard_device(struct class* dev_class, dev_t dev_nr){
    device_destroy(dev_class, dev_nr);
}

static ssize_t keyboard_write(struct file *File, const char *user_buffer, size_t count, loff_t *offs){
    // TODO
    printk("aoa_hid_driver - Keyboard write was called\n");
    return count;
}