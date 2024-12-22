# About

A simple Linux USB driver for controlling Android devices using the [HID](https://en.wikipedia.org/wiki/Human_interface_device) support in the [Android Open Accessory (AOA) 2.0 protocol](https://source.android.com/docs/core/interaction/accessories/aoa2). Unlike [ADB](https://developer.android.com/tools/adb), this approach does not require USB debugging to be enabled on the device.

Basically this driver will put the Android device in [accessory mode](https://developer.android.com/develop/connectivity/usb) and then leverages AOAv2 to send HID events to the Android device. This driver only leverages only some simple HID functionality: keyboard, mouse, volume, brightness.

# How to use

First figure out Vendor ID and Product ID of the Android devices. A simple way to achieve this is to connect the device and use `lsusb`, which will show for example an entry like:

```
Bus 003 Device 005: ID 04e8:6860 Samsung Electronics Co., Ltd Galaxy series, misc. (MTP mode)
```

Here the Vendor ID is `04e8` and the Product ID is `6860`. Then, to install this USB driver, run:

```
make install
```

This will create a file `/sys/kernel/android_usb/add_known_device`. In this file, add all Vendor IDs and Product IDs discovered earlier as follows:

```
echo 04e8 6860 > /sys/kernel/android_usb/add_known_device
```

Then connect (reconnect) the Android device. This will eventually create 4 device files, which can be utilized to control the Android phone.

```
/dev/android_keyboard0
/dev/android_mouse0
/dev/android_volume0
/dev/android_brightness0
```

To remove the USB driver, run:

```
make uninstall
```

Any errors with the driver will be reported in the kernel log which can be examined using `dmesg`.

# Keyboard

To steer the keyboard, write characters to the `/dev/android_keyboard_` file, at most 32 characters can be written at once. For example:

```
echo -n "abdeAR10" > /dev/android_keyboard0
```

# Mouse

For mouse commands, write 4 bytes to the `/dev/android_mouse_` file.
- Byte 0: A signed character between -127 (0x81) and 127 (0x7F), which will move the mouse from left to right
- Byte 1: A signed character between -127 (0x81) and 127 (0x7F), which will move the mouse from up to down
- Byte 2: A signed character between -127 (0x81) and 127 (0x7F), which will scroll up 
- Byte 3: Either 0 or 1, if 1 then the mouse will click at the current position

For example, to move the mouse to the left:
```
echo -n -e '\x90\x00\x00\x00' > /dev/android_mouse0
```

For example, to move the mouse down:
```
echo -n -e '\x00\x50\x00\x00' > /dev/android_mouse0
```

For example, to scroll upwards:
```
echo -n -e '\x00\x00\x01\x00' > /dev/android_mouse0
```

For example, to click at the current position:
```
echo -n -e '\x00\x00\x00\x01' > /dev/android_mouse0
```