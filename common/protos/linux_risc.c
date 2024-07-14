#if defined(__riscv64) || defined(__aarch64__)

#include <stdint.h>
#include <stddef.h>
#include <stdnoreturn.h>
#include <protos/linux.h>
#include <fs/file.h>
#include <lib/libc.h>
#include <lib/misc.h>
#include <lib/term.h>
#include <lib/config.h>
#include <lib/print.h>
#include <lib/uri.h>
#include <mm/pmm.h>
#include <sys/idt.h>
#include <lib/fb.h>
#include <lib/acpi.h>
#include <lib/fdt.h>
#include <libfdt/libfdt.h>

// The following definitions and struct were copied and adapted from Linux
// kernel headers released under GPL-2.0 WITH Linux-syscall-note
// allowing their inclusion in non GPL compliant code.

struct linux_header {
    uint32_t code0;
    uint32_t code1;
    uint64_t text_offset;
    uint64_t image_size;
    uint64_t flags;
    uint32_t version;
    uint32_t res1;
    uint64_t res2;
    uint64_t res3;          // originally 'magic' field, deprecated
    uint32_t magic2;
    uint32_t res4;
} __attribute__((packed));

// End of Linux code

#if defined(__riscv64)
#define LINUX_HEADER_MAGIC2             0x05435352
#define LINUX_HEADER_MAJOR_VER(ver)     (((ver) >> 16) & 0xffff)
#define LINUX_HEADER_MINOR_VER(ver)     (((ver) >> 0)  & 0xffff)
#elif defined(__aarch64__)
#define LINUX_HEADER_MAGIC2             0x644d5241
#endif

void add_framebuffer(struct fb_info *fb) {
    struct screen_info *screen_info = ext_mem_alloc(sizeof(struct screen_info));

    screen_info->capabilities   = VIDEO_CAPABILITY_64BIT_BASE | VIDEO_CAPABILITY_SKIP_QUIRKS;
    screen_info->flags          = VIDEO_FLAGS_NOCURSOR;
    screen_info->lfb_base       = (uint32_t)fb->framebuffer_addr;
    screen_info->ext_lfb_base   = (uint32_t)(fb->framebuffer_addr >> 32);
    screen_info->lfb_size       = fb->framebuffer_pitch * fb->framebuffer_height;
    screen_info->lfb_width      = fb->framebuffer_width;
    screen_info->lfb_height     = fb->framebuffer_height;
    screen_info->lfb_depth      = fb->framebuffer_bpp;
    screen_info->lfb_linelength = fb->framebuffer_pitch;
    screen_info->red_size       = fb->red_mask_size;
    screen_info->red_pos        = fb->red_mask_shift;
    screen_info->green_size     = fb->green_mask_size;
    screen_info->green_pos      = fb->green_mask_shift;
    screen_info->blue_size      = fb->blue_mask_size;
    screen_info->blue_pos       = fb->blue_mask_shift;

    screen_info->orig_video_isVGA = VIDEO_TYPE_EFI;

    EFI_GUID screen_info_table_guid = {0xe03fc20a, 0x85dc, 0x406e, {0xb9, 0x0e, 0x4a, 0xb5, 0x02, 0x37, 0x1d, 0x95}};
    EFI_STATUS ret = gBS->InstallConfigurationTable(&screen_info_table_guid, screen_info);

    if (ret != EFI_SUCCESS) {
        panic(true, "linux: failed to install screen info configuration table: '%x'", ret);
    }
}

