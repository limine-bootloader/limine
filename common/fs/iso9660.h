#ifndef __FS__ISO9660_H__
#define __FS__ISO9660_H__

#include <stdint.h>
#include <lib/part.h>

#define ISO9660_SECTOR_SIZE (2 << 10)

struct iso9660_context {
    struct volume *vol;
    void* root;
    uint32_t root_size;
};

struct iso9660_file_handle {
    struct iso9660_context *context;
    uint32_t LBA;
    uint32_t size;
};

int iso9660_check_signature(struct volume *vol);
bool iso9660_open(struct iso9660_file_handle *ret, struct volume *vol, const char *path);
void iso9660_read(struct iso9660_file_handle *file, void *buf, uint64_t loc, uint64_t count);
void iso9660_close(struct iso9660_file_handle *file);

#endif
