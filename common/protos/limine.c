#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <config.h>
#include <lib/elf.h>
#include <lib/blib.h>
#include <lib/acpi.h>
#include <lib/config.h>
#include <lib/time.h>
#include <lib/print.h>
#include <lib/real.h>
#include <lib/libc.h>
#include <lib/gterm.h>
#include <lib/uri.h>
#include <lib/fb.h>
#include <lib/term.h>
#include <fs/file.h>
#include <mm/pmm.h>
#include <stivale2.h>
#include <pxe/tftp.h>
#include <drivers/edid.h>
#include <drivers/vga_textmode.h>
#include <lib/rand.h>
#include <sys/smp.h>
#include <protos/stivale.h>

#if port_x86
#include <arch/x86/lapic.h>
#include <arch/x86/pic.h>
#include <arch/x86/cpu.h>
#include <arch/x86/gdt.h>
#elif port_aarch64
#include <arch/aarch64/spinup.h>
#endif

#define LIMINE_NO_POINTERS
#include <protos/limine.h>
#include <limine.h>

#define MAX_REQUESTS 128
#define MAX_MEMMAP 256

static uint64_t physical_base, virtual_base, slide, direct_map_offset;
static size_t requests_count;
static void **requests;

static uint64_t reported_addr(void *addr) {
    return (uint64_t)(uintptr_t)addr + direct_map_offset;
}

struct fdt_header {
    uint32_t magic;
    uint32_t totalsize;
    uint32_t off_dt_struct;
    uint32_t off_dt_strings;
    uint32_t off_mem_rsvmap;
    uint32_t version;
    uint32_t last_comp_version;
    uint32_t boot_cpuid_phys;
    uint32_t size_dt_strings;
    uint32_t size_dt_struct;
};

static struct limine_file get_file(struct file_handle *file, char *cmdline) {
    struct limine_file ret = {0};

    if (file->pxe) {
        ret.media_type = LIMINE_MEDIA_TYPE_TFTP;

        ret.tftp_ip = file->pxe_ip;
        ret.tftp_port = file->pxe_port;
    } else {
        struct volume *vol = file->vol;

        if (vol->is_optical) {
            ret.media_type = LIMINE_MEDIA_TYPE_OPTICAL;
        }

        ret.partition_index = vol->partition;

        ret.mbr_disk_id = mbr_get_id(vol);

        if (vol->guid_valid) {
            memcpy(&ret.part_uuid, &vol->guid, sizeof(struct limine_uuid));
        }

        if (vol->part_guid_valid) {
            memcpy(&ret.gpt_part_uuid, &vol->part_guid, sizeof(struct limine_uuid));
        }

        struct guid gpt_disk_uuid;
        if (gpt_get_guid(&gpt_disk_uuid, vol->backing_dev ?: vol) == true) {
            memcpy(&ret.gpt_disk_uuid, &gpt_disk_uuid, sizeof(struct limine_uuid));
        }
    }

    char *path = ext_mem_alloc(strlen(file->path) + 1);
    strcpy(path, file->path);
    ret.path = reported_addr(path);

    ret.address = reported_addr(freadall(file, MEMMAP_KERNEL_AND_MODULES));
    ret.size = file->size;

    ret.cmdline = reported_addr(cmdline);

    return ret;
}

static void *_get_request(uint64_t id[4]) {
    for (size_t i = 0; i < requests_count; i++) {
        uint64_t *p = requests[i];

        if (p[2] != id[2]) {
            continue;
        }
        if (p[3] != id[3]) {
            continue;
        }

        return p;
    }

    return NULL;
}

#define get_request(REQ) _get_request((uint64_t[4])REQ)

#define FEAT_START do {
#define FEAT_END } while (0);

