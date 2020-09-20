#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <fs/ext2.h>
#include <drivers/disk.h>
#include <lib/libc.h>
#include <lib/blib.h>
#include <mm/pmm.h>

/* EXT2 Filesystem States */
#define EXT2_FS_UNRECOVERABLE_ERRORS 3

/* Ext2 incompatible features */
#define EXT2_IF_COMPRESSION 0x01
#define EXT2_IF_EXTENTS 0x40
#define EXT2_IF_64BIT 0x80
#define EXT2_IF_INLINE_DATA 0x8000
#define EXT2_IF_ENCRYPT 0x10000

#define EXT2_S_MAGIC    0xEF53

/* Superblock Fields */
struct ext2_superblock {
    uint32_t s_inodes_count;
    uint32_t s_blocks_count;
    uint32_t s_r_blocks_count;
    uint32_t s_free_blocks_count;
    uint32_t s_free_inodes_count;
    uint32_t s_first_data_block;
    uint32_t s_log_block_size;
    uint32_t s_log_frag_size;
    uint32_t s_blocks_per_group;
    uint32_t s_frags_per_group;
    uint32_t s_inodes_per_group;
    uint32_t s_mtime;
    uint32_t s_wtime;

    uint16_t s_mnt_count;
    uint16_t s_max_mnt_count;
    uint16_t s_magic;
    uint16_t s_state;
    uint16_t s_errors;
    uint16_t s_minor_rev_level;

    uint32_t s_lastcheck;
    uint32_t s_checkinterval;
    uint32_t s_creator_os;
    uint32_t s_rev_level;
    uint16_t s_def_resuid;
    uint16_t s_def_gid;

    // if version number >= 1, we have to use the ext2 extended superblock as well

    /* Extended Superblock */
    uint32_t s_first_ino;

    uint16_t s_inode_size;
    uint16_t s_block_group_nr;

    uint32_t s_feature_compat;
    uint32_t s_feature_incompat;
    uint32_t s_feature_ro_compat;

    uint64_t s_uuid[2];
    uint64_t s_volume_name[2];

    uint64_t s_last_mounted[8];
} __attribute__((packed));

/* EXT2 Block Group Descriptor */
struct ext2_bgd {
    uint32_t bg_block_bitmap;
    uint32_t bg_inode_bitmap;
    uint32_t bg_inode_table;

    uint16_t bg_free_blocks_count;
    uint16_t bg_free_inodes_count;
    uint16_t bg_dirs_count;

    uint16_t reserved[7];
} __attribute__((packed));

/* EXT2 Inode Types */
#define EXT2_INO_DIRECTORY  0x4000

/* EXT2 Directory Entry */
struct ext2_dir_entry {
    uint32_t inode;
    uint16_t rec_len;
    uint8_t  name_len;
    uint8_t  type;
} __attribute__((packed));

static int inode_read(void *buf, uint64_t loc, uint64_t count,
                      uint64_t block_size, struct ext2_inode *inode,
                      uint64_t drive, struct part *part);

// parse an inode given the partition base and inode number
static int ext2_get_inode(struct ext2_inode *ret, uint64_t drive, struct part *part,
                          uint64_t inode, struct ext2_superblock *sb) {
    if (inode == 0)
        return -1;

    const uint64_t ino_blk_grp = (inode - 1) / sb->s_inodes_per_group;
    const uint64_t ino_tbl_idx = (inode - 1) % sb->s_inodes_per_group;

    const uint64_t block_size = ((uint64_t)1024 << sb->s_log_block_size);

    struct ext2_bgd target_descriptor;

    const uint64_t bgd_start_offset = block_size >= 2048 ? block_size : block_size * 2;
    const uint64_t bgd_offset = bgd_start_offset + (sizeof(struct ext2_bgd) * ino_blk_grp);

    read_partition(drive, part, &target_descriptor, bgd_offset, sizeof(struct ext2_bgd));

    const uint64_t ino_size = sb->s_rev_level == 0 ? sizeof(struct ext2_inode) : sb->s_inode_size;
    const uint64_t ino_offset = (target_descriptor.bg_inode_table * block_size) +
                                (ino_size * ino_tbl_idx);

    read_partition(drive, part, ret, ino_offset, sizeof(struct ext2_inode));

    return 0;
}

