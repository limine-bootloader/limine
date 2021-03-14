#if defined (uefi)

#include <efi.h>
#include <lib/blib.h>
#include <drivers/gop.h>
#include <drivers/edid.h>
#include <lib/print.h>

static uint16_t linear_masks_to_bpp(uint32_t red_mask, uint32_t green_mask,
                                    uint32_t blue_mask, uint32_t alpha_mask) {
    uint32_t compound_mask = red_mask | green_mask | blue_mask | alpha_mask;
    uint16_t ret = 32;
    while ((compound_mask & (1 << 31)) == 0) {
        ret--;
        compound_mask <<= 1;
    }
    return ret;
}

static void linear_mask_to_mask_shift(
                uint8_t *mask, uint8_t *shift, uint32_t linear_mask) {
    *shift = 0;
    while ((linear_mask & 1) == 0) {
        (*shift)++;
        linear_mask >>= 1;
    }
    *mask = 0;
    while ((linear_mask & 1) == 1) {
        (*mask)++;
        linear_mask >>= 1;
    }
}

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

    size_t current_fallback = 0;

retry:
    for (size_t i = 0; i < modes_count; i++) {
        status = uefi_call_wrapper(gop->QueryMode, 4,
            gop, i, &mode_info_size, &mode_info);

        if (status)
            continue;

        switch (mode_info->PixelFormat) {
            case PixelBlueGreenRedReserved8BitPerColor:
                ret->framebuffer_bpp = 32;
                ret->red_mask_size = 8;
                ret->red_mask_shift = 16;
                ret->green_mask_size = 8;
                ret->green_mask_shift = 8;
                ret->blue_mask_size = 8;
                ret->blue_mask_shift = 0;
                break;
            case PixelRedGreenBlueReserved8BitPerColor:
                ret->framebuffer_bpp = 32;
                ret->red_mask_size = 8;
                ret->red_mask_shift = 0;
                ret->green_mask_size = 8;
                ret->green_mask_shift = 8;
                ret->blue_mask_size = 8;
                ret->blue_mask_shift = 16;
                break;
            case PixelBitMask:
                ret->framebuffer_bpp = linear_masks_to_bpp(
                                          mode_info->PixelInformation.RedMask,
                                          mode_info->PixelInformation.GreenMask,
                                          mode_info->PixelInformation.BlueMask,
                                          mode_info->PixelInformation.ReservedMask);
                linear_mask_to_mask_shift(&ret->red_mask_size,
                                          &ret->red_mask_shift,
                                          mode_info->PixelInformation.RedMask);
                linear_mask_to_mask_shift(&ret->green_mask_size,
                                          &ret->green_mask_shift,
                                          mode_info->PixelInformation.GreenMask);
                linear_mask_to_mask_shift(&ret->blue_mask_size,
                                          &ret->blue_mask_shift,
                                          mode_info->PixelInformation.BlueMask);
                break;
            default:
                panic("gop: Invalid PixelFormat");
        }

        if (mode_info->HorizontalResolution != target_width
         || mode_info->VerticalResolution != target_height
         || ret->framebuffer_bpp != target_bpp)
            continue;

        print("gop: Found matching mode %x, attempting to set...\n", i);

        status = uefi_call_wrapper(gop->SetMode, 2, gop, i);

        if (status) {
            print("gop: Failed to set video mode %x, moving on...\n", i);
            continue;
        }

        ret->memory_model = 0x06;
        ret->framebuffer_addr = gop->Mode->FrameBufferBase;
        ret->framebuffer_pitch = gop->Mode->Info->PixelsPerScanLine * 4;
        ret->framebuffer_width = gop->Mode->Info->HorizontalResolution;
        ret->framebuffer_height = gop->Mode->Info->VerticalResolution;

        return true;
    }

    if (current_fallback < SIZEOF_ARRAY(fallback_resolutions)) {
        target_width  = fallback_resolutions[current_fallback].width;
        target_height = fallback_resolutions[current_fallback].height;
        target_bpp    = fallback_resolutions[current_fallback].bpp;
        current_fallback++;
        goto retry;
    }

    return false;
}

#endif
