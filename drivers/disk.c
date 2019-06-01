#include <stdint.h>
#include <stddef.h>
#include <lib/libc.h>
#include <drivers/disk.h>
#include <lib/real.h>
#include <lib/print.h>

#define SECTOR_SIZE 512

static uint8_t sector_buf[512];

static struct {
    uint16_t size;
    uint16_t count;
    uint16_t offset;
    uint16_t segment;
    uint64_t lba;
} dap = { 16, 1, 0, 0, 0 };

int read_sector(int drive, int lba, int count, void *buffer) {
    dap.offset = (uint16_t)(size_t)sector_buf;

    while (count--) {
        dap.lba = lba++;
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

        buffer += SECTOR_SIZE;
    }

    return 0;
}
