#if defined (bios)

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <drivers/vga_textmode.h>
#include <sys/cpu.h>
#include <lib/real.h>
#include <lib/libc.h>
#include <lib/term.h>
#include <mm/pmm.h>

#define VIDEO_BOTTOM ((VD_ROWS * VD_COLS) - 1)
#define VD_COLS (80 * 2)
#define VD_ROWS 25

static uint8_t *back_buffer = NULL;
static uint8_t *front_buffer = NULL;
static uint8_t *video_mem = (uint8_t *)0xb8000;

static uint8_t *current_buffer;

static size_t cursor_offset;
static int cursor_status;
static uint8_t text_palette;
static uint8_t cursor_palette;

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

static void scroll(void) {
    // move the text up by one row
    for (size_t i = 0; i <= VIDEO_BOTTOM - VD_COLS; i++) {
        current_buffer[i] = current_buffer[i + VD_COLS];
        if (current_buffer == front_buffer)
            video_mem[i] = current_buffer[i + VD_COLS];
    }
    // clear the last line of the screen
    for (size_t i = VIDEO_BOTTOM; i > VIDEO_BOTTOM - VD_COLS; i -= 2) {
        current_buffer[i] = text_palette;
        current_buffer[i - 1] = ' ';
        if (current_buffer == front_buffer) {
            video_mem[i] = text_palette;
            video_mem[i - 1] = ' ';
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
    cursor_status = 1;
    draw_cursor();
    return;
}

bool text_disable_cursor(void) {
    bool ret = cursor_status != 0;
    clear_cursor();
    cursor_status = 0;
    return ret;
}

void init_vga_textmode(int *_rows, int *_cols, bool managed) {
    if (current_video_mode != -1) {
        struct rm_regs r = {0};
        r.eax = 0x0003;
        rm_int(0x10, &r, &r);

        current_video_mode = -1;
    }

    back_buffer = ext_mem_alloc(VD_ROWS * VD_COLS);
    front_buffer = ext_mem_alloc(VD_ROWS * VD_COLS);

    cursor_offset = 0;
    cursor_status = 1;
    text_palette = 0x07;
    cursor_palette = 0x70;

    text_double_buffer(false);

    text_clear(false);

    *_rows = VD_ROWS;
    *_cols = VD_COLS / 2;

    struct rm_regs r;

    if (!managed) {
        text_disable_cursor();
        r = (struct rm_regs){0};
        r.eax = 0x0200;
        rm_int(0x10, &r, &r);
        r = (struct rm_regs){0};
        r.eax = 0x0100;
        r.ecx = 0x0607;
        rm_int(0x10, &r, &r);
    } else {
        r = (struct rm_regs){0};
        r.eax = 0x0100;
        r.ecx = 0x2706;
        rm_int(0x10, &r, &r);
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

static int text_get_cursor_pos_y(void) {
    return cursor_offset / VD_COLS;
}

void text_get_cursor_pos(int *x, int *y) {
    *x = (cursor_offset % VD_COLS) / 2;
    *y = cursor_offset / VD_COLS;
}

void text_set_cursor_pos(int x, int y) {
    clear_cursor();
    cursor_offset = y * VD_COLS + x * 2;
    draw_cursor();
}

static uint8_t ansi_colours[] = { 0, 4, 2, 0x0e, 1, 5, 3, 7, 0, 7 };

void text_set_text_fg(int fg) {
    text_palette = (text_palette & 0xf0) | ansi_colours[fg];
}

void text_set_text_bg(int bg) {
    text_palette = (text_palette & 0x0f) | (ansi_colours[bg] << 4);
}

void text_putchar(uint8_t c) {
    switch (c) {
        case '\b':
            if (cursor_offset) {
                clear_cursor();
                cursor_offset -= 2;
                draw_cursor();
            }
            break;
        case '\r':
            text_set_cursor_pos(0, text_get_cursor_pos_y());
            break;
        case '\n':
            if (text_get_cursor_pos_y() == (VD_ROWS - 1)) {
                clear_cursor();
                scroll();
                text_set_cursor_pos(0, (VD_ROWS - 1));
            } else {
                text_set_cursor_pos(0, (text_get_cursor_pos_y() + 1));
            }
            break;
        default:
            clear_cursor();
            current_buffer[cursor_offset] = c;
            current_buffer[cursor_offset+1] = text_palette;
            if (current_buffer == front_buffer) {
                video_mem[cursor_offset] = c;
                video_mem[cursor_offset+1] = text_palette;
            }
            if (cursor_offset >= (VIDEO_BOTTOM - 1)) {
                scroll();
                cursor_offset = VIDEO_BOTTOM - (VD_COLS - 1);
            } else {
                cursor_offset += 2;
            }
            draw_cursor();
    }
}

#endif
