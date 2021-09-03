#include <fs/ntfs.h>

#include <lib/print.h>

#include <stdbool.h>

struct mft_file_record {
    char name[4];
    uint16_t update_seq_offset;
    uint16_t update_seq_size;
    uint64_t log_seq_number;
    uint16_t sequence_number;
    uint16_t hard_link_count;
    uint16_t attribute_offset;
    uint16_t flags;
    uint32_t real_size;
    uint32_t allocated_size;
    uint64_t base_record_number;
} __attribute__((packed));

struct file_record_attr_header {
    uint32_t type;
    uint32_t length;
    uint8_t non_res_flag;
    uint8_t name_length;
    uint16_t name_offset;
    uint16_t flags;
    uint16_t attribute_id;
} __attribute__((packed));

#define     FR_ATTRIBUTE_LIST           0x00000020
#define     FR_ATTRIBUTE_NAME           0x00000030
#define     FR_ATTRIBUTE_VOLUME_NAME    0x00000060
#define     FR_ATTRIBUTE_DATA           0x00000080
#define     FR_ATTRIBUTE_INDEX_ROOT     0x00000090
#define     FR_ATTRIBUTE_INDEX_ALLOC    0x000000A0

struct file_record_attr_header_res {
    struct file_record_attr_header header;
    uint32_t info_length;
    uint16_t info_offset;
    uint16_t index_flag;
} __attribute__((packed));

struct file_record_attr_header_non_res {
    struct file_record_attr_header header;
    uint64_t first_vcn;
    uint64_t last_vcn;
    uint16_t run_offset;
} __attribute__((packed));

struct index_entry {
    uint64_t mft_record;
    uint16_t entry_size;
    uint16_t name_offset;
    uint16_t index_Flag;
    uint16_t padding;
    uint64_t mft_parent_record;
    uint64_t creation_time;
    uint64_t altered_time;
    uint64_t mft_changed_time;
    uint64_t read_time;
    uint64_t alloc_size;
    uint64_t real_size;
    uint64_t file_flags;
    uint8_t name_length;
    uint8_t name_type;
    wchar_t name[];
} __attribute__((packed));

int ntfs_check_signature(struct volume *part) {
    struct ntfs_bpb bpb;
    if (!volume_read(part, &bpb, 0, sizeof(bpb))) {
        return 0;
    }

    //
    // validate the bpb
    //

    if (strncmp(bpb.oem, "NTFS    ", SIZEOF_ARRAY(bpb.oem))) {
        return 0;
    }

    if (bpb.sector_totals != 0) {
        return 0;
    }

    if (bpb.large_sectors_count != 0) {
        return 0;
    }

    if (bpb.sectors_count_64 == 0) {
        return 0;
    }

    // this is a valid ntfs sector
    return 1;
}

/**
 * Read the the directory's file record from the mft
 */
// static int ntfs_read_directory(struct ntfs_file_handle *handle, uint64_t mft_record, char *file_record) {

// }

/**
 * Get an attribute from the given file record
 */
static bool ntfs_get_file_record_attr(uint8_t* file_record, uint32_t attr_type, uint8_t **out_attr) {
    struct mft_file_record *fr = (struct mft_file_record *)file_record;

    // get the offset to the first attribute
    uint8_t* cur_attr_ptr = file_record + fr->attribute_offset;

    while (true) {
        if (cur_attr_ptr + sizeof(struct file_record_attr_header) > file_record + 4096)
            panic("File record attribute is outside of file record");

        struct file_record_attr_header *cur_attr = (struct file_record_attr_header *)cur_attr_ptr;

        if (cur_attr->type == attr_type) {
            *out_attr = cur_attr_ptr;
            return true;
        }

        // we either found an attr with higher type or the end type
        if (cur_attr->type > attr_type || cur_attr->type == 0xFF)
            return false;

        if (cur_attr->length == 0)
            panic("File record attribute has zero length");

        cur_attr_ptr += cur_attr->length;
    }
}

/**
 * Prepare for reading a file by reading the root directory into the file handle
 */
static void ntfs_read_root(struct ntfs_file_handle *handle) {
    // calculate the offset for the mft
    handle->mft_offset = (uint64_t)handle->bpb.mft_cluster * (uint64_t)handle->bpb.sectors_per_cluster * (uint64_t)handle->bpb.bytes_per_sector;

    // read the mft file record, this should be the size of a sector
    // but we will use 4096 since it should cover it 
    uint8_t file_record_buffer[4096];
    if (!volume_read(handle->part, file_record_buffer, handle->mft_offset, sizeof(file_record_buffer)))
        panic("Failed to read MFT file record");

    // get the file attribute
    struct file_record_attr_header_non_res *attr;
    if (!ntfs_get_file_record_attr(file_record_buffer, FR_ATTRIBUTE_DATA, (uint8_t **)&attr))
        panic("MFT file record missing DATA attribute");

    // verify the attr and run list are in the buffer
    if ((uint8_t *)attr + sizeof(*attr) > file_record_buffer + sizeof(file_record_buffer))
        panic("MFT file record attribute is outside of file record");
    if ((uint8_t *)attr + attr->run_offset + 256 > file_record_buffer + sizeof(file_record_buffer))
        panic("MFT Run list is outside of file record");

    // save the run list
    memcpy(handle->mft_run_list, (uint8_t *)attr + attr->run_offset, sizeof(handle->mft_run_list));

    // TODO: read the root directory
}

// static int ntfs_find_file_in_directory(char *dir, size_t dir_size, short *name, struct index_entry* entry) {
// }

int ntfs_open(struct ntfs_file_handle *ret, struct volume *part, const char *path) {
    // save the part
    ret->part = part;

    // start by reading the bpb so we can access it later on
    if (!volume_read(part, &ret->bpb, 0, sizeof(ret->bpb)))
        panic("Failed to read the BPB");

    // now prepare the root directory so we can search for 
    // the rest of the stuff
    ntfs_read_root(ret);

    // TODO: search for the file we want
    (void)path;

    return 1;
}

int ntfs_read(struct ntfs_file_handle *file, void *buf, uint64_t loc, uint64_t count) {
    (void)file;
    (void)buf;
    (void)loc;
    (void)count;
    return 1;
}
