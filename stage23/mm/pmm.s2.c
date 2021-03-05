#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <mm/pmm.h>
#include <sys/e820.h>
#include <lib/blib.h>
#include <lib/libc.h>
#include <lib/print.h>
#if defined (uefi)
#  include <efi.h>
#endif

#define PAGE_SIZE   4096
#define MEMMAP_BASE ((size_t)0x100000)
#define MEMMAP_MAX_ENTRIES 256

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
        default:
            return "???";
    }
}

void print_memmap(struct e820_entry_t *mm, size_t size) {
    for (size_t i = 0; i < size; i++) {
        print("[%X -> %X] : %X  <%s>\n",
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

    uint64_t top = *base + *length;

    if (*base < MEMMAP_BASE) {
        if (top > MEMMAP_BASE) {
            *length -= MEMMAP_BASE - *base;
            *base    = MEMMAP_BASE;
        } else {
            return false;
        }
    }

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

    // Merge contiguous bootloader-reclaimable entries
    for (size_t i = 0; i < memmap_entries - 1; i++) {
        if (memmap[i].type != MEMMAP_BOOTLOADER_RECLAIMABLE)
            continue;

        if (memmap[i+1].type == MEMMAP_BOOTLOADER_RECLAIMABLE
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

    // Align bootloader-reclaimable entries
    if (align_entries) {
        for (size_t i = 0; i < memmap_entries; i++) {
            if (memmap[i].type != MEMMAP_BOOTLOADER_RECLAIMABLE)
                continue;

            if (!align_entry(&memmap[i].base, &memmap[i].length)) {
                // Eradicate from memmap
                for (size_t j = i; j < memmap_entries - 1; j++) {
                    memmap[j] = memmap[j+1];
                }
                memmap_entries--;
                i--;
            }
        }
    }
}

static bool allocations_disallowed = true;

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

        memmap[memmap_entries++] = e820_map[i];
    }

    sanitise_entries(false);

    allocations_disallowed = false;
}
#endif

void *ext_mem_alloc(size_t count) {
    return ext_mem_alloc_type(count, MEMMAP_BOOTLOADER_RECLAIMABLE);
}

void *ext_mem_alloc_aligned(size_t count, size_t alignment) {
    return ext_mem_alloc_aligned_type(count, alignment, MEMMAP_BOOTLOADER_RECLAIMABLE);
}

void *ext_mem_alloc_type(size_t count, uint32_t type) {
    return ext_mem_alloc_aligned_type(count, 4, type);
}

// Allocate memory top down, hopefully without bumping into kernel or modules
static void *_ext_mem_alloc_aligned_type(size_t count, size_t alignment, uint32_t type) {
    if (allocations_disallowed)
        panic("Extended memory allocations disallowed");

    for (int i = memmap_entries - 1; i >= 0; i--) {
        if (memmap[i].type != 1)
            continue;

        int64_t entry_base = (int64_t)(memmap[i].base);
        int64_t entry_top  = (int64_t)(memmap[i].base + memmap[i].length);

        // Let's make sure the entry is not > 4GiB
        if (entry_base >= 0x100000000 || entry_top >= 0x100000000) {
            // Theoretically there could be an entry which crosses the 4GiB
            // boundary, but realistically this does not happen as far as I
            // have seen. Let's just discard the entry.
            continue;
        }

        int64_t alloc_base = ALIGN_DOWN(entry_top - (int64_t)count, alignment);

        // This entry is too small for us.
        if (alloc_base < entry_base)
            continue;

        // We now reserve the range we need.
        int64_t aligned_length = entry_top - alloc_base;
        memmap_alloc_range((uint64_t)alloc_base, (uint64_t)aligned_length, type, true, true);

        void *ret = (void *)(size_t)alloc_base;

        // Zero out allocated space
        memset(ret, 0, count);

        sanitise_entries(false);

        return ret;
    }

    panic("High memory allocator: Out of memory");
}

#if defined (bios)
void *ext_mem_alloc_aligned_type(size_t count, size_t alignment, uint32_t type) {
    return _ext_mem_alloc_aligned_type(count, alignment, type);
}
#endif

#if defined (uefi)
bool pmm_mmap_efi2ours(EFI_MEMORY_DESCRIPTOR *efi_mmap,
                       size_t desc_size, size_t entry_count) {
    for (size_t i = 0; i < entry_count; i++) {
        EFI_MEMORY_DESCRIPTOR *entry = (void *)efi_mmap + i * desc_size;

        uint32_t our_type;
        switch (entry->Type) {
            case EfiReservedMemoryType:
            case EfiRuntimeServicesCode:
            case EfiRuntimeServicesData:
            case EfiUnusableMemory:
            case EfiMemoryMappedIO:
            case EfiMemoryMappedIOPortSpace:
            case EfiPalCode:
            default:
                our_type = MEMMAP_RESERVED; break;
            case EfiBootServicesCode:
            case EfiBootServicesData:
            case EfiLoaderCode:
            case EfiLoaderData:
                our_type = MEMMAP_BOOTLOADER_RECLAIMABLE; break;
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

        memmap_entries++;
    }

    sanitise_entries(false);

    print_memmap(memmap, memmap_entries);

    allocations_disallowed = false;

    return true;
}

void *ext_mem_alloc_aligned_type(size_t count, size_t alignment, uint32_t type) {
    if (efi_boot_services_exited) {
        return _ext_mem_alloc_aligned_type(count, alignment, type);
    }

    EFI_STATUS status;

    void *ret;

    if (alignment && alignment % 4096 == 0) {
        status = uefi_call_wrapper(gBS->AllocatePages, 4, 0, 2,
                                   DIV_ROUNDUP(count, 4096), &ret);
    } else {
        status = uefi_call_wrapper(gBS->AllocatePool, 3, 4, count, &ret);
    }

    if (status) {
        panic("Memory allocation error %x", status);
    }

    if ((uintptr_t)ret % alignment) {
        panic("Memory alloc align bad");
    }

    memset(ret, 0, count);

    return ret;
}
#endif

bool memmap_alloc_range(uint64_t base, uint64_t length, uint32_t type, bool free_only, bool do_panic) {
    uint64_t top = base + length;

    if (base < 0x100000) {
        if (do_panic) {
            // We don't do allocations below 1 MiB
            panic("Attempt to allocate memory below 1 MiB (%X-%X)",
                  base, base + length);
        } else {
            return false;
        }
    }

    for (size_t i = 0; i < memmap_entries; i++) {
        if (free_only && memmap[i].type != 1)
            continue;

        uint64_t entry_base = memmap[i].base;
        uint64_t entry_top  = memmap[i].base + memmap[i].length;
        if (base >= entry_base && base <  entry_top &&
            top  >= entry_base && top  <= entry_top) {

            memmap[i].length = base - entry_base;

            if (memmap[i].length == 0) {
                // Eradicate from memmap
                for (size_t j = i; j < memmap_entries - 1; j++) {
                    memmap[j] = memmap[j+1];
                }
                memmap_entries--;
            }

            if (memmap_entries >= MEMMAP_MAX_ENTRIES) {
                panic("Memory map exhausted.");
            }
            struct e820_entry_t *target = &memmap[memmap_entries];

            target->length = entry_top - top;

            if (target->length != 0) {
                target->base = top;
                target->type = 1;

                memmap_entries++;
            }

            if (memmap_entries >= MEMMAP_MAX_ENTRIES) {
                panic("Memory map exhausted.");
            }
            target = &memmap[memmap_entries++];

            target->type   = type;
            target->base   = base;
            target->length = length;

            return true;
        }
    }

    if (do_panic)
        panic("Out of memory");

    return false;
}

#if defined (bios)
extern symbol bss_end;
static size_t bump_allocator_base = (size_t)bss_end;
static size_t bump_allocator_limit = 0x70000;

void *conv_mem_alloc(size_t count) {
    return conv_mem_alloc_aligned(count, 4);
}

void *conv_mem_alloc_aligned(size_t count, size_t alignment) {
    size_t new_base = ALIGN_UP(bump_allocator_base, alignment);
    void *ret = (void *)new_base;
    new_base += count;
    if (new_base >= bump_allocator_limit)
        panic("Memory allocation failed");
    bump_allocator_base = new_base;

    // Zero out allocated space
    memset(ret, 0, count);

    return ret;
}
#endif
