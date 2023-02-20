#ifndef __FS__EXT2_H__
#define __FS__EXT2_H__

#include <stdbool.h>
#include <lib/part.h>
#include <fs/file.h>

bool ext2_get_guid(struct guid *guid, struct volume *part);
char *ext2_get_label(struct volume *part);

struct file_handle *ext2_open(struct volume *part, const char *path);

#endif
