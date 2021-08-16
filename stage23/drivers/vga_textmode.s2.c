#if bios == 1

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <drivers/vga_textmode.h>
#include <sys/cpu.h>
#include <lib/real.h>
#include <lib/libc.h>
#include <lib/blib.h>
#include <lib/term.h>
#include <mm/pmm.h>

#define VIDEO_BOTTOM ((VD_ROWS * VD_COLS) - 1)
#define VD_COLS (80 * 2)
#define VD_ROWS 25

static uint8_t *video_mem = (uint8_t *)0xb8000;

static uint8_t *back_buffer = NULL;
static uint8_t *front_buffer = NULL;

static struct context {
    uint8_t *current_buffer;
#define current_buffer context.current_buffer
    size_t cursor_offset;
#define cursor_offset context.cursor_offset
    bool cursor_status;
#define cursor_status context.cursor_status
    uint8_t text_palette;
#define text_palette context.text_palette
    uint8_t cursor_palette;
#define cursor_palette context.cursor_palette
    bool scroll_enabled;
#define scroll_enabled context.scroll_enabled
} context;

static void clear_cursor(void) {
    if (cursor_status) {
        video_mem[cursor_offset + 1] = current_buffer[cursor_offset + 1];
    }
}

static void draw_cursor(void) {
    if (cursor_status) {
        video_mem[cursor_offset + 1] = cursor_palette;
    }
}

void text_swap_palette(void) {
    text_palette = (text_palette << 4) | (text_palette >> 4);
}

bool text_scroll_disable(void) {
    bool ret = scroll_enabled;
    scroll_enabled = false;
    return ret;
}

void text_scroll_enable(void) {
    scroll_enabled = true;
}

void text_scroll(void) {
    // move the text up by one row
    for (size_t i = term_context.scroll_top_margin * VD_COLS;
         i < (term_context.scroll_bottom_margin - 1) * VD_COLS; i++) {
        current_buffer[i] = current_buffer[i + VD_COLS];
        if (current_buffer == front_buffer)
            video_mem[i] = current_buffer[i + VD_COLS];
    }
    // clear the last line of the screen
    for (size_t i = (term_context.scroll_bottom_margin - 1) * VD_COLS;
         i < term_context.scroll_bottom_margin * VD_COLS; i += 2) {
        current_buffer[i] = ' ';
        current_buffer[i + 1] = text_palette;
        if (current_buffer == front_buffer) {
            video_mem[i] = ' ';
            video_mem[i + 1] = text_palette;
        }
    }
}

void text_clear(bool move) {
    clear_cursor();
    for (size_t i = 0; i < VIDEO_BOTTOM; i += 2) {
        current_buffer[i] = ' ';
        current_buffer[i + 1] = text_palette;
        if (current_buffer == front_buffer) {
            video_mem[i] = ' ';
            video_mem[i + 1] = text_palette;
        }
    }
    if (move)
        cursor_offset = 0;
    draw_cursor();
    return;
}

void text_enable_cursor(void) {
    cursor_status = true;
    draw_cursor();
    return;
}

bool text_disable_cursor(void) {
    bool ret = cursor_status;
    clear_cursor();
    cursor_status = false;
    return ret;
}

uint64_t text_context_size(void) {
    uint64_t ret = 0;

    ret += sizeof(struct context);
    ret += VD_ROWS * VD_COLS; // front buffer

    return ret;
}

void text_context_save(uint64_t ptr) {
    memcpy32to64(ptr, (uint64_t)(uintptr_t)&context, sizeof(struct context));
    ptr += sizeof(struct context);

    memcpy32to64(ptr, (uint64_t)(uintptr_t)front_buffer, VD_ROWS * VD_COLS);
}

void text_context_restore(uint64_t ptr) {
    memcpy32to64((uint64_t)(uintptr_t)&context, ptr, sizeof(struct context));
    ptr += sizeof(struct context);

    memcpy32to64((uint64_t)(uintptr_t)front_buffer, ptr, VD_ROWS * VD_COLS);

    for (size_t i = 0; i < VD_ROWS * VD_COLS; i++) {
        video_mem[i] = front_buffer[i];
    }

    draw_cursor();
}

void text_full_refresh(void) {
    for (size_t i = 0; i < VD_ROWS * VD_COLS; i++) {
        video_mem[i] = front_buffer[i];
    }

    draw_cursor();
}

