#include <stdint.h>
#include <stddef.h>
#include <lib/gterm.h>
#include <lib/blib.h>
#include <lib/libc.h>
#include <lib/config.h>
#include <lib/print.h>
#include <lib/uri.h>
#include <lib/fb.h>
#include <mm/mtrr.h>
#include <mm/pmm.h>

#define VGA_FONT_WIDTH  8
#define VGA_FONT_HEIGHT 16
#define VGA_FONT_GLYPHS 256
#define VGA_FONT_MAX    (VGA_FONT_HEIGHT * VGA_FONT_GLYPHS)

struct fb_info fbinfo;
static uint32_t *gterm_framebuffer;
static uint16_t  gterm_pitch;
static uint16_t  gterm_width;
static uint16_t  gterm_height;
static uint16_t  gterm_bpp;

extern symbol _binary_font_bin_start;

static uint8_t *vga_font = NULL;

static uint32_t ansi_colours[10];

static int frame_height, frame_width;

static struct image *background;

static struct gterm_char *grid = NULL;
static struct gterm_char *front_grid = NULL;

static uint32_t *bg_canvas = NULL;

static bool double_buffer_enabled = false;

static bool cursor_status = true;

static int cursor_x;
static int cursor_y;

static int rows;
static int cols;
static int margin_gradient;

#define A(rgb) (uint8_t)(rgb >> 24)
#define R(rgb) (uint8_t)(rgb >> 16)
#define G(rgb) (uint8_t)(rgb >> 8)
#define B(rgb) (uint8_t)(rgb)
#define ARGB(a, r, g, b) (a << 24) | ((r & 0xFF) << 16) | ((g & 0xFF) << 8) | (b & 0xFF)

static inline uint32_t colour_blend(uint32_t fg, uint32_t bg) {
    unsigned alpha = 255 - A(fg);
    unsigned inv_alpha = A(fg) + 1;

    uint8_t r = (uint8_t)((alpha * R(fg) + inv_alpha * R(bg)) / 256);
    uint8_t g = (uint8_t)((alpha * G(fg) + inv_alpha * G(bg)) / 256);
    uint8_t b = (uint8_t)((alpha * B(fg) + inv_alpha * B(bg)) / 256);

    return ARGB(0, r, g, b);
}

void gterm_plot_px(int x, int y, uint32_t hex) {
    size_t fb_i = x + (gterm_pitch / sizeof(uint32_t)) * y;

    gterm_framebuffer[fb_i] = hex;
}

static uint32_t blend_gradient_from_box(int x, int y, uint32_t hex) {
    if (x >= frame_width  && x < frame_width  + VGA_FONT_WIDTH  * cols
     && y >= frame_height && y < frame_height + VGA_FONT_HEIGHT * rows) {
        return colour_blend(hex, background->get_pixel(background, x, y));
    }

    uint32_t bg_px = background->get_pixel(background, x, y);

    if (margin_gradient == 0)
        return bg_px;

    int distance, x_distance, y_distance;

    if (x < frame_width)
        x_distance = frame_width - x;
    else
        x_distance = x - (frame_width + VGA_FONT_WIDTH * cols);

    if (y < frame_height)
        y_distance = frame_height - y;
    else
        y_distance = y - (frame_height + VGA_FONT_HEIGHT * rows);

    if (x >= frame_width && x < frame_width + VGA_FONT_WIDTH * cols) {
        distance = y_distance;
    } else if (y >= frame_height && y < frame_height + VGA_FONT_HEIGHT * rows) {
        distance = x_distance;
    } else {
        distance = sqrt((uint64_t)x_distance * (uint64_t)x_distance
                      + (uint64_t)y_distance * (uint64_t)y_distance);
    }

    if (distance > margin_gradient)
        return bg_px;

    uint8_t gradient_step = (0xff - A(hex)) / margin_gradient;
    uint8_t new_alpha     = A(hex) + gradient_step * distance;

    return colour_blend((hex & 0xffffff) | (new_alpha << 24), bg_px);
}

void gterm_generate_canvas(void) {
    if (background) {
        for (int y = 0; y < gterm_height; y++) {
            for (int x = 0; x < gterm_width; x++) {
                bg_canvas[y * gterm_width + x] = blend_gradient_from_box(x, y, ansi_colours[8]);
                gterm_plot_px(x, y, bg_canvas[y * gterm_width + x]);
            }
        }
    } else {
        for (int y = 0; y < gterm_height; y++) {
            for (int x = 0; x < gterm_width; x++) {
                bg_canvas[y * gterm_width + x] = ansi_colours[8];
                gterm_plot_px(x, y, ansi_colours[8]);
            }
        }
    }
}

struct gterm_char {
    uint32_t c;
    int fg;
    int bg;
};

