#include <stdint.h>
#include <stddef.h>
#include <lib/libc.h>
#include <drivers/disk.h>
#include <lib/real.h>
#include <lib/print.h>
#include <lib/mbr.h>

#define SECTOR_SIZE 512

static uint64_t cached_sector = -1;
static uint8_t sector_buf[512];

static struct {
    uint16_t size;
    uint16_t count;
    uint16_t offset;
    uint16_t segment;
    uint64_t lba;
} dap = { 16, 1, 0, 0, 0 };

static int cache_sector(int drive, uint64_t lba) {
    if (lba == cached_sector)
        return 0;

    dap.offset = (uint16_t)(size_t)sector_buf;
    dap.lba = lba;

    struct rm_regs r = {0};
    r.eax = 0x4200;
    r.edx = drive;
    r.esi = (uint32_t)&dap;

    rm_int(0x13, &r, &r);

    if (r.eflags & EFLAGS_CF) {
        int ah = (r.eax >> 8) & 0xff;
        print("Disk error %x. Drive %x, LBA %x.\n", ah, drive, lba);
        return ah;
    }

    cached_sector = lba;

    return 0;
}

int read_sector(int drive, void *buffer, uint64_t lba, uint64_t count) {
    while (count--) {
        int ret;
        if ((ret = cache_sector(drive, lba++)))
            return ret;

        memcpy(buffer, sector_buf, SECTOR_SIZE);

        buffer += SECTOR_SIZE;
    }

    return 0;
}

int read(int drive, void *buffer, uint64_t loc, uint64_t count) {
    uint64_t progress = 0;
    while (progress < count) {
        uint64_t sect = (loc + progress) / SECTOR_SIZE;

        int ret;
        if ((ret = cache_sector(drive, sect)))
            return ret;

        uint64_t chunk = count - progress;
        uint64_t offset = (loc + progress) % SECTOR_SIZE;
        if (chunk > SECTOR_SIZE - offset)
            chunk = SECTOR_SIZE - offset;

        memcpy(buffer + progress, &sector_buf[offset], chunk);
        progress += chunk;
    }

    return 0;
}

int read_partition(int drive, int partition, void *buffer, uint64_t loc, uint64_t count) {
    struct mbr_part part;
    int ret = mbr_get_part(&part, drive, partition);
    if (ret) {
        return ret;
    }

    return read(drive, buffer, loc + (part.first_sect * 512), count);
}
