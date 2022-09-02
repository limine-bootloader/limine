#ifndef __LIB__ACPI_H__
#define __LIB__ACPI_H__

#include <stdint.h>
#include <stddef.h>
#include <sys/cpu.h>

#define EBDA (ebda_get())

#if defined (BIOS)
static inline uintptr_t ebda_get(void) {
    uintptr_t ebda = (uintptr_t)mminw(0x40e) << 4;

    // Sanity checks
    if (ebda < 0x80000 || ebda >= 0xa0000) {
        ebda = 0x80000;
    }

    return ebda;
}
#endif

struct sdt {
    char     signature[4];
    uint32_t length;
    uint8_t  rev;
    uint8_t  checksum;
    char     oem_id[6];
    char     oem_table_id[8];
    uint32_t oem_rev;
    char     creator_id[4];
    uint32_t creator_rev;
} __attribute__((packed));

struct rsdp {
    char     signature[8];
    uint8_t  checksum;
    char     oem_id[6];
    uint8_t  rev;
    uint32_t rsdt_addr;
    // Revision 2 only after this comment
    uint32_t length;
    uint64_t xsdt_addr;
    uint8_t  ext_checksum;
    uint8_t  reserved[3];
} __attribute__((packed));

struct rsdt {
    struct sdt header;
    char ptrs_start[];
} __attribute__((packed));

struct smbios_entry_point_32 {
    char anchor_str[4];
    /// This value summed with all the values of the table.
    uint8_t checksum;
    /// Length of the entry point table.
    uint8_t length;
    /// Major version of SMBIOS.
    uint8_t major_version;
    /// Minor version of SMBIOS.
    uint8_t minor_version;
    /// Size of the largest SMBIOS structure, in bytes, and encompasses the
    /// structure’s formatted area and text strings
    uint16_t max_structure_size;
    uint8_t entry_point_revision;
    char formatted_area[5];

    char intermediate_anchor_str[5];
    /// Checksum for values from intermediate anchor str to the
    /// end of table.
    uint8_t intermediate_checksum;
    /// Total length of SMBIOS Structure Table, pointed to by the structure
    /// table address, in bytes.
    uint16_t table_length;
    /// 32-bit physical starting address of the read-only SMBIOS Structure
    /// Table.
    uint32_t table_address;
    /// Total number of structures present in the SMBIOS Structure Table.
    uint16_t number_of_structures;
    /// Indicates compliance with a revision of this specification.
    uint8_t bcd_revision;
} __attribute__((packed));

struct smbios_entry_point_64 {
    char anchor_str[5];
    /// This value summed with all the values of the table.
    uint8_t checksum;
    /// Length of the entry point table.
    uint8_t length;
    /// Major version of SMBIOS.
    uint8_t major_version;
    /// Minor version of SMBIOS.
    uint8_t minor_version;
    uint8_t docrev;
    uint8_t entry_point_revision;
    uint8_t reserved;
    /// Size of the largest SMBIOS structure, in bytes, and encompasses the
    /// structure’s formatted area and text strings
    uint16_t max_structure_size;
    /// 64-bit physical starting address of the read-only SMBIOS Structure
    /// Table.
    uint64_t table_address;
} __attribute__((packed));

uint8_t acpi_checksum(void *ptr, size_t size);
void   *acpi_get_rsdp(void);

void   *acpi_get_rsdp_v1(void);
void   *acpi_get_rsdp_v2(void);

void   *acpi_get_table(const char *signature, int index);
void    acpi_get_smbios(void **smbios32, void **smbios64);

#endif
