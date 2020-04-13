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

/* EXT2 Inode Types */
#define EXT2_INO_FIFO            0x1000
#define EXT2_INO_CHR_DEV         0x2000 // Character device
#define EXT2_INO_DIRECTORY       0x4000
#define EXT2_INO_BLK_DEV         0x6000 // Block device
#define EXT2_INO_FILE            0x8000
#define EXT2_INO_SYMLINK         0xA000
#define EXT2_INO_UNIX_SOCKET     0xC000

/* EXT2 Inode Permissions */
#define EXT2_INO_X_OTHER     0x001
#define EXT2_INO_W_OTHER     0x002
#define EXT2_INO_R_OTHER     0x004
#define EXT2_INO_X_GROUP     0x008
#define EXT2_INO_W_GROUP     0x010
#define EXT2_INO_R_GROUP     0x020
#define EXT2_INO_X_USER      0x040
#define EXT2_INO_W_USER      0x080
#define EXT2_INO_R_USER      0x100
#define EXT2_INO_STICKY      0x200
#define EXT2_INO_S_GRP_ID    0x400   // Set User ID
#define EXT2_INO_S_USR_ID    0x800   // Set Group ID

/* EXT2 Inode Flags */
#define EXT2_INO_SECURE_DELETION     0x00000001  // Secure deletion                      (unused)
#define EXT2_INO_KEEP_COPY           0x00000002  // Keep copy of data upon deleting      (unused)
#define EXT2_INO_FILE_COMPRESSION    0x00000004  // File compression                     (unused)
#define EXT2_INO_SYNC_UPDATES        0x00000008  // Sync updates to disk
#define EXT2_INO_FILE_IMMUTABLE      0x00000010  // File is readonly
#define EXT2_INO_APPEND_ONLY         0x00000020  // Append only
#define EXT2_INO_NO_INCLUDE_DUMP     0x00000040  // File not included in dump command
#define EXT2_INO_NO_UDPATE_LAT       0x00000080  // Dont update the last access time
#define EXT2_INO_HASH_IDX_DIR        0x00010000  // Directory is hash indexed
#define EXT2_INO_AFS_DIR             0x00020000  // Is AFS directory
#define EXT2_INO_JOURNAL_DATA        0x00040000  // Journal File Data

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
} __attribute__((packed));

struct ext2fs_file_handle {
    uint64_t drive;
    struct mbr_part part;
    struct ext2fs_inode *inode;
    uint64_t size;
};

extern uint64_t num_entries;
extern struct ext2fs_dir_entry **entries;
extern char **entry_names;

void init_ext2(uint64_t drive, struct mbr_part part);

int is_ext2();

uint64_t ext2fs_parse_dirent(int drive, struct mbr_part part, char* filename);
struct ext2fs_inode *ext2fs_get_inode(uint64_t drive, uint64_t base, uint64_t inode);

struct ext2fs_file_handle *ext2fs_open(uint64_t drive, struct mbr_part part, uint64_t inode_num);
uint8_t ext2fs_read(void *buffer, uint64_t loc, uint64_t size, struct ext2fs_file_handle *handle);

#endif