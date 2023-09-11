#include <stddef.h>
#include <stdint.h>
#include <stdnoreturn.h>
#include <lib/term.h>
#include <lib/real.h>
#include <lib/misc.h>
#include <lib/libc.h>
#include <lib/part.h>
#include <lib/config.h>
#include <lib/trace.h>
#include <sys/e820.h>
#include <sys/a20.h>
#include <sys/idt.h>
#include <sys/gdt.h>
#include <lib/print.h>
#include <fs/file.h>
#include <lib/elf.h>
#include <mm/pmm.h>
#include <menu.h>
#include <pxe/pxe.h>
#include <pxe/tftp.h>
#include <drivers/disk.h>
#include <sys/lapic.h>
#include <lib/readline.h>

void stage3_common(void);

#if defined (UEFI)
extern symbol __slide;

noreturn void uefi_entry(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    gST = SystemTable;
    gBS = SystemTable->BootServices;
    gRT = SystemTable->RuntimeServices;
    efi_image_handle = ImageHandle;

    EFI_STATUS status;

    gST->ConOut->EnableCursor(gST->ConOut, false);

    init_memmap();

    term_fallback();

    status = gBS->SetWatchdogTimer(0, 0x10000, 0, NULL);
    if (status) {
        print("WARNING: Failed to disable watchdog timer!\n");
    }

#if defined (__x86_64__) || defined (__i386__)
    init_gdt();
#endif

#if defined (__x86_64__)
    if ((uintptr_t)__slide >= 0x100000000) {
        panic(false, "Limine does not support being loaded above 4GiB");
    }
#endif

    disk_create_index();

    boot_volume = NULL;

    EFI_HANDLE current_handle = ImageHandle;
    for (size_t j = 0; j < 25; j++) {
        if (current_handle == NULL) {
could_not_match:
            print("WARNING: Could not meaningfully match the boot device handle with a volume.\n");
            print("         Using the first volume containing a Limine configuration!\n");

            for (size_t i = 0; i < volume_index_i; i++) {
                struct file_handle *f;

                bool old_cif = case_insensitive_fopen;
                case_insensitive_fopen = true;
                if ((f = fopen(volume_index[i], "/limine.cfg")) == NULL
                 && (f = fopen(volume_index[i], "/limine/limine.cfg")) == NULL
                 && (f = fopen(volume_index[i], "/boot/limine.cfg")) == NULL
                 && (f = fopen(volume_index[i], "/boot/limine/limine.cfg")) == NULL
                 && (f = fopen(volume_index[i], "/EFI/BOOT/limine.cfg")) == NULL) {
                    case_insensitive_fopen = old_cif;
                    continue;
                }
                case_insensitive_fopen = old_cif;

                fclose(f);

                if (volume_index[i]->backing_dev != NULL) {
                    boot_volume = volume_index[i]->backing_dev;
                } else {
                    boot_volume = volume_index[i];
                }

                break;
            }

            if (boot_volume != NULL) {
                stage3_common();
            }

            panic(false, "No volume contained a Limine configuration file");
        }

        EFI_GUID loaded_img_prot_guid = EFI_LOADED_IMAGE_PROTOCOL_GUID;
        EFI_LOADED_IMAGE_PROTOCOL *loaded_image = NULL;

        status = gBS->HandleProtocol(current_handle, &loaded_img_prot_guid,
                                     (void **)&loaded_image);

        if (status) {
            goto could_not_match;
        }

        boot_volume = disk_volume_from_efi_handle(loaded_image->DeviceHandle);

        if (boot_volume != NULL) {
            stage3_common();
        }

        current_handle = loaded_image->ParentHandle;
    }

    goto could_not_match;
}
#endif

noreturn void stage3_common(void) {
#if defined (__x86_64__) || defined (__i386__)
    init_flush_irqs();
    init_io_apics();
#endif

    term_notready();

    menu(true);
}
