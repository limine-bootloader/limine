#include <stddef.h>
#include <stdint.h>
#include <protos/chainload_next.h>
#include <protos/chainload.h>
#include <lib/blib.h>
#include <lib/print.h>
#include <lib/part.h>

#if bios == 1
static void try(char *config, struct volume *v) {
    (void)config;
    bios_chainload_volume(v);
}
#endif

#if uefi == 1
static void try(char *config, struct volume *v) {
    for (int i = 0; i <= v->max_partition + 1; i++) {
        struct file_handle *image;
        struct volume *p = volume_get_by_coord(v->is_optical, v->index, i);

        if ((image = fopen(p, "/EFI/BOOT/BOOTX64.EFI")) == NULL
         && (image = fopen(p, "/efi/boot/bootx64.efi")) == NULL
         && (image = fopen(p, "/EFI/BOOT/BOOTx64.EFI")) == NULL) {
            continue;
        }

        efi_chainload_file(config, image);
    }
}
#endif

void chainload_next(char *config) {
    bool wrap = false;
    for (int i = boot_volume->is_optical ? 0 : (wrap = true, boot_volume->index + 1);
         boot_volume->is_optical ? true : i != boot_volume->index; i++) {
        struct volume *v = volume_get_by_coord(false, i, 0);
        if (v == NULL) {
            if (wrap) {
                i = 0;
                continue;
            } else {
                break;
            }
        }

        try(config, v);
    }

    wrap = false;
    for (int i = boot_volume->is_optical ? (wrap = true, boot_volume->index + 1) : 0;
         boot_volume->is_optical ? i != boot_volume->index : true; i++) {
        struct volume *v = volume_get_by_coord(true, i, 0);
        if (v == NULL) {
            if (wrap) {
                i = 0;
                continue;
            } else {
                break;
            }
        }

        try(config, v);
    }

    panic(true, "chainload_next: No other bootable device");

    __builtin_unreachable();
}
