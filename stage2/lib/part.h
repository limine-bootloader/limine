#ifndef __LIB__PART_H__
#define __LIB__PART_H__

#include <stdint.h>
#include <stdbool.h>
#include <lib/blib.h>

struct part {
    int drive;
    int partition;
    uint64_t first_sect;
    uint64_t sect_count;
    bool guid_valid;
    struct guid guid;
};

int get_part(struct part *part, int drive, int partition);
bool part_get_by_guid(int *drive, int *part, struct guid *guid);

void part_create_index(void);

#endif
