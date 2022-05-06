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
#include <arch/x86/idt.h>
#include <drivers/vga_textmode.h>
#include <mm/pmm.h>
#if uefi == 1
#  include <efi.h>
#endif

#if bios == 1

__attribute__((noinline, section(".realmode")))
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
                panic(true, "chainload: BIOS partition number outside range 0-256");
            }
            part = val;
        }
    }
    int drive; {
        char *drive_config = config_get_value(config, 0, "DRIVE");
        if (drive_config == NULL) {
            drive = boot_volume->index;
        } else {
            val = strtoui(drive_config, NULL, 10);
            if (val < 1 || val > 256) {
                panic(true, "chainload: BIOS drive number outside range 1-256");
            }
            drive = val;
        }
    }

    struct volume *p = volume_get_by_coord(false, drive, part);

    size_t rows, cols;
    init_vga_textmode(&rows, &cols, false);

    volume_read(p, (void *)0x7c00, 0, 512);

    spinup(p->drive);
}

#elif uefi == 1

void chainload(char *config) {
    EFI_STATUS status;

    char *image_path = config_get_value(config, 0, "IMAGE_PATH");
    if (image_path == NULL)
        panic(true, "chainload: IMAGE_PATH not specified");

    struct file_handle *image;
    if ((image = uri_open(image_path)) == NULL)
        panic(true, "chainload: Failed to open image with path `%s`. Is the path correct?", image_path);

    EFI_HANDLE efi_part_handle = image->efi_part_handle;

    void *_ptr = freadall(image, MEMMAP_RESERVED);
    size_t image_size = image->size;
    void *ptr;
    status = gBS->AllocatePool(EfiLoaderData, image_size, &ptr);
    if (status)
        panic(true, "chainload: Allocation failure");
    memcpy(ptr, _ptr, image_size);

    pmm_free(_ptr, image->size);
    fclose(image);

    term_deinit();

    size_t req_width = 0, req_height = 0, req_bpp = 0;

    char *resolution = config_get_value(config, 0, "RESOLUTION");
    if (resolution != NULL)
        parse_resolution(&req_width, &req_height, &req_bpp, resolution);

    struct fb_info fbinfo;
    if (!fb_init(&fbinfo, req_width, req_height, req_bpp))
        panic(true, "chainload: Unable to set video mode");

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

    status = gBS->LoadImage(0, efi_image_handle,
                            (EFI_DEVICE_PATH *)memdev_path,
                            ptr, image_size, &new_handle);
    if (status) {
        panic(false, "chainload: LoadImage failure (%x)", status);
    }

    // Apparently we need to make sure that the DeviceHandle field is the same
    // as us (the loader) for some EFI images to properly work (Windows for instance)
    EFI_GUID loaded_img_prot_guid = EFI_LOADED_IMAGE_PROTOCOL_GUID;

    EFI_LOADED_IMAGE_PROTOCOL *new_handle_loaded_image = NULL;
    status = gBS->HandleProtocol(new_handle, &loaded_img_prot_guid,
                                 (void **)&new_handle_loaded_image);
    if (status) {
        panic(false, "chainload: HandleProtocol failure (%x)", status);
    }

    if (efi_part_handle != 0) {
        new_handle_loaded_image->DeviceHandle = efi_part_handle;
    }

    UINTN exit_data_size = 0;
    CHAR16 *exit_data = NULL;
    EFI_STATUS exit_status = gBS->StartImage(new_handle, &exit_data_size, &exit_data);

    status = gBS->Exit(efi_image_handle, exit_status, exit_data_size, exit_data);
    if (status) {
        panic(false, "chainload: Exit failure (%x)", status);
    }

    __builtin_unreachable();
}

#endif
