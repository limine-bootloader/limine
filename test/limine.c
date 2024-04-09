#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <limine.h>
#include <e9print.h>
#include <flanterm/flanterm.h>
#include <flanterm/backends/fb.h>

__attribute__((section(".limine_requests")))
static volatile LIMINE_BASE_REVISION(1);

static void limine_main(void);

__attribute__((used))
__attribute__((section(".limine_requests")))
static volatile struct limine_entry_point_request entry_point_request = {
    .id = LIMINE_ENTRY_POINT_REQUEST,
    .revision = 0, .response = NULL,

    .entry = limine_main
};

__attribute__((section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0, .response = NULL
};

__attribute__((section(".limine_requests")))
static volatile struct limine_bootloader_info_request bootloader_info_request = {
    .id = LIMINE_BOOTLOADER_INFO_REQUEST,
    .revision = 0, .response = NULL
};

__attribute__((section(".limine_requests")))
static volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST,
    .revision = 0, .response = NULL
};

__attribute__((section(".limine_requests")))
static volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST,
    .revision = 0, .response = NULL
};

__attribute__((section(".limine_requests")))
static volatile struct limine_kernel_file_request kf_request = {
    .id = LIMINE_KERNEL_FILE_REQUEST,
    .revision = 0, .response = NULL
};

struct limine_internal_module internal_module1 = {
    .path = "/boot/test.elf",
    .cmdline = "First internal module"
};

struct limine_internal_module internal_module2 = {
    .path = "test.elf",
    .cmdline = "Second internal module"
};

struct limine_internal_module internal_module3 = {
    .path = "./limine.cfg",
    .cmdline = "Third internal module"
};

struct limine_internal_module *internal_modules[] = {
    &internal_module1,
    &internal_module2,
    &internal_module3
};

__attribute__((section(".limine_requests")))
static volatile struct limine_module_request module_request = {
    .id = LIMINE_MODULE_REQUEST,
    .revision = 1, .response = NULL,

    .internal_module_count = 3,
    .internal_modules = internal_modules
};

__attribute__((section(".limine_requests")))
static volatile struct limine_rsdp_request rsdp_request = {
    .id = LIMINE_RSDP_REQUEST,
    .revision = 0, .response = NULL
};

__attribute__((section(".limine_requests")))
static volatile struct limine_smbios_request smbios_request = {
    .id = LIMINE_SMBIOS_REQUEST,
    .revision = 0, .response = NULL
};

__attribute__((section(".limine_requests")))
static volatile struct limine_efi_system_table_request est_request = {
    .id = LIMINE_EFI_SYSTEM_TABLE_REQUEST,
    .revision = 0, .response = NULL
};

__attribute__((section(".limine_requests")))
static volatile struct limine_efi_memmap_request efi_memmap_request = {
    .id = LIMINE_EFI_MEMMAP_REQUEST,
    .revision = 0, .response = NULL
};

__attribute__((section(".limine_requests")))
static volatile struct limine_boot_time_request boot_time_request = {
    .id = LIMINE_BOOT_TIME_REQUEST,
    .revision = 0, .response = NULL
};

__attribute__((section(".limine_requests")))
static volatile struct limine_kernel_address_request kernel_address_request = {
    .id = LIMINE_KERNEL_ADDRESS_REQUEST,
    .revision = 0, .response = NULL
};

__attribute__((section(".limine_requests")))
static volatile struct limine_smp_request _smp_request = {
    .id = LIMINE_SMP_REQUEST,
    .revision = 0, .response = NULL
};

__attribute__((section(".limine_requests")))
static volatile struct limine_dtb_request _dtb_request = {
    .id = LIMINE_DTB_REQUEST,
    .revision = 0, .response = NULL
};

__attribute__((section(".limine_requests")))
static volatile struct limine_paging_mode_request _pm_request = {
    .id = LIMINE_PAGING_MODE_REQUEST,
    .revision = 0, .response = NULL,
    .mode = LIMINE_PAGING_MODE_X86_64_5LVL,
    .flags = 0,
};

__attribute__((used))
__attribute__((section(".limine_requests_delimiter")))
static volatile LIMINE_REQUESTS_DELIMITER;

