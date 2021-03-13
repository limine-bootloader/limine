#if defined (bios)

#include <lib/print.h>
#include <lib/real.h>
#include <pxe/pxe.h>
#include <lib/libc.h>
#include <lib/blib.h>
#include <mm/pmm.h>

void set_pxe_fp(uint32_t fp);

struct volume *pxe_bind_volume(void) {
    struct volume *volume = ext_mem_alloc(sizeof(struct volume));

    volume->pxe = true;

    return volume;
}

void pxe_init(void) {
    //pxe installation check
    struct rm_regs r = { 0 };
    r.ebx = 0;
    r.ecx = 0;
    r.eax = 0x5650;
    r.es = 0;

    rm_int(0x1a, &r, &r);
    if ((r.eax & 0xffff) != 0x564e) {
        panic("PXE installation check failed");
    }

    struct pxenv* pxenv = { 0 };

    pxenv = (struct pxenv*)((r.es << 4) + (r.ebx & 0xffff));
    if (memcmp(pxenv->signature, PXE_SIGNATURE, sizeof(pxenv->signature)) != 0) {
        panic("PXENV structure signature corrupted");
    }

    if (pxenv->version < 0x201) {
        //we won't support pxe < 2.1, grub does this too and it seems to work fine
        panic("pxe version too old");
    }

    struct bangpxe* bangpxe = (struct bangpxe*)((((pxenv->pxe_ptr & 0xffff0000) >> 16) << 4) + (pxenv->pxe_ptr & 0xffff));

    if (memcmp(bangpxe->signature, PXE_BANGPXE_SIGNATURE,
            sizeof(bangpxe->signature))
        != 0) {
        panic("!pxe signature corrupted");
    }
    set_pxe_fp(bangpxe->rm_entry);
    print("Successfully initialized pxe");
}

#endif