static void plot_char_mem(uint32_t *buf, struct gterm_char *c, int x, int y) {
    uint8_t *glyph = &vga_font[(size_t)c->c * VGA_FONT_HEIGHT];

    for (int i = 0; i < VGA_FONT_HEIGHT; i++) {
        for (int j = 0; j < VGA_FONT_WIDTH; j++) {
            if ((glyph[i] & (0x80 >> j))) {
                buf[i * VGA_FONT_WIDTH + j] = ansi_colours[c->fg];
            } else {
                if (c->bg == 8)
                    buf[i * VGA_FONT_WIDTH + j] = bg_canvas[(y + i) * gterm_width + (x + j)];
                else
                    buf[i * VGA_FONT_WIDTH + j] = ansi_colours[c->bg];
            }
        }
    }
}

void gterm_plot_char(struct gterm_char *c, int x, int y) {
    uint8_t *glyph = &vga_font[(size_t)c->c * VGA_FONT_HEIGHT];

    for (int i = 0; i < VGA_FONT_HEIGHT; i++) {
        for (int j = 0; j < VGA_FONT_WIDTH; j++) {
            if ((glyph[i] & (0x80 >> j))) {
                gterm_plot_px(x + j, y + i, ansi_colours[c->fg]);
            } else {
                if (c->bg == 8)
                    gterm_plot_px(x + j, y + i, bg_canvas[(y + i) * gterm_width + (x + j)]);
                else
                    gterm_plot_px(x + j, y + i, ansi_colours[c->bg]);
            }
        }
    }
}

static void plot_char_grid(struct gterm_char *c, int x, int y) {
    uint32_t old_char[VGA_FONT_WIDTH * VGA_FONT_HEIGHT];
    uint32_t new_char[VGA_FONT_WIDTH * VGA_FONT_HEIGHT];

    plot_char_mem(old_char, &grid[x + y * cols],
                  x * VGA_FONT_WIDTH + frame_width, y * VGA_FONT_HEIGHT + frame_height);
    plot_char_mem(new_char, c,
                  x * VGA_FONT_WIDTH + frame_width, y * VGA_FONT_HEIGHT + frame_height);

    if (!double_buffer_enabled) {
        for (int i = 0; i < VGA_FONT_HEIGHT; i++) {
            for (int j = 0; j < VGA_FONT_WIDTH; j++) {
                if (old_char[i * VGA_FONT_WIDTH + j] != new_char[i * VGA_FONT_WIDTH + j])
                    gterm_plot_px(x * VGA_FONT_WIDTH + frame_width + j,
                                  y * VGA_FONT_HEIGHT + frame_height + i,
                                  new_char[i * VGA_FONT_WIDTH + j]);
            }
        }
    }

    grid[x + y * cols] = *c;
}

static void clear_cursor(void) {
    struct gterm_char c = grid[cursor_x + cursor_y * cols];
    c.fg = 9;
    c.bg = 8;
    plot_char_grid(&c, cursor_x, cursor_y);
}

static void draw_cursor(void) {
    if (cursor_status) {
        struct gterm_char c = grid[cursor_x + cursor_y * cols];
        c.fg = 0;
        c.bg = 7;
        plot_char_grid(&c, cursor_x, cursor_y);
    }
}

static inline bool compare_char(struct gterm_char *a, struct gterm_char *b) {
    return !(a->c != b->c || a->bg != b->bg || a->fg != b->fg);
}

static void scroll(void) {
    clear_cursor();

    for (int i = cols; i < rows * cols; i++) {
        if (!compare_char(&grid[i], &grid[i - cols]))
            plot_char_grid(&grid[i], (i - cols) % cols, (i - cols) / cols);
    }

    // Clear the last line of the screen.
    struct gterm_char empty;
    empty.c  = ' ';
    empty.fg = 9;
    empty.bg = 8;
    for (int i = rows * cols - cols; i < rows * cols; i++) {
        if (!compare_char(&grid[i], &empty))
            plot_char_grid(&empty, i % cols, i / cols);
    }

    draw_cursor();
}

void gterm_clear(bool move) {
    clear_cursor();

    struct gterm_char empty;
    empty.c  = ' ';
    empty.fg = 9;
    empty.bg = 8;
    for (int i = 0; i < rows * cols; i++) {
        plot_char_grid(&empty, i % cols, i / cols);
    }

    if (move) {
        cursor_x = 0;
        cursor_y = 0;
    }

    draw_cursor();
}

void gterm_enable_cursor(void) {
    cursor_status = true;
    draw_cursor();
}

void gterm_disable_cursor(void) {
    clear_cursor();
    cursor_status = false;
}

void gterm_set_cursor_pos(int x, int y) {
    clear_cursor();
    cursor_x = x;
    cursor_y = y;
    draw_cursor();
}

void gterm_get_cursor_pos(int *x, int *y) {
    *x = cursor_x;
    *y = cursor_y;
}

static int text_fg = 9, text_bg = 8;

void gterm_set_text_fg(int fg) {
    text_fg = fg;
}

void gterm_set_text_bg(int bg) {
    text_bg = bg;
}

