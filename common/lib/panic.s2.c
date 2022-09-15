#include <stddef.h>
#include <stdbool.h>
#include <stdnoreturn.h>
#include <lib/print.h>
#include <lib/real.h>
#include <lib/trace.h>
#if defined (UEFI)
#   include <efi.h>
#endif
#include <lib/misc.h>
#include <lib/readline.h>
#include <lib/gterm.h>
#include <lib/term.h>
#include <mm/pmm.h>
#include <menu.h>

noreturn void panic(bool allow_menu, const char *fmt, ...) {
    va_list args;

    va_start(args, fmt);

    quiet = false;

    static bool is_nested = false;

    if (is_nested) {
        goto nested;
    }

    is_nested = true;

#if defined (BIOS)
    if (stage3_loaded == true && term_backend == NOT_READY) {
        early_term = true;
        term_vbe(NULL, 0, 0);
    }
#endif

#if defined (UEFI)
    if (term_backend == NOT_READY) {
        if (term_enabled_once) {
            term_vbe(NULL, 0, 0);
        } else {
            term_fallback();
        }
    }
#endif

nested:
    if (term_backend == NOT_READY) {
        term_fallback();
    }

#if defined (BIOS)
    if (stage3_loaded) {
#endif
        print("\033[31mPANIC\033[37;1m\033[0m: ");
#if defined (BIOS)
    } else {
        print("PANIC: ");
    }
#endif
    vprint(fmt, args);

    va_end(args);

    print("\n");
    print_stacktrace(NULL);

    if (
#if defined (BIOS)
      stage3_loaded == true &&
#endif
      allow_menu == true) {
        print("Press a key to return to menu.");

        getchar();

        menu(false);
/*
        fb_clear(&fbinfo);

        // release all uefi memory and return to firmware
        pmm_release_uefi_mem();
        gBS->Exit(efi_image_handle, EFI_ABORTED, 0, NULL);
*/
    } else {
#if defined (BIOS)
        print("Press CTRL+ALT+DEL to reboot.");
        rm_hcf();
#elif defined (UEFI)
        print("System halted.");
        for (;;) {
#if defined (__x86_64__) || defined (__i386__)
            asm ("hlt");
#elif defined (__aarch64__)
            asm ("wfi");
#else
#error Unknown architecture
#endif
        }
#endif
    }
}
