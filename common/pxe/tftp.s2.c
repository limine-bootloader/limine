#if bios == 1

#include <pxe/tftp.h>
#include <pxe/pxe.h>
#include <lib/real.h>
#include <lib/print.h>
#include <lib/libc.h>
#include <mm/pmm.h>
#include <lib/blib.h>

uint32_t get_boot_server_info(void) {
    struct pxenv_get_cached_info cachedinfo = { 0 };
    cachedinfo.packet_type = 2;
    pxe_call(PXENV_GET_CACHED_INFO, ((uint16_t)rm_seg(&cachedinfo)), (uint16_t)rm_off(&cachedinfo));
    struct bootph *ph = (struct bootph*)(void *) (((((uint32_t)cachedinfo.buffer) >> 16) << 4) + (((uint32_t)cachedinfo.buffer) & 0xFFFF));
    return ph->sip;
}

bool tftp_open(struct file_handle *handle, uint32_t server_ip, uint16_t server_port, const char *name) {
    int ret = 0;

    if (!server_ip) {
        server_ip = get_boot_server_info();
    }

    struct PXENV_UNDI_GET_INFORMATION undi_info = { 0 };
    ret = pxe_call(UNDI_GET_INFORMATION, ((uint16_t)rm_seg(&undi_info)), (uint16_t)rm_off(&undi_info));
    if (ret) {
        return false;
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
        return false;
    }

    handle->size = fsize.file_size;
    handle->is_memfile = true;

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
        return false;
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

    return true;
}

#endif
