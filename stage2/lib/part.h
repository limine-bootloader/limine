#ifndef __LIB__PART_H__
#define __LIB__PART_H__

#include <stdint.h>
#include <stdbool.h>
#include <lib/guid.h>

struct part {
    int drive;
    int partition;
    uint64_t first_sect;
    uint64_t sect_count;
    bool guid_valid;
    struct guid guid;
};

void part_create_index(void);

int part_get(struct part *part, int drive, int partition);
bool part_get_by_guid(struct part *part, struct guid *guid);

int part_read(struct part *part, void *buffer, uint64_t loc, uint64_t count);

#endif
