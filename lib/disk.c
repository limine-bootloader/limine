#include <stdint.h>
#include <stddef.h>
#include <lib/disk.h>
#include <lib/real.h>

struct dap
{
    unsigned short size; // 16
    unsigned short count;
    unsigned short offset;
    unsigned short segment;
    unsigned long long lba;
};

struct dap dap = { 16, 0, 0, 0, 0 };
struct rm_regs reg = { 0 };

inline void setup_dap(int lba, int count, int off, int seg)
{
    dap.lba = lba;
    dap.count = count;
    dap.offset = off;
    dap.segment = seg;
}

int read_sector(int drive, int lba, int count, unsigned char *buffer)
{
    while (count > 0)
    {
        setup_dap(lba, (count > 128) ? 128 : count, rm_off(buffer), rm_seg(buffer));
        struct rm_regs r;
        r.eax = 0x4200;
        r.edx = drive;
        r.esi = (unsigned int)&dap;
        rm_int(0x13, &r, &r);

        count -= 128;
        buffer += 128 * 512;
    }
        
    return 0;
}

int write_sector(int drive, int lba, int count, unsigned char *buffer)
{
    while (count > 0)
    {
        setup_dap(lba, (count > 128) ? 128 : count, rm_off(buffer), rm_seg(buffer));
        struct rm_regs r;
        r.eax = 0x4300;
        r.edx = drive;
        r.esi = (unsigned int)&dap;
        rm_int(0x13, &r, &r);

        count -= 128;
        buffer += 128 * 512;
    }

    return 0;
}
