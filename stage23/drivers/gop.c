#if defined (uefi)

#include <efi.h>
#include <lib/blib.h>
#include <drivers/gop.h>
#include <lib/print.h>

// Most of this code taken from https://wiki.osdev.org/GOP

bool init_gop(struct fb_info *ret,
              uint16_t target_width, uint16_t target_height, uint16_t target_bpp) {
    (void)ret; (void)target_width; (void)target_height; (void)target_bpp;

    EFI_STATUS status;

    EFI_GUID gop_guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop;

    uefi_call_wrapper(gBS->LocateProtocol, 3, &gop_guid, NULL, (void **)&gop);

    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *mode_info;
    UINTN mode_info_size;
    //UINTN native_mode, modes_count;

    status = uefi_call_wrapper(gop->QueryMode, 4, gop,
                               gop->Mode == NULL ? 0 : gop->Mode->Mode,
                               &mode_info_size, &mode_info);

    if (status == EFI_NOT_STARTED) {
        status = uefi_call_wrapper(gop->SetMode, 2, gop, 0);
    }

    if (EFI_ERROR(status)) {
        panic("GOP initialisation failed");
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
