#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdnoreturn.h>
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
#include <sys/smp.h>
#include <sys/cpu.h>
#include <sys/gdt.h>
#include <lib/fb.h>
#include <lib/term.h>
#include <sys/pic.h>
#include <sys/lapic.h>
#include <sys/idt.h>
#include <fs/file.h>
#include <mm/pmm.h>
#include <pxe/tftp.h>
#include <drivers/edid.h>
#include <drivers/vga_textmode.h>
#include <lib/rand.h>
#define LIMINE_NO_POINTERS
#include <protos/limine.h>
#include <limine.h>

#define MAX_REQUESTS 128
#define MAX_MEMMAP 256

static pagemap_t build_pagemap(bool level5pg, struct elf_range *ranges, size_t ranges_count,
                               uint64_t physical_base, uint64_t virtual_base,
                               uint64_t direct_map_offset) {
    pagemap_t pagemap = new_pagemap(level5pg ? 5 : 4);

    if (ranges_count == 0) {
        // Map 0 to 2GiB at 0xffffffff80000000
        for (uint64_t i = 0; i < 0x80000000; i += 0x40000000) {
            map_page(pagemap, 0xffffffff80000000 + i, i, VMM_FLAG_WRITE, Size1GiB);
        }
    } else {
        for (size_t i = 0; i < ranges_count; i++) {
            uint64_t virt = ranges[i].base;
            uint64_t phys;

            if (virt & ((uint64_t)1 << 63)) {
                phys = physical_base + (virt - virtual_base);
            } else {
                panic(false, "limine: Protected memory ranges are only supported for higher half kernels");
            }

            uint64_t pf =
                (ranges[i].permissions & ELF_PF_X ? 0 : VMM_FLAG_NOEXEC) |
                (ranges[i].permissions & ELF_PF_W ? VMM_FLAG_WRITE : 0);

            for (uint64_t j = 0; j < ranges[i].length; j += 0x1000) {
                map_page(pagemap, virt + j, phys + j, pf, Size4KiB);
            }
        }
    }

    // Sub 2MiB mappings
    for (uint64_t i = 0; i < 0x200000; i += 0x1000) {
        if (i != 0) {
            map_page(pagemap, i, i, VMM_FLAG_WRITE, Size4KiB);
        }
        map_page(pagemap, direct_map_offset + i, i, VMM_FLAG_WRITE, Size4KiB);
    }

    // Map 2MiB to 4GiB at higher half base and 0
    //
    // NOTE: We cannot just directly map from 2MiB to 4GiB with 1GiB
    // pages because if you do the math.
    //
    //     start = 0x200000
    //     end   = 0x40000000
    //
    //     pages_required = (end - start) / (4096 * 512 * 512)
    //
    // So we map 2MiB to 1GiB with 2MiB pages and then map the rest
    // with 1GiB pages :^)
    for (uint64_t i = 0x200000; i < 0x40000000; i += 0x200000) {
        map_page(pagemap, i, i, VMM_FLAG_WRITE, Size2MiB);
        map_page(pagemap, direct_map_offset + i, i, VMM_FLAG_WRITE, Size2MiB);
    }

    for (uint64_t i = 0x40000000; i < 0x100000000; i += 0x40000000) {
        map_page(pagemap, i, i, VMM_FLAG_WRITE, Size1GiB);
        map_page(pagemap, direct_map_offset + i, i, VMM_FLAG_WRITE, Size1GiB);
    }

    size_t _memmap_entries = memmap_entries;
    struct memmap_entry *_memmap =
        ext_mem_alloc(_memmap_entries * sizeof(struct memmap_entry));
    for (size_t i = 0; i < _memmap_entries; i++)
        _memmap[i] = memmap[i];

    // Map any other region of memory from the memmap
    for (size_t i = 0; i < _memmap_entries; i++) {
        uint64_t base   = _memmap[i].base;
        uint64_t length = _memmap[i].length;
        uint64_t top    = base + length;

        if (base < 0x100000000)
            base = 0x100000000;

        if (base >= top)
            continue;

        uint64_t aligned_base   = ALIGN_DOWN(base, 0x40000000);
        uint64_t aligned_top    = ALIGN_UP(top, 0x40000000);
        uint64_t aligned_length = aligned_top - aligned_base;

        for (uint64_t j = 0; j < aligned_length; j += 0x40000000) {
            uint64_t page = aligned_base + j;
            map_page(pagemap, page, page, VMM_FLAG_WRITE, Size1GiB);
            map_page(pagemap, direct_map_offset + page, page, VMM_FLAG_WRITE, Size1GiB);
        }
    }

    return pagemap;
}

