#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <mm/pmm.h>
#include <sys/e820.h>
#include <lib/acpi.h>
#include <lib/blib.h>
#include <lib/libc.h>
#include <lib/print.h>
#if defined (uefi)
#  include <efi.h>
#endif

#define PAGE_SIZE   4096
#define MEMMAP_MAX_ENTRIES 256

#if defined (bios)
extern symbol bss_end;
#endif

static bool allocations_disallowed = true;
static void sanitise_entries(bool align_entries);

void *conv_mem_alloc(size_t count) {
    static uintptr_t base = 4096;

    if (allocations_disallowed)
        panic("Memory allocations disallowed");

    count = ALIGN_UP(count, 4096);

    for (;;) {
        if (base + count > 0x100000)
            panic("Conventional memory allocation failed");

        if (memmap_alloc_range(base, count, MEMMAP_BOOTLOADER_RECLAIMABLE, true, false, false, false)) {
            void *ret = (void *)base;
            // Zero out allocated space
            memset(ret, 0, count);
            base += count;

            sanitise_entries(false);

            return ret;
        }

        base += 4096;
    }
}

struct e820_entry_t memmap[MEMMAP_MAX_ENTRIES];
size_t memmap_entries = 0;

static const char *memmap_type(uint32_t type) {
    switch (type) {
        case MEMMAP_USABLE:
            return "Usable RAM";
        case MEMMAP_RESERVED:
            return "Reserved";
        case MEMMAP_ACPI_RECLAIMABLE:
            return "ACPI reclaimable";
        case MEMMAP_ACPI_NVS:
            return "ACPI NVS";
        case MEMMAP_BAD_MEMORY:
            return "Bad memory";
        case MEMMAP_BOOTLOADER_RECLAIMABLE:
            return "Bootloader reclaimable";
        case MEMMAP_KERNEL_AND_MODULES:
            return "Kernel/Modules";
        case MEMMAP_EFI_RECLAIMABLE:
            return "EFI reclaimable";
        default:
            return "???";
    }
}

void print_memmap(struct e820_entry_t *mm, size_t size) {
    for (size_t i = 0; i < size; i++) {
        printv("[%X -> %X] : %X  <%s>\n",
               mm[i].base,
               mm[i].base + mm[i].length,
               mm[i].length,
               memmap_type(mm[i].type));
    }
}

static bool align_entry(uint64_t *base, uint64_t *length) {
    if (*length < PAGE_SIZE)
        return false;

    uint64_t orig_base = *base;

    *base = ALIGN_UP(*base, PAGE_SIZE);

    *length -= (*base - orig_base);
    *length =  ALIGN_DOWN(*length, PAGE_SIZE);

    if (!length)
        return false;

    return true;
}

static void sanitise_entries(bool align_entries) {
    for (size_t i = 0; i < memmap_entries; i++) {
        if (memmap[i].type != MEMMAP_USABLE)
            continue;

        // Check if the entry overlaps other entries
        for (size_t j = 0; j < memmap_entries; j++) {
            if (j == i)
                continue;

            uint64_t base   = memmap[i].base;
            uint64_t length = memmap[i].length;
            uint64_t top    = base + length;

            uint64_t res_base   = memmap[j].base;
            uint64_t res_length = memmap[j].length;
            uint64_t res_top    = res_base + res_length;

            // TODO actually handle splitting off usable chunks
            if ( (res_base >= base && res_base < top)
              && (res_top  >= base && res_top  < top) ) {
                panic("A non-usable e820 entry is inside a usable section.");
            }

            if (res_base >= base && res_base < top) {
                top = res_base;
            }

            if (res_top  >= base && res_top  < top) {
                base = res_top;
            }

            memmap[i].base   = base;
            memmap[i].length = top - base;
        }

        if (!memmap[i].length
         || (align_entries && !align_entry(&memmap[i].base, &memmap[i].length))) {
            // Eradicate from memmap
            for (size_t j = i; j < memmap_entries - 1; j++) {
                memmap[j] = memmap[j+1];
            }
            memmap_entries--;
            i--;
        }
    }

    // Sort the entries
    for (size_t p = 0; p < memmap_entries - 1; p++) {
        uint64_t min = memmap[p].base;
        size_t min_index = p;
        for (size_t i = p; i < memmap_entries; i++) {
            if (memmap[i].base < min) {
                min = memmap[i].base;
                min_index = i;
            }
        }
        struct e820_entry_t min_e = memmap[min_index];
        memmap[min_index] = memmap[p];
        memmap[p] = min_e;
    }

    // Merge contiguous bootloader-reclaimable and usable entries
    for (size_t i = 0; i < memmap_entries - 1; i++) {
        if (memmap[i].type != MEMMAP_BOOTLOADER_RECLAIMABLE
         && memmap[i].type != MEMMAP_USABLE)
            continue;

        if (memmap[i+1].type == memmap[i].type
         && memmap[i+1].base == memmap[i].base + memmap[i].length) {
            memmap[i].length += memmap[i+1].length;

            // Eradicate from memmap
            for (size_t j = i+1; j < memmap_entries - 1; j++) {
                memmap[j] = memmap[j+1];
            }
            memmap_entries--;
            i--;
        }
    }
}

