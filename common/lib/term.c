#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <lib/term.h>
#include <lib/real.h>
#include <lib/misc.h>
#include <lib/fb.h>
#include <mm/pmm.h>
#include <mm/mtrr.h>
#include <mm/efi_pt.h>
#include <drivers/vga_textmode.h>
#include <lib/gterm.h>
#include <flanterm_backends/fb.h>

#if defined (BIOS)
int current_video_mode = -1;
#endif

struct flanterm_context **terms = NULL;
size_t terms_i = 0;

// panic() falls back to this terminal precisely when something has already gone
// wrong, which includes after allocations have been disallowed, so it cannot be
// allowed to depend on the allocator.
static struct flanterm_context fallback_ctx;
static struct flanterm_context *fallback_terms[1];

#if defined (UEFI)
// Built while the allocator is live, for panic() to use once it is not.
static struct flanterm_context *post_ebs_term = NULL;
#endif

int term_backend = _NOT_READY;

void term_notready(void) {
#if defined (__i386__) || defined (__x86_64__)
#if defined (__x86_64__) && defined (UEFI)
    efi_pt_restore();
#else
    mtrr_restore();
#endif
#endif

    for (size_t i = 0; i < terms_i; i++) {
        struct flanterm_context *term = terms[i];

#if defined (UEFI)
        // Never released: pmm_free panics once allocations are locked out,
        // which is the state this terminal exists to be usable in.
        if (term == post_ebs_term) {
            continue;
        }
#endif

        term->deinit(term, pmm_free_size_t);
    }

    if (terms != fallback_terms) {
        pmm_free(terms, terms_i * sizeof(void *));
    }

    terms_i = 0;
    terms = NULL;

    term_backend = _NOT_READY;
}

// --- fallback ---

#if defined (BIOS)
static void fallback_raw_putchar(struct flanterm_context *ctx, uint8_t c) {
    (void)ctx;
    struct rm_regs r = {0};
    r.eax = 0x0e00 | c;
    rm_int(0x10, &r, &r);
}

static void fallback_set_cursor_pos(struct flanterm_context *ctx, size_t x, size_t y);
static void fallback_get_cursor_pos(struct flanterm_context *ctx, size_t *x, size_t *y);

static void fallback_clear(struct flanterm_context *ctx, bool move) {
    (void)ctx;
    size_t x, y;
    fallback_get_cursor_pos(NULL, &x, &y);
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
    if (move) {
        x = y = 0;
    }
    fallback_set_cursor_pos(NULL, x, y);
}

static void fallback_set_cursor_pos(struct flanterm_context *ctx, size_t x, size_t y) {
    (void)ctx;
    struct rm_regs r = {0};
    r.eax = 0x0200;
    r.ebx = 0;
    r.edx = (y << 8) + x;
    rm_int(0x10, &r, &r);
}

static void fallback_get_cursor_pos(struct flanterm_context *ctx, size_t *x, size_t *y) {
    (void)ctx;
    struct rm_regs r = {0};
    r.eax = 0x0300;
    r.ebx = 0;
    rm_int(0x10, &r, &r);
    *x = r.edx & 0xff;
    *y = r.edx >> 8;
}

static void fallback_scroll(struct flanterm_context *ctx) {
    (void)ctx;
    size_t x, y;
    fallback_get_cursor_pos(NULL, &x, &y);
    fallback_set_cursor_pos(NULL, ctx->cols - 1, ctx->rows - 1);
    fallback_raw_putchar(NULL, ' ');
    fallback_set_cursor_pos(NULL, x, y);
}

#elif defined (UEFI)

static size_t cursor_x = 0, cursor_y = 0;

static void fallback_scroll(struct flanterm_context *ctx) {
    (void)ctx;
    EFI_UINTN uefi_x_size, uefi_y_size;
    gST->ConOut->QueryMode(gST->ConOut, gST->ConOut->Mode->Mode, &uefi_x_size, &uefi_y_size);
    gST->ConOut->SetCursorPosition(gST->ConOut, uefi_x_size - 1, uefi_y_size - 1);
    EFI_CHAR16 string[2];
    string[0] = ' ';
    string[1] = 0;
    gST->ConOut->OutputString(gST->ConOut, string);
    gST->ConOut->SetCursorPosition(gST->ConOut, cursor_x, cursor_y);
}

