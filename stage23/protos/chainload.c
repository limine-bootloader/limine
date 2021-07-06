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
#include <lib/libc.h>
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
    struct idtr real_mode_idt;
    real_mode_idt.limit = 0x3ff;
    real_mode_idt.ptr   = 0;

    asm volatile (
        "cli\n\t"
        "cld\n\t"

        "lidt (%%eax)\n\t"

        "pushl $0x08\n\t"
        "pushl $1f\n\t"
        "lret\n\t"
        "1: .code16\n\t"
        "movw $0x10, %%ax\n\t"
        "movw %%ax, %%ds\n\t"
        "movw %%ax, %%es\n\t"
        "movw %%ax, %%fs\n\t"
        "movw %%ax, %%gs\n\t"
        "movw %%ax, %%ss\n\t"
        "movl %%cr0, %%eax\n\t"
        "andb $0xfe, %%al\n\t"
        "movl %%eax, %%cr0\n\t"
        "movl $1f, %%eax\n\t"
        "pushw $0\n\t"
        "pushw %%ax\n\t"
        "lret\n\t"
        "1:\n\t"
        "xorw %%ax, %%ax\n\t"
        "movw %%ax, %%ds\n\t"
        "movw %%ax, %%es\n\t"
        "movw %%ax, %%fs\n\t"
        "movw %%ax, %%gs\n\t"
        "movw %%ax, %%ss\n\t"

        "sti\n\t"

        "pushw $0\n\t"
        "pushw $0x7c00\n\t"
        "lret\n\t"

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
            part = 0;
        } else {
            val = strtoui(part_config, NULL, 10);
            if (val > 256) {
                panic("BIOS partition number outside range 0-256");
            }
            part = val;
        }
    }
    int drive; {
        char *drive_config = config_get_value(config, 0, "DRIVE");
        if (drive_config == NULL) {
            panic("DRIVE not specified");
        }
        val = strtoui(drive_config, NULL, 10);
        if (val < 1 || val > 256) {
            panic("BIOS drive number outside range 1-256");
        }
        drive = val;
    }

    int rows, cols;
    init_vga_textmode(&rows, &cols, false);

    struct volume *p = volume_get_by_coord(false, drive, part);

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
        panic("chainload: Failed to open image with path `%s`. Is the path correct?", image_path);

    void *_ptr = freadall(image, MEMMAP_RESERVED);
    size_t image_size = image->size;
    void *ptr;
    status = uefi_call_wrapper(gBS->AllocatePool, 3,
        EfiLoaderData, image_size, &ptr);
    if (status)
        panic("chainload: Allocation failure");
    memcpy(ptr, _ptr, image_size);

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
        panic("chainload: HandleProtocol failure (%x)", status);
    }

    EFI_LOADED_IMAGE_PROTOCOL *new_handle_loaded_image = NULL;
    status = uefi_call_wrapper(gBS->HandleProtocol, 3,
                               new_handle, &loaded_img_prot_guid,
                               &new_handle_loaded_image);
    if (status) {
        panic("chainload: HandleProtocol failure (%x)", status);
    }

    new_handle_loaded_image->DeviceHandle = loader_loaded_image->DeviceHandle;

    status = uefi_call_wrapper(gBS->StartImage, 3, new_handle, NULL, NULL);
    if (status) {
        panic("chainload: StartImage failure (%x)", status);
    }

    __builtin_unreachable();
}

#endif
