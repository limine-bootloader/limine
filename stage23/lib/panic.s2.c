#include <lib/print.h>
#include <lib/real.h>
#include <lib/trace.h>
#if defined (uefi)
#   include <efi.h>
#endif
#include <lib/blib.h>
#include <lib/readline.h>
#include <lib/gterm.h>
#include <mm/pmm.h>

__attribute__((noreturn)) void panic(const char *fmt, ...) {
    va_list args;

    va_start(args, fmt);

    print("\033[31mPANIC\033[37;1m\033[0m: ");
    vprint(fmt, args);

    va_end(args);

    print("\n");
    print_stacktrace(NULL);

#if defined (bios)
    print("System halted.");
    rm_hcf();
#elif defined (uefi)
    if (efi_boot_services_exited == false) {
        print("Press [ENTER] to return to firmware.");
        while (getchar() != '\n');
        fb_clear(&fbinfo);

        // release all uefi memory and return to firmware
        pmm_release_uefi_mem();
        uefi_call_wrapper(gBS->Exit, 4, efi_image_handle, EFI_ABORTED, 0, NULL);
        __builtin_unreachable();
    } else {
        print("System halted.");
        for (;;) {
            asm ("hlt");
        }
    }
#endif
}