#if defined (__x86_64__) || defined (__i386__)
extern symbol limine_spinup_32;
#elif defined (__aarch64__)

#define LIMINE_SCTLR ((1 << 29) /* Res1 */                \
                    | (1 << 28) /* Res1 */                \
                    | (1 << 23) /* Res1 */                \
                    | (1 << 22) /* Res1 */                \
                    | (1 << 20) /* Res1 */                \
                    | (1 << 12) /* I-Cache */             \
                    | (1 << 11) /* Res1 */                \
                    | (1 << 8)  /* Res1 */                \
                    | (1 << 7)  /* Res1 */                \
                    | (1 << 4)  /* SP0 Alignment check */ \
                    | (1 << 3)  /* SP Alignment check */  \
                    | (1 << 2)  /* D-Cache */             \
                    | (1 << 0)) /* MMU */                 \

#define LIMINE_MAIR ( (0b11111111 << 0)   /* Normal WB RW-allocate non-transient */ \
                    | (0b01000100 << 8) ) /* Normal NC */

#define LIMINE_TCR(tsz, pa) ( ((uint64_t)(pa) << 32)         /* Intermediate address size */  \
                            | ((uint64_t)2 << 30)            /* TTBR1 4K granule */           \
                            | ((uint64_t)2 << 28)            /* TTBR1 Inner shareable */      \
                            | ((uint64_t)1 << 26)            /* TTBR1 Outer WB RW-Allocate */ \
                            | ((uint64_t)1 << 24)            /* TTBR1 Inner WB RW-Allocate */ \
                            | ((uint64_t)(tsz) << 16)        /* Address bits in TTBR1 */      \
                                                             /* TTBR0 4K granule */           \
                            | ((uint64_t)2 << 12)            /* TTBR0 Inner shareable */      \
                            | ((uint64_t)1 << 10)            /* TTBR0 Outer WB RW-Allocate */ \
                            | ((uint64_t)1 << 8)             /* TTBR0 Inner WB RW-Allocate */ \
                            | ((uint64_t)(tsz) << 0))        /* Address bits in TTBR0 */

#else
#error Unknown architecture
#endif

static uint64_t physical_base, virtual_base, slide, direct_map_offset;
static size_t requests_count;
static void **requests;

