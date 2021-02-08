#include <stddef.h>
#include <stdint.h>
#include <lib/part.h>
#include <drivers/disk.h>
#include <lib/libc.h>
#include <lib/blib.h>
#include <lib/real.h>
#include <lib/print.h>
#include <mm/pmm.h>
#include <fs/file.h>

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
    struct guid disk_guid;

    // entries related
    uint64_t partition_entry_lba;
    uint32_t number_of_partition_entries;
    uint32_t size_of_partition_entry;
    uint32_t partition_entry_array_crc32;
} __attribute__((packed));

struct gpt_entry {
    struct guid partition_type_guid;

    struct guid unique_partition_guid;

    uint64_t starting_lba;
    uint64_t ending_lba;

    uint64_t attributes;

    uint16_t partition_name[36];
} __attribute__((packed));

static int gpt_get_part(struct part *ret, int drive, int partition) {
    struct gpt_table_header header = {0};

    int sector_size = disk_get_sector_size(drive);

    // read header, located after the first block
    disk_read(drive, &header, sector_size * 1, sizeof(header));

    // check the header
    // 'EFI PART'
    if (strncmp(header.signature, "EFI PART", 8))
        return INVALID_TABLE;
    if (header.revision != 0x00010000)
        return END_OF_TABLE;

    // parse the entries if reached here
    if ((uint32_t)partition >= header.number_of_partition_entries)
        return END_OF_TABLE;

    struct gpt_entry entry = {0};
    disk_read(drive, &entry,
         (header.partition_entry_lba * sector_size) + (partition * sizeof(entry)),
         sizeof(entry));

    struct guid empty_guid = {0};
    if (!memcmp(&entry.unique_partition_guid, &empty_guid, sizeof(struct guid)))
        return NO_PARTITION;

    ret->drive       = drive;
    ret->partition   = partition;
    ret->sector_size = sector_size;
    ret->first_sect  = entry.starting_lba;
    ret->sect_count  = (entry.ending_lba - entry.starting_lba) + 1;

    struct guid guid;
    if (!fs_get_guid(&guid, ret)) {
        ret->guid_valid = false;
    } else {
        ret->guid_valid = true;
        ret->guid = guid;
    }

    ret->part_guid_valid = true;
    ret->part_guid = entry.unique_partition_guid;

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

static int mbr_get_logical_part(struct part *ret, struct part *extended_part,
                                int drive, int partition) {
    struct mbr_entry entry;

    size_t ebr_sector = 0;

    for (int i = 0; i < partition; i++) {
        size_t entry_offset = ebr_sector * extended_part->sector_size + 0x1ce;

        int r;
        r = part_read(extended_part, &entry, entry_offset, sizeof(struct mbr_entry));
        if (r)
            return r;

        if (entry.type != 0x0f && entry.type != 0x05)
            return END_OF_TABLE;

        ebr_sector = entry.first_sect;
    }

    size_t entry_offset = ebr_sector * extended_part->sector_size + 0x1be;

    int r;
    r = part_read(extended_part, &entry, entry_offset, sizeof(struct mbr_entry));
    if (r)
        return r;

    if (entry.type == 0)
        return NO_PARTITION;

    ret->drive       = drive;
    ret->partition   = partition + 4;
    ret->sector_size = disk_get_sector_size(drive);
    ret->first_sect  = extended_part->first_sect + ebr_sector + entry.first_sect;
    ret->sect_count  = entry.sect_count;

    struct guid guid;
    if (!fs_get_guid(&guid, ret)) {
        ret->guid_valid = false;
    } else {
        ret->guid_valid = true;
        ret->guid = guid;
    }

    ret->part_guid_valid = false;

    return 0;
}

static int mbr_get_part(struct part *ret, int drive, int partition) {
    // Check if actually valid mbr
    uint16_t hint;
    disk_read(drive, &hint, 444, sizeof(uint16_t));
    if (hint && hint != 0x5a5a)
        return INVALID_TABLE;

    struct mbr_entry entry;

    if (partition > 3) {
        for (int i = 0; i < 4; i++) {
            size_t entry_offset = 0x1be + sizeof(struct mbr_entry) * i;

            int r = disk_read(drive, &entry, entry_offset, sizeof(struct mbr_entry));
            if (r)
                return r;

            if (entry.type != 0x0f)
                continue;

            struct part extended_part;

            extended_part.drive       = drive;
            extended_part.partition   = i;
            extended_part.sector_size = disk_get_sector_size(drive);
            extended_part.first_sect  = entry.first_sect;
            extended_part.sect_count  = entry.sect_count;

            return mbr_get_logical_part(ret, &extended_part, drive, partition - 4);
        }

        return END_OF_TABLE;
    }

    size_t entry_offset = 0x1be + sizeof(struct mbr_entry) * partition;

    int r = disk_read(drive, &entry, entry_offset, sizeof(struct mbr_entry));
    if (r)
        return r;

    if (entry.type == 0)
        return NO_PARTITION;

    ret->drive       = drive;
    ret->partition   = partition;
    ret->sector_size = disk_get_sector_size(drive);
    ret->first_sect  = entry.first_sect;
    ret->sect_count  = entry.sect_count;

    struct guid guid;
    if (!fs_get_guid(&guid, ret)) {
        ret->guid_valid = false;
    } else {
        ret->guid_valid = true;
        ret->guid = guid;
    }

    ret->part_guid_valid = false;

    return 0;
}

int part_get(struct part *part, int drive, int partition) {
    int ret;

    ret = gpt_get_part(part, drive, partition);
    if (ret != INVALID_TABLE)
        return ret;

    ret = mbr_get_part(part, drive, partition);
    if (ret != INVALID_TABLE)
        return ret;

    return INVALID_TABLE;
}

static struct part *part_index = NULL;
static size_t part_index_i = 0;

void part_create_index(void) {
    size_t part_count = 0;

    for (uint8_t drive = 0x80; drive < 0x8f; drive++) {
        struct rm_regs r = {0};
        struct bios_drive_params drive_params;

        r.eax = 0x4800;
        r.edx = drive;
        r.ds  = rm_seg(&drive_params);
        r.esi = rm_off(&drive_params);

        drive_params.buf_size = sizeof(struct bios_drive_params);

        rm_int(0x13, &r, &r);

        if (r.eflags & EFLAGS_CF)
            continue;

        print("Found BIOS drive %x\n", drive);
        print(" ... %X total %u-byte sectors\n",
              drive_params.lba_count, drive_params.bytes_per_sect);

        for (int part = 0; ; part++) {
            struct part p;
            int ret = part_get(&p, drive, part);

            if (ret == END_OF_TABLE || ret == INVALID_TABLE)
                break;
            if (ret == NO_PARTITION)
                continue;

            part_count++;
        }
    }

    part_index = ext_mem_alloc(sizeof(struct part) * part_count);

    for (uint8_t drive = 0x80; drive < 0x8f; drive++) {
        struct rm_regs r = {0};
        struct bios_drive_params drive_params;

        r.eax = 0x4800;
        r.edx = drive;
        r.ds  = rm_seg(&drive_params);
        r.esi = rm_off(&drive_params);

        drive_params.buf_size = sizeof(struct bios_drive_params);

        rm_int(0x13, &r, &r);

        if (r.eflags & EFLAGS_CF)
            continue;

        for (int part = 0; ; part++) {
            struct part p;
            int ret = part_get(&p, drive, part);

            if (ret == END_OF_TABLE || ret == INVALID_TABLE)
                break;
            if (ret == NO_PARTITION)
                continue;

            part_index[part_index_i++] = p;
        }
    }
}

bool part_get_by_guid(struct part *part, struct guid *guid) {
    size_t i;
    for (i = 0; i < part_index_i; i++) {
        if (part_index[i].guid_valid
         && memcmp(&part_index[i].guid, guid, 16) == 0) {
            goto found;
        }
        if (part_index[i].part_guid_valid
         && memcmp(&part_index[i].part_guid, guid, 16) == 0) {
            goto found;
        }
    }
    return false;
found:
    *part = part_index[i];
    return true;
}

int part_read(struct part *part, void *buffer, uint64_t loc, uint64_t count) {
    return disk_read(part->drive, buffer,
                     loc + (part->first_sect * part->sector_size), count);
}
