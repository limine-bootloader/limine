#ifndef __DRIVERS__VBE_H__
#define __DRIVERS__VBE_H__

int init_vbe(uint64_t *framebuffer, uint16_t *pitch, uint16_t *target_width, uint16_t *target_height, uint16_t *target_bpp);

#endif
