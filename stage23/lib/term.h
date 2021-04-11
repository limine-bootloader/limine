#ifndef __LIB__TERM_H__
#define __LIB__TERM_H__

#include <stddef.h>
#include <stdbool.h>
#include <lib/image.h>

extern void (*raw_putchar)(uint8_t c);
extern void (*clear)(bool move);
extern void (*enable_cursor)(void);
extern void (*disable_cursor)(void);
extern void (*set_cursor_pos)(int x, int y);
extern void (*get_cursor_pos)(int *x, int *y);
extern void (*set_text_fg)(int fg);
extern void (*set_text_bg)(int bg);

extern void (*term_double_buffer)(bool status);
extern void (*term_double_buffer_flush)(void);

void term_vbe(int width, int height);
void term_textmode(void);
void term_write(const char *buf, size_t count);

void term_deinit(void);

extern int term_rows, term_cols;

enum {
    NOT_READY,
    VBE,
    TEXTMODE
};

extern int term_backend;
extern int current_video_mode;

extern bool early_term;

#endif
