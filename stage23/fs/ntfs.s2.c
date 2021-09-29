#include <fs/ntfs.h>
#include <mm/pmm.h>
#include <lib/print.h>

#include <stdbool.h>

// created using documentation from:
//  https://dubeyko.com/development/FileSystems/NTFS/ntfsdoc.pdf

// This is the total size of a file record, including the attributes
// TODO: calculate this
#define MIN_FILE_RECORD_SIZE 1024

// The mft number is only 48bit, the other bits are used 
// for something else
#define MFT_RECORD_NO_MASK (0xFFFFFFFFFFFF)

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

struct file_record_attr_name {
    uint64_t mft_parent_record;
    uint64_t creation_time;
    uint64_t altered_time;
    uint64_t mft_changed_time;
    uint64_t read_time;
    uint64_t allocated_size;
    uint64_t real_size;
    uint32_t flags;
    uint32_t reparse;
    uint8_t name_length;
    uint8_t name_type;
    uint16_t name[];
} __attribute__((packed));

struct file_record_attr_index_root {
    uint32_t type;
    uint32_t collation;
    uint32_t size;
    uint8_t clusters_per_index_rec;
    uint8_t _padding[3];
    uint32_t offset;
    uint32_t total_size;
    uint32_t alloc_size;
    uint8_t flags;
} __attribute__((packed));

