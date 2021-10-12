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

// Maximum allowed font size in bytes. 16kB should be enough as 9x32 is the
// largest font I've seen, and that would take 9*32 * 256 * 1/8 byte =
// 9216 bytes.
#define VGA_FONT_MAX 16384
#define VGA_FONT_GLYPHS 256

#define DEFAULT_FONT_WIDTH 8
#define DEFAULT_FONT_HEIGHT 16

static size_t last_vga_font_bool = 0;
static size_t vga_font_width;
static size_t vga_font_height;
static size_t glyph_width = 8;
static size_t glyph_height = 16;

static size_t vga_font_scale_x = 1;
static size_t vga_font_scale_y = 1;

struct fb_info fbinfo;
static volatile uint32_t *gterm_framebuffer;
static uint16_t  gterm_pitch;
static uint16_t  gterm_width;
static uint16_t  gterm_height;
static uint16_t  gterm_bpp;

extern symbol _binary_font_bin_start;

static uint8_t *vga_font_bits = NULL;
static bool *vga_font_bool = NULL;

static uint32_t ansi_colours[8];
static uint32_t ansi_bright_colours[8];
static uint32_t default_fg, default_bg;

static struct image *background;

static size_t last_bg_canvas_size = 0;
static uint32_t *bg_canvas = NULL;

static size_t rows;
static size_t cols;
static size_t margin;
static size_t margin_gradient;

static size_t last_grid_size = 0;
static size_t last_queue_size = 0;
static size_t last_map_size = 0;

struct gterm_char {
    uint32_t c;
    uint32_t fg;
    uint32_t bg;
};

static struct gterm_char *grid = NULL;

struct queue_item {
    size_t x, y;
    struct gterm_char c;
};

static struct queue_item *queue = NULL;
static size_t queue_i = 0;

static struct queue_item **map = NULL;

static struct context {
    uint32_t text_fg;
#define text_fg context.text_fg
    uint32_t text_bg;
#define text_bg context.text_bg
    bool cursor_status;
#define cursor_status context.cursor_status
    size_t cursor_x;
#define cursor_x context.cursor_x
    size_t cursor_y;
#define cursor_y context.cursor_y
} context;

static size_t old_cursor_x = 0;
static size_t old_cursor_y = 0;

void gterm_swap_palette(void) {
    uint32_t tmp = text_bg;
    text_bg = text_fg;
    if (tmp == 0xffffffff) {
        text_fg = default_bg;
    } else {
        text_fg = tmp;
    }
}

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

static inline void gterm_plot_px(size_t x, size_t y, uint32_t hex) {
    if (x >= gterm_width || y >= gterm_height) {
        return;
    }

    size_t fb_i = x + (gterm_pitch / sizeof(uint32_t)) * y;

    gterm_framebuffer[fb_i] = hex;
}

