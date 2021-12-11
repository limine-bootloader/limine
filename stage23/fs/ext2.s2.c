#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <fs/ext2.h>
#include <drivers/disk.h>
#include <lib/libc.h>
#include <lib/blib.h>
#include <lib/print.h>
#include <mm/pmm.h>

/* Inode types */
#define S_IFIFO  0x1000
#define S_IFCHR  0x2000
#define S_IFDIR  0x4000
#define S_IFBLK  0x6000
#define S_IFREG  0x8000
#define S_IFLNK  0xa000
#define S_IFSOCK 0xc000

#define FMT_MASK 0xf000

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
                      struct ext2_inode *inode, struct ext2_file_handle *fd,
                      uint32_t *alloc_map);
static bool ext2_parse_dirent(struct ext2_dir_entry *dir, struct ext2_file_handle *fd, const char *path);

// parse an inode given the partition base and inode number
static bool ext2_get_inode(struct ext2_inode *ret,
                          struct ext2_file_handle *fd, uint64_t inode) {
    if (inode == 0)
        return false;

    struct ext2_superblock *sb = &fd->sb;

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

        volume_read(fd->part, &target_descriptor, bgd_offset, sizeof(struct ext2_bgd));

        ino_offset = ((target_descriptor.bg_inode_table) * block_size) +
                                    (ino_size * ino_tbl_idx);
    } else {
        struct ext4_bgd target_descriptor;
        const uint64_t bgd_offset = bgd_start_offset + (sizeof(struct ext4_bgd) * ino_blk_grp);

        volume_read(fd->part, &target_descriptor, bgd_offset, sizeof(struct ext4_bgd));

        ino_offset = ((target_descriptor.bg_inode_table | (bit64 ? ((uint64_t)target_descriptor.inode_id_hi << 32) : 0)) * block_size) +
                                    (ino_size * ino_tbl_idx);
    }

    volume_read(fd->part, ret, ino_offset, sizeof(struct ext2_inode));

    return true;
}

static uint32_t *create_alloc_map(struct ext2_file_handle *fd,
                                  struct ext2_inode *inode) {
    if (inode->i_flags & EXT4_EXTENTS_FLAG)
        return NULL;

    size_t entries_per_block = fd->block_size / sizeof(uint32_t);

    // Cache the map of blocks
    uint32_t *alloc_map = ext_mem_alloc(inode->i_blocks_count * sizeof(uint32_t));
    for (uint32_t i = 0; i < inode->i_blocks_count; i++) {
        uint32_t block = i;
        if (block < 12) {
            // Direct block
            alloc_map[i] = inode->i_blocks[block];
        } else {
            // Indirect block
            block -= 12;
            if (block >= entries_per_block) {
                // Double indirect block
                block -= entries_per_block;
                uint32_t index = block / entries_per_block;
                uint32_t indirect_block;
                if (index >= entries_per_block) {
                    uint32_t first_index = index / entries_per_block;
                    uint32_t first_indirect_block;
                    volume_read(
                        fd->part, &first_indirect_block,
                        inode->i_blocks[14] * fd->block_size + first_index * sizeof(uint32_t),
                        sizeof(uint32_t)
                    );
                    uint32_t second_index = index % entries_per_block;
                    volume_read(
                        fd->part, &indirect_block,
                        first_indirect_block * fd->block_size + second_index * sizeof(uint32_t),
                        sizeof(uint32_t)
                    );
                } else {
                    volume_read(
                        fd->part, &indirect_block,
                        inode->i_blocks[13] * fd->block_size + index * sizeof(uint32_t),
                        sizeof(uint32_t)
                    );
                }
                for (uint32_t j = 0; j < entries_per_block; j++) {
                    if (i + j >= inode->i_blocks_count)
                        return alloc_map;
                    volume_read(
                        fd->part, &alloc_map[i + j],
                        indirect_block * fd->block_size + j * sizeof(uint32_t),
                        sizeof(uint32_t)
                    );
                }
                i += entries_per_block - 1;
            } else {
                // Single indirect block
                volume_read(
                    fd->part, &alloc_map[i],
                    inode->i_blocks[12] * fd->block_size + block * sizeof(uint32_t),
                    sizeof(uint32_t)
                );
            }
        }
    }

    return alloc_map;
}

