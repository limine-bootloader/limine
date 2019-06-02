#include <stdint.h>
#include <stddef.h>
#include <lib/libc.h>
#include <drivers/disk.h>
#include <lib/real.h>
#include <lib/print.h>

#define SECTOR_SIZE 512

static uint64_t last_sector = -1;
static uint8_t sector_buf[512];

static struct {
    uint16_t size;
    uint16_t count;
    uint16_t offset;
    uint16_t segment;
    uint64_t lba;
} dap = { 16, 1, 0, 0, 0 };

int read_sector(int drive, void *buffer, uint64_t lba, size_t count) {
    dap.offset = (uint16_t)(size_t)sector_buf;

    while (count--) {
        dap.lba = lba;
        struct rm_regs r = {0};
        r.eax = 0x4200;
        r.edx = drive;
        r.esi = (unsigned int)&dap;
        rm_int(0x13, &r, &r);

        int ah = (r.eax >> 8) & 0xFF;
        if (ah) {
            print("Disk error %x\n", ah);
            return ah;
        }

        memcpy(buffer, sector_buf, SECTOR_SIZE);
        last_sector = lba++;

        buffer += SECTOR_SIZE;
    }

    return 0;
}

int read(int drive, void *buffer, int offset, size_t count) {
    int res;
    
    if (last_sector == (uint64_t)-1)
        if ((res = read_sector(drive, sector_buf, 0, 1)))
            return res;
    
    uint64_t cur_sector = last_sector + (offset / SECTOR_SIZE);
    offset %= SECTOR_SIZE;

    size_t sectors_count = 1 + ((offset + count) / SECTOR_SIZE);

    while (sectors_count--) {
        if (cur_sector != last_sector)
            if ((res = read_sector(drive, sector_buf, cur_sector, 1)))
                return res;

        size_t limited_count = ((offset + count) > SECTOR_SIZE) ? (size_t)(SECTOR_SIZE - offset) : count;
        memcpy(buffer, &sector_buf[offset], limited_count);

        offset = (offset + limited_count) % SECTOR_SIZE;
        buffer += limited_count;
        cur_sector++;
    }

    return 0;
}
