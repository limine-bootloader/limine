#include <stddef.h>
#include <stdint.h>
#include <lib/part.h>
#include <drivers/disk.h>
#include <lib/libc.h>
#include <lib/blib.h>

#define NO_PARTITION  (-1)
#define INVALID_TABLE (-2)

struct gpt_table_header {
    // the head
    char     signature[8];
    uint32_t revision;
    uint32_t header_size;
    uint32_t crc32;
    uint32_t _reserved0;

    // the partitioning info
    uint64_t my_lba;
    uint64_t alternate_lba;
    uint64_t first_usable_lba;
    uint64_t last_usable_lba;

    // the guid
    struct {
        uint64_t low;
        uint64_t high;
    } disk_guid;

    // entries related
    uint64_t partition_entry_lba;
    uint32_t number_of_partition_entries;
    uint32_t size_of_partition_entry;
    uint32_t partition_entry_array_crc32;
} __attribute__((packed));

struct gpt_entry {
    struct {
        uint64_t low;
        uint64_t high;
    } partition_type_guid;

    struct {
        uint64_t low;
        uint64_t high;
    } unique_partition_guid;

    uint64_t starting_lba;
    uint64_t ending_lba;

    uint64_t attributes;

    uint16_t partition_name[36];
} __attribute__((packed));

static int gpt_get_part(struct part *ret, int drive, int partition) {
    struct gpt_table_header header = {0};

    // read header, located after the first block
    read(drive, &header, 512, sizeof(header));

    // check the header
    // 'EFI PART'
    if (strncmp(header.signature, "EFI PART", 8)) return INVALID_TABLE;
    if (header.revision != 0x00010000) return NO_PARTITION;

    // parse the entries if reached here
    if ((uint32_t)partition >= header.number_of_partition_entries)
        return NO_PARTITION;

    struct gpt_entry entry = {0};
    read(drive, &entry,
         (header.partition_entry_lba * 512) + (partition * sizeof(entry)),
         sizeof(entry));

    if (entry.unique_partition_guid.low  == 0 &&
        entry.unique_partition_guid.high == 0) return NO_PARTITION;

    ret->first_sect = entry.starting_lba;
    ret->sect_count = (entry.ending_lba - entry.starting_lba) + 1;

    return 0;
}

struct mbr_entry {
	uint8_t status;
	uint8_t chs_first_sect[3];
	uint8_t type;
	uint8_t chs_last_sect[3];
	uint32_t first_sect;
	uint32_t sect_count;
} __attribute__((packed));

static int mbr_get_part(struct part *ret, int drive, int partition) {
    // Variables.
    struct mbr_entry entry;
    const size_t entry_address = 0x1be + sizeof(struct mbr_entry) * partition;

    // Read the entry of the MBR.
    int r;
    if ((r = read(drive, &entry, entry_address, sizeof(struct mbr_entry)))) {
        return r;
    }

    // Check if the partition exists, fail if it doesnt.
    if (entry.type == 0) {
        return NO_PARTITION;
    }

    // Assign the final fields and return.
    ret->first_sect = entry.first_sect;
    ret->sect_count = entry.sect_count;
    return 0;
}

int get_part(struct part *part, int drive, int partition) {
    int ret;

    ret = gpt_get_part(part, drive, partition);
    if (ret != INVALID_TABLE)
        return ret;

    ret = mbr_get_part(part, drive, partition);
    if (ret != INVALID_TABLE)
        return ret;

    return -1;
}
