#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <drivers/vbe.h>
#include <lib/libc.h>
#include <lib/blib.h>
#include <lib/real.h>
#include <lib/print.h>
#include <lib/image.h>
#include <mm/pmm.h>

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

static uint32_t *vbe_framebuffer;
static uint16_t  vbe_pitch;
static uint16_t  vbe_width = 0;
static uint16_t  vbe_height = 0;
static uint16_t  vbe_bpp = 0;

static int frame_height, frame_width;

static struct image *background;

static struct vbe_char *grid;

static bool cursor_status = true;

static int cursor_x;
static int cursor_y;

static uint32_t cursor_fg = 0x00000000;
static uint32_t cursor_bg = 0x00ffffff;
static uint32_t text_fg;
static uint32_t text_bg;

static int rows;
static int cols;

#define A(rgb) (uint8_t)(rgb >> 24)
#define R(rgb) (uint8_t)(rgb >> 16)
#define G(rgb) (uint8_t)(rgb >> 8)
#define B(rgb) (uint8_t)(rgb)
#define ARGB(a, r, g, b) (a << 24) | ((r & 0xFF) << 16) | ((g & 0xFF) << 8) | (b & 0xFF)

static inline uint32_t colour_blend(uint32_t fg, uint32_t bg) {
    uint8_t alpha = 255 - A(fg);
    uint8_t inv_alpha = A(fg) - 1;

    uint8_t r = (uint8_t)((alpha * R(fg) + inv_alpha * R(bg)) / 255);
    uint8_t g = (uint8_t)((alpha * G(fg) + inv_alpha * G(bg)) / 255);
    uint8_t b = (uint8_t)((alpha * B(fg) + inv_alpha * B(bg)) / 255);

    return ARGB(0, r, g, b);
}

void vbe_plot_px(int x, int y, uint32_t hex) {
    size_t fb_i = x + (vbe_pitch / sizeof(uint32_t)) * y;

    vbe_framebuffer[fb_i] = hex;
}

void vbe_plot_bg_blent_px(int x, int y, uint32_t hex) {
    vbe_plot_px(x, y, colour_blend(hex, background->get_pixel(background, x, y)));
}

