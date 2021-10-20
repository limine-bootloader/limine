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
    char name[];
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
} __attribute__((packed));

struct iso9660_primary_volume {
    struct iso9660_volume_descriptor volume_descriptor;

    union {
        struct {
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
        } __attribute__((packed));

        uint8_t padding[2041];
    };
} __attribute__((packed));


// --- Implementation ---
struct iso9660_contexts_node {
    struct iso9660_context context;
    struct iso9660_contexts_node *next;
};

static struct iso9660_contexts_node *contexts = NULL;

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
        if (current->context.vol == vol)
            return &current->context;
        current = current->next;
    }

    // The context is not cached at this point
    struct iso9660_contexts_node *node = ext_mem_alloc(sizeof(struct iso9660_contexts_node));
    node->context.vol = vol;
    iso9660_cache_root(vol, &node->context.root, &node->context.root_size);

    node->next = contexts;
    contexts = node;
    return &node->context;
}

static bool load_name(char *buf, struct iso9660_directory_entry *entry) {
    unsigned char* sysarea = ((unsigned char*)entry) + sizeof(struct iso9660_directory_entry) + entry->filename_size;
    int sysarea_len = entry->length - sizeof(struct iso9660_directory_entry) - entry->filename_size;
    if ((entry->filename_size & 0x1) == 0) {
        sysarea++;
        sysarea_len--;
    }

    int rrnamelen = 0;
    while ((sysarea_len >= 4) && ((sysarea[3] == 1) || (sysarea[2] == 2))) {
        if (sysarea[0] == 'N' && sysarea[1] == 'M') {
            rrnamelen = sysarea[2] - 5;
            break;
        }
        sysarea_len -= sysarea[2];
        sysarea += sysarea[2];
    }

    size_t name_len = 0;
    if (rrnamelen) {
        /* rock ridge naming scheme */
        name_len = rrnamelen;
        memcpy(buf, sysarea + 5, name_len);
        buf[name_len] = 0;
        return true;
    } else {
        name_len = entry->filename_size;
        size_t j;
        for (j = 0; j < name_len; j++) {
            if (entry->name[j] == ';')
                break;
            if (entry->name[j] == '.' && entry->name[j+1] == ';')
                break;
            buf[j] = entry->name[j];
        }
        buf[j] = 0;
        return false;
    }
}

static struct iso9660_directory_entry *iso9660_find(void *buffer, uint32_t size, const char *filename) {
    while (size) {
        struct iso9660_directory_entry *entry = buffer;

        if (entry->length == 0) {
            if (size <= ISO9660_SECTOR_SIZE)
                return NULL;
            size_t prev_size = size;
            size = ALIGN_DOWN(size, ISO9660_SECTOR_SIZE);
            buffer += prev_size - size;
            continue;
        }

        char entry_filename[128];
        bool rr = load_name(entry_filename, entry);

        if (rr) {
            if (strcmp(filename, entry_filename) == 0) {
                return buffer;
            }
        } else {
            if (strcasecmp(filename, entry_filename) == 0) {
                return buffer;
            }
        }

        size -= entry->length;
        buffer += entry->length;
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

bool iso9660_open(struct iso9660_file_handle *ret, struct volume *vol, const char *path) {
    ret->context = iso9660_get_context(vol);

    while (*path == '/')
        ++path;

    struct iso9660_directory_entry *current = ret->context->root;
    uint32_t current_size = ret->context->root_size;

    bool first = true;

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
            return false;    // Not found :(

        next_sector = entry->extent.little;
        next_size = entry->extent_size.little;

        if (*path++ == '\0')
            break;    // Found :)

        if (!first) {
            pmm_free(current, current_size);
        }

        current_size = next_size;
        current = ext_mem_alloc(current_size);

        first = false;

        volume_read(vol, current, next_sector * ISO9660_SECTOR_SIZE, current_size);
    }

    ret->LBA = next_sector;
    ret->size = next_size;
    return true;
}

void iso9660_read(struct iso9660_file_handle *file, void *buf, uint64_t loc, uint64_t count) {
    volume_read(file->context->vol, buf, file->LBA * ISO9660_SECTOR_SIZE + loc, count);
}

void iso9660_close(struct iso9660_file_handle *file) {
    pmm_free(file, sizeof(struct iso9660_file_handle));
}
