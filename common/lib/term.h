#ifndef __LIB__TERM_H__
#define __LIB__TERM_H__

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <lib/image.h>
#include <lib/print.h>

#define TERM_TABSIZE (8)
#define MAX_ESC_VALUES (16)

extern struct term_context {
    bool control_sequence;
    bool csi;
    bool escape;
    bool rrr;
    bool discard_next;
    bool bold;
    bool reverse_video;
    bool dec_private;
    bool insert_mode;
    uint8_t g_select;
    uint8_t charsets[2];
    size_t current_charset;
    size_t escape_offset;
    size_t esc_values_i;
    size_t saved_cursor_x;
    size_t saved_cursor_y;
    size_t current_primary;
    size_t scroll_top_margin;
    size_t scroll_bottom_margin;
    uint32_t esc_values[MAX_ESC_VALUES];

    bool saved_state_bold;
    bool saved_state_reverse_video;
    size_t saved_state_current_charset;
    size_t saved_state_current_primary;
} term_context;

enum {
    NOT_READY,
    VBE,
    TEXTMODE,
    FALLBACK
};

extern int current_video_mode;
extern int term_backend;
extern size_t term_rows, term_cols;
extern bool term_runtime;
extern bool early_term;

void term_fallback(void);

void term_reinit(void);
void term_deinit(void);
void term_vbe(size_t width, size_t height);
void term_textmode(void);
void term_notready(void);
void term_putchar(uint8_t c);
void term_write(uint64_t buf, uint64_t count);

extern void (*raw_putchar)(uint8_t c);
extern void (*clear)(bool move);
extern void (*enable_cursor)(void);
extern bool (*disable_cursor)(void);
extern void (*set_cursor_pos)(size_t x, size_t y);
extern void (*get_cursor_pos)(size_t *x, size_t *y);
extern void (*set_text_fg)(size_t fg);
extern void (*set_text_bg)(size_t bg);
extern void (*set_text_fg_bright)(size_t fg);
extern void (*set_text_bg_bright)(size_t bg);
extern void (*set_text_fg_default)(void);
extern void (*set_text_bg_default)(void);
extern bool (*scroll_disable)(void);
extern void (*scroll_enable)(void);
extern void (*term_move_character)(size_t new_x, size_t new_y, size_t old_x, size_t old_y);
extern void (*term_scroll)(void);
extern void (*term_revscroll)(void);
extern void (*term_swap_palette)(void);
extern void (*term_save_state)(void);
extern void (*term_restore_state)(void);

extern void (*term_double_buffer_flush)(void);

extern uint64_t (*term_context_size)(void);
extern void (*term_context_save)(uint64_t ptr);
extern void (*term_context_restore)(uint64_t ptr);
extern void (*term_full_refresh)(void);

#define TERM_CB_DEC 10
#define TERM_CB_BELL 20
#define TERM_CB_PRIVATE_ID 30
#define TERM_CB_STATUS_REPORT 40
#define TERM_CB_POS_REPORT 50
#define TERM_CB_KBD_LEDS 60
#define TERM_CB_MODE 70
#define TERM_CB_LINUX 80

#define TERM_CTX_SIZE ((uint64_t)(-1))
#define TERM_CTX_SAVE ((uint64_t)(-2))
#define TERM_CTX_RESTORE ((uint64_t)(-3))
#define TERM_FULL_REFRESH ((uint64_t)(-4))

extern void (*term_callback)(uint64_t, uint64_t, uint64_t, uint64_t);

extern bool term_autoflush;

inline void reset_term(void) {
    term_autoflush = true;
    enable_cursor();
    print("\e[2J\e[H");
    term_double_buffer_flush();
}

inline void set_cursor_pos_helper(size_t x, size_t y) {
    print("\e[%u;%uH", (int)y + 1, (int)x + 1);
}

#endif
