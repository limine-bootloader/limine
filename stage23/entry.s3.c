#include <lib/term.h>
#include <lib/real.h>
#include <lib/blib.h>
#include <lib/libc.h>
#include <lib/part.h>
#include <lib/config.h>
#include <lib/trace.h>
#include <sys/e820.h>
#include <sys/a20.h>
#include <lib/print.h>
#include <fs/file.h>
#include <lib/elf.h>
#include <mm/pmm.h>
#include <protos/stivale.h>
#include <protos/stivale2.h>
#include <protos/linux.h>
#include <protos/chainload.h>
#include <protos/multiboot1.h>
#include <menu.h>
#include <pxe/pxe.h>
#include <pxe/tftp.h>
#include <drivers/disk.h>

void stage3_common(void);

#if defined (uefi)
__attribute__((naked))
EFI_STATUS efi_main(
    __attribute__((unused)) EFI_HANDLE ImageHandle,
    __attribute__((unused)) EFI_SYSTEM_TABLE *SystemTable) {
    // Invalid return address of 0 to end stacktraces here
    asm (
        "pushq $0\n\t"
        "pushq $0\n\t"
        "xorl %eax, %eax\n\t"
        "jmp uefi_entry\n\t"
    );
}

__attribute__((noreturn))
void uefi_entry(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    gST = SystemTable;
    gBS = SystemTable->BootServices;
    gRT = SystemTable->RuntimeServices;
    efi_image_handle = ImageHandle;

    EFI_STATUS status;

    init_memmap();

    term_vbe(0, 0);
    early_term = true;

    print("Limine " LIMINE_VERSION "\n\n");

    disk_create_index();

    EFI_HANDLE current_handle = ImageHandle;
    for (;;) {
        EFI_GUID loaded_img_prot_guid = EFI_LOADED_IMAGE_PROTOCOL_GUID;
        EFI_LOADED_IMAGE_PROTOCOL *loaded_image = NULL;

        status = uefi_call_wrapper(gBS->HandleProtocol, 3,
                                   current_handle, &loaded_img_prot_guid,
                                   &loaded_image);

        if (status) {
            panic("HandleProtocol failure (%x)", status);
        }

        boot_volume = disk_volume_from_efi_handle(loaded_image->DeviceHandle);

        if (boot_volume != NULL)
            stage3_common();

        current_handle = loaded_image->ParentHandle;
    }
}
#endif

#if defined (bios)
__attribute__((section(".stage3_build_id")))
uint64_t stage3_build_id = BUILD_ID;

__attribute__((section(".stage3_entry")))
#endif
__attribute__((noreturn))
void stage3_common(void) {
    volume_iterate_parts(boot_volume,
        if (!init_config_disk(_PART)) {
            boot_volume = _PART;
            break;
        }
    );

    char *verbose_str = config_get_value(NULL, 0, "VERBOSE");
    verbose = verbose_str != NULL && strcmp(verbose_str, "yes") == 0;

    if (verbose) {
        print("Boot drive: %x\n", boot_volume->index);
        print("Boot partition: %d\n", boot_volume->partition);
    }

    char *cmdline;
    char *config = menu(&cmdline);

    char *proto = config_get_value(config, 0, "PROTOCOL");
    if (proto == NULL) {
        panic("PROTOCOL not specified");
    }

    if (!strcmp(proto, "stivale")) {
        stivale_load(config, cmdline);
    } else if (!strcmp(proto, "stivale2")) {
#if defined (bios)
        void *efi_system_table = NULL;
#elif defined (uefi)
        void *efi_system_table = gST;
#endif

        stivale2_load(config, cmdline, boot_volume->pxe, efi_system_table);
    } else if (!strcmp(proto, "linux")) {
        linux_load(config, cmdline);
    } else if (!strcmp(proto, "chainload")) {
        chainload(config);
    } else if (!strcmp(proto, "multiboot1")) {
        multiboot1_load(config, cmdline);
    }

    panic("Invalid protocol specified");
}
