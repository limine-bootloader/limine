#include <stddef.h>
#include <stdbool.h>
#include <lib/print.h>
#include <lib/real.h>
#include <lib/trace.h>
#if uefi == 1
#   include <efi.h>
#endif
#include <lib/blib.h>
#include <lib/readline.h>
#include <lib/gterm.h>
#include <lib/term.h>
#include <mm/pmm.h>
#include <menu.h>

__attribute__((noreturn)) void panic(bool allow_menu, const char *fmt, ...) {
    va_list args;

    va_start(args, fmt);

    quiet = false;

    if (term_backend == NOT_READY) {
#if bios == 1
        term_textmode();
#elif uefi == 1
        term_vbe(0, 0);
#endif
    }

    print("\033[31mPANIC\033[37;1m\033[0m: ");
    vprint(fmt, args);

    va_end(args);

    print("\n");
    print_stacktrace(NULL);

    if (
#if bios == 1
      stage3_loaded == true &&
#endif
      allow_menu == true) {
        print("Press a key to return to menu.");

        getchar();

        menu(false);
        __builtin_unreachable();
/*
        fb_clear(&fbinfo);

        // release all uefi memory and return to firmware
        pmm_release_uefi_mem();
        gBS->Exit(efi_image_handle, EFI_ABORTED, 0, NULL);
*/
    } else {
#if bios == 1
        print("Press CTRL+ALT+DEL to reboot.");
        rm_hcf();
#elif uefi == 1
        print("System halted.");
        for (;;) {
            asm ("hlt");
        }
#endif
    }
}
