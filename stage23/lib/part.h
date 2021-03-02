#ifndef __LIB__PART_H__
#define __LIB__PART_H__

#include <stdint.h>
#include <stdbool.h>
#include <lib/guid.h>
#if defined (uefi)
#  include <efi.h>
#endif

#define NO_PARTITION  (-1)
#define INVALID_TABLE (-2)
#define END_OF_TABLE  (-3)

#if defined (bios)
typedef int drive_t;
#elif defined (uefi)
typedef EFI_HANDLE drive_t;
#endif

struct volume {
    drive_t drive;
    int partition;
    int sector_size;
    uint64_t first_sect;
    uint64_t sect_count;
    bool guid_valid;
    struct guid guid;
    bool part_guid_valid;
    struct guid part_guid;
};

void volume_create_index(void);

bool gpt_get_guid(struct guid *guid, struct volume *volume);

int part_get(struct volume *part, struct volume *volume, int partition);
bool volume_get_by_guid(struct volume *part, struct guid *guid);
bool volume_get_by_coord(struct volume *part, drive_t drive, int partition);

int volume_read(struct volume *part, void *buffer, uint64_t loc, uint64_t count);

#endif
