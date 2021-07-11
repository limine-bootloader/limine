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

static uint8_t *vga_font_bits = NULL;
static bool *vga_font_bool = NULL;

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

static uint32_t blend_gradient_from_box(int x, int y, uint32_t bg_px, uint32_t hex) {
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

typedef int fixedp6; // the last 6 bits are the fixed point part
static int fixedp6_to_int(fixedp6 value) { return value / 64; }
static fixedp6 int_to_fixedp6(int value) { return value * 64; }

// Draw rect at coordinates, copying from the image to the fb and canvas, applying fn on every pixel
__attribute__((always_inline)) static inline void genloop(int xstart, int xend, int ystart, int yend, uint32_t (*blend)(int x, int y, uint32_t orig)) {
    uint8_t *img = background->img;
    const int img_width = background->img_width, img_height = background->img_height, img_pitch = background->pitch, colsize = background->bpp / 8;

    switch (background->type) {
    case IMAGE_TILED:
        for (int y = ystart; y < yend; y++) {
            int image_y = y % img_height, image_x = xstart % img_width;
            const size_t off = img_pitch * (img_height - 1 - image_y);
            int canvas_off = gterm_width * y, fb_off = gterm_pitch / 4 * y;
            for (int x = xstart; x < xend; x++) {
                uint32_t img_pixel = *(uint32_t*)(img + image_x * colsize + off);
                uint32_t i = blend(x, y, img_pixel);
                bg_canvas[canvas_off + x] = i; gterm_framebuffer[fb_off + x] = i;
                if (image_x++ == img_width) image_x = 0; // image_x = x % img_width, but modulo is too expensive
            }
        }
        break;

    case IMAGE_CENTERED:
        for (int y = ystart; y < yend; y++) {
            int image_y = y - background->y_displacement;
            const size_t off = img_pitch * (img_height - 1 - image_y);
            int canvas_off = gterm_width * y, fb_off = gterm_pitch / 4 * y;
            if ((image_y < 0) || (image_y >= background->y_size)) { /* external part */
                for (int x = xstart; x < xend; x++) {
                    uint32_t i = blend(x, y, background->back_colour);
                    bg_canvas[canvas_off + x] = i; gterm_framebuffer[fb_off + x] = i;
                }
            }
            else { /* internal part */
                for (int x = xstart; x < xend; x++) {
                    int image_x = (x - background->x_displacement);
                    bool x_external = (image_x < 0) || (image_x >= background->x_size);
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
        for (int y = ystart; y < yend; y++) {
            int img_y = (y * img_height) / gterm_height; // calculate Y with full precision
            int off = img_pitch * (img_height - 1 - img_y);
            int canvas_off = gterm_width * y, fb_off = gterm_pitch / 4 * y;

            size_t ratio = int_to_fixedp6(img_width) / gterm_width;
            fixedp6 img_x = ratio * xstart;
            for (int x = xstart; x < xend; x++) {
                uint32_t img_pixel = *(uint32_t*)(img + fixedp6_to_int(img_x) * colsize + off);
                uint32_t i = blend(x, y, img_pixel);
                bg_canvas[canvas_off + x] = i; gterm_framebuffer[fb_off + x] = i;
                img_x += ratio;
            }
        }
        break;
    }
}
static uint32_t blend_external(int x, int y, uint32_t orig) { (void)x; (void)y; return orig; }
static uint32_t blend_internal(int x, int y, uint32_t orig) { (void)x; (void)y; return colour_blend(ansi_colours[8], orig); }
static uint32_t blend_margin(int x, int y, uint32_t orig) { return blend_gradient_from_box(x, y, orig, ansi_colours[8]); }

static void loop_external(int xstart, int xend, int ystart, int yend) { genloop(xstart, xend, ystart, yend, blend_external); }
static void loop_margin(int xstart, int xend, int ystart, int yend) { genloop(xstart, xend, ystart, yend, blend_margin); }
static void loop_internal(int xstart, int xend, int ystart, int yend) { genloop(xstart, xend, ystart, yend, blend_internal); }

void gterm_generate_canvas(void) {
    if (background) {
        const int frame_height_end = frame_height + VGA_FONT_HEIGHT * rows, frame_width_end = frame_width + VGA_FONT_WIDTH * cols;
        const int fheight = frame_height - margin_gradient, fheight_end = frame_height_end + margin_gradient,
            fwidth = frame_width - margin_gradient, fwidth_end = frame_width_end + margin_gradient;

        loop_external(0, gterm_width, 0, fheight);
        loop_external(0, gterm_width, fheight_end, gterm_height);
        loop_external(0, fwidth, fheight, fheight_end);
        loop_external(fwidth_end, gterm_width, fheight, fheight_end);

        if (margin_gradient) {
            loop_margin(fwidth, fwidth_end, fheight, frame_height);
            loop_margin(fwidth, fwidth_end, frame_height_end, fheight_end);
            loop_margin(fwidth, frame_width, frame_height, frame_height_end);
            loop_margin(frame_width_end, fwidth_end, frame_height, frame_height_end);
        }

        loop_internal(frame_width, frame_width_end, frame_height, frame_height_end);
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

void gterm_plot_char(struct gterm_char *c, int x, int y) {
    bool *glyph = &vga_font_bool[c->c * VGA_FONT_HEIGHT * VGA_FONT_WIDTH];
    for (int i = 0; i < VGA_FONT_HEIGHT; i++) {
        uint32_t *fb_line = gterm_framebuffer + x + (y + i) * (gterm_pitch / 4);
        uint32_t *canvas_line = bg_canvas + x + (y + i) * gterm_width;
        for (int j = 0; j < VGA_FONT_WIDTH; j++) {
            bool draw = glyph[i * VGA_FONT_WIDTH + j];
            if (c->bg == 8)
                fb_line[j] = draw ? ansi_colours[c->fg] : canvas_line[j];
            else
                fb_line[j] = draw ? ansi_colours[c->fg] : ansi_colours[c->bg];
        }
    }
}

void gterm_plot_char_fast(struct gterm_char *old, struct gterm_char *c, int x, int y) {
    bool *new_glyph = &vga_font_bool[c->c * VGA_FONT_HEIGHT * VGA_FONT_WIDTH];
    bool *old_glyph = &vga_font_bool[old->c * VGA_FONT_HEIGHT * VGA_FONT_WIDTH];
    for (int i = 0; i < VGA_FONT_HEIGHT; i++) {
        uint32_t *fb_line = gterm_framebuffer + x + (y + i) * (gterm_pitch / 4);
        uint32_t *canvas_line = bg_canvas + x + (y + i) * gterm_width;
        for (int j = 0; j < VGA_FONT_WIDTH; j++) {
            bool old_draw = old_glyph[i * VGA_FONT_WIDTH + j];
            bool new_draw = new_glyph[i * VGA_FONT_WIDTH + j];
            if (old_draw == new_draw)
                continue;
            if (c->bg == 8)
                fb_line[j] = new_draw ? ansi_colours[c->fg] : canvas_line[j];
            else
                fb_line[j] = new_draw ? ansi_colours[c->fg] : ansi_colours[c->bg];
        }
    }
}

static void plot_char_grid_force(struct gterm_char *c, int x, int y) {
    gterm_plot_char(c, frame_width + x * VGA_FONT_WIDTH, frame_height + y * VGA_FONT_HEIGHT);
}

static void plot_char_grid(struct gterm_char *c, int x, int y) {
    if (!double_buffer_enabled) {
        struct gterm_char *old = &grid[x + y * cols];

        if (old->c != c->c || old->fg != c->fg || old->bg != c->bg) {
            if (old->fg == c->fg && old->bg == c->bg) {
                gterm_plot_char_fast(old, c, frame_width + x * VGA_FONT_WIDTH, frame_height + y * VGA_FONT_HEIGHT);
            } else {
                gterm_plot_char(c, frame_width + x * VGA_FONT_WIDTH, frame_height + y * VGA_FONT_HEIGHT);
            }
        }
    }

    grid[x + y * cols] = *c;
}

static void clear_cursor(void) {
    if (cursor_status) {
        struct gterm_char c = grid[cursor_x + cursor_y * cols];
        plot_char_grid_force(&c, cursor_x, cursor_y);
    }
}

static void draw_cursor(void) {
    if (cursor_status) {
        struct gterm_char c = grid[cursor_x + cursor_y * cols];
        c.fg = 0;
        c.bg = 7;
        plot_char_grid_force(&c, cursor_x, cursor_y);
    }
}

static inline bool compare_char(struct gterm_char *a, struct gterm_char *b) {
    return !(a->c != b->c || a->bg != b->bg || a->fg != b->fg);
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

bool gterm_disable_cursor(void) {
    bool ret = cursor_status;
    clear_cursor();
    cursor_status = false;
    return ret;
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

    draw_cursor();
}

void gterm_double_buffer(bool state) {
    if (state) {
        memcpy(front_grid, grid, rows * cols * sizeof(struct gterm_char));
        double_buffer_enabled = true;
        gterm_clear(true);
        gterm_double_buffer_flush();
    } else {
        gterm_clear(true);
        gterm_double_buffer_flush();
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
                if (scroll_enabled) {
                    gterm_set_cursor_pos(0, rows - 1);
                    scroll();
                }
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
            if (cursor_x == cols && (cursor_y < rows - 1 || scroll_enabled)) {
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
    margin_gradient = 4;

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

    if (vga_font_bits == NULL) {
        vga_font_bits = ext_mem_alloc(VGA_FONT_MAX);

        memcpy(vga_font_bits, (void *)_binary_font_bin_start, VGA_FONT_MAX);
    }

    char *menu_font = config_get_value(NULL, 0, "MENU_FONT");
    if (menu_font == NULL)
        menu_font = config_get_value(NULL, 0, "TERMINAL_FONT");
    if (menu_font != NULL) {
        struct file_handle f;
        if (!uri_open(&f, menu_font)) {
            print("menu: Could not open font file.\n");
        } else {
            fread(&f, vga_font_bits, 0, VGA_FONT_MAX);
        }
    }

    if (vga_font_bool == NULL) {
        vga_font_bool = ext_mem_alloc(VGA_FONT_GLYPHS * VGA_FONT_HEIGHT * VGA_FONT_WIDTH * sizeof(bool));

        for (size_t i = 0; i < VGA_FONT_GLYPHS; i++) {
            uint8_t *glyph = &vga_font_bits[i * VGA_FONT_HEIGHT];

            for (int y = 0; y < VGA_FONT_HEIGHT; y++) {
                for (int x = 0; x < VGA_FONT_WIDTH; x++) {
                    size_t offset = i * VGA_FONT_HEIGHT * VGA_FONT_WIDTH + y * VGA_FONT_WIDTH + x;
                    if ((glyph[y] & (0x80 >> x))) {
                        vga_font_bool[offset] = true;
                    } else {
                        vga_font_bool[offset] = false;
                    }
                }
            }
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
