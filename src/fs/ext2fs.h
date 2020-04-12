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

/* Error Codes */
#define SUCCESS  0
#define ERROR   -1

/* EXT2 Directory File Types */
#define EXT2_FT_UNKNOWN  0  // Unknown
#define EXT2_FT_FILE     1  // Regular file
#define EXT2_FT_DIR      2  // Directory
#define EXT2_FT_CHRDEV   3  // Character Device
#define EXT2_FT_BLKDEV   4  // Block Device
#define EXT2_FT_FIFO     5  // FIFO
#define EXT2_FT_SOCKET   6  // Unix Socket
#define EXT2_FT_SYMLINK  7  // Symbolic Link

/* EXT2 Directory Entry */
struct ext2fs_dir_entry {
    uint32_t inode;     // Inode number of file entry
    uint16_t rec_len;   // Displacement to next directory entry from start of current one
    uint8_t name_len;   // Length of the name
    uint8_t type;       // File type

    /* NAME */
} __attribute__((packed));

struct ext2fs_file_handle {
    uint64_t drive;
    struct mbr_part part;
    uint64_t inode;
};

extern uint64_t num_entries;
extern struct ext2fs_dir_entry **entries;
extern char **entry_names;

void init_ext2(uint64_t drive, struct mbr_part part);
int is_ext2();
struct ext2fs_inode *ext2fs_get_inode(uint64_t drive, uint64_t base, uint64_t inode);

// Sample blib read function proto: 
// void bfgets(char* buffer, char* filename, char* mode)

// bfgets should only parse the root dir and get the inode number
// ext2fs_read will actually parse the inode and read its contents
struct ext2fs_file_handle *ext2fs_open(uint64_t drive, struct mbr_part part, uint64_t inode);
uint8_t ext2fs_read(void *buffer, uint64_t loc, uint64_t size, struct ext2fs_file_handle *handle);

#endif