static uint64_t reported_addr(void *addr) {
    return (uint64_t)(uintptr_t)addr + direct_map_offset;
}

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

    ret.path = reported_addr(file->path);

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
extern symbol limine_term_write_entry;
void *limine_rt_stack = NULL;
uint64_t limine_term_callback_ptr = 0;
uint64_t limine_term_write_ptr = 0;
void limine_term_callback(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
#endif

static void term_write_shim(uint64_t context, uint64_t buf, uint64_t count) {
    (void)context;
    term_write(buf, count);
}

noreturn void limine_load(char *config, char *cmdline) {
#if defined (__x86_64__) || defined (__i386__)
    uint32_t eax, ebx, ecx, edx;
#endif

    char *kernel_path = config_get_value(config, 0, "KERNEL_PATH");
    if (kernel_path == NULL)
        panic(true, "limine: KERNEL_PATH not specified");

    print("limine: Loading kernel `%s`...\n", kernel_path);

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
        panic(true, "limine: Kernel in unrecognised format");
    }

    // ELF loading
    uint64_t entry_point = 0;
    struct elf_range *ranges;
    uint64_t ranges_count;

    uint64_t image_size;
    bool is_reloc;

    if (elf64_load(kernel, &entry_point, NULL, &slide,
                   MEMMAP_KERNEL_AND_MODULES, kaslr,
                   &ranges, &ranges_count,
                   true, &physical_base, &virtual_base, &image_size,
                   &is_reloc)) {
        panic(true, "limine: ELF64 load failure");
    }

    kaslr = kaslr && is_reloc;

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

#if defined (__x86_64__) || defined (__i386__)
    // Check if 64 bit CPU
    if (!cpuid(0x80000001, 0, &eax, &ebx, &ecx, &edx) || !(edx & (1 << 29))) {
        panic(true, "limine: This CPU does not support 64-bit mode.");
    }
#endif

    printv("limine: Physical base:   %X\n", physical_base);
    printv("limine: Virtual base:    %X\n", virtual_base);
    printv("limine: Slide:           %X\n", slide);
    printv("limine: ELF entry point: %X\n", entry_point);
    printv("limine: Requests count:  %u\n", requests_count);

    // 5 level paging feature & HHDM slide
    bool want_5lv;
FEAT_START
    // Check if 5-level paging is available
    bool level5pg = false;
    // TODO(qookie): aarch64 also has optional 5 level paging when using 4K pages
#if defined (__x86_64__) || defined (__i386__)
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

#if defined (__aarch64__)
    uint64_t aa64mmfr0;
    asm volatile ("mrs %0, id_aa64mmfr0_el1" : "=r" (aa64mmfr0));

    uint64_t pa = aa64mmfr0 & 0xF;

    uint64_t tsz = 64 - (want_5lv ? 57 : 48);
#endif

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

    // Device tree blob feature
FEAT_START
    struct limine_dtb_request *dtb_request = get_request(LIMINE_DTB_REQUEST);
    if (dtb_request == NULL) {
        break; // next feature
    }

#if uefi == 1
    struct limine_dtb_response *dtb_response =
        ext_mem_alloc(sizeof(struct limine_dtb_response));

    // TODO: Looking for the DTB should be moved out of here and into lib/, because:
    // 1. We will need it for core bring-up for the SMP request.
    // 2. We will need to patch it for the Linux boot protocol to set the initramfs
    //    and boot arguments.
    // 3. If Limine is ported to platforms that use a DTB but do not use UEFI, it will
    //    need to be found in a different way.
    const EFI_GUID dtb_guid = EFI_DTB_TABLE_GUID;

    // Look for the DTB in the configuration tables
    for (size_t i = 0; i < gST->NumberOfTableEntries; i++) {
        EFI_CONFIGURATION_TABLE *cur_table = &gST->ConfigurationTable[i];

        if (memcmp(&cur_table->VendorGuid, &dtb_guid, sizeof(EFI_GUID)) == 0)
            dtb_response->dtb_ptr = (uint64_t)(uintptr_t)cur_table->VendorTable;
    }

    dtb_request->response = reported_addr(dtb_response);
#endif

FEAT_END

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

    uint64_t *term_fb_ptr = NULL;

    // Terminal feature
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

    char *term_conf_override_s = config_get_value(config, 0, "TERM_CONFIG_OVERRIDE");
    if (term_conf_override_s != NULL && strcmp(term_conf_override_s, "yes") == 0) {
        term_vbe(config, req_width, req_height);
    } else {
        term_vbe(NULL, req_width, req_height);
    }

    if (current_video_mode < 0) {
        panic(true, "limine: Failed to initialise terminal");
    }

    fb = fbinfo;

    if (terminal_request->callback != 0) {
#if defined (__i386__)
        term_callback = limine_term_callback;
        limine_term_callback_ptr = terminal_request->callback;
#elif defined (__x86_64__) || defined (__aarch64__)
        term_callback = (void *)terminal_request->callback;
#else
#error Unknown architecture
#endif
    }

    term_arg = reported_addr(terminal);

#if defined (__i386__)
    if (limine_rt_stack == NULL) {
        limine_rt_stack = ext_mem_alloc(16384) + 16384;
    }

    limine_term_write_ptr = (uintptr_t)term_write_shim;
    terminal_response->write = (uintptr_t)(void *)limine_term_write_entry;
#elif defined (__x86_64__) || defined (__aarch64__)
    terminal_response->write = (uintptr_t)term_write_shim;
#else
#error Unknown architecture
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

    term_deinit();

    if (!fb_init(&fb, req_width, req_height, req_bpp)) {
        panic(true, "limine: Could not acquire framebuffer");
    }

skip_fb_init:
    memmap_alloc_range(fb.framebuffer_addr,
                       (uint64_t)fb.framebuffer_pitch * fb.framebuffer_height,
                       MEMMAP_FRAMEBUFFER, false, false, false, true);

    // Framebuffer feature
FEAT_START
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

    // Wrap-up stuff before memmap close
#if defined (__x86_64__) || defined (__i386__)
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
    pagemap = build_pagemap(want_5lv, ranges, ranges_count,
                            physical_base, virtual_base, direct_map_offset);

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
#if defined (__x86_64__) || defined (__i386__)
    uint32_t bsp_lapic_id;
    smp_info = init_smp(0, (void **)&smp_array,
                        &cpu_count, &bsp_lapic_id,
                        true, want_5lv,
                        pagemap, smp_request->flags & LIMINE_SMP_X2APIC, true,
                        direct_map_offset, true);
#elif defined (__aarch64__)
    uint64_t bsp_mpidr;

    smp_info = init_smp(0, (void **)&smp_array,
                        &cpu_count, &bsp_mpidr,
                        pagemap, LIMINE_MAIR, LIMINE_TCR(tsz, pa), LIMINE_SCTLR);
#else
#error Unknown architecture
#endif

    if (smp_info == NULL) {
        break;
    }

    for (size_t i = 0; i < cpu_count; i++) {
        void *cpu_stack = ext_mem_alloc(stack_size) + stack_size;
        smp_info[i].stack_addr = reported_addr(cpu_stack + stack_size);
    }

    struct limine_smp_response *smp_response =
        ext_mem_alloc(sizeof(struct limine_smp_response));

#if defined (__x86_64__) || defined (__i386__)
    smp_response->flags |= (smp_request->flags & LIMINE_SMP_X2APIC) && x2apic_check();
    smp_response->bsp_lapic_id = bsp_lapic_id;
#elif defined (__aarch64__)
    smp_response->bsp_mpidr = bsp_mpidr;
#else
#error Unknown architecture
#endif

    uint64_t *smp_list = ext_mem_alloc(cpu_count * sizeof(uint64_t));
    for (size_t i = 0; i < cpu_count; i++) {
        smp_list[i] = reported_addr(&smp_array[i]);
    }

    smp_response->cpu_count = cpu_count;
    smp_response->cpus = reported_addr(smp_list);

    smp_request->response = reported_addr(smp_response);
FEAT_END

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
    struct memmap_entry *mmap = get_memmap(&mmap_entries);

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

#if defined (__x86_64__) || defined (__i386__)
#if bios == 1
    // If we're going 64, we might as well call this BIOS interrupt
    // to tell the BIOS that we are entering Long Mode, since it is in
    // the specification.
    struct rm_regs r = {0};
    r.eax = 0xec00;
    r.ebx = 0x02;   // Long mode only
    rm_int(0x15, &r, &r);
#endif

    vmm_assert_nx();

    pic_mask_all();
    io_apic_mask_all();

    irq_flush_type = IRQ_PIC_APIC_FLUSH;

    uint64_t reported_stack = reported_addr(stack);

    common_spinup(limine_spinup_32, 7,
        want_5lv, (uint32_t)(uintptr_t)pagemap.top_level,
        (uint32_t)entry_point, (uint32_t)(entry_point >> 32),
        (uint32_t)reported_stack, (uint32_t)(reported_stack >> 32),
        (uint32_t)(uintptr_t)local_gdt);
#elif defined (__aarch64__)
    vmm_assert_4k_pages();

    enter_in_el1(entry_point, (uint64_t)stack, LIMINE_SCTLR, LIMINE_MAIR, LIMINE_TCR(tsz, pa),
                 (uint64_t)pagemap.top_level[0],
                 (uint64_t)pagemap.top_level[1], 0);
#else
#error Unknown architecture
#endif
}