static char *get_memmap_type(uint64_t type) {
    switch (type) {
        case LIMINE_MEMMAP_USABLE:
            return "Usable";
        case LIMINE_MEMMAP_RESERVED:
            return "Reserved";
        case LIMINE_MEMMAP_ACPI_RECLAIMABLE:
            return "ACPI reclaimable";
        case LIMINE_MEMMAP_ACPI_NVS:
            return "ACPI NVS";
        case LIMINE_MEMMAP_BAD_MEMORY:
            return "Bad memory";
        case LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE:
            return "Bootloader reclaimable";
        case LIMINE_MEMMAP_KERNEL_AND_MODULES:
            return "Kernel and modules";
        case LIMINE_MEMMAP_FRAMEBUFFER:
            return "Framebuffer";
        default:
            return "???";
    }
}

static void print_file(struct limine_file *file) {
    e9_printf("File->Revision: %d", file->revision);
    e9_printf("File->Address: %x", file->address);
    e9_printf("File->Size: %x", file->size);
    e9_printf("File->Path: %s", file->path);
    e9_printf("File->CmdLine: %s", file->cmdline);
    e9_printf("File->MediaType: %d", file->media_type);
    e9_printf("File->PartIndex: %d", file->partition_index);
    e9_printf("File->TFTPIP: %d.%d.%d.%d",
              (file->tftp_ip & (0xff << 0)) >> 0,
              (file->tftp_ip & (0xff << 8)) >> 8,
              (file->tftp_ip & (0xff << 16)) >> 16,
              (file->tftp_ip & (0xff << 24)) >> 24);
    e9_printf("File->TFTPPort: %d", file->tftp_port);
    e9_printf("File->MBRDiskId: %x", file->mbr_disk_id);
    e9_printf("File->GPTDiskUUID: %x-%x-%x-%x",
              file->gpt_disk_uuid.a,
              file->gpt_disk_uuid.b,
              file->gpt_disk_uuid.c,
              *(uint64_t *)file->gpt_disk_uuid.d);
    e9_printf("File->GPTPartUUID: %x-%x-%x-%x",
              file->gpt_part_uuid.a,
              file->gpt_part_uuid.b,
              file->gpt_part_uuid.c,
              *(uint64_t *)file->gpt_part_uuid.d);
    e9_printf("File->PartUUID: %x-%x-%x-%x",
              file->part_uuid.a,
              file->part_uuid.b,
              file->part_uuid.c,
              *(uint64_t *)file->part_uuid.d);
}

uint32_t ctr = 0;

void ap_entry(struct limine_smp_info *info) {
    e9_printf("Hello from AP!");

#if defined (__x86_64__)
    e9_printf("My LAPIC ID: %x", info->lapic_id);
#elif defined (__aarch64__)
    e9_printf("My GIC CPU Interface no.: %x", info->gic_iface_no);
    e9_printf("My MPIDR: %x", info->mpidr);
#elif defined (__riscv)
    e9_printf("My Hart ID: %x", info->hartid);
#endif

    __atomic_fetch_add(&ctr, 1, __ATOMIC_SEQ_CST);

    while (1);
}

#define FEAT_START do {
#define FEAT_END } while (0);

extern char kernel_start[];

struct flanterm_context *ft_ctx = NULL;

