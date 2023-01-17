#ifndef PXE_H
#define PXE_H

#include <stdint.h>
#include <lib/part.h>

#if defined (BIOS)

struct volume *pxe_bind_volume(void);
void pxe_init(void);
int pxe_call(uint16_t opcode, uint16_t buf_seg, uint16_t buf_off);

#define MAC_ADDR_LEN 16
typedef uint8_t MAC_ADDR_t[MAC_ADDR_LEN];

struct bootph {
    uint8_t opcode;
    uint8_t Hardware;
    uint8_t Hardlen;
    uint8_t Gatehops;
    uint32_t ident;
    uint16_t seconds;
    uint16_t Flags;
    uint32_t cip;
    uint32_t yip;
    uint32_t sip;
    uint32_t gip;
    MAC_ADDR_t CAddr;
    uint8_t Sname[64];
    uint8_t bootfile[128];
    union bootph_vendor {
        uint8_t d[1024];
        struct bootph_vendor_v {
            uint8_t magic[4];
            uint32_t flags;
            uint8_t pad[56];
        } v;
    } vendor;
};

 struct PXENV_UNDI_GET_INFORMATION {
    uint16_t Status;
    uint16_t BaseIo;
    uint16_t IntNumber;
    uint16_t MaxTranUnit;
    uint16_t HwType;
    uint16_t HwAddrLen;
    uint8_t CurrentNodeAddress[16];
    uint8_t PermNodeAddress[16];
    uint16_t ROMAddress;
    uint16_t RxBufCt;
    uint16_t TxBufCt;
 };

#define PXE_SIGNATURE "PXENV+"
struct pxenv {
    uint8_t signature[6];
    uint16_t version;
    uint8_t length;
    uint8_t checksum;
    uint32_t rm_entry;
    uint32_t pm_offset;
    uint16_t pm_selector;
    uint16_t stack_seg;
    uint16_t stack_size;
    uint16_t bc_code_seg;
    uint16_t bc_code_size;
    uint16_t bc_data_seg;
    uint16_t bc_data_size;
    uint16_t undi_data_seg;
    uint16_t undi_data_size;
    uint16_t undi_code_seg;
    uint16_t undi_code_size;
    uint32_t pxe_ptr;
} __attribute__((packed));

#define PXE_BANGPXE_SIGNATURE "!PXE"
struct bangpxe {
    uint8_t signature[4];
    uint8_t length;
    uint8_t chksum;
    uint8_t rev;
    uint8_t reserved;
    uint32_t undiromid;
    uint32_t baseromid;
    uint32_t rm_entry;
    uint32_t pm_entry;
} __attribute__((packed));

#define PXENV_GET_CACHED_INFO 0x0071
struct pxenv_get_cached_info {
    uint16_t status;
    uint16_t packet_type;
    uint16_t buffer_size;
    uint32_t buffer;
    uint16_t buffer_limit;
} __attribute__((packed));

#elif defined (UEFI)

struct volume *pxe_bind_volume(EFI_HANDLE efi_handle, EFI_PXE_BASE_CODE *pxe_base_code);

#endif

#endif
