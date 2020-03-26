#include <stddef.h>
#include <stdint.h>
#include <drivers/vbe.h>
#include <lib/blib.h>
#include <lib/real.h>

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

    if ((r.eax & 0x00ff) == 0x4f)
        return -1;
    if ((r.eax & 0xff00) != 0)
        return -1;

    return 0;
}

int init_vbe(uint64_t *framebuffer, uint16_t *pitch, uint16_t *target_width, uint16_t *target_height) {
    print("vbe: Initialising...\n");

    struct vbe_info_struct vbe_info;
    get_vbe_info(&vbe_info);

    print("vbe: Version: %u.%u\n", vbe_info.version_maj, vbe_info.version_min);
    print("vbe: OEM: %s\n", (char *)rm_desegment(vbe_info.oem_seg, vbe_info.oem_off));
    print("vbe: Graphics vendor: %s\n", (char *)rm_desegment(vbe_info.vendor_seg, vbe_info.vendor_off));
    print("vbe: Product name: %s\n", (char *)rm_desegment(vbe_info.prod_name_seg, vbe_info.prod_name_off));
    print("vbe: Product revision: %s\n", (char *)rm_desegment(vbe_info.prod_rev_seg, vbe_info.prod_rev_off));

    struct edid_info_struct edid_info;
    if (!*target_width || !*target_height) {
        if (get_edid_info(&edid_info)) {
            print("vbe: EDID unavailable, defaulting to 1024x768\n");
            *target_width  = 1024;
            *target_height = 768;
        } else {
            print("vbe: EDID detected screen resolution of %ux%u\n");
            *target_width   = (int)edid_info.det_timing_desc1[2];
            *target_width  += ((int)edid_info.det_timing_desc1[4] & 0xf0) << 4;
            *target_height  = (int)edid_info.det_timing_desc1[5];
            *target_height += ((int)edid_info.det_timing_desc1[7] & 0xf0) << 4;
        }
    } else {
        print("vbe: Requested resolution of %ux%u\n", *target_width, *target_height);
    }

    uint16_t *vid_modes = (uint16_t *)rm_desegment(vbe_info.vid_modes_seg,
                                                   vbe_info.vid_modes_off);

    for (size_t i = 0; vid_modes[i] != 0xffff; i++) {
        struct vbe_mode_info_struct vbe_mode_info;
        get_vbe_mode_info(&vbe_mode_info, vid_modes[i]);
        if  (vbe_mode_info.res_x == *target_width
          && vbe_mode_info.res_y == *target_height
          /*&& vbe_mode_info.bpp   == *target_bpp*/) {
            print("vbe: Found matching mode %x, attempting to set\n", vid_modes[i]);
            *framebuffer = (uint64_t)vbe_mode_info.framebuffer;
            *pitch       = (int)vbe_mode_info.pitch;
            print("vbe: Framebuffer address: %x\n", vbe_mode_info.framebuffer);
            set_vbe_mode(vid_modes[i]);
            return 0;
        }
    }

    return -1;
}
