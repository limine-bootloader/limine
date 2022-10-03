#ifndef __LIB__TERM_H__
#define __LIB__TERM_H__

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <lib/image.h>
#include <lib/print.h>
#include <term/term.h>

enum {
    _NOT_READY,
    GTERM,
    TEXTMODE,
    FALLBACK
};

extern int current_video_mode;
extern int term_backend;

extern struct term_context *term;

#define TERM_CTX_SIZE ((uint64_t)(-1))
#define TERM_CTX_SAVE ((uint64_t)(-2))
#define TERM_CTX_RESTORE ((uint64_t)(-3))
#define TERM_FULL_REFRESH ((uint64_t)(-4))

inline void reset_term(void) {
    term->autoflush = true;
    term->enable_cursor(term);
    print("\e[2J\e[H");
    term->double_buffer_flush(term);
}

inline void set_cursor_pos_helper(size_t x, size_t y) {
    print("\e[%u;%uH", (int)y + 1, (int)x + 1);
}

void term_fallback(void);
void term_textmode(void);

void _term_write(uint64_t buf, uint64_t count);

#endif