void vbe_plot_background(int x, int y, int width, int height) {
    if (background) {
        for (int yy = 0; yy < height; yy++) {
            for (int xx = 0; xx < width; xx++) {
                vbe_plot_px(x + xx, y + yy, background->get_pixel(background, x + xx, y + yy));
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
    char c;
    uint32_t fg;
    uint32_t bg;
};

void vbe_plot_char(struct vbe_char c, int x, int y) {
    uint8_t *glyph = &vga_font[c.c * VGA_FONT_HEIGHT];

    if (background && A(c.fg)) {
        if (A(c.bg))
            vbe_plot_bg_blent_rect(x, y, VGA_FONT_WIDTH, VGA_FONT_HEIGHT, c.bg);
        else
            vbe_plot_rect(x, y, VGA_FONT_WIDTH, VGA_FONT_HEIGHT, c.bg);

        for (int i = 0; i < VGA_FONT_HEIGHT; i++) {
            for (int j = 0; j < VGA_FONT_WIDTH; j++) {
                if ((glyph[i] & (0x80 >> j)))
                    vbe_plot_bg_blent_px(x + j, y + i, c.fg);
            }
        }
    } else {
        if (A(c.bg))
            vbe_plot_bg_blent_rect(x, y, VGA_FONT_WIDTH, VGA_FONT_HEIGHT, c.bg);
        else
            vbe_plot_rect(x, y, VGA_FONT_WIDTH, VGA_FONT_HEIGHT, c.bg);

        for (int i = 0; i < VGA_FONT_HEIGHT; i++) {
            for (int j = 0; j < VGA_FONT_WIDTH; j++) {
                if ((glyph[i] & (0x80 >> j)))
                    vbe_plot_px(x + j, y + i, c.fg);
            }
        }
    }
}

static void plot_char_grid(struct vbe_char c, int x, int y) {
    vbe_plot_char(c, x * VGA_FONT_WIDTH + frame_width,
                     y * VGA_FONT_HEIGHT + frame_height);
    grid[x + y * cols] = c;
}

static void clear_cursor(void) {
    if (cursor_status) {
        vbe_plot_char(grid[cursor_x + cursor_y * cols],
                  cursor_x * VGA_FONT_WIDTH + frame_width,
                  cursor_y * VGA_FONT_HEIGHT + frame_height);
    }
}

static void draw_cursor(void) {
    struct vbe_char c = grid[cursor_x + cursor_y * cols];
    c.fg = cursor_fg;
    c.bg = cursor_bg;
    if (cursor_status)
        vbe_plot_char(c, cursor_x * VGA_FONT_WIDTH + frame_width,
                         cursor_y * VGA_FONT_HEIGHT + frame_height);
}

static void scroll(void) {
    clear_cursor();

    for (int i = cols; i < rows * cols; i++) {
        plot_char_grid(grid[i], (i - cols) % cols, (i - cols) / cols);
    }

    // Clear the last line of the screen.
    struct vbe_char empty;
    empty.c  = ' ';
    empty.fg = text_fg;
    empty.bg = text_bg;
    for (int i = rows * cols - cols; i < rows * cols; i++) {
        plot_char_grid(empty, i % cols, i / cols);
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
        plot_char_grid(empty, i % cols, i / cols);
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

void vbe_putchar(char c) {
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
            plot_char_grid(ch, cursor_x++, cursor_y);
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

void vbe_tty_init(int *_rows, int *_cols, uint32_t *_colours, int _margin, struct image *_background) {
    init_vbe(&vbe_framebuffer, &vbe_pitch, &vbe_width, &vbe_height, &vbe_bpp);
    vga_font_retrieve();
    *_cols = cols = (vbe_width - _margin * 2) / VGA_FONT_WIDTH;
    *_rows = rows = (vbe_height - _margin * 2) / VGA_FONT_HEIGHT;
    grid = ext_mem_alloc(rows * cols * sizeof(struct vbe_char));
    background = _background;
    memcpy(ansi_colours, _colours, sizeof(ansi_colours));
    text_bg = ansi_colours[0];
    text_fg = ansi_colours[7];

    frame_height = vbe_height / 2 - (VGA_FONT_HEIGHT * rows) / 2;
    frame_width  = vbe_width  / 2 - (VGA_FONT_WIDTH  * cols) / 2;

    vbe_plot_background(0, 0, vbe_width, vbe_height);
    vbe_clear(true);
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
    uint8_t pad0[16];
    uint16_t pitch;
    uint16_t res_x;
    uint16_t res_y;
    uint8_t pad1[3];
    uint8_t bpp;
    uint8_t pad2[14];
    uint32_t framebuffer;
    uint8_t pad3[212];
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

static void set_vbe_mode(uint16_t mode) {
    struct rm_regs r = {0};

    r.eax = 0x4f02;
    r.ebx = (uint32_t)mode | (1 << 14);
    rm_int(0x10, &r, &r);
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

int init_vbe(uint32_t **framebuffer, uint16_t *pitch, uint16_t *target_width, uint16_t *target_height, uint16_t *target_bpp) {
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
    if (!*target_width || !*target_height || !*target_bpp) {
        *target_width  = 1024;
        *target_height = 768;
        *target_bpp    = 32;
        if (!get_edid_info(&edid_info)) {
            int edid_width   = (int)edid_info.det_timing_desc1[2];
                edid_width  += ((int)edid_info.det_timing_desc1[4] & 0xf0) << 4;
            int edid_height  = (int)edid_info.det_timing_desc1[5];
                edid_height += ((int)edid_info.det_timing_desc1[7] & 0xf0) << 4;
            if (edid_width && edid_height) {
                *target_width  = edid_width;
                *target_height = edid_height;
                print("vbe: EDID detected screen resolution of %ux%u\n",
                      *target_width, *target_height);
            }
        }
    } else {
        print("vbe: Requested resolution of %ux%ux%u\n",
              *target_width, *target_height, *target_bpp);
    }

retry:;
    uint16_t *vid_modes = (uint16_t *)rm_desegment(vbe_info.vid_modes_seg,
                                                   vbe_info.vid_modes_off);

    for (size_t i = 0; vid_modes[i] != 0xffff; i++) {
        struct vbe_mode_info_struct vbe_mode_info;
        get_vbe_mode_info(&vbe_mode_info, vid_modes[i]);
        if  (vbe_mode_info.res_x == *target_width
          && vbe_mode_info.res_y == *target_height
          && vbe_mode_info.bpp   == *target_bpp) {
            print("vbe: Found matching mode %x, attempting to set\n", vid_modes[i]);
            *framebuffer = (uint32_t *)vbe_mode_info.framebuffer;
            *pitch       = (int)vbe_mode_info.pitch;
            print("vbe: Framebuffer address: %x\n", vbe_mode_info.framebuffer);
            set_vbe_mode(vid_modes[i]);
            return 0;
        }
    }

    if (current_fallback < SIZEOF_ARRAY(fallback_resolutions)) {
        *target_width  = fallback_resolutions[current_fallback].width;
        *target_height = fallback_resolutions[current_fallback].height;
        *target_bpp    = fallback_resolutions[current_fallback].bpp;
        current_fallback++;
        goto retry;
    }

    panic("Could not set a video mode");
}
