#include "keyboard.h"
#include "../usb.h"

#define MAX_ACCEPTED_WRITE_SIZE 32

/*
    Forward declarations for private functions for this keyboard.c file
*/
static ssize_t keyboard_write(struct file* File, const char* user_buffer, size_t count, loff_t* offs);
static int driver_open(struct inode* device_file, struct file* instance);
static int driver_close(struct inode* device_file, struct file* instance);

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = driver_open,
    .release = driver_close,
    .write = keyboard_write
};

static dev_t keyboard_device_nr;
static struct cdev keyboard_device;
static struct class* keyboard_device_class;

static unsigned int file_is_open[NUM_POSSIBLE_ACCESSORY_MODE_DEVICES];
static char buffer[NUM_POSSIBLE_ACCESSORY_MODE_DEVICES][MAX_ACCEPTED_WRITE_SIZE];
static char* keyboard_hid_events[NUM_POSSIBLE_ACCESSORY_MODE_DEVICES];

int setup_keyboard(void){
    for(int i=0; i<NUM_POSSIBLE_ACCESSORY_MODE_DEVICES; i++){
        keyboard_hid_events[i] = NULL;
        file_is_open[i] = 0;
    }

    for(int i=0; i<NUM_POSSIBLE_ACCESSORY_MODE_DEVICES; i++){
        keyboard_hid_events[i] = kmalloc(3, GFP_KERNEL);
        if(!keyboard_hid_events[i]){
            goto setup_keyboard_error0;
        }
    }

    if(alloc_chrdev_region(&keyboard_device_nr, 0, NUM_POSSIBLE_ACCESSORY_MODE_DEVICES, "android_keyboards") < 0){
		printk("aoa_hid_driver - keyboard_device_nr could not be allocated\n");
		goto setup_keyboard_error0;
	}

    if(!(keyboard_device_class = class_create("android_keyboard"))){
        printk("aoa_hid_driver - Error creating class for android keyboard");
        goto setup_keyboard_error1;
    }

    cdev_init(&keyboard_device, &fops);
    if(cdev_add(&keyboard_device, keyboard_device_nr, NUM_POSSIBLE_ACCESSORY_MODE_DEVICES)){
        printk("aoa_hid_driver - Error adding keyboard device\n");
        goto setup_keyboard_error2;
    }

    return 0;

setup_keyboard_error2:
    class_destroy(keyboard_device_class);

setup_keyboard_error1:
    unregister_chrdev_region(keyboard_device_nr, NUM_POSSIBLE_ACCESSORY_MODE_DEVICES);

setup_keyboard_error0:
    for(int i=0; i<NUM_POSSIBLE_ACCESSORY_MODE_DEVICES; i++){
        if(keyboard_hid_events[i]){
            kfree(keyboard_hid_events[i]);
        }
    }

    return -1;
}

void cleanup_keyboard(void){
    cdev_del(&keyboard_device);
    class_destroy(keyboard_device_class);
    unregister_chrdev_region(keyboard_device_nr, NUM_POSSIBLE_ACCESSORY_MODE_DEVICES);
    for(int i=0; i<NUM_POSSIBLE_ACCESSORY_MODE_DEVICES; i++){
        kfree(keyboard_hid_events[i]);
    }
}

int add_keyboard_device(int minor){
    if(device_create(keyboard_device_class, NULL, keyboard_device_nr + minor, NULL, "android_keyboard%d", minor)==NULL){
		printk("aoa_hid_driver - Can not create device file for minor %d\n", minor);
		goto add_keyboard_device_error0;
	}

    return 0;

add_keyboard_device_error0:
    return -1;
}

void remove_keyboard_device(int minor){
    device_destroy(keyboard_device_class, keyboard_device_nr + minor);
}

static ssize_t keyboard_write(struct file* File, const char* user_buffer, size_t count, loff_t* offs){
    if(count > MAX_ACCEPTED_WRITE_SIZE){
        printk("aoa_hid_driver - Error writing to keyboard device, a single write to the keyboard can handle at most %d characters but attempted to write %d characters instead\n", MAX_ACCEPTED_WRITE_SIZE, (int)count);
        return -EINVAL;
    }

    int minor = iminor(file_inode(File));
    int not_copied = copy_from_user(buffer[minor], user_buffer, count);

    for(int i=0; i<count; i++){
        unsigned char keyindex1 = 0;
        unsigned char keyindex2 = 0;
        // https://usb.org/sites/default/files/hut1_21.pdf page 82-83
        switch(buffer[minor][i]){
            case 'a' ... 'z':
                keyindex1 = 0x00;
                keyindex2 = (buffer[minor][i] - 'a') + 0x04;
                break;
            case 'A' ... 'Z':
                keyindex1 = 0x02;
                keyindex2 = (buffer[minor][i] - 'A') + 0x04;
                break;
            case '0':
                keyindex1 = 0x00;
                keyindex2 = 0x27;
                break;
            case '1' ... '9':
                keyindex1 = 0x00;
                keyindex2 = (buffer[minor][i] - '1') + 0x1E;
                break;
            default:
                break;
        }

        if(keyindex1 != 0 || keyindex2 != 0){
            keyboard_hid_events[minor][0] = 0x01;
            keyboard_hid_events[minor][1] = keyindex1;
            keyboard_hid_events[minor][2] = keyindex2;
            usb_control_msg(get_usb_device(minor), usb_sndctrlpipe(get_usb_device(minor), 0), ACCESSORY_SEND_HID_EVENT, USB_DIR_OUT | USB_TYPE_VENDOR, 1, 0, keyboard_hid_events[minor], 3, 1000);
            msleep(100);
            keyboard_hid_events[minor][0] = 0x01;
            keyboard_hid_events[minor][1] = 0x00;
            keyboard_hid_events[minor][2] = 0x00;
            usb_control_msg(get_usb_device(minor), usb_sndctrlpipe(get_usb_device(minor), 0), ACCESSORY_SEND_HID_EVENT, USB_DIR_OUT | USB_TYPE_VENDOR, 1, 0, keyboard_hid_events[minor], 3, 1000);
            msleep(100);
        }
    }

    int delta = count - not_copied;
    return delta;
}

static int driver_open(struct inode* device_file, struct file* instance){
    int minor = iminor(device_file);

    if(atomic_cmpxchg((atomic_t*)&file_is_open[minor], 0, 1)){
        printk("aoa_hid_driver - Error opening keyboard device, device is already open\n");
        return -EBUSY;
    }

    return 0;
}

static int driver_close(struct inode* device_file, struct file* instance){
    int minor = iminor(device_file);

    if(!atomic_cmpxchg((atomic_t*)&file_is_open[minor], 1, 0)){
        printk("aoa_hid_driver - Error closing keyboard device, device is already closed\n");
        return -EBUSY;
    }
    
    return 0;
}