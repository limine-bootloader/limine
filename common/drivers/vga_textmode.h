#ifndef __DRIVERS__VGA_TEXTMODE_H__
#define __DRIVERS__VGA_TEXTMODE_H__

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <term/term.h>

struct textmode_context {
    struct term_context term;

    volatile uint8_t *video_mem;

    uint8_t *back_buffer;
    uint8_t *front_buffer;

    size_t cursor_offset;
    size_t old_cursor_offset;
    bool cursor_status;
    uint8_t text_palette;

    uint8_t saved_state_text_palette;
    size_t saved_state_cursor_offset;
};

void vga_textmode_init(bool managed);

#endif
