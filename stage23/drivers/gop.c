#if defined (uefi)

#include <efi.h>
#include <lib/blib.h>
#include <drivers/gop.h>

bool init_gop(struct fb_info *ret,
              uint16_t target_width, uint16_t target_height, uint16_t target_bpp) {
    (void)ret; (void)target_width; (void)target_height; (void)target_bpp;

    EFI_GUID gop_guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop;

    uefi_call_wrapper(gBS->LocateProtocol, 3, &gop_guid, NULL, (void **)&gop);

    for (;;);
}

#endif
