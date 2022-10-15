#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <fs/echfs.h>
#include <lib/libc.h>
#include <lib/misc.h>
#include <lib/print.h>
#include <drivers/disk.h>
#include <mm/pmm.h>

struct echfs_dir_entry {
    uint64_t parent_id;
    uint8_t type;
    char name[201];
    uint64_t atime;
    uint64_t mtime;
    uint16_t perms;
    uint16_t owner;
    uint16_t group;
    uint64_t ctime;
    uint64_t payload;
    uint64_t size;
} __attribute__((packed));

struct echfs_file_handle {
    struct volume *part;
    uint64_t block_size;
    uint64_t block_count;
    uint64_t dir_length;
    uint64_t alloc_table_size;
    uint64_t alloc_table_offset;
    uint64_t dir_offset;
    uint64_t file_block_count;
    uint64_t *alloc_map;
    struct echfs_dir_entry dir_entry;
};

struct echfs_identity_table {
    uint8_t  jmp[4];
    char     signature[8];
    uint64_t block_count;
    uint64_t dir_length;
    uint64_t block_size;
    uint32_t reserved;
    struct guid guid;
} __attribute__((packed));

#define ROOT_DIR_ID  (~((uint64_t)0))
#define END_OF_CHAIN (~((uint64_t)0))
#define FILE_TYPE    0
#define DIR_TYPE     1

static bool read_block(struct echfs_file_handle *file, void *buf, uint64_t block, uint64_t offset, uint64_t count) {
    return volume_read(file->part, buf, (file->alloc_map[block] * file->block_size) + offset, count);
}

static void echfs_read(struct file_handle *h, void *buf, uint64_t loc, uint64_t count) {
    struct echfs_file_handle *file = h->fd;

    for (uint64_t progress = 0; progress < count;) {
        uint64_t block = (loc + progress) / file->block_size;

        uint64_t chunk = count - progress;
        uint64_t offset = (loc + progress) % file->block_size;
        if (chunk > file->block_size - offset)
            chunk = file->block_size - offset;

        read_block(file, buf + progress, block, offset, chunk);
        progress += chunk;
    }
}

bool echfs_get_guid(struct guid *guid, struct volume *part) {
    struct echfs_identity_table id_table;
    volume_read(part, &id_table, 0, sizeof(struct echfs_identity_table));

    if (strncmp(id_table.signature, "_ECH_FS_", 8)) {
        return false;
    }

    *guid = id_table.guid;

    return true;
}

static void echfs_close(struct file_handle *file) {
    struct echfs_file_handle *f = file->fd;
    pmm_free(f->alloc_map, f->file_block_count * sizeof(uint64_t));
    pmm_free(f, sizeof(struct echfs_file_handle));
}

struct file_handle *echfs_open(struct volume *part, const char *path) {
    struct echfs_identity_table id_table;
    volume_read(part, &id_table, 0, sizeof(struct echfs_identity_table));

    if (strncmp(id_table.signature, "_ECH_FS_", 8)) {
        return NULL;
    }

    struct echfs_file_handle *ret = ext_mem_alloc(sizeof(struct echfs_file_handle));

    ret->part = part;

    ret->block_size         = id_table.block_size;
    ret->block_count        = id_table.block_count;
    ret->dir_length         = id_table.dir_length * ret->block_size;
    ret->alloc_table_size   = DIV_ROUNDUP(ret->block_count * sizeof(uint64_t), ret->block_size) * ret->block_size;
    ret->alloc_table_offset = 16 * ret->block_size;
    ret->dir_offset         = ret->alloc_table_offset + ret->alloc_table_size;

    // Find the file in the root dir.
    uint64_t wanted_parent = ROOT_DIR_ID;
    bool     last_elem     = false;

next:;
    char wanted_name[128];
    for (; *path == '/'; path++);
    for (int i = 0; ; i++, path++) {
        if (*path == '\0' || *path == '/') {
            if (*path == '\0')
                last_elem = true;
            wanted_name[i] = '\0';
            path++;
            break;
        }
        wanted_name[i] = *path;
    }

    for (uint64_t i = 0; i < ret->dir_length; i += sizeof(struct echfs_dir_entry)) {
        volume_read(ret->part, &ret->dir_entry, i + ret->dir_offset, sizeof(struct echfs_dir_entry));

        if (!ret->dir_entry.parent_id) {
            break;
        }

        int (*strcmpfn)(const char *, const char *) = case_insensitive_fopen ? strcasecmp : strcmp;

        if (strcmpfn(wanted_name, ret->dir_entry.name) == 0 &&
            ret->dir_entry.parent_id == wanted_parent &&
            ret->dir_entry.type == (last_elem ? FILE_TYPE : DIR_TYPE)) {
            if (last_elem) {
                goto found;
            } else {
                wanted_parent = ret->dir_entry.payload;
                goto next;
            }
        }
    }

    pmm_free(ret, sizeof(struct echfs_file_handle));
    return NULL;

found:;
    // Load the allocation map.
    ret->file_block_count = DIV_ROUNDUP(ret->dir_entry.size, ret->block_size);

    ret->alloc_map = ext_mem_alloc(ret->file_block_count * sizeof(uint64_t));

    ret->alloc_map[0] = ret->dir_entry.payload;
    for (uint64_t i = 1; i < ret->file_block_count; i++) {
        // Read the next block.
        volume_read(ret->part,
            &ret->alloc_map[i],
            ret->alloc_table_offset + ret->alloc_map[i-1] * sizeof(uint64_t),
            sizeof(uint64_t));
    }

    struct file_handle *handle = ext_mem_alloc(sizeof(struct file_handle));

    handle->fd = ret;
    handle->read = (void *)echfs_read;
    handle->close = (void *)echfs_close;
    handle->size = ret->dir_entry.size;
    handle->vol = part;
#if defined (UEFI)
    handle->efi_part_handle = part->efi_part_handle;
#endif

    return handle;
}