// Inverse of the Unicode to CP437 mapping Flanterm applies before raw_putchar,
// taking the code point standard CP437 assigns where it maps several to one
// code. It never produces 0, which is left blank like the IBM glyph.
static const EFI_CHAR16 cp437_to_unicode[256] = {
    0x0020, 0x263a, 0x263b, 0x2665, 0x2666, 0x2663, 0x2660, 0x2022,
    0x25d8, 0x25cb, 0x25d9, 0x2642, 0x2640, 0x266a, 0x266b, 0x263c,
    0x25ba, 0x25c4, 0x2195, 0x203c, 0x00b6, 0x00a7, 0x25ac, 0x21a8,
    0x2191, 0x2193, 0x2192, 0x2190, 0x221f, 0x2194, 0x25b2, 0x25bc,
    0x0020, 0x0021, 0x0022, 0x0023, 0x0024, 0x0025, 0x0026, 0x0027,
    0x0028, 0x0029, 0x002a, 0x002b, 0x002c, 0x002d, 0x002e, 0x002f,
    0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037,
    0x0038, 0x0039, 0x003a, 0x003b, 0x003c, 0x003d, 0x003e, 0x003f,
    0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047,
    0x0048, 0x0049, 0x004a, 0x004b, 0x004c, 0x004d, 0x004e, 0x004f,
    0x0050, 0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057,
    0x0058, 0x0059, 0x005a, 0x005b, 0x005c, 0x005d, 0x005e, 0x005f,
    0x0060, 0x0061, 0x0062, 0x0063, 0x0064, 0x0065, 0x0066, 0x0067,
    0x0068, 0x0069, 0x006a, 0x006b, 0x006c, 0x006d, 0x006e, 0x006f,
    0x0070, 0x0071, 0x0072, 0x0073, 0x0074, 0x0075, 0x0076, 0x0077,
    0x0078, 0x0079, 0x007a, 0x007b, 0x007c, 0x007d, 0x007e, 0x2302,
    0x00c7, 0x00fc, 0x00e9, 0x00e2, 0x00e4, 0x00e0, 0x00e5, 0x00e7,
    0x00ea, 0x00eb, 0x00e8, 0x00ef, 0x00ee, 0x00ec, 0x00c4, 0x00c5,
    0x00c9, 0x00e6, 0x00c6, 0x00f4, 0x00f6, 0x00f2, 0x00fb, 0x00f9,
    0x00ff, 0x00d6, 0x00dc, 0x00a2, 0x00a3, 0x00a5, 0x20a7, 0x0192,
    0x00e1, 0x00ed, 0x00f3, 0x00fa, 0x00f1, 0x00d1, 0x00aa, 0x00ba,
    0x00bf, 0x2310, 0x00ac, 0x00bd, 0x00bc, 0x00a1, 0x00ab, 0x00bb,
    0x2591, 0x2592, 0x2593, 0x2502, 0x2524, 0x2561, 0x2562, 0x2556,
    0x2555, 0x2563, 0x2551, 0x2557, 0x255d, 0x255c, 0x255b, 0x2510,
    0x2514, 0x2534, 0x252c, 0x251c, 0x2500, 0x253c, 0x255e, 0x255f,
    0x255a, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256c, 0x2567,
    0x2568, 0x2564, 0x2565, 0x2559, 0x2558, 0x2552, 0x2553, 0x256b,
    0x256a, 0x2518, 0x250c, 0x2588, 0x2584, 0x258c, 0x2590, 0x2580,
    0x03b1, 0x00df, 0x0393, 0x03c0, 0x03a3, 0x03c3, 0x00b5, 0x03c4,
    0x03a6, 0x0398, 0x03a9, 0x03b4, 0x221e, 0x03c6, 0x03b5, 0x2229,
    0x2261, 0x00b1, 0x2265, 0x2264, 0x2320, 0x2321, 0x00f7, 0x2248,
    0x00b0, 0x2219, 0x00b7, 0x221a, 0x207f, 0x00b2, 0x25a0, 0x00a0,
};

static void fallback_raw_putchar(struct flanterm_context *ctx, uint8_t c) {
    if (!ctx->scroll_enabled && cursor_x == ctx->cols - 1 && cursor_y == ctx->rows - 1) {
        return;
    }
    gST->ConOut->EnableCursor(gST->ConOut, true);
    EFI_CHAR16 string[2];
    string[0] = cp437_to_unicode[c];
    string[1] = 0;
    // '?' is ASCII too, so only non-ASCII is worth testing.
    if (string[0] >= 0x80 && gST->ConOut->TestString(gST->ConOut, string) != EFI_SUCCESS) {
        string[0] = '?';
    }
    gST->ConOut->OutputString(gST->ConOut, string);
    if (++cursor_x >= ctx->cols) {
        cursor_x = 0;
        if (++cursor_y >= ctx->rows) {
            cursor_y--;
        }
    }
    gST->ConOut->SetCursorPosition(gST->ConOut, cursor_x, cursor_y);
}

