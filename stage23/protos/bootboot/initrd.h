#ifndef __PROTOS__BOOTBOOT__INITRD_H__
#define __PROTOS__BOOTBOOT__INITRD_H__

#include <stdint.h>

struct initrd_file {
    uint64_t size;
    uint8_t *data;
};

#define INITRD_HANDLER(name) struct initrd_file initrd_open_##name(struct initrd_file file, const char *path)

INITRD_HANDLER(auto);

#endif
