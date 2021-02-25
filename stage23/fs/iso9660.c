#include <fs/iso9660.h>
#include <lib/blib.h>
#include <lib/libc.h>
#include <mm/pmm.h>

#define ISO9660_FIRST_VOLUME_DESCRIPTOR 0x10
#define ISO9660_VOLUME_DESCRIPTOR_SIZE ISO9660_SECTOR_SIZE
#define ROCK_RIDGE_MAX_FILENAME 255

// --- Both endian structures ---
struct BE16_t { uint16_t little, big; } __attribute__((packed));
struct BE32_t { uint32_t little, big; } __attribute__((packed));

// --- Directory entries ---
struct iso9660_directory_entry {
    uint8_t length;
    uint8_t extended_attribute_length;
    struct BE32_t extent;
    struct BE32_t extent_size;
    uint8_t datetime[7];
    uint8_t flags;
    uint8_t interleaved_unit_size;
    uint8_t interleaved_gap_size;
    struct BE16_t volume_seq;
    uint8_t filename_size;
} __attribute__((packed));

// --- Volume descriptors ---
// VDT = Volume Descriptor Type
enum {
    ISO9660_VDT_BOOT_RECORD,
    ISO9660_VDT_PRIMARY,
    ISO9660_VDT_SUPPLEMENTARY,
    ISO9660_VDT_PARTITION_DESCRIPTOR,
    ISO9660_VDT_TERMINATOR = 255
};

struct iso9660_volume_descriptor {
    uint8_t type;
    char identifier[5];
    uint8_t version;
    uint8_t data[2041];
} __attribute__((packed));

struct iso9660_primary_volume {
    uint8_t type;
    char standard_identifier[5];
    uint8_t version;
    uint8_t unused0[1];
    char system_identifier[32];
    char volume_identifier[32];
    uint8_t unused1[8];
    struct BE32_t space_size;
    uint8_t unused2[32];
    struct BE16_t set_size;
    struct BE16_t volume_seq;
    struct BE16_t LBA_size;
    struct BE32_t path_table_size;

    uint32_t LBA_path_table_little;
    uint32_t LBA_optional_path_table_little;
    uint32_t LBA_path_table_big;
    uint32_t LBA_optional_path_table_big;

    struct iso9660_directory_entry root;
    uint8_t no_one_cares[1858];
} __attribute__((packed));


// --- Implementation ---
struct iso9660_contexts_node {
    struct iso9660_context context;
    struct iso9660_contexts_node *next;
};
struct iso9660_contexts_node *contexts = NULL;

static void iso9660_find_PVD(struct iso9660_volume_descriptor *desc, struct volume *vol) {
    uint32_t lba = ISO9660_FIRST_VOLUME_DESCRIPTOR;
    while (true) {
        volume_read(vol, desc, lba * ISO9660_SECTOR_SIZE, ISO9660_SECTOR_SIZE);

        switch (desc->type) {
        case ISO9660_VDT_PRIMARY:
            return;
        case ISO9660_VDT_TERMINATOR:
            panic("ISO9660: no primary volume descriptor");
            break;
        }

        ++lba;
    }
}

static void iso9660_cache_root(struct volume *vol,
                               void **root,
                               uint32_t *root_size) {
    struct iso9660_primary_volume pv;
    iso9660_find_PVD((struct iso9660_volume_descriptor *)&pv, vol);

    *root_size = pv.root.extent_size.little;
    *root = ext_mem_alloc(*root_size);
    volume_read(vol, *root, pv.root.extent.little * ISO9660_SECTOR_SIZE, *root_size);
}

static struct iso9660_context *iso9660_get_context(struct volume *vol) {
    struct iso9660_contexts_node *current = contexts;
    while (current) {
        if (current->context.vol.drive == vol->drive)
            return &current->context;
        current = current->next;
    }

    // The context is not cached at this point
    struct iso9660_contexts_node *node = ext_mem_alloc(sizeof(struct iso9660_contexts_node));
    node->context.vol = *vol;
    iso9660_cache_root(vol, &node->context.root, &node->context.root_size);

    node->next = contexts;
    contexts = node;
    return &node->context;
}

static int iso9660_strcmp(const char *a, const char *b, size_t size) {
    while (size--) {
        char ca = *a++;
        char cb = *b++;
        if (!(ca == cb || (ca - ('a'-'A')) == cb))
            return 1;
    }

    return 0;
}

static struct iso9660_directory_entry *iso9660_find(void *buffer, uint32_t size, const char *filename) {
    // The file can be either FILENAME or FILENAME;1
    uint32_t len = strlen(filename);
    char finalfile[len + 2];
    strcpy(finalfile, filename);
    finalfile[len + 0] = ';';
    finalfile[len + 1] = '1';

    while (size) {
        struct iso9660_directory_entry *entry = buffer;
        char* entry_filename = (char*)entry + sizeof(struct iso9660_directory_entry);

        if (!entry->length) {
            return NULL;
        } else if (entry->filename_size == len && !iso9660_strcmp(filename, entry_filename, len)) {
            return buffer;
        } else if (entry->filename_size == len+2 && !iso9660_strcmp(finalfile, entry_filename, len+2)) {
            return buffer;
        } else {
            size -= entry->length;
            buffer += entry->length;
        }
    }

    return NULL;
}


// --- Public functions ---
int iso9660_check_signature(struct volume *vol) {
    char buf[6];
    const uint64_t signature = ISO9660_FIRST_VOLUME_DESCRIPTOR * ISO9660_SECTOR_SIZE + 1;
    volume_read(vol, buf, signature, 5);
    buf[5] = '\0';
    return !strcmp(buf, "CD001");
}

int iso9660_open(struct iso9660_file_handle *ret, struct volume *vol, const char *path) {
    ret->context = iso9660_get_context(vol);

    while (*path == '/')
        ++path;

    struct iso9660_directory_entry *current = ret->context->root;
    uint32_t current_size = ret->context->root_size;

    uint32_t next_sector = 0;
    uint32_t next_size = 0;

    char filename[ROCK_RIDGE_MAX_FILENAME];
    while (true) {
        char *aux = filename;
        while (!(*path == '/' || *path == '\0'))
            *aux++ = *path++;
        *aux = '\0';

        struct iso9660_directory_entry *entry = iso9660_find(current, current_size, filename);
        if (!entry)
            return 1;    // Not found :(

        next_sector = entry->extent.little;
        next_size = entry->extent_size.little;

        if (*path++ == '\0')
            break;    // Found :)

        current_size = next_size;
        current = ext_mem_alloc(current_size);
        volume_read(vol, current, next_sector * ISO9660_SECTOR_SIZE, current_size);
    }

    ret->LBA = next_sector;
    ret->size = next_size;
    return 0;
}

int iso9660_read(struct iso9660_file_handle *file, void *buf, uint64_t loc, uint64_t count) {
    volume_read(&file->context->vol, buf, file->LBA * ISO9660_SECTOR_SIZE + loc, count);
    return 0;
}