static bool symlink_to_inode(struct ext2_inode *inode, struct ext2_file_handle *fd) {
    // I cannot find whether this is 0-terminated or not, so I'm gonna take the
    // safe route here and assume it is not.
    if (inode->i_size < 59) {
        struct ext2_dir_entry dir;
        char *symlink = (char *)inode->i_blocks;
        symlink[59] = 0;
        if (!ext2_parse_dirent(&dir, fd, symlink))
            return false;
        ext2_get_inode(inode, fd, dir.inode);
        return true;
    } else {
        print("ext2: Symlinks with destination paths longer than 60 chars unsupported\n");
        return false;
    }
}

static bool ext2_parse_dirent(struct ext2_dir_entry *dir, struct ext2_file_handle *fd, const char *path) {
    if (*path == '/')
        path++;

    struct ext2_inode current_inode = fd->root_inode;

    bool escape = false;
    static char token[256];

    bool ret;

next:
    memset(token, 0, 256);

    for (size_t i = 0; i < 255 && *path != '/' && *path != '\0'; i++, path++)
        token[i] = *path;

    if (*path == '\0')
        escape = true;
    else
        path++;

    uint32_t *alloc_map = create_alloc_map(fd, &current_inode);

    for (uint32_t i = 0; i < current_inode.i_size; ) {
        // preliminary read
        inode_read(dir, i, sizeof(struct ext2_dir_entry),
                   &current_inode, fd, alloc_map);

        // name read
        char name[dir->name_len + 1];

        memset(name, 0, dir->name_len + 1);
        inode_read(name, i + sizeof(struct ext2_dir_entry), dir->name_len,
                   &current_inode, fd, alloc_map);

        if (!strcmp(token, name)) {
            if (escape) {
                ret = true;
                goto out;
            } else {
                // update the current inode
                ext2_get_inode(&current_inode, fd, dir->inode);
                while ((current_inode.i_mode & FMT_MASK) != S_IFDIR) {
                    if ((current_inode.i_mode & FMT_MASK) == S_IFLNK) {
                        if (!symlink_to_inode(&current_inode, fd)) {
                            ret = false;
                            goto out;
                        }
                    } else {
                        print("ext2: Part of path is not directory nor symlink\n");
                        ret = false;
                        goto out;
                    }
                }
                pmm_free(alloc_map, current_inode.i_blocks_count * sizeof(uint32_t));
                goto next;
            }
        }

        i += dir->rec_len;
    }

    ret = false;

out:
    pmm_free(alloc_map, current_inode.i_blocks_count * sizeof(uint32_t));
    return ret;
}

bool ext2_open(struct ext2_file_handle *ret, struct volume *part, const char *path) {
    ret->part = part;

    volume_read(ret->part, &ret->sb, 1024, sizeof(struct ext2_superblock));

    struct ext2_superblock *sb = &ret->sb;

    if (sb->s_state == EXT2_FS_UNRECOVERABLE_ERRORS)
        panic(false, "ext2: unrecoverable errors found");

    ret->block_size = ((uint64_t)1024 << ret->sb.s_log_block_size);

    ext2_get_inode(&ret->root_inode, ret, 2);

    struct ext2_dir_entry entry;

    if (!ext2_parse_dirent(&entry, ret, path))
        return false;

    ext2_get_inode(&ret->inode, ret, entry.inode);

    while ((ret->inode.i_mode & FMT_MASK) != S_IFREG) {
        if ((ret->inode.i_mode & FMT_MASK) == S_IFLNK) {
            if (!symlink_to_inode(&ret->inode, ret))
                return false;
        } else {
            print("ext2: Entity is not regular file nor symlink\n");
            return false;
        }
    }

    ret->size = ret->inode.i_size;

    ret->alloc_map = create_alloc_map(ret, &ret->inode);

    return true;
}

void ext2_close(struct ext2_file_handle *file) {
    pmm_free(file->alloc_map, file->inode.i_blocks_count * sizeof(uint32_t));
    pmm_free(file, sizeof(struct ext2_file_handle));
}

