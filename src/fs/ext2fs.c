#include <fs/ext2fs.h>

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

#define EXT2_S_MAGIC    0xEF53

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

struct ext2fs_superblock *superblock;

uint64_t num_entries = 0;
struct ext2fs_dir_entry **entries;
char **entry_names;

// parse an inode given the partition base and inode number
struct ext2fs_inode *ext2fs_get_inode(uint64_t drive, uint64_t base, uint64_t inode) {
    uint64_t bgdt_loc = base + EXT2_BLOCK_SIZE;
    
    uint64_t ino_blk_grp = (inode - 1) / superblock->s_inodes_per_group;
    uint64_t ino_tbl_idx = (inode - 1) % superblock->s_inodes_per_group;

    struct ext2fs_bgd *target_descriptor = balloc(sizeof(struct ext2fs_bgd));
    read(drive, target_descriptor, bgdt_loc + (sizeof(struct ext2fs_bgd) * ino_blk_grp), sizeof(struct ext2fs_bgd));

    struct ext2fs_inode *target = balloc(sizeof(struct ext2fs_inode));
    read(drive, target, base + (target_descriptor->bg_inode_table * EXT2_BLOCK_SIZE) + (sizeof(struct ext2fs_inode) * ino_tbl_idx), sizeof(struct ext2fs_inode));

    return target;
}

uint64_t ext2fs_parse_dirent(int drive, struct mbr_part part, char* filename) {
    for (uint64_t i = 0; i < num_entries; i++) {
        if (strncmp(entry_names[i], filename, entries[i]->name_len) == 0) {
            return entries[i]->inode;
        }
    }

    return NULL;
}

// attempts to initialize the ext2 filesystem
void init_ext2(uint64_t drive, struct mbr_part part) {
    uint64_t base = part.first_sect * 512;
    superblock = balloc(1024);
    read(drive, superblock, base + 1024, 1024);

    if (superblock->s_magic == EXT2_S_MAGIC) {
        print("   Found!\n");
        
        // grab the root directory entries
        struct ext2fs_inode *root_inode = ext2fs_get_inode(drive, base, 2);

        // directory entry and name storage
        entries = balloc(sizeof(struct ext2fs_dir_entry *) * root_inode->i_links_count);
        entry_names = balloc(sizeof(char *) * root_inode->i_links_count);

        uint64_t offset = base + (root_inode->i_blocks[0] * EXT2_BLOCK_SIZE);

        num_entries = root_inode->i_links_count + 2;
        for (uint32_t i = 0; i < (root_inode->i_links_count + 2); i++) {
            struct ext2fs_dir_entry *dir = balloc(sizeof(struct ext2fs_dir_entry));

            // preliminary read
            read(drive, dir, offset, sizeof(struct ext2fs_dir_entry));

            // name read
            char* name = balloc(sizeof(char) * dir->name_len);
            read(drive, name, offset + sizeof(struct ext2fs_dir_entry), dir->name_len);
            print("      => Name: %s\n", name, dir->inode);

            entries[i] = dir;
            entry_names[i] = name;

            offset += dir->rec_len;
        }

        return;
    } else {
        print("      EXT2FS not found!\n");
        return;
    }
}

int is_ext2() {
    if (superblock->s_magic != EXT2_S_MAGIC) {
        return -1;
    }

    return 0;
}

struct ext2fs_file_handle *ext2fs_open(uint64_t drive, struct mbr_part part, uint64_t inode_num) {
    struct ext2fs_file_handle *handle = balloc(sizeof(struct ext2fs_file_handle));
    handle->drive = drive;
    handle->part = part;
    handle->inode = ext2fs_get_inode(drive, part.first_sect * 512, inode_num);
    handle->size = handle->inode->i_size; // ultra crust

    return handle;
}

uint8_t ext2fs_read(void *buffer, uint64_t loc, uint64_t size, struct ext2fs_file_handle *handle) {
    uint64_t base = handle->part.first_sect * 512;

    // read the contents of the inode
    // it is assumed that bfread has already done the directory check

    // TODO: add support for the indirect block pointers
    // TOOD: add support for reading multiple blocks

    read(handle->drive, buffer, base + (handle->inode->i_blocks[0] * EXT2_BLOCK_SIZE) + loc, size);

    // always returns SUCCESS
    return SUCCESS;
}