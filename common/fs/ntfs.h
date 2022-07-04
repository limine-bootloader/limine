#ifndef __FS__NTFS_H__
#define __FS__NTFS_H__

#include <lib/part.h>
#include <fs/file.h>

struct file_handle *ntfs_open(struct volume *part, const char *path);

#endif
