#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <linux/uaccess.h>
#include <linux/cdev.h>

#define major(x)        ((int32_t)(((u_int32_t)(x) >> 24) & 0xff))
#define minor(x)        ((int32_t)((x) & 0xffffff))

int setup_keyboard(dev_t dev_nr);
void cleanup_keyboard(void);

int add_keyboard_device(struct class* dev_class, dev_t dev_nr);
void remove_keyboard_device(struct class* dev_class, dev_t dev_nr);

#endif