void *prepare_device_tree_blob(char *config, char *cmdline) {
    // Hopefully 4K should be enough (mainly depends on the length of cmdline).
    void *dtb = get_device_tree_blob(0x1000);
    int ret;

    // Delete all /memory@... nodes. Linux will use the given UEFI memory map
    // instead.
    while (true) {
        int offset = fdt_subnode_offset_namelen(dtb, 0, "memory@", 7);

        if (offset == -FDT_ERR_NOTFOUND) {
            break;
        }

        if (offset < 0) {
            panic(true, "linux: failed to find node: '%s'", fdt_strerror(offset));
        }

        ret = fdt_del_node(dtb, offset);
        if (ret < 0) {
            panic(true, "linux: failed to delete memory node: '%s'", fdt_strerror(ret));
        }
    }

    // Load an initrd if requested and add it to the device tree.
    char *module_path = config_get_value(config, 0, "MODULE_PATH");
    if (module_path) {
        print("linux: Loading module `%#`...\n", module_path);

        struct file_handle *module_file = uri_open(module_path);
        if (!module_file) {
            panic(true, "linux: failed to open module `%s`. Is the path correct?", module_path);
        }

        size_t module_size = module_file->size;
        void *module_base = ext_mem_alloc_type_aligned(
                        ALIGN_UP(module_size, 4096),
                        MEMMAP_KERNEL_AND_MODULES, 4096);

        fread(module_file, module_base, 0, module_size);
        fclose(module_file);
        printv("linux: loaded module `%s` at %x, size %u\n", module_path, module_base, module_size);

        ret = fdt_set_chosen_uint64(dtb, "linux,initrd-start", (uint64_t)module_base);
        if (ret < 0) {
            panic(true, "linux: cannot set initrd parameter: '%s'", fdt_strerror(ret));
        }

        ret = fdt_set_chosen_uint64(dtb, "linux,initrd-end", (uint64_t)(module_base + module_size));
        if (ret < 0) {
            panic(true, "linux: cannot set initrd parameter: '%s'", fdt_strerror(ret));
        }
    }

    size_t req_width = 0, req_height = 0, req_bpp = 0;

    char *resolution = config_get_value(config, 0, "RESOLUTION");
    if (resolution != NULL) {
        parse_resolution(&req_width, &req_height, &req_bpp, resolution);
    }

    struct fb_info *fbs;
    size_t fbs_count;

    term_notready();

    fb_init(&fbs, &fbs_count, req_width, req_height, req_bpp);

    // TODO(qookie): Let the user pick a framebuffer if there's > 1
    if (fbs_count > 0) {
        add_framebuffer(&fbs[0]);
    }

    // Set the kernel command line arguments.
    ret = fdt_set_chosen_string(dtb, "bootargs", cmdline);
    if (ret < 0) {
        panic(true, "linux: failed to set bootargs: '%s'", fdt_strerror(ret));
    }

    efi_exit_boot_services();

    // Tell Linux about the UEFI memory map and system table.
    ret = fdt_set_chosen_uint64(dtb, "linux,uefi-system-table", (uint64_t)gST);
    if (ret < 0) {
        panic(true, "linux: failed to set UEFI system table pointer: '%s'", fdt_strerror(ret));
    }

    ret = fdt_set_chosen_uint64(dtb, "linux,uefi-mmap-start", (uint64_t)efi_mmap);
    if (ret < 0) {
        panic(true, "linux: failed to set UEFI memory map pointer: '%s'", fdt_strerror(ret));
    }

    ret = fdt_set_chosen_uint32(dtb, "linux,uefi-mmap-size", efi_mmap_size);
    if (ret < 0) {
        panic(true, "linux: failed to set UEFI memory map size: '%s'", fdt_strerror(ret));
    }

    ret = fdt_set_chosen_uint32(dtb, "linux,uefi-mmap-desc-size", efi_desc_size);
    if (ret < 0) {
        panic(true, "linux: failed to set UEFI memory map descriptor size: '%s'", fdt_strerror(ret));
    }

    ret = fdt_set_chosen_uint32(dtb, "linux,uefi-mmap-desc-ver", efi_desc_ver);
    if (ret < 0) {
        panic(true, "linux: failed to set UEFI memory map descriptor version: '%s'", fdt_strerror(ret));
    }

    // This property is not required by mainline Linux, but is required by
    // Debian (and derivative) kernels, because Debian has a patch that adds
    // this flag, and the existing logic that deals with it will just outright
    // fail if any of the properties is missing.  We don't care about Debian's
    // hardening or whatever, so just always report that secure boot is off.
    ret = fdt_set_chosen_uint32(dtb, "linux,uefi-secure-boot", 0);
    if (ret < 0) {
        panic(true, "linux: failed to set UEFI secure boot state: '%s'", fdt_strerror(ret));
    }

    size_t efi_mmap_entry_count = efi_mmap_size / efi_desc_size;
    for (size_t i = 0; i < efi_mmap_entry_count; i++) {
        EFI_MEMORY_DESCRIPTOR *entry = (void *)efi_mmap + i * efi_desc_size;

        if (entry->Attribute & EFI_MEMORY_RUNTIME)
            entry->VirtualStart = entry->PhysicalStart;
    }

    EFI_STATUS status = gRT->SetVirtualAddressMap(efi_mmap_size, efi_desc_size, efi_desc_ver, efi_mmap);
    if (status != EFI_SUCCESS) {
        panic(false, "linux: failed to set UEFI virtual address map: '%x'", status);
    }

    return dtb;
}

