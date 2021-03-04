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

void stage3_common(void);

#if defined (uefi)
__attribute__((ms_abi))
EFI_STATUS efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    (void)ImageHandle;

    gST = SystemTable;
    gBS = SystemTable->BootServices;
    gRT = SystemTable->RuntimeServices;

    print("Limine " LIMINE_VERSION "\n%X\n", print);

    volume_create_index();

    EFI_GUID loaded_img_prot_guid = EFI_LOADED_IMAGE_PROTOCOL_GUID;
    EFI_LOADED_IMAGE_PROTOCOL *loaded_image = NULL;

    uefi_call_wrapper(gBS->HandleProtocol, 3, ImageHandle, &loaded_img_prot_guid,
                      &loaded_image);

    struct volume boot_volume = {0};
    if (!disk_volume_from_efi_handle(&boot_volume, loaded_image->DeviceHandle)) {
        panic("Can't determine boot disk");
    }

    if (!volume_iterate_parts(boot_volume,
        if (!init_config_disk(&_PART_)) {
            print("Config file found and loaded.\n");
            boot_partition = _PARTNUM_;
            break;
        }
    )) {
        panic("Config file not found.");
    }

    for (;;);
    //stage3_common();
}
#endif

#if defined (bios)
__attribute__((section(".stage3_build_id")))
uint64_t stage3_build_id = BUILD_ID;

__attribute__((noreturn))
__attribute__((section(".stage3_entry")))
void stage3_entry(int boot_from) {
    mtrr_save();

    switch (boot_from) {
        case BOOT_FROM_HDD:
        case BOOT_FROM_CD: {
            struct volume boot_volume = {0};
            volume_get_by_coord(&boot_volume, boot_drive, -1);

            if (!volume_iterate_parts(boot_volume,
                if (!init_config_disk(&_PART_)) {
                    print("Config file found and loaded.\n");
                    boot_partition = _PARTNUM_;
                    break;
                }
            )) {
                panic("Config file not found.");
            }
            break;
        case BOOT_FROM_PXE:
            pxe_init();
            if (init_config_pxe()) {
                panic("Failed to load config file");
            }
            print("Config loaded via PXE\n");
            break;
        }
    }

    stage3_common();
}
#endif

__attribute__((noreturn))
void stage3_common(void) {
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
