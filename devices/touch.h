#ifndef TOUCH_H
#define TOUCH_H

#include <linux/uaccess.h>
#include <linux/cdev.h>

int setup_touch(void);
void cleanup_touch(void);

int add_touch_device(int minor);
void remove_touch_device(int minor);

#endif