void init_vga_textmode(size_t *_rows, size_t *_cols, bool managed) {
    if (current_video_mode != -1) {
        struct rm_regs r = {0};
        r.eax = 0x0003;
        rm_int(0x10, &r, &r);

        current_video_mode = -1;
    }

    if (back_buffer == NULL)
        back_buffer = ext_mem_alloc(VD_ROWS * VD_COLS);
    if (front_buffer == NULL)
        front_buffer = ext_mem_alloc(VD_ROWS * VD_COLS);

    current_buffer = front_buffer;
    cursor_offset = 0;
    cursor_status = true;
    text_palette = 0x07;
    cursor_palette = 0x70;
    scroll_enabled = true;

    text_clear(false);

    *_rows = VD_ROWS;
    *_cols = VD_COLS / 2;

    // VGA cursor code taken from: https://wiki.osdev.org/Text_Mode_Cursor

    if (!managed) {
        text_disable_cursor();

        outb(0x3d4, 0x0a);
        outb(0x3d5, (inb(0x3d5) & 0xc0) | 14);
        outb(0x3d4, 0x0b);
        outb(0x3d5, (inb(0x3d5) & 0xe0) | 15);
        outb(0x3d4, 0x0f);
        outb(0x3d5, 0);
        outb(0x3d4, 0x0e);
        outb(0x3d5, 0);
    } else {
        outb(0x3d4, 0x0a);
        outb(0x3d5, 0x20);
    }
}

void text_double_buffer(bool state) {
    if (state) {
        memset(video_mem, 0, VD_ROWS * VD_COLS);
        memset(back_buffer, 0, VD_ROWS * VD_COLS);
        memset(front_buffer, 0, VD_ROWS * VD_COLS);
        current_buffer = back_buffer;
        text_clear(true);
        text_double_buffer_flush();
    } else {
        current_buffer = front_buffer;
        text_clear(true);
    }
}

void text_double_buffer_flush(void) {
    for (size_t i = 0; i < VD_ROWS * VD_COLS; i++) {
        if (back_buffer[i] == front_buffer[i])
            continue;

        front_buffer[i] = back_buffer[i];
        video_mem[i]    = back_buffer[i];
    }

    draw_cursor();
}

void text_get_cursor_pos(size_t *x, size_t *y) {
    *x = (cursor_offset % VD_COLS) / 2;
    *y = cursor_offset / VD_COLS;
}

void text_move_character(size_t new_x, size_t new_y, size_t old_x, size_t old_y) {
    if (old_x >= VD_COLS / 2 || old_y >= VD_ROWS
     || new_x >= VD_COLS / 2 || new_y >= VD_ROWS) {
        return;
    }

    current_buffer[new_y * VD_COLS + new_x * 2] = current_buffer[old_y * VD_COLS + old_x * 2];
    if (current_buffer == front_buffer) {
        video_mem[new_y * VD_COLS + new_x * 2] = current_buffer[old_y * VD_COLS + old_x * 2];
    }
}

void text_set_cursor_pos(size_t x, size_t y) {
    clear_cursor();
    if (x >= VD_COLS / 2) {
        x = VD_COLS / 2 - 1;
    }
    if (y >= VD_ROWS) {
        y = VD_ROWS - 1;
    }
    cursor_offset = y * VD_COLS + x * 2;
    draw_cursor();
}

static uint8_t ansi_colours[] = { 0, 4, 2, 6, 1, 5, 3, 7 };

void text_set_text_fg(size_t fg) {
    text_palette = (text_palette & 0xf0) | ansi_colours[fg];
}

void text_set_text_bg(size_t bg) {
    text_palette = (text_palette & 0x0f) | (ansi_colours[bg] << 4);
}

void text_set_text_fg_bright(size_t fg) {
    text_palette = (text_palette & 0xf0) | (ansi_colours[fg] | (1 << 3));
}

void text_set_text_fg_default(void) {
    text_palette = (text_palette & 0xf0) | 7;
}

void text_set_text_bg_default(void) {
    text_palette &= 0x0f;
}

void text_putchar(uint8_t c) {
    clear_cursor();
    current_buffer[cursor_offset] = c;
    current_buffer[cursor_offset + 1] = text_palette;
    if (current_buffer == front_buffer) {
        video_mem[cursor_offset] = c;
        video_mem[cursor_offset + 1] = text_palette;
    }
    if (cursor_offset / VD_COLS == term_context.scroll_bottom_margin - 1
     && cursor_offset % VD_COLS == VD_COLS - 2) {
        if (scroll_enabled) {
            text_scroll();
            cursor_offset -= cursor_offset % VD_COLS;
        }
    } else if (cursor_offset >= (VIDEO_BOTTOM - 1)) {
        cursor_offset -= cursor_offset % VD_COLS;
    } else {
        cursor_offset += 2;
    }
    draw_cursor();
}

#endif
