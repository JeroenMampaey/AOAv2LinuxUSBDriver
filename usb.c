#include "usb.h"
#include "sys_files.h"
#include "devices/keyboard.h"
#include "devices/mouse.h"
#include "devices/volume.h"
#include "devices/brightness.h"
#include "hid_descriptor.h"

#include <linux/device.h>
#include <linux/slab.h>

#define MANUFACTURER_STRING "Not a Real Manufacturer"
#define MODEL_STRING "Not a Real Model"
#define DESCRIPTION_STRING "Connection for using HID over the AOAv2 protocol"
#define VERSION_STRING "1.0"

static char* manufacturer = NULL;
static char* model = NULL;
static char* description = NULL;
static char* version = NULL;

/*
    Forward declarations for private functions for this usb.c file
*/
static int android_default_probe(struct usb_interface* interface, const struct usb_device_id* id);
static void android_default_disconnect(struct usb_interface* interface);
static int android_accessory_mode_probe(struct usb_interface* interface, const struct usb_device_id* id);
static void android_accessory_mode_disconnect(struct usb_interface* interface);

static struct usb_device_id any_usb_device_table[] = {
     {.driver_info = 42},
     {}
};
MODULE_DEVICE_TABLE(usb, any_usb_device_table);

// https://source.android.com/docs/core/interaction/accessories/aoa2
static struct usb_device_id accessory_mode_android_device_table[] = {
    {USB_DEVICE(0x18D1, 0x2D00)},
    {USB_DEVICE(0x18D1, 0x2D01)},
    {USB_DEVICE(0x18D1, 0x2D02)},
    {USB_DEVICE(0x18D1, 0x2D03)},
    {USB_DEVICE(0x18D1, 0x2D04)},
    {USB_DEVICE(0x18D1, 0x2D05)},
    {}
};
MODULE_DEVICE_TABLE(usb, accessory_mode_android_device_table);

static struct usb_driver android_default_driver = {
    .name = "android_default_usb",
    .probe = android_default_probe,
    .disconnect = android_default_disconnect,
    .id_table = any_usb_device_table,
};

static struct usb_driver android_accessory_mode_driver = {
    .name = "android_accessory_mode_usb",
    .probe = android_accessory_mode_probe,
    .disconnect = android_accessory_mode_disconnect,
    .id_table = accessory_mode_android_device_table,
};

static struct usb_device* accessory_mode_devices[NUM_POSSIBLE_ACCESSORY_MODE_DEVICES];

int setup_usb(void){
    manufacturer = kmalloc(strlen(MANUFACTURER_STRING)+1, GFP_KERNEL);
    if(!manufacturer){
        printk("aoa_hid_driver - Error allocating memory for manufacturer string\n");
        goto setup_usb_error0;
    }
    strcpy(manufacturer, MANUFACTURER_STRING);

    model = kmalloc(strlen(MODEL_STRING)+1, GFP_KERNEL);
    if(!model){
        printk("aoa_hid_driver - Error allocating memory for model string\n");
        goto setup_usb_error1;
    }
    strcpy(model, MODEL_STRING);

    description = kmalloc(strlen(DESCRIPTION_STRING)+1, GFP_KERNEL);
    if(!description){
        printk("aoa_hid_driver - Error allocating memory for description string\n");
        goto setup_usb_error1;
    }
    strcpy(description, DESCRIPTION_STRING);

    version = kmalloc(strlen(VERSION_STRING)+1, GFP_KERNEL);
    if(!version){
        printk("aoa_hid_driver - Error allocating memory for version string\n");
        goto setup_usb_error1;
    }
    strcpy(version, VERSION_STRING);

    for(int i=0; i<NUM_POSSIBLE_ACCESSORY_MODE_DEVICES; i++){
        accessory_mode_devices[i] = NULL;
    }

    if(setup_hid_descriptor()){
        printk("aoa_hid_driver - Error setting up HID descriptor\n");
        goto setup_usb_error1;
    }

    if(usb_register(&android_default_driver)){
        printk("aoa_hid_driver - Error registering USB driver\n");
        goto setup_usb_error2;
    }

    if(setup_keyboard()){
        printk("aoa_hid_driver - Error setting up keyboard\n");
        goto setup_usb_error5;
    }

    if(setup_mouse()){
        printk("aoa_hid_driver - Error setting up mouse\n");
        goto setup_usb_error6;
    }

    if(setup_volume()){
        printk("aoa_hid_driver - Error setting up volume\n");
        goto setup_usb_error7;
    }

    if(setup_brightness()){
        printk("aoa_hid_driver - Error setting up brightness\n");
        goto setup_usb_error8;
    }

    if(usb_register(&android_accessory_mode_driver)){
        printk("aoa_hid_driver - Error registering USB driver\n");
        goto setup_usb_error9;
    }

    return 0;

setup_usb_error9:
    cleanup_brightness();

setup_usb_error8:
    cleanup_volume();

setup_usb_error7:
    cleanup_mouse();

setup_usb_error6:
    cleanup_keyboard();

setup_usb_error5:
    usb_deregister(&android_default_driver);

setup_usb_error2:
    cleanup_hid_descriptor();

setup_usb_error1:
    if(manufacturer){
        kfree(manufacturer);
    }
    if(model){
        kfree(model);
    }
    if(description){
        kfree(description);
    }
    if(version){
        kfree(version);
    }

setup_usb_error0:
    return -1;
}

