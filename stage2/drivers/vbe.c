#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <drivers/vbe.h>
#include <lib/libc.h>
#include <lib/blib.h>
#include <lib/real.h>
#include <lib/print.h>
#include <lib/image.h>
#include <lib/config.h>
#include <lib/uri.h>
#include <mm/pmm.h>
#include <mm/mtrr.h>

#define VGA_FONT_WIDTH  8
#define VGA_FONT_HEIGHT 16
#define VGA_FONT_GLYPHS 256
#define VGA_FONT_MAX    (VGA_FONT_HEIGHT * VGA_FONT_GLYPHS)

static uint8_t *vga_font;

static void vga_font_retrieve(void) {
    struct rm_regs r = {0};

    r.eax = 0x1130;
    r.ebx = 0x0600;
    rm_int(0x10, &r, &r);

    vga_font = ext_mem_alloc(VGA_FONT_MAX);

    memcpy(vga_font, (void *)rm_desegment(r.es, r.ebp), VGA_FONT_MAX);
}

static uint32_t ansi_colours[8];

static struct vbe_framebuffer_info fbinfo;
static uint32_t *vbe_framebuffer;
static uint16_t  vbe_pitch;
static uint16_t  vbe_width;
static uint16_t  vbe_height;
static uint16_t  vbe_bpp;

static int frame_height, frame_width;

static struct image *background;

static struct vbe_char *grid;
static struct vbe_char *front_grid;

static bool double_buffer_enabled = false;

static bool cursor_status = true;

static int cursor_x;
static int cursor_y;

static uint32_t cursor_fg = 0x00000000;
static uint32_t cursor_bg = 0x00ffffff;
static uint32_t text_fg;
static uint32_t text_bg;

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

void vbe_plot_px(int x, int y, uint32_t hex) {
    size_t fb_i = x + (vbe_pitch / sizeof(uint32_t)) * y;

    vbe_framebuffer[fb_i] = hex;
}

static void _vbe_plot_bg_blent_px(int x, int y, uint32_t hex) {
    vbe_plot_px(x, y, colour_blend(hex, background->get_pixel(background, x, y)));
}

void (*vbe_plot_bg_blent_px)(int x, int y, uint32_t hex) = vbe_plot_px;

