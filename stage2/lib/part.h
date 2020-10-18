#ifndef __LIB__PART_H__
#define __LIB__PART_H__

#include <stdint.h>
#include <stdbool.h>
#include <lib/blib.h>

struct part {
    uint64_t first_sect;
    uint64_t sect_count;
    struct guid guid;
};

int get_part(struct part *part, int drive, int partition);

void part_create_index(void);

#endif
