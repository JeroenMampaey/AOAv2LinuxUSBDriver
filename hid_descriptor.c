#include "hid_descriptor.h"
#include <linux/slab.h>

static char hid_descriptor[] = {
    0x05, 0x01,                     // Usage Page (Generic Desktop Ctrls)
    0x09, 0x06,                     // Usage (Keyboard)
    0xA1, 0x01,                     // Collection (Application)
    0x85, 0x01,                     //   Report ID (1)
    0x05, 0x07,                     //   Usage Page (Kbrd/Keypad)
    0x75, 0x01,                     //   Report Size (1)
    0x95, 0x08,                     //   Report Count (8)
    0x19, 0xE0,                     //   Usage Minimum (0xE0)
    0x29, 0xE7,                     //   Usage Maximum (0xE7)
    0x15, 0x00,                     //   Logical Minimum (0)
    0x25, 0x01,                     //   Logical Maximum (1)
    0x81, 0x02,                     //   Input (Data,Var,Absolute)
    0x95, 0x01,                     //   Report Count (1)
    0x75, 0x08,                     //   Report Size (8)
    0x15, 0x00,                     //   Logical Minimum (0)
    0x25, 0x64,                     //   Logical Maximum (100)
    0x05, 0x07,                     //   Usage Page (Kbrd/Keypad)
    0x19, 0x00,                     //   Usage Minimum (0x00)
    0x29, 0x65,                     //   Usage Maximum (0x65)
    0x81, 0x00,                     //   Input (Data,Array,Absolute)
    0xC0,                           // End Collection
};

static char* dynamically_allocated_hid_descriptor = NULL;

int setup_hid_descriptor(void){
    dynamically_allocated_hid_descriptor = kmalloc(sizeof(hid_descriptor), GFP_KERNEL);
    if(!dynamically_allocated_hid_descriptor){
        goto setup_hid_descriptor_error0;
    }

    memcpy(dynamically_allocated_hid_descriptor, hid_descriptor, sizeof(hid_descriptor));

    return 0;

setup_hid_descriptor_error0:
    return -1;
}

void cleanup_hid_descriptor(void){
    kfree(dynamically_allocated_hid_descriptor);
}

char* get_hid_descriptor(void){
    return dynamically_allocated_hid_descriptor;
}

u16 get_hid_descriptor_size(void){
    return (u16)sizeof(hid_descriptor);
}