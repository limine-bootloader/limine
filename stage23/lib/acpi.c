#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <lib/acpi.h>
#include <lib/blib.h>
#include <lib/libc.h>
#include <lib/print.h>

// Following function based on https://github.com/managarm/lai/blob/master/helpers/pc-bios.c's function lai_bios_calc_checksum()
uint8_t acpi_checksum(void *ptr, size_t size) {
    uint8_t sum = 0, *_ptr = ptr;
    for (size_t i = 0; i < size; i++)
        sum += _ptr[i];
    return sum;
}

#if defined (bios)

void *acpi_get_rsdp(void) {
    size_t ebda = EBDA;

    for (size_t i = ebda; i < 0x100000; i += 16) {
        if (i == ebda + 1024) {
            // We probed the 1st KiB of the EBDA as per spec, move onto 0xe0000
            i = 0xe0000;
        }
        if (!memcmp((char *)i, "RSD PTR ", 8)
         && !acpi_checksum((void *)i, sizeof(struct rsdp))) {
            printv("acpi: Found RSDP at %x\n", i);
            return (void *)i;
        }
    }

    return NULL;
}

void acpi_get_smbios(void **smbios32, void **smbios64) {
    *smbios32 = NULL;
    *smbios64 = NULL;

    for (size_t i = 0xf0000; i < 0x100000; i += 16) {
        if (!memcmp((char *)i, "_SM_", 4)
         && !acpi_checksum((void *)i, *((uint8_t *)(i + 5)))) {
            printv("acpi: Found SMBIOS 32-bit entry point at %x\n", i);
            *smbios32 = (void *)i;
            break;
        }
    }

    for (size_t i = 0xf0000; i < 0x100000; i += 16) {
        if (!memcmp((char *)i, "_SM3_", 5)
         && !acpi_checksum((void *)i, *((uint8_t *)(i + 6)))) {
            printv("acpi: Found SMBIOS 64-bit entry point at %x\n", i);
            *smbios64 = (void *)i;
            break;
        }
    }
}

#endif

#if defined (uefi)

#include <efi.h>

void *acpi_get_rsdp(void) {
    for (size_t i = 0; i < gST->NumberOfTableEntries; i++) {
        EFI_CONFIGURATION_TABLE *cur_table = &gST->ConfigurationTable[i];
        EFI_GUID acpi_2_guid = ACPI_20_TABLE_GUID;

        if (memcmp(&cur_table->VendorGuid, &acpi_2_guid, sizeof(EFI_GUID)) != 0)
            continue;

        if (acpi_checksum(cur_table->VendorTable, sizeof(struct rsdp)) != 0)
            continue;

        printv("acpi: Found RSDP at %X\n", cur_table->VendorTable);

        return (void *)cur_table->VendorTable;
    }

    return NULL;
}

void acpi_get_smbios(void **smbios32, void **smbios64) {
    *smbios32 = NULL;
    *smbios64 = NULL;

    for (size_t i = 0; i < gST->NumberOfTableEntries; i++) {
        EFI_CONFIGURATION_TABLE *cur_table = &gST->ConfigurationTable[i];
        EFI_GUID smbios_guid = SMBIOS_TABLE_GUID;

        if (memcmp(&cur_table->VendorGuid, &smbios_guid, sizeof(EFI_GUID)) != 0)
            continue;

        if (acpi_checksum(cur_table->VendorTable,
                          *((uint8_t *)(cur_table->VendorTable + 5))) != 0)
            continue;

        printv("acpi: Found SMBIOS 32-bit entry point at %X\n", cur_table->VendorTable);

        *smbios32 = cur_table->VendorTable;

        break;
    }

    for (size_t i = 0; i < gST->NumberOfTableEntries; i++) {
        EFI_CONFIGURATION_TABLE *cur_table = &gST->ConfigurationTable[i];
        EFI_GUID smbios3_guid = SMBIOS3_TABLE_GUID;

        if (memcmp(&cur_table->VendorGuid, &smbios3_guid, sizeof(EFI_GUID)) != 0)
            continue;

        if (acpi_checksum(cur_table->VendorTable,
                          *((uint8_t *)(cur_table->VendorTable + 6))) != 0)
            continue;

        printv("acpi: Found SMBIOS 64-bit entry point at %X\n", cur_table->VendorTable);

        *smbios64 = cur_table->VendorTable;

        break;
    }
}

#endif

void *acpi_get_table(const char *signature, int index) {
    int cnt = 0;

    struct rsdp *rsdp = acpi_get_rsdp();
    if (rsdp == NULL)
        return NULL;

    bool use_xsdt = false;
    if (rsdp->rev >= 2 && rsdp->xsdt_addr)
        use_xsdt = true;

    struct rsdt *rsdt;
    if (use_xsdt)
        rsdt = (struct rsdt *)(uintptr_t)rsdp->xsdt_addr;
    else
        rsdt = (struct rsdt *)(uintptr_t)rsdp->rsdt_addr;

    size_t entry_count =
        (rsdt->header.length - sizeof(struct sdt)) / (use_xsdt ? 8 : 4);

    for (size_t i = 0; i < entry_count; i++) {
        struct sdt *ptr;
        if (use_xsdt)
            ptr = (struct sdt *)(uintptr_t)((uint64_t *)rsdt->ptrs_start)[i];
        else
            ptr = (struct sdt *)(uintptr_t)((uint32_t *)rsdt->ptrs_start)[i];

        if (!memcmp(ptr->signature, signature, 4)
         && !acpi_checksum(ptr, ptr->length)
         && cnt++ == index) {
            printv("acpi: Found \"%s\" at %x\n", signature, ptr);
            return ptr;
        }
    }

    printv("acpi: \"%s\" not found\n", signature);
    return NULL;
}
