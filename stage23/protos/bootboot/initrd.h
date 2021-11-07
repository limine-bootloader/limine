#ifndef __PROTOS__BOOTBOOT_FS_H__
#define __PROTOS__BOOTBOOT_FS_H__

#include <stdint.h>
#include <stdbool.h>

typedef struct file {
    uint64_t size;
    uint8_t* data;
} file_t;

#define INITRD_HANDLER(name) file_t initrd_open_##name(file_t file, const char* path)

INITRD_HANDLER(auto);

uint32_t oct2bin(uint8_t* str, uint32_t max);
uint32_t hex2bin(uint8_t* str, uint32_t size);

#endif
