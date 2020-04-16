#ifndef __FS_EXT2FS_H__
#define __FS_EXT2FS_H__

#include <stdint.h>
#include <stddef.h>
#include <drivers/disk.h>
#include <lib/libc.h>
#include <lib/blib.h>
#include <lib/part.h>

/* EXT2 OS Specific Value 2 (only Linux support) */
struct ext2fs_linux {
    uint8_t frag_num;           // Number of fragments
    uint8_t frag_size;          // Fragment Size

    uint16_t reserved_16;       // Reserved
    uint16_t user_id_high;      // High 16 bits of 32 bit user_id
    uint16_t group_id_high;     // High 16 bits of 32 bit group_id

    uint32_t reserved_32;       // Reserved
} __attribute__((packed));

/* EXT2 Inode */
struct ext2fs_inode {
    uint16_t i_mode;            // Types and permissions
    uint16_t i_uid;             // User ID

    uint32_t i_size;            // Lower 32 bits of the size (in bytes)
    uint32_t i_atime;           // Time of last access
    uint32_t i_ctime;           // Time of creation
    uint32_t i_mtime;           // Time of last modification
    uint32_t i_dtime;           // Time of last deletion

    uint16_t i_gid;             // Block group ID this inode belongs to
    uint16_t i_links_count;     // Number of directory entries in this inode

    uint32_t i_blocks_count;    // Number of blocks in use by this inode
    uint32_t i_flags;           // Flags for this inode
    uint32_t i_osd1;            // OS specific value #1 (linux support only) (unused)
    uint32_t i_blocks[15];      // Block Pointers
    uint32_t i_generation;      // Generation number

    /* EXT2 v >= 1.0 */
    uint32_t i_eab;             // Extended Attribute Block
    uint32_t i_maj;             // If feature bit set, upper 32 bit of file size. Directory ACL if inode is directory

    /* EXT2 vAll */
    uint32_t i_frag_block;      // Block address of fragment

    struct ext2fs_linux i_osd2; // OS specific value #2 (linux support only)
} __attribute__((packed));

struct ext2fs_file_handle {
    int drive;
    struct part part;
    int size;
    struct ext2fs_inode root_inode;
    struct ext2fs_inode inode;
    uint64_t block_size;
};

int ext2fs_check_signature(int drive, int partition);

int ext2fs_open(struct ext2fs_file_handle *ret, int drive, int partition, const char* filename);
int ext2fs_read(struct ext2fs_file_handle *file, void* buf, uint64_t loc, uint64_t count);

#endif
