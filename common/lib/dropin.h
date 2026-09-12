#ifndef LIB__DROPIN_H__
#define LIB__DROPIN_H__

#if defined (UEFI)

#include <efi.h>

// Loads and starts every EFI driver dropped into `\EFI\systemd\drivers` on
// the volume `device_handle` names, then reconnects the firmware's device
// handles so the drivers take effect.
void dropin_load_drivers(EFI_HANDLE parent_image, EFI_HANDLE device_handle);

#endif

#endif
