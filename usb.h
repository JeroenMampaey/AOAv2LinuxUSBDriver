#ifndef USB_H
#define USB_H

#include <linux/usb.h>

#define NUM_POSSIBLE_ACCESSORY_MODE_DEVICES 64

// https://source.android.com/docs/core/interaction/accessories/aoa
// https://source.android.com/docs/core/interaction/accessories/aoa2
#define ACCESSORY_GET_PROTOCOL 51
#define ACCESSORY_SEND_STRING 52
#define ACCESSORY_START 53
#define ACCESSORY_REGISTER_HID 54
#define ACCESSORY_UNREGISTER_HID 55
#define ACCESSORY_SET_HID_REPORT_DESC 56
#define ACCESSORY_SEND_HID_EVENT 57

int setup_usb(void);
void cleanup_usb(void);

struct usb_device* get_usb_device(int minor);

#endif