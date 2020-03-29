/**
 * TODO
 * 1. Finish ext2 open and read
 * 2. Implement EXT2 FS check
 * 3. Implement generic filesystem functions in lib/file.h
 */

#ifndef __FS_EXT2FS_H__
#define __FS_EXT2FS_H__

#include <stdint.h>
#include <stddef.h>
#include <drivers/disk.h>
#include <lib/libc.h>
#include <lib/blib.h>

/* Drive Format Error Codes */
#define EXT2            0
#define OTHER          -1

uint8_t init_ext2(int disk);

#endif