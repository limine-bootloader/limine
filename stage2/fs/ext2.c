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
#define EXT2_FEATURE_INCOMPAT_META_BG 0x0010

/* Ext4 flags */
#define EXT4_EXTENTS_FLAG 0x80000

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

    uint32_t compression_info;
    uint8_t prealloc_blocks;
    uint8_t prealloc_dir_blocks;
    uint16_t reserved_gdt_blocks;
    uint8_t journal_uuid[16];
    uint32_t journal_inum;
    uint32_t journal_dev;
    uint32_t last_orphan;
    uint32_t hash_seed[4];
    uint8_t def_hash_version;
    uint8_t jnl_backup_type;
    uint16_t group_desc_size;
    uint32_t default_mount_opts;
    uint32_t first_meta_bg;
    uint32_t mkfs_time;
    uint32_t jnl_blocks[17];
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

struct ext4_bgd {
    uint32_t bg_block_bitmap;
    uint32_t bg_inode_bitmap;
    uint32_t bg_inode_table;

    uint16_t bg_free_blocks_count;
    uint16_t bg_free_inodes_count;
    uint16_t bg_dirs_count;

    uint16_t pad;
    uint32_t reserved[3];
    uint32_t block_id_hi;
    uint32_t inode_id_hi;
    uint32_t inode_table_id_hi;
    uint16_t free_blocks_hi;
    uint16_t free_inodes_hi;
    uint16_t used_dirs_hi;
    uint16_t pad2;
    uint32_t reserved2[3];
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

struct ext4_extent_header {
    uint16_t magic;
    uint16_t entries;
    uint16_t max;
    uint16_t depth;
    uint16_t generation;
} __attribute__((packed));

struct ext4_extent {
    uint32_t block;
    uint16_t len;
    uint16_t start_hi;
    uint32_t start;
} __attribute__((packed));

struct ext4_extent_idx {
    uint32_t block;
    uint32_t leaf;
    uint16_t leaf_hi;
    uint16_t empty;
} __attribute__((packed));

static int inode_read(void *buf, uint64_t loc, uint64_t count,
                      uint64_t block_size, struct ext2_inode *inode,
                      uint64_t drive, struct part *part);

// parse an inode given the partition base and inode number
static int ext2_get_inode(struct ext2_inode *ret, uint64_t drive, struct part *part,
                          uint64_t inode, struct ext2_superblock *sb) {
    if (inode == 0)
        return -1;

    //determine if we need to use 64 bit inode ids
    bool bit64 = false;
    if (sb->s_rev_level != 0
        && (sb->s_feature_incompat & (EXT2_IF_64BIT))
        && sb->group_desc_size != 0
        && ((sb->group_desc_size & (sb->group_desc_size - 1)) == 0)) {
                if(sb->group_desc_size > 32) {
                    bit64 = true;
                }
            }

    const uint64_t ino_blk_grp = (inode - 1) / sb->s_inodes_per_group;
    const uint64_t ino_tbl_idx = (inode - 1) % sb->s_inodes_per_group;

    const uint64_t block_size = ((uint64_t)1024 << sb->s_log_block_size);
    uint64_t ino_offset;
    const uint64_t bgd_start_offset = block_size >= 2048 ? block_size : block_size * 2;
    const uint64_t ino_size = sb->s_rev_level == 0 ? sizeof(struct ext2_inode) : sb->s_inode_size;

    if (!bit64) {
        struct ext2_bgd target_descriptor;
        const uint64_t bgd_offset = bgd_start_offset + (sizeof(struct ext2_bgd) * ino_blk_grp);

        read_partition(drive, part, &target_descriptor, bgd_offset, sizeof(struct ext2_bgd));

        ino_offset = ((target_descriptor.bg_inode_table) * block_size) +
                                    (ino_size * ino_tbl_idx);
    } else {
        struct ext4_bgd target_descriptor;
        const uint64_t bgd_offset = bgd_start_offset + (sizeof(struct ext4_bgd) * ino_blk_grp);

        read_partition(drive, part, &target_descriptor, bgd_offset, sizeof(struct ext4_bgd));

        ino_offset = ((target_descriptor.bg_inode_table | (bit64 ? ((uint64_t)target_descriptor.inode_id_hi << 32) : 0)) * block_size) +
                                    (ino_size * ino_tbl_idx);
    }

    read_partition(drive, part, ret, ino_offset, sizeof(struct ext2_inode));

    return 0;
}

static int ext2_parse_dirent(struct ext2_dir_entry *dir, struct ext2_file_handle *fd, struct ext2_superblock *sb, const char *path) {
    if (*path == '/')
        path++;

    struct ext2_inode current_inode = fd->root_inode;

    bool escape = false;

    char token[256] = {0};

next:
    for (size_t i = 0; *path != '/' && *path != '\0'; i++, path++)
        token[i] = *path;

    if (*path == '\0')
        escape = true;
    else
        path++;

    for (uint32_t i = 0; i < current_inode.i_size; ) {
        // preliminary read
        inode_read(dir, i, sizeof(struct ext2_dir_entry),
                   fd->block_size, &current_inode,
                   fd->drive, &fd->part);

        // name read
        char name[dir->name_len + 1];

        memset(name, 0, dir->name_len + 1);
        inode_read(name, i + sizeof(struct ext2_dir_entry), dir->name_len,
                   fd->block_size, &current_inode, fd->drive, &fd->part);

        int r = strcmp(token, name);

        if (!r) {
            if (escape) {
                return 0;
            } else {
                // update the current inode
                ext2_get_inode(&current_inode, fd->drive, &fd->part, dir->inode, sb);
                goto next;
            }
        }

        i += dir->rec_len;
    }

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

static struct ext4_extent_header* ext4_find_leaf(struct ext4_extent_header* ext_block, uint32_t read_block, uint64_t block_size, uint64_t drive, struct part *part) {
    struct ext4_extent_idx* index;
    void* buf = NULL;

    while (1) {
        index = (struct ext4_extent_idx*)((size_t)ext_block + 12);

        #define EXT4_EXT_MAGIC 0xf30a
        if (ext_block->magic != EXT4_EXT_MAGIC)
            panic("invalid extent magic");

        if (ext_block->depth == 0) {
            return ext_block;
        }

        int i;
        for (i = 0; i < ext_block->entries; i++) {
            if(read_block < index[i].block)
                break;
        }

        if (--i < 0)
            panic("extent not found");

        uint64_t block = ((uint64_t)index[i].leaf_hi << 32) | index[i].leaf;
        if(!buf)
            buf = conv_mem_alloc(block_size);
        read_partition(drive, part, buf, (block * block_size), block_size);
        ext_block = buf;
    }
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

        if (inode->i_flags & EXT4_EXTENTS_FLAG) {
            struct ext4_extent_header *leaf;
            struct ext4_extent *ext;
            int i;

            leaf = ext4_find_leaf((struct ext4_extent_header*)inode->i_blocks, block, block_size, drive, part);

            if (!leaf)
                panic("invalid extent");
            ext = (struct ext4_extent*)((size_t)leaf + 12);

            for (i = 0; i < leaf->entries; i++) {
                if (block < ext[i].block) {
                    break;
                }
            }

            if (--i >= 0) {
                block -= ext[i].block;
                if (block >= ext[i].len) {
                    panic("block longer than extent");
                } else {
                    uint64_t start = ((uint64_t)ext[i].start_hi << 32) + ext[i].start;
                    block_index = start + block;
                }
            } else {
                panic("extent for block not found");
            }
        } else if (block < 12) {
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
int ext2_check_signature(struct part *part) {
    struct ext2_superblock sb;
    read_partition(part->drive, part, &sb, 1024, sizeof(struct ext2_superblock));

    if (sb.s_magic != EXT2_S_MAGIC)
        return 0;

    // If the revision level is 0, we can't test for features.
    if (sb.s_rev_level == 0)
        return 1;

    if (sb.s_feature_incompat & EXT2_IF_COMPRESSION ||
        sb.s_feature_incompat & EXT2_IF_INLINE_DATA ||
        sb.s_feature_incompat & EXT2_FEATURE_INCOMPAT_META_BG ||
        sb.s_feature_incompat & EXT2_IF_ENCRYPT)
        panic("EXT2: filesystem has unsupported features %x", sb.s_feature_incompat);

    return 1;
}

bool ext2_get_guid(struct guid *guid, struct part *part) {
    struct ext2_superblock sb;
    read_partition(part->drive, part, &sb, 1024, sizeof(struct ext2_superblock));

    if (sb.s_magic != EXT2_S_MAGIC)
        return false;

    ((uint64_t *)guid)[0] = sb.s_uuid[0];
    ((uint64_t *)guid)[1] = sb.s_uuid[1];

    return true;
}
