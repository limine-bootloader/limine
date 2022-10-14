#if defined (UEFI)

#include <stdint.h>
#include <stddef.h>
#include <efi.h>
#include <lib/misc.h>
#include <lib/term.h>
#include <drivers/gop.h>
#include <drivers/edid.h>
#include <lib/print.h>
#include <mm/pmm.h>

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

static bool mode_to_fb_info(EFI_GRAPHICS_OUTPUT_PROTOCOL *gop, struct fb_info *ret, size_t mode) {
    EFI_STATUS status;

    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *mode_info;
    UINTN mode_info_size;

    status = gop->QueryMode(gop, mode, &mode_info_size, &mode_info);

    if (status) {
        return false;
    }

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
            return false;
    }

    ret->memory_model = 0x06;
    ret->framebuffer_pitch = mode_info->PixelsPerScanLine * (ret->framebuffer_bpp / 8);
    ret->framebuffer_width = mode_info->HorizontalResolution;
    ret->framebuffer_height = mode_info->VerticalResolution;

    return true;
}

bool gop_force_16 = false;

static bool try_mode(EFI_GRAPHICS_OUTPUT_PROTOCOL *gop,
                     struct fb_info *ret, size_t mode, uint64_t width, uint64_t height, int bpp) {
    EFI_STATUS status;

    if (!mode_to_fb_info(gop, ret, mode)) {
        return false;
    }

    if (width != 0 && height != 0 && bpp != 0) {
        if (ret->framebuffer_width != width
         || ret->framebuffer_height != height
         || ret->framebuffer_bpp != bpp) {
            return false;
        }
    }

    if (gop_force_16) {
        if (ret->framebuffer_width >= 65536
         || ret->framebuffer_height >= 65536
         || ret->framebuffer_pitch >= 65536) {
            return false;
        }
    }

    printv("gop: Found matching mode %x, attempting to set...\n", mode);

    if ((int)mode == current_video_mode) {
        printv("gop: Mode was already set, perfect!\n");
    } else {
        status = gop->SetMode(gop, mode);

        if (status) {
            current_video_mode = -1;
            printv("gop: Failed to set video mode %x, moving on...\n", mode);
            return false;
        }
    }

    current_video_mode = mode;

    ret->framebuffer_addr = gop->Mode->FrameBufferBase;

    fb_clear(ret);

    return true;
}

struct fb_info *gop_get_mode_list(size_t *count) {
    EFI_STATUS status;

    EFI_HANDLE tmp_handles[1];

    EFI_HANDLE *handles = tmp_handles;
    UINTN handles_size = sizeof(EFI_HANDLE);
    EFI_GUID gop_guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;

    status = gBS->LocateHandle(ByProtocol, &gop_guid, NULL, &handles_size, handles);

    if (status != EFI_SUCCESS && status != EFI_BUFFER_TOO_SMALL) {
        return false;
    }

    handles = ext_mem_alloc(handles_size);

    status = gBS->LocateHandle(ByProtocol, &gop_guid, NULL, &handles_size, handles);
    if (status != EFI_SUCCESS) {
        pmm_free(handles, handles_size);
        return false;
    }

    EFI_HANDLE gop_handle = handles[0];
    pmm_free(handles, handles_size);

    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop;

    status = gBS->HandleProtocol(gop_handle, &gop_guid, (void **)&gop);
    if (status != EFI_SUCCESS) {
        return false;
    }

    UINTN modes_count = gop->Mode->MaxMode;

    struct fb_info *ret = ext_mem_alloc(modes_count * sizeof(struct fb_info));

    size_t actual_count = 0;
    for (size_t i = 0; i < modes_count; i++) {
        if (mode_to_fb_info(gop, &ret[actual_count], i)) {
            actual_count++;
        }
    }

    struct fb_info *tmp = ext_mem_alloc(actual_count * sizeof(struct fb_info));
    memcpy(tmp, ret, actual_count * sizeof(struct fb_info));

    pmm_free(ret, modes_count * sizeof(struct fb_info));
    ret = tmp;

    *count = modes_count;
    return ret;
}

#define INVALID_PRESET_MODE 0xffffffff

