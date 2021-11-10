#ifndef __PROTOS__BOOTBOOT__INITRD_H__
#define __PROTOS__BOOTBOOT__INITRD_H__

#include <stdint.h>
#include <stdbool.h>

struct initrd_file {
    uint64_t size;
    uint8_t *data;
};

enum initrd_format {
    INITRD_FORMAT_UNKNOWN,
    INITRD_FORMAT_JAMESM,
    INITRD_FORMAT_CPIO,
    INITRD_FORMAT_USTAR
};

struct initrd_file bruteforce_kernel(struct initrd_file file);
enum initrd_format initrd_format(struct initrd_file file);

#define INITRD_HANDLER(name) struct initrd_file initrd_open_##name(struct initrd_file file, const char *path)

INITRD_HANDLER(auto);

#endif
