#ifndef __LIB__PART_H__
#define __LIB__PART_H__

#include <stdint.h>
#include <stdbool.h>
#include <lib/guid.h>

#define NO_PARTITION  (-1)
#define INVALID_TABLE (-2)
#define END_OF_TABLE  (-3)

struct volume {
    int drive;
    int partition;
    int sector_size;
    int cache_status;
    uint8_t *cache;
    uint64_t cached_block;
    uint64_t first_sect;
    uint64_t sect_count;
    bool guid_valid;
    struct guid guid;
    bool part_guid_valid;
    struct guid part_guid;
};

void volume_create_index(void);

int part_get(struct volume *part, struct volume *volume, int partition);
bool volume_get_by_guid(struct volume *part, struct guid *guid);
bool volume_get_by_coord(struct volume *part, int drive, int partition);

bool volume_read(struct volume *part, void *buffer, uint64_t loc, uint64_t count);

#endif
