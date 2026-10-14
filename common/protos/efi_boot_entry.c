#include <protos/efi_boot_entry.h>

#if defined(UEFI)

#include <mm/pmm.h>
#include <efi.h>
#include <lib/config.h>
#include <lib/misc.h>
#include <lib/utf.h>
#include <stdbool.h>

static bool uefi_string_matches(EFI_CHAR16 *desc, size_t desc_max_chars, EFI_CHAR16 *target) {
    while (*target) {
        if (desc_max_chars == 0)
            return false;
        EFI_CHAR16 a = *desc >= L'a' && *desc <= L'z' ? *desc - 32 : *desc;
        EFI_CHAR16 b = *target >= L'a' && *target <= L'z' ? *target - 32 : *target;
        if (a != b)
            return false;
        desc++;
        target++;
        desc_max_chars--;
    }
    return desc_max_chars > 0 && *desc == L'\0';
}

static void format_boot_var(EFI_CHAR16 *out, EFI_UINT16 num) {
    out[0] = L'B';
    out[1] = L'o';
    out[2] = L'o';
    out[3] = L't';
    out[4] = L"0123456789ABCDEF"[(num >> 12) & 0xF];
    out[5] = L"0123456789ABCDEF"[(num >> 8) & 0xF];
    out[6] = L"0123456789ABCDEF"[(num >> 4) & 0xF];
    out[7] = L"0123456789ABCDEF"[(num >> 0) & 0xF];
    out[8] = L'\0';
}

static bool find_boot_entry(EFI_CHAR16 *entry, uint16_t *out) {
    EFI_STATUS status;
    uint16_t *boot_order = NULL;
    EFI_UINTN size = 0;
    EFI_UINTN boot_order_alloc = 0;
    EFI_GUID global_variable = EFI_GLOBAL_VARIABLE;

    /* Allow one retry if BootOrder grows after the size query. */
    for (size_t attempt = 0; attempt < 3; attempt++) {
        status = gRT->GetVariable(L"BootOrder", &global_variable, NULL, &size, boot_order);

        if (status != EFI_BUFFER_TOO_SMALL || attempt == 2) {
            break;
        }

        pmm_free(boot_order, boot_order_alloc);
        boot_order_alloc = size;
        boot_order = ext_mem_alloc(boot_order_alloc);
    }

    if (EFI_ERROR(status)) {
        pmm_free(boot_order, boot_order_alloc);
        panic(true, "efi_boot_entry: Failed to get BootOrder variable (%X)",
            (uint64_t)status);
    }

    size_t count = size / sizeof(uint16_t);

    for (size_t i = 0; i < count; i++) {
        EFI_CHAR16 var_name[9];

        format_boot_var(var_name, boot_order[i]);

        EFI_UINTN buf_size = 0;
        /* Query for buf size */
        if (gRT->GetVariable(var_name, &global_variable, NULL, &buf_size, NULL)
                != EFI_BUFFER_TOO_SMALL) {
            continue;
        }
        size_t buf_alloc = buf_size;
        uint8_t *buf = ext_mem_alloc(buf_alloc);
        status = gRT->GetVariable(var_name, &global_variable, NULL, &buf_size, buf);

        if (EFI_ERROR(status)) {
            pmm_free(buf, buf_alloc);
            continue;
        }
        /* Get the description */
        if (buf_size < sizeof(uint32_t) + sizeof(uint16_t) + sizeof(EFI_CHAR16)) {
            pmm_free(buf, buf_alloc);
            continue;
        }
        size_t desc_offset = sizeof(uint32_t) + sizeof(uint16_t);
        EFI_CHAR16 *desc = (EFI_CHAR16 *)(buf + desc_offset);
        size_t desc_max_chars = (buf_size - desc_offset) / sizeof(EFI_CHAR16);
        if (uefi_string_matches(desc, desc_max_chars, entry)) {
            *out = boot_order[i];
            pmm_free(buf, buf_alloc);
            pmm_free(boot_order, boot_order_alloc);
            return true;
        }
        pmm_free(buf, buf_alloc);
    }

    pmm_free(boot_order, boot_order_alloc);
    return false;
}

noreturn void efi_boot_entry(char *config) {
    char *boot_entry = config_get_value(config, 0, "ENTRY");

    if (boot_entry == NULL) {
        panic(true, "efi_boot_entry: No entry specified");
    }

    /* Convert entry string to UTF-16 */
    size_t boot_entry_len = strlen(boot_entry);
    /* UTF-16 never takes more code units than UTF-8 takes bytes. */
    EFI_CHAR16 *boot_entry_utf16 = ext_mem_alloc_counted(boot_entry_len + 1, sizeof(EFI_CHAR16));

    utf8_to_utf16(boot_entry_utf16, boot_entry_len + 1, boot_entry, boot_entry_len, NULL);

    /* Find the desired boot entry */
    uint16_t boot_next = 0;

    if (!find_boot_entry(boot_entry_utf16, &boot_next)) {
        panic(true, "efi_boot_entry: Failed to find boot entry '%s'", boot_entry);
    }

    EFI_GUID global_variable = EFI_GLOBAL_VARIABLE;

    /* Set BootNext to it */
    EFI_STATUS status = gRT->SetVariable(L"BootNext", &global_variable,
                                       EFI_VARIABLE_NON_VOLATILE |
                                       EFI_VARIABLE_BOOTSERVICE_ACCESS |
                                       EFI_VARIABLE_RUNTIME_ACCESS,
                                       sizeof(boot_next), &boot_next);

    if (status) {
        panic(true, "efi_boot_entry: Failed to set BootNext variable (%X)",
            (uint64_t)status);
    }

    /* Now reboot */
    gRT->ResetSystem(EfiResetWarm, EFI_SUCCESS, 0, NULL);

    panic(true, "efi_boot_entry: Failed to reboot");
}

#endif