noreturn void linux_load(char *config, char *cmdline) {
    struct file_handle *kernel_file;

    char *kernel_path = config_get_value(config, 0, "KERNEL_PATH");
    if (kernel_path == NULL) {
        panic(true, "linux: KERNEL_PATH not specified");
    }

    print("linux: Loading kernel `%#`...\n", kernel_path);

    if ((kernel_file = uri_open(kernel_path)) == NULL) {
        panic(true, "linux: failed to open kernel `%s`. Is the path correct?", kernel_path);
    }

    struct linux_header header;
    fread(kernel_file, &header, 0, sizeof(header));

    if (header.magic2 != LINUX_HEADER_MAGIC2) {
        panic(true, "linux: kernel header magic does not match");
    }

    // Version fields are RV-specific
#if defined(__riscv64)
    printv("linux: boot protocol version %d.%d\n",
           LINUX_HEADER_MAJOR_VER(header.version),
           LINUX_HEADER_MINOR_VER(header.version));
    if (LINUX_HEADER_MAJOR_VER(header.version) == 0
     && LINUX_HEADER_MINOR_VER(header.version) < 2) {
        panic(true, "linux: protocols < 0.2 are not supported");
    }
#endif

    size_t kernel_size = kernel_file->size;
    void *kernel_base = ext_mem_alloc_type_aligned(
                ALIGN_UP(kernel_size, 4096),
                MEMMAP_KERNEL_AND_MODULES, 2 * 1024 * 1024);
    fread(kernel_file, kernel_base, 0, kernel_size);
    fclose(kernel_file);
    printv("linux: loaded kernel `%s` at %x, size %u\n", kernel_path, kernel_base, kernel_size);

    void *dtb = prepare_device_tree_blob(config, cmdline);
    if (!dtb) {
        panic(true, "linux: failed to prepare the device tree blob");
    }

#if defined(__riscv64)
    printv("linux: bsp hart %d, device tree blob at %x\n", bsp_hartid, dtb);

    void (*kernel_entry)(uint64_t hartid, uint64_t dtb) = kernel_base;
    asm ("csrci   sstatus, 0x2\n\t"
         "csrw    sie, zero\n\t");
    kernel_entry(bsp_hartid, (uint64_t)dtb);
#elif defined(__aarch64__)
    printv("linux: device tree blob at %x\n", dtb);

    void (*kernel_entry)(uint64_t dtb, uint64_t res0, uint64_t res1, uint64_t res2) = kernel_base;
    asm ("msr daifset, 0xF");
    kernel_entry((uint64_t)dtb, 0, 0, 0);
#endif
    __builtin_unreachable();
}

#endif
