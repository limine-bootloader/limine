#ifndef __LIB__ACPI_H__
#define __LIB__ACPI_H__

#include <stdint.h>
#include <stddef.h>

#define EBDA ((size_t)(*((uint16_t *)0x40e)) * 16)

struct sdt {
    char     signature[4];
    uint32_t length;
    uint8_t  rev;
    uint8_t  checksum;
    char     oem_id[6];
    char     oem_table_id[8];
    uint32_t oem_rev;
    uint32_t creator_id;
    uint32_t creator_rev;
} __attribute__((packed));

struct rsdp {
    char     signature[8];
    uint8_t  checksum;
    char     oem_id[6];
    uint8_t  rev;
    uint32_t rsdt_addr;
} __attribute__((packed));

struct rsdp_rev2 {
    struct rsdp rsdp;
    uint32_t length;
    uint64_t xsdt_addr;
    uint8_t  ext_checksum;
    uint8_t  reserved[3];
} __attribute__((packed));

struct rsdt {
    struct sdt sdt;
    char       ptrs_start[];
} __attribute__((packed));

uint8_t acpi_checksum(void *ptr, size_t size);
void   *acpi_get_rsdp(void);
void   *acpi_get_table(const char *signature);

#endif
