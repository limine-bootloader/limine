#include <stdint.h>
#include <lib/libc.h>
#include <lib/print.h>
#include <mm/pmm.h>
#include <lib/blib.h>

#if port_x86
#include <arch/x86/cpu.h>
#elif port_aarch64
#include <arch/aarch64/dtb.h>
#endif

struct fw_cfg_file {
    uint32_t size;
    uint16_t select;
    uint16_t reserved;
    char name[56];
};
struct fw_cfg_files {
    uint32_t count;
    struct fw_cfg_file f[];
};

struct fw_cfg_context {
#if port_aarch64
    void* fwcfg_address;
#endif
};

static void fwcfg_disp_read(struct fw_cfg_context* ctx, uint16_t sel, uint32_t outsz, uint8_t* outbuf) {
#if port_x86
    outw(0x510, sel);
    for (uint32_t i = 0;i < outsz;i++) outbuf[i] = inb(0x511);
#elif port_aarch64
    *(volatile uint16_t*)(ctx->fwcfg_address + 8) = bswap16(sel);
    for (uint32_t i = 0;i < outsz;i++) outbuf[i] = *(volatile uint8_t*)ctx->fwcfg_address;
#endif
}

static struct fw_cfg_files* filebuf = NULL;
static const char* simple_mode_config = 
    "TIMEOUT=0\n"
    ":simple mode config\n"
    "KERNEL_PATH=fwcfg:///opt/org.limine-bootloader.kernel";

static const char* simple_mode_bg_config = 
    "TIMEOUT=0\n"
    "GRAPHICS=yes\n"
    "THEME_BACKGROUND=50000000\n"
    "BACKGROUND_PATH=fwcfg:///opt/org.limine-bootloader.background\n"
    "BACKGROUND_STYLE=stretched\n"
    ":simple mode config\n"
    "KERNEL_PATH=fwcfg:///opt/org.limine-bootloader.kernel";

static bool simple_mode = false;

static bool fwcfg_do_open(struct fw_cfg_context* ctx, struct file_handle *handle, const char *name) {
    char sig[5] = { 0 };
    fwcfg_disp_read(ctx, /* signature */ 0x0000, 4, (uint8_t*)sig);
    if (strcmp(sig, "QEMU")) return false;
    
    uint32_t count;
    fwcfg_disp_read(ctx, 0x0019, 4, (uint8_t*)&count);
    count = bswap32(count);
    
    if (!filebuf) {
        filebuf = (struct fw_cfg_files*)ext_mem_alloc(count * 64 + 4);
        fwcfg_disp_read(ctx, 0x0019, count * 64 + 4, (uint8_t*)filebuf);
    }

    bool has_kernel = false, has_background = false;
    for (uint32_t i = 0;i < count;i++) {
        if (!strncmp(filebuf->f[i].name, name, 56)) {
            uint16_t sel = bswap16(filebuf->f[i].select);
            handle->size = bswap32(filebuf->f[i].size);
                handle->is_memfile = true;
            uint8_t* buf = (uint8_t*)(handle->fd = ext_mem_alloc(handle->size));
            fwcfg_disp_read(ctx, sel, handle->size, buf);
            return true;
        }
        if (!strncmp(filebuf->f[i].name, "opt/org.limine-bootloader.background", 56)) {
            has_background = true;
        }
        if (!strncmp(filebuf->f[i].name, "opt/org.limine-bootloader.kernel", 56)) {
            has_kernel = true;
        }
    }
    
    if (has_kernel && !strcmp(name, "opt/org.limine-bootloader.config")) {
        const char* conf = has_background ? simple_mode_bg_config : simple_mode_config;
        handle->size = strlen(conf);
            handle->is_memfile = true;
        char* buf = (char*)(handle->fd = ext_mem_alloc(handle->size + 1));
        strcpy(buf, conf);
        simple_mode = true;
        return true;
    }

    return false;
}

#if port_x86
bool fwcfg_open(struct file_handle *handle, const char *path) {
    return fwcfg_do_open(NULL, handle, path);
}
#elif port_aarch64
bool fwcfg_open(struct file_handle *handle, const char *path) {
    static struct fw_cfg_context context = {.fwcfg_address=NULL};
    static bool initialized_context = false;

    if (initialized_context && !context.fwcfg_address) return false;
    if (initialized_context) return fwcfg_do_open(&context, handle, path);

    struct dtb dtb;
    if (!dtb_init(&dtb)) return false;

    struct dtb_ref r = dtb_get_root(&dtb);
    const char* name;
    dtb_next_node(&r, &name, &r);
    struct dtb_ref node;

    uint32_t aclsz;
    void* aclp;
    if (!dtb_get_prop(&r, "#address-cells", &aclsz, &aclp)) return false;
    if (aclsz != 4) return false;
    uint32_t acl = bswap32(*(uint32_t*)aclp);
    if (acl != 2) return false;

    void* fwcfg_base = NULL;
    while (dtb_next_node(&r, &name, &node)) {
        void* cdata;
        uint32_t csize;
        if (!dtb_get_prop(&node, "compatible", &csize, &cdata)) continue;
        if (*(uint8_t*)(cdata + csize - 1)) continue; // nope: needs a null terminator

        for (void* cur = cdata; cur < cdata+csize;cur += strlen(cur)+1) {
            if (!strcmp(cur, "qemu,fw-cfg-mmio")) {
                void* rdata;
                uint32_t rsize;
                if (!dtb_get_prop(&node, "reg", &rsize, &rdata)) continue;
                if (rsize < 8) continue;
                uint64_t ptr = bswap64(*(uint64_t*)rdata);
                fwcfg_base = (void*)ptr;
            }
        }
    }
    if (!fwcfg_base) return false;

    initialized_context = true;
    context.fwcfg_address = fwcfg_base;
    return fwcfg_do_open(&context, handle, path);
}
#endif
