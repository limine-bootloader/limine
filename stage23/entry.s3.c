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
#include <menu.h>
#include <pxe/pxe.h>
#include <pxe/tftp.h>
#include <drivers/disk.h>

void stage3_common(void);

#if defined (uefi)
EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    gST = SystemTable;
    gBS = SystemTable->BootServices;
    gRT = SystemTable->RuntimeServices;
    efi_image_handle = ImageHandle;

    init_memmap();

    uint32_t colourscheme[] = {
        0x00000000, // black
        0x00aa0000, // red
        0x0000aa00, // green
        0x00aa5500, // brown
        0x000000aa, // blue
        0x00aa00aa, // magenta
        0x0000aaaa, // cyan
        0x00aaaaaa, // grey
        0x00000000, // background (black)
        0x00aaaaaa  // foreground (white)
    };

    term_vbe(colourscheme, 64, 0, NULL);

    print("Limine " LIMINE_VERSION "\n\n");

    disk_create_index();

    EFI_GUID loaded_img_prot_guid = EFI_LOADED_IMAGE_PROTOCOL_GUID;
    EFI_LOADED_IMAGE_PROTOCOL *loaded_image = NULL;

    uefi_call_wrapper(gBS->HandleProtocol, 3, ImageHandle, &loaded_img_prot_guid,
                      &loaded_image);

    boot_volume = disk_volume_from_efi_handle(loaded_image->DeviceHandle);
    if (boot_volume == NULL) {
        panic("Can't determine boot disk");
    }

    // Invalid return address of 0 to end stacktraces here
    asm volatile (
        "push 0\n\t"
        "jmp stage3_common\n\t"
    );

    __builtin_unreachable();
}
#endif

#if defined (bios)
__attribute__((section(".stage3_build_id")))
uint64_t stage3_build_id = BUILD_ID;

__attribute__((section(".stage3_entry")))
#endif
__attribute__((noreturn))
void stage3_common(void) {
    bool got_config = false;
    volume_iterate_parts(boot_volume,
        if (!init_config_disk(_PART)) {
            print("Config file found and loaded.\n");
            boot_volume = _PART;
            got_config = true;
            break;
        }
    );

    if (!got_config)
        panic("Config file not found.");

    print("Boot drive: %x\n", boot_volume->drive);
    print("Boot partition: %d\n", boot_volume->partition);

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
#if defined (bios)
        chainload(config);
#elif defined (uefi)
        panic("UEFI Limine does not support the chainload boot protocol");
#endif
    }

    panic("Invalid protocol specified");
}