static no_unwind size_t preset_mode = INVALID_PRESET_MODE;
static no_unwind EFI_GRAPHICS_OUTPUT_MODE_INFORMATION preset_mode_info;

bool init_gop(struct fb_info *ret,
              uint64_t target_width, uint64_t target_height, uint16_t target_bpp) {
    ret->default_res = false;

    EFI_STATUS status;

    EFI_HANDLE tmp_handles[1];

    EFI_HANDLE *handles = tmp_handles;
    UINTN handles_size = sizeof(EFI_HANDLE);
    EFI_GUID gop_guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;

    status = gBS->LocateHandle(ByProtocol, &gop_guid, NULL, &handles_size, handles);

    if (status != EFI_SUCCESS && status != EFI_BUFFER_TOO_SMALL) {
        return false;
    }

    handles = ext_mem_alloc(handles_size);

    status = gBS->LocateHandle(ByProtocol, &gop_guid, NULL, &handles_size, handles);
    if (status != EFI_SUCCESS) {
        pmm_free(handles, handles_size);
        return false;
    }

    EFI_HANDLE gop_handle = handles[0];
    pmm_free(handles, handles_size);

    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop;

    status = gBS->HandleProtocol(gop_handle, &gop_guid, (void **)&gop);
    if (status != EFI_SUCCESS) {
        return false;
    }

    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *mode_info;
    UINTN mode_info_size;

    status = gop->QueryMode(gop, gop->Mode == NULL ? 0 : gop->Mode->Mode,
                            &mode_info_size, &mode_info);

    if (status == EFI_NOT_STARTED) {
        status = gop->SetMode(gop, 0);
        if (status) {
            panic(false, "gop: Initialisation failed");
        }
        status = gop->QueryMode(gop, gop->Mode == NULL ? 0 : gop->Mode->Mode,
                                &mode_info_size, &mode_info);
    }

    if (status) {
        panic(false, "gop: Initialisation failed");
    }

    if (preset_mode == INVALID_PRESET_MODE) {
        preset_mode = gop->Mode->Mode;
        memcpy(&preset_mode_info, mode_info, mode_info_size);
        current_video_mode = preset_mode;
    }

    struct resolution fallback_resolutions[] = {
        { 0,    0,   0  },   // Overridden by EDID
        { 0,    0,   0  },   // Overridden by preset
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
        goto fallback;
    } else {
        printv("gop: Requested resolution of %ux%ux%u\n",
               target_width, target_height, target_bpp);
    }

retry:
    for (size_t i = 0; i < modes_count; i++) {
        if (try_mode(gop, ret, i, target_width, target_height, target_bpp)) {
            gop_force_16 = false;
            return true;
        }
    }

fallback:
    ret->default_res = true;

    if (current_fallback == 0) {
        current_fallback++;

        struct edid_info_struct *edid_info = get_edid_info();
        if (edid_info != NULL) {
            uint64_t edid_width = (uint64_t)edid_info->det_timing_desc1[2];
                     edid_width += ((uint64_t)edid_info->det_timing_desc1[4] & 0xf0) << 4;
            uint64_t edid_height = (uint64_t)edid_info->det_timing_desc1[5];
                     edid_height += ((uint64_t)edid_info->det_timing_desc1[7] & 0xf0) << 4;
            if (edid_width >= preset_mode_info.HorizontalResolution
             && edid_height >= preset_mode_info.VerticalResolution) {
                target_width  = edid_width;
                target_height = edid_height;
                target_bpp    = 32;
                goto retry;
            }
        }
    }

    if (current_fallback == 1) {
        current_fallback++;

        if (try_mode(gop, ret, preset_mode, 0, 0, 0)) {
            gop_force_16 = false;
            return true;
        }
    }

    if (current_fallback < SIZEOF_ARRAY(fallback_resolutions)) {
        current_fallback++;

        target_width  = fallback_resolutions[current_fallback].width;
        target_height = fallback_resolutions[current_fallback].height;
        target_bpp    = fallback_resolutions[current_fallback].bpp;
        goto retry;
    }

    gop_force_16 = false;
    return false;
}

#endif
