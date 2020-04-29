#include <drivers/disk.h>
#include <fs/fat32fs.h>
#include <lib/blib.h>
#include <lib/libc.h>

#define FAT32_FSINFO_LEAD_SIGNATURE 0x41615252

struct fat32_bios_parameter_block {
    uint8_t jump_to_code[3];
    uint8_t oem_name[8];
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sector_count;
    uint8_t fat_count;
    uint16_t root_entry_count;
    uint16_t sector_count_16;
    uint8_t media_type;
    uint16_t sectors_per_fat;
    uint16_t sectors_per_track;
    uint16_t head_count;
    uint32_t hidden_sectors;
    uint32_t sector_count_32;
} __attribute__((packed));

struct fat32_extended_boot_record {
    uint32_t sectors_per_fat;
    uint16_t flags;
    uint16_t fs_version;
    uint32_t root_dir_start_cluster;
    uint16_t fsinfo_sector_number;
    uint16_t backup_bootsector_sector_number;
    uint8_t reserved1[12];
    uint8_t drive_number;
    uint8_t reserved2;
    uint8_t signature;
    uint32_t volume_id;
    uint8_t volume_label[11];
    uint8_t file_system_type[8];
} __attribute__((packed));

/// \return Position of the dot relative to the beginning of the string,
///         -1 if the dot was not found.
static int find_last_dot(const char* str)
{
    int last_dot = -1;

    const char* ptr = str;
    while (*ptr != '\0') {
        if (*ptr == '.')
            last_dot = ptr - str;
        ptr++;
    }

    return last_dot;
}

static int read_cluster(struct fat32_file_handle* file, void* buf, uint64_t cluster, uint64_t offset, uint64_t count)
{
    return read_partition(file->disk, &file->part, buf, file->allocation_map[cluster] + offset, count);
}

int fat32_check_signature(int disk, int partition)
{
    struct part part;
    get_part(&part, disk, partition);

    struct fat32_bios_parameter_block bpb;
    read_partition(disk, &part, &bpb, 0, sizeof(struct fat32_bios_parameter_block));

    struct fat32_extended_boot_record ebr;
    read_partition(disk, &part, &ebr, 36, sizeof(struct fat32_extended_boot_record));

    uint64_t fsinfo_position = ebr.fsinfo_sector_number * bpb.bytes_per_sector;
    uint32_t fsinfo_signature;
    read_partition(disk, &part, &fsinfo_signature, fsinfo_position, sizeof(uint32_t));

    return fsinfo_signature == FAT32_FSINFO_LEAD_SIGNATURE ? 1 : 0;
}

