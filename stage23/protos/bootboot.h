#ifndef __PROTOS__BOOTBOOT_H__
#define __PROTOS__BOOTBOOT_H__

#include <stdint.h>
#include <stdbool.h>

void bootboot_load(char *config);

/*
 * what follows is (modified) bootboot.h
 * https://gitlab.com/bztsrc/bootboot
 *
 * Copyright (C) 2021 pitust (piotr@stelmaszek.com)
 * Copyright (C) 2017 - 2021 bzt (bztsrc@gitlab)
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * This file is part of the BOOTBOOT Protocol package.
 * @brief The BOOTBOOT structure
 *
 */

#define BOOTBOOT_MAGIC "BOOT"

/* default virtual addresses for level 0 and 1 static loaders */
#define BOOTBOOT_MMIO   0xfffffffff8000000  /* memory mapped IO virtual address */
#define BOOTBOOT_FB     0xfffffffffc000000  /* frame buffer virtual address */
#define BOOTBOOT_INFO   0xffffffffffe00000  /* bootboot struct virtual address */
#define BOOTBOOT_ENV    0xffffffffffe01000  /* environment string virtual address */
#define BOOTBOOT_CORE   0xffffffffffe02000  /* core loadable segment start */


typedef struct {
  uint64_t   ptr;
  uint64_t   size;
} MMapEnt;

typedef struct {
  uint8_t    magic[4];
  uint32_t   size;
  uint8_t    protocol;
  uint8_t    fb_type;
  uint16_t   numcores;
  uint16_t   bspid;
  int16_t    timezone;
  uint8_t    datetime[8];
  uint64_t   initrd_ptr;
  uint64_t   initrd_size;
  uint64_t   fb_ptr;
  uint32_t   fb_size;
  uint32_t   fb_width;
  uint32_t   fb_height;
  uint32_t   fb_scanline;

  union {
    struct {
      uint64_t acpi_ptr;
      uint64_t smbi_ptr;
      uint64_t efi_ptr;
      uint64_t mp_ptr;
      uint64_t unused0;
      uint64_t unused1;
      uint64_t unused2;
      uint64_t unused3;
    } x86_64;
  } arch;

  MMapEnt    mmap[];
} BOOTBOOT;

#endif
