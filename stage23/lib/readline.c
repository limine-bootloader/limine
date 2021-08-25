#include <stdint.h>
#include <stddef.h>
#include <lib/readline.h>
#include <lib/libc.h>
#include <lib/blib.h>
#include <lib/term.h>
#if bios == 1
#  include <lib/real.h>
#elif uefi == 1
#  include <efi.h>
#endif

int getchar_internal(uint8_t scancode, uint8_t ascii) {
    switch (scancode) {
#if bios == 1
        case 0x44:
            return GETCHAR_F10;
        case 0x4b:
            return GETCHAR_CURSOR_LEFT;
        case 0x4d:
            return GETCHAR_CURSOR_RIGHT;
        case 0x48:
            return GETCHAR_CURSOR_UP;
        case 0x50:
            return GETCHAR_CURSOR_DOWN;
        case 0x53:
            return GETCHAR_DELETE;
        case 0x4f:
            return GETCHAR_END;
        case 0x47:
            return GETCHAR_HOME;
        case 0x49:
            return GETCHAR_PGUP;
        case 0x51:
            return GETCHAR_PGDOWN;
        case 0x01:
            return GETCHAR_ESCAPE;
#elif uefi == 1
        case SCAN_F10:
            return GETCHAR_F10;
        case SCAN_LEFT:
            return GETCHAR_CURSOR_LEFT;
        case SCAN_RIGHT:
            return GETCHAR_CURSOR_RIGHT;
        case SCAN_UP:
            return GETCHAR_CURSOR_UP;
        case SCAN_DOWN:
            return GETCHAR_CURSOR_DOWN;
        case SCAN_DELETE:
            return GETCHAR_DELETE;
        case SCAN_END:
            return GETCHAR_END;
        case SCAN_HOME:
            return GETCHAR_HOME;
        case SCAN_PAGE_UP:
            return GETCHAR_PGUP;
        case SCAN_PAGE_DOWN:
            return GETCHAR_PGDOWN;
        case SCAN_ESC:
            return GETCHAR_ESCAPE;
#endif
    }
    switch (ascii) {
        case '\r':
            return '\n';
        case '\b':
            return '\b';
    }
    // Guard against non-printable values
    if (ascii < 0x20 || ascii > 0x7e) {
        return -1;
    }
    return ascii;
}

#if bios == 1
int getchar(void) {
again:;
    struct rm_regs r = {0};
    rm_int(0x16, &r, &r);
    int ret = getchar_internal((r.eax >> 8) & 0xff, r.eax);
    if (ret == -1)
        goto again;
    return ret;
}

int _pit_sleep_and_quit_on_keypress(uint32_t ticks);

int pit_sleep_and_quit_on_keypress(int seconds) {
    return _pit_sleep_and_quit_on_keypress(seconds * 18);
}
#endif

#if uefi == 1
int getchar(void) {
again:;
    EFI_INPUT_KEY key = {0};

    UINTN which;

    gBS->WaitForEvent(1, (EFI_EVENT[]){ gST->ConIn->WaitForKey }, &which);

    gST->ConIn->ReadKeyStroke(gST->ConIn, &key);

    int ret = getchar_internal(key.ScanCode, key.UnicodeChar);

    if (ret == -1)
        goto again;

    return ret;
}

int pit_sleep_and_quit_on_keypress(int seconds) {
    EFI_INPUT_KEY key = {0};

    UINTN which;

    EFI_EVENT events[2];

    events[0] = gST->ConIn->WaitForKey;

    gBS->CreateEvent(EVT_TIMER, TPL_CALLBACK, NULL, NULL, &events[1]);

    gBS->SetTimer(events[1], TimerRelative, 10000000 * seconds);

again:
    gBS->WaitForEvent(2, events, &which);

    if (which == 1) {
        return 0;
    }

    gST->ConIn->ReadKeyStroke(gST->ConIn, &key);

    int ret = getchar_internal(key.ScanCode, key.UnicodeChar);

    if (ret == -1)
        goto again;

    return ret;
}
#endif