static void fallback_clear(struct flanterm_context *ctx, bool move) {
    (void)ctx;
    gST->ConOut->ClearScreen(gST->ConOut);
    if (move) {
        cursor_x = cursor_y = 0;
    }
    gST->ConOut->SetCursorPosition(gST->ConOut, cursor_x, cursor_y);
}

static void fallback_set_cursor_pos(struct flanterm_context *ctx, size_t x, size_t y) {
    (void)ctx;
    if (x >= ctx->cols || y >= ctx->rows) {
        return;
    }
    gST->ConOut->SetCursorPosition(gST->ConOut, x, y);
    cursor_x = x;
    cursor_y = y;
}

static void fallback_get_cursor_pos(struct flanterm_context *ctx, size_t *x, size_t *y) {
    (void)ctx;
    *x = cursor_x;
    *y = cursor_y;
}

static EFI_UINTN ansi_colours[] = {
    EFI_BLACK,
    EFI_RED,
    EFI_GREEN,
    EFI_YELLOW,
    EFI_BLUE,
    EFI_MAGENTA,
    EFI_CYAN,
    EFI_LIGHTGRAY
};

static EFI_UINTN conout_current_fg, conout_current_bg;

// SetAttribute rejects any background above EFI_LIGHTGRAY. The stored one
// keeps its bright bit, which reverse video swaps back into the foreground.
static void fallback_set_attribute(void) {
    gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(conout_current_fg, conout_current_bg & 0x07));
}

static void fallback_set_text_fg(struct flanterm_context *ctx, size_t fg) {
    (void)ctx;
    conout_current_fg = ansi_colours[fg];
    fallback_set_attribute();
}

static void fallback_set_text_bg(struct flanterm_context *ctx, size_t bg) {
    (void)ctx;
    conout_current_bg = ansi_colours[bg];
    fallback_set_attribute();
}

static void fallback_set_text_fg_bright(struct flanterm_context *ctx, size_t fg) {
    (void)ctx;
    conout_current_fg = ansi_colours[fg] | EFI_BRIGHT;
    fallback_set_attribute();
}

static void fallback_set_text_bg_bright(struct flanterm_context *ctx, size_t bg) {
    (void)ctx;
    // bg does not support bright
    conout_current_bg = ansi_colours[bg];
    fallback_set_attribute();
}

static void fallback_set_text_fg_default(struct flanterm_context *ctx) {
    (void)ctx;
    conout_current_fg = EFI_LIGHTGRAY;
    fallback_set_attribute();
}

static void fallback_set_text_bg_default(struct flanterm_context *ctx) {
    (void)ctx;
    conout_current_bg = EFI_BLACK;
    fallback_set_attribute();
}

static void fallback_set_text_fg_default_bright(struct flanterm_context *ctx) {
    (void)ctx;
    conout_current_fg = EFI_LIGHTGRAY | EFI_BRIGHT;
    fallback_set_attribute();
}

static void fallback_set_text_bg_default_bright(struct flanterm_context *ctx) {
    (void)ctx;
    // bg does not support bright
    conout_current_bg = EFI_BLACK;
    fallback_set_attribute();
}

static void fallback_swap_palette(struct flanterm_context *ctx) {
    (void)ctx;
    EFI_UINTN tmp = conout_current_bg;
    conout_current_bg = conout_current_fg;
    conout_current_fg = tmp;
    fallback_set_attribute();
}

#endif

static void dummy_handle(struct flanterm_context *ctx) {
    (void)ctx;
}

static void dummy_handle_colour(struct flanterm_context *ctx, size_t colour) {
    (void)ctx;
    (void)colour;
}

static void dummy_handle_rgb(struct flanterm_context *ctx, uint32_t rgb) {
    (void)ctx;
    (void)rgb;
}

static void dummy_handle_move_character(struct flanterm_context *ctx, size_t new_x, size_t new_y, size_t old_x, size_t old_y) {
    (void)ctx;
    (void)new_x;
    (void)new_y;
    (void)old_x;
    (void)old_y;
}

static void dummy_handle_deinit(struct flanterm_context *ctx, void (*_free)(void *, size_t)) {
    (void)ctx;
    (void)_free;
}