int fat32_open(struct fat32_file_handle* ret, int disk, int partition, const char* filename)
{
    if (!fat32_check_signature(disk, partition)) {
        print("FAT32: Not a valid FAT32 partition.");
        return -1;
    }

    ret->disk = disk;

    get_part(&ret->part, disk, partition);

    struct fat32_bios_parameter_block bpb;
    read_partition(disk, &ret->part, &bpb, 0, sizeof(struct fat32_bios_parameter_block));

    struct fat32_extended_boot_record ebr;
    read_partition(disk, &ret->part, &ebr, 36, sizeof(struct fat32_extended_boot_record));

    ret->sector_size = bpb.bytes_per_sector;
    ret->reserved_sector_count = bpb.reserved_sector_count;
    ret->sector_count = bpb.sector_count_16 == 0 ? bpb.sector_count_32 : bpb.sector_count_16;

    ret->cluster_size = bpb.sectors_per_cluster * bpb.bytes_per_sector;

    ret->fat_offset = bpb.reserved_sector_count * bpb.bytes_per_sector;
    const uint32_t sectors_per_fat = (bpb.sectors_per_fat == 0 ? ebr.sectors_per_fat : bpb.sectors_per_fat);
    ret->fat_size = sectors_per_fat * bpb.bytes_per_sector;

    ret->root_directory_offset = (bpb.reserved_sector_count + bpb.fat_count * sectors_per_fat) * bpb.bytes_per_sector;

    // ! Only looking though the root directory is supported.
    // ! There is no support for Long File Names.

    if (filename[0] == '/')
        filename += 1;
    size_t filename_length = strlen(filename);
    if (filename_length > 12) {
        print("FAT32: %s: File name is too long. LFN is not supported yet.\n", filename);
        return -1;
    }

    int dot_position = find_last_dot(filename);
    print("FAT32: Debug.\n");
    if (filename_length - dot_position - 1 > 3) {
        print("FAT32: %s: File extension is too long. LFN is not supported yet.\n", filename);
        return -1;
    }
    if (dot_position > 8) {
        print("FAT32: %s: File name name is too long. LFN is not supported yet.\n", filename);
        return -1;
    }

    char looking_for[11] = "           ";
    // Copy the file name.
    memcpy(looking_for, filename, dot_position);
    // Copy the extension.
    memcpy(looking_for + 8, filename + dot_position + 1, filename_length - dot_position - 1);
    // Uppercase the file name.
    for (int i = 0; i < 11; ++i) {
        looking_for[i] = toupper(looking_for[i]);
    }

    char debug[11] = "";
    strncpy(debug, looking_for, 11);
    print("FILENAME: %s\n", debug);

    for (
        uint32_t i = ret->root_directory_offset;
        i < ret->root_directory_offset + ret->cluster_size;
        i += sizeof(struct fat32_directory_entry)) {

        struct fat32_directory_entry current_entry;
        read_partition(disk, &ret->part, &current_entry.file_name, i, sizeof(struct fat32_directory_entry));

        if (strncmp((char*)current_entry.file_name, looking_for, 11) == 0) {
            print("FAT32: Found the requested entry.\n");
            ret->directory_entry = current_entry;

            ret->allocation_map_size = DIV_ROUNDUP(current_entry.file_size, ret->cluster_size);
            ret->allocation_map = balloc(ret->allocation_map_size * sizeof(uint32_t));

            const uint32_t fat_size = (bpb.sectors_per_fat == 0) ? ebr.sectors_per_fat : bpb.sectors_per_fat;
            const uint32_t root_directory_sectors = DIV_ROUNDUP(bpb.root_entry_count * 32, bpb.bytes_per_sector);
            const uint32_t first_data_sector = bpb.reserved_sector_count + (bpb.fat_count * fat_size) + root_directory_sectors;

#define GET_FIRST_SECTOR_OF_CLUSTER(Cluster) (first_data_sector + (Cluster - 2) * bpb.sectors_per_cluster)
#define GET_CLUSTER_ADDRESS(Cluster) (GET_FIRST_SECTOR_OF_CLUSTER(Cluster) * bpb.bytes_per_sector)

            // Allocation map contains addresses of clusters to load.
            const uint32_t first_entry_cluster = (current_entry.first_cluster_high << 16) + current_entry.first_cluster_low;

            //ret->allocation_map[0] = GET_CLUSTER_ADDRESS(first_entry_cluster);
            ret->allocation_map[0] = GET_CLUSTER_ADDRESS(first_entry_cluster);

            const uint32_t fat_start = bpb.reserved_sector_count * bpb.bytes_per_sector;

            uint32_t next_cluster;
            read_partition(ret->disk, &ret->part,
                &next_cluster,
                fat_start + first_entry_cluster * sizeof(uint32_t),
                sizeof(uint32_t));
            ret->allocation_map[1] = GET_CLUSTER_ADDRESS(next_cluster);

            for (uint32_t i = 2; i < ret->allocation_map_size; ++i) {
                read_partition(ret->disk, &ret->part,
                    &next_cluster,
                    fat_start + next_cluster * sizeof(uint32_t),
                    sizeof(uint32_t));
                ret->allocation_map[i] = GET_CLUSTER_ADDRESS(next_cluster);
            }

            for (uint32_t i = 0; i < ret->allocation_map_size; ++i) {
                print("CHAIN: %x\n", ret->allocation_map[i]);
            }

            // ret->allocation_map = balloc(ret->allocation_map_size * sizeof(uint32_t));
            // ret->allocation_map[0] = current_entry.first_cluster_high << 16 | current_entry.first_cluster_low;
            // for (uint32_t i = 0; i < ret->allocation_map_size; ++i) {
            //     read_partition(ret->disk, &ret->part,
            //         &ret->allocation_map[i],
            //         ret->fat_offset + ret->allocation_map[i - 1] * sizeof(uint32_t),
            //         sizeof(uint32_t));
            // }

#undef GET_CLUSTER_ADDRESS
#undef GET_FIRST_SECTOR_OF_CLUSTER

            return 0;
        }
    }

    print("FAT32: File %s was not found.\n", filename);
    return -1;
}

int fat32_read(struct fat32_file_handle* file, void* buf, uint64_t loc, uint64_t count)
{
    uint32_t progress = 0;
    while (progress < count) {
        uint32_t cluster_number = (loc + progress) / file->cluster_size;
        uint32_t chunk_size = count - progress;
        uint32_t offset_inside_cluster = (loc + progress) % file->cluster_size;

        if (chunk_size > file->cluster_size - offset_inside_cluster)
            chunk_size = file->cluster_size - offset_inside_cluster;

        read_cluster(file, buf + progress, cluster_number, offset_inside_cluster, chunk_size);
        progress += chunk_size;
    }
}
