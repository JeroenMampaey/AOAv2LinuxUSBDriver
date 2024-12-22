#ifndef VOLUME_H
#define VOLUME_H

#include <linux/uaccess.h>
#include <linux/cdev.h>

int setup_volume(void);
void cleanup_volume(void);

int add_volume_device(int minor);
void remove_volume_device(int minor);

#endif