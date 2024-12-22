#include "usb.h"
#include "sys_files.h"

#include <linux/usb.h>
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

static struct usb_device_id any_usb_device_table[] = {
     {.driver_info = 42},
     {}
};
MODULE_DEVICE_TABLE(usb, any_usb_device_table);

static struct usb_driver android_driver = {
    .name = "android_default_usb",
    .probe = android_default_probe,
    .disconnect = android_default_disconnect,
    .id_table = any_usb_device_table,
};

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

    if(usb_register(&android_driver)){
        printk("aoa_hid_driver - Error registering USB driver\n");
        goto setup_usb_error1;
    }

    return 0;

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
    usb_deregister(&android_driver);
    kfree(manufacturer);
    kfree(model);
    kfree(description);
    kfree(version);
}

static int android_default_probe(struct usb_interface* interface, const struct usb_device_id* id){
    int interface_number = interface->cur_altsetting->desc.bInterfaceNumber;
    if(interface_number != 0){
        return 0;
    }

    struct usb_device* usb_dev = interface_to_usbdev(interface);
    u16 vendor_id = usb_dev->descriptor.idVendor;
    u16 product_id = usb_dev->descriptor.idProduct;
    
    if(!is_android_device(vendor_id, product_id)){
        return 0;
    }

    printk("aoa_hid_driver - Android device found, attempting to configure for AOAv2\n");

    u16 protocol = 0;
    if(usb_control_msg_recv(usb_dev, usb_rcvctrlpipe(usb_dev, 0), ACCESSORY_GET_PROTOCOL, USB_DIR_IN | USB_TYPE_VENDOR, 0, 0, &protocol, sizeof(protocol), 1000, GFP_KERNEL)){
        printk("aoa_hid_driver - Error getting protocol from android device\n");
        goto usb_default_probe_error0;
    }

    if(protocol != 2){
        printk("aoa_hid_driver - Android device found but does not support AOAv2, get protocol returned %d\n", protocol);
        return 0;
    }

    int num_bytes_send = usb_control_msg(usb_dev, usb_sndctrlpipe(usb_dev, 0), ACCESSORY_SEND_STRING, USB_DIR_OUT | USB_TYPE_VENDOR, 0, 0, manufacturer, strlen(manufacturer)+1, 1000);
    if(num_bytes_send != strlen(manufacturer)+1){
        printk("aoa_hid_driver - Error sending manufacturer string to android device, usb_control_msg returned %d instead of %d\n", num_bytes_send, (int)strlen(manufacturer)+1);
        goto usb_default_probe_error0;
    }

    num_bytes_send = usb_control_msg(usb_dev, usb_sndctrlpipe(usb_dev, 0), ACCESSORY_SEND_STRING, USB_DIR_OUT | USB_TYPE_VENDOR, 0, 1, model, strlen(model)+1, 1000);
    if(num_bytes_send != strlen(model)+1){
        printk("aoa_hid_driver - Error sending model string to android device, usb_control_msg returned %d instead of %d\n", num_bytes_send, (int)strlen(model)+1);
        goto usb_default_probe_error0;
    }

    num_bytes_send = usb_control_msg(usb_dev, usb_sndctrlpipe(usb_dev, 0), ACCESSORY_SEND_STRING, USB_DIR_OUT | USB_TYPE_VENDOR, 0, 2, description, strlen(description)+1, 1000);
    if(num_bytes_send != strlen(description)+1){
        printk("aoa_hid_driver - Error sending description string to android device, usb_control_msg returned %d instead of %d\n", num_bytes_send, (int)strlen(description)+1);
        goto usb_default_probe_error0;
    }

    num_bytes_send = usb_control_msg(usb_dev, usb_sndctrlpipe(usb_dev, 0), ACCESSORY_SEND_STRING, USB_DIR_OUT | USB_TYPE_VENDOR, 0, 3, version, (int)strlen(version)+1, 1000);
    if(num_bytes_send != strlen(version)+1){
        printk("aoa_hid_driver - Error sending version string to android device, usb_control_msg returned %d instead of %d\n", num_bytes_send, (int)strlen(version)+1);
        goto usb_default_probe_error0;
    }

    num_bytes_send = usb_control_msg(usb_dev, usb_sndctrlpipe(usb_dev, 0), ACCESSORY_START, USB_DIR_OUT | USB_TYPE_VENDOR, 0, 0, NULL, 0, 1000);
    if(num_bytes_send != 0){
        printk("aoa_hid_driver - Error starting accessory mode on android device, usb_control_msg returned %d instead of 0\n", num_bytes_send);
        goto usb_default_probe_error0;
    }

    printk("aoa_hid_driver - Android device found and configured for AOAv2\n");

    return 0;

usb_default_probe_error0:
    return -1;
}

static void android_default_disconnect(struct usb_interface* interface){
    // TODO
}