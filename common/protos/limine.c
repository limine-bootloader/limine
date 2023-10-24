#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdnoreturn.h>
#include <config.h>
#include <lib/elf.h>
#include <lib/misc.h>
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
#include <flanterm/backends/fb.h>
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

static pagemap_t build_pagemap(int paging_mode, bool nx, struct elf_range *ranges, size_t ranges_count,
                               uint64_t physical_base, uint64_t virtual_base,
                               uint64_t direct_map_offset) {
    pagemap_t pagemap = new_pagemap(paging_mode);

    if (ranges_count == 0) {
        panic(true, "limine: ranges_count == 0");
    }

    for (size_t i = 0; i < ranges_count; i++) {
        uint64_t virt = ranges[i].base;
        uint64_t phys;

        if (virt & ((uint64_t)1 << 63)) {
            phys = physical_base + (virt - virtual_base);
        } else {
            panic(false, "limine: Virtual address of a PHDR in lower half");
        }

        uint64_t pf =
            (ranges[i].permissions & ELF_PF_X ? 0 : (nx ? VMM_FLAG_NOEXEC : 0)) |
            (ranges[i].permissions & ELF_PF_W ? VMM_FLAG_WRITE : 0);

        for (uint64_t j = 0; j < ranges[i].length; j += 0x1000) {
            map_page(pagemap, virt + j, phys + j, pf, Size4KiB);
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

        if (base < 0x100000000) {
            base = 0x100000000;
        }

        if (base >= top) {
            continue;
        }

        uint64_t aligned_base   = ALIGN_DOWN(base, 0x40000000);
        uint64_t aligned_top    = ALIGN_UP(top, 0x40000000);
        uint64_t aligned_length = aligned_top - aligned_base;

        for (uint64_t j = 0; j < aligned_length; j += 0x40000000) {
            uint64_t page = aligned_base + j;
            map_page(pagemap, page, page, VMM_FLAG_WRITE, Size1GiB);
            map_page(pagemap, direct_map_offset + page, page, VMM_FLAG_WRITE, Size1GiB);
        }
    }

    // Map the framebuffer with appropriate permissions
    for (size_t i = 0; i < _memmap_entries; i++) {
        if (_memmap[i].type != MEMMAP_FRAMEBUFFER) {
            continue;
        }

        uint64_t base   = _memmap[i].base;
        uint64_t length = _memmap[i].length;
        uint64_t top    = base + length;

        uint64_t aligned_base   = ALIGN_DOWN(base, 0x1000);
        uint64_t aligned_top    = ALIGN_UP(top, 0x1000);
        uint64_t aligned_length = aligned_top - aligned_base;

        for (uint64_t j = 0; j < aligned_length; j += 0x1000) {
            uint64_t page = aligned_base + j;
            map_page(pagemap, page, page, VMM_FLAG_WRITE | VMM_FLAG_FB, Size4KiB);
            map_page(pagemap, direct_map_offset + page, page, VMM_FLAG_WRITE | VMM_FLAG_FB, Size4KiB);
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

#define LIMINE_MAIR(fb) ( ((uint64_t)0b11111111 << 0) /* Normal WB RW-allocate non-transient */ \
                        | ((uint64_t)(fb) << 8) )     /* Framebuffer type */

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

static void set_paging_mode(int paging_mode, bool kaslr) {
    direct_map_offset = paging_mode_higher_half(paging_mode);
    if (kaslr) {
        uint64_t mask = ((uint64_t)1 << (paging_mode_va_bits(paging_mode) - 4)) - 1;
        direct_map_offset += (rand64() & ~((uint64_t)0x40000000 - 1)) & mask;
    }
}

static uint64_t reported_addr(void *addr) {
    return (uint64_t)(uintptr_t)addr + direct_map_offset;
}

#define get_phys_addr(addr) ({ \
    __auto_type get_phys_addr__addr = (addr); \
    uintptr_t get_phys_addr__r; \
    if (get_phys_addr__addr & ((uint64_t)1 << 63)) { \
        get_phys_addr__r = physical_base + (get_phys_addr__addr - virtual_base); \
    } else { \
        get_phys_addr__r = get_phys_addr__addr; \
    } \
    get_phys_addr__r; \
})

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

    char *path = ext_mem_alloc(file->path_len);
    memcpy(path, file->path, file->path_len);

    ret.path = reported_addr(path);

    ret.address = reported_addr(freadall_mode(file, MEMMAP_KERNEL_AND_MODULES, true));
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

static uint64_t term_arg;
static void (*actual_callback)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

static void callback_shim(struct flanterm_context *ctx, uint64_t a, uint64_t b, uint64_t c, uint64_t d) {
    (void)ctx;
    actual_callback(term_arg, a, b, c, d);
}

// TODO pair with specific terminal
static void term_write_shim(uint64_t context, uint64_t buf, uint64_t count) {
    (void)context;
    _term_write(terms[0], buf, count);
}

noreturn void limine_load(char *config, char *cmdline) {
#if defined (__x86_64__) || defined (__i386__)
    uint32_t eax, ebx, ecx, edx;
#endif

    char *kernel_path = config_get_value(config, 0, "KERNEL_PATH");
    if (kernel_path == NULL)
        panic(true, "limine: KERNEL_PATH not specified");

    print("limine: Loading kernel `%#`...\n", kernel_path);

    struct file_handle *kernel_file;
    if ((kernel_file = uri_open(kernel_path)) == NULL)
        panic(true, "limine: Failed to open kernel with path `%#`. Is the path correct?", kernel_path);

    char *k_path_copy = ext_mem_alloc(strlen(kernel_path) + 1);
    strcpy(k_path_copy, kernel_path);
    char *k_resource = NULL, *k_root = NULL, *k_path = NULL, *k_hash = NULL;
    uri_resolve(k_path_copy, &k_resource, &k_root, &k_path, &k_hash);
    char *k_path_ = ext_mem_alloc(strlen(k_path) + 2);
    k_path_[0] = '/';
    strcpy(k_path_ + 1, k_path);
    k_path = k_path_;
    for (size_t i = strlen(k_path) - 1; ; i--) {
        if (k_path[i] == '/' || i == 1) {
            k_path[i] = 0;
            break;
        }
        k_path[i] = 0;
    }

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

    uint64_t image_size_before_bss;
    bool is_reloc;

    if (!elf64_load(kernel, &entry_point, &slide,
                   MEMMAP_KERNEL_AND_MODULES, kaslr,
                   &ranges, &ranges_count,
                   &physical_base, &virtual_base, NULL,
                   &image_size_before_bss,
                   &is_reloc)) {
        panic(true, "limine: ELF64 load failure");
    }

    kaslr = kaslr && is_reloc;

    // Load requests
    if (elf64_load_section(kernel, &requests, ".limine_reqs", 0, slide)) {
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
        for (size_t i = 0; i < ALIGN_DOWN(image_size_before_bss, 8); i += 8) {
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

    // Paging Mode
    int paging_mode, max_paging_mode;

#if defined (__x86_64__) || defined (__i386__)
    paging_mode = max_paging_mode = PAGING_MODE_X86_64_4LVL;
    if (cpuid(0x00000007, 0, &eax, &ebx, &ecx, &edx) && (ecx & (1 << 16))) {
        printv("limine: CPU has 5-level paging support\n");
        max_paging_mode = PAGING_MODE_X86_64_5LVL;
    }

#elif defined (__aarch64__)
    paging_mode = max_paging_mode = PAGING_MODE_AARCH64_4LVL;
    // TODO(qookie): aarch64 also has optional 5 level paging when using 4K pages

#else
#error Unknown architecture
#endif

#define paging_mode_limine_to_vmm(x) (x)
#define paging_mode_vmm_to_limine(x) (x)

    bool have_paging_mode_request = false;
    bool paging_mode_set = false;
FEAT_START
    struct limine_paging_mode_request *pm_request = get_request(LIMINE_PAGING_MODE_REQUEST);
    if (pm_request == NULL)
        break;
    have_paging_mode_request = true;

    if (pm_request->mode > LIMINE_PAGING_MODE_MAX) {
        print("warning: ignoring invalid mode in paging mode request\n");
        break;
    }

    paging_mode = paging_mode_limine_to_vmm(pm_request->mode);
    if (paging_mode > max_paging_mode)
        paging_mode = max_paging_mode;

    set_paging_mode(paging_mode, kaslr);
    paging_mode_set = true;

    struct limine_paging_mode_response *pm_response =
        ext_mem_alloc(sizeof(struct limine_paging_mode_response));

    pm_response->mode = paging_mode_vmm_to_limine(paging_mode);
    pm_request->response = reported_addr(pm_response);

FEAT_END

    // 5 level paging feature & HHDM slide
FEAT_START
    struct limine_5_level_paging_request *lv5pg_request = get_request(LIMINE_5_LEVEL_PAGING_REQUEST);
    if (lv5pg_request == NULL)
        break;

    if (have_paging_mode_request) {
        print("paging: ignoring 5-level paging request in favor of paging mode request\n");
        break;
    }
#if defined (__x86_64__) || defined (__i386__)
    if (max_paging_mode < PAGING_MODE_X86_64_5LVL)
        break;
    paging_mode = PAGING_MODE_X86_64_5LVL;
#elif defined (__aarch64__)
    if (max_paging_mode < PAGING_MODE_AARCH64_5LVL)
        break;
    paging_mode = PAGING_MODE_AARCH64_5LVL;
#else
#error Unknown architecture
#endif

    set_paging_mode(paging_mode, kaslr);
    paging_mode_set = true;

    void *lv5pg_response = ext_mem_alloc(sizeof(struct limine_5_level_paging_response));
    lv5pg_request->response = reported_addr(lv5pg_response);
FEAT_END

    if (!paging_mode_set) {
        set_paging_mode(paging_mode, kaslr);
    }

#if defined (__aarch64__)
    uint64_t aa64mmfr0;
    asm volatile ("mrs %0, id_aa64mmfr0_el1" : "=r" (aa64mmfr0));

    uint64_t pa = aa64mmfr0 & 0xF;

    uint64_t tsz = 64 - paging_mode_va_bits(paging_mode);
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

    if (smbios_entry_32 == NULL && smbios_entry_64 == NULL) {
        pmm_free(smbios_response, sizeof(struct limine_smbios_response));
    } else {
        smbios_request->response = reported_addr(smbios_response);
    }
FEAT_END

#if defined (UEFI)
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

#if defined (UEFI)
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

        if (memcmp(&cur_table->VendorGuid, &dtb_guid, sizeof(EFI_GUID)) == 0) {
            struct limine_dtb_response *dtb_response =
                ext_mem_alloc(sizeof(struct limine_dtb_response));
            dtb_response->dtb_ptr = reported_addr((void *)cur_table->VendorTable);
            dtb_request->response = reported_addr(dtb_response);
            break;
        }
    }

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

    if (module_request->revision >= 1) {
        module_count += module_request->internal_module_count;
    }

    if (module_count == 0) {
        break;
    }

    struct limine_module_response *module_response =
        ext_mem_alloc(sizeof(struct limine_module_response));

    module_response->revision = 1;

    struct limine_file *modules = ext_mem_alloc(module_count * sizeof(struct limine_file));

    size_t final_module_count = 0;
    for (size_t i = 0; i < module_count; i++) {
        char *module_path;
        char *module_cmdline;
        bool module_required = true;
        bool module_path_allocated = false;

        if (module_request->revision >= 1 && i < module_request->internal_module_count) {
            uint64_t *internal_modules = (void *)get_phys_addr(module_request->internal_modules);
            struct limine_internal_module *internal_module = (void *)get_phys_addr(internal_modules[i]);

            module_path = (char *)get_phys_addr(internal_module->path);
            module_cmdline = (char *)get_phys_addr(internal_module->cmdline);

            char *module_path_abs = ext_mem_alloc(1024);
            char *module_path_abs_p = module_path_abs;
            strcpy(module_path_abs_p, k_resource);
            module_path_abs_p += strlen(k_resource);
            strcpy(module_path_abs_p, "://");
            module_path_abs_p += 3;
            strcpy(module_path_abs_p, k_root);
            module_path_abs_p += strlen(k_root);
            get_absolute_path(module_path_abs_p, module_path, k_path);

            module_path = module_path_abs;
            module_path_allocated = true;

            module_required = internal_module->flags & LIMINE_INTERNAL_MODULE_REQUIRED;
        } else {
            struct conf_tuple conf_tuple =
                    config_get_tuple(config, i - (module_request->revision >= 1 ? module_request->internal_module_count : 0),
                                     "MODULE_PATH", "MODULE_CMDLINE");

            module_path = conf_tuple.value1;
            module_cmdline = conf_tuple.value2;
        }

        if (module_cmdline == NULL) {
            module_cmdline = "";
        }

        print("limine: Loading module `%#`...\n", module_path);

        struct file_handle *f;
        if ((f = uri_open(module_path)) == NULL) {
            if (module_required) {
                panic(true, "limine: Failed to open module with path `%#`. Is the path correct?", module_path);
            }
            print("limine: Warning: Non-required internal module `%#` not found\n", module_path);
            if (module_path_allocated) {
                pmm_free(module_path, 1024);
            }
            continue;
        }
        if (module_path_allocated) {
            pmm_free(module_path, 1024);
        }

        struct limine_file *l = &modules[final_module_count++];
        *l = get_file(f, module_cmdline);

        fclose(f);
    }

    uint64_t *modules_list = ext_mem_alloc(final_module_count * sizeof(uint64_t));
    for (size_t i = 0; i < final_module_count; i++) {
        modules_list[i] = reported_addr(&modules[i]);
    }

    module_response->module_count = final_module_count;
    module_response->modules = reported_addr(modules_list);

    module_request->response = reported_addr(module_response);
FEAT_END

    size_t req_width = 0, req_height = 0, req_bpp = 0;

    char *resolution = config_get_value(config, 0, "RESOLUTION");
    if (resolution != NULL) {
        parse_resolution(&req_width, &req_height, &req_bpp, resolution);
    }

    uint64_t *term_fb_ptr = NULL;
    uint64_t term_fb_addr;

    struct fb_info *fbs;
    size_t fbs_count;

    // Terminal feature
FEAT_START
    struct limine_terminal_request *terminal_request = get_request(LIMINE_TERMINAL_REQUEST);
    if (terminal_request == NULL) {
        break; // next feature
    }

    struct limine_terminal_response *terminal_response =
        ext_mem_alloc(sizeof(struct limine_terminal_response));

    terminal_response->revision = 1;

    struct limine_terminal *terminal = ext_mem_alloc(sizeof(struct limine_terminal));

    quiet = false;
    serial = false;

    char *term_conf_override_s = config_get_value(config, 0, "TERM_CONFIG_OVERRIDE");
    if (term_conf_override_s != NULL && strcmp(term_conf_override_s, "yes") == 0) {
        if (!gterm_init(&fbs, &fbs_count, config, req_width, req_height)) {
            goto term_fail;
        }
    } else {
        if (!gterm_init(&fbs, &fbs_count, NULL, req_width, req_height)) {
            goto term_fail;
        }
    }

    if (0) {
term_fail:
        pmm_free(terminal, sizeof(struct limine_terminal));
        pmm_free(terminal_response, sizeof(struct limine_terminal_response));
        break; // next feature
    }

    if (terminal_request->callback != 0) {
        terms[0]->callback = callback_shim;

#if defined (__i386__)
        actual_callback = (void *)limine_term_callback;
        limine_term_callback_ptr = terminal_request->callback;
#elif defined (__x86_64__) || defined (__aarch64__)
        actual_callback = (void *)terminal_request->callback;
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
    term_fb_addr = reported_addr((void *)(((struct flanterm_fb_context *)terms[0])->framebuffer));

    terminal->columns = terms[0]->cols;
    terminal->rows = terms[0]->rows;

    uint64_t *term_list = ext_mem_alloc(1 * sizeof(uint64_t));
    term_list[0] = reported_addr(terminal);

    terminal_response->terminal_count = 1;
    terminal_response->terminals = reported_addr(term_list);

    terminal_request->response = reported_addr(terminal_response);

    goto skip_fb_init;
FEAT_END

    term_notready();

    fb_init(&fbs, &fbs_count, req_width, req_height, req_bpp);
    if (fbs_count == 0) {
        goto no_fb;
    }

skip_fb_init:
    for (size_t i = 0; i < fbs_count; i++) {
        memmap_alloc_range(fbs[i].framebuffer_addr,
                           (uint64_t)fbs[i].framebuffer_pitch * fbs[i].framebuffer_height,
                           MEMMAP_FRAMEBUFFER, 0, false, false, true);
    }

    // Framebuffer feature
FEAT_START
    struct limine_framebuffer_request *framebuffer_request = get_request(LIMINE_FRAMEBUFFER_REQUEST);
    if (framebuffer_request == NULL && term_fb_ptr == NULL) {
        break; // next feature
    }

    struct limine_framebuffer *fbp = ext_mem_alloc(fbs_count * sizeof(struct limine_framebuffer));

    struct limine_framebuffer_response *framebuffer_response =
        ext_mem_alloc(sizeof(struct limine_framebuffer_response));

    framebuffer_response->revision = 1;

    uint64_t *fb_list = ext_mem_alloc(fbs_count * sizeof(uint64_t));

    for (size_t i = 0; i < fbs_count; i++) {
        uint64_t *modes_list = ext_mem_alloc(fbs[i].mode_count * sizeof(uint64_t));
        for (size_t j = 0; j < fbs[i].mode_count; j++) {
            fbs[i].mode_list[j].memory_model = LIMINE_FRAMEBUFFER_RGB;
            modes_list[j] = reported_addr(&fbs[i].mode_list[j]);
        }
        fbp[i].modes = reported_addr(modes_list);
        fbp[i].mode_count = fbs[i].mode_count;

        if (fbs[i].edid != NULL) {
            fbp[i].edid_size = sizeof(struct edid_info_struct);
            fbp[i].edid = reported_addr(fbs[i].edid);
        }

        fbp[i].memory_model     = LIMINE_FRAMEBUFFER_RGB;
        fbp[i].address          = reported_addr((void *)(uintptr_t)fbs[i].framebuffer_addr);
        fbp[i].width            = fbs[i].framebuffer_width;
        fbp[i].height           = fbs[i].framebuffer_height;
        fbp[i].bpp              = fbs[i].framebuffer_bpp;
        fbp[i].pitch            = fbs[i].framebuffer_pitch;
        fbp[i].red_mask_size    = fbs[i].red_mask_size;
        fbp[i].red_mask_shift   = fbs[i].red_mask_shift;
        fbp[i].green_mask_size  = fbs[i].green_mask_size;
        fbp[i].green_mask_shift = fbs[i].green_mask_shift;
        fbp[i].blue_mask_size   = fbs[i].blue_mask_size;
        fbp[i].blue_mask_shift  = fbs[i].blue_mask_shift;

        fb_list[i] = reported_addr(&fbp[i]);
    }

    framebuffer_response->framebuffer_count = fbs_count;
    framebuffer_response->framebuffers = reported_addr(fb_list);

    if (framebuffer_request != NULL) {
        framebuffer_request->response = reported_addr(framebuffer_response);
    }
    if (term_fb_ptr != NULL) {
        for (size_t i = 0; i < fbs_count; i++) {
            if (fbp[i].address == term_fb_addr) {
                *term_fb_ptr = reported_addr(&fbp[i]);
                break;
            }
        }
    }
FEAT_END

no_fb:
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

#if defined (__aarch64__)
    // Find the most restrictive caching mode from all framebuffers to use
    uint64_t fb_attr = (uint64_t)-1;

    for (size_t i = 0; i < fbs_count; i++) {
        int el = current_el();
        uint64_t res;

        // Figure out the caching mode used for this particular framebuffer
        if (el == 1) {
            asm volatile (
                    "at s1e1w, %1\n\t"
                    "isb\n\t"
                    "mrs %0, par_el1"
                    : "=r"(res)
                    : "r"(fbs[i].framebuffer_addr)
                    : "memory");
        } else if (el == 2) {
            asm volatile (
                    "at s1e2w, %1\n\t"
                    "isb\n\t"
                    "mrs %0, par_el1"
                    : "=r"(res)
                    : "r"(fbs[i].framebuffer_addr)
                    : "memory");
        } else {
            panic(false, "Unexpected EL in limine_load");
        }

        if (res & 1)
            panic(false, "Address translation for framebuffer failed");

        uint64_t new_attr = res >> 56;

        // Use whatever we find first
        if (fb_attr == (uint64_t)-1)
            fb_attr = new_attr;
        // Prefer Device memory over Normal memory
        else if ((fb_attr & 0b11110000) && !(new_attr & 0b11110000))
            fb_attr = new_attr;
        // Prefer tighter Device memory (lower values)
        else if (!(fb_attr & 0b11110000) && !(new_attr & 0b11110000) && fb_attr > new_attr)
            fb_attr = new_attr;
        // Use Normal non-cacheable otherwise (avoid trying to figure out how to downgrade inner vs outer).
        else if ((fb_attr & 0b11110000) && (new_attr & 0b11110000))
            fb_attr = 0b01000100; // Inner&outer Non-cacheable
        // Otherwise do nothing (fb_attr is already more restrictive than new_attr).
    }

    // If no framebuffers are found, just zero out the MAIR entry
    if (fb_attr == (uint64_t)-1)
        fb_attr = 0;
#endif

    void *stack = ext_mem_alloc(stack_size) + stack_size;

    bool nx_available = true;
#if defined (__x86_64__) || defined (__i386__)
    // Check if we have NX
    if (!cpuid(0x80000001, 0, &eax, &ebx, &ecx, &edx) || !(edx & (1 << 20))) {
        nx_available = false;
    }
#endif

    pagemap_t pagemap = {0};
    pagemap = build_pagemap(paging_mode, nx_available, ranges, ranges_count,
                            physical_base, virtual_base, direct_map_offset);

#if defined (UEFI)
    efi_exit_boot_services();
#endif

    // SMP
FEAT_START
    struct limine_smp_request *smp_request = get_request(LIMINE_SMP_REQUEST);
    if (smp_request == NULL) {
        break; // next feature
    }

    struct limine_smp_info *smp_info;
    size_t cpu_count;
#if defined (__x86_64__) || defined (__i386__)
    uint32_t bsp_lapic_id;
    smp_info = init_smp(&cpu_count, &bsp_lapic_id,
                        paging_mode,
                        pagemap, smp_request->flags & LIMINE_SMP_X2APIC, nx_available,
                        direct_map_offset, true);
#elif defined (__aarch64__)
    uint64_t bsp_mpidr;

    smp_info = init_smp(&cpu_count, &bsp_mpidr,
                        pagemap, LIMINE_MAIR(fb_attr), LIMINE_TCR(tsz, pa), LIMINE_SCTLR);
#else
#error Unknown architecture
#endif

    if (smp_info == NULL) {
        break;
    }

    for (size_t i = 0; i < cpu_count; i++) {
        void *cpu_stack = ext_mem_alloc(stack_size) + stack_size;
        smp_info[i].reserved = reported_addr(cpu_stack);
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
        smp_list[i] = reported_addr(&smp_info[i]);
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
        _memmap = ext_mem_alloc(sizeof(struct limine_memmap_entry) * MEMMAP_MAX);
        memmap_list = ext_mem_alloc(MEMMAP_MAX * sizeof(uint64_t));
    }

    size_t mmap_entries;
    struct memmap_entry *mmap = get_memmap(&mmap_entries);

    if (memmap_request == NULL) {
        break; // next feature
    }

    if (mmap_entries > MEMMAP_MAX) {
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
    FOR_TERM(flanterm_write(TERM, "\e[2J\e[H", 7));

#if defined (__x86_64__) || defined (__i386__)
#if defined (BIOS)
    // If we're going 64, we might as well call this BIOS interrupt
    // to tell the BIOS that we are entering Long Mode, since it is in
    // the specification.
    struct rm_regs r = {0};
    r.eax = 0xec00;
    r.ebx = 0x02;   // Long mode only
    rm_int(0x15, &r, &r);
#endif

    // Set PAT as:
    // PAT0 -> WB  (06)
    // PAT1 -> WT  (04)
    // PAT2 -> UC- (07)
    // PAT3 -> UC  (00)
    // PAT4 -> WP  (05)
    // PAT5 -> WC  (01)
    uint64_t pat = (uint64_t)0x010500070406;
    wrmsr(0x277, pat);

    pic_mask_all();
    io_apic_mask_all();

    irq_flush_type = IRQ_PIC_APIC_FLUSH;

    uint64_t reported_stack = reported_addr(stack);

    common_spinup(limine_spinup_32, 8,
        paging_mode, (uint32_t)(uintptr_t)pagemap.top_level,
        (uint32_t)entry_point, (uint32_t)(entry_point >> 32),
        (uint32_t)reported_stack, (uint32_t)(reported_stack >> 32),
        (uint32_t)(uintptr_t)local_gdt, nx_available);
#elif defined (__aarch64__)
    vmm_assert_4k_pages();

    uint64_t reported_stack = reported_addr(stack);

    enter_in_el1(entry_point, reported_stack, LIMINE_SCTLR, LIMINE_MAIR(fb_attr), LIMINE_TCR(tsz, pa),
                 (uint64_t)pagemap.top_level[0],
                 (uint64_t)pagemap.top_level[1], 0);
#else
#error Unknown architecture
#endif
}
