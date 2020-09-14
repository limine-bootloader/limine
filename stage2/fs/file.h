#ifndef __FS__FILE_H__
#define __FS__FILE_H__

#include <stdint.h>

struct file_handle {
    int        disk;
    int        partition;
    void      *fd;
    int      (*read)(void *fd, void *buf, uint64_t loc, uint64_t count);
    uint64_t   size;
};

int fopen(struct file_handle *ret, int disk, int partition, const char *filename);
int fread(struct file_handle *fd, void *buf, uint64_t loc, uint64_t count);

#endif
