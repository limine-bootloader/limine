#include <stddef.h>
#include <stdint.h>
#include <stdnoreturn.h>
#include <lib/term.h>
#include <lib/real.h>
#include <lib/blib.h>
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

#if uefi == 1
__attribute__((naked))
EFI_STATUS efi_main(
    __attribute__((unused)) EFI_HANDLE ImageHandle,
    __attribute__((unused)) EFI_SYSTEM_TABLE *SystemTable) {
    // Invalid return address of 0 to end stacktraces here
#if defined (__x86_64__)
    asm (
        "xorl %eax, %eax\n\t"
        "movq %rax, (%rsp)\n\t"
        "jmp uefi_entry\n\t"
    );
#elif defined (__i386__)
    asm (
        "xorl %eax, %eax\n\t"
        "movl %eax, (%esp)\n\t"
        "jmp uefi_entry\n\t"
    );
#endif
}

extern symbol __image_base;

noreturn void uefi_entry(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    gST = SystemTable;
    gBS = SystemTable->BootServices;
    gRT = SystemTable->RuntimeServices;
    efi_image_handle = ImageHandle;

    EFI_STATUS status;

    status = gBS->SetWatchdogTimer(0, 0x10000, 0, NULL);
    if (status) {
        term_vbe(NULL, 0, 0);
        early_term = true;
        print("WARNING: Failed to disable watchdog timer!\n");
    }

    term_notready();

    init_memmap();

    init_gdt();

#if defined (__x86_64__)
    if ((uintptr_t)__image_base > 0x100000000) {
        panic(false, "Limine does not support being loaded above 4GiB");
    }
#endif

    disk_create_index();

    boot_volume = NULL;

    EFI_HANDLE current_handle = ImageHandle;
    for (;;) {
        if (current_handle == NULL) {
            term_vbe(NULL, 0, 0);
            early_term = true;

            print("WARNING: Could not meaningfully match the boot device handle with a volume.\n");
            print("         Using the first volume containing a Limine configuration!\n");

            for (size_t i = 0; i < volume_index_i; i++) {
                struct file_handle *f;

                if ((f = fopen(volume_index[i], "/limine.cfg")) == NULL
                 && (f = fopen(volume_index[i], "/boot/limine.cfg")) == NULL
                 && (f = fopen(volume_index[i], "/EFI/BOOT/limine.cfg")) == NULL) {
                    continue;
                }

                fclose(f);

                if (volume_index[i]->backing_dev != NULL) {
                    boot_volume = volume_index[i]->backing_dev;
                } else {
                    boot_volume = volume_index[i];
                }

                break;
            }

            if (boot_volume != NULL)
                stage3_common();

            panic(false, "No volume contained a Limine configuration file");
        }

        EFI_GUID loaded_img_prot_guid = EFI_LOADED_IMAGE_PROTOCOL_GUID;
        EFI_LOADED_IMAGE_PROTOCOL *loaded_image = NULL;

        status = gBS->HandleProtocol(current_handle, &loaded_img_prot_guid,
                                     (void **)&loaded_image);

        if (status) {
            panic(false, "HandleProtocol failure (%x)", status);
        }

        boot_volume = disk_volume_from_efi_handle(loaded_image->DeviceHandle);

        if (boot_volume != NULL)
            stage3_common();

        current_handle = loaded_image->ParentHandle;
    }
}
#endif

noreturn void stage3_common(void) {
    term_notready();

    init_flush_irqs();
    init_io_apics();

    menu(true);
}
