#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <linux/uaccess.h>
#include <linux/cdev.h>

int setup_keyboard(dev_t dev_nr);
void cleanup_keyboard(void);

int add_keyboard_device(struct class* dev_class, dev_t dev_nr);
void remove_keyboard_device(struct class* dev_class, dev_t dev_nr);

#endif