#include <stdint.h>
#include <stddef.h>
#include <lib/disk.h>
#include <lib/real.h>
#include <lib/print.h>

#define DAP_SIZE 16
#define MAX_COUNT 127
#define SECTOR_SIZE(drv) ((drv == 0xE0) ? 2048 : 512) 

static struct dap
{
    unsigned short size;
    unsigned short count;
    unsigned short offset;
    unsigned short segment;
    unsigned long long lba;
};

static struct dap dap = { DAP_SIZE, 0, 0, 0, 0 };

static const unsigned char *disk_errstr[] = 
{
    "No error",
    "Invalid command",
    "Cannot find address mark",
    "Attempted write on write protected disk", 
    "Sector not found",
    "Reset failed",
    "Disk change line 'active'",
    "Drive parameter activity failed",
    "DMA overrun", 
    "Attempt to DMA over 64kb boundary", 
    "Bad sector detected", 
    "Bad cylinder (track) detected", 
    "Media type not found", 
    "Invalid number of sectors", 
    "Control data address mark detected", 
    "DMA out of range",
    "CRC/ECC data error", 
    "ECC corrected data error" 
} 

static inline void setup_dap(int lba, int count, int off, int seg)
{
    dap.lba = lba;
    dap.count = count;
    dap.offset = off;
    dap.segment = seg;
}

static int check_results(struct rm_regs out)
{
    int ah = (out.eax >> 8) & 0xFF;
    if (ah) 
        print("Disk error ‰x (%s)", ah, (ah < 0x12) ? errstr[ah] : "");

    return ah;
} 

int read_sector(int drive, int lba, int count, unsigned char *buffer)
{
    while (count > 0)
    {
        setup_dap(lba, (count > MAX_COUNT) ? MAX_COUNT : count, rm_off(buffer), rm_seg(buffer));
        struct rm_regs r = {0};
        r.eax = 0x4200;
        r.edx = drive;
        r.esi = (unsigned int)&dap;
        rm_int(0x13, &r, &r);
        if (check_results(r)) 
            return (r.eax >> 8) & 0xFF;

        count -= MAX_COUNT;
        buffer += MAX_COUNT * SECTOR_SIZE(drive);
    }
        
    return 0;
}

int write_sector(int drive, int lba, int count, unsigned char *buffer)
{
    while (count > 0)
    {
        setup_dap(lba, (count > MAX_COUNT) ? MAX_COUNT : count, rm_off(buffer), rm_seg(buffer));
        struct rm_regs r = {0};
        r.eax = 0x4300;
        r.edx = drive;
        r.esi = (unsigned int)&dap;
        rm_int(0x13, &r, &r);
        if (check_results(r)) 
            return (r.eax >> 8) & 0xFF;

        count -= MAX_COUNT;
        buffer += MAX_COUNT * SECTOR_SIZE(drive);
    }

    return 0;
}
