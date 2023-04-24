#include <pxe/tftp.h>
#include <pxe/pxe.h>
#if defined (BIOS)
#  include <lib/real.h>
#elif defined (UEFI)
#  include <efi.h>
#endif
#include <lib/print.h>
#include <lib/libc.h>
#include <mm/pmm.h>
#include <lib/misc.h>

// cache the dhcp packet
uint8_t cached_dhcp_packet[1472] = { 0 };
int cached_dhcp_packet_len = 0;

#if defined (BIOS)

static uint32_t get_boot_server_info(void) {
    struct pxenv_get_cached_info cachedinfo = { 0 };
    cachedinfo.packet_type = PXENV_PACKET_TYPE_CACHED_REPLY;
    pxe_call(PXENV_GET_CACHED_INFO, ((uint16_t)rm_seg(&cachedinfo)), (uint16_t)rm_off(&cachedinfo));
    struct bootph *ph = (struct bootph*)(void *) (((((uint32_t)cachedinfo.buffer) >> 16) << 4) + (((uint32_t)cachedinfo.buffer) & 0xFFFF));
    memcpy(&cached_dhcp_packet, ph, sizeof(struct bootph));
    cached_dhcp_packet_len = sizeof(struct bootph);
    return ph->sip;
}

static uint32_t parse_ip_addr(const char *server_addr) {
    uint32_t out;

    if (!server_addr || !strlen(server_addr)) {
        return get_boot_server_info();
    }

    if (inet_pton(server_addr, &out)) {
        panic(true, "tftp: Invalid IPv4 address: \"%s\"", server_addr);
    }

    return out;
}

struct file_handle *tftp_open(struct volume *part, const char *server_addr, const char *name) {
    uint32_t server_ip = parse_ip_addr(server_addr);
    const uint16_t server_port = 69; // This couldn't be changed previously either
    int ret = 0;

    (void)part;

    struct PXENV_UNDI_GET_INFORMATION undi_info = { 0 };
    ret = pxe_call(UNDI_GET_INFORMATION, ((uint16_t)rm_seg(&undi_info)), (uint16_t)rm_off(&undi_info));
    if (ret) {
        return NULL;
    }

    //TODO figure out a more proper way to do this.
    uint16_t mtu = undi_info.MaxTranUnit - 48;

    struct pxenv_get_file_size fsize = {
        .status = 0,
        .sip = server_ip,
    };
    strcpy((char*)fsize.name, name);
    ret = pxe_call(TFTP_GET_FILE_SIZE, ((uint16_t)rm_seg(&fsize)), (uint16_t)rm_off(&fsize));
    if (ret) {
        return NULL;
    }

    struct file_handle *handle = ext_mem_alloc(sizeof(struct file_handle));

    handle->size = fsize.file_size;
    handle->is_memfile = true;

    handle->pxe = true;
    handle->pxe_ip = server_ip;
    handle->pxe_port = server_port;

    size_t name_len = strlen(name);
    handle->path = ext_mem_alloc(1 + name_len + 1);
    handle->path[0] = '/';
    memcpy(&handle->path[1], name, name_len);
    handle->path_len = 1 + name_len + 1;

    struct pxenv_open open = {
        .status = 0,
        .sip = server_ip,
        .port = (server_port) << 8,
        .packet_size = mtu
    };
    strcpy((char*)open.name, name);

    ret = pxe_call(TFTP_OPEN, ((uint16_t)rm_seg(&open)), (uint16_t)rm_off(&open));
    if (ret) {
        print("tftp: Failed to open file %x or bad packet size", open.status);
        pmm_free(handle, sizeof(struct file_handle));
        return NULL;
    }

    mtu = open.packet_size;

    uint8_t *buf = conv_mem_alloc(mtu);
    handle->fd = ext_mem_alloc(handle->size);

    size_t progress = 0;
    bool slow = false;

    while (progress < handle->size) {
        struct pxenv_read read = {
            .boff = ((uint16_t)rm_off(buf)),
            .bseg = ((uint16_t)rm_seg(buf)),
        };

        ret = pxe_call(TFTP_READ, ((uint16_t)rm_seg(&read)), (uint16_t)rm_off(&read));
        if (ret) {
            panic(false, "tftp: Read failure");
        }

        memcpy(handle->fd + progress, buf, read.bsize);

        progress += read.bsize;

        if (read.bsize < mtu && !slow && progress < handle->size) {
            slow = true;
            print("tftp: Server is sending the file in smaller packets (it sent %d bytes), download might take longer.\n", read.bsize);
        }
    }

    uint16_t close = 0;
    ret = pxe_call(TFTP_CLOSE, ((uint16_t)rm_seg(&close)), (uint16_t)rm_off(&close));
    if (ret) {
        panic(false, "tftp: Close failure");
    }

    pmm_free(buf, mtu);

    return handle;
}

#elif defined (UEFI)

static EFI_IP_ADDRESS *parse_ip_addr(struct volume *part, const char *server_addr) {
    static EFI_IP_ADDRESS out;

    if (!server_addr || !strlen(server_addr)) {
        EFI_PXE_BASE_CODE_PACKET* packet;
        if (part->pxe_base_code->Mode->PxeReplyReceived) packet = &part->pxe_base_code->Mode->PxeReply;
        else if (part->pxe_base_code->Mode->ProxyOfferReceived) packet = &part->pxe_base_code->Mode->ProxyOffer;
        else packet = &part->pxe_base_code->Mode->DhcpAck;
        memcpy(out.Addr, packet->Dhcpv4.BootpSiAddr, 4);
        memcpy(cached_dhcp_packet, packet, sizeof(EFI_PXE_BASE_CODE_PACKET));
        cached_dhcp_packet_len = sizeof(EFI_PXE_BASE_CODE_PACKET);
    } else {
        if (inet_pton(server_addr, &out.Addr)) {
            panic(true, "tftp: Invalid IPv4 address: \"%s\"", server_addr);
        }
    }

    return &out;
}

struct file_handle *tftp_open(struct volume *part, const char *server_addr, const char *name) {
    if (!part->pxe_base_code) {
        return NULL;
    }

    EFI_IP_ADDRESS *ip = parse_ip_addr(part, server_addr);

    uint64_t file_size;
    EFI_STATUS status;

    status = part->pxe_base_code->Mtftp(
            part->pxe_base_code,
            EFI_PXE_BASE_CODE_TFTP_GET_FILE_SIZE,
            NULL,
            false,
            &file_size,
            NULL,
            ip,
            (uint8_t *)name,
            NULL,
            false);

    if (status) {
        return NULL;
    }

    struct file_handle *handle = ext_mem_alloc(sizeof(struct file_handle));

    handle->size = file_size;
    handle->is_memfile = true;

    handle->pxe = true;
    handle->pxe_ip = *(uint32_t *)&ip;
    handle->pxe_port = 69;

    size_t name_len = strlen(name);
    handle->path = ext_mem_alloc(1 + name_len + 1);
    handle->path[0] = '/';
    memcpy(&handle->path[1], name, name_len);
    handle->path_len = 1 + name_len + 1;

    handle->fd = ext_mem_alloc(handle->size);

    status = part->pxe_base_code->Mtftp(
            part->pxe_base_code,
            EFI_PXE_BASE_CODE_TFTP_READ_FILE,
            handle->fd,
            false,
            &file_size,
            NULL,
            ip,
            (uint8_t *)name,
            NULL,
            false);

    if (status) {
        return NULL;
    }

    return handle;
}

#endif
