#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <lib/libc.h>
#include <lib/blib.h>
#include <lib/print.h>
#include <lib/trace.h>
#include <lib/real.h>
#include <fs/file.h>
#include <mm/pmm.h>

#if defined (uefi)
EFI_SYSTEM_TABLE *gST;
EFI_BOOT_SERVICES *gBS;
EFI_RUNTIME_SERVICES *gRT;
EFI_HANDLE efi_image_handle;
#endif

bool parse_resolution(int *width, int *height, int *bpp, const char *buf) {
    int res[3] = {0};

    const char *first = buf;
    for (int i = 0; i < 3; i++) {
        const char *last;
        int x = strtoui(first, &last, 10);
        if (first == last)
            break;
        res[i] = x;
        if (*last == 0)
            break;
        first = last + 1;
    }

    if (res[0] == 0 || res[1] == 0)
        return false;

    if (res[2] == 0)
        res[2] = 32;

    *width = res[0], *height = res[1], *bpp = res[2];

    return true;
}

// This integer sqrt implementation has been adapted from:
// https://stackoverflow.com/questions/1100090/looking-for-an-efficient-integer-square-root-algorithm-for-arm-thumb2
uint64_t sqrt(uint64_t a_nInput) {
    uint64_t op  = a_nInput;
    uint64_t res = 0;
    uint64_t one = (uint64_t)1 << 62;

    // "one" starts at the highest power of four <= than the argument.
    while (one > op) {
        one >>= 2;
    }

    while (one != 0) {
        if (op >= res + one) {
            op = op - (res + one);
            res = res +  2 * one;
        }
        res >>= 1;
        one >>= 2;
    }

    return res;
}

#if defined (uefi)

bool efi_boot_services_exited = false;

bool efi_exit_boot_services(void) {
    EFI_MEMORY_DESCRIPTOR tmp_mmap[1];
    UINTN mmap_size = sizeof(tmp_mmap);
    UINTN mmap_key = 0, desc_size = 0, desc_ver = 0;

    uefi_call_wrapper(gBS->GetMemoryMap, 5,
        &mmap_size, tmp_mmap, &mmap_key, &desc_size, &desc_ver);

    EFI_MEMORY_DESCRIPTOR *efi_mmap = ext_mem_alloc(mmap_size);

    uefi_call_wrapper(gBS->GetMemoryMap, 5,
        &mmap_size, efi_mmap, &mmap_key, &desc_size, &desc_ver);

    uefi_call_wrapper(gBS->ExitBootServices, 2, efi_image_handle, mmap_key);

    pmm_mmap_efi2ours(efi_mmap, desc_size, mmap_size / desc_size);

    efi_boot_services_exited = true;

    print("efi: Exited boot services.\n");

    return true;
}

#endif