struct e820_entry_t *get_memmap(size_t *entries) {
    sanitise_entries(true);

    *entries = memmap_entries;

    allocations_disallowed = true;

    return memmap;
}

#if defined (bios)
void init_memmap(void) {
    for (size_t i = 0; i < e820_entries; i++) {
        if (memmap_entries == MEMMAP_MAX_ENTRIES) {
            panic("Memory map exhausted.");
        }

        memmap[memmap_entries] = e820_map[i];

        uint64_t top = memmap[memmap_entries].base + memmap[memmap_entries].length;

        if (memmap[memmap_entries].type == MEMMAP_USABLE) {
            if (memmap[memmap_entries].base < 0x1000) {
                if (top <= 0x1000) {
                    continue;
                }

                memmap[memmap_entries].length -= 0x1000 - memmap[memmap_entries].base;
                memmap[memmap_entries].base = 0x1000;
            }

            if (memmap[memmap_entries].base >= EBDA && memmap[memmap_entries].base < 0x100000) {
                if (top <= 0x100000)
                    continue;

                memmap[memmap_entries].length -= 0x100000 - memmap[memmap_entries].base;
                memmap[memmap_entries].base = 0x100000;
            }

            if (top > EBDA && top <= 0x100000) {
                memmap[memmap_entries].length -= top - EBDA;
            }
        }

        memmap_entries++;
    }

    // Allocate bootloader itself
    memmap_alloc_range(4096,
        ALIGN_UP((uintptr_t)bss_end, 4096) - 4096, MEMMAP_BOOTLOADER_RECLAIMABLE, true, true, false, false);

    sanitise_entries(false);

    allocations_disallowed = false;
}
#endif

