#if uefi == 1

#include <efi.h>
#include <lib/blib.h>
#include <lib/term.h>
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

static bool try_mode(struct fb_info *ret, size_t mode, int width, int height, int bpp) {
    EFI_STATUS status;

    EFI_GUID gop_guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop;

    gBS->LocateProtocol(&gop_guid, NULL, (void **)&gop);

    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *mode_info;
    UINTN mode_info_size;

    status = gop->QueryMode(gop, mode, &mode_info_size, &mode_info);

    if (status)
        return false;

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
            panic(false, "gop: Invalid PixelFormat");
    }

    if (width != 0 && height != 0 && bpp != 0) {
        if ((int)mode_info->HorizontalResolution != width
         || (int)mode_info->VerticalResolution != height
         || (int)ret->framebuffer_bpp != bpp)
            return false;
    }

    printv("gop: Found matching mode %x, attempting to set...\n", mode);

    if ((int)mode == current_video_mode) {
        printv("gop: Mode was already set, perfect!\n");
    } else {
        status = gop->SetMode(gop, mode);

        if (status) {
            current_video_mode = -2;
            printv("gop: Failed to set video mode %x, moving on...\n", mode);
            return false;
        }
    }

    current_video_mode = mode;

    ret->memory_model = 0x06;
    ret->framebuffer_addr = gop->Mode->FrameBufferBase;
    ret->framebuffer_pitch = gop->Mode->Info->PixelsPerScanLine * (ret->framebuffer_bpp / 8);
    ret->framebuffer_width = gop->Mode->Info->HorizontalResolution;
    ret->framebuffer_height = gop->Mode->Info->VerticalResolution;

    fb_clear(ret);

    return true;
}

#define INVALID_PRESET_MODE 0xffffffff

static size_t preset_mode = INVALID_PRESET_MODE;

bool init_gop(struct fb_info *ret,
              uint16_t target_width, uint16_t target_height, uint16_t target_bpp) {
    ret->default_res = false;

    EFI_STATUS status;

    EFI_GUID gop_guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop;

    gBS->LocateProtocol(&gop_guid, NULL, (void **)&gop);

    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *mode_info;
    UINTN mode_info_size;

    status = gop->QueryMode(gop, gop->Mode == NULL ? 0 : gop->Mode->Mode,
                            &mode_info_size, &mode_info);

    if (status == EFI_NOT_STARTED) {
        status = gop->SetMode(gop, 0);
    }

    if (status) {
        panic(false, "gop: Initialisation failed");
    }

    if (preset_mode == INVALID_PRESET_MODE)
        preset_mode = gop->Mode->Mode;

    struct resolution fallback_resolutions[] = {
        { 0,    0,   0  },   // Overridden by preset mode
        { 1024, 768, 32 },
        { 800,  600, 32 },
        { 640,  480, 32 },
        { 1024, 768, 24 },
        { 800,  600, 24 },
        { 640,  480, 24 },
        { 1024, 768, 16 },
        { 800,  600, 16 },
        { 640,  480, 16 }
    };

    UINTN modes_count = gop->Mode->MaxMode;

    size_t current_fallback = 0;

    if (!target_width || !target_height || !target_bpp) {
        ret->default_res = true;

        struct edid_info_struct *edid_info = get_edid_info();
        if (edid_info != NULL) {
            int edid_width   = (int)edid_info->det_timing_desc1[2];
                edid_width  += ((int)edid_info->det_timing_desc1[4] & 0xf0) << 4;
            int edid_height  = (int)edid_info->det_timing_desc1[5];
                edid_height += ((int)edid_info->det_timing_desc1[7] & 0xf0) << 4;
            if (edid_width && edid_height) {
                target_width  = edid_width;
                target_height = edid_height;
                target_bpp    = 32;
                printv("gop: EDID detected screen resolution of %ux%u\n",
                       target_width, target_height);
                goto retry;
            }
        }
        goto fallback;
    } else {
        printv("gop: Requested resolution of %ux%ux%u\n",
               target_width, target_height, target_bpp);
    }

retry:
    for (size_t i = 0; i < modes_count; i++) {
        if (try_mode(ret, i, target_width, target_height, target_bpp))
            return true;
    }

fallback:
    ret->default_res = true;

    if (current_fallback == 0) {
        if (try_mode(ret, preset_mode, 0, 0, 0))
            return true;

        current_fallback++;
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
