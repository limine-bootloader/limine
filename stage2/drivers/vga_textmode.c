#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <sys/cpu.h>
#include <lib/real.h>
#include <drivers/vga_textmode.h>

#define VIDEO_BOTTOM ((VD_ROWS * VD_COLS) - 1)
#define VD_COLS (80 * 2)
#define VD_ROWS 25

static char *video_mem = (char *)0xb8000;
static size_t cursor_offset = 0;
static int cursor_status = 1;
static uint8_t text_palette = 0x07;
static uint8_t cursor_palette = 0x70;

static void clear_cursor(void) {
    video_mem[cursor_offset + 1] = text_palette;
    return;
}

static void draw_cursor(void) {
    if (cursor_status) {
        video_mem[cursor_offset + 1] = cursor_palette;
    }
    return;
}

static void scroll(void) {
    // move the text up by one row
    for (size_t i = 0; i <= VIDEO_BOTTOM - VD_COLS; i++)
        video_mem[i] = video_mem[i + VD_COLS];
    // clear the last line of the screen
    for (size_t i = VIDEO_BOTTOM; i > VIDEO_BOTTOM - VD_COLS; i -= 2) {
        video_mem[i] = text_palette;
        video_mem[i - 1] = ' ';
    }
    return;
}

void text_clear(bool move) {
    clear_cursor();
    for (size_t i = 0; i < VIDEO_BOTTOM; i += 2) {
        video_mem[i] = ' ';
        video_mem[i + 1] = text_palette;
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

void text_disable_cursor(void) {
    cursor_status = 0;
    clear_cursor();
    return;
}

// VGA cursor code taken from: https://wiki.osdev.org/Text_Mode_Cursor

void init_vga_textmode(int *_rows, int *_cols) {
    outb(0x3d4, 0x0a);
    outb(0x3d5, 0x20);
    text_clear(true);

    *_rows = VD_ROWS;
    *_cols = VD_COLS / 2;
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

static uint8_t ansi_colours[] = { 0, 4, 2, 0x0e, 1, 5, 3, 7 };

void text_set_text_fg(int fg) {
    text_palette = (text_palette & 0xf0) | ansi_colours[fg];
}

void text_set_text_bg(int bg) {
    text_palette = (text_palette & 0x0f) | (ansi_colours[bg] << 4);
}

void text_putchar(char c) {
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
            video_mem[cursor_offset] = c;
            if (cursor_offset >= (VIDEO_BOTTOM - 1)) {
                scroll();
                cursor_offset = VIDEO_BOTTOM - (VD_COLS - 1);
            } else
                cursor_offset += 2;
            draw_cursor();
    }
}
