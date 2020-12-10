#ifndef __LIB__PART_H__
#define __LIB__PART_H__

#include <stdint.h>
#include <stdbool.h>
#include <lib/guid.h>

#define NO_PARTITION  (-1)
#define INVALID_TABLE (-2)
#define END_OF_TABLE  (-3)

struct part {
    int drive;
    int partition;
    int sector_size;
    uint64_t first_sect;
    uint64_t sect_count;
    bool guid_valid;
    struct guid guid;
    bool part_guid_valid;
    struct guid part_guid;
};

void part_create_index(void);

int part_get(struct part *part, int drive, int partition);
bool part_get_by_guid(struct part *part, struct guid *guid);

int part_read(struct part *part, void *buffer, uint64_t loc, uint64_t count);

#endif
