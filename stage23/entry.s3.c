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
#include <lib/print.h>
#include <fs/file.h>
#include <lib/elf.h>
#include <mm/pmm.h>
#include <protos/stivale.h>
#include <protos/stivale2.h>
#include <protos/linux.h>
#include <protos/chainload.h>
#include <protos/multiboot1.h>
#include <protos/multiboot2.h>
#include <menu.h>
#include <pxe/pxe.h>
#include <pxe/tftp.h>
#include <drivers/disk.h>
#include <sys/lapic.h>

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

__attribute__((noreturn))
void uefi_entry(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    gST = SystemTable;
    gBS = SystemTable->BootServices;
    gRT = SystemTable->RuntimeServices;
    efi_image_handle = ImageHandle;

    EFI_STATUS status;

    status = gBS->SetWatchdogTimer(0, 0x10000, 0, NULL);
    if (status) {
        term_vbe(0, 0);
        early_term = true;
        print("WARNING: Failed to disable watchdog timer!\n");
    }

    term_notready();

    init_memmap();

    disk_create_index();

    boot_volume = NULL;

    EFI_HANDLE current_handle = ImageHandle;
    for (;;) {
        if (current_handle == NULL) {
            term_vbe(0, 0);
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

            panic("No volume contained a Limine configuration file");
        }

        EFI_GUID loaded_img_prot_guid = EFI_LOADED_IMAGE_PROTOCOL_GUID;
        EFI_LOADED_IMAGE_PROTOCOL *loaded_image = NULL;

        status = gBS->HandleProtocol(current_handle, &loaded_img_prot_guid,
                                     (void **)&loaded_image);

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

#if bios == 1
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

    char *quiet_str = config_get_value(NULL, 0, "QUIET");
    quiet = quiet_str != NULL && strcmp(quiet_str, "yes") == 0;

    char *verbose_str = config_get_value(NULL, 0, "VERBOSE");
    verbose = verbose_str != NULL && strcmp(verbose_str, "yes") == 0;

    char *randomise_mem_str = config_get_value(NULL, 0, "RANDOMISE_MEMORY");
    if (randomise_mem_str == NULL)
        randomise_mem_str = config_get_value(NULL, 0, "RANDOMIZE_MEMORY");
    bool randomise_mem = randomise_mem_str != NULL && strcmp(randomise_mem_str, "yes") == 0;
    if (randomise_mem)
        pmm_randomise_memory();

    init_flush_irqs();
    init_io_apics();

    if (verbose) {
        print("Boot drive: %d\n", boot_volume->index);
        print("Boot partition: %d\n", boot_volume->partition);
    }

    char *cmdline;
    char *config = menu(&cmdline);

    char *proto = config_get_value(config, 0, "PROTOCOL");
    if (proto == NULL) {
        printv("PROTOCOL not specified, using autodetection...\n");
autodetect:
        stivale2_load(config, cmdline);
        stivale_load(config, cmdline);
        multiboot2_load(config, cmdline);
        multiboot1_load(config, cmdline);
        linux_load(config, cmdline);
        panic("Kernel protocol autodetection failed");
    }

    bool ret = true;

    if (!strcmp(proto, "stivale1") || !strcmp(proto, "stivale")) {
        ret = stivale_load(config, cmdline);
    } else if (!strcmp(proto, "stivale2")) {
        ret = stivale2_load(config, cmdline);
    } else if (!strcmp(proto, "linux")) {
        ret = linux_load(config, cmdline);
    } else if (!strcmp(proto, "multiboot1") || !strcmp(proto, "multiboot")) {
        ret = multiboot1_load(config, cmdline);
    } else if (!strcmp(proto, "multiboot2")) {
        ret = multiboot2_load(config, cmdline);
    } else if (!strcmp(proto, "chainload")) {
        chainload(config);
    }

    if (ret) {
        print("WARNING: Unsupported protocol specified: %s.\n", proto);
    } else {
        print("WARNING: Incorrect protocol specified for kernel.\n");
    }

    print("         Attempting autodetection.\n");
    goto autodetect;
}
