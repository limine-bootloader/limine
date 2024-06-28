#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <lib/libc.h>
#include <lib/misc.h>
#include <lib/print.h>
#include <lib/trace.h>
#include <lib/real.h>
#include <fs/file.h>
#include <mm/pmm.h>
#include <libfdt/libfdt.h>

#if defined (UEFI)
EFI_SYSTEM_TABLE *gST;
EFI_BOOT_SERVICES *gBS;
EFI_RUNTIME_SERVICES *gRT;
EFI_HANDLE efi_image_handle;
EFI_MEMORY_DESCRIPTOR *efi_mmap = NULL;
UINTN efi_mmap_size = 0, efi_desc_size = 0;
UINT32 efi_desc_ver = 0;
#endif

bool editor_enabled = true;
bool help_hidden = false;

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

uint32_t oct2bin(uint8_t *str, uint32_t max) {
    uint32_t value = 0;
    while (max-- > 0) {
        value <<= 3;
        value += *str++ - '0';
    }
    return value;
}

uint32_t hex2bin(uint8_t *str, uint32_t size) {
    uint32_t value = 0;
    while (size-- > 0) {
        value <<= 4;
        if (*str >= '0' && *str <= '9')
            value += (uint32_t)((*str) - '0');
        else if (*str >= 'A' && *str <= 'F')
            value += (uint32_t)((*str) - 'A' + 10);
        else if (*str >= 'a' && *str <= 'f')
            value += (uint32_t)((*str) - 'a' + 10);
        str++;
    }
    return value;
}

#if defined (UEFI)

void *get_device_tree_blob(size_t extra_size) {
    int ret;

    EFI_GUID dtb_guid = EFI_DTB_TABLE_GUID;
    for (size_t i = 0; i < gST->NumberOfTableEntries; i++) {
        EFI_CONFIGURATION_TABLE *cur_table = &gST->ConfigurationTable[i];
        if (memcmp(&cur_table->VendorGuid, &dtb_guid, sizeof(EFI_GUID)))
            continue;

        size_t s = fdt_totalsize(cur_table->VendorTable);

        printv("efi: found dtb at %p, size %x\n", cur_table->VendorTable, s);

        if (extra_size == 0) {
            return cur_table->VendorTable;
        }

        void *new_tab = ext_mem_alloc(s + extra_size);

        ret = fdt_resize(cur_table->VendorTable, new_tab, s + extra_size);
        if (ret < 0) {
            panic(true, "dtb: failed to resize new DTB");
        }

        return new_tab;
    }

    if (extra_size == 0) {
        return NULL;
    }

    void *dtb = ext_mem_alloc(extra_size);

    ret = fdt_create_empty_tree(dtb, extra_size);
    if (ret < 0) {
        panic(true, "dtb: failed to create a device tree blob: '%s'", fdt_strerror(ret));
    }

    ret = fdt_setprop_u32(dtb, 0, "#address-cells", 2);
    if (ret < 0) {
        panic(true, "dtb: failed to set #address-cells: '%s'", fdt_strerror(ret));
    }

    ret = fdt_setprop_u32(dtb, 0, "#size-cells", 1);
    if (ret < 0) {
        panic(true, "dtb: failed to set #size-cells: '%s'", fdt_strerror(ret));
    }

    return dtb;
}

#if defined (__riscv)

RISCV_EFI_BOOT_PROTOCOL *get_riscv_boot_protocol(void) {
    EFI_GUID boot_proto_guid = RISCV_EFI_BOOT_PROTOCOL_GUID;
    RISCV_EFI_BOOT_PROTOCOL *proto;

    // LocateProtocol() is available from EFI version 1.1
    if (gBS->Hdr.Revision >= ((1 << 16) | 10)) {
        if (gBS->LocateProtocol(&boot_proto_guid, NULL, (void **)&proto) == EFI_SUCCESS) {
            return proto;
        }
    }

    UINTN bufsz = 0;
    if (gBS->LocateHandle(ByProtocol, &boot_proto_guid, NULL, &bufsz, NULL) != EFI_BUFFER_TOO_SMALL)
        return NULL;

    EFI_HANDLE *handles_buf = ext_mem_alloc(bufsz);
    if (handles_buf == NULL)
        return NULL;

    if (bufsz < sizeof(EFI_HANDLE))
        goto error;

    if (gBS->LocateHandle(ByProtocol, &boot_proto_guid, NULL, &bufsz, handles_buf) != EFI_SUCCESS)
        goto error;

    if (gBS->HandleProtocol(handles_buf[0], &boot_proto_guid, (void **)&proto) != EFI_SUCCESS)
        goto error;

    pmm_free(handles_buf, bufsz);
    return proto;

error:
    pmm_free(handles_buf, bufsz);
    return NULL;
}

#endif

no_unwind bool efi_boot_services_exited = false;

