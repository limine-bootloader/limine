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
void fallback_raw_putchar(uint8_t c) {
    struct rm_regs r = {0};
    r.eax = 0x0e00 | c;
    rm_int(0x10, &r, &r);
}

void fallback_clear(bool move) {
    (void)move;
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
}

void fallback_set_cursor_pos(size_t x, size_t y) {
    struct rm_regs r = {0};
    r.eax = 0x0200;
    r.ebx = 0;
    r.edx = (y << 8) + x;
    rm_int(0x10, &r, &r);
}

void fallback_get_cursor_pos(size_t *x, size_t *y) {
    struct rm_regs r = {0};
    r.eax = 0x0300;
    r.ebx = 0;
    rm_int(0x10, &r, &r);
    *x = r.edx & 0xff;
    *y = r.edx >> 8;
}

#elif uefi == 1
static int cursor_x = 0, cursor_y = 0;

void fallback_raw_putchar(uint8_t c) {
    CHAR16 string[2];
    string[0] = c;
    string[1] = 0;
    gST->ConOut->OutputString(gST->ConOut, string);
    switch (c) {
        case 0x08:
            if (cursor_x > 0)
                cursor_x--;
            break;
        case 0x0A:
            cursor_x = 0;
            break;
        case 0x0D:
            if (cursor_y < 24)
                cursor_y++;
            break;
        default:
            if (++cursor_x > 80) {
                cursor_x = 0;
                if (cursor_y < 24)
                    cursor_y++;
            }
    }
}

void fallback_clear(bool move) {
    (void)move;
    gST->ConOut->ClearScreen(gST->ConOut);
    cursor_x = cursor_y = 0;
}

void fallback_set_cursor_pos(size_t x, size_t y) {
    if (x >= 80 || y >= 25)
        return;
    gST->ConOut->SetCursorPosition(gST->ConOut, x, y);
    cursor_x = x;
    cursor_y = y;
}

void fallback_get_cursor_pos(size_t *x, size_t *y) {
    *x = cursor_x;
    *y = cursor_y;
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
#if uefi == 1
        if (!efi_boot_services_exited) {
            gST->ConOut->Reset(gST->ConOut, false);
            gST->ConOut->SetMode(gST->ConOut, 0);
            cursor_x = cursor_y = 0;
#elif bios == 1
            fallback_clear(true);
#endif
            term_notready();
            raw_putchar = fallback_raw_putchar;
            clear = fallback_clear;
            set_cursor_pos = fallback_set_cursor_pos;
            get_cursor_pos = fallback_get_cursor_pos;
            term_backend = FALLBACK;
#if uefi == 1
        }
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
