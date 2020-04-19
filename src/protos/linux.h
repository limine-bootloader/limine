#ifndef __PROTOS__LINUX_H__
#define __PROTOS__LINUX_H__

#include <fs/file.h>

void linux_load(struct file_handle *fd, char *cmdline);

#endif
