#ifndef SYS_FILES_H
#define SYS_FILES_H

#include <linux/kernel.h>

bool is_android_device(u16 id_vendor, u16 id_product);

int setup_sysfs(void);
void cleanup_sysfs(void);

#endif