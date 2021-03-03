#include <stdint.h>
#include <stddef.h>
#include <drivers/disk.h>
#include <lib/libc.h>
#include <lib/real.h>
#include <lib/blib.h>
#include <lib/print.h>
#include <mm/pmm.h>

struct dap {
    uint16_t size;
    uint16_t count;
    uint16_t offset;
    uint16_t segment;
    uint64_t lba;
};

static struct dap *dap = NULL;

#define XFER_BUF_SIZE 16384
static void *xfer_buf = NULL;

bool disk_read_sectors(struct volume *volume, void *buf, uint64_t block, size_t count) {
    if (count * volume->sector_size > XFER_BUF_SIZE)
        panic("XFER");

    if (xfer_buf == NULL)
        xfer_buf = conv_mem_alloc_aligned(XFER_BUF_SIZE, 16);

    if (dap == NULL) {
        dap = conv_mem_alloc(sizeof(struct dap));
        dap->size  = 16;
    }

    dap->count = count;

    dap->segment = rm_seg(xfer_buf);
    dap->offset  = rm_off(xfer_buf);
    dap->lba     = block;

    struct rm_regs r = {0};
    r.eax = 0x4200;
    r.edx = volume->drive;
    r.esi = (uint32_t)rm_off(dap);
    r.ds  = rm_seg(dap);

    rm_int(0x13, &r, &r);

    if (r.eflags & EFLAGS_CF) {
        int ah = (r.eax >> 8) & 0xff;
        panic("Disk error %x. Drive %x, LBA %x.", ah, volume->drive, dap->lba);
    }

    memcpy(buf, xfer_buf, count * volume->sector_size);

    return true;
}
