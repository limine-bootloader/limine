#include <stdint.h>
#include <stddef.h>
#include <lib/libc.h>
#include <lib/disk.h>
#include <lib/real.h>
#include <lib/print.h>

#define DAP_SIZE 16
#define SECTOR_SIZE 512

static struct {
    unsigned short size;
    unsigned short count;
    unsigned short offset;
    unsigned short segment;
    unsigned long long lba;
} dap = { DAP_SIZE, 0, 0, 0, 0 };

static unsigned char sector_buf[512];

static inline void setup_dap(int lba, int count, int off, int seg) {
    dap.lba = lba;
    dap.count = count;
    dap.offset = off;
    dap.segment = seg;
}

static int check_results(struct rm_regs *out) {
    int ah = (out->eax >> 8) & 0xFF;
    
    if (ah) 
        print("Disk error %x\n", ah);

    return ah;
} 

int read_sector(int drive, int lba, int count, unsigned char *buffer) {
    while (count--) {
        setup_dap(lba++, 1, sector_buf, 0);
        struct rm_regs r = {0};
        r.eax = 0x4200;
        r.edx = drive;
        r.esi = (unsigned int)&dap;
        rm_int(0x13, &r, &r);
        if (check_results(&r)) 
            return (r.eax >> 8) & 0xFF;

        memcpy(buffer, sector_buf, SECTOR_SIZE);
        
        buffer += SECTOR_SIZE;
    }
        
    return 0;
}

int write_sector(int drive, int lba, int count, const unsigned char *buffer) {
    while (count--) {
        memcpy(sector_buf, buffer, SECTOR_SIZE);

        setup_dap(lba++, 1, sector_buf, 0);
        struct rm_regs r = {0};
        r.eax = 0x4300;
        r.edx = drive;
        r.esi = (unsigned int)&dap;
        rm_int(0x13, &r, &r);
        if (check_results(&r)) 
            return (r.eax >> 8) & 0xFF;

        buffer += SECTOR_SIZE;
    }

    return 0;
}
