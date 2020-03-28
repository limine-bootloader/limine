#include <fs/ext2fs.h>
#include <stdint.h>
#include <stddef.h>
#include <drivers/disk.h>
#include <lib/libc.h>
#include <lib/blib.h>

#define DIV_ROUND_UP(a, b) (((a) + (b) - 1) / (b))

/* EXT2 Filesystem States */
#define FS_CLEAN   1
#define FS_ERRORS  2

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

struct ext2fs_bgdt {
    uint32_t block_bitmap_addr;     // Block address of block usage bitmap
    uint32_t inode_bitmap_addr;     // Block address of inode usage bitmap
    uint32_t itable_block_addr;     // Starting block address of inode table

    uint16_t num_free_blocks;       // Number of unallocated blocks in group
    uint16_t num_free_inodes;       // Number of unallocated blocks in inode
    uint16_t num_dirs;              // Number of directories in group

    /* 18 - 31 UNUSED*/
} __attribute__((packed));

/* EXT2 Inode Types */
#define FIFO            0x1000
#define CHAR_DEVICE     0x2000  // Character device
#define DIRECTORY       0x4000
#define BLOCK_DEVICE    0x6000
#define FILE            0x8000
#define SYMLINK         0xA000
#define UNIX_SOCKET     0xC000

/* EXT2 Inode Permissions */
#define X_OTHER     0x001
#define W_OTHER     0x002
#define R_OTHER     0x004
#define X_GROUP     0x008
#define W_GROUP     0x010
#define R_GROUP     0x020
#define X_USER      0x040
#define W_USER      0x080
#define R_USER      0x100
#define STICKY      0x200
#define S_GRP_ID    0x400   // Set User ID
#define S_USR_ID    0x800   // Set Group ID

/* EXT2 Inode Flags */
#define SECURE_DELETION     0x00000001  // Secure deletion                      (unused)
#define KEEP_COPY           0x00000002  // Keep copy of data upon deleting      (unused)
#define FILE_COMPRESSION    0x00000004  // File compression                     (unused)
#define SYNC_UPDATES        0x00000008  // Sync updates to disk
#define FILE_IMMUTABLE      0x00000010  // File is readonly
#define APPEND_ONLY         0x00000020  // Append only
#define NO_INCLUDE_DUMP     0x00000040  // File not included in dump command
#define NO_UDPATE_LAT       0x00000080  // Dont update the last access time
#define HASH_IDX_DIR        0x00010000  // Directory is hash indexed
#define AFS_DIR             0x00020000  // Is AFS directory
#define JOURNAL_DATA        0x00040000  // Journal File Data

/* EXT2 OS Specific Value 2 (only Linux support) */
struct ext2fs_linux {
    uint8_t frag_num;           // Number of fragments
    uint8_t frag_size;          // Fragment Size

    uint16_t reserved_16;       // Reserved
    uint16_t user_id_high;      // High 16 bits of 32 bit user_id
    uint16_t group_id_high;     // High 16 bits of 32 bit group_id

    uint32_t reserved_32;       // Reserved
} __attribute__((packed));

struct ext2fs_inode {
    uint16_t permssions;                // Types and permissions
    uint16_t user_id;                   // User ID

    uint32_t size_lower;                // Lower 32 bits of the size (in bytes)
    uint32_t last_access_time;          // Time of last access
    uint32_t creation_time;             // Time of creation
    uint32_t last_modification_time;    // Time of last modification
    uint32_t deletion_time;             // Time of last deletion
    
    uint16_t group_id;                  // Block group ID this inode belongs to
    uint16_t num_hard_links;            // Number of directory entries in this inode

    uint32_t used_sectors;              // Number of sectors in use by this inode
    uint32_t flags;                     // Flags for this inode
    uint32_t os_val_1;                  // OS specific value #1 (linux support only) (unused)

    uint32_t block_ptr[12];             // Block Pointers

    uint32_t singly_indr_block_ptr;     // Singly Indirect Block Pointer
    uint32_t doubly_indr_block_ptr;     // Doubly Indirect Block Pointer
    uint32_t triply_indr_block_ptr;     // Triply Indirect Block Pointer

    uint32_t gen_number;                // Generation number
    
    /* EXT2 v >= 1.0 */
    uint32_t eab;                       // Extended Attribute Block
    uint32_t major;                     // If feature bit set, upper 32 bit of file size. Directory ACL if inode is directory

    /* EXT2 vAll */
    uint32_t frag_block_addr;           // Block address of fragment

    struct ext2fs_linux os_val_2;         // OS specific value #2 (linux support only)
} __attribute__((packed));

struct ext2fs_superblock *superblock;
struct ext2fs_bgdt *bgdt;

// attempts to initialize the ext2 filesystem
uint8_t init_ext2(int drive) {
    superblock = balloc(1024);
    read(drive, superblock, 1024, 1024);

    if (superblock->signature == 0xEF53) {
        uint64_t num_block_groups = DIV_ROUND_UP(superblock->block_num / superblock->blocks_per_group, 10);
        
        uint8_t bgdt_block_num = 1; // Block that contains the Block Group Descriptor Table

        if (superblock->block_size >= 1024)
            bgdt_block_num = 2;

        // addr (in bytes) is block_num * block_size
        bgdt = balloc(32);
        read(drive, bgdt, bgdt_block_num * superblock->block_size, superblock->block_size);

        print("Inode Table Addr: %d\n", bgdt->itable_block_addr);

        return EXT2;
    }

    return OTHER;
}