#ifndef __SYS__E820_H__
#define __SYS__E820_H__

#include <stdint.h>
#include <stddef.h>

struct e820_entry_t {
    uint64_t base;
    uint64_t length;
    uint32_t type;
    uint32_t unused;
} __attribute__((packed));

extern struct e820_entry_t *e820_map;
extern size_t e820_entries;

void init_e820(void);

#endif