#if defined (uefi)
void init_memmap(void) {
    EFI_STATUS status;

    EFI_MEMORY_DESCRIPTOR tmp_mmap[1];
    efi_mmap_size = sizeof(tmp_mmap);
    UINTN mmap_key = 0;

    status = uefi_call_wrapper(gBS->GetMemoryMap, 5,
        &efi_mmap_size, tmp_mmap, &mmap_key, &efi_desc_size, &efi_desc_ver);

    efi_mmap_size += 4096;

    status = uefi_call_wrapper(gBS->AllocatePool, 3,
        EfiLoaderData, efi_mmap_size, &efi_mmap);

    status = uefi_call_wrapper(gBS->GetMemoryMap, 5,
        &efi_mmap_size, efi_mmap, &mmap_key, &efi_desc_size, &efi_desc_ver);

    size_t entry_count = efi_mmap_size / efi_desc_size;

    for (size_t i = 0; i < entry_count; i++) {
        EFI_MEMORY_DESCRIPTOR *entry = (void *)efi_mmap + i * efi_desc_size;

        uint32_t our_type;
        switch (entry->Type) {
            case EfiReservedMemoryType:
            case EfiRuntimeServicesCode:
            case EfiRuntimeServicesData:
            case EfiUnusableMemory:
            case EfiMemoryMappedIO:
            case EfiMemoryMappedIOPortSpace:
            case EfiPalCode:
            case EfiLoaderCode:
            case EfiLoaderData:
            default:
                our_type = MEMMAP_RESERVED; break;
            case EfiBootServicesCode:
            case EfiBootServicesData:
                our_type = MEMMAP_EFI_RECLAIMABLE; break;
            case EfiACPIReclaimMemory:
                our_type = MEMMAP_ACPI_RECLAIMABLE; break;
            case EfiACPIMemoryNVS:
                our_type = MEMMAP_ACPI_NVS; break;
            case EfiConventionalMemory:
                our_type = MEMMAP_USABLE; break;
        }

        memmap[memmap_entries].type = our_type;
        memmap[memmap_entries].base = entry->PhysicalStart;
        memmap[memmap_entries].length = entry->NumberOfPages * 4096;

        if (memmap[memmap_entries].base < 0x1000) {
            if (memmap[memmap_entries].base + memmap[memmap_entries].length <= 0x1000) {
                continue;
            }

            memmap[memmap_entries].length -= 0x1000 - memmap[memmap_entries].base;
            memmap[memmap_entries].base = 0x1000;
        }

        memmap_entries++;
    }

    sanitise_entries(false);

    allocations_disallowed = false;

    // Let's leave 64MiB to the firmware
    ext_mem_alloc_type(65536, MEMMAP_EFI_RECLAIMABLE);

    // Now own all the usable entries
    for (size_t i = 0; i < memmap_entries; i++) {
        if (memmap[i].type != MEMMAP_USABLE)
            continue;

        EFI_PHYSICAL_ADDRESS base = memmap[i].base;

        status = uefi_call_wrapper(gBS->AllocatePages, 4,
          AllocateAddress, EfiLoaderData, memmap[i].length / 4096, &base);

        if (status)
            panic("pmm: AllocatePages failure (%x)", status);
    }
}

void pmm_reclaim_uefi_mem(void) {
    for (size_t i = 0; i < memmap_entries; i++) {
        if (memmap[i].type != MEMMAP_EFI_RECLAIMABLE)
            continue;

        memmap[i].type = MEMMAP_USABLE;
    }

    sanitise_entries(false);
}

void pmm_release_uefi_mem(void) {
    EFI_STATUS status;

    for (size_t i = 0; i < memmap_entries; i++) {
        if (memmap[i].type != MEMMAP_USABLE
         && memmap[i].type != MEMMAP_BOOTLOADER_RECLAIMABLE) {
            continue;
        }

        status = uefi_call_wrapper(gBS->FreePages, 2,
                                   memmap[i].base, memmap[i].length / 4096);

        if (status)
            panic("pmm: FreePages failure (%x)", status);
    }

    allocations_disallowed = true;
}
#endif

#if defined (bios)
struct e820_entry_t *get_raw_memmap(size_t *entry_count) {
    size_t mmap_count = e820_entries;
    size_t mmap_len = mmap_count * sizeof(struct e820_entry_t);

    struct e820_entry_t *mmap = conv_mem_alloc(mmap_len);

    for (size_t i = 0; i < mmap_count; i++) {
        mmap[i].base   = e820_map[i].base;
        mmap[i].length = e820_map[i].length;
        mmap[i].type   = e820_map[i].type;
    }

    *entry_count = mmap_count;
    return mmap;
}
#endif

#if defined (uefi)
struct e820_entry_t *get_raw_memmap(size_t *entry_count) {
    size_t mmap_count = efi_mmap_size / efi_desc_size;
    size_t mmap_len = mmap_count * sizeof(struct e820_entry_t);

    struct e820_entry_t *mmap = conv_mem_alloc(mmap_len);

