#ifndef MOUSE_H
#define MOUSE_H

#include <linux/uaccess.h>
#include <linux/cdev.h>

int setup_mouse(void);
void cleanup_mouse(void);

int add_mouse_device(int minor);
void remove_mouse_device(int minor);

#endif