#ifndef FS__ISO9660_H__
#define FS__ISO9660_H__

#include <stdint.h>
#include <lib/part.h>
#include <fs/file.h>

struct file_handle *iso9660_open(struct volume *vol, const char *path);

#endif