static uint32_t blend_gradient_from_box(size_t x, size_t y, uint32_t bg_px, uint32_t hex) {
    size_t distance, x_distance, y_distance;
    size_t gradient_stop_x = gterm_width - margin;
    size_t gradient_stop_y = gterm_height - margin;

    if (x < margin)
        x_distance = margin - x;
    else
        x_distance = x - gradient_stop_x;

    if (y < margin)
        y_distance = margin - y;
    else
        y_distance = y - gradient_stop_y;

    if (x >= margin && x < gradient_stop_x) {
        distance = y_distance;
    } else if (y >= margin && y < gradient_stop_y) {
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

typedef size_t fixedp6; // the last 6 bits are the fixed point part
static size_t fixedp6_to_int(fixedp6 value) { return value / 64; }
static fixedp6 int_to_fixedp6(size_t value) { return value * 64; }

// Draw rect at coordinates, copying from the image to the fb and canvas, applying fn on every pixel
__attribute__((always_inline)) static inline void genloop(size_t xstart, size_t xend, size_t ystart, size_t yend, uint32_t (*blend)(size_t x, size_t y, uint32_t orig)) {
    uint8_t *img = background->img;
    const size_t img_width = background->img_width, img_height = background->img_height, img_pitch = background->pitch, colsize = background->bpp / 8;

    switch (background->type) {
    case IMAGE_TILED:
        for (size_t y = ystart; y < yend; y++) {
            size_t image_y = y % img_height, image_x = xstart % img_width;
            const size_t off = img_pitch * (img_height - 1 - image_y);
            size_t canvas_off = gterm_width * y, fb_off = gterm_pitch / 4 * y;
            for (size_t x = xstart; x < xend; x++) {
                uint32_t img_pixel = *(uint32_t*)(img + image_x * colsize + off);
                uint32_t i = blend(x, y, img_pixel);
                bg_canvas[canvas_off + x] = i; gterm_framebuffer[fb_off + x] = i;
                if (image_x++ == img_width) image_x = 0; // image_x = x % img_width, but modulo is too expensive
            }
        }
        break;

    case IMAGE_CENTERED:
        for (size_t y = ystart; y < yend; y++) {
            size_t image_y = y - background->y_displacement;
            const size_t off = img_pitch * (img_height - 1 - image_y);
            size_t canvas_off = gterm_width * y, fb_off = gterm_pitch / 4 * y;
            if (image_y >= background->y_size) { /* external part */
                for (size_t x = xstart; x < xend; x++) {
                    uint32_t i = blend(x, y, background->back_colour);
                    bg_canvas[canvas_off + x] = i; gterm_framebuffer[fb_off + x] = i;
                }
            }
            else { /* internal part */
                for (size_t x = xstart; x < xend; x++) {
                    size_t image_x = (x - background->x_displacement);
                    bool x_external = image_x >= background->x_size;
                    uint32_t img_pixel = *(uint32_t*)(img + image_x * colsize + off);
                    uint32_t i = blend(x, y, x_external ? background->back_colour : img_pixel);
                    bg_canvas[canvas_off + x] = i; gterm_framebuffer[fb_off + x] = i;
                }
            }
        }
        break;
    // For every pixel, ratio = img_width / gterm_width, img_x = x * ratio, x = (xstart + i)
    // hence x = xstart * ratio + i * ratio
    // so you can set x = xstart * ratio, and increment by ratio at each iteration
    case IMAGE_STRETCHED:
        for (size_t y = ystart; y < yend; y++) {
            size_t img_y = (y * img_height) / gterm_height; // calculate Y with full precision
            size_t off = img_pitch * (img_height - 1 - img_y);
            size_t canvas_off = gterm_width * y, fb_off = gterm_pitch / 4 * y;

            size_t ratio = int_to_fixedp6(img_width) / gterm_width;
            fixedp6 img_x = ratio * xstart;
            for (size_t x = xstart; x < xend; x++) {
                uint32_t img_pixel = *(uint32_t*)(img + fixedp6_to_int(img_x) * colsize + off);
                uint32_t i = blend(x, y, img_pixel);
                bg_canvas[canvas_off + x] = i; gterm_framebuffer[fb_off + x] = i;
                img_x += ratio;
            }
        }
        break;
    }
}

static uint32_t blend_external(size_t x, size_t y, uint32_t orig) { (void)x; (void)y; return orig; }
static uint32_t blend_internal(size_t x, size_t y, uint32_t orig) { (void)x; (void)y; return colour_blend(default_bg, orig); }
static uint32_t blend_margin(size_t x, size_t y, uint32_t orig) { return blend_gradient_from_box(x, y, orig, default_bg); }

static void loop_external(size_t xstart, size_t xend, size_t ystart, size_t yend) { genloop(xstart, xend, ystart, yend, blend_external); }
static void loop_margin(size_t xstart, size_t xend, size_t ystart, size_t yend) { genloop(xstart, xend, ystart, yend, blend_margin); }
static void loop_internal(size_t xstart, size_t xend, size_t ystart, size_t yend) { genloop(xstart, xend, ystart, yend, blend_internal); }

static void gterm_generate_canvas(void) {
    if (background) {
        size_t margin_no_gradient = margin - margin_gradient;
        size_t scan_stop_x = gterm_width - margin_no_gradient;
        size_t scan_stop_y = gterm_height - margin_no_gradient;

        loop_external(0, gterm_width, 0, margin_no_gradient);
        loop_external(0, gterm_width, scan_stop_y, gterm_height);
        loop_external(0, margin_no_gradient, margin_no_gradient, scan_stop_y);
        loop_external(scan_stop_x, gterm_width, margin_no_gradient, scan_stop_y);

        size_t gradient_stop_x = gterm_width - margin;
        size_t gradient_stop_y = gterm_height - margin;

        if (margin_gradient) {
            loop_margin(margin_no_gradient, scan_stop_x, margin_no_gradient, margin);
            loop_margin(margin_no_gradient, scan_stop_x, gradient_stop_y, scan_stop_y);
            loop_margin(margin_no_gradient, margin, margin, gradient_stop_y);
            loop_margin(gradient_stop_x, scan_stop_x, margin, gradient_stop_y);
        }

        loop_internal(margin, gradient_stop_x, margin, gradient_stop_y);
    } else {
        for (size_t y = 0; y < gterm_height; y++) {
            for (size_t x = 0; x < gterm_width; x++) {
                bg_canvas[y * gterm_width + x] = default_bg;
                gterm_plot_px(x, y, default_bg);
            }
        }
    }
}

static void plot_char(struct gterm_char *c, size_t x, size_t y) {
    if (x >= cols || y >= rows) {
        return;
    }

    x = margin + x * glyph_width;
    y = margin + y * glyph_height;

    bool *glyph = &vga_font_bool[c->c * vga_font_height * vga_font_width];
    // naming: fx,fy for font coordinates, gx,gy for glyph coordinates
    for (size_t gy = 0; gy < glyph_height; gy++) {
        uint8_t fy = gy / vga_font_scale_y;
        volatile uint32_t *fb_line = gterm_framebuffer + x + (y + gy) * (gterm_pitch / 4);
        uint32_t *canvas_line = bg_canvas + x + (y + gy) * gterm_width;
        for (size_t fx = 0; fx < vga_font_width; fx++) {
            bool draw = glyph[fy * vga_font_width + fx];
            for (size_t i = 0; i < vga_font_scale_x; i++) {
                size_t gx = vga_font_scale_x * fx + i;
                uint32_t bg = c->bg == 0xffffffff ? canvas_line[gx] : c->bg;
                uint32_t fg = c->fg == 0xffffffff ? canvas_line[gx] : c->fg;
                fb_line[gx] = draw ? fg : bg;
            }
        }
    }
}

static size_t plot_from_queue(struct queue_item *qu, size_t max) {
    for (size_t gy = 0; ; gy++) {
        size_t y = margin + qu->y * glyph_height;
        size_t fy = (gy / vga_font_scale_y) * vga_font_width;
        volatile uint32_t *fb_line = gterm_framebuffer + (y + gy) * (gterm_pitch / 4);
        uint32_t *canvas_line = bg_canvas + (y + gy) * gterm_width;
        for (size_t qi = 0; ; qi++) {
            struct queue_item *q = &qu[qi];
            if (qi != 0 && q->y != qu[qi - 1].y) {
                if (gy == glyph_height - 1) {
                    return qi;
                } else {
                    // break to next line
                    break;
                }
            }
            size_t offset = q->y * cols + q->x;
            if (map[offset] == NULL) {
                goto epilogue;
            }
            size_t x = margin + q->x * glyph_width;
            struct gterm_char *old = &grid[offset];
            bool *new_glyph = &vga_font_bool[q->c.c * vga_font_height * vga_font_width];
            bool *old_glyph = &vga_font_bool[old->c * vga_font_height * vga_font_width];
            bool same_palette = q->c.fg == old->fg && q->c.bg == old->bg;
            for (size_t fx = 0; fx < vga_font_width; fx++) {
                bool old_draw = old_glyph[fy + fx];
                bool new_draw = new_glyph[fy + fx];
                if (old_draw == new_draw && same_palette) {
                    continue;
                }
                for (size_t i = 0; i < vga_font_scale_x; i++) {
                    size_t gx = x + vga_font_scale_x * fx + i;
                    uint32_t bg = q->c.bg == 0xffffffff ? canvas_line[gx] : q->c.bg;
                    uint32_t fg = q->c.fg == 0xffffffff ? canvas_line[gx] : q->c.fg;
                    fb_line[gx] = new_draw ? fg : bg;
                }
            }
            if (gy == glyph_height - 1) {
                grid[offset] = q->c;
                map[offset] = NULL;
            }
epilogue:
            if (qi == max - 1) {
                if (gy == glyph_height - 1) {
                    return max;
                } else {
                    break;
                }
            }
        }
    }
}

static inline bool compare_char(struct gterm_char *a, struct gterm_char *b) {
    return !(a->c != b->c || a->bg != b->bg || a->fg != b->fg);
}

static void push_to_queue(struct gterm_char *c, size_t x, size_t y) {
    if (x >= cols || y >= rows) {
        return;
    }

    size_t i = y * cols + x;

    struct queue_item *q = map[i];

    if (q == NULL) {
        if (compare_char(&grid[i], c)) {
            return;
        }
        q = &queue[queue_i++];
        q->x = x;
        q->y = y;
        map[i] = q;
    }

    q->c = *c;
}

static bool scroll_enabled = true;

bool gterm_scroll_disable(void) {
    bool ret = scroll_enabled;
    scroll_enabled = false;
    return ret;
}

void gterm_scroll_enable(void) {
    scroll_enabled = true;
}

void gterm_scroll(void) {
    for (size_t i = (term_context.scroll_top_margin + 1) * cols;
         i < term_context.scroll_bottom_margin * cols; i++) {
        struct gterm_char *c;
        struct queue_item *q = map[i];
        if (q != NULL) {
            c = &q->c;
        } else {
            c = &grid[i];
        }
        push_to_queue(c, (i - cols) % cols, (i - cols) / cols);
    }

    // Clear the last line of the screen.
    struct gterm_char empty;
    empty.c  = ' ';
    empty.fg = text_fg;
    empty.bg = text_bg;
    for (size_t i = (term_context.scroll_bottom_margin - 1) * cols;
         i < term_context.scroll_bottom_margin * cols; i++) {
        push_to_queue(&empty, i % cols, i / cols);
    }
}

void gterm_clear(bool move) {
    struct gterm_char empty;
    empty.c  = ' ';
    empty.fg = text_fg;
    empty.bg = text_bg;
    for (size_t i = 0; i < rows * cols; i++) {
        push_to_queue(&empty, i % cols, i / cols);
    }

    if (move) {
        cursor_x = 0;
        cursor_y = 0;
    }
}

void gterm_enable_cursor(void) {
    cursor_status = true;
}

bool gterm_disable_cursor(void) {
    bool ret = cursor_status;
    cursor_status = false;
    return ret;
}

void gterm_set_cursor_pos(size_t x, size_t y) {
    if (x >= cols) {
        if ((int)x < 0) {
            x = 0;
        } else {
            x = cols - 1;
        }
    }
    if (y >= rows) {
        if ((int)y < 0) {
            y = 0;
        } else {
            y = rows - 1;
        }
    }
    cursor_x = x;
    cursor_y = y;
}

void gterm_get_cursor_pos(size_t *x, size_t *y) {
    *x = cursor_x;
    *y = cursor_y;
}

void gterm_move_character(size_t new_x, size_t new_y, size_t old_x, size_t old_y) {
    if (old_x >= cols || old_y >= rows
     || new_x >= cols || new_y >= rows) {
        return;
    }

    size_t i = old_x + old_y * cols;

    struct gterm_char *c;
    struct queue_item *q = map[i];
    if (q != NULL) {
        c = &q->c;
    } else {
        c = &grid[i];
    }

    push_to_queue(c, new_x, new_y);
}

void gterm_set_text_fg(size_t fg) {
    text_fg = ansi_colours[fg];
}

void gterm_set_text_bg(size_t bg) {
    text_bg = ansi_colours[bg];
}

void gterm_set_text_fg_bright(size_t fg) {
    text_fg = ansi_bright_colours[fg];
}

void gterm_set_text_bg_bright(size_t bg) {
    text_bg = ansi_bright_colours[bg];
}

void gterm_set_text_fg_default(void) {
    text_fg = default_fg;
}

void gterm_set_text_bg_default(void) {
    text_bg = 0xffffffff;
}

static void draw_cursor(void) {
    size_t i = cursor_x + cursor_y * cols;
    struct gterm_char c;
    struct queue_item *q = map[i];
    if (q != NULL) {
        c = q->c;
    } else {
        c = grid[i];
    }
    uint32_t tmp = c.fg;
    c.fg = c.bg;
    c.bg = tmp;
    plot_char(&c, cursor_x, cursor_y);
    if (q != NULL) {
        grid[i] = q->c;
        map[i] = NULL;
    }
}

void gterm_double_buffer_flush(void) {
    if (cursor_status) {
        draw_cursor();
    }

    for (size_t i = 0; i < queue_i; ) {
        i += plot_from_queue(&queue[i], queue_i - i);
    }

    if (old_cursor_x != cursor_x || old_cursor_y != cursor_y) {
        plot_char(&grid[old_cursor_x + old_cursor_y * cols], old_cursor_x, old_cursor_y);
    }

    old_cursor_x = cursor_x;
    old_cursor_y = cursor_y;

    queue_i = 0;
}

void gterm_putchar(uint8_t c) {
    struct gterm_char ch;
    ch.c  = c;
    ch.fg = text_fg;
    ch.bg = text_bg;
    push_to_queue(&ch, cursor_x++, cursor_y);
    if (cursor_x == cols && (cursor_y < term_context.scroll_bottom_margin - 1 || scroll_enabled)) {
        cursor_x = 0;
        cursor_y++;
    }
    if (cursor_y == term_context.scroll_bottom_margin) {
        cursor_y--;
        gterm_scroll();
    }
}

bool gterm_init(size_t *_rows, size_t *_cols, size_t width, size_t height) {
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

    cursor_status = true;

    // default scheme
    margin = 64;
    margin_gradient = 4;

    default_bg = 0x00000000; // background (black)
    default_fg = 0x00aaaaaa; // foreground (grey)

    ansi_colours[0] = 0x00000000; // black
    ansi_colours[1] = 0x00aa0000; // red
    ansi_colours[2] = 0x0000aa00; // green
    ansi_colours[3] = 0x00aa5500; // brown
    ansi_colours[4] = 0x000000aa; // blue
    ansi_colours[5] = 0x00aa00aa; // magenta
    ansi_colours[6] = 0x0000aaaa; // cyan
    ansi_colours[7] = 0x00aaaaaa; // grey

    char *colours = config_get_value(NULL, 0, "THEME_COLOURS");
    if (colours == NULL)
        colours = config_get_value(NULL, 0, "THEME_COLORS");
    if (colours != NULL) {
        const char *first = colours;
        size_t i;
        for (i = 0; i < 10; i++) {
            const char *last;
            uint32_t col = strtoui(first, &last, 16);
            if (first == last)
                break;
            if (i < 8) {
                ansi_colours[i] = col & 0xffffff;
            } else if (i == 8) {
                default_bg = col;
            } else if (i == 9) {
                default_fg = col & 0xffffff;
            }
            if (*last == 0)
                break;
            first = last + 1;
        }
    }

    ansi_bright_colours[0] = 0x00555555; // black
    ansi_bright_colours[1] = 0x00ff5555; // red
    ansi_bright_colours[2] = 0x0055ff55; // green
    ansi_bright_colours[3] = 0x00ffff55; // brown
    ansi_bright_colours[4] = 0x005555ff; // blue
    ansi_bright_colours[5] = 0x00ff55ff; // magenta
    ansi_bright_colours[6] = 0x0055ffff; // cyan
    ansi_bright_colours[7] = 0x00ffffff; // grey

    char *bright_colours = config_get_value(NULL, 0, "THEME_BRIGHT_COLOURS");
    if (bright_colours == NULL)
        bright_colours = config_get_value(NULL, 0, "THEME_BRIGHT_COLORS");
    if (bright_colours != NULL) {
        const char *first = bright_colours;
        size_t i;
        for (i = 0; i < 8; i++) {
            const char *last;
            uint32_t col = strtoui(first, &last, 16);
            if (first == last)
                break;
            ansi_bright_colours[i] = col & 0xffffff;
            if (*last == 0)
                break;
            first = last + 1;
        }
    }

    char *theme_background = config_get_value(NULL, 0, "THEME_BACKGROUND");
    if (theme_background != NULL) {
        default_bg = strtoui(theme_background, NULL, 16);
    }

    char *theme_foreground = config_get_value(NULL, 0, "THEME_FOREGROUND");
    if (theme_foreground != NULL) {
        default_fg = strtoui(theme_foreground, NULL, 16) & 0xffffff;
    }

    text_fg = default_fg;
    text_bg = 0xffffffff;

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
        } else if (background_layout != NULL && strcmp(background_layout, "stretched") == 0) {
            image_make_stretched(background, fbinfo.framebuffer_width, fbinfo.framebuffer_height);
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

    vga_font_width = DEFAULT_FONT_WIDTH, vga_font_height = DEFAULT_FONT_HEIGHT;
    size_t font_bytes = (vga_font_width * vga_font_height * VGA_FONT_GLYPHS) / 8;

    if (vga_font_bits == NULL) {
        vga_font_bits = ext_mem_alloc(VGA_FONT_MAX);
    }

    memcpy(vga_font_bits, (void *)_binary_font_bin_start, VGA_FONT_MAX);

    size_t tmp_font_width, tmp_font_height;

    char *menu_font_size = config_get_value(NULL, 0, "MENU_FONT_SIZE");
    if (menu_font_size == NULL)
        menu_font_size = config_get_value(NULL, 0, "TERMINAL_FONT_SIZE");
    if (menu_font_size != NULL) {
        parse_resolution(&tmp_font_width, &tmp_font_height, NULL, menu_font_size);

        size_t tmp_font_bytes = (tmp_font_width * tmp_font_height * VGA_FONT_GLYPHS) / 8;

        if (tmp_font_bytes > VGA_FONT_MAX) {
            print("Font would be too large (%u bytes, %u bytes allowed). Not loading.\n", tmp_font_bytes, VGA_FONT_MAX);
            goto no_load_font;
        }

        font_bytes = tmp_font_bytes;
    }

    char *menu_font = config_get_value(NULL, 0, "MENU_FONT");
    if (menu_font == NULL)
        menu_font = config_get_value(NULL, 0, "TERMINAL_FONT");
    if (menu_font != NULL) {
        struct file_handle f;
        if (!uri_open(&f, menu_font)) {
            print("menu: Could not open font file.\n");
        } else {
            if (fread(&f, vga_font_bits, 0, font_bytes) == 0) {
                if (menu_font_size != NULL) {
                    vga_font_width = tmp_font_width;
                    vga_font_height = tmp_font_height;
                }
            }
        }
    }

no_load_font:;
    size_t font_spacing = 1;
    char *font_spacing_str = config_get_value(NULL, 0, "MENU_FONT_SPACING");
    if (font_spacing_str == NULL)
        font_spacing_str = config_get_value(NULL, 0, "TERMINAL_FONT_SPACING");
    if (font_spacing_str != NULL) {
        font_spacing = strtoui(font_spacing_str, NULL, 10);
    }

    vga_font_width += font_spacing;

    size_t this_vga_font_bool = VGA_FONT_GLYPHS * vga_font_height * vga_font_width * sizeof(bool);
    if (last_vga_font_bool < this_vga_font_bool) {
        vga_font_bool = ext_mem_alloc(this_vga_font_bool);
        last_vga_font_bool = this_vga_font_bool;
    }

    for (size_t i = 0; i < VGA_FONT_GLYPHS; i++) {
        uint8_t *glyph = &vga_font_bits[i * vga_font_height];

        for (size_t y = 0; y < vga_font_height; y++) {
            // NOTE: the characters in VGA fonts are always one byte wide.
            // 9 dot wide fonts have 8 dots and one empty column, except
            // characters 0xC0-0xDF replicate column 9.
            for (size_t x = 0; x < 8; x++) {
                size_t offset = i * vga_font_height * vga_font_width + y * vga_font_width + x;

                if ((glyph[y] & (0x80 >> x))) {
                    vga_font_bool[offset] = true;
                } else {
                    vga_font_bool[offset] = false;
                }
            }
            // fill columns above 8 like VGA Line Graphics Mode does
            for (size_t x = 8; x < vga_font_width; x++) {
                size_t offset = i * vga_font_height * vga_font_width + y *  vga_font_width + x;

                if (i >= 0xC0 && i <= 0xDF) {
                    vga_font_bool[offset] = (glyph[y] & 1);
                } else {
                    vga_font_bool[offset] = false;
                }
            }
        }
    }

    vga_font_scale_x = 1;
    vga_font_scale_y = 1;

    char *menu_font_scale = config_get_value(NULL, 0, "MENU_FONT_SCALE");
    if (menu_font_scale == NULL) {
        menu_font_scale = config_get_value(NULL, 0, "TERMINAL_FONT_SCALE");
    }
    if (menu_font_scale != NULL) {
        parse_resolution(&vga_font_scale_x, &vga_font_scale_y, NULL, menu_font_scale);
        if (vga_font_scale_x > 8 || vga_font_scale_y > 8) {
            vga_font_scale_x = 1;
            vga_font_scale_y = 1;
        }
    }

    glyph_width = vga_font_width * vga_font_scale_x;
    glyph_height = vga_font_height * vga_font_scale_y;

    *_cols = cols = (gterm_width - margin * 2) / glyph_width;
    *_rows = rows = (gterm_height - margin * 2) / glyph_height;

    size_t new_grid_size = rows * cols * sizeof(struct gterm_char);
    if (new_grid_size > last_grid_size) {
        grid = ext_mem_alloc(new_grid_size);
        last_grid_size = new_grid_size;
    } else {
        memset(grid, 0, new_grid_size);
    }

    size_t new_queue_size = rows * cols * sizeof(struct queue_item);
    if (new_queue_size > last_queue_size) {
        queue = ext_mem_alloc(new_queue_size);
        last_queue_size = new_queue_size;
    }
    queue_i = 0;

    size_t new_map_size = rows * cols * sizeof(struct queue_item *);
    if (new_map_size > last_map_size) {
        map = ext_mem_alloc(new_map_size);
        last_map_size = new_map_size;
    } else {
        memset(map, 0, new_map_size);
    }

    size_t new_bg_canvas_size = gterm_width * gterm_height * sizeof(uint32_t);
    if (new_bg_canvas_size > last_bg_canvas_size) {
        bg_canvas = ext_mem_alloc(new_bg_canvas_size);
        last_bg_canvas_size = new_bg_canvas_size;
    }

    gterm_generate_canvas();
    gterm_clear(true);
    gterm_double_buffer_flush();

    return true;
}

uint64_t gterm_context_size(void) {
    uint64_t ret = 0;

    ret += sizeof(struct context);
    ret += last_grid_size;

    return ret;
}

void gterm_context_save(uint64_t ptr) {
    memcpy32to64(ptr, (uint64_t)(uintptr_t)&context, sizeof(struct context));
    ptr += sizeof(struct context);

    memcpy32to64(ptr, (uint64_t)(uintptr_t)grid, last_grid_size);
}

void gterm_context_restore(uint64_t ptr) {
    memcpy32to64((uint64_t)(uintptr_t)&context, ptr, sizeof(struct context));
    ptr += sizeof(struct context);

    memcpy32to64((uint64_t)(uintptr_t)grid, ptr, last_grid_size);

    for (size_t i = 0; i < (size_t)rows * cols; i++) {
        size_t x = i % cols;
        size_t y = i / cols;

        plot_char(&grid[i], x, y);
    }

    if (cursor_status) {
        draw_cursor();
    }
}

void gterm_full_refresh(void) {
    gterm_generate_canvas();

    for (size_t i = 0; i < (size_t)rows * cols; i++) {
        size_t x = i % cols;
        size_t y = i / cols;

        plot_char(&grid[i], x, y);
    }

    if (cursor_status) {
        draw_cursor();
    }
}
