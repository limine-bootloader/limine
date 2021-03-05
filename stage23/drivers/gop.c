#if defined (uefi)

#include <efi.h>
#include <lib/blib.h>
#include <drivers/gop.h>
#include <drivers/edid.h>
#include <lib/print.h>

// Most of this code taken from https://wiki.osdev.org/GOP

bool init_gop(struct fb_info *ret,
              uint16_t target_width, uint16_t target_height, uint16_t target_bpp) {
    EFI_STATUS status;

    if (!target_width || !target_height || !target_bpp) {
        target_width  = 1024;
        target_height = 768;
        target_bpp    = 32;
        struct edid_info_struct *edid_info = get_edid_info();
        if (edid_info != NULL) {
            int edid_width   = (int)edid_info->det_timing_desc1[2];
                edid_width  += ((int)edid_info->det_timing_desc1[4] & 0xf0) << 4;
            int edid_height  = (int)edid_info->det_timing_desc1[5];
                edid_height += ((int)edid_info->det_timing_desc1[7] & 0xf0) << 4;
            if (edid_width && edid_height) {
                target_width  = edid_width;
                target_height = edid_height;
                print("gop: EDID detected screen resolution of %ux%u\n",
                      target_width, target_height);
            }
        }
    } else {
        print("gop: Requested resolution of %ux%ux%u\n",
              target_width, target_height, target_bpp);
    }

    EFI_GUID gop_guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop;

    uefi_call_wrapper(gBS->LocateProtocol, 3, &gop_guid, NULL, (void **)&gop);

    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *mode_info;
    UINTN mode_info_size;

    status = uefi_call_wrapper(gop->QueryMode, 4, gop,
                               gop->Mode == NULL ? 0 : gop->Mode->Mode,
                               &mode_info_size, &mode_info);

    if (status == EFI_NOT_STARTED) {
        status = uefi_call_wrapper(gop->SetMode, 2, gop, 0);
    }

    if (status) {
        panic("gop: Initialisation failed");
    }

    UINTN modes_count = gop->Mode->MaxMode;

    // Find our mode
    for (size_t i = 0; i < modes_count; i++) {
        status = uefi_call_wrapper(gop->QueryMode, 4,
            gop, i, &mode_info_size, &mode_info);

        if (status)
            continue;

        if (mode_info->HorizontalResolution != target_width
         || mode_info->VerticalResolution != target_height)
            continue;

        print("gop: Found matching mode %x, attempting to set...\n", i);

        status = uefi_call_wrapper(gop->SetMode, 2, gop, i);

        if (status) {
            print("gop: Failed to set video mode %x, moving on...\n", i);
            continue;
        }
    }

    ret->memory_model = 0x06;
    ret->framebuffer_addr = gop->Mode->FrameBufferBase;
    ret->framebuffer_pitch = (gop->Mode->Info->PixelsPerScanLine * 4);
    ret->framebuffer_width = gop->Mode->Info->HorizontalResolution;
    ret->framebuffer_height = gop->Mode->Info->VerticalResolution;
    ret->framebuffer_bpp = 32;
    ret->red_mask_size = 8;
    ret->red_mask_shift = 16;
    ret->green_mask_size = 8;
    ret->green_mask_shift = 8;
    ret->blue_mask_size = 8;
    ret->blue_mask_shift = 0;

    return true;
}

#endif
