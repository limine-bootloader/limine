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

#if bios == 1
void fallback_print(const char *string) {
    int i;
    struct rm_regs r = {0};
    for (i=0; string[i]; i++) {
        r.eax = 0x0e00 | string[i];
        rm_int(0x10, &r, &r);
    }
}
#endif

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

    if (term_backend == NOT_READY) {
#if bios == 1
        struct rm_regs r = {0};
        rm_int(0x11, &r, &r);
        switch ((r.eax >> 4) & 3) {
            case 0:
                r.eax = 3;
                break;
            case 1:
                r.eax = 1;
                break;
            case 2:
                r.eax = 3;
                break;
            case 3:
                r.eax = 7;
                break;
        }
        rm_int(0x10, &r, &r);
        fallback_print("PANIC: ");
        fallback_print(fmt);
        fallback_print("\r\nPress CTRL+ALT+DEL to reboot.");
        rm_hcf();
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