static void limine_main(void) {
    e9_printf("\nWe're alive");

    if (LIMINE_BASE_REVISION_SUPPORTED == false) {
        e9_printf("Limine base revision not supported");
        for (;;);
    }

    struct limine_framebuffer *fb = framebuffer_request.response->framebuffers[0];

    ft_ctx = flanterm_fb_init(
        NULL,
        NULL,
        fb->address, fb->width, fb->height, fb->pitch,
        fb->red_mask_size, fb->red_mask_shift,
        fb->green_mask_size, fb->green_mask_shift,
        fb->blue_mask_size, fb->blue_mask_shift,
        NULL,
        NULL, NULL,
        NULL, NULL,
        NULL, NULL,
        NULL, 0, 0, 1,
        0, 0,
        0
    );

    uint64_t kernel_slide = (uint64_t)kernel_start - 0xffffffff80000000;

    e9_printf("Kernel slide: %x", kernel_slide);

FEAT_START
    e9_printf("");
    if (bootloader_info_request.response == NULL) {
        e9_printf("Bootloader info not passed");
        break;
    }
    struct limine_bootloader_info_response *bootloader_info_response = bootloader_info_request.response;
    e9_printf("Bootloader info feature, revision %d", bootloader_info_response->revision);
    e9_printf("Bootloader name: %s", bootloader_info_response->name);
    e9_printf("Bootloader version: %s", bootloader_info_response->version);
FEAT_END

FEAT_START
    e9_printf("");
    if (kernel_address_request.response == NULL) {
        e9_printf("Kernel address not passed");
        break;
    }
    struct limine_kernel_address_response *ka_response = kernel_address_request.response;
    e9_printf("Kernel address feature, revision %d", ka_response->revision);
    e9_printf("Physical base: %x", ka_response->physical_base);
    e9_printf("Virtual base: %x", ka_response->virtual_base);
FEAT_END

FEAT_START
    e9_printf("");
    if (hhdm_request.response == NULL) {
        e9_printf("HHDM not passed");
        break;
    }
    struct limine_hhdm_response *hhdm_response = hhdm_request.response;
    e9_printf("HHDM feature, revision %d", hhdm_response->revision);
    e9_printf("Higher half direct map at: %x", hhdm_response->offset);
FEAT_END

FEAT_START
    e9_printf("");
    if (memmap_request.response == NULL) {
        e9_printf("Memory map not passed");
        break;
    }
    struct limine_memmap_response *memmap_response = memmap_request.response;
    e9_printf("Memory map feature, revision %d", memmap_response->revision);
    e9_printf("%d memory map entries", memmap_response->entry_count);
    for (size_t i = 0; i < memmap_response->entry_count; i++) {
        struct limine_memmap_entry *e = memmap_response->entries[i];
        e9_printf("%x->%x %s", e->base, e->base + e->length, get_memmap_type(e->type));
    }
FEAT_END

FEAT_START
    e9_printf("");
    if (framebuffer_request.response == NULL) {
        e9_printf("Framebuffer not passed");
        break;
    }
    struct limine_framebuffer_response *fb_response = framebuffer_request.response;
    e9_printf("Framebuffers feature, revision %d", fb_response->revision);
    e9_printf("%d framebuffer(s)", fb_response->framebuffer_count);
    for (size_t i = 0; i < fb_response->framebuffer_count; i++) {
        struct limine_framebuffer *fb = fb_response->framebuffers[i];
        e9_printf("Address: %x", fb->address);
        e9_printf("Width: %d", fb->width);
        e9_printf("Height: %d", fb->height);
        e9_printf("Pitch: %d", fb->pitch);
        e9_printf("BPP: %d", fb->bpp);
        e9_printf("Memory model: %d", fb->memory_model);
        e9_printf("Red mask size: %d", fb->red_mask_size);
        e9_printf("Red mask shift: %d", fb->red_mask_shift);
        e9_printf("Green mask size: %d", fb->green_mask_size);
        e9_printf("Green mask shift: %d", fb->green_mask_shift);
        e9_printf("Blue mask size: %d", fb->blue_mask_size);
        e9_printf("Blue mask shift: %d", fb->blue_mask_shift);
        e9_printf("EDID size: %d", fb->edid_size);
        e9_printf("EDID at: %x", fb->edid);
        e9_printf("Video modes:");
        for (size_t j = 0; j < fb->mode_count; j++) {
            e9_printf("  %dx%dx%d", fb->modes[j]->width, fb->modes[j]->height, fb->modes[j]->bpp);
        }
    }
FEAT_END

FEAT_START
    e9_printf("");
    if (kf_request.response == NULL) {
        e9_printf("Kernel file not passed");
        break;
    }
    struct limine_kernel_file_response *kf_response = kf_request.response;
    e9_printf("Kernel file feature, revision %d", kf_response->revision);
    print_file(kf_response->kernel_file);
FEAT_END

FEAT_START
    e9_printf("");
    if (module_request.response == NULL) {
        e9_printf("Modules not passed");
        break;
    }
    struct limine_module_response *module_response = module_request.response;
    e9_printf("Modules feature, revision %d", module_response->revision);
    e9_printf("%d module(s)", module_response->module_count);
    for (size_t i = 0; i < module_response->module_count; i++) {
        struct limine_file *f = module_response->modules[i];
        e9_printf("---");
        print_file(f);
    }
FEAT_END

FEAT_START
    e9_printf("");
    if (rsdp_request.response == NULL) {
        e9_printf("RSDP not passed");
        break;
    }
    struct limine_rsdp_response *rsdp_response = rsdp_request.response;
    e9_printf("RSDP feature, revision %d", rsdp_response->revision);
    e9_printf("RSDP at: %x", rsdp_response->address);
FEAT_END

FEAT_START
    e9_printf("");
    if (smbios_request.response == NULL) {
        e9_printf("SMBIOS not passed");
        break;
    }
    struct limine_smbios_response *smbios_response = smbios_request.response;
    e9_printf("SMBIOS feature, revision %d", smbios_response->revision);
    e9_printf("SMBIOS 32-bit entry at: %x", smbios_response->entry_32);
    e9_printf("SMBIOS 64-bit entry at: %x", smbios_response->entry_64);
FEAT_END

FEAT_START
    e9_printf("");
    if (est_request.response == NULL) {
        e9_printf("EFI system table not passed");
        break;
    }
    struct limine_efi_system_table_response *est_response = est_request.response;
    e9_printf("EFI system table feature, revision %d", est_response->revision);
    e9_printf("EFI system table at: %x", est_response->address);
FEAT_END

FEAT_START
    e9_printf("");
    if (efi_memmap_request.response == NULL) {
        e9_printf("EFI memory map not passed");
        break;
    }
    struct limine_efi_memmap_response *efi_memmap_response = efi_memmap_request.response;
    e9_printf("EFI memory map feature, revision %d", efi_memmap_response->revision);
    e9_printf("EFI memory map at: %x", efi_memmap_response->memmap);
    e9_printf("EFI memory map size: %x", efi_memmap_response->memmap_size);
    e9_printf("EFI memory descriptor size: %x", efi_memmap_response->desc_size);
    e9_printf("EFI memory descriptor version: %d", efi_memmap_response->desc_version);
FEAT_END

FEAT_START
    e9_printf("");
    if (boot_time_request.response == NULL) {
        e9_printf("Boot time not passed");
        break;
    }
    struct limine_boot_time_response *boot_time_response = boot_time_request.response;
    e9_printf("Boot time feature, revision %d", boot_time_response->revision);
    e9_printf("Boot time: %d", boot_time_response->boot_time);
FEAT_END

FEAT_START
    e9_printf("");
    if (_smp_request.response == NULL) {
        e9_printf("SMP info not passed");
        break;
    }
    struct limine_smp_response *smp_response = _smp_request.response;
    e9_printf("SMP feature, revision %d", smp_response->revision);
    e9_printf("Flags: %x", smp_response->flags);
#if defined (__x86_64__)
    e9_printf("BSP LAPIC ID: %x", smp_response->bsp_lapic_id);
#elif defined (__aarch64__)
    e9_printf("BSP MPIDR: %x", smp_response->bsp_mpidr);
#elif defined (__riscv)
    e9_printf("BSP Hart ID: %x", smp_response->bsp_hartid);
#endif
    e9_printf("CPU count: %d", smp_response->cpu_count);
    for (size_t i = 0; i < smp_response->cpu_count; i++) {
        struct limine_smp_info *cpu = smp_response->cpus[i];
        e9_printf("Processor ID: %x", cpu->processor_id);
#if defined (__x86_64__)
        e9_printf("LAPIC ID: %x", cpu->lapic_id);
#elif defined (__aarch64__)
        e9_printf("GIC CPU Interface no.: %x", cpu->gic_iface_no);
        e9_printf("MPIDR: %x", cpu->mpidr);
#elif defined (__riscv)
        e9_printf("Hart ID: %x", cpu->hartid);
#endif


#if defined (__x86_64__)
        if (cpu->lapic_id != smp_response->bsp_lapic_id) {
#elif defined (__aarch64__)
        if (cpu->mpidr != smp_response->bsp_mpidr) {
#elif defined (__riscv)
        if (cpu->hartid != smp_response->bsp_hartid) {
#endif
            uint32_t old_ctr = __atomic_load_n(&ctr, __ATOMIC_SEQ_CST);

            __atomic_store_n(&cpu->goto_address, ap_entry, __ATOMIC_SEQ_CST);

            while (__atomic_load_n(&ctr, __ATOMIC_SEQ_CST) == old_ctr)
                ;
        }
    }
FEAT_END

FEAT_START
    e9_printf("");
    if (_dtb_request.response == NULL) {
        e9_printf("Device tree blob not passed");
        break;
    }
    struct limine_dtb_response *dtb_response = _dtb_request.response;
    e9_printf("Device tree blob feature, revision %d", dtb_response->revision);
    e9_printf("Device tree blob pointer: %x", dtb_response->dtb_ptr);
FEAT_END

FEAT_START
    e9_printf("");
    if (_pm_request.response == NULL) {
        e9_printf("Paging mode not passed");
        break;
    }
    struct limine_paging_mode_response *pm_response = _pm_request.response;
    e9_printf("Paging mode feature, revision %d", pm_response->revision);
    e9_printf("  mode: %d", pm_response->mode);
    e9_printf("  flags: %x", pm_response->flags);
FEAT_END

    for (;;);
}