bool efi_exit_boot_services(void) {
    EFI_STATUS status;

    EFI_MEMORY_DESCRIPTOR tmp_mmap[1];
    efi_mmap_size = sizeof(tmp_mmap);
    UINTN mmap_key = 0;

    gBS->GetMemoryMap(&efi_mmap_size, tmp_mmap, &mmap_key, &efi_desc_size, &efi_desc_ver);

    efi_mmap_size += 4096;

    status = gBS->FreePool(efi_mmap);
    if (status) {
        goto fail;
    }

    status = gBS->AllocatePool(EfiLoaderData, efi_mmap_size, (void **)&efi_mmap);
    if (status) {
        goto fail;
    }

    EFI_MEMORY_DESCRIPTOR *efi_copy;
    status = gBS->AllocatePool(EfiLoaderData, efi_mmap_size * 2, (void **)&efi_copy);
    if (status) {
        goto fail;
    }

    const size_t EFI_COPY_MAX_ENTRIES = (efi_mmap_size * 2) / efi_desc_size;

    size_t retries = 0;

retry:
    status = gBS->GetMemoryMap(&efi_mmap_size, efi_mmap, &mmap_key, &efi_desc_size, &efi_desc_ver);
    if (retries == 0 && status) {
        goto fail;
    }

    // Be gone, UEFI!
    status = gBS->ExitBootServices(efi_image_handle, mmap_key);
    if (status) {
        if (retries == 128) {
            goto fail;
        }
        retries++;
        goto retry;
    }

#if defined(__x86_64__) || defined(__i386__)
    asm volatile ("cli" ::: "memory");
#elif defined (__aarch64__)
    asm volatile ("msr daifset, #15" ::: "memory");
#elif defined (__riscv64)
    asm volatile ("csrci sstatus, 0x2" ::: "memory");
#else
#error Unknown architecture
#endif

    // Go through new EFI memmap and free up bootloader entries
    size_t entry_count = efi_mmap_size / efi_desc_size;

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

            if (top > untouched_memmap[j].base && top <= untouched_memmap[j].base + untouched_memmap[j].length) {
                if (untouched_memmap[j].base < base) {
                    new_entry->NumberOfPages = (base - untouched_memmap[j].base) / 4096;

                    efi_copy_i++;
                    if (efi_copy_i == EFI_COPY_MAX_ENTRIES) {
                        panic(false, "efi: New memory map exhausted");
                    }
                    new_entry = (void *)efi_copy + efi_copy_i * efi_desc_size;
                    memcpy(new_entry, orig_entry, efi_desc_size);

                    new_entry->NumberOfPages -= (base - untouched_memmap[j].base) / 4096;
                    new_entry->PhysicalStart = base;
                    new_entry->VirtualStart = 0;

                    length = new_entry->NumberOfPages * 4096;
                    top = base + length;
                }

                if (untouched_memmap[j].base > base) {
                    new_entry->NumberOfPages = (untouched_memmap[j].base - base) / 4096;

                    efi_copy_i++;
                    if (efi_copy_i == EFI_COPY_MAX_ENTRIES) {
                        panic(false, "efi: New memory map exhausted");
                    }
                    new_entry = (void *)efi_copy + efi_copy_i * efi_desc_size;
                    memcpy(new_entry, orig_entry, efi_desc_size);

                    new_entry->NumberOfPages -= (untouched_memmap[j].base - base) / 4096;
                    new_entry->PhysicalStart = untouched_memmap[j].base;
                    new_entry->VirtualStart = 0;

                    base = new_entry->PhysicalStart;
                    length = new_entry->NumberOfPages * 4096;
                    top = base + length;
                }

                if (length < untouched_memmap[j].length) {
                    panic(false, "efi: Memory map corruption");
                }

                new_entry->Type = EfiConventionalMemory;

                if (length == untouched_memmap[j].length) {
                    // It's a perfect match!
                    break;
                }

                new_entry->NumberOfPages = untouched_memmap[j].length / 4096;

                efi_copy_i++;
                if (efi_copy_i == EFI_COPY_MAX_ENTRIES) {
                    panic(false, "efi: New memory map exhausted");
                }
                new_entry = (void *)efi_copy + efi_copy_i * efi_desc_size;
                memcpy(new_entry, orig_entry, efi_desc_size);

                new_entry->NumberOfPages = (length - untouched_memmap[j].length) / 4096;
                new_entry->PhysicalStart = base + untouched_memmap[j].length;
                new_entry->VirtualStart = 0;

                break;
            }
        }

        efi_copy_i++;
        if (efi_copy_i == EFI_COPY_MAX_ENTRIES) {
            panic(false, "efi: New memory map exhausted");
        }
    }

    efi_mmap = efi_copy;
    efi_mmap_size = efi_copy_i * efi_desc_size;

    efi_boot_services_exited = true;

    printv("efi: Exited boot services.\n");

    return true;

fail:
    panic(false, "efi: Failed to exit boot services");
}

#endif
