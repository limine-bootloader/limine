#ifndef __LIB__E820_H__
#define __LIB__E820_H__

#include <stdint.h>

#define E820_MAX_ENTRIES 256

struct e820_entry_t {
    uint64_t base;
    uint64_t length;
    uint32_t type;
    uint32_t unused;
} __attribute__((packed));

extern struct e820_entry_t *e820_map;

int init_e820(void);

#endif