#if defined (UEFI)
void term_prepare_post_ebs(void) {
    if (post_ebs_term != NULL) {
        return;
    }

    // ConOut is gone once boot services are, so the framebuffer is the only
    // output left. Where writes to it cannot be made to reach memory there is
    // nothing to fall back to, and a terminal would only cost memory to draw a
    // message that never appears.
    if (!fb_flush_reliable()) {
        return;
    }

    // The graphical terminal is built on 32-bpp framebuffers only, so taking
    // the first of those puts the panic on the screen the menu was drawn on.
    struct fb_info *fb = NULL;
    for (size_t i = 0; i < fb_fbs_count; i++) {
        if (fb_fbs[i].framebuffer_bpp == 32) {
            fb = &fb_fbs[i];
            break;
        }
    }

    if (fb == NULL) {
        return;
    }

    // Staged with autoflush off so that building it does not paint the screen.
    post_ebs_term = flanterm_fb_init(ext_mem_alloc_size_t, pmm_free_size_t,
        (void *)(uintptr_t)fb->framebuffer_addr, fb->framebuffer_width,
        fb->framebuffer_height, fb->framebuffer_pitch,
        fb->red_mask_size, fb->red_mask_shift,
        fb->green_mask_size, fb->green_mask_shift,
        fb->blue_mask_size, fb->blue_mask_shift,
        NULL,
        NULL, NULL,
        NULL, NULL,
        NULL, NULL,
        NULL, 0, 0, 1,
        0, 0,
        0,
        gterm_get_rotation(NULL),
        false
    );

    if (post_ebs_term != NULL) {
        flanterm_fb_set_flush_callback(post_ebs_term, fb_flush_cb);
    }
}
#endif

void term_fallback(void) {
#if defined (UEFI)
    int prev_backend = term_backend;
#endif

    term_notready();

    fallback_terms[0] = &fallback_ctx;
    terms = fallback_terms;
    terms_i = 1;

    struct flanterm_context *term = terms[0];

#if defined (UEFI)
    if (!efi_boot_services_exited) {
        if (prev_backend != FALLBACK && prev_backend != _NOT_READY) {
            gST->ConOut->Reset(gST->ConOut, true);
        }
#endif

        // XXX: Ideally we clear the screen, but that gets rid of the BGRT boot logo
        // and is slow, so...
        //fallback_clear(NULL, true);

        term->set_text_fg = dummy_handle_colour;
        term->set_text_bg = dummy_handle_colour;
        term->set_text_fg_bright = dummy_handle_colour;
        term->set_text_bg_bright = dummy_handle_colour;
        term->set_text_fg_rgb = dummy_handle_rgb;
        term->set_text_bg_rgb = dummy_handle_rgb;
        term->set_text_fg_default = dummy_handle;
        term->set_text_bg_default = dummy_handle;
        term->set_text_fg_default_bright = dummy_handle;
        term->set_text_bg_default_bright = dummy_handle;
        term->move_character = dummy_handle_move_character;
        term->revscroll = dummy_handle;
        term->swap_palette = dummy_handle;
        term->save_state = dummy_handle;
        term->restore_state = dummy_handle;
        term->double_buffer_flush = dummy_handle;
        term->full_refresh = dummy_handle;
        term->deinit = dummy_handle_deinit;

        term->raw_putchar = fallback_raw_putchar;
        term->clear = fallback_clear;
        term->set_cursor_pos = fallback_set_cursor_pos;
        term->get_cursor_pos = fallback_get_cursor_pos;
        term->scroll = fallback_scroll;
        term->cols = 80;
        term->rows = 24;
        term_backend = FALLBACK;
        flanterm_context_reinit(term);
#if defined (UEFI)

        cursor_x = 0;
        cursor_y = 0;
        gST->ConOut->SetCursorPosition(gST->ConOut, 0, 0);

        term->set_text_fg = fallback_set_text_fg;
        term->set_text_bg = fallback_set_text_bg;
        term->set_text_fg_bright = fallback_set_text_fg_bright;
        term->set_text_bg_bright = fallback_set_text_bg_bright;
        term->set_text_fg_default = fallback_set_text_fg_default;
        term->set_text_bg_default = fallback_set_text_bg_default;
        term->set_text_fg_default_bright = fallback_set_text_fg_default_bright;
        term->set_text_bg_default_bright = fallback_set_text_bg_default_bright;
        term->swap_palette = fallback_swap_palette;

        term->set_text_fg_default(term);
        term->set_text_bg_default(term);
    } else {
        if (post_ebs_term == NULL) {
            goto fail;
        }

        terms[0] = post_ebs_term;
        term_backend = FALLBACK;

        // Staged with autoflush off, so this is where it is allowed to paint.
        terms[0]->autoflush = true;
    }

    return;

fail:
    terms_i = 0;
    terms = NULL;
#endif
}