void cleanup_usb(void){
    usb_deregister(&android_accessory_mode_driver);
    for(int i=0; i<NUM_POSSIBLE_ACCESSORY_MODE_DEVICES; i++){
        if(accessory_mode_devices[i]){
            remove_keyboard_device(i);
            remove_mouse_device(i);
            remove_volume_device(i);
            remove_brightness_device(i);
            accessory_mode_devices[i] = NULL;
        }
    }
    cleanup_brightness();
    cleanup_volume();
    cleanup_mouse();
    cleanup_keyboard();
    usb_deregister(&android_default_driver);
    cleanup_hid_descriptor();
    kfree(manufacturer);
    kfree(model);
    kfree(description);
    kfree(version);
}

static int android_default_probe(struct usb_interface* interface, const struct usb_device_id* id){
    int interface_number = interface->cur_altsetting->desc.bInterfaceNumber;
    if(interface_number != 0){
        goto android_default_probe_error0;
    }

    struct usb_device* usb_dev = interface_to_usbdev(interface);
    u16 vendor_id = usb_dev->descriptor.idVendor;
    u16 product_id = usb_dev->descriptor.idProduct;

    if(!is_android_device(vendor_id, product_id)){
        goto android_default_probe_error0;
    }

    u16 protocol = 0;
    if(usb_control_msg_recv(usb_dev, usb_rcvctrlpipe(usb_dev, 0), ACCESSORY_GET_PROTOCOL, USB_DIR_IN | USB_TYPE_VENDOR, 0, 0, &protocol, sizeof(protocol), 1000, GFP_KERNEL)){
        printk("aoa_hid_driver - Error getting protocol from android device\n");
        goto android_default_probe_error0;
    }

    if(protocol != 2){
        printk("aoa_hid_driver - Android device found but does not support AOAv2, get protocol returned %d\n", protocol);
        goto android_default_probe_error0;
    }

    int num_bytes_send = usb_control_msg(usb_dev, usb_sndctrlpipe(usb_dev, 0), ACCESSORY_SEND_STRING, USB_DIR_OUT | USB_TYPE_VENDOR, 0, 0, manufacturer, strlen(manufacturer)+1, 1000);
    if(num_bytes_send != strlen(manufacturer)+1){
        printk("aoa_hid_driver - Error sending manufacturer string to android device, usb_control_msg returned %d instead of %d\n", num_bytes_send, (int)strlen(manufacturer)+1);
        goto android_default_probe_error0;
    }

    num_bytes_send = usb_control_msg(usb_dev, usb_sndctrlpipe(usb_dev, 0), ACCESSORY_SEND_STRING, USB_DIR_OUT | USB_TYPE_VENDOR, 0, 1, model, strlen(model)+1, 1000);
    if(num_bytes_send != strlen(model)+1){
        printk("aoa_hid_driver - Error sending model string to android device, usb_control_msg returned %d instead of %d\n", num_bytes_send, (int)strlen(model)+1);
        goto android_default_probe_error0;
    }

    num_bytes_send = usb_control_msg(usb_dev, usb_sndctrlpipe(usb_dev, 0), ACCESSORY_SEND_STRING, USB_DIR_OUT | USB_TYPE_VENDOR, 0, 2, description, strlen(description)+1, 1000);
    if(num_bytes_send != strlen(description)+1){
        printk("aoa_hid_driver - Error sending description string to android device, usb_control_msg returned %d instead of %d\n", num_bytes_send, (int)strlen(description)+1);
        goto android_default_probe_error0;
    }

    num_bytes_send = usb_control_msg(usb_dev, usb_sndctrlpipe(usb_dev, 0), ACCESSORY_SEND_STRING, USB_DIR_OUT | USB_TYPE_VENDOR, 0, 3, version, (int)strlen(version)+1, 1000);
    if(num_bytes_send != strlen(version)+1){
        printk("aoa_hid_driver - Error sending version string to android device, usb_control_msg returned %d instead of %d\n", num_bytes_send, (int)strlen(version)+1);
        goto android_default_probe_error0;
    }

    num_bytes_send = usb_control_msg(usb_dev, usb_sndctrlpipe(usb_dev, 0), ACCESSORY_START, USB_DIR_OUT | USB_TYPE_VENDOR, 0, 0, NULL, 0, 1000);
    if(num_bytes_send != 0){
        printk("aoa_hid_driver - Error starting accessory mode on android device, usb_control_msg returned %d instead of 0\n", num_bytes_send);
        goto android_default_probe_error0;
    }

    return 0;

android_default_probe_error0:
    return -ENODEV;
}