static uint32_t blend_gradient_from_box(int x, int y, uint32_t hex) {
    if (x >= frame_width  && x < frame_width  + VGA_FONT_WIDTH  * cols
     && y >= frame_height && y < frame_height + VGA_FONT_HEIGHT * rows) {
        return hex;
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

void vbe_plot_background(int x, int y, int width, int height) {
    if (background) {
        for (int yy = 0; yy < height; yy++) {
            for (int xx = 0; xx < width; xx++) {
                vbe_plot_px(x + xx, y + yy, blend_gradient_from_box(xx, yy, text_bg));
            }
        }
    } else {
        for (int yy = 0; yy < height; yy++) {
            for (int xx = 0; xx < width; xx++) {
                vbe_plot_px(x + xx, y + yy, text_bg);
            }
        }
    }
}

void vbe_plot_rect(int x, int y, int width, int height, uint32_t hex) {
    for (int yy = 0; yy < height; yy++) {
        for (int xx = 0; xx < width; xx++) {
            vbe_plot_px(x + xx, y + yy, hex);
        }
    }
}

void vbe_plot_bg_blent_rect(int x, int y, int width, int height, uint32_t hex) {
    for (int yy = 0; yy < height; yy++) {
        for (int xx = 0; xx < width; xx++) {
            vbe_plot_bg_blent_px(x + xx, y + yy, hex);
        }
    }
}

struct vbe_char {
    uint32_t c;
    uint32_t fg;
    uint32_t bg;
};

void vbe_plot_char(struct vbe_char *c, int x, int y) {
    uint8_t *glyph = &vga_font[(size_t)c->c * VGA_FONT_HEIGHT];

    vbe_plot_bg_blent_rect(x, y, VGA_FONT_WIDTH, VGA_FONT_HEIGHT, c->bg);

    for (int i = 0; i < VGA_FONT_HEIGHT; i++) {
        for (int j = 0; j < VGA_FONT_WIDTH; j++) {
            if ((glyph[i] & (0x80 >> j)))
                vbe_plot_bg_blent_px(x + j, y + i, c->fg);
        }
    }
}

static void plot_char_grid(struct vbe_char *c, int x, int y) {
    if (!double_buffer_enabled) {
        vbe_plot_char(c, x * VGA_FONT_WIDTH + frame_width,
                         y * VGA_FONT_HEIGHT + frame_height);
    }
    grid[x + y * cols] = *c;
}

static void clear_cursor(void) {
    struct vbe_char c = grid[cursor_x + cursor_y * cols];
    c.fg = text_fg;
    c.bg = text_bg;
    plot_char_grid(&c, cursor_x, cursor_y);
}

static void draw_cursor(void) {
    if (cursor_status) {
        struct vbe_char c = grid[cursor_x + cursor_y * cols];
        c.fg = cursor_fg;
        c.bg = cursor_bg;
        plot_char_grid(&c, cursor_x, cursor_y);
    }
}

static void scroll(void) {
    clear_cursor();

    for (int i = cols; i < rows * cols; i++) {
        plot_char_grid(&grid[i], (i - cols) % cols, (i - cols) / cols);
    }

    // Clear the last line of the screen.
    struct vbe_char empty;
    empty.c  = ' ';
    empty.fg = text_fg;
    empty.bg = text_bg;
    for (int i = rows * cols - cols; i < rows * cols; i++) {
        plot_char_grid(&empty, i % cols, i / cols);
    }

    draw_cursor();
}

void vbe_clear(bool move) {
    clear_cursor();

    struct vbe_char empty;
    empty.c  = ' ';
    empty.fg = text_fg;
    empty.bg = text_bg;
    for (int i = 0; i < rows * cols; i++) {
        plot_char_grid(&empty, i % cols, i / cols);
    }

    if (move) {
        cursor_x = 0;
        cursor_y = 0;
    }

    draw_cursor();
}

void vbe_enable_cursor(void) {
    cursor_status = true;
    draw_cursor();
}

void vbe_disable_cursor(void) {
    clear_cursor();
    cursor_status = false;
}

void vbe_set_cursor_pos(int x, int y) {
    clear_cursor();
    cursor_x = x;
    cursor_y = y;
    draw_cursor();
}

void vbe_get_cursor_pos(int *x, int *y) {
    *x = cursor_x;
    *y = cursor_y;
}

void vbe_set_text_fg(int fg) {
    text_fg = ansi_colours[fg];
}

void vbe_set_text_bg(int bg) {
    text_bg = ansi_colours[bg];
}

void vbe_double_buffer_flush(void) {
    for (size_t i = 0; i < (size_t)rows * cols; i++) {
        if (!memcmp(&grid[i], &front_grid[i], sizeof(struct vbe_char)))
            continue;

        front_grid[i] = grid[i];

        int x = i % cols;
        int y = i / cols;

        vbe_plot_char(&grid[i], x * VGA_FONT_WIDTH + frame_width,
                                y * VGA_FONT_HEIGHT + frame_height);
    }
}

void vbe_double_buffer(bool state) {
    if (state) {
        memcpy(front_grid, grid, rows * cols * sizeof(struct vbe_char));
        double_buffer_enabled = true;
        vbe_clear(true);
        vbe_double_buffer_flush();
    } else {
        bool pcs = cursor_status;
        cursor_status = false;
        vbe_clear(true);
        vbe_double_buffer_flush();
        cursor_status = pcs;
        draw_cursor();
        double_buffer_enabled = false;
    }
}

void vbe_putchar(uint8_t c) {
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
            vbe_set_cursor_pos(0, cursor_y);
            break;
        case '\n':
            if (cursor_y == (rows - 1)) {
                vbe_set_cursor_pos(0, rows - 1);
                scroll();
            } else {
                vbe_set_cursor_pos(0, cursor_y + 1);
            }
            break;
        default: {
            clear_cursor();
            struct vbe_char ch;
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

bool vbe_tty_init(int *_rows, int *_cols, uint32_t *_colours, int _margin, int _margin_gradient, struct image *_background) {
    int req_width = 0, req_height = 0, req_bpp = 0;

    char *menu_resolution = config_get_value(NULL, 0, "MENU_RESOLUTION");
    if (menu_resolution == NULL)
        parse_resolution(&req_width, &req_height, &req_bpp, menu_resolution);

    // We force bpp to 32
    req_bpp = 32;

    init_vbe(&fbinfo, req_width, req_height, req_bpp);

    // Ensure this is xRGB8888, we only support that for the menu
    if (fbinfo.red_mask_size    != 8
     || fbinfo.red_mask_shift   != 16
     || fbinfo.green_mask_size  != 8
     || fbinfo.green_mask_shift != 8
     || fbinfo.blue_mask_size   != 8
     || fbinfo.blue_mask_shift  != 0)
        return false;

    vbe_framebuffer = (void *)fbinfo.framebuffer_addr;
    vbe_width       = fbinfo.framebuffer_width;
    vbe_height      = fbinfo.framebuffer_height;
    vbe_bpp         = fbinfo.framebuffer_bpp;
    vbe_pitch       = fbinfo.framebuffer_pitch;

    mtrr_set_range((uint64_t)(size_t)vbe_framebuffer,
                   (uint64_t)vbe_pitch * vbe_height, MTRR_MEMORY_TYPE_WC);

    char *menu_font = config_get_value(NULL, 0, "MENU_FONT");
    if (menu_font == NULL) {
        vga_font_retrieve();
    } else {
        struct file_handle f;
        if (!uri_open(&f, menu_font)) {
            print("menu: Could not open font file.\n");
            vga_font_retrieve();
        } else {
            vga_font = ext_mem_alloc(VGA_FONT_MAX);
            fread(&f, vga_font, 0, VGA_FONT_MAX);
        }
    }

    *_cols = cols = (vbe_width - _margin * 2) / VGA_FONT_WIDTH;
    *_rows = rows = (vbe_height - _margin * 2) / VGA_FONT_HEIGHT;
    grid = ext_mem_alloc(rows * cols * sizeof(struct vbe_char));
    front_grid = ext_mem_alloc(rows * cols * sizeof(struct vbe_char));
    background = _background;

    if (background)
        vbe_plot_bg_blent_px = _vbe_plot_bg_blent_px;

    memcpy(ansi_colours, _colours, sizeof(ansi_colours));
    text_bg = ansi_colours[0];
    text_fg = ansi_colours[7];

    margin_gradient = _margin_gradient;

    frame_height = vbe_height / 2 - (VGA_FONT_HEIGHT * rows) / 2;
    frame_width  = vbe_width  / 2 - (VGA_FONT_WIDTH  * cols) / 2;

    vbe_plot_background(0, 0, vbe_width, vbe_height);
    vbe_clear(true);

    return true;
}

struct vbe_info_struct {
    char     signature[4];
    uint8_t  version_min;
    uint8_t  version_maj;
    uint16_t oem_off;
    uint16_t oem_seg;
    uint32_t capabilities;
    uint16_t vid_modes_off;
    uint16_t vid_modes_seg;
    uint16_t vid_mem_blocks;
    uint16_t software_rev;
    uint16_t vendor_off;
    uint16_t vendor_seg;
    uint16_t prod_name_off;
    uint16_t prod_name_seg;
    uint16_t prod_rev_off;
    uint16_t prod_rev_seg;
    uint8_t  reserved[222];
    uint8_t  oem_data[256];
} __attribute__((packed));

struct vbe_mode_info_struct {
    uint16_t mode_attributes;
    uint8_t  wina_attributes;
    uint8_t  winb_attributes;
    uint16_t win_granularity;
    uint16_t win_size;
    uint16_t wina_segment;
    uint16_t winb_segment;
    uint32_t win_farptr;
    uint16_t bytes_per_scanline;

    uint16_t res_x;
    uint16_t res_y;
    uint8_t  charsize_x;
    uint8_t  charsize_y;
    uint8_t  plane_count;
    uint8_t  bpp;
    uint8_t  bank_count;
    uint8_t  memory_model;
    uint8_t  bank_size;
    uint8_t  image_count;
    uint8_t  reserved0;

    uint8_t  red_mask_size;
    uint8_t  red_mask_shift;
    uint8_t  green_mask_size;
    uint8_t  green_mask_shift;
    uint8_t  blue_mask_size;
    uint8_t  blue_mask_shift;
    uint8_t  rsvd_mask_size;
    uint8_t  rsvd_mask_shift;
    uint8_t  direct_color_info;

    uint32_t framebuffer_addr;
    uint8_t  reserved1[6];

    uint16_t lin_bytes_per_scanline;
    uint8_t  banked_image_count;
    uint8_t  lin_image_count;
    uint8_t  lin_red_mask_size;
    uint8_t  lin_red_mask_shift;
    uint8_t  lin_green_mask_size;
    uint8_t  lin_green_mask_shift;
    uint8_t  lin_blue_mask_size;
    uint8_t  lin_blue_mask_shift;
    uint8_t  lin_rsvd_mask_size;
    uint8_t  lin_rsvd_mask_shift;
    uint32_t max_pixel_clock;

    uint8_t  reserved2[189];
} __attribute__((packed));

static void get_vbe_info(struct vbe_info_struct *buf) {
    struct rm_regs r = {0};

    r.eax = 0x4f00;
    r.edi = (uint32_t)buf;
    rm_int(0x10, &r, &r);
}

static void get_vbe_mode_info(struct vbe_mode_info_struct *buf,
                              uint16_t mode) {
    struct rm_regs r = {0};

    r.eax = 0x4f01;
    r.ecx = (uint32_t)mode;
    r.edi = (uint32_t)buf;
    rm_int(0x10, &r, &r);
}

static int set_vbe_mode(uint16_t mode) {
    struct rm_regs r = {0};

    r.eax = 0x4f02;
    r.ebx = (uint32_t)mode | (1 << 14);
    rm_int(0x10, &r, &r);

    return r.eax & 0xff;
}

struct edid_info_struct {
    uint8_t padding[8];
    uint16_t manufacturer_id_be;
    uint16_t edid_id_code;
    uint32_t serial_num;
    uint8_t man_week;
    uint8_t man_year;
    uint8_t edid_version;
    uint8_t edid_revision;
    uint8_t video_input_type;
    uint8_t max_hor_size;
    uint8_t max_ver_size;
    uint8_t gamma_factor;
    uint8_t dpms_flags;
    uint8_t chroma_info[10];
    uint8_t est_timings1;
    uint8_t est_timings2;
    uint8_t man_res_timing;
    uint16_t std_timing_id[8];
    uint8_t det_timing_desc1[18];
    uint8_t det_timing_desc2[18];
    uint8_t det_timing_desc3[18];
    uint8_t det_timing_desc4[18];
    uint8_t unused;
    uint8_t checksum;
} __attribute__((packed));

static int get_edid_info(struct edid_info_struct *buf) {
    struct rm_regs r = {0};

    r.eax = 0x4f15;
    r.ebx = 0x0001;
    r.edi = (uint32_t)buf;
    rm_int(0x10, &r, &r);

    if ((r.eax & 0x00ff) != 0x4f)
        return -1;
    if ((r.eax & 0xff00) != 0)
        return -1;

    return 0;
}

struct resolution {
    uint16_t width;
    uint16_t height;
    uint16_t bpp;
};

static struct resolution fallback_resolutions[] = {
    { 1024, 768, 32 },
    { 800,  600, 32 },
    { 640,  480, 32 }
};

bool init_vbe(struct vbe_framebuffer_info *ret,
              uint16_t target_width, uint16_t target_height, uint16_t target_bpp) {
    print("vbe: Initialising...\n");

    size_t current_fallback = 0;

    struct vbe_info_struct vbe_info;
    get_vbe_info(&vbe_info);

    print("vbe: Version: %u.%u\n", vbe_info.version_maj, vbe_info.version_min);
    print("vbe: OEM: %s\n", (char *)rm_desegment(vbe_info.oem_seg, vbe_info.oem_off));
    print("vbe: Graphics vendor: %s\n", (char *)rm_desegment(vbe_info.vendor_seg, vbe_info.vendor_off));
    print("vbe: Product name: %s\n", (char *)rm_desegment(vbe_info.prod_name_seg, vbe_info.prod_name_off));
    print("vbe: Product revision: %s\n", (char *)rm_desegment(vbe_info.prod_rev_seg, vbe_info.prod_rev_off));

    struct edid_info_struct edid_info;
    if (!target_width || !target_height || !target_bpp) {
        target_width  = 1024;
        target_height = 768;
        target_bpp    = 32;
        if (!get_edid_info(&edid_info)) {
            int edid_width   = (int)edid_info.det_timing_desc1[2];
                edid_width  += ((int)edid_info.det_timing_desc1[4] & 0xf0) << 4;
            int edid_height  = (int)edid_info.det_timing_desc1[5];
                edid_height += ((int)edid_info.det_timing_desc1[7] & 0xf0) << 4;
            if (edid_width && edid_height) {
                target_width  = edid_width;
                target_height = edid_height;
                print("vbe: EDID detected screen resolution of %ux%u\n",
                      target_width, target_height);
            }
        }
    } else {
        print("vbe: Requested resolution of %ux%ux%u\n",
              target_width, target_height, target_bpp);
    }

    uint16_t *vid_modes = (uint16_t *)rm_desegment(vbe_info.vid_modes_seg,
                                                   vbe_info.vid_modes_off);

retry:
    for (size_t i = 0; vid_modes[i] != 0xffff; i++) {
        struct vbe_mode_info_struct vbe_mode_info;
        get_vbe_mode_info(&vbe_mode_info, vid_modes[i]);
        if  (vbe_mode_info.res_x == target_width
          && vbe_mode_info.res_y == target_height
          && vbe_mode_info.bpp   == target_bpp) {
            // We only support RGB for now
            if (vbe_mode_info.memory_model != 0x06)
                continue;
            // We only support linear modes
            if (!(vbe_mode_info.mode_attributes & (1 << 7)))
                continue;
            print("vbe: Found matching mode %x, attempting to set...\n", vid_modes[i]);
            if (set_vbe_mode(vid_modes[i]) == 0x01) {
                print("vbe: Failed to set video mode %x, moving on...\n", vid_modes[i]);
                continue;
            }
            print("vbe: Framebuffer address: %x\n", vbe_mode_info.framebuffer_addr);
            ret->memory_model       = vbe_mode_info.memory_model;
            ret->framebuffer_addr   = vbe_mode_info.framebuffer_addr;
            ret->framebuffer_width  = vbe_mode_info.res_x;
            ret->framebuffer_height = vbe_mode_info.res_y;
            ret->framebuffer_bpp    = vbe_mode_info.bpp;
            if (vbe_info.version_maj < 3) {
                ret->framebuffer_pitch  = vbe_mode_info.bytes_per_scanline;
                ret->red_mask_size      = vbe_mode_info.red_mask_size;
                ret->red_mask_shift     = vbe_mode_info.red_mask_shift;
                ret->green_mask_size    = vbe_mode_info.green_mask_size;
                ret->green_mask_shift   = vbe_mode_info.green_mask_shift;
                ret->blue_mask_size     = vbe_mode_info.blue_mask_size;
                ret->blue_mask_shift    = vbe_mode_info.blue_mask_shift;
            } else {
                ret->framebuffer_pitch  = vbe_mode_info.lin_bytes_per_scanline;
                ret->red_mask_size      = vbe_mode_info.lin_red_mask_size;
                ret->red_mask_shift     = vbe_mode_info.lin_red_mask_shift;
                ret->green_mask_size    = vbe_mode_info.lin_green_mask_size;
                ret->green_mask_shift   = vbe_mode_info.lin_green_mask_shift;
                ret->blue_mask_size     = vbe_mode_info.lin_blue_mask_size;
                ret->blue_mask_shift    = vbe_mode_info.lin_blue_mask_shift;
            }
            return true;
        }
    }

    if (current_fallback < SIZEOF_ARRAY(fallback_resolutions)) {
        target_width  = fallback_resolutions[current_fallback].width;
        target_height = fallback_resolutions[current_fallback].height;
        target_bpp    = fallback_resolutions[current_fallback].bpp;
        current_fallback++;
        goto retry;
    }

    return false;
}
