#ifndef __FS__ECHFS_H__
#define __FS__ECHFS_H__

#include <stdbool.h>
#include <lib/part.h>
#include <fs/file.h>

bool echfs_get_guid(struct guid *guid, struct volume *part);

struct file_handle *echfs_open(struct volume *part, const char *filename);

#endif
