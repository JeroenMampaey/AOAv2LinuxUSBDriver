#ifndef HID_DESCRIPTOR_H
#define HID_DESCRIPTOR_H

#include <linux/kernel.h>

int setup_hid_descriptor(void);
void cleanup_hid_descriptor(void);

char* get_hid_descriptor(void);
u16 get_hid_descriptor_size(void);

#endif