#if defined (bios)

#include <pxe/tftp.h>
#include <pxe/pxe.h>
#include <lib/real.h>
#include <lib/print.h>
#include <lib/libc.h>
#include <mm/pmm.h>
#include <lib/blib.h>

uint32_t get_boot_server_info() {
    struct pxenv_get_cached_info cachedinfo = { 0 };
    cachedinfo.packet_type = 2;
    pxe_call(PXENV_GET_CACHED_INFO, ((uint16_t)rm_seg(&cachedinfo)), (uint16_t)rm_off(&cachedinfo));
    struct bootph *ph = (struct bootph*)(void *) (((((uint32_t)cachedinfo.buffer) >> 16) << 4) + (((uint32_t)cachedinfo.buffer) & 0xFFFF));
    return ph->sip;
}

int tftp_open(struct tftp_file_handle *handle, uint32_t server_ip, uint16_t server_port, const char *name) {
    int ret = 0;
    if (!server_ip) {
        struct pxenv_get_cached_info cachedinfo = { 0 };
        cachedinfo.packet_type = 2;
        pxe_call(PXENV_GET_CACHED_INFO, ((uint16_t)rm_seg(&cachedinfo)), (uint16_t)rm_off(&cachedinfo));
        struct bootph *ph = (struct bootph*)(void *) (((((uint32_t)cachedinfo.buffer) >> 16) << 4) + (((uint32_t)cachedinfo.buffer) & 0xFFFF));
        server_ip = ph->sip;
    }

    struct PXENV_UNDI_GET_INFORMATION undi_info = { 0 };
    ret = pxe_call(UNDI_GET_INFORMATION, ((uint16_t)rm_seg(&undi_info)), (uint16_t)rm_off(&undi_info));
    if (ret) {
        return -1;
    }

    //TODO figure out a more proper way to do this.
    uint16_t mtu = undi_info.MaxTranUnit - 48;

    handle->server_ip = server_ip;
    handle->server_port = server_port;
    handle->packet_size = mtu;

    struct pxenv_get_file_size fsize = {
        .status = 0,
        .sip = server_ip,
    };
    strcpy((char*)fsize.name, name);
    ret = pxe_call(TFTP_GET_FILE_SIZE, ((uint16_t)rm_seg(&fsize)), (uint16_t)rm_off(&fsize));
    if (ret) {
        return -1;
    }

    handle->file_size = fsize.file_size;

    volatile struct pxenv_open open = {
        .status = 0,
        .sip = server_ip,
        .port = (server_port) << 8,
        .packet_size = mtu
    };
    strcpy((char*)open.name, name);
    ret = pxe_call(TFTP_OPEN, ((uint16_t)rm_seg(&open)), (uint16_t)rm_off(&open));
    if (ret) {
        print("failed to open file %x or bad packet size", open.status);
        return -1;
    }
    mtu = open.packet_size;

    uint8_t *buf = conv_mem_alloc(mtu);
    handle->data = ext_mem_alloc(handle->file_size);
    memset(handle->data, 0, handle->file_size);
    size_t to_transfer = handle->file_size;
    size_t progress = 0;

    bool slow = false;

    while (to_transfer > 0) {
        volatile struct pxenv_read read = {
            .boff = ((uint16_t)rm_off(buf)),
            .bseg = ((uint16_t)rm_seg(buf)),
        };
        ret = pxe_call(TFTP_READ, ((uint16_t)rm_seg(&read)), (uint16_t)rm_off(&read));
        if (ret) {
            panic("failed reading");
        }
        memcpy(handle->data + progress, buf, read.bsize);

        if (read.bsize < mtu && !slow) {
            slow = true;
            print("Server is sending the file in smaller packets (it sent %d bytes), download might take longer.\n", read.bsize);
        }
        to_transfer -= read.bsize;
        progress += read.bsize;
    }

    uint16_t close = 0;
    ret = pxe_call(TFTP_CLOSE, ((uint16_t)rm_seg(&close)), (uint16_t)rm_off(&close));
    if (ret) {
        panic("close failed");
    }
    return 0;
}

int tftp_read(void* fd, void *buf, uint64_t loc, uint64_t count) {
    struct tftp_file_handle *handle = (struct tftp_file_handle*)fd;
    if ((loc + count) > handle->file_size) {
        return -1;
    }
    memcpy(buf, handle->data + loc, count);
    return 0;
}

#endif
