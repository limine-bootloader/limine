#ifndef __LIB__PART_H__
#define __LIB__PART_H__

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <lib/guid.h>
#if defined (uefi)
#  include <efi.h>
#endif

#define NO_PARTITION  (-1)
#define INVALID_TABLE (-2)
#define END_OF_TABLE  (-3)

struct volume {
#if defined (uefi)
    EFI_HANDLE efi_handle;
#endif

    bool pxe;

    int drive;
    int partition;
    int sector_size;
    struct volume *backing_dev;

    int max_partition;

    int cache_status;
    uint8_t *cache;
    uint64_t cached_block;

    uint64_t first_sect;
    uint64_t sect_count;

    bool guid_valid;
    struct guid guid;
    bool part_guid_valid;
    struct guid part_guid;
};

extern struct volume **volume_index;
extern size_t volume_index_i;

bool gpt_get_guid(struct guid *guid, struct volume *volume);

int part_get(struct volume *part, struct volume *volume, int partition);

struct volume *volume_get_by_guid(struct guid *guid);
struct volume *volume_get_by_coord(int drive, int partition);

bool volume_read(struct volume *part, void *buffer, uint64_t loc, uint64_t count);

#define volume_iterate_parts(_VOLUME_, _BODY_) ({   \
    struct volume *_VOLUME = _VOLUME_;   \
    if (_VOLUME->pxe) { \
        do { \
            struct volume *_PART = _VOLUME; \
            _BODY_ \
        } while (0); \
    } else { \
        while (_VOLUME->backing_dev != NULL) { \
            _VOLUME = _VOLUME->backing_dev; \
        } \
 \
        int _PART_CNT = -1; \
        for (size_t _PARTNO = -1; ; _PARTNO++) { \
            if (_PART_CNT > _VOLUME->max_partition) \
                break; \
 \
            struct volume *_PART = volume_get_by_coord(_VOLUME->drive, _PARTNO); \
            if (_PART == NULL) \
                continue; \
 \
            _PART_CNT++; \
 \
            _BODY_ \
        } \
    } \
})

#endif
