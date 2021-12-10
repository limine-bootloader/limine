#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <mm/pmm.h>
#include <sys/e820.h>
#include <lib/acpi.h>
#include <lib/blib.h>
#include <lib/libc.h>
#include <lib/print.h>
#if uefi == 1
#  include <efi.h>
#endif

#define PAGE_SIZE   4096

#if bios == 1
extern symbol bss_end;
#endif

static bool allocations_disallowed = true;
static void sanitise_entries(struct e820_entry_t *, size_t *, bool);

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

            sanitise_entries(memmap, &memmap_entries, false);

            return ret;
        }

        base += 4096;
    }
}

#if bios == 1
#define memmap_max_entries ((size_t)512)

struct e820_entry_t memmap[memmap_max_entries];
size_t memmap_entries = 0;
#endif

#if uefi == 1
static size_t memmap_max_entries;

struct e820_entry_t *memmap;
size_t memmap_entries = 0;

struct e820_entry_t *untouched_memmap;
size_t untouched_memmap_entries = 0;
#endif

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
        case MEMMAP_FRAMEBUFFER:
            return "Framebuffer";
        case MEMMAP_BOOTLOADER_RECLAIMABLE:
            return "Bootloader reclaimable";
        case MEMMAP_KERNEL_AND_MODULES:
            return "Kernel/Modules";
        case MEMMAP_EFI_RECLAIMABLE:
            return "EFI reclaimable";
        case MEMMAP_EFI_BOOTSERVICES:
            return "EFI boot services";
        default:
            return "???";
    }
}

