#ifndef MOUSE_H
#define MOUSE_H

#include <linux/uaccess.h>
#include <linux/cdev.h>

int setup_mouse(dev_t dev_nr);
void cleanup_mouse(void);

int add_mouse_device(struct class* dev_class, dev_t dev_nr);
void remove_mouse_device(struct class* dev_class, dev_t dev_nr);

#endif