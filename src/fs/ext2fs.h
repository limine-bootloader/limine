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

/* EXT2 Filesystem States */
#define CLEAN           1
#define ERRORS          2

/* EXT2 Error Handling */
#define IGNORE          1
#define REMOUNT_AS_READ 2
#define PANIC           3

/* EXT2 Creator OS IDs */
#define LINUX           0
#define GNU_HURD        1
#define MASIX           2
#define FREEBSD         3
#define BSD_DERIVATIVE  4

/* EXT2 Optional Feature Flags */
#define PREALLOC        0x0001  // Prealloc x number of blocks (superblock byte 205)
#define AFS_INODES      0x0002  // AFS server inodes exist
#define JOURNAL         0x0004  // FS has a journal (ext3)
#define INODE_EXT_ATTR  0x0008  // Inodes have extended attributes
#define FS_RESIZE       0x0010  // FS can resize itself for larger partitions
#define DIR_HASH_IDX    0x0020  // Directories use a hash index

/* EXT2 Required Feature Flags */
#define COMPRESSION     0x0001  // the FS uses compression
#define DIR_TYPE_FIELD  0x0002  // Dir entries contain a type field
#define JOURNAL_REPLAY  0x0004  // FS needs to replay its journal
#define USE_JOURNAL     0x0008  // FS uses a journal device

/* EXT2 Read-Only Feature Flags */
#define SPARSE           0x0001  // Sparse superblocks and group descriptor tables
#define FS_LONG          0x0002  // FS uses 64 bit file sizes
#define BTREE            0x0004  // Directory contents are stored in a Binary Tree

// https://wiki.osdev.org/Ext2#Superblock
// the superblock starts at byte 1024 and occupies 1024 bytes
// the size of each block is located at byte 24 of the superblock

/* Superblock Fields */
struct ext2fs_superblock {
    uint32_t inode_num;                     // total number of inodes in the system
    uint32_t block_num;                     // total number of blocks in the system
    uint32_t reserved_blocks;               // blocks that only the superuser can access
    uint32_t free_block_num;                // number of free blocks
    uint32_t free_inode_num;                // number of free inodes
    uint32_t superblock_block;              // block number of block that contains superblock
    uint32_t block_size;                    // [log2(blocksize) - 10] shift left 1024 to get block size
    uint32_t frag_size;                     // [log2(fragsize) - 10] sift left 1024 to get fragment size
    uint32_t blocks_per_group;              // number of blocks per block group
    uint32_t frags_per_group;               // number of fragments per block group
    uint32_t inodes_per_group;              // number of inodes per block group
    uint32_t last_mount_time;               // Last mount time
    uint32_t last_write_time;               // Last write time

    uint16_t times_mounted_before_check;    // number of times the volume was mounted before last consistency check
    uint16_t mounts_allowed_after_check;    // number of times the drive can be mounted before a check
    uint16_t signature;                     // 0xEF53 | used to confirm ext2 presence
    uint16_t fs_state;                      // state of the filesystem
    uint16_t error_action;                  // what to do incase of an error
    uint16_t version_minor;                 // combine with major portion to get full version
    
    uint32_t last_check_time;               // timestamp of last consistency check
    uint32_t forced_check_interval;         // amount of time between required consistency checks
    uint32_t os_id;                         // operating system ID
    uint32_t version_major;                 // combine with minor portion to get full version
    uint32_t user_id;                       // User ID that can use reserved blocks
    uint32_t group_id;                      // Group ID that can use reserved blocks

    // if version number >= 1, we have to use the ext2 extended superblock as well

    /* Extended Superblock */
    uint32_t first_non_reserved_inode;      // first non reserved inode in the fs (fixed to 11 when version < 1)

    uint16_t inode_size;                    // size of each inode (in bytes) (fixed to 128 when version < 1)
    uint16_t block_group;                   // block group this superblock is part of

    uint32_t optional_features_present;     // if optional features are present
    uint32_t required_features_present;     // if required features are present
    uint32_t unsupported_features;          // features that are unsupported (make FS readonly)

    uint64_t fs_id[2];                      // FS ID
    uint64_t volume_name[2];                // Volume Name

    uint64_t last_mount_path[8];            // last path the volume was mounted to (C-style string)

    uint32_t compression_alg;               // Compression algorithm used

    uint8_t num_file_block_prealloc;        // Number of blocks to preallocate for files
    uint8_t num_dir_block_prealloc;         // Number of blocks to preallocate for directories

    uint16_t unused;                        // Unused

    uint64_t journal_id;                    // Journal ID

    uint32_t journal_inode;                 // Journal Inode number
    uint32_t journal_device;                // Journal device
    uint32_t orphan_inode_head;             // Head of orphan inode list

    /* 236 - 1023 UNUSED */
} __attribute__((packed));

struct ext2_block_group {
    uint32_t block_bitmap_addr;             // Block address of block usage bitmap
    uint32_t inode_bitmap_addr;             // Block address of inode usage bitmap
    uint32_t init_block_addr;               // Starting block address of inode table

    uint16_t num_free_blocks;               // Number of unallocated blocks in group
    uint16_t num_free_inodes;               // Number of unallocated blocks in inode
    uint16_t num_dirs;                      // Number of directories in group

    /* 18 - 31 UNUSED*/
}


/* Drive Format Error Codes */
#define EXT2            0
#define OTHER          -1
#define EXT2_READONLY  -2

uint8_t initEXT2(int disk);

#endif