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

bool parse_resolution(size_t *width, size_t *height, size_t *bpp, const char *buf) {
    size_t res[3] = {0};

    const char *first = buf;
    for (size_t i = 0; i < 3; i++) {
        const char *last;
        size_t x = strtoui(first, &last, 10);
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

    *width = res[0], *height = res[1];
    if (bpp != NULL)
        *bpp = res[2];

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

    EFI_MEMORY_DESCRIPTOR *efi_copy = ext_mem_alloc(256 * efi_desc_size);
    size_t efi_copy_i = 0;

    for (size_t i = 0; i < entry_count; i++) {
        EFI_MEMORY_DESCRIPTOR *orig_entry = (void *)efi_mmap + i * efi_desc_size;
        EFI_MEMORY_DESCRIPTOR *new_entry = (void *)efi_copy + efi_copy_i * efi_desc_size;

        memcpy(new_entry, orig_entry, efi_desc_size);

        uint64_t base = orig_entry->PhysicalStart;
        uint64_t length = orig_entry->NumberOfPages * 4096;
        uint64_t top = base + length;

        // Find for a match in the untouched memory map
        for (size_t j = 0; j < untouched_memmap_entries; j++) {
            if (untouched_memmap[j].type != MEMMAP_USABLE)
                continue;

            if (untouched_memmap[j].base >= base && untouched_memmap[j].base < top) {
                if (untouched_memmap[j].base > base) {
                    new_entry->NumberOfPages = (untouched_memmap[j].base - base) / 4096;

                    efi_copy_i++;
                    if (efi_copy_i == 256) {
                        panic("efi: New memory map exhausted");
                    }
                    new_entry = (void *)efi_copy + efi_copy_i * efi_desc_size;
                    memcpy(new_entry, orig_entry, efi_desc_size);

                    new_entry->NumberOfPages -= (untouched_memmap[j].base - base) / 4096;
                    new_entry->PhysicalStart = untouched_memmap[j].base;
                    new_entry->VirtualStart = new_entry->PhysicalStart;

                    base = new_entry->PhysicalStart;
                    length = new_entry->NumberOfPages * 4096;
                    top = base + length;
                }

                if (length < untouched_memmap[j].length) {
                    panic("efi: Memory map corruption");
                }

                new_entry->Type = EfiConventionalMemory;

                if (length == untouched_memmap[j].length) {
                    // It's a perfect match!
                    break;
                }

                new_entry->NumberOfPages = untouched_memmap[j].length / 4096;

                efi_copy_i++;
                if (efi_copy_i == 256) {
                    panic("efi: New memory map exhausted");
                }
                new_entry = (void *)efi_copy + efi_copy_i * efi_desc_size;
                memcpy(new_entry, orig_entry, efi_desc_size);

                new_entry->NumberOfPages = (length - untouched_memmap[j].length) / 4096;
                new_entry->PhysicalStart = base + untouched_memmap[j].length;
                new_entry->VirtualStart = new_entry->PhysicalStart;

                break;
            }
        }

        efi_copy_i++;
        if (efi_copy_i == 256) {
            panic("efi: New memory map exhausted");
        }
    }

    efi_mmap = efi_copy;
    efi_mmap_size = efi_copy_i * efi_desc_size;

    efi_boot_services_exited = true;

    printv("efi: Exited boot services.\n");

    return true;

fail:
    panic("efi: Failed to exit boot services");
}

#endif
