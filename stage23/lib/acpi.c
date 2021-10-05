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

#if bios == 1

void *acpi_get_rsdp(void) {
    size_t ebda = EBDA;

    for (size_t i = ebda; i < 0x100000; i += 16) {
        if (i == ebda + 1024) {
            // We probed the 1st KiB of the EBDA as per spec, move onto 0xe0000
            i = 0xe0000;
        }
        if (!memcmp((char *)i, "RSD PTR ", 8)
         && !acpi_checksum((void *)i, 20)) {
            printv("acpi: Found RSDP at %x\n", i);
            return (void *)i;
        }
    }

    return NULL;
}

/// Returns the RSDP v1 pointer if avaliable or else NULL.
void *acpi_get_rsdp_v1(void) {
    // In BIOS according to the ACPI spec (see ACPI 6.2 section 
    // 5.2.5.1 'Finding the RSDP on IA-PC Systems') it either contains
    // the RSDP or the XSDP and it cannot contain both. So, we directly
    // use acpi_get_rsdp function to find the RSDP and if it has the correct
    // revision, return it.
    struct rsdp *rsdp = acpi_get_rsdp();

    if (rsdp != NULL && rsdp->rev == 1)
        return rsdp;
    
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

#if uefi == 1

#include <efi.h>

void *acpi_get_rsdp(void) {
    EFI_GUID acpi_2_guid = ACPI_20_TABLE_GUID;
    EFI_GUID acpi_1_guid = ACPI_TABLE_GUID;

    void *rsdp = NULL;

    for (size_t i = 0; i < gST->NumberOfTableEntries; i++) {
        EFI_CONFIGURATION_TABLE *cur_table = &gST->ConfigurationTable[i];

        bool is_xsdp = memcmp(&cur_table->VendorGuid, &acpi_2_guid, sizeof(EFI_GUID)) == 0;
        bool is_rsdp = memcmp(&cur_table->VendorGuid, &acpi_1_guid, sizeof(EFI_GUID)) == 0;

        if (!is_xsdp && !is_rsdp)
            continue;
        
        if ((is_xsdp && acpi_checksum(cur_table->VendorTable, sizeof(struct rsdp)) != 0) || // XSDP is 36 bytes wide
            (is_rsdp && acpi_checksum(cur_table->VendorTable, 20) != 0)) // RSDP is 20 bytes wide
            continue;

        printv("acpi: Found %s at %X\n", is_xsdp ? "XSDP" : "RSDP", cur_table->VendorTable);

        // We want to return the XSDP if it exists rather then returning
        // the RSDP. We need to add a check for that since the table entries
        // are not in the same order for all EFI systems since it might be the
        // case where the RSDP ocurs before the XSDP.
        if (rsdp != NULL && is_xsdp) {
            rsdp = (void *)cur_table->VendorTable;
            break; // Found it!.
        } else {
            // Found the RSDP but we continue to loop since we might
            // find the XSDP.
            rsdp = (void *)cur_table->VendorTable;
        }
    }

    return rsdp;
}

/// Returns the RSDP v1 pointer if avaliable or else NULL.
void *acpi_get_rsdp_v1(void) {
    // To maintain GRUB compatibility we will need to probe for the RSDP
    // again since UEFI can contain both XSDP and RSDP (see ACPI 6.2 section 
    // 5.2.5.2 'Finding the RSDP on UEFI Enabled Systems') and in the acpi_get_rsdp
    // function we look for the RSDP with the latest revision.
    EFI_GUID acpi_1_guid = ACPI_TABLE_GUID;

    for (size_t i = 0; i < gST->NumberOfTableEntries; i++) {
        EFI_CONFIGURATION_TABLE *cur_table = &gST->ConfigurationTable[i];

        if (memcmp(&cur_table->VendorGuid, &acpi_1_guid, sizeof(EFI_GUID)) != 0)
            continue;
        
        if (acpi_checksum(cur_table->VendorTable, 20) != 0)
            continue;

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

/// Returns the RSDP v2 pointer if avaliable or else NULL.
void *acpi_get_rsdp_v2(void) {
    // Since the acpi_get_rsdp function already looks for the XSDP we can
    // just check if it has the correct revision and return the pointer :^)
    struct rsdp *rsdp = acpi_get_rsdp();

    if (rsdp != NULL && rsdp->rev >= 2)
        return rsdp;

    return NULL;
}

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
