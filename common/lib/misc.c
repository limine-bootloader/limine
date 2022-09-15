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

no_unwind bool efi_boot_services_exited = false;

#define EFI_COPY_MAX_ENTRIES 512

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

    asm volatile ("cli" ::: "memory");

    efi_boot_services_exited = true;

    printv("efi: Exited boot services.\n");

    return true;

fail:
    panic(false, "efi: Failed to exit boot services");
}

#endif
