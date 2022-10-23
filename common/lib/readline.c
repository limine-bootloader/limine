#include <stdint.h>
#include <stddef.h>
#include <lib/readline.h>
#include <lib/libc.h>
#include <lib/misc.h>
#include <lib/term.h>
#include <lib/print.h>
#if defined (BIOS)
#  include <lib/real.h>
#elif defined (UEFI)
#  include <efi.h>
#endif
#include <drivers/serial.h>
#include <sys/cpu.h>

int getchar(void) {
    for (;;) {
        int ret = pit_sleep_and_quit_on_keypress(65535);
        if (ret != 0) {
            return ret;
        }
    }
}

int getchar_internal(uint8_t scancode, uint8_t ascii, uint32_t shift_state) {
    switch (scancode) {
#if defined (BIOS)
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
#elif defined (UEFI)
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
        case 'a': return GETCHAR_HOME;
        case 'e': return GETCHAR_END;
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

#if defined (BIOS)
int _pit_sleep_and_quit_on_keypress(uint32_t ticks);

static int input_sequence(void) {
    int val = 0;

    for (;;) {
        int ret = -1;
        size_t retries = 0;

        while (ret == -1 && retries < 1000000) {
            ret = serial_in();
            retries++;
        }
        if (ret == -1) {
            return 0;
        }

        switch (ret) {
            case 'A':
                return GETCHAR_CURSOR_UP;
            case 'B':
                return GETCHAR_CURSOR_DOWN;
            case 'C':
                return GETCHAR_CURSOR_RIGHT;
            case 'D':
                return GETCHAR_CURSOR_LEFT;
            case 'F':
                return GETCHAR_END;
            case 'H':
                return GETCHAR_HOME;
        }

        if (ret > '9' || ret < '0') {
            break;
        }

        val *= 10;
        val += ret - '0';
    }

    switch (val) {
        case 3:
            return GETCHAR_DELETE;
        case 5:
            return GETCHAR_PGUP;
        case 6:
            return GETCHAR_PGDOWN;
        case 21:
            return GETCHAR_F10;
    }

    return 0;
}

int pit_sleep_and_quit_on_keypress(int seconds) {
    if (!serial) {
        return _pit_sleep_and_quit_on_keypress(seconds * 18);
    }

    for (int i = 0; i < seconds * 18; i++) {
        int ret = _pit_sleep_and_quit_on_keypress(1);

        if (ret != 0) {
            return ret;
        }

        ret = serial_in();

        if (ret != -1) {
again:
            switch (ret) {
                case '\r':
                    return '\n';
                case 0x1b:
                    delay(10000);
                    ret = serial_in();
                    if (ret == -1) {
                        return GETCHAR_ESCAPE;
                    }
                    if (ret == '[') {
                        return input_sequence();
                    }
                    goto again;
                case 0x7f:
                    return '\b';
            }

            return ret;
        }
    }

    return 0;
}
#endif

#if defined (UEFI)
static int input_sequence(bool ext,
                   EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL *exproto,
                   EFI_SIMPLE_TEXT_IN_PROTOCOL *sproto) {
    EFI_STATUS status;
    EFI_KEY_DATA kd;

    int val = 0;

    for (;;) {
        if (ext == false) {
            status = sproto->ReadKeyStroke(sproto, &kd.Key);
        } else {
            status = exproto->ReadKeyStrokeEx(exproto, &kd);
        }

        if (status != EFI_SUCCESS) {
            return 0;
        }

        switch (kd.Key.UnicodeChar) {
            case 'A':
                return GETCHAR_CURSOR_UP;
            case 'B':
                return GETCHAR_CURSOR_DOWN;
            case 'C':
                return GETCHAR_CURSOR_RIGHT;
            case 'D':
                return GETCHAR_CURSOR_LEFT;
            case 'F':
                return GETCHAR_END;
            case 'H':
                return GETCHAR_HOME;
        }

        if (kd.Key.UnicodeChar > '9' || kd.Key.UnicodeChar < '0') {
            break;
        }

        val *= 10;
        val += kd.Key.UnicodeChar - '0';
    }

    switch (val) {
        case 3:
            return GETCHAR_DELETE;
        case 5:
            return GETCHAR_PGUP;
        case 6:
            return GETCHAR_PGDOWN;
        case 21:
            return GETCHAR_F10;
    }

    return 0;
}

int pit_sleep_and_quit_on_keypress(int seconds) {
    EFI_KEY_DATA kd;

    UINTN which;

    EFI_EVENT events[2];

    EFI_GUID exproto_guid = EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL_GUID;
    EFI_GUID sproto_guid = EFI_SIMPLE_TEXT_INPUT_PROTOCOL_GUID;
    EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL *exproto = NULL;
    EFI_SIMPLE_TEXT_IN_PROTOCOL *sproto = NULL;

    bool use_sproto = false;

    if (gBS->HandleProtocol(gST->ConsoleInHandle, &exproto_guid, (void **)&exproto) != EFI_SUCCESS) {
        if (gBS->HandleProtocol(gST->ConsoleInHandle, &sproto_guid, (void **)&sproto) != EFI_SUCCESS) {
            if (gST->ConIn != NULL) {
                sproto = gST->ConIn;
            } else {
                panic(false, "Your input device doesn't have an input protocol!");
            }
        }

        events[0] = sproto->WaitForKey;

        use_sproto = true;
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
    if (use_sproto) {
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

    if (kd.Key.ScanCode == 0x08) {
        return '\b';
    }

    if (kd.Key.ScanCode == SCAN_ESC) {
        gBS->CreateEvent(EVT_TIMER, TPL_CALLBACK, NULL, NULL, &events[1]);

        gBS->SetTimer(events[1], TimerRelative, 100000);

        gBS->WaitForEvent(2, events, &which);

        if (which == 1) {
            return GETCHAR_ESCAPE;
        }

        if (use_sproto) {
            status = sproto->ReadKeyStroke(sproto, &kd.Key);
        } else {
            status = exproto->ReadKeyStrokeEx(exproto, &kd);
        }

        if (status != EFI_SUCCESS) {
            goto again;
        }

        if (kd.Key.UnicodeChar == '[') {
            return input_sequence(!use_sproto, exproto, sproto);
        }
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
    term->disable_cursor(term);
    term->get_cursor_pos(term, &orig_x, &orig_y);
    set_cursor_pos_helper(x, y);
    print("%s", s);
    set_cursor_pos_helper(orig_x, orig_y);
    term->enable_cursor(term);
}

static void cursor_back(void) {
    size_t x, y;
    term->get_cursor_pos(term, &x, &y);
    if (x) {
        x--;
    } else if (y) {
        y--;
        x = term->cols - 1;
    }
    set_cursor_pos_helper(x, y);
}

static void cursor_fwd(void) {
    size_t x, y;
    term->get_cursor_pos(term, &x, &y);
    if (x < term->cols - 1) {
        x++;
    } else {
        x = 0;
        if (y < term->rows - 1) {
            y++;
        }
    }
    set_cursor_pos_helper(x, y);
}

void readline(const char *orig_str, char *buf, size_t limit) {
    bool prev_autoflush = term->autoflush;
    term->autoflush = false;

    size_t orig_str_len = strlen(orig_str);
    memmove(buf, orig_str, orig_str_len);
    buf[orig_str_len] = 0;

    size_t orig_x, orig_y;
    term->get_cursor_pos(term, &orig_x, &orig_y);

    print("%s", orig_str);

    for (size_t i = orig_str_len; ; ) {
        term->double_buffer_flush(term);
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
                    if (buf[i] == 0) {
                        continue;
                    }
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
                print("\n");
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
                    size_t prev_x, prev_y;
                    term->get_cursor_pos(term, &prev_x, &prev_y);
                    cursor_fwd();
                    reprint_string(orig_x, orig_y, buf);
                    // If cursor has wrapped around, move the line start position up one row
                    if (prev_x == term->cols - 1 && prev_y == term->rows - 1) {
                        orig_y--;
                        print("\e[J");  // Clear the bottom line
                    }
                }
            }
        }
    }

out:
    term->double_buffer_flush(term);
    term->autoflush = prev_autoflush;
}
