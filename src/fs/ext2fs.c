#include <fs/ext2fs.h>

#define DIV_ROUND_UP(a, b) (((a) + (b) - 1) / (b))

// it willl most likely be 4096 bytes
#define EXT2_BLOCK_SIZE 4096

/* EXT2 Filesystem States */
#define EXT2_FS_CLEAN   1
#define EXT2_FS_ERRORS  2

/* EXT2 Error Handling */
#define EXT2_ERR_IGNORE          1
#define EXT2_ERR_REMOUNT_AS_READ 2
#define EXT2_ERR_PANIC           3

/* EXT2 Creator OS IDs */
#define EXT2_LINUX           0
#define EXT2_GNU_HURD        1
#define EXT2_MASIX           2
#define EXT2_FREEBSD         3
#define EXT2_BSD_DERIVATIVE  4

/* EXT2 Optional Feature Flags */
#define EXT2_OPT_PREALLOC        0x0001  // Prealloc x number of blocks (superblock byte 205)
#define EXT2_OPT_AFS_INODES      0x0002  // AFS server inodes exist
#define EXT2_OPT_JOURNAL         0x0004  // FS has a journal (ext3)
#define EXT2_OPT_INODE_EXT_ATTR  0x0008  // Inodes have extended attributes
#define EXT2_OPT_FS_RESIZE       0x0010  // FS can resize itself for larger partitions
#define EXT2_OPT_DIR_HASH_IDX    0x0020  // Directories use a hash index

/* EXT2 Required Feature Flags */
#define EXT2_REQ_COMPRESSION     0x0001  // the FS uses compression
#define EXT2_REQ_DIR_TYPE_FIELD  0x0002  // Dir entries contain a type field
#define EXT2_REQ_JOURNAL_REPLAY  0x0004  // FS needs to replay its journal
#define EXT2_REQ_USE_JOURNAL     0x0008  // FS uses a journal device

/* EXT2 Read-Only Feature Flags */
#define EXT2_SPARSE           0x0001  // Sparse superblocks and group descriptor tables
#define EXT2_FS_LONG          0x0002  // FS uses 64 bit file sizes
#define EXT2_BTREE            0x0004  // Directory contents are stored in a Binary Tree

// https://wiki.osdev.org/Ext2#Superblock
// the superblock starts at byte 1024 and occupies 1024 bytes
// the size of each block is located at byte 24 of the superblock

/* Superblock Fields */
struct ext2fs_superblock {
    uint32_t s_inodes_count;        // total number of inodes in the system
    uint32_t s_blocks_count;        // total number of blocks in the system
    uint32_t s_r_blocks_count;      // blocks that only the superuser can access
    uint32_t s_free_blocks_count;   // number of free blocks
    uint32_t s_free_inodes_count;   // number of free inodes
    uint32_t s_first_data_block;    // block number of block that contains superblock
    uint32_t s_log_block_size;      // [log2(blocksize) - 10] shift left 1024 to get block size
    uint32_t s_log_frag_size;       // [log2(fragsize) - 10] sift left 1024 to get fragment size
    uint32_t s_blocks_per_group;    // number of blocks per block group
    uint32_t s_frags_per_group;     // number of fragments per block group
    uint32_t s_inodes_per_group;    // number of inodes per block group
    uint32_t s_mtime;               // Last mount time
    uint32_t s_wtime;               // Last write time

    uint16_t s_mnt_count;           // number of times the volume was mounted before last consistency check
    uint16_t s_max_mnt_count;       // number of times the drive can be mounted before a check
    uint16_t s_magic;               // 0xEF53 | used to confirm ext2 presence
    uint16_t s_state;               // state of the filesystem
    uint16_t s_errors;              // what to do incase of an error
    uint16_t s_minor_rev_level;     // combine with major portion to get full version
    
    uint32_t s_lastcheck;           // timestamp of last consistency check
    uint32_t s_checkinterval;       // amount of time between required consistency checks
    uint32_t s_creator_os;          // operating system ID
    uint32_t s_rev_level;           // combine with minor portion to get full version
    uint32_t s_def_resuid;          // User ID that can use reserved blocks
    uint32_t s_def_gid;             // Group ID that can use reserved blocks