void ext2_read(struct ext2_file_handle *file, void *buf, uint64_t loc, uint64_t count) {
    inode_read(buf, loc, count, &file->inode, file, file->alloc_map);
}

static struct ext4_extent_header* ext4_find_leaf(struct ext4_extent_header* ext_block, uint32_t read_block, uint64_t block_size, struct volume *part) {
    struct ext4_extent_idx* index;
    void* buf = NULL;

    while (1) {
        index = (struct ext4_extent_idx*)((size_t)ext_block + 12);

        #define EXT4_EXT_MAGIC 0xf30a
        if (ext_block->magic != EXT4_EXT_MAGIC)
            panic(false, "invalid extent magic");

        if (ext_block->depth == 0) {
            return ext_block;
        }

        int i;
        for (i = 0; i < ext_block->entries; i++) {
            if(read_block < index[i].block)
                break;
        }

        if (--i < 0)
            panic(false, "extent not found");

        uint64_t block = ((uint64_t)index[i].leaf_hi << 32) | index[i].leaf;
        if(!buf)
            buf = ext_mem_alloc(block_size);
        volume_read(part, buf, (block * block_size), block_size);
        ext_block = buf;
    }
}

static int inode_read(void *buf, uint64_t loc, uint64_t count,
                      struct ext2_inode *inode, struct ext2_file_handle *fd,
                      uint32_t *alloc_map) {
    for (uint64_t progress = 0; progress < count;) {
        uint64_t block = (loc + progress) / fd->block_size;

        uint64_t chunk = count - progress;
        uint64_t offset = (loc + progress) % fd->block_size;
        if (chunk > fd->block_size - offset)
            chunk = fd->block_size - offset;

        uint32_t block_index;

        if (inode->i_flags & EXT4_EXTENTS_FLAG) {
            struct ext4_extent_header *leaf;
            struct ext4_extent *ext;
            int i;

            leaf = ext4_find_leaf((struct ext4_extent_header*)inode->i_blocks, block, fd->block_size, fd->part);

            if (!leaf)
                panic(false, "invalid extent");
            ext = (struct ext4_extent*)((size_t)leaf + 12);

            for (i = 0; i < leaf->entries; i++) {
                if (block < ext[i].block) {
                    break;
                }
            }

            if (--i >= 0) {
                block -= ext[i].block;
                if (block >= ext[i].len) {
                    panic(false, "block longer than extent");
                } else {
                    uint64_t start = ((uint64_t)ext[i].start_hi << 32) + ext[i].start;
                    block_index = start + block;
                }
            } else {
                panic(false, "extent for block not found");
            }

            pmm_free(leaf, fd->block_size);
        } else {
            block_index = alloc_map[block];
        }

        volume_read(fd->part, buf + progress, (block_index * fd->block_size) + offset, chunk);

        progress += chunk;
    }

    return 0;
}

// attempts to initialize the ext2 filesystem
// and checks if all features are supported
int ext2_check_signature(struct volume *part) {
    struct ext2_superblock sb;
    volume_read(part, &sb, 1024, sizeof(struct ext2_superblock));

    if (sb.s_magic != EXT2_S_MAGIC)
        return 0;

    // If the revision level is 0, we can't test for features.
    if (sb.s_rev_level == 0)
        return 1;

    if (sb.s_feature_incompat & EXT2_IF_COMPRESSION ||
        sb.s_feature_incompat & EXT2_IF_INLINE_DATA ||
        sb.s_feature_incompat & EXT2_FEATURE_INCOMPAT_META_BG ||
        sb.s_feature_incompat & EXT2_IF_ENCRYPT)
        panic(false, "EXT2: filesystem has unsupported features %x", sb.s_feature_incompat);

    return 1;
}

bool ext2_get_guid(struct guid *guid, struct volume *part) {
    struct ext2_superblock sb;
    volume_read(part, &sb, 1024, sizeof(struct ext2_superblock));

    if (sb.s_magic != EXT2_S_MAGIC)
        return false;

    ((uint64_t *)guid)[0] = sb.s_uuid[0];
    ((uint64_t *)guid)[1] = sb.s_uuid[1];

    return true;
}
