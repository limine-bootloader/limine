#if defined(bios)

#include <stdint.h>
#include <stddef.h>
#include <drivers/disk.h>
#include <lib/libc.h>
#include <lib/real.h>
#include <lib/blib.h>
#include <lib/print.h>
#include <mm/pmm.h>

#define BLOCK_SIZE_IN_SECTORS 16
#define BLOCK_SIZE (sector_size * BLOCK_SIZE_IN_SECTORS)

#define CACHE_INVALID (~((uint64_t)0))

static uint8_t *cache        = NULL;
static uint64_t cached_block = CACHE_INVALID;

struct dap {
    uint16_t size;
    uint16_t count;
    uint16_t offset;
    uint16_t segment;
    uint64_t lba;
};

static struct dap *dap = NULL;

static int cache_block(int drive, uint64_t block, int sector_size) {
    if (block == cached_block)
        return 0;

    if (!dap) {
        dap = conv_mem_alloc(sizeof(struct dap));
        dap->size  = 16;
        dap->count = BLOCK_SIZE_IN_SECTORS;
    }

    if (!cache)
        cache = conv_mem_alloc_aligned(BLOCK_SIZE, 16);

    dap->segment = rm_seg(cache);
    dap->offset  = rm_off(cache);
    dap->lba     = block * BLOCK_SIZE_IN_SECTORS;

    struct rm_regs r = {0};
    r.eax = 0x4200;
    r.edx = drive;
    r.esi = (uint32_t)rm_off(dap);
    r.ds  = rm_seg(dap);

    rm_int(0x13, &r, &r);

    if (r.eflags & EFLAGS_CF) {
        int ah = (r.eax >> 8) & 0xff;
        panic("Disk error %x. Drive %x, LBA %x.", ah, drive, dap->lba);
    }

    cached_block = block;

    return 0;
}

int disk_get_sector_size(int drive) {
    struct rm_regs r = {0};
    struct bios_drive_params drive_params;

    r.eax = 0x4800;
    r.edx = drive;
    r.ds  = rm_seg(&drive_params);
    r.esi = rm_off(&drive_params);

    drive_params.buf_size = sizeof(struct bios_drive_params);

    rm_int(0x13, &r, &r);

    if (r.eflags & EFLAGS_CF) {
        int ah = (r.eax >> 8) & 0xff;
        panic("Disk error %x. Drive %x.", ah, drive);
    }

    return drive_params.bytes_per_sect;
}

int disk_read(int drive, void *buffer, uint64_t loc, uint64_t count) {
    int sector_size = disk_get_sector_size(drive);

    uint64_t progress = 0;
    while (progress < count) {
        uint64_t block = (loc + progress) / BLOCK_SIZE;

        int ret;
        if ((ret = cache_block(drive, block, sector_size)))
            return ret;

        uint64_t chunk = count - progress;
        uint64_t offset = (loc + progress) % BLOCK_SIZE;
        if (chunk > BLOCK_SIZE - offset)
            chunk = BLOCK_SIZE - offset;

        memcpy(buffer + progress, &cache[offset], chunk);
        progress += chunk;
    }

    return 0;
}

size_t disk_create_index(struct volume **ret) {
    struct volume *volume_index;
    size_t volume_count = 0, volume_index_i = 0;

    for (uint8_t drive = 0x80; drive; drive++) {
        struct rm_regs r = {0};
        struct bios_drive_params drive_params;

        r.eax = 0x4800;
        r.edx = drive;
        r.ds  = rm_seg(&drive_params);
        r.esi = rm_off(&drive_params);

        drive_params.buf_size = sizeof(struct bios_drive_params);

        rm_int(0x13, &r, &r);

        if (r.eflags & EFLAGS_CF)
            continue;

        print("Found BIOS drive %x\n", drive);
        print(" ... %X total %u-byte sectors\n",
              drive_params.lba_count, drive_params.bytes_per_sect);

        volume_count++;

        struct volume block;

        block.drive = drive;
        block.sector_size = drive_params.bytes_per_sect;
        block.first_sect = 0;
        block.sect_count = drive_params.lba_count;

        for (int part = 0; ; part++) {
            struct volume p;
            int ret = part_get(&p, &block, part);

            if (ret == END_OF_TABLE || ret == INVALID_TABLE)
                break;
            if (ret == NO_PARTITION)
                continue;

            volume_count++;
        }
    }

    volume_index = ext_mem_alloc(sizeof(struct volume) * volume_count);

    for (uint8_t drive = 0x80; drive; drive++) {
        struct rm_regs r = {0};
        struct bios_drive_params drive_params;

        r.eax = 0x4800;
        r.edx = drive;
        r.ds  = rm_seg(&drive_params);
        r.esi = rm_off(&drive_params);

        drive_params.buf_size = sizeof(struct bios_drive_params);

        rm_int(0x13, &r, &r);

        if (r.eflags & EFLAGS_CF)
            continue;

        struct volume *block = &volume_index[volume_index_i++];

        block->drive = drive;
        block->partition = -1;
        block->sector_size = drive_params.bytes_per_sect;
        block->first_sect = 0;
        block->sect_count = drive_params.lba_count;

        if (gpt_get_guid(&block->guid, block)) {
            block->guid_valid = true;
        }

        for (int part = 0; ; part++) {
            struct volume p;
            int ret = part_get(&p, block, part);

            if (ret == END_OF_TABLE || ret == INVALID_TABLE)
                break;
            if (ret == NO_PARTITION)
                continue;

            volume_index[volume_index_i++] = p;
        }
    }

    *ret = volume_index;
    return volume_count;
}

#endif
