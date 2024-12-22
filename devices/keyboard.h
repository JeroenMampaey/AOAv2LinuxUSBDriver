#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <linux/uaccess.h>
#include <linux/cdev.h>

int setup_keyboard(void);
void cleanup_keyboard(void);

int add_keyboard_device(int minor);
void remove_keyboard_device(int minor);

#endif