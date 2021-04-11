#include <stdint.h>
#include <stddef.h>
#include <lib/gterm.h>
#include <lib/term.h>
#include <lib/blib.h>
#include <lib/libc.h>
#include <lib/config.h>
#include <lib/print.h>
#include <lib/uri.h>
#include <lib/fb.h>
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

static size_t last_grid_size = 0;
static struct gterm_char *grid = NULL;
static size_t last_front_grid_size = 0;
static struct gterm_char *front_grid = NULL;

static size_t last_bg_canvas_size = 0;
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

bool gterm_init(int *_rows, int *_cols, int width, int height) {
    if (current_video_mode >= 0
     && fbinfo.default_res == true
     && width == 0
     && height == 0
     && fbinfo.framebuffer_bpp == 32
     && !early_term) {
        *_rows = rows;
        *_cols = cols;
        gterm_clear(true);
        return true;
    }

    if (current_video_mode >= 0
     && fbinfo.framebuffer_width == width
     && fbinfo.framebuffer_height == height
     && fbinfo.framebuffer_bpp == 32
     && !early_term) {
        *_rows = rows;
        *_cols = cols;
        gterm_clear(true);
        return true;
    }

    early_term = false;

    // We force bpp to 32
    if (!fb_init(&fbinfo, width, height, 32))
        return false;

    // default scheme
    int margin = 64;
    margin_gradient = 20;

    ansi_colours[0] = 0x00000000; // black
    ansi_colours[1] = 0x00aa0000; // red
    ansi_colours[2] = 0x0000aa00; // green
    ansi_colours[3] = 0x00aa5500; // brown
    ansi_colours[4] = 0x000000aa; // blue
    ansi_colours[5] = 0x00aa00aa; // magenta
    ansi_colours[6] = 0x0000aaaa; // cyan
    ansi_colours[7] = 0x00aaaaaa; // grey
    ansi_colours[8] = 0x00000000; // background (black)
    ansi_colours[9] = 0x00aaaaaa; // foreground (grey)

    char *colours = config_get_value(NULL, 0, "THEME_COLOURS");
    if (colours == NULL)
        colours = config_get_value(NULL, 0, "THEME_COLORS");
    if (colours != NULL) {
        const char *first = colours;
        int i;
        for (i = 0; i < 10; i++) {
            const char *last;
            uint32_t col = strtoui(first, &last, 16);
            if (first == last)
                break;
            ansi_colours[i] = col;
            if (*last == 0)
                break;
            first = last + 1;
        }
        if (i < 8) {
            ansi_colours[8] = ansi_colours[0];
            ansi_colours[9] = ansi_colours[7];
        }
    }

    char *theme_background = config_get_value(NULL, 0, "THEME_BACKGROUND");
    if (theme_background != NULL) {
        ansi_colours[8] = strtoui(theme_background, NULL, 16);
    }

    char *theme_foreground = config_get_value(NULL, 0, "THEME_FOREGROUND");
    if (theme_foreground != NULL) {
        ansi_colours[9] = strtoui(theme_foreground, NULL, 16);
    }

    char *theme_margin = config_get_value(NULL, 0, "THEME_MARGIN");
    if (theme_margin != NULL) {
        margin = strtoui(theme_margin, NULL, 10);
    }

    char *theme_margin_gradient = config_get_value(NULL, 0, "THEME_MARGIN_GRADIENT");
    if (theme_margin_gradient != NULL) {
        margin_gradient = strtoui(theme_margin_gradient, NULL, 10);
    }

    char *background_path = config_get_value(NULL, 0, "BACKGROUND_PATH");
    if (background_path != NULL) {
        struct file_handle *bg_file = ext_mem_alloc(sizeof(struct file_handle));
        if (uri_open(bg_file, background_path)) {
            background = ext_mem_alloc(sizeof(struct image));
            if (open_image(background, bg_file)) {
                background = NULL;
            }
        }
    }

    if (background != NULL) {
        char *background_layout = config_get_value(NULL, 0, "BACKGROUND_STYLE");
        if (background_layout != NULL && strcmp(background_layout, "centered") == 0) {
            char *background_colour = config_get_value(NULL, 0, "BACKDROP_COLOUR");
            if (background_colour == NULL)
                background_colour = config_get_value(NULL, 0, "BACKDROP_COLOR");
            if (background_colour == NULL)
                background_colour = "0";
            uint32_t bg_col = strtoui(background_colour, NULL, 16);
            image_make_centered(background, fbinfo.framebuffer_width, fbinfo.framebuffer_height, bg_col);
        }
    }

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

    if (vga_font == NULL)
        vga_font = ext_mem_alloc(VGA_FONT_MAX);

    memcpy(vga_font, (void *)_binary_font_bin_start, VGA_FONT_MAX);

    char *menu_font = config_get_value(NULL, 0, "MENU_FONT");
    if (menu_font == NULL)
        menu_font = config_get_value(NULL, 0, "TERMINAL_FONT");
    if (menu_font != NULL) {
        struct file_handle f;
        if (!uri_open(&f, menu_font)) {
            print("menu: Could not open font file.\n");
        } else {
            fread(&f, vga_font, 0, VGA_FONT_MAX);
        }
    }

    *_cols = cols = (gterm_width - margin * 2) / VGA_FONT_WIDTH;
    *_rows = rows = (gterm_height - margin * 2) / VGA_FONT_HEIGHT;

    size_t new_grid_size = rows * cols * sizeof(struct gterm_char);
    if (new_grid_size > last_grid_size) {
        grid = ext_mem_alloc(new_grid_size);
        last_grid_size = new_grid_size;
    }

    size_t new_front_grid_size = rows * cols * sizeof(struct gterm_char);
    if (new_front_grid_size > last_front_grid_size) {
        front_grid = ext_mem_alloc(new_front_grid_size);
        last_front_grid_size = new_front_grid_size;
    }

    frame_height = gterm_height / 2 - (VGA_FONT_HEIGHT * rows) / 2;
    frame_width  = gterm_width  / 2 - (VGA_FONT_WIDTH  * cols) / 2;

    size_t new_bg_canvas_size = gterm_width * gterm_height * sizeof(uint32_t);
    if (new_bg_canvas_size > last_bg_canvas_size) {
        bg_canvas = ext_mem_alloc(new_bg_canvas_size);
        last_bg_canvas_size = new_bg_canvas_size;
    }

    gterm_generate_canvas();
    gterm_clear(true);

    return true;
}
