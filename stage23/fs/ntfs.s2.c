#include <fs/ntfs.h>

#include <lib/print.h>

#include <stdbool.h>

// This is the total size of a file record, including the attributes
// TODO: calculate this
#define MIN_FILE_RECORD_SIZE 1024

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
 * Gets a count and cluster from the runlist, if next is true then it updates the list intenrally
 * so the next call will return the next element
 * 
 * if returned false we got to the end of the file.
 */
static bool ntfs_get_next_run_list_element(uint8_t **runlist, uint64_t *out_cluster_count, uint64_t *out_cluster, bool next) {
    uint8_t *runlist_ptr = *runlist;

    // we have reached the end of the file
    if (runlist_ptr[0] == 0) {
        return false;
    }

    uint8_t low = runlist_ptr[0] & 0xF;
    uint8_t high = (runlist_ptr[0] >> 4) & 0xF;
    runlist_ptr++;

    // get the run length
    uint64_t count = 0;
    for (int i = low; i > 0; i--) {
        count <<= 8;
        count |= runlist_ptr[i - 1];
    }
    runlist_ptr += low;

    // get the high byte first
    int8_t high_byte = (int8_t)runlist_ptr[high - 1];

    // get the run offset
    uint64_t cluster = 0;
    for (int i = high; i > 0; i--) {
        cluster <<= 8;
        cluster |= runlist_ptr[i - 1];
    }
    runlist_ptr += high;

    // if the offset is negative, fill the empty bytes with 0xff
    if (high_byte < 0 && high < 8) {
        uint64_t fill = 0;
        for (int i = 8; i > high; i--) {
            fill >>= 8;
            fill |= 0xFF00000000000000;
        }
        cluster |= fill;
    }

    // out it
    *out_cluster = cluster;
    *out_cluster_count = count;

    // update it 
    if (next) {
        *runlist = runlist_ptr;
    }

    return true;
}

static bool ntfs_get_file_record(struct ntfs_file_handle *handle, uint64_t mft_record_no, uint8_t *file_record_buffer) {
    uint8_t *runlist = handle->mft_run_list;

    // get the 
    uint64_t count = 0;
    uint64_t cluster = 0;
    if (!ntfs_get_next_run_list_element(&runlist, &count, &cluster, true)) {
        return false;
    }

    size_t bytes_per_cluster = handle->bpb.bytes_per_sector * handle->bpb.sectors_per_cluster;
    uint64_t byte_count = count * bytes_per_cluster;
    uint64_t sector = cluster * handle->bpb.sectors_per_cluster;
    uint64_t record_count = 0;
    do {
        // consume the items from the current runlist
        if (byte_count > 0) {
            sector += handle->sectors_per_file_record;
            byte_count -= handle->file_record_size;
        } else {
            // get the next run list...
            if (!ntfs_get_next_run_list_element(&runlist, &count, &cluster, true)) {
                // reached the end of the mft, did not find it...
                return false;
            }
            byte_count = count * bytes_per_cluster;
            sector = cluster * handle->bpb.sectors_per_cluster;
            continue;
        }
        record_count++;
    } while (record_count < mft_record_no);

    // we found the sector of the file record!
    uint64_t offset = sector * handle->bpb.bytes_per_sector;
    

    if(!volume_read(handle->part, file_record_buffer, offset, handle->file_record_size))
        panic("NTFS: Failed to read file record from mft");

    // make sure this is a valid file record
    struct mft_file_record* fr = (struct mft_file_record*)file_record_buffer;
    if (strncmp(fr->name, "FILE", SIZEOF_ARRAY(fr->name)))
        panic("NTFS: File record has invalid signature (got %c%c%c%c, should be FILE)!",
            fr->name[0], fr->name[1], fr->name[2], fr->name[3]);

    // we good!
    return true;
}

/**
 * Read the the directory's file record from the mft
 */
static bool ntfs_read_directory(struct ntfs_file_handle *handle, uint64_t mft_record, uint8_t *file_record) {
    // get the record of the directory
    if (!ntfs_get_file_record(handle, mft_record, file_record))
        return false;

    return true;
}

/**
 * Get an attribute from the given file record
 */
static bool ntfs_get_file_record_attr(uint8_t* file_record, uint32_t attr_type, uint8_t **out_attr) {
    struct mft_file_record *fr = (struct mft_file_record *)file_record;

    // get the offset to the first attribute
    uint8_t* cur_attr_ptr = file_record + fr->attribute_offset;

    while (true) {
        // TODO: don't check for the min size, but for the actual size...
        if (cur_attr_ptr + sizeof(struct file_record_attr_header) > file_record + MIN_FILE_RECORD_SIZE)
            panic("NTFS: File record attribute is outside of file record");

        struct file_record_attr_header *cur_attr = (struct file_record_attr_header *)cur_attr_ptr;

        if (cur_attr->type == attr_type) {
            *out_attr = cur_attr_ptr;
            return true;
        }

        // we either found an attr with higher type or the end type
        if (cur_attr->type > attr_type || cur_attr->type == 0xFF)
            return false;

        if (cur_attr->length == 0)
            panic("NTFS: File record attribute has zero length");

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
    uint8_t file_record_buffer[handle->file_record_size];
    if (!volume_read(handle->part, file_record_buffer, handle->mft_offset, sizeof(file_record_buffer)))
        panic("NTFS: Failed to read MFT file record");

    // get the file attribute
    struct file_record_attr_header_non_res *attr;
    if (!ntfs_get_file_record_attr(file_record_buffer, FR_ATTRIBUTE_DATA, (uint8_t **)&attr))
        panic("NTFS: MFT file record missing DATA attribute");

    // verify the attr and run list are in the buffer
    if ((uint8_t *)attr + sizeof(*attr) > file_record_buffer + sizeof(file_record_buffer))
        panic("NTFS: MFT file record attribute is outside of file record");
    if ((uint8_t *)attr + attr->run_offset + 256 > file_record_buffer + sizeof(file_record_buffer))
        panic("NTFS: MFT Run list is outside of file record");

    // save the run list
    memcpy(handle->mft_run_list, (uint8_t *)attr + attr->run_offset, sizeof(handle->mft_run_list));

    // read the root directory record, which has the number 5
    if (!ntfs_read_directory(handle, 5, file_record_buffer))
        panic("NTFS: Missing root directory file record!");
}

int ntfs_open(struct ntfs_file_handle *ret, struct volume *part, const char *path) {
    // save the part
    ret->part = part;

    // start by reading the bpb so we can access it later on
    if (!volume_read(part, &ret->bpb, 0, sizeof(ret->bpb)))
        panic("NTFS: Failed to read the BPB");

    // in NTFS sector size can be 512 to 4096 bytes, file records are 
    // at least 1024 bytes, in here calculate the sectors per file record
    // and the file record size
    if (ret->bpb.bytes_per_sector <= MIN_FILE_RECORD_SIZE) {
        // this has multiple sectors
        ret->sectors_per_file_record = MIN_FILE_RECORD_SIZE / ret->bpb.bytes_per_sector;
        ret->file_record_size = MIN_FILE_RECORD_SIZE;
    } else {
        // this has a single sector
        ret->sectors_per_file_record = 1;
        ret->file_record_size = ret->bpb.bytes_per_sector;
    }
    
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
