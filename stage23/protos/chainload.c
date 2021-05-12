#include <stddef.h>
#include <stdint.h>
#include <protos/chainload.h>
#include <lib/part.h>
#include <lib/config.h>
#include <lib/blib.h>
#include <drivers/disk.h>
#include <lib/term.h>
#include <lib/fb.h>
#include <lib/uri.h>
#include <lib/print.h>
#include <sys/idt.h>
#include <drivers/vga_textmode.h>
#include <mm/pmm.h>
#if defined (uefi)
#  include <efi.h>
#endif

#if defined (bios)

__attribute__((noinline))
__attribute__((section(".realmode")))
static void spinup(uint8_t drive) {
    struct idtr real_mode_idt = { 0x3ff, 0x0 };

    asm volatile (
        "cli\n\t"
        "cld\n\t"

        "lidt [eax]\n\t"

        "jmp 0x08:1f\n\t"
        "1: .code16\n\t"
        "mov ax, 0x10\n\t"
        "mov ds, ax\n\t"
        "mov es, ax\n\t"
        "mov fs, ax\n\t"
        "mov gs, ax\n\t"
        "mov ss, ax\n\t"
        "mov eax, cr0\n\t"
        "and al, 0xfe\n\t"
        "mov cr0, eax\n\t"
        "mov eax, OFFSET 1f\n\t"
        "push 0\n\t"
        "push ax\n\t"
        "retf\n\t"
        "1:\n\t"
        "mov ax, 0\n\t"
        "mov ds, ax\n\t"
        "mov es, ax\n\t"
        "mov fs, ax\n\t"
        "mov gs, ax\n\t"
        "mov ss, ax\n\t"

        "sti\n\t"

        "push 0\n\t"
        "push 0x7c00\n\t"
        "retf\n\t"

        ".code32\n\t"
        :
        : "a" (&real_mode_idt), "d" (drive)
        : "memory"
    );
}

void chainload(char *config) {
    uint64_t val;

    int part; {
        char *part_config = config_get_value(config, 0, "PARTITION");
        if (part_config == NULL) {
            part = -1;
        } else {
            val = strtoui(part_config, NULL, 10);
            if (val < 1 || val > 256) {
                panic("BIOS partition number outside range 1-256");
            }
            part = val - 1;
        }
    }
    int drive; {
        char *drive_config = config_get_value(config, 0, "DRIVE");
        if (drive_config == NULL) {
            panic("DRIVE not specified");
        }
        val = strtoui(drive_config, NULL, 10);
        if (val < 1 || val > 16) {
            panic("BIOS drive number outside range 1-16");
        }
        drive = (val - 1) + 0x80;
    }

    int rows, cols;
    init_vga_textmode(&rows, &cols, false);

    struct volume *p = volume_get_by_coord(drive, part);

    volume_read(p, (void *)0x7c00, 0, 512);

    spinup(drive);
}

#elif defined (uefi)

void chainload(char *config) {
    EFI_STATUS status;

    char *image_path = config_get_value(config, 0, "IMAGE_PATH");
    if (image_path == NULL)
        panic("chainload: IMAGE_PATH not specified");

    struct file_handle *image = ext_mem_alloc(sizeof(struct file_handle));
    if (!uri_open(image, image_path))
        panic("chainload: Could not open image");

    void *ptr = freadall(image, MEMMAP_EFI_LOADER);
    size_t image_size = image->size;

    term_deinit();

    int req_width = 0, req_height = 0, req_bpp = 0;

    char *resolution = config_get_value(config, 0, "RESOLUTION");
    if (resolution != NULL)
        parse_resolution(&req_width, &req_height, &req_bpp, resolution);

    struct fb_info fbinfo;
    if (!fb_init(&fbinfo, req_width, req_height, req_bpp))
        panic("chainload: Unable to set video mode");

    pmm_release_uefi_mem();

    MEMMAP_DEVICE_PATH memdev_path[2];

    memdev_path[0].Header.Type      = HARDWARE_DEVICE_PATH;
    memdev_path[0].Header.SubType   = HW_MEMMAP_DP;
    memdev_path[0].Header.Length[0] = sizeof(MEMMAP_DEVICE_PATH);
    memdev_path[0].Header.Length[1] = sizeof(MEMMAP_DEVICE_PATH) >> 8;

    memdev_path[0].MemoryType       = EfiLoaderData;
    memdev_path[0].StartingAddress  = (uintptr_t)ptr;
    memdev_path[0].EndingAddress    = (uintptr_t)ptr + image_size;

    memdev_path[1].Header.Type      = END_DEVICE_PATH_TYPE;
    memdev_path[1].Header.SubType   = END_ENTIRE_DEVICE_PATH_SUBTYPE;
    memdev_path[1].Header.Length[0] = sizeof(EFI_DEVICE_PATH);
    memdev_path[1].Header.Length[1] = sizeof(EFI_DEVICE_PATH) >> 8;

    EFI_HANDLE new_handle = 0;

    status = uefi_call_wrapper(gBS->LoadImage, 6, 0, efi_image_handle, memdev_path,
                               ptr, image_size, &new_handle);
    if (status) {
        panic("chainload: LoadImage failure (%x)", status);
    }

    // Apparently we need to make sure that the DeviceHandle field is the same
    // as us (the loader) for some EFI images to properly work (Windows for instance)
    EFI_GUID loaded_img_prot_guid = EFI_LOADED_IMAGE_PROTOCOL_GUID;

    EFI_LOADED_IMAGE_PROTOCOL *loader_loaded_image = NULL;
    status = uefi_call_wrapper(gBS->HandleProtocol, 3,
                               efi_image_handle, &loaded_img_prot_guid,
                               &loader_loaded_image);
    if (status) {
        panic("HandleProtocol failure (%x)\n", status);
    }

    EFI_LOADED_IMAGE_PROTOCOL *new_handle_loaded_image = NULL;
    status = uefi_call_wrapper(gBS->HandleProtocol, 3,
                               new_handle, &loaded_img_prot_guid,
                               &new_handle_loaded_image);
    if (status) {
        panic("HandleProtocol failure (%x)\n", status);
    }

    new_handle_loaded_image->DeviceHandle = loader_loaded_image->DeviceHandle;

    status = uefi_call_wrapper(gBS->StartImage, 3, new_handle, NULL, NULL);
    if (status) {
        panic("chainload: StartImage failure (%x)", status);
    }

    __builtin_unreachable();
}

#endif