    // if version number >= 1, we have to use the ext2 extended superblock as well

    /* Extended Superblock */
    uint32_t s_first_ino;           // first non reserved inode in the fs (fixed to 11 when version < 1)

    uint16_t s_inode_size;          // size of each inode (in bytes) (fixed to 128 when version < 1)
    uint16_t s_block_group_nr;      // block group this superblock is part of

    uint32_t s_feature_compat;      // if optional features are present
    uint32_t s_feature_incompat;    // if required features are present
    uint32_t s_feature_ro_compat;   // features that are unsupported (make FS readonly)

    uint64_t s_uuid[2];             // FS ID
    uint64_t s_volume_name[2];      // Volume Name

    uint64_t s_last_mounted[8];     // last path the volume was mounted to (C-style string)

    uint32_t s_algo_bitmap;         // Compression algorithm used

    uint8_t s_prealloc_blocks;      // Number of blocks to preallocate for files
    uint8_t s_prealloc_dir_blocks;  // Number of blocks to preallocate for directories

    uint16_t unused;                // Unused

    uint64_t s_journal_uuid;        // Journal ID

    uint32_t s_journal_inum;        // Journal Inode number
    uint32_t s_journal_dev;         // Journal device
    uint32_t s_last_orphan;         // Head of orphan inode list
    uint32_t s_hash_seed[4];        // Seeds used for hashing algo for dir indexing
    
    uint8_t s_def_hash_version;     // Default hash versrion used for dir indexing

    uint32_t s_default_mnt_opts;    // Default mount options
    uint32_t s_first_meta_bg;       // Block group ID for first meta group

    /* UNUSED */
} __attribute__((packed));

/* EXT2 Block Group Descriptor */
struct ext2fs_bgd {
    uint32_t bg_block_bitmap;       // Block address of block usage bitmap
    uint32_t bg_inode_bitmap;       // Block address of inode usage bitmap
    uint32_t bg_inode_table;        // Starting block address of inode table

    uint16_t bg_free_blocks_count;  // Number of unallocated blocks in group
    uint16_t bg_free_inodes_count;  // Number of unallocated blocks in inode
    uint16_t bg_dirs_count;         // Number of directories in group
    
    uint16_t reserved[7];
} __attribute__((packed));

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
    uint32_t i_blocks[12];      // Block Pointers
    uint32_t i_s_block_ptr;     // Singly Indirect Block Pointer
    uint32_t i_d_blocK_ptr;     // Doubly Indirect Block Pointer
    uint32_t i_t_block_ptr;     // Triply Indirect Block Pointer
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

    /* NAME */
} __attribute__((packed));

struct ext2fs_superblock *superblock;

// attempts to initialize the ext2 filesystem
uint8_t init_ext2(uint64_t drive, struct mbr_part *part) {
    uint64_t base = part->first_sect * 512;
    superblock = balloc(1024);
    read(drive, superblock, base + 1024, 1024);

    if (superblock->s_magic == 0xEF53) {
        uint64_t superblock_base = base + 1024;
        print("   Found Superblock at %u!\n", superblock_base);
        
        uint64_t bgdt_loc = base + EXT2_BLOCK_SIZE;
        
        struct ext2fs_bgd *root_descriptor = balloc(sizeof(struct ext2fs_bgd));
        read(drive, root_descriptor, bgdt_loc, sizeof(struct ext2fs_bgd));

        uint64_t root_index = 1 % superblock->s_inodes_per_group;
        uint64_t root_inode_block = (root_index * 128) / EXT2_BLOCK_SIZE;

        struct ext2fs_inode *root_inode = balloc(sizeof(struct ext2fs_inode));
        read(drive, root_inode, base + (root_descriptor->bg_inode_table * EXT2_BLOCK_SIZE) + sizeof(struct ext2fs_inode), sizeof(struct ext2fs_inode));

        struct ext2fs_dir_entry *root_dir_entry = balloc(sizeof(struct ext2fs_dir_entry));

        // first we read the length of the name
        uint32_t tmp = 0;
        read(drive, &tmp, root_inode->i_blocks[0] * EXT2_BLOCK_SIZE + 6, 1);

        print("   Name Length: %u\n", tmp);

        return EXT2;
    }

    return OTHER;
}