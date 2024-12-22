#ifndef BRIGHTNESS_H
#define BRIGHTNESS_H

#include <linux/uaccess.h>
#include <linux/cdev.h>

int setup_brightness(void);
void cleanup_brightness(void);

int add_brightness_device(int minor);
void remove_brightness_device(int minor);

#endif