static void android_default_disconnect(struct usb_interface* interface){
    // Should not do anything
}

static int android_accessory_mode_probe(struct usb_interface* interface, const struct usb_device_id* id){
    int interface_number = interface->cur_altsetting->desc.bInterfaceNumber;
    if(interface_number != 0){
        goto android_accessory_mode_probe_error0;
    }

    struct usb_device* usb_dev = interface_to_usbdev(interface);

    unsigned int candidate_index = 0;
    bool found_candidate = false;
    bool found_duplicate = false;
    for(int i=NUM_POSSIBLE_ACCESSORY_MODE_DEVICES-1; i>=0; i--){
        if(accessory_mode_devices[i] == usb_dev){
            found_duplicate = true;
            break;
        }

        if(!accessory_mode_devices[i]){
            found_candidate = true;
            candidate_index = i;
        }
    }

    if(found_duplicate){
        return 0;
    }

    if(!found_candidate){
        printk("aoa_hid_driver - No more space for accessory mode devices\n");
        goto android_accessory_mode_probe_error0;
    }

    int num_bytes_send = usb_control_msg(usb_dev, usb_sndctrlpipe(usb_dev, 0), ACCESSORY_REGISTER_HID, USB_DIR_OUT | USB_TYPE_VENDOR, 1, get_hid_descriptor_size(), NULL, 0, 1000);
    if(num_bytes_send != 0){
        printk("aoa_hid_driver - Error registering HID descriptor with android device, usb_control_msg returned %d instead of 0\n", num_bytes_send);
        goto android_accessory_mode_probe_error0;
    }

    num_bytes_send = usb_control_msg(usb_dev, usb_sndctrlpipe(usb_dev, 0), ACCESSORY_SET_HID_REPORT_DESC, USB_DIR_OUT | USB_TYPE_VENDOR, 1, 0, get_hid_descriptor(), get_hid_descriptor_size(), 1000);
    if(num_bytes_send != get_hid_descriptor_size()){
        printk("aoa_hid_driver - Error setting HID report descriptor with android device, usb_control_msg returned %d instead of %d\n", num_bytes_send, (int)get_hid_descriptor_size());
        goto android_accessory_mode_probe_error0;
    }

    accessory_mode_devices[candidate_index] = usb_dev;

    if(add_keyboard_device(candidate_index)){
        printk("aoa_hid_driver - Error adding keyboard device\n");
        goto android_accessory_mode_probe_error1;
    }

    if(add_mouse_device(candidate_index)){
        printk("aoa_hid_driver - Error adding mouse device\n");
        goto android_accessory_mode_probe_error2;
    }

    if(add_volume_device(candidate_index)){
        printk("aoa_hid_driver - Error adding volume device\n");
        goto android_accessory_mode_probe_error3;
    }

    if(add_brightness_device(candidate_index)){
        printk("aoa_hid_driver - Error adding brightness device\n");
        goto android_accessory_mode_probe_error4;
    }

    return 0;

android_accessory_mode_probe_error4:
    remove_volume_device(candidate_index);

android_accessory_mode_probe_error3:
    remove_mouse_device(candidate_index);

android_accessory_mode_probe_error2:
    remove_keyboard_device(candidate_index);

android_accessory_mode_probe_error1:
    accessory_mode_devices[candidate_index] = NULL;

android_accessory_mode_probe_error0:
    return -ENODEV;
}

static void android_accessory_mode_disconnect(struct usb_interface* interface){
    int interface_number = interface->cur_altsetting->desc.bInterfaceNumber;
    if(interface_number != 0){
        return;
    }

    struct usb_device* usb_dev = interface_to_usbdev(interface);

    for(int i=0; i<NUM_POSSIBLE_ACCESSORY_MODE_DEVICES; i++){
        if(accessory_mode_devices[i] == usb_dev){
            remove_keyboard_device(i);
            remove_mouse_device(i);
            remove_volume_device(i);
            remove_brightness_device(i);
            accessory_mode_devices[i] = NULL;
            return;
        }
    }
}

struct usb_device* get_usb_device(int minor){
    if(minor < 0 || minor >= NUM_POSSIBLE_ACCESSORY_MODE_DEVICES){
        return NULL;
    }

    return accessory_mode_devices[minor];
}