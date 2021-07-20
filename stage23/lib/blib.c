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

#if uefi == 1
EFI_SYSTEM_TABLE *gST;
EFI_BOOT_SERVICES *gBS;
EFI_RUNTIME_SERVICES *gRT;
EFI_HANDLE efi_image_handle;
EFI_MEMORY_DESCRIPTOR *efi_mmap = NULL;
UINTN efi_mmap_size = 0, efi_desc_size = 0, efi_desc_ver = 0;
#endif

bool verbose = false;

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

size_t get_trailing_zeros(uint64_t val) {
    for (size_t i = 0; i < 64; i++) {
        if ((val & 1) != 0) {
            return i;
        }
        val >>= 1;
    }
    return 64;
}

#if uefi == 1

bool efi_boot_services_exited = false;

bool efi_exit_boot_services(void) {
    EFI_STATUS status;

    EFI_MEMORY_DESCRIPTOR tmp_mmap[1];
    efi_mmap_size = sizeof(tmp_mmap);
    UINTN mmap_key = 0;

    uefi_call_wrapper(gBS->GetMemoryMap, 5,
        &efi_mmap_size, tmp_mmap, &mmap_key, &efi_desc_size, &efi_desc_ver);

    efi_mmap_size += 4096;

    status = uefi_call_wrapper(gBS->FreePool, 1, efi_mmap);
    if (status)
        goto fail;

    status = uefi_call_wrapper(gBS->AllocatePool, 3,
        EfiLoaderData, efi_mmap_size, (void **)&efi_mmap);
    if (status)
        goto fail;

    status = uefi_call_wrapper(gBS->GetMemoryMap, 5,
        &efi_mmap_size, efi_mmap, &mmap_key, &efi_desc_size, &efi_desc_ver);
    if (status)
        goto fail;

    // Be gone, UEFI!
    status = uefi_call_wrapper(gBS->ExitBootServices, 2, efi_image_handle, mmap_key);

    asm volatile ("cli" ::: "memory");

    if (status)
        goto fail;

    pmm_reclaim_uefi_mem();

    // Go through new EFI memmap and free up bootloader entries
    size_t entry_count = efi_mmap_size / efi_desc_size;

    for (size_t i = 0; i < entry_count; i++) {
        EFI_MEMORY_DESCRIPTOR *entry = (void *)efi_mmap + i * efi_desc_size;

        uint64_t base = entry->PhysicalStart;
        uint64_t length = entry->NumberOfPages * 4096;

        // Find for a match in the untouched memory map
        for (size_t j = 0; j < untouched_memmap_entries; j++) {
            if (untouched_memmap[j].type != MEMMAP_USABLE)
                continue;

            if (untouched_memmap[j].base == base && untouched_memmap[j].length == length) {
                // It's a match!
                entry->Type = EfiConventionalMemory;
                break;
            }
        }
    }

    efi_boot_services_exited = true;

    printv("efi: Exited boot services.\n");

    return true;

fail:
    panic("efi: Failed to exit boot services");
}

#endif
