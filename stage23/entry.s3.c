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
#include <mm/mtrr.h>
#include <protos/stivale.h>
#include <protos/stivale2.h>
#include <protos/linux.h>
#include <protos/chainload.h>
#include <menu.h>
#include <pxe/pxe.h>
#include <pxe/tftp.h>
#include <drivers/disk.h>

void stage3_common(struct volume *boot_volume);

#if defined (uefi)
EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    gST = SystemTable;
    gBS = SystemTable->BootServices;
    gRT = SystemTable->RuntimeServices;
    efi_image_handle = ImageHandle;

    print("Limine " LIMINE_VERSION "\n\n", print);

    init_memmap();

    disk_create_index();

    EFI_GUID loaded_img_prot_guid = EFI_LOADED_IMAGE_PROTOCOL_GUID;
    EFI_LOADED_IMAGE_PROTOCOL *loaded_image = NULL;

    uefi_call_wrapper(gBS->HandleProtocol, 3, ImageHandle, &loaded_img_prot_guid,
                      &loaded_image);

    struct volume *boot_volume = disk_volume_from_efi_handle(loaded_image->DeviceHandle);
    if (boot_volume == NULL) {
        panic("Can't determine boot disk");
    }

    stage3_common(boot_volume);
}
#endif

#if defined (bios)
__attribute__((section(".stage3_build_id")))
uint64_t stage3_build_id = BUILD_ID;

__attribute__((noreturn))
__attribute__((section(".stage3_entry")))
void stage3_entry(int boot_from) {
    (void)boot_from;

    struct volume *boot_volume = volume_get_by_coord(boot_drive, -1);

    stage3_common(boot_volume);
}
#endif

__attribute__((noreturn))
void stage3_common(struct volume *boot_volume) {
    bool got_config = false;
    volume_iterate_parts(boot_volume,
        if (!init_config_disk(_PART)) {
            print("Config file found and loaded.\n");
            boot_partition = _PARTNO;
            boot_drive = _PART->drive;
            got_config = true;
            break;
        }
    );

    if (!got_config)
        panic("Config file not found.");

    print("Boot drive: %x\n", boot_drive);
    print("Boot partition: %d\n", boot_partition);

    mtrr_save();

    char *cmdline;
    char *config = menu(&cmdline);

    char *proto = config_get_value(config, 0, "PROTOCOL");
    if (proto == NULL) {
        panic("PROTOCOL not specified");
    }

    if (0) {

    } else if (!strcmp(proto, "stivale")) {
        stivale_load(config, cmdline);
    } else if (!strcmp(proto, "stivale2")) {
        stivale2_load(config, cmdline, booted_from_pxe);
#if defined (bios)
    } else if (!strcmp(proto, "linux")) {
        linux_load(config, cmdline);
    } else if (!strcmp(proto, "chainload")) {
        chainload(config);
#endif
    }

    panic("Invalid protocol specified");
}
