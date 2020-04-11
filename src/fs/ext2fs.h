/**
 * TODO
 * 1. Finish ext2 open and read
 * 2. Implement EXT2 FS check
 * 3. Implement generic filesystem functions in lib/file.h
 * 4. Implement writing
 */

#ifndef __FS_EXT2FS_H__
#define __FS_EXT2FS_H__

#include <stdint.h>
#include <stddef.h>
#include <drivers/disk.h>
#include <lib/libc.h>
#include <lib/blib.h>
#include <lib/mbr.h>

/* Drive Format Error Codes */
#define EXT2    0
#define OTHER  -1

/* Error Codes */
#define SUCCESS  0
#define ERROR   -1

struct ext2fs_file_handle {
    uint64_t drive;
    struct mbr_part *part;
    uint64_t inode;
};

extern struct ext2fs_dir_entry **entries;
extern char **entry_names;

uint8_t init_ext2(uint64_t drive, struct mbr_part *part);

// Sample blib read function proto: 
// void bfread(char* buffer, char* filename, char* mode)

// bfgets should only parse the root dir and get the inode number
// ext2fs_read will actually parse the inode and read its contents
struct ext2fs_file_handle *ext2fs_open(uint64_t drive, struct mbr_part *part, uint64_t inode);
uint8_t ext2fs_read(char *buffer, size_t size, struct ext2fs_file_handle *handle);

#endif