#include <lib/mbr.h>
#include <drivers/disk.h>
#include <stddef.h>

struct mbr_entry {
	uint8_t status;
	uint8_t chs_first_sect[3];
	uint8_t type;
	uint8_t chs_last_sect[3];
	uint32_t first_sect;
	uint32_t sect_count;
} __attribute__((packed));

int mbr_get_part(struct mbr_part *part, int drive, int partition) {
    // Variables.
    struct mbr_entry entry;
    const size_t entry_address = 0x1be + sizeof(struct mbr_entry) * partition;
    
    // Read the entry of the MBR.
    int ret;
    if ((ret = read(drive, &entry, entry_address, sizeof(struct mbr_entry)))) {
        return ret;
    }

    // Check if the partition exists, fail if it doesnt.
    if (entry.type == 0) {
        return -1;
    }

    // Assign the final fields and return.
    part->first_sect = entry.first_sect;
    part->sect_count = entry.sect_count;
    return 0;
}
