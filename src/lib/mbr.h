#ifndef __MBR_H__
#define __MBR_H__

#include <stdint.h>

struct mbr_part {
	uint64_t first_sect;
	uint64_t sect_count;
};

int mbr_get_part(struct mbr_part *part, int drive, int partition);

#endif