struct index_record {
    char name[4];
    uint16_t update_seq_offset;
    uint16_t update_seq_size;
    uint64_t log_seq_number;
    uint64_t vcn;
    uint32_t index_entry_offset;
    uint32_t index_entry_size;
    uint32_t index_entry_alloc;
    uint8_t leaf_node;
    uint8_t _reserved[3];
    uint16_t update_seq;
    uint16_t sequence_array[];
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
    uint16_t name[];
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

// the temp buffer is used for storing dirs and alike
// in memory, because limine only has allocate without 
// free we are going to allocate it once globally and just 
// make sure to only use it in the ntfs_open function...
static uint8_t *dir_buffer = NULL;
static size_t dir_buffer_size = 0;
static size_t dir_buffer_cap = 0;

/**
 * Get an attribute from the given file record
 */
static bool ntfs_get_file_record_attr(uint8_t* file_record, uint32_t attr_type, uint8_t **out_attr) {
    struct mft_file_record *fr = (struct mft_file_record *)file_record;

    // get the offset to the first attribute
    uint8_t *cur_attr_ptr = file_record + fr->attribute_offset;

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

    uint8_t count_size = runlist_ptr[0] & 0xF;
    uint8_t cluster_size = (runlist_ptr[0] >> 4) & 0xF;
    runlist_ptr++;

    // get the run length
    uint64_t count = 0;
    for (int i = count_size; i > 0; i--) {
        count <<= 8;
        count |= runlist_ptr[i - 1];
    }
    runlist_ptr += count_size;

    // get the run offset
    int64_t cluster = 0;
    for (int i = cluster_size; i > 0; i--) {
        cluster <<= 8;
        cluster |= runlist_ptr[i - 1];
    }
    runlist_ptr += cluster_size;

    // sign exten the run offset
    if (cluster >> (cluster_size * 8 - 1)) {
        for (int i = 7; i >= cluster_size; i--) {
            cluster |= (uint64_t)0xFF << (i * 8);
        }
    }

    // out it, the cluster is relative to the last cluster
    // so add it
    *out_cluster += cluster;
    *out_cluster_count = count;

    // update it 
    if (next) {
        *runlist = runlist_ptr;
    }

    return true;
}

static bool ntfs_get_file_record(struct ntfs_file_handle *handle, uint64_t mft_record_no, uint8_t *file_record_buffer) {
    uint8_t *runlist = handle->mft_run_list;

    // make sure we only take the number itself
    mft_record_no &= MFT_RECORD_NO_MASK;

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
    struct mft_file_record *fr = (struct mft_file_record *)file_record_buffer;
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

    //
    // First we get the data from the index root (aka resident entries)
    //
    uint8_t* index_root_ptr;
    if (ntfs_get_file_record_attr(file_record, FR_ATTRIBUTE_INDEX_ROOT, &index_root_ptr)) {
        // we have a resident index root
        struct file_record_attr_header_res *index_root_header = (struct file_record_attr_header_res *)index_root_ptr;
        struct file_record_attr_index_root *index_root = (struct file_record_attr_index_root *)(index_root_ptr + index_root_header->info_offset);
        uint8_t *index_root_data = (uint8_t *)index_root + index_root->offset + offsetof(struct file_record_attr_index_root, offset);
        if (index_root->total_size > sizeof(handle->resident_index))
            panic("NTFS: Resident index is too big!");
        handle->resident_index_size = index_root->total_size;
        memcpy(handle->resident_index, index_root_data, index_root->total_size);
    } else {
        // no resident data, clear
        handle->resident_index_size = 0;
    }

    //
    // Now get the non-resident index records, for that we need to get the INDEX_ALLOC
    // attribute and read the runlist from that
    //
    uint8_t *index_alloc_ptr;
    if (ntfs_get_file_record_attr(file_record, FR_ATTRIBUTE_INDEX_ALLOC, &index_alloc_ptr)) {
        struct file_record_attr_header_non_res *index_alloc = (struct file_record_attr_header_non_res *)index_alloc_ptr;
        uint8_t *runlist_ptr = index_alloc_ptr + index_alloc->run_offset;
        if (runlist_ptr - file_record + 128u > handle->file_record_size)
            panic("NTFS: runlist is outside of file record!");
        memcpy(handle->run_list, runlist_ptr, sizeof(handle->run_list));

        // calculate the directory size by just going through the runlist
        uint8_t *runlist = handle->run_list;
        uint64_t dir_size = 0;
        uint64_t cluster = 0;
        uint64_t cluster_count = 0;
        bool status = false;
        do {
            status = ntfs_get_next_run_list_element(&runlist, &cluster_count, &cluster, true);
            if (status)
                dir_size += cluster_count;
        } while(status);
        dir_size *= handle->bpb.sectors_per_cluster * handle->bpb.bytes_per_sector;

        // allocate a buffer for the directory data
        if (dir_buffer == NULL) {
            // allocate enough just in case, idk how much is good
            dir_buffer_cap = dir_size > 64 * 1024 ? dir_size : 64 * 1024;
            dir_buffer = ext_mem_alloc(dir_buffer_cap);
        } else {
            // we must truncate it...
            if (dir_size > dir_buffer_cap) {
                dir_size = dir_buffer_cap;
            }
        }

        // set the size of the dir size
        dir_buffer_size = dir_size;

        // read the directory
        if (ntfs_read(handle, dir_buffer, 0, dir_size))
            panic("NTFS: EOF before reading directory fully...");
    } else {
        // if no runlist then empty the runlist 
        memset(handle->run_list, 0, sizeof(handle->run_list));
    }
    
    return true;
}

/**
 * Prepare for reading a file by reading the root directory into the file handle
 */
static void ntfs_read_root(struct ntfs_file_handle *handle) {
    // calculate the offset for the mft
    handle->mft_offset = (uint64_t)handle->bpb.mft_cluster * (uint64_t)handle->bpb.sectors_per_cluster * (uint64_t)handle->bpb.bytes_per_sector;

    // read the mft file record, this should be the size of a sector
    uint8_t file_record_buffer[MIN_FILE_RECORD_SIZE];
    if (!volume_read(handle->part, file_record_buffer, handle->mft_offset, sizeof(file_record_buffer)))
        panic("NTFS: Failed to read MFT file record");

    // get the file attribute
    uint8_t *attr_ptr = NULL;
    if (!ntfs_get_file_record_attr(file_record_buffer, FR_ATTRIBUTE_DATA, &attr_ptr))
        panic("NTFS: MFT file record missing DATA attribute");
    struct file_record_attr_header_non_res *attr = (struct file_record_attr_header_non_res *)attr_ptr;

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

/**
 * Iterate the files over a single index with entries
 */
static bool ntfs_iterate_index_entries(struct ntfs_file_handle *handle, uint8_t *entry_ptr, size_t index_size, const char *filename, size_t filename_size, struct index_entry **out_entry) {
    // loop the record for all of its indexes
    while (index_size) {
        // get the entry, if size is zero we done                
        struct index_entry *entry = (struct index_entry *)entry_ptr;
        if (entry->entry_size == 0)
            break;

        if (filename_size == entry->name_length) {
            // this name seem legit, need to get the real name from the mft
            // sometimes it works to use the index name but sometimes it has
            // invalid names for whatever reason that I can not understand, so
            // just always take it from the mft file record
            uint8_t file_record_buffer[MIN_FILE_RECORD_SIZE];
            if (!ntfs_get_file_record(handle, entry->mft_record, file_record_buffer))
                panic("NTFS: Failed to get file record");

            uint8_t *name_attr = NULL;
            if (!ntfs_get_file_record_attr(file_record_buffer, FR_ATTRIBUTE_NAME, &name_attr))
                panic("NTFS: File record missing name attribute");

            // get the offset to the actual info
            struct file_record_attr_header_res *header = (struct file_record_attr_header_res *)name_attr;
            struct file_record_attr_name *name = (struct file_record_attr_name *)(name_attr + header->info_offset);

            // compare the name
            for (int i = 0; i < name->name_length; i++) {
                if (name->name[i] != filename[i]) {
                    goto next_entry;
                }
            }

            // name is good, return the entry and return true
            // that we found the entry
            *out_entry = entry;
            return true;
        }

        // next entry
    next_entry:
        entry_ptr += entry->entry_size;
        index_size -= entry->entry_size;
    }

    return false;
}

/**
 * Search for a file in the ntfs directory, assumes the directory has been read and is stored in
 * the temp buffer
 */
static bool ntfs_find_file_in_directory(struct ntfs_file_handle *handle, const char* filename, struct index_entry** out_entry) {
    // get the size of the name we need to compare
    const char* temp_filename = filename;
    size_t filename_size = 0;
    while (*temp_filename != '\0' && *temp_filename != '\\' && *temp_filename != '/') {
        filename_size++;
        temp_filename++;
    }

    // first search in the resident records
    if (ntfs_iterate_index_entries(handle, handle->resident_index, handle->resident_index_size, filename, filename_size, out_entry))
        return true;

    // now iterate the non-resident files in the directory
    uint8_t *dir_ptr = dir_buffer;
    size_t dir_size = dir_buffer_size;
    size_t offset = 0;
    while (dir_size) {
        // check if the dir pointer is still in the buffer, if not then we could
        // not find the file...
        if (dir_ptr + sizeof(struct index_record) > dir_buffer + dir_buffer_size)
            panic("NTFS: Tried to read index record outside of directory");

        // get the index and check it, if it is not valid just return 
        // we did not find the file
        struct index_record *index_record = (struct index_record *)dir_ptr;
        if (strncmp(index_record->name, "INDX", SIZEOF_ARRAY(index_record->name)))
            return false;
        
        // calculate the offset to the entry
        size_t index_size = index_record->index_entry_size;
        offset += index_record->index_entry_offset + offsetof(struct index_record, index_entry_offset);
        uint8_t *entry_ptr = dir_ptr + offset;

        // check if any of the entries is valid
        if (ntfs_iterate_index_entries(handle, entry_ptr, index_size, filename, filename_size, out_entry))
            return true;

        // next record, need to do some rounding
        index_size = index_record->index_entry_size;
        if (index_size < 0x1000) {
            index_size = 0x1000;
        } else {
            index_size = (index_size + 0x100) & 0xffffff00;
        }

        dir_ptr += index_size;
        dir_size -= index_size;
    }

    return false;
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
    if (ret->file_record_size != MIN_FILE_RECORD_SIZE)
        panic("NTFS: TODO: support file record size which is not 1024 bytes");

    // now prepare the root directory so we can search for 
    // the rest of the stuff
    ntfs_read_root(ret);

    // iterate the directories to find the entry
    const char* current_path = path;
    struct index_entry* entry = NULL;
    for (;;) {
        // skip slash
        while (*current_path == '\\' || *current_path == '/') {
            current_path++;
        }
        
        // find the file in the directory
        entry = NULL;
        if (!ntfs_find_file_in_directory(ret, current_path, &entry))
            return 1;

        size_t filename_len = entry->name_length;

        // check if this is the last entry
        uint8_t file_record_buffer[MIN_FILE_RECORD_SIZE];
        if (*(current_path + filename_len) == '\0') {
            // we found the file!
            ret->size_bytes = entry->real_size;

            // get its runlist...
            if (!ntfs_get_file_record(ret, entry->mft_record, file_record_buffer))
                panic("NTFS: Failed to get file record of file");

            // get the file attribute
            uint8_t *attr_ptr = NULL;
            if (!ntfs_get_file_record_attr(file_record_buffer, FR_ATTRIBUTE_DATA, &attr_ptr))
                panic("NTFS: File record missing DATA attribute");
            struct file_record_attr_header_non_res *attr = (struct file_record_attr_header_non_res *)attr_ptr;

            // verify the attr and run list are in the buffer
            if ((uint8_t *)attr + sizeof(*attr) > file_record_buffer + sizeof(file_record_buffer))
                panic("NTFS: File record attribute is outside of file record");
            if ((uint8_t *)attr + attr->run_offset + 256 > file_record_buffer + sizeof(file_record_buffer))
                panic("NTFS: Run list is outside of file record");

            // save the run list
            memcpy(ret->run_list, (uint8_t *)attr + attr->run_offset, sizeof(ret->run_list));

            return 0;

        } else {
            // read the directory
            if (!ntfs_read_directory(ret, entry->mft_record, file_record_buffer))
                panic("NTFS: Failed to read directory");

            // next path element
            current_path += filename_len;
        }
    }

    // should not be able to reach here...
    __builtin_unreachable();
}

int ntfs_read(struct ntfs_file_handle *file, void *buf, uint64_t loc, uint64_t count) {
    // get the runlist
    uint8_t *runlist = file->run_list;

    // TODO: remember the last read location so we can have faster sequential reads...

    // we are going to go over the runlist until we get to the offset
    // once we get to the offset we are going to continue going over
    // the runlist while copying bytes
    uint64_t bytes_per_cluster = file->bpb.sectors_per_cluster * file->bpb.bytes_per_sector;
    do {
        // get the next element from the runlist
        uint64_t cluster = 0;
        uint64_t cluster_count = 0;
        if (!ntfs_get_next_run_list_element(&runlist, &cluster_count, &cluster, true))
            break;

        // calculate the cont size and offset on disk
        uint64_t total_cont_bytes = cluster_count * bytes_per_cluster;
        uint64_t abs_byte = cluster * bytes_per_cluster;

        // check if we arrived at the wanted offset
        if (loc != 0) {
            if (loc >= total_cont_bytes) {
                // we need to go more...
                loc -= total_cont_bytes;
            } else {
                // we got to the offset, adjust base and size
                // and set the loc to 0
                total_cont_bytes -= loc;
                abs_byte += loc;
                loc = 0;
            }
        }

        if (loc == 0) {
            // get how much we wanna read now and 
            // subtract that from the total we need 
            // to read
            size_t read_now = total_cont_bytes;
            if (read_now > count) {
                read_now = count;
            }
            count -= read_now;

            // read it!
            if (!volume_read(file->part, buf, abs_byte, read_now))
                panic("NTFS: Runlist points to outside the volume (%x)", abs_byte);
        }
    } while(count);

    // if we didn't read it all then we got a problem
    return count != 0;
}
