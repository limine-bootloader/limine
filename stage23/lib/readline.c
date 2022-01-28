#include <stdint.h>
#include <stddef.h>
#include <lib/readline.h>
#include <lib/libc.h>
#include <lib/blib.h>
#include <lib/term.h>
#include <lib/print.h>
#if bios == 1
#  include <lib/real.h>
#elif uefi == 1
#  include <efi.h>
#endif

int getchar_internal(uint8_t scancode, uint8_t ascii, uint32_t shift_state) {
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

    if (shift_state & (GETCHAR_LCTRL | GETCHAR_RCTRL)) {
        switch (ascii) {
        case 'p': return GETCHAR_CURSOR_UP;
        case 'n': return GETCHAR_CURSOR_DOWN;
        case 'b': return GETCHAR_CURSOR_LEFT;
        case 'f': return GETCHAR_CURSOR_RIGHT;
        default: break;
        }
    }

    // Guard against non-printable values
    if (ascii < 0x20 || ascii > 0x7e) {
        return -1;
    }
    return ascii;
}

#if bios == 1
int getchar(void) {
    uint8_t scancode = 0;
    uint8_t ascii = 0;
    uint32_t mods = 0;
again:;
    struct rm_regs r = {0};
    rm_int(0x16, &r, &r);
    scancode = (r.eax >> 8) & 0xff;
    ascii = r.eax & 0xff;

    r = (struct rm_regs){ 0 };
    r.eax = 0x0200; // GET SHIFT FLAGS
    rm_int(0x16, &r, &r);

    if (r.eax & GETCHAR_LCTRL) {
        /* the bios subtracts 0x60 from ascii if ctrl is pressed */
        mods = GETCHAR_LCTRL;
        ascii += 0x60;
    }

    int ret = getchar_internal(scancode, ascii, mods);
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
    EFI_KEY_DATA kd;

    UINTN which;

    EFI_EVENT events[1];

    EFI_GUID exproto_guid = EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL_GUID;
    EFI_GUID sproto_guid = EFI_SIMPLE_TEXT_INPUT_PROTOCOL_GUID;
    EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL *exproto = NULL;
    EFI_SIMPLE_TEXT_IN_PROTOCOL *sproto = NULL;

    if (gBS->HandleProtocol(gST->ConsoleInHandle, &exproto_guid, (void **)&exproto) != EFI_SUCCESS) {
        if (gBS->HandleProtocol(gST->ConsoleInHandle, &sproto_guid, (void **)&sproto) != EFI_SUCCESS) {
            panic(false, "Your input device doesn't have an input protocol!");
        }

        events[0] = sproto->WaitForKey;
    } else {
        events[0] = exproto->WaitForKeyEx;
    }

again:
    memset(&kd, 0, sizeof(EFI_KEY_DATA));

    gBS->WaitForEvent(1, events, &which);

    EFI_STATUS status;
    if (events[0] == sproto->WaitForKey) {
        status = sproto->ReadKeyStroke(sproto, &kd.Key);
    } else {
        status = exproto->ReadKeyStrokeEx(exproto, &kd);
    }

    if (status != EFI_SUCCESS) {
        goto again;
    }

    if ((kd.KeyState.KeyShiftState & EFI_SHIFT_STATE_VALID) == 0) {
        kd.KeyState.KeyShiftState = 0;
    }

    int ret = getchar_internal(kd.Key.ScanCode, kd.Key.UnicodeChar,
                               kd.KeyState.KeyShiftState);

    if (ret == -1) {
        goto again;
    }

    return ret;
}