void print_memmap(struct e820_entry_t *mm, size_t size) {
    for (size_t i = 0; i < size; i++) {
        printv("[%X -> %X] : %X  <%s (%x)>\n",
               mm[i].base,
               mm[i].base + mm[i].length,
               mm[i].length,
               memmap_type(mm[i].type), mm[i].type);
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

static bool sanitiser_keep_first_page = false;

static void sanitise_entries(struct e820_entry_t *m, size_t *_count, bool align_entries) {
    size_t count = *_count;

    for (size_t i = 0; i < count; i++) {
        if (m[i].type != MEMMAP_USABLE)
            continue;

        // Check if the entry overlaps other entries
        for (size_t j = 0; j < count; j++) {
            if (j == i)
                continue;

            uint64_t base   = m[i].base;
            uint64_t length = m[i].length;
            uint64_t top    = base + length;

            uint64_t res_base   = m[j].base;
            uint64_t res_length = m[j].length;
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

            m[i].base   = base;
            m[i].length = top - base;
        }

        if (!m[i].length
         || (align_entries && !align_entry(&m[i].base, &m[i].length))) {
            // Remove i from memmap
            m[i] = m[count - 1];
            count--; i--;
        }
    }

    // Remove 0 length usable entries and usable entries below 0x1000
    for (size_t i = 0; i < count; i++) {
        if (m[i].type != MEMMAP_USABLE)
            continue;

        if (!sanitiser_keep_first_page && m[i].base < 0x1000) {
            if (m[i].base + m[i].length <= 0x1000) {
                goto del_mm1;
            }

            m[i].length -= 0x1000 - m[i].base;
            m[i].base = 0x1000;
        }

        if (m[i].length == 0) {
del_mm1:
            // Remove i from memmap
            m[i] = m[count - 1];
            count--; i--;
        }
    }

    // Sort the entries
    for (size_t p = 0; p < count - 1; p++) {
        uint64_t min = m[p].base;
        size_t min_index = p;
        for (size_t i = p; i < count; i++) {
            if (m[i].base < min) {
                min = m[i].base;
                min_index = i;
            }
        }
        struct e820_entry_t min_e = m[min_index];
        m[min_index] = m[p];
        m[p] = min_e;
    }

    // Merge contiguous bootloader-reclaimable and usable entries
    for (size_t i = 0; i < count - 1; i++) {
        if (m[i].type != MEMMAP_BOOTLOADER_RECLAIMABLE
         && m[i].type != MEMMAP_USABLE)
            continue;

        if (m[i+1].type == m[i].type
         && m[i+1].base == m[i].base + m[i].length) {
            m[i].length += m[i+1].length;

            // Eradicate from memmap
            for (size_t j = i + 2; j < count; j++) {
                m[j - 1] = m[j];
            }
            count--;
            i--;
        }
    }

    *_count = count;
}

struct e820_entry_t *get_memmap(size_t *entries) {
    sanitise_entries(memmap, &memmap_entries, true);

    *entries = memmap_entries;

    allocations_disallowed = true;

    return memmap;
}

#if bios == 1
void init_memmap(void) {
    for (size_t i = 0; i < e820_entries; i++) {
        if (memmap_entries == memmap_max_entries) {
            panic("Memory map exhausted.");
        }

        memmap[memmap_entries] = e820_map[i];

        uint64_t top = memmap[memmap_entries].base + memmap[memmap_entries].length;

        if (memmap[memmap_entries].type == MEMMAP_USABLE) {
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

    sanitise_entries(memmap, &memmap_entries, false);

    // Allocate bootloader itself
    memmap_alloc_range(4096,
        ALIGN_UP((uintptr_t)bss_end, 4096) - 4096, MEMMAP_BOOTLOADER_RECLAIMABLE, true, true, false, false);

    sanitise_entries(memmap, &memmap_entries, false);

    allocations_disallowed = false;
}
#endif

#if uefi == 1
extern symbol ImageBase;
extern symbol _edata;

void init_memmap(void) {
    EFI_STATUS status;

    EFI_MEMORY_DESCRIPTOR tmp_mmap[1];
    efi_mmap_size = sizeof(tmp_mmap);
    UINTN mmap_key = 0;

    gBS->GetMemoryMap(&efi_mmap_size, tmp_mmap, &mmap_key, &efi_desc_size, &efi_desc_ver);

    memmap_max_entries = (efi_mmap_size / efi_desc_size) + 512;

    efi_mmap_size += 4096;

    status = gBS->AllocatePool(EfiLoaderData, efi_mmap_size, (void **)&efi_mmap);
    if (status) {
        goto fail;
    }

    status = gBS->AllocatePool(EfiLoaderData, memmap_max_entries * sizeof(struct e820_entry_t), (void **)&memmap);
    if (status) {
        goto fail;
    }

    status = gBS->AllocatePool(EfiLoaderData, memmap_max_entries * sizeof(struct e820_entry_t), (void **)&untouched_memmap);
    if (status) {
        goto fail;
    }

    status = gBS->GetMemoryMap(&efi_mmap_size, efi_mmap, &mmap_key, &efi_desc_size, &efi_desc_ver);
    if (status) {
        goto fail;
    }

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
                our_type = MEMMAP_EFI_BOOTSERVICES; break;
            case EfiACPIReclaimMemory:
                our_type = MEMMAP_ACPI_RECLAIMABLE; break;
            case EfiACPIMemoryNVS:
                our_type = MEMMAP_ACPI_NVS; break;
            case EfiConventionalMemory:
                our_type = MEMMAP_USABLE; break;
        }

        uint64_t base = entry->PhysicalStart;
        uint64_t length = entry->NumberOfPages * 4096;

        if (our_type == MEMMAP_USABLE) {
            if (base + length >= 0x100000000) {
                our_type = MEMMAP_EFI_RECLAIMABLE;
            }
        }

        memmap[memmap_entries].base = base;
        memmap[memmap_entries].length = length;
        memmap[memmap_entries].type = our_type;

        memmap_entries++;
    }

    sanitise_entries(memmap, &memmap_entries, false);

    allocations_disallowed = false;

    // Let's leave 64MiB to the firmware below 4GiB
    for (size_t i = 0; i < 64; i++) {
        ext_mem_alloc_type(0x100000, MEMMAP_EFI_RECLAIMABLE);
    }

    // Now own all the usable entries
    for (size_t i = 0; i < memmap_entries; i++) {
        if (memmap[i].type != MEMMAP_USABLE)
            continue;

        EFI_PHYSICAL_ADDRESS base = memmap[i].base;

        status = gBS->AllocatePages(AllocateAddress, EfiLoaderData,
                                    memmap[i].length / 4096, &base);

        if (status) {
            print("pmm: WARNING: AllocatePages failure (%d)\n", status);
            memmap_alloc_range(memmap[i].base, memmap[i].length, MEMMAP_RESERVED, true, true, false, false);
        }
    }

    memcpy(untouched_memmap, memmap, memmap_entries * sizeof(struct e820_entry_t));
    untouched_memmap_entries = memmap_entries;

    // Allocate bootloader itself
    memmap_alloc_range((uintptr_t)ImageBase, (uintptr_t)_edata - (uintptr_t)ImageBase,
                       MEMMAP_BOOTLOADER_RECLAIMABLE, false, true, false, true);

    sanitise_entries(memmap, &memmap_entries, false);

    return;

fail:
    panic("pmm: Failure initialising memory map");
}

void pmm_reclaim_uefi_mem(void) {
    // First, ensure the boot services are still boot services, or free, in
    // the EFI memmap
    for (size_t i = 0; i < memmap_entries; i++) {
        if (memmap[i].type != MEMMAP_EFI_BOOTSERVICES)
            continue;

        // Go through EFI memmap and ensure this entry fits within a boot services
        // or conventional entry
        size_t entry_count = efi_mmap_size / efi_desc_size;

        for (size_t j = 0; j < entry_count; j++) {
            EFI_MEMORY_DESCRIPTOR *entry = (void *)efi_mmap + j * efi_desc_size;

            switch (entry->Type) {
                case EfiBootServicesCode:
                case EfiBootServicesData:
                case EfiConventionalMemory:
                    break;
                default:
                    continue;
            }

            uintptr_t base = memmap[i].base;
            uintptr_t top = base + memmap[i].length;
            uintptr_t efi_base = entry->PhysicalStart;
            uintptr_t efi_size = entry->NumberOfPages * 4096;
            uintptr_t efi_top = efi_base + efi_size;

            if (!(base >= efi_base && base <  efi_top
               && top  >  efi_base && top  <= efi_top))
                continue;

            memmap[i].type = MEMMAP_USABLE;
        }
    }

    size_t recl_i = 0;

    for (size_t i = 0; i < memmap_entries; i++) {
        if (memmap[i].type == MEMMAP_EFI_RECLAIMABLE) {
            recl_i++;
        }
    }

    struct e820_entry_t *recl = ext_mem_alloc(recl_i * sizeof(struct e820_entry_t));

    {
        size_t recl_j = 0;
        for (size_t i = 0; i < memmap_entries; i++) {
            if (memmap[i].type == MEMMAP_EFI_RECLAIMABLE) {
                recl[recl_j++] = memmap[i];
            }
        }
    }

another_recl:;
    // Punch holes in our EFI reclaimable entry for every EFI area which is
    // boot services or conventional that fits within
    size_t efi_mmap_entry_count = efi_mmap_size / efi_desc_size;
    for (size_t i = 0; i < efi_mmap_entry_count; i++) {
        EFI_MEMORY_DESCRIPTOR *entry = (void *)efi_mmap + i * efi_desc_size;

        uintptr_t base = recl->base;
        uintptr_t top = base + recl->length;
        uintptr_t efi_base = entry->PhysicalStart;
        uintptr_t efi_size = entry->NumberOfPages * 4096;

        if (efi_base < base) {
            if (efi_size <= base - efi_base)
                continue;
            efi_size -= base - efi_base;
            efi_base = base;
        }

        uintptr_t efi_top = efi_base + efi_size;

        if (efi_top > top) {
            if (efi_size <= efi_top - top)
                continue;
            efi_size -= efi_top - top;
            efi_top = top;
        }

        // Sanity check
        if (!(efi_base >= base && efi_base <  top
           && efi_top  >  base && efi_top  <= top))
            continue;

        uint32_t our_type;
        switch (entry->Type) {
            case EfiBootServicesCode:
            case EfiBootServicesData:
            case EfiConventionalMemory:
                our_type = MEMMAP_USABLE; break;
            case EfiACPIReclaimMemory:
                our_type = MEMMAP_ACPI_RECLAIMABLE; break;
            case EfiACPIMemoryNVS:
                our_type = MEMMAP_ACPI_NVS; break;
            default:
                our_type = MEMMAP_RESERVED; break;
        }

        memmap_alloc_range(efi_base, efi_size, our_type, false, true, false, true);
    }

    if (--recl_i > 0) {
        recl++;
        goto another_recl;
    }

    sanitise_entries(memmap, &memmap_entries, false);
}

void pmm_release_uefi_mem(void) {
    EFI_STATUS status;

    for (size_t i = 0; i < untouched_memmap_entries; i++) {
        if (untouched_memmap[i].type != MEMMAP_USABLE
         && untouched_memmap[i].type != MEMMAP_BOOTLOADER_RECLAIMABLE) {
            continue;
        }

        status = gBS->FreePages(untouched_memmap[i].base, untouched_memmap[i].length / 4096);

        if (status) {
            panic("pmm: FreePages failure (%x)", status);
        }
    }

    allocations_disallowed = true;
}
#endif

#if bios == 1
struct e820_entry_t *get_raw_memmap(size_t *entry_count) {
    *entry_count = e820_entries;
    return e820_map;
}
#endif

#if uefi == 1
struct e820_entry_t *get_raw_memmap(size_t *entry_count) {
    size_t mmap_count = efi_mmap_size / efi_desc_size;
    size_t mmap_len = mmap_count * sizeof(struct e820_entry_t);

    struct e820_entry_t *mmap = ext_mem_alloc(mmap_len);

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

    bool s_old = sanitiser_keep_first_page;
    sanitiser_keep_first_page = true;
    sanitise_entries(mmap, &mmap_count, false);
    sanitiser_keep_first_page = s_old;

    *entry_count = mmap_count;
    return mmap;
}
#endif

void pmm_free(void *ptr, size_t count) {
    count = ALIGN_UP(count, 4096);
    if (allocations_disallowed)
        panic("Memory allocations disallowed");
    memmap_alloc_range((uintptr_t)ptr, count, MEMMAP_USABLE, false, false, false, false);
}

void *ext_mem_alloc(size_t count) {
    return ext_mem_alloc_type(count, MEMMAP_BOOTLOADER_RECLAIMABLE);
}

void *ext_mem_alloc_type(size_t count, uint32_t type) {
    return ext_mem_alloc_type_aligned(count, type, 4096);
}

// Allocate memory top down, hopefully without bumping into kernel or modules
void *ext_mem_alloc_type_aligned(size_t count, uint32_t type, size_t alignment) {
    count = ALIGN_UP(count, alignment);

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

        int64_t alloc_base = ALIGN_DOWN(entry_top - (int64_t)count, alignment);

        // This entry is too small for us.
        if (alloc_base < entry_base)
            continue;

        // We now reserve the range we need.
        int64_t aligned_length = entry_top - alloc_base;
        memmap_alloc_range((uint64_t)alloc_base, (uint64_t)aligned_length, type, true, true, false, false);

        void *ret = (void *)(size_t)alloc_base;

        // Zero out allocated space
        memset(ret, 0, count);

        sanitise_entries(memmap, &memmap_entries, false);

        return ret;
    }

    panic("High memory allocator: Out of memory");
}

/// Compute and returns the amount of upper and lower memory till
/// the first hole.
struct meminfo mmap_get_info(size_t mmap_count, struct e820_entry_t *mmap) {
    struct meminfo info = {0};

    for (size_t i = 0; i < mmap_count; i++) {
        if (mmap[i].type == MEMMAP_USABLE) {
            // NOTE: Upper memory starts at address 1MiB and the
            // value of uppermem is the address of the first upper memory
            // hole minus 1MiB.
            if (mmap[i].base < 0x100000) {
                if (mmap[i].base + mmap[i].length > 0x100000) {
                    size_t low_len = 0x100000 - mmap[i].base;

                    info.lowermem += low_len;
                    info.uppermem += mmap[i].length - low_len;
                } else {
                    info.lowermem += mmap[i].length;
                }
            } else {
                info.uppermem += mmap[i].length;
            }
        }
    }

    return info;
}

static bool pmm_new_entry(uint64_t base, uint64_t length, uint32_t type) {
    uint64_t top = base + length;

    // Handle overlapping new entries.
    for (size_t i = 0; i < memmap_entries; i++) {
        uint64_t entry_base = memmap[i].base;
        uint64_t entry_top  = memmap[i].base + memmap[i].length;

        // Full overlap
        if (base <= entry_base && top >= entry_top) {
            // Remove overlapped entry
            for (size_t j = i + 1; j < memmap_entries; j++) {
                memmap[j - 1] = memmap[j];
            }
            memmap_entries--;
            i--;
            continue;
        }

        // Partial overlap (bottom)
        if (base <= entry_base && top < entry_top && top > entry_base) {
            // Entry gets bottom shaved off
            memmap[i].base += top - entry_base;
            memmap[i].length -= top - entry_base;
            continue;
        }

        // Partial overlap (top)
        if (base > entry_base && base < entry_top && top >= entry_top) {
            // Entry gets top shaved off
            memmap[i].length -= entry_top - base;
            continue;
        }

        // Nested (pain)
        if (base > entry_base && top < entry_top) {
            // Entry gets top shaved off first
            memmap[i].length -= entry_top - base;

            // Now we need to create a new entry
            if (memmap_entries >= memmap_max_entries)
                panic("Memory map exhausted.");

            struct e820_entry_t *new_entry = &memmap[memmap_entries++];

            new_entry->type = memmap[i].type;
            new_entry->base = top;
            new_entry->length = entry_top - top;

            continue;
        }
    }

    if (memmap_entries >= memmap_max_entries)
        panic("Memory map exhausted.");

    struct e820_entry_t *target = &memmap[memmap_entries++];

    target->type = type;
    target->base = base;
    target->length = length;

    sanitise_entries(memmap, &memmap_entries, false);

    return true;
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
                if (memmap_entries >= memmap_max_entries)
                    panic("Memory map exhausted.");

                target = &memmap[memmap_entries++];
            }

            target->type   = type;
            target->base   = base;
            target->length = length;

            if (top < entry_top) {
                if (memmap_entries >= memmap_max_entries)
                    panic("Memory map exhausted.");

                target = &memmap[memmap_entries++];

                target->type   = entry_type;
                target->base   = top;
                target->length = entry_top - top;
            }

            sanitise_entries(memmap, &memmap_entries, false);

            return true;
        }
    }

    if (!new_entry && do_panic)
        panic("Memory allocation failure.");

    if (new_entry) {
        return pmm_new_entry(base, length, type);
    }

    return false;
}

bool pmm_allocation_allowed(void) {
    return !allocations_disallowed;
}