    for (size_t i = 0; i < mmap_count; i++) {
        EFI_MEMORY_DESCRIPTOR *entry = (void *)efi_mmap + i * efi_desc_size;

        uint32_t our_type;
        switch (entry->Type) {
            case EfiReservedMemoryType:
            case EfiRuntimeServicesCode:
            case EfiRuntimeServicesData:
            case EfiUnusableMemory:
            case EfiMemoryMappedIO:
            case EfiMemoryMappedIOPortSpace:
            case EfiPalCode:
            case EfiLoaderCode:
            case EfiLoaderData:
            default:
                our_type = MEMMAP_RESERVED; break;
            case EfiACPIReclaimMemory:
                our_type = MEMMAP_ACPI_RECLAIMABLE; break;
            case EfiACPIMemoryNVS:
                our_type = MEMMAP_ACPI_NVS; break;
            case EfiBootServicesCode:
            case EfiBootServicesData:
            case EfiConventionalMemory:
                our_type = MEMMAP_USABLE; break;
        }

        mmap[i].base   = entry->PhysicalStart;
        mmap[i].length = entry->NumberOfPages * 4096;
        mmap[i].type   = our_type;
    }

    *entry_count = mmap_count;
    return mmap;
}
#endif

void *ext_mem_alloc(size_t count) {
    return ext_mem_alloc_type(count, MEMMAP_BOOTLOADER_RECLAIMABLE);
}

// Allocate memory top down, hopefully without bumping into kernel or modules
void *ext_mem_alloc_type(size_t count, uint32_t type) {
    count = ALIGN_UP(count, 4096);

    if (allocations_disallowed)
        panic("Memory allocations disallowed");

    for (int i = memmap_entries - 1; i >= 0; i--) {
        if (memmap[i].type != 1)
            continue;

        int64_t entry_base = (int64_t)(memmap[i].base);
        int64_t entry_top  = (int64_t)(memmap[i].base + memmap[i].length);

        // Let's make sure the entry is not > 4GiB
        if (entry_top >= 0x100000000) {
            entry_top = 0x100000000;
            if (entry_base >= entry_top)
                continue;
        }

        int64_t alloc_base = ALIGN_DOWN(entry_top - (int64_t)count, 4096);

        // This entry is too small for us.
        if (alloc_base < entry_base)
            continue;

        // We now reserve the range we need.
        int64_t aligned_length = entry_top - alloc_base;
        memmap_alloc_range((uint64_t)alloc_base, (uint64_t)aligned_length, type, true, true, false, false);

        void *ret = (void *)(size_t)alloc_base;

        // Zero out allocated space
        memset(ret, 0, count);

        sanitise_entries(false);

        return ret;
    }

    panic("High memory allocator: Out of memory");
}

bool memmap_alloc_range(uint64_t base, uint64_t length, uint32_t type, bool free_only, bool do_panic, bool simulation, bool new_entry) {
    if (length == 0)
        return true;

    uint64_t top = base + length;

    for (size_t i = 0; i < memmap_entries; i++) {
        if (free_only && memmap[i].type != MEMMAP_USABLE)
            continue;

        uint64_t entry_base = memmap[i].base;
        uint64_t entry_top  = memmap[i].base + memmap[i].length;
        uint32_t entry_type = memmap[i].type;

        if (base >= entry_base && base < entry_top && top <= entry_top) {
            if (simulation)
                return true;

            struct e820_entry_t *target;

            memmap[i].length -= entry_top - base;

            if (memmap[i].length == 0) {
                target = &memmap[i];
            } else {
                if (memmap_entries >= MEMMAP_MAX_ENTRIES)
                    panic("Memory map exhausted.");

                target = &memmap[memmap_entries++];
            }

            target->type   = type;
            target->base   = base;
            target->length = length;

            if (top < entry_top) {
                if (memmap_entries >= MEMMAP_MAX_ENTRIES)
                    panic("Memory map exhausted.");

                target = &memmap[memmap_entries++];

                target->type   = entry_type;
                target->base   = top;
                target->length = entry_top - top;
            }

            sanitise_entries(false);

            return true;
        }
    }

    if (!new_entry && do_panic)
        panic("Memory allocation failure.");

    if (new_entry) {
        if (memmap_entries >= MEMMAP_MAX_ENTRIES)
            panic("Memory map exhausted.");

        struct e820_entry_t *target = &memmap[memmap_entries++];

        target->type = type;
        target->base = base;
        target->length = length;

        sanitise_entries(false);

        return true;
    }

    return false;
}
