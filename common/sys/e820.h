#ifndef SYS__E820_H__
#define SYS__E820_H__

#include <stdint.h>
#include <stddef.h>
#include <mm/pmm.h>

#define MAX_E820_ENTRIES 256

extern struct memmap_entry e820_map[];
extern size_t e820_entries;

void init_e820(void);

#endif
