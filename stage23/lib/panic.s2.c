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

static const char* recursive_panic = 0;

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

__attribute__((noreturn)) void panic(const char *fmt, ...) {

    va_list args;

    va_start(args, fmt);

    quiet = false;

    if (recursive_panic) {
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
        fallback_print("RECURSIVE PANIC: \r\n");
        fallback_print("Original panic: ");
        fallback_print(recursive_panic);
        fallback_print("\r\nSubsequent panic: ");
        fallback_print(fmt);
        fallback_print("\r\nPress CTRL+ALT+DEL to reboot.");
        rm_hcf();
#endif
    }

    recursive_panic = fmt;

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

#if bios == 1
    print("Press CTRL+ALT+DEL to reboot.");
    rm_hcf();
#elif uefi == 1
    if (efi_boot_services_exited == false) {
        print("Press [ENTER] to return to firmware.");
        while (getchar() != '\n');
        fb_clear(&fbinfo);

        // release all uefi memory and return to firmware
        pmm_release_uefi_mem();
        gBS->Exit(efi_image_handle, EFI_ABORTED, 0, NULL);
        __builtin_unreachable();
    } else {
        print("System halted.");
        for (;;) {
            asm ("hlt");
        }
    }
#endif
}
