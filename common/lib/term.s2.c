#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <lib/term.h>
#include <lib/misc.h>
#include <lib/real.h>

bool early_term = false;

no_unwind int current_video_mode = -1;
int term_backend = NOT_READY;
size_t term_rows, term_cols;
bool term_runtime = false;

void (*raw_putchar)(uint8_t c);
void (*clear)(bool move);
void (*enable_cursor)(void);
bool (*disable_cursor)(void);
void (*set_cursor_pos)(size_t x, size_t y);
void (*get_cursor_pos)(size_t *x, size_t *y);
void (*set_text_fg)(size_t fg);
void (*set_text_bg)(size_t bg);
void (*set_text_fg_bright)(size_t fg);
void (*set_text_bg_bright)(size_t bg);
void (*set_text_fg_default)(void);
void (*set_text_bg_default)(void);
bool (*scroll_disable)(void);
void (*scroll_enable)(void);
void (*term_move_character)(size_t new_x, size_t new_y, size_t old_x, size_t old_y);
void (*term_scroll)(void);
void (*term_revscroll)(void);
void (*term_swap_palette)(void);
void (*term_save_state)(void);
void (*term_restore_state)(void);

void (*term_double_buffer_flush)(void);

uint64_t (*term_context_size)(void);
void (*term_context_save)(uint64_t ptr);
void (*term_context_restore)(uint64_t ptr);
void (*term_full_refresh)(void);

// --- fallback ---

#if defined (BIOS)
static void fallback_raw_putchar(uint8_t c) {
    struct rm_regs r = {0};
    r.eax = 0x0e00 | c;
    rm_int(0x10, &r, &r);
}

static void fallback_clear(bool move) {
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

static void fallback_set_cursor_pos(size_t x, size_t y) {
    struct rm_regs r = {0};
    r.eax = 0x0200;
    r.ebx = 0;
    r.edx = (y << 8) + x;
    rm_int(0x10, &r, &r);
}

static void fallback_get_cursor_pos(size_t *x, size_t *y) {
    struct rm_regs r = {0};
    r.eax = 0x0300;
    r.ebx = 0;
    rm_int(0x10, &r, &r);
    *x = r.edx & 0xff;
    *y = r.edx >> 8;
}

#elif defined (UEFI)
static int cursor_x = 0, cursor_y = 0;

static void fallback_raw_putchar(uint8_t c) {
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

static void fallback_clear(bool move) {
    (void)move;
    gST->ConOut->ClearScreen(gST->ConOut);
    cursor_x = cursor_y = 0;
}

static void fallback_set_cursor_pos(size_t x, size_t y) {
    if (x >= 80 || y >= 25)
        return;
    gST->ConOut->SetCursorPosition(gST->ConOut, x, y);
    cursor_x = x;
    cursor_y = y;
}

static void fallback_get_cursor_pos(size_t *x, size_t *y) {
    *x = cursor_x;
    *y = cursor_y;
}
#endif

void term_fallback(void) {
#if defined (UEFI)
    if (!efi_boot_services_exited) {
        gST->ConOut->Reset(gST->ConOut, false);
        gST->ConOut->SetMode(gST->ConOut, 0);
        cursor_x = cursor_y = 0;
#elif defined (BIOS)
        fallback_clear(true);
#endif
        term_notready();
        raw_putchar = fallback_raw_putchar;
        clear = fallback_clear;
        set_cursor_pos = fallback_set_cursor_pos;
        get_cursor_pos = fallback_get_cursor_pos;
        term_backend = FALLBACK;
#if defined (UEFI)
    }
#endif
}

// --- notready ---

static void notready_raw_putchar(uint8_t c) {
    (void)c;
}
static void notready_clear(bool move) {
    (void)move;
}
static void notready_void(void) {}
static void notready_set_cursor_pos(size_t x, size_t y) {
    (void)x; (void)y;
}
static void notready_get_cursor_pos(size_t *x, size_t *y) {
    *x = 0;
    *y = 0;
}
static void notready_size_t(size_t n) {
    (void)n;
}
static bool notready_disable(void) {
    return false;
}
static void notready_move_character(size_t a, size_t b, size_t c, size_t d) {
    (void)a; (void)b; (void)c; (void)d;
}
static uint64_t notready_context_size(void) {
    return 0;
}
static void notready_uint64_t(uint64_t n) {
    (void)n;
}

void term_notready(void) {
    term_backend = NOT_READY;

    raw_putchar = notready_raw_putchar;
    clear = notready_clear;
    enable_cursor = notready_void;
    disable_cursor = notready_disable;
    set_cursor_pos = notready_set_cursor_pos;
    get_cursor_pos = notready_get_cursor_pos;
    set_text_fg = notready_size_t;
    set_text_bg = notready_size_t;
    set_text_fg_bright = notready_size_t;
    set_text_bg_bright = notready_size_t;
    set_text_fg_default = notready_void;
    set_text_bg_default = notready_void;
    scroll_disable = notready_disable;
    scroll_enable = notready_void;
    term_move_character = notready_move_character;
    term_scroll = notready_void;
    term_revscroll = notready_void;
    term_swap_palette = notready_void;
    term_save_state = notready_void;
    term_restore_state = notready_void;
    term_double_buffer_flush = notready_void;
    term_context_size = notready_context_size;
    term_context_save = notready_uint64_t;
    term_context_restore = notready_uint64_t;
    term_full_refresh = notready_void;

    term_cols = 80;
    term_rows = 24;
}
