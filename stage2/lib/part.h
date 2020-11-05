#ifndef __LIB__PART_H__
#define __LIB__PART_H__

#include <stdint.h>

struct part {
	uint64_t first_sect;
	uint64_t sect_count;
    int sector_size;
};

int get_part(struct part *part, int drive, int partition);

#endif
