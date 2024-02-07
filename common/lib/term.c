#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <lib/term.h>
#include <lib/real.h>
#include <lib/misc.h>
#include <mm/pmm.h>
#include <drivers/vga_textmode.h>
#include <flanterm/backends/fb.h>

#if defined (BIOS)
int current_video_mode = -1;
#endif

struct flanterm_context **terms = NULL;
size_t terms_i = 0;

int term_backend = _NOT_READY;

void term_notready(void) {
    for (size_t i = 0; i < terms_i; i++) {
        struct flanterm_context *term = terms[i];

        term->deinit(term, pmm_free);
    }

    pmm_free(terms, terms_i * sizeof(void *));

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
    gST->ConOut->SetCursorPosition(gST->ConOut, ctx->cols - 1, ctx->rows - 1);
    CHAR16 string[2];
    string[0] = ' ';
    string[1] = 0;
    gST->ConOut->OutputString(gST->ConOut, string);
    gST->ConOut->SetCursorPosition(gST->ConOut, cursor_x, cursor_y);
}

static void fallback_raw_putchar(struct flanterm_context *ctx, uint8_t c) {
    if (!ctx->scroll_enabled && cursor_x == ctx->cols - 1 && cursor_y == ctx->rows - 1) {
        return;
    }
    gST->ConOut->EnableCursor(gST->ConOut, true);
    CHAR16 string[2];
    string[0] = c;
    string[1] = 0;
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
#endif

static bool dummy_handle(void) {
    return true;
}

void term_fallback(void) {
    term_notready();

#if defined (UEFI)
    if (!efi_boot_services_exited) {
#endif

        terms = ext_mem_alloc(sizeof(void *));
        terms_i = 1;

        terms[0] = ext_mem_alloc(sizeof(struct flanterm_context));

        struct flanterm_context *term = terms[0];

        fallback_clear(NULL, true);

        term->set_text_fg = (void *)dummy_handle;
        term->set_text_bg = (void *)dummy_handle;
        term->set_text_fg_bright = (void *)dummy_handle;
        term->set_text_bg_bright = (void *)dummy_handle;
        term->set_text_fg_rgb = (void *)dummy_handle;
        term->set_text_bg_rgb = (void *)dummy_handle;
        term->set_text_fg_default = (void *)dummy_handle;
        term->set_text_bg_default = (void *)dummy_handle;
        term->move_character = (void *)dummy_handle;
        term->revscroll = (void *)dummy_handle;
        term->swap_palette = (void *)dummy_handle;
        term->save_state = (void *)dummy_handle;
        term->restore_state = (void *)dummy_handle;
        term->double_buffer_flush = (void *)dummy_handle;
        term->full_refresh = (void *)dummy_handle;
        term->deinit = (void *)dummy_handle;

        term->raw_putchar = fallback_raw_putchar;
        term->clear = fallback_clear;
        term->set_cursor_pos = fallback_set_cursor_pos;
        term->get_cursor_pos = fallback_get_cursor_pos;
        term->scroll = fallback_scroll;
        term->cols = 80;
        term->rows = 25;
        term_backend = FALLBACK;
        flanterm_context_reinit(term);
#if defined (UEFI)
    }
#endif
}

extern void reset_term(void);
extern void set_cursor_pos_helper(size_t x, size_t y);

#if defined (__i386__)
#define TERM_XFER_CHUNK 8192

static uint8_t xfer_buf[TERM_XFER_CHUNK];
#endif

static uint64_t context_size(struct flanterm_context *term) {
    switch (term_backend) {
#if defined (BIOS)
        case TEXTMODE:
            return sizeof(struct textmode_context) + (VD_ROWS * VD_COLS) * 2;
#endif
        case GTERM: {
            struct flanterm_fb_context *ctx = (void *)term;
            return sizeof(struct flanterm_fb_context) +
                   ctx->font_bits_size +
                   ctx->font_bool_size +
                   ctx->canvas_size +
                   ctx->grid_size +
                   ctx->queue_size +
                   ctx->map_size;
        }
        default:
            return 0;
    }
}

static void context_save(struct flanterm_context *term, uint64_t buf) {
    switch (term_backend) {
#if defined (BIOS)
        case TEXTMODE: {
            struct textmode_context *ctx = (void *)term;
            memcpy32to64(buf, (uintptr_t)ctx, sizeof(struct textmode_context));
            buf += sizeof(struct textmode_context);
            memcpy32to64(buf, (uintptr_t)ctx->back_buffer, VD_ROWS * VD_COLS);
            buf += VD_ROWS * VD_COLS;
            memcpy32to64(buf, (uintptr_t)ctx->front_buffer, VD_ROWS * VD_COLS);
            buf += VD_ROWS * VD_COLS;
            break;
        }
#endif
        case GTERM: {
            struct flanterm_fb_context *ctx = (void *)term;
            memcpy32to64(buf, (uintptr_t)ctx, sizeof(struct flanterm_fb_context));
            buf += sizeof(struct flanterm_fb_context);
            memcpy32to64(buf, (uintptr_t)ctx->font_bits, ctx->font_bits_size);
            buf += ctx->font_bits_size;
            memcpy32to64(buf, (uintptr_t)ctx->font_bool, ctx->font_bool_size);
            buf += ctx->font_bool_size;
            memcpy32to64(buf, (uintptr_t)ctx->canvas, ctx->canvas_size);
            buf += ctx->canvas_size;
            memcpy32to64(buf, (uintptr_t)ctx->grid, ctx->grid_size);
            buf += ctx->grid_size;
            memcpy32to64(buf, (uintptr_t)ctx->queue, ctx->queue_size);
            buf += ctx->queue_size;
            memcpy32to64(buf, (uintptr_t)ctx->map, ctx->map_size);
            buf += ctx->map_size;
            break;
        }
    }
}

static void context_restore(struct flanterm_context *term, uint64_t buf) {
    switch (term_backend) {
#if defined (BIOS)
        case TEXTMODE: {
            struct textmode_context *ctx = (void *)term;
            memcpy32to64((uintptr_t)ctx, buf, sizeof(struct textmode_context));
            buf += sizeof(struct textmode_context);
            memcpy32to64((uintptr_t)ctx->back_buffer, buf, VD_ROWS * VD_COLS);
            buf += VD_ROWS * VD_COLS;
            memcpy32to64((uintptr_t)ctx->front_buffer, buf, VD_ROWS * VD_COLS);
            buf += VD_ROWS * VD_COLS;
            break;
        }
#endif
        case GTERM: {
            struct flanterm_fb_context *ctx = (void *)term;
            memcpy32to64((uintptr_t)ctx, buf, sizeof(struct flanterm_fb_context));
            buf += sizeof(struct flanterm_fb_context);
            memcpy32to64((uintptr_t)ctx->font_bits, buf, ctx->font_bits_size);
            buf += ctx->font_bits_size;
            memcpy32to64((uintptr_t)ctx->font_bool, buf, ctx->font_bool_size);
            buf += ctx->font_bool_size;
            memcpy32to64((uintptr_t)ctx->canvas, buf, ctx->canvas_size);
            buf += ctx->canvas_size;
            memcpy32to64((uintptr_t)ctx->grid, buf, ctx->grid_size);
            buf += ctx->grid_size;
            memcpy32to64((uintptr_t)ctx->queue, buf, ctx->queue_size);
            buf += ctx->queue_size;
            memcpy32to64((uintptr_t)ctx->map, buf, ctx->map_size);
            buf += ctx->map_size;
            break;
        }
    }
}

void _term_write(struct flanterm_context *term, uint64_t buf, uint64_t count) {
    switch (count) {
        case TERM_OOB_OUTPUT_GET: {
            memcpy32to64(buf, (uint64_t)(uintptr_t)&term->oob_output, sizeof(uint64_t));
            return;
        }
        case TERM_OOB_OUTPUT_SET: {
            memcpy32to64((uint64_t)(uintptr_t)&term->oob_output, buf, sizeof(uint64_t));
            return;
        }
        case TERM_CTX_SIZE: {
            uint64_t ret = context_size(term);
            memcpy32to64(buf, (uint64_t)(uintptr_t)&ret, sizeof(uint64_t));
            return;
        }
        case TERM_CTX_SAVE: {
            context_save(term, buf);
            return;
        }
        case TERM_CTX_RESTORE: {
            context_restore(term, buf);
            return;
        }
        case TERM_FULL_REFRESH: {
            term->full_refresh(term);
            return;
        }
    }

    bool native = false;
#if defined (__x86_64__) || defined (__aarch64__) || defined (__riscv64)
    native = true;
#elif !defined (__i386__)
#error Unknown architecture
#endif

    bool autoflush = term->autoflush;
    term->autoflush = false;

    if (native) {
        const char *s = (const char *)(uintptr_t)buf;

        flanterm_write(term, s, count);
    } else {
#if defined (__i386__)
        while (count != 0) {
            uint64_t chunk;
            if (count > TERM_XFER_CHUNK) {
                chunk = TERM_XFER_CHUNK;
            } else {
                chunk = count;
            }

            memcpy32to64((uint64_t)(uintptr_t)xfer_buf, buf, chunk);

            flanterm_write(term, (const char *)xfer_buf, chunk);

            count -= chunk;
            buf += chunk;
        }
#endif
    }

    if (autoflush) {
        term->double_buffer_flush(term);
    }

    term->autoflush = autoflush;
}
