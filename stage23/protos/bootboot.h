#ifndef __PROTOS__BOOTBOOT_H__
#define __PROTOS__BOOTBOOT_H__

#include <stdint.h>
#include <stdbool.h>

void bootboot_load(char *config, char *cmdline, void *efi_system_table);

/*
 * bootboot.h
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

/* minimum protocol level:
 *  hardcoded kernel name, static kernel memory addresses */
#define PROTOCOL_MINIMAL 0
/* static protocol level:
 *  kernel name parsed from environment, static kernel memory addresses */
#define PROTOCOL_STATIC  1
/* dynamic protocol level:
 *  kernel name parsed, kernel memory addresses from ELF or PE symbols */
#define PROTOCOL_DYNAMIC 2
/* big-endian flag */
#define PROTOCOL_BIGENDIAN 0x80

/* loader types, just informational */
#define LOADER_BIOS     (0<<2)
#define LOADER_UEFI     (1<<2)
#define LOADER_RPI      (2<<2)
#define LOADER_COREBOOT (3<<2)

/* framebuffer pixel format, only 32 bits supported */
#define FB_ARGB   0
#define FB_RGBA   1
#define FB_ABGR   2
#define FB_BGRA   3

/* mmap entry, type is stored in least significant tetrad (half byte) of size
 * this means size described in 16 byte units (not a problem, most modern
 * firmware report memory in pages, 4096 byte units anyway). */
typedef struct {
  uint64_t   ptr;
  uint64_t   size;
} MMapEnt;
#define MMapEnt_Ptr(a)  ((a)->ptr)
#define MMapEnt_Size(a) ((a)->size & 0xFFFFFFFFFFFFFFF0)
#define MMapEnt_Type(a) ((a)->size & 0xF)
#define MMapEnt_IsFree(a) (((a)->size&0xF)==1)
#define MMapEnt_Combine(addr, type) (((addr) & ~0xF) | ((type)))

#define MMAP_USED     0   /* don't use. Reserved or unknown regions */
#define MMAP_FREE     1   /* usable memory */
#define MMAP_ACPI     2   /* acpi memory, volatile and non-volatile as well */
#define MMAP_MMIO     3   /* memory mapped IO region */

#define INITRD_MAXSIZE 16 /* Mb */

typedef struct {
  /* first 64 bytes is platform independent */
  uint8_t    magic[4];    /* 'BOOT' magic */
  uint32_t   size;        /* length of bootboot structure, minimum 128 */
  uint8_t    protocol;    /* 1, static addresses, see PROTOCOL_* and LOADER_* above */
  uint8_t    fb_type;     /* framebuffer type, see FB_* above */
  uint16_t   numcores;    /* number of processor cores */
  uint16_t   bspid;       /* Bootsrap processor ID (Local APIC Id on x86_64) */
  int16_t    timezone;    /* in minutes -1440..1440 */
  uint8_t    datetime[8]; /* in BCD yyyymmddhhiiss UTC (independent to timezone) */
  uint64_t   initrd_ptr;  /* ramdisk image position and size */
  uint64_t   initrd_size;
  uint64_t   fb_ptr;      /* framebuffer pointer and dimensions */
  uint32_t   fb_size;
  uint32_t   fb_width;
  uint32_t   fb_height;
  uint32_t   fb_scanline;

  /* the rest (64 bytes) is platform specific */
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

  /* from 128th byte, MMapEnt[], more records may follow */
  MMapEnt    mmap[];
  /* use like this:
   * MMapEnt *mmap_ent = &bootboot.mmap; mmap_ent++;
   * until you reach bootboot->size, while(mmap_ent < bootboot + bootboot->size) */
} BOOTBOOT;

#endif