static int ext2_parse_dirent(struct ext2_dir_entry *dir, struct ext2_file_handle *fd, struct ext2_superblock *sb, const char *path) {
    if (*path == '/')
        path++;

    struct ext2_inode *current_inode = conv_mem_alloc(sizeof(struct ext2_inode));
    *current_inode = fd->root_inode;

    bool escape = false;

    char token[256] = {0};

next:
    for (size_t i = 0; *path != '/' && *path != '\0'; i++, path++)
        token[i] = *path;

    if (*path == '\0')
        escape = true;
    else
        path++;

    for (uint32_t i = 0; i < current_inode->i_size; ) {
        // preliminary read
        inode_read(dir, i, sizeof(struct ext2_dir_entry),
                   fd->block_size, current_inode,
                   fd->drive, &fd->part);

        // name read
        char *name = conv_mem_alloc(dir->name_len + 1);

        memset(name, 0, dir->name_len + 1);
        inode_read(name, i + sizeof(struct ext2_dir_entry), dir->name_len,
                   fd->block_size, current_inode, fd->drive, &fd->part);

        int r = strcmp(token, name);

        conv_mem_rewind();

        if (!r) {
            if (escape) {
                conv_mem_rewind();
                return 0;
            } else {
                // update the current inode
                ext2_get_inode(current_inode, fd->drive, &fd->part, dir->inode, sb);
                goto next;
            }
        }

        i += dir->rec_len;
    }

    conv_mem_rewind();
    return -1;
}

int ext2_open(struct ext2_file_handle *ret, int drive, int partition, const char *path) {
    if (get_part(&ret->part, drive, partition))
        panic("Invalid partition");

    ret->drive = drive;

    struct ext2_superblock sb;
    read_partition(drive, &ret->part, &sb, 1024, sizeof(struct ext2_superblock));

    if (sb.s_state == EXT2_FS_UNRECOVERABLE_ERRORS)
        panic("EXT2: unrecoverable errors found\n");

    ret->block_size = ((uint64_t)1024 << sb.s_log_block_size);

    ext2_get_inode(&ret->root_inode, drive, &ret->part, 2, &sb);

    struct ext2_dir_entry entry;
    int r = ext2_parse_dirent(&entry, ret, &sb, path);

    if (r)
        return r;

    ext2_get_inode(&ret->inode, drive, &ret->part, entry.inode, &sb);
    ret->size = ret->inode.i_size;

    if ((ret->inode.i_mode & 0xf000) == EXT2_INO_DIRECTORY)
        panic("ext2: Requested file \"%s\" is a directory!", path);

    return 0;
}

int ext2_read(struct ext2_file_handle *file, void *buf, uint64_t loc, uint64_t count) {
    return inode_read(buf, loc, count, file->block_size, &file->inode,
                      file->drive, &file->part);
}

static int inode_read(void *buf, uint64_t loc, uint64_t count,
                      uint64_t block_size, struct ext2_inode *inode,
                      uint64_t drive, struct part *part) {
    for (uint64_t progress = 0; progress < count;) {
        uint64_t block = (loc + progress) / block_size;

        uint64_t chunk = count - progress;
        uint64_t offset = (loc + progress) % block_size;
        if (chunk > block_size - offset)
            chunk = block_size - offset;

        uint32_t block_index;

        if (block < 12) {
            // Direct block
            block_index = inode->i_blocks[block];
        } else {
            // Indirect block
            block -= 12;
            if (block * sizeof(uint32_t) >= block_size) {
                // Double indirect block
                block -= block_size / sizeof(uint32_t);
                uint32_t index  = block / (block_size / sizeof(uint32_t));
                if (index * sizeof(uint32_t) >= block_size) {
                    // Triple indirect block
                    panic("ext2: triply indirect blocks unsupported");
                }
                uint32_t offset = block % (block_size / sizeof(uint32_t));
                uint32_t indirect_block;
                read_partition(
                    drive, part, &indirect_block,
                    inode->i_blocks[13] * block_size + index * sizeof(uint32_t),
                    sizeof(uint32_t)
                );
                read_partition(
                    drive, part, &block_index,
                    indirect_block * block_size + offset * sizeof(uint32_t),
                    sizeof(uint32_t)
                );
            } else {
                read_partition(
                    drive, part, &block_index,
                    inode->i_blocks[12] * block_size + block * sizeof(uint32_t),
                    sizeof(uint32_t)
                );
            }
        }

        read_partition(drive, part, buf + progress, (block_index * block_size) + offset, chunk);

        progress += chunk;
    }

    return 0;
}

// attempts to initialize the ext2 filesystem
// and checks if all features are supported
int ext2_check_signature(int drive, int partition) {
    struct part part;
    if (get_part(&part, drive, partition))
        panic("Invalid partition");

    struct ext2_superblock sb;
    read_partition(drive, &part, &sb, 1024, sizeof(struct ext2_superblock));

    if (sb.s_magic != EXT2_S_MAGIC)
        return 0;

    // If the revision level is 0, we can't test for features.
    if (sb.s_rev_level == 0)
        return 1;

    if (sb.s_feature_incompat & EXT2_IF_COMPRESSION ||
        sb.s_feature_incompat & EXT2_IF_EXTENTS ||
        sb.s_feature_incompat & EXT2_IF_INLINE_DATA ||
        sb.s_feature_incompat & EXT2_IF_64BIT ||
        sb.s_feature_incompat & EXT2_IF_ENCRYPT)
        panic("EXT2: filesystem has unsupported features");

    return 1;
}