int pit_sleep_and_quit_on_keypress(int seconds) {
    EFI_KEY_DATA kd;

    UINTN which;

    EFI_EVENT events[2];

    EFI_GUID exproto_guid = EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL_GUID;
    EFI_GUID sproto_guid = EFI_SIMPLE_TEXT_INPUT_PROTOCOL_GUID;
    EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL *exproto = NULL;
    EFI_SIMPLE_TEXT_IN_PROTOCOL *sproto = NULL;

    if (gBS->HandleProtocol(gST->ConsoleInHandle, &exproto_guid, (void **)&exproto) != EFI_SUCCESS) {
        if (gBS->HandleProtocol(gST->ConsoleInHandle, &sproto_guid, (void **)&sproto) != EFI_SUCCESS) {
            panic(false, "Your input device doesn't have an input protocol!");
        }

        events[0] = sproto->WaitForKey;
    } else {
        events[0] = exproto->WaitForKeyEx;
    }

    gBS->CreateEvent(EVT_TIMER, TPL_CALLBACK, NULL, NULL, &events[1]);

    gBS->SetTimer(events[1], TimerRelative, 10000000 * seconds);

again:
    memset(&kd, 0, sizeof(EFI_KEY_DATA));

    gBS->WaitForEvent(2, events, &which);

    if (which == 1) {
        return 0;
    }

    EFI_STATUS status;
    if (events[0] == sproto->WaitForKey) {
        status = sproto->ReadKeyStroke(sproto, &kd.Key);
    } else {
        status = exproto->ReadKeyStrokeEx(exproto, &kd);
    }

    if (status != EFI_SUCCESS) {
        goto again;
    }

    if ((kd.KeyState.KeyShiftState & EFI_SHIFT_STATE_VALID) == 0) {
        kd.KeyState.KeyShiftState = 0;
    }

    int ret = getchar_internal(kd.Key.ScanCode, kd.Key.UnicodeChar,
                               kd.KeyState.KeyShiftState);

    if (ret == -1) {
        goto again;
    }

    return ret;
}
#endif

static void reprint_string(int x, int y, const char *s) {
    size_t orig_x, orig_y;
    disable_cursor();
    get_cursor_pos(&orig_x, &orig_y);
    set_cursor_pos(x, y);
    term_write((uintptr_t)s, strlen(s));
    set_cursor_pos(orig_x, orig_y);
    enable_cursor();
}

static void cursor_back(void) {
    size_t x, y;
    get_cursor_pos(&x, &y);
    if (x) {
        x--;
    } else if (y) {
        y--;
        x = term_cols - 1;
    }
    set_cursor_pos(x, y);
}

static void cursor_fwd(void) {
    size_t x, y;
    get_cursor_pos(&x, &y);
    if (x < term_cols - 1) {
        x++;
    } else if (y < term_rows - 1) {
        y++;
        x = 0;
    }
    set_cursor_pos(x, y);
}

void readline(const char *orig_str, char *buf, size_t limit) {
    bool prev_autoflush = term_autoflush;
    term_autoflush = false;

    size_t orig_str_len = strlen(orig_str);
    memset(buf, 0, limit);
    memmove(buf, orig_str, orig_str_len);
    buf[orig_str_len] = 0;

    size_t orig_x, orig_y;
    get_cursor_pos(&orig_x, &orig_y);

    term_write((uintptr_t)orig_str, orig_str_len);

    for (size_t i = orig_str_len; ; ) {
        term_double_buffer_flush();
        int c = getchar();
        switch (c) {
            case GETCHAR_CURSOR_LEFT:
                if (i) {
                    i--;
                    cursor_back();
                }
                continue;
            case GETCHAR_CURSOR_RIGHT:
                if (i < strlen(buf)) {
                    i++;
                    cursor_fwd();
                }
                continue;
            case '\b':
                if (i) {
                    i--;
                    cursor_back();
            case GETCHAR_DELETE:;
                    size_t j;
                    for (j = i; ; j++) {
                        buf[j] = buf[j+1];
                        if (!buf[j]) {
                            buf[j] = ' ';
                            break;
                        }
                    }
                    reprint_string(orig_x, orig_y, buf);
                    buf[j] = 0;
                }
                continue;
            case '\n':
                term_write((uintptr_t)"\n", 1);
                goto out;
            case GETCHAR_END:
                for (size_t j = 0; j < strlen(buf) - i; j++) {
                    cursor_fwd();
                }
                i = strlen(buf);
                continue;
            case GETCHAR_HOME:
                for (size_t j = 0; j < i; j++) {
                    cursor_back();
                }
                i = 0;
                continue;
            default: {
                if (strlen(buf) < limit - 1 && isprint(c)) {
                    for (size_t j = strlen(buf); ; j--) {
                        buf[j+1] = buf[j];
                        if (j == i)
                            break;
                    }
                    buf[i] = c;
                    i++;
                    cursor_fwd();
                    reprint_string(orig_x, orig_y, buf);
                }
            }
        }
    }

out:
    term_double_buffer_flush();
    term_autoflush = prev_autoflush;
}