void gterm_double_buffer_flush(void) {
    for (size_t i = 0; i < (size_t)rows * cols; i++) {
        if (!memcmp(&grid[i], &front_grid[i], sizeof(struct gterm_char)))
            continue;

        front_grid[i] = grid[i];

        int x = i % cols;
        int y = i / cols;

        gterm_plot_char(&grid[i], x * VGA_FONT_WIDTH + frame_width,
                                y * VGA_FONT_HEIGHT + frame_height);
    }
}

void gterm_double_buffer(bool state) {
    if (state) {
        memcpy(front_grid, grid, rows * cols * sizeof(struct gterm_char));
        double_buffer_enabled = true;
        gterm_clear(true);
        gterm_double_buffer_flush();
    } else {
        bool pcs = cursor_status;
        cursor_status = false;
        gterm_clear(true);
        gterm_double_buffer_flush();
        cursor_status = pcs;
        draw_cursor();
        double_buffer_enabled = false;
    }
}

void gterm_putchar(uint8_t c) {
    switch (c) {
        case '\b':
            if (cursor_x || cursor_y) {
                clear_cursor();
                if (cursor_x) {
                    cursor_x--;
                } else {
                    cursor_y--;
                    cursor_x = cols - 1;
                }
                draw_cursor();
            }
            break;
        case '\r':
            gterm_set_cursor_pos(0, cursor_y);
            break;
        case '\n':
            if (cursor_y == (rows - 1)) {
                gterm_set_cursor_pos(0, rows - 1);
                scroll();
            } else {
                gterm_set_cursor_pos(0, cursor_y + 1);
            }
            break;
        default: {
            clear_cursor();
            struct gterm_char ch;
            ch.c  = c;
            ch.fg = text_fg;
            ch.bg = text_bg;
            plot_char_grid(&ch, cursor_x++, cursor_y);
            if (cursor_x == cols) {
                cursor_x = 0;
                cursor_y++;
            }
            if (cursor_y == rows) {
                cursor_y--;
                scroll();
            }
            draw_cursor();
            break;
        }
    }
}

bool gterm_init(int *_rows, int *_cols, uint32_t *_colours, int _margin, int _margin_gradient, struct image *_background) {
    int req_width = 0, req_height = 0, req_bpp = 0;

    char *menu_resolution = config_get_value(NULL, 0, "MENU_RESOLUTION");
    if (menu_resolution != NULL)
        parse_resolution(&req_width, &req_height, &req_bpp, menu_resolution);

    // We force bpp to 32
    req_bpp = 32;

    fb_init(&fbinfo, req_width, req_height, req_bpp);

    // Ensure this is xRGB8888, we only support that for the menu
    if (fbinfo.red_mask_size    != 8
     || fbinfo.red_mask_shift   != 16
     || fbinfo.green_mask_size  != 8
     || fbinfo.green_mask_shift != 8
     || fbinfo.blue_mask_size   != 8
     || fbinfo.blue_mask_shift  != 0)
        return false;

    gterm_framebuffer = (void *)(uintptr_t)fbinfo.framebuffer_addr;
    gterm_width       = fbinfo.framebuffer_width;
    gterm_height      = fbinfo.framebuffer_height;
    gterm_bpp         = fbinfo.framebuffer_bpp;
    gterm_pitch       = fbinfo.framebuffer_pitch;

    mtrr_set_range((uint64_t)(size_t)gterm_framebuffer,
                   (uint64_t)gterm_pitch * gterm_height, MTRR_MEMORY_TYPE_WC);

    if (vga_font == NULL)
        vga_font = ext_mem_alloc(VGA_FONT_MAX);

    memcpy(vga_font, (void *)_binary_font_bin_start, VGA_FONT_MAX);

    char *menu_font = config_get_value(NULL, 0, "MENU_FONT");
    if (menu_font != NULL) {
        struct file_handle f;
        if (!uri_open(&f, menu_font)) {
            print("menu: Could not open font file.\n");
        } else {
            fread(&f, vga_font, 0, VGA_FONT_MAX);
        }
    }

    *_cols = cols = (gterm_width - _margin * 2) / VGA_FONT_WIDTH;
    *_rows = rows = (gterm_height - _margin * 2) / VGA_FONT_HEIGHT;
    if (grid == NULL)
        grid = ext_mem_alloc(rows * cols * sizeof(struct gterm_char));
    if (front_grid == NULL)
        front_grid = ext_mem_alloc(rows * cols * sizeof(struct gterm_char));
    background = _background;

    memcpy(ansi_colours, _colours, sizeof(ansi_colours));

    margin_gradient = _margin_gradient;

    frame_height = gterm_height / 2 - (VGA_FONT_HEIGHT * rows) / 2;
    frame_width  = gterm_width  / 2 - (VGA_FONT_WIDTH  * cols) / 2;

    if (bg_canvas == NULL)
        bg_canvas = ext_mem_alloc(gterm_width * gterm_height * sizeof(uint32_t));

    gterm_generate_canvas();
    gterm_clear(true);

    return true;
}