#if defined (__i386__)
extern symbol stivale2_term_write_entry;
extern void *stivale2_rt_stack;
extern uint64_t stivale2_term_callback_ptr;
extern uint64_t stivale2_term_write_ptr;
void stivale2_term_callback(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
#endif

static void term_write_shim(uint64_t context, uint64_t buf, uint64_t count) {
    (void)context;
    term_write(buf, count);
}

bool limine_load(char *config, char *cmdline) {
#if port_x86
    uint32_t eax, ebx, ecx, edx;
#endif

    char *kernel_path = config_get_value(config, 0, "KERNEL_PATH");
    if (kernel_path == NULL)
        panic(true, "limine: KERNEL_PATH not specified");

    struct file_handle *kernel_file;
    if ((kernel_file = uri_open(kernel_path)) == NULL)
        panic(true, "limine: Failed to open kernel with path `%s`. Is the path correct?", kernel_path);

    uint8_t *kernel = freadall(kernel_file, MEMMAP_BOOTLOADER_RECLAIMABLE);

    char *kaslr_s = config_get_value(config, 0, "KASLR");
    bool kaslr = true;
    if (kaslr_s != NULL && strcmp(kaslr_s, "no") == 0)
        kaslr = false;

    int bits = elf_bits(kernel);

    if (bits == -1 || bits == 32) {
        printv("limine: Kernel in unrecognised format");
        return false;
    }

    // ELF loading
    uint64_t entry_point = 0;
    struct elf_range *ranges;
    uint64_t ranges_count;

    uint64_t image_size;
    bool is_reloc;

    if (elf64_load(kernel, &entry_point, NULL, &slide,
                   MEMMAP_KERNEL_AND_MODULES, kaslr, false,
                   &ranges, &ranges_count,
                   true, &physical_base, &virtual_base, &image_size,
                   &is_reloc)) {
        return false;
    }

    kaslr = is_reloc;

    // Load requests
    if (elf64_load_section(kernel, &requests, ".limine_reqs", 0, slide) == 0) {
        for (size_t i = 0; ; i++) {
            if (requests[i] == NULL) {
                break;
            }
            requests[i] -= virtual_base;
            requests[i] += physical_base;
            requests_count++;
        }
    } else {
        requests = ext_mem_alloc(MAX_REQUESTS * sizeof(void *));
        requests_count = 0;
        uint64_t common_magic[2] = { LIMINE_COMMON_MAGIC };
        for (size_t i = 0; i < ALIGN_DOWN(image_size, 8); i += 8) {
            uint64_t *p = (void *)(uintptr_t)physical_base + i;

            if (p[0] != common_magic[0]) {
                continue;
            }
            if (p[1] != common_magic[1]) {
                continue;
            }

            if (requests_count == MAX_REQUESTS) {
                panic(true, "limine: Maximum requests exceeded");
            }

            // Check for a conflict
            if (_get_request(p) != NULL) {
                panic(true, "limine: Conflict detected for request ID %X %X", p[2], p[3]);
            }

            requests[requests_count++] = p;
        }
    }

    if (requests_count == 0) {
        return false;
    }

#if port_x86
    // Check if 64 bit CPU
    if (!cpuid(0x80000001, 0, &eax, &ebx, &ecx, &edx) || !(edx & (1 << 29))) {
        panic(true, "limine: This CPU does not support 64-bit mode.");
    }
#endif

    print("limine: Loading kernel `%s`...\n", kernel_path);

    printv("limine: Physical base:   %X\n", physical_base);
    printv("limine: Virtual base:    %X\n", virtual_base);
    printv("limine: Slide:           %X\n", slide);
    printv("limine: ELF entry point: %X\n", entry_point);
    printv("limine: Requests count:  %u\n", requests_count);

    // 5 level paging feature & HHDM slide
    bool want_5lv = false;
FEAT_START
    bool level5pg = false;
#if port_x86
    // Check if 5-level paging is available
    if (cpuid(0x00000007, 0, &eax, &ebx, &ecx, &edx) && (ecx & (1 << 16))) {
        printv("limine: CPU has 5-level paging support\n");
        level5pg = true;
    }
#endif

    struct limine_5_level_paging_request *lv5pg_request = get_request(LIMINE_5_LEVEL_PAGING_REQUEST);
    want_5lv = lv5pg_request != NULL && level5pg;

    direct_map_offset = want_5lv ? 0xff00000000000000 : 0xffff800000000000;

    if (kaslr) {
        direct_map_offset += (rand64() & ~((uint64_t)0x40000000 - 1)) & 0xfffffffffff;
    }

    if (want_5lv) {
        void *lv5pg_response = ext_mem_alloc(sizeof(struct limine_5_level_paging_response));
        lv5pg_request->response = reported_addr(lv5pg_response);
    }
FEAT_END

    struct limine_file *kf = ext_mem_alloc(sizeof(struct limine_file));
    *kf = get_file(kernel_file, cmdline);
    fclose(kernel_file);

    // Entry point feature
FEAT_START
    struct limine_entry_point_request *entrypoint_request = get_request(LIMINE_ENTRY_POINT_REQUEST);
    if (entrypoint_request == NULL) {
        break;
    }

    entry_point = entrypoint_request->entry;

    printv("limine: Entry point at %X\n", entry_point);

    struct limine_entry_point_response *entrypoint_response =
        ext_mem_alloc(sizeof(struct limine_entry_point_response));

    entrypoint_request->response = reported_addr(entrypoint_response);
FEAT_END

    // Bootloader info feature
FEAT_START
    struct limine_bootloader_info_request *bootloader_info_request = get_request(LIMINE_BOOTLOADER_INFO_REQUEST);
    if (bootloader_info_request == NULL) {
        break; // next feature
    }

    struct limine_bootloader_info_response *bootloader_info_response =
        ext_mem_alloc(sizeof(struct limine_bootloader_info_response));

    bootloader_info_response->name = reported_addr("Limine");
    bootloader_info_response->version = reported_addr(LIMINE_VERSION);

    bootloader_info_request->response = reported_addr(bootloader_info_response);
FEAT_END

    // Kernel address feature
FEAT_START
    struct limine_kernel_address_request *kernel_address_request = get_request(LIMINE_KERNEL_ADDRESS_REQUEST);
    if (kernel_address_request == NULL) {
        break; // next feature
    }

    struct limine_kernel_address_response *kernel_address_response =
        ext_mem_alloc(sizeof(struct limine_kernel_address_response));

    kernel_address_response->physical_base = physical_base;
    kernel_address_response->virtual_base = virtual_base;

    kernel_address_request->response = reported_addr(kernel_address_response);
FEAT_END

    // HHDM feature
FEAT_START
    struct limine_hhdm_request *hhdm_request = get_request(LIMINE_HHDM_REQUEST);
    if (hhdm_request == NULL) {
        break; // next feature
    }

    struct limine_hhdm_response *hhdm_response =
        ext_mem_alloc(sizeof(struct limine_hhdm_response));

    hhdm_response->offset = direct_map_offset;

    hhdm_request->response = reported_addr(hhdm_response);
FEAT_END

    // RSDP feature
FEAT_START
    struct limine_rsdp_request *rsdp_request = get_request(LIMINE_RSDP_REQUEST);
    if (rsdp_request == NULL) {
        break; // next feature
    }

    struct limine_rsdp_response *rsdp_response =
        ext_mem_alloc(sizeof(struct limine_rsdp_response));

    void *rsdp = acpi_get_rsdp();
    if (rsdp) {
        rsdp_response->address = reported_addr(rsdp);
    }

    rsdp_request->response = reported_addr(rsdp_response);
FEAT_END

    // SMBIOS feature
FEAT_START
    struct limine_smbios_request *smbios_request = get_request(LIMINE_SMBIOS_REQUEST);
    if (smbios_request == NULL) {
        break; // next feature
    }

    struct limine_smbios_response *smbios_response =
        ext_mem_alloc(sizeof(struct limine_smbios_response));

    void *smbios_entry_32 = NULL, *smbios_entry_64 = NULL;
    acpi_get_smbios(&smbios_entry_32, &smbios_entry_64);

    if (smbios_entry_32) {
        smbios_response->entry_32 = reported_addr(smbios_entry_32);
    }
    if (smbios_entry_64) {
        smbios_response->entry_64 = reported_addr(smbios_entry_64);
    }

    smbios_request->response = reported_addr(smbios_response);
FEAT_END


#if uefi == 1
    // EFI system table feature
FEAT_START
    struct limine_efi_system_table_request *est_request = get_request(LIMINE_EFI_SYSTEM_TABLE_REQUEST);
    if (est_request == NULL) {
        break; // next feature
    }

    struct limine_efi_system_table_response *est_response =
        ext_mem_alloc(sizeof(struct limine_efi_system_table_response));

    est_response->address = reported_addr(gST);

    est_request->response = reported_addr(est_response);
FEAT_END
#endif

    // Stack size
    uint64_t stack_size = 65536;
FEAT_START
    struct limine_stack_size_request *stack_size_request = get_request(LIMINE_STACK_SIZE_REQUEST);
    if (stack_size_request == NULL) {
        break; // next feature
    }

    struct limine_stack_size_response *stack_size_response =
        ext_mem_alloc(sizeof(struct limine_stack_size_response));

    if (stack_size_request->stack_size > stack_size) {
        stack_size = stack_size_request->stack_size;
    }

    stack_size_request->response = reported_addr(stack_size_response);
FEAT_END

    // Kernel file
FEAT_START
    struct limine_kernel_file_request *kernel_file_request = get_request(LIMINE_KERNEL_FILE_REQUEST);
    if (kernel_file_request == NULL) {
        break; // next feature
    }

    struct limine_kernel_file_response *kernel_file_response =
        ext_mem_alloc(sizeof(struct limine_kernel_file_response));

    kernel_file_response->kernel_file = reported_addr(kf);

    kernel_file_request->response = reported_addr(kernel_file_response);
FEAT_END

    // Modules
FEAT_START
    struct limine_module_request *module_request = get_request(LIMINE_MODULE_REQUEST);
    if (module_request == NULL) {
        break; // next feature
    }

    size_t module_count;
    for (module_count = 0; ; module_count++) {
        char *module_file = config_get_value(config, module_count, "MODULE_PATH");
        if (module_file == NULL)
            break;
    }

    if (module_count == 0) {
        break;
    }

    struct limine_module_response *module_response =
        ext_mem_alloc(sizeof(struct limine_module_response));

    struct limine_file *modules = ext_mem_alloc(module_count * sizeof(struct limine_file));

    for (size_t i = 0; i < module_count; i++) {
        struct conf_tuple conf_tuple =
                config_get_tuple(config, i,
                                 "MODULE_PATH", "MODULE_CMDLINE");

        char *module_path = conf_tuple.value1;
        char *module_cmdline = conf_tuple.value2;

        if (module_cmdline == NULL) {
            module_cmdline = "";
        }

        print("limine: Loading module `%s`...\n", module_path);

        struct file_handle *f;
        if ((f = uri_open(module_path)) == NULL)
            panic(true, "limine: Failed to open module with path `%s`. Is the path correct?", module_path);

        struct limine_file *l = &modules[i];
        *l = get_file(f, module_cmdline);

        fclose(f);
    }

    uint64_t *modules_list = ext_mem_alloc(module_count * sizeof(uint64_t));
    for (size_t i = 0; i < module_count; i++) {
        modules_list[i] = reported_addr(&modules[i]);
    }

    module_response->module_count = module_count;
    module_response->modules = reported_addr(modules_list);

    module_request->response = reported_addr(module_response);
FEAT_END

    size_t req_width = 0, req_height = 0, req_bpp = 0;

    char *resolution = config_get_value(config, 0, "RESOLUTION");
    if (resolution != NULL) {
        parse_resolution(&req_width, &req_height, &req_bpp, resolution);
    }

    struct fb_info fb;

    // Terminal feature
    uint64_t *term_fb_ptr = NULL;
FEAT_START
    struct limine_terminal_request *terminal_request = get_request(LIMINE_TERMINAL_REQUEST);
    if (terminal_request == NULL) {
        break; // next feature
    }

    struct limine_terminal_response *terminal_response =
        ext_mem_alloc(sizeof(struct limine_terminal_response));

    struct limine_terminal *terminal = ext_mem_alloc(sizeof(struct limine_terminal));

    quiet = false;
    serial = false;

    term_vbe(req_width, req_height);

    if (current_video_mode < 0) {
        panic(true, "limine: Failed to initialise terminal");
    }

    fb = fbinfo;

    if (terminal_request->callback != 0) {
#if defined (__i386__)
        term_callback = stivale2_term_callback;
        stivale2_term_callback_ptr = terminal_request->callback;
#elif defined (__x86_64__)
        term_callback = (void *)terminal_request->callback;
#endif
    }

    term_arg = reported_addr(terminal);

#if defined (__i386__)
    if (stivale2_rt_stack == NULL) {
        stivale2_rt_stack = ext_mem_alloc(16384) + 16384;
    }

    stivale2_term_write_ptr = (uintptr_t)term_write_shim;
    terminal_response->write = (uintptr_t)(void *)stivale2_term_write_entry;
#elif defined (__x86_64__) || defined (__aarch64__)
    terminal_response->write = (uintptr_t)term_write_shim;
#endif

    term_fb_ptr = &terminal->framebuffer;

    terminal->columns = term_cols;
    terminal->rows = term_rows;

    uint64_t *term_list = ext_mem_alloc(1 * sizeof(uint64_t));
    term_list[0] = reported_addr(terminal);

    terminal_response->terminal_count = 1;
    terminal_response->terminals = reported_addr(term_list);

    terminal_request->response = reported_addr(terminal_response);

    goto skip_fb_init;
FEAT_END

    // Framebuffer feature
FEAT_START
    term_deinit();

    if (!fb_init(&fb, req_width, req_height, req_bpp)) {
        panic(true, "limine: Could not acquire framebuffer");
    }

skip_fb_init:;
    memmap_alloc_range(fb.framebuffer_addr,
                       (uint64_t)fb.framebuffer_pitch * fb.framebuffer_height,
                       MEMMAP_FRAMEBUFFER, false, false, false, true);

    // For now we only support 1 framebuffer
    struct limine_framebuffer *fbp = ext_mem_alloc(sizeof(struct limine_framebuffer));

    if (term_fb_ptr != NULL) {
        *term_fb_ptr = reported_addr(fbp);
    }

    struct limine_framebuffer_request *framebuffer_request = get_request(LIMINE_FRAMEBUFFER_REQUEST);
    if (framebuffer_request == NULL) {
        break; // next feature
    }

    struct limine_framebuffer_response *framebuffer_response =
        ext_mem_alloc(sizeof(struct limine_framebuffer_response));

    struct edid_info_struct *edid_info = get_edid_info();
    if (edid_info != NULL) {
        fbp->edid_size = sizeof(struct edid_info_struct);
        fbp->edid = reported_addr(edid_info);
    }

    fbp->memory_model     = LIMINE_FRAMEBUFFER_RGB;
    fbp->address          = reported_addr((void *)(uintptr_t)fb.framebuffer_addr);
    fbp->width            = fb.framebuffer_width;
    fbp->height           = fb.framebuffer_height;
    fbp->bpp              = fb.framebuffer_bpp;
    fbp->pitch            = fb.framebuffer_pitch;
    fbp->red_mask_size    = fb.red_mask_size;
    fbp->red_mask_shift   = fb.red_mask_shift;
    fbp->green_mask_size  = fb.green_mask_size;
    fbp->green_mask_shift = fb.green_mask_shift;
    fbp->blue_mask_size   = fb.blue_mask_size;
    fbp->blue_mask_shift  = fb.blue_mask_shift;

    uint64_t *fb_list = ext_mem_alloc(1 * sizeof(uint64_t));
    fb_list[0] = reported_addr(fbp);

    framebuffer_response->framebuffer_count = 1;
    framebuffer_response->framebuffers = reported_addr(fb_list);

    framebuffer_request->response = reported_addr(framebuffer_response);
FEAT_END

    // Boot time feature
FEAT_START
    struct limine_boot_time_request *boot_time_request = get_request(LIMINE_BOOT_TIME_REQUEST);
    if (boot_time_request == NULL) {
        break; // next feature
    }

    struct limine_boot_time_response *boot_time_response =
        ext_mem_alloc(sizeof(struct limine_boot_time_response));

    boot_time_response->boot_time = time();

    boot_time_request->response = reported_addr(boot_time_response);
FEAT_END

#if port_x86
    // Wrap-up stuff before memmap close
    struct gdtr *local_gdt = ext_mem_alloc(sizeof(struct gdtr));
    local_gdt->limit = gdt.limit;
    uint64_t local_gdt_base = (uint64_t)gdt.ptr;
    local_gdt_base += direct_map_offset;
    local_gdt->ptr = local_gdt_base;
#if defined (__i386__)
    local_gdt->ptr_hi = local_gdt_base >> 32;
#endif
#endif

    void *stack = ext_mem_alloc(stack_size) + stack_size;

    pagemap_t pagemap = {0};
#if port_x86
    pagemap = stivale_build_pagemap(want_5lv, true, ranges, ranges_count, true,
                                    physical_base, virtual_base, direct_map_offset);
#elif port_aarch64
    pagemap = stivale_build_pagemap(true, ranges, ranges_count, true,
                                    physical_base, virtual_base, direct_map_offset);
#endif

#if uefi == 1
    efi_exit_boot_services();
#endif

    // SMP
FEAT_START
    struct limine_smp_request *smp_request = get_request(LIMINE_SMP_REQUEST);
    if (smp_request == NULL) {
        break; // next feature
    }

    struct limine_smp_info *smp_array;
    struct smp_information *smp_info;
    size_t cpu_count;
    uint32_t bsp_lapic_id;
    smp_info = init_smp(0, (void **)&smp_array,
                        &cpu_count, &bsp_lapic_id,
                        true, want_5lv,
                        pagemap, smp_request->flags & LIMINE_SMP_X2APIC, true,
                        direct_map_offset, true);

    if (smp_info == NULL) {
        break;
    }

    for (size_t i = 0; i < cpu_count; i++) {
        void *cpu_stack = ext_mem_alloc(stack_size) + stack_size;
        smp_info[i].stack_addr = reported_addr(cpu_stack + stack_size);
    }

    struct limine_smp_response *smp_response =
        ext_mem_alloc(sizeof(struct limine_smp_response));

#if port_x86
    smp_response->flags |= (smp_request->flags & LIMINE_SMP_X2APIC) && x2apic_check();
#endif
    smp_response->bsp_lapic_id = bsp_lapic_id;

    uint64_t *smp_list = ext_mem_alloc(cpu_count * sizeof(uint64_t));
    for (size_t i = 0; i < cpu_count; i++) {
        smp_list[i] = reported_addr(&smp_array[i]);
    }

    smp_response->cpu_count = cpu_count;
    smp_response->cpus = reported_addr(smp_list);

    smp_request->response = reported_addr(smp_response);

FEAT_END

#if uefi
    // DTB
FEAT_START
    struct limine_dtb_request *dtb_request = get_request(LIMINE_DTB_REQUEST);
    struct limine_dtb_response *dtb_response;

    void* dtb = NULL;
    for (uint64_t i = 0;i < gST->NumberOfTableEntries;i++) {
        if (!memcmp(&gST->ConfigurationTable[i].VendorGuid, &(EFI_GUID)EFI_DTB_TABLE_GUID, sizeof(EFI_GUID))) {
            dtb = gST->ConfigurationTable[i].VendorTable;
        }
    }
    if (!dtb) {
        dtb_request->response = 0;
        break;
    }

    if (dtb_request != NULL) {
        dtb_response = ext_mem_alloc(sizeof(struct limine_dtb_response));
        memmap_alloc_range((uint64_t)(size_t)(dtb), bswap32(*(uint32_t*)(dtb + 4)), MEMMAP_BOOTLOADER_RECLAIMABLE, false, false, false, true);
    } else {
        break;
    }

    dtb_response->dtb = reported_addr(dtb);
    dtb_response->dtb_size = bswap32(*(uint32_t*)(dtb + 4));
    dtb_request->response = reported_addr(dtb_response);
FEAT_END
#endif

    // Memmap
FEAT_START
    struct limine_memmap_request *memmap_request = get_request(LIMINE_MEMMAP_REQUEST);
    struct limine_memmap_response *memmap_response;
    struct limine_memmap_entry *_memmap;
    uint64_t *memmap_list;

    if (memmap_request != NULL) {
        memmap_response = ext_mem_alloc(sizeof(struct limine_memmap_response));
        _memmap = ext_mem_alloc(sizeof(struct limine_memmap_entry) * MAX_MEMMAP);
        memmap_list = ext_mem_alloc(MAX_MEMMAP * sizeof(uint64_t));
    }

    size_t mmap_entries;
    struct e820_entry_t *mmap = get_memmap(&mmap_entries);

    if (memmap_request == NULL) {
        break; // next feature
    }

    if (mmap_entries > MAX_MEMMAP) {
        panic(false, "limine: Too many memmap entries");
    }

    for (size_t i = 0; i < mmap_entries; i++) {
        _memmap[i].base = mmap[i].base;
        _memmap[i].length = mmap[i].length;

        switch (mmap[i].type) {
            case MEMMAP_USABLE:
                _memmap[i].type = LIMINE_MEMMAP_USABLE;
                break;
            case MEMMAP_ACPI_RECLAIMABLE:
                _memmap[i].type = LIMINE_MEMMAP_ACPI_RECLAIMABLE;
                break;
            case MEMMAP_ACPI_NVS:
                _memmap[i].type = LIMINE_MEMMAP_ACPI_NVS;
                break;
            case MEMMAP_BAD_MEMORY:
                _memmap[i].type = LIMINE_MEMMAP_BAD_MEMORY;
                break;
            case MEMMAP_BOOTLOADER_RECLAIMABLE:
                _memmap[i].type = LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE;
                break;
            case MEMMAP_KERNEL_AND_MODULES:
                _memmap[i].type = LIMINE_MEMMAP_KERNEL_AND_MODULES;
                break;
            case MEMMAP_FRAMEBUFFER:
                _memmap[i].type = LIMINE_MEMMAP_FRAMEBUFFER;
                break;
            default:
            case MEMMAP_RESERVED:
                _memmap[i].type = LIMINE_MEMMAP_RESERVED;
                break;
        }
    }

    for (size_t i = 0; i < mmap_entries; i++) {
        memmap_list[i] = reported_addr(&_memmap[i]);
    }

    memmap_response->entry_count = mmap_entries;
    memmap_response->entries = reported_addr(memmap_list);

    memmap_request->response = reported_addr(memmap_response);
FEAT_END

    // Clear terminal for kernels that will use the Limine terminal
    term_write((uint64_t)(uintptr_t)("\e[2J\e[H"), 7);

    term_runtime = true;

#if port_x86
    stivale_spinup(64, want_5lv, &pagemap, entry_point, 0,
                   reported_addr(stack), true, true, (uintptr_t)local_gdt);
#elif port_aarch64
    common_spinup(&pagemap, entry_point, 0, reported_addr(stack), true);
#endif

    __builtin_unreachable();
}
