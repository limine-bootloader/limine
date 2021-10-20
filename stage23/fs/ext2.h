#ifndef __FS__EXT2_H__
#define __FS__EXT2_H__

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <lib/part.h>
#include <lib/blib.h>

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

struct ext2_linux {
    uint8_t  frag_num;
    uint8_t  frag_size;

    uint16_t reserved_16;
    uint16_t user_id_high;
    uint16_t group_id_high;

    uint32_t reserved_32;
} __attribute__((packed));

struct ext2_inode {
    uint16_t i_mode;
    uint16_t i_uid;

    uint32_t i_size;
    uint32_t i_atime;
    uint32_t i_ctime;
    uint32_t i_mtime;
    uint32_t i_dtime;

    uint16_t i_gid;
    uint16_t i_links_count;

    uint32_t i_blocks_count;
    uint32_t i_flags;
    uint32_t i_osd1;
    uint32_t i_blocks[15];
    uint32_t i_generation;

    /* EXT2 v >= 1.0 */
    uint32_t i_eab;
    uint32_t i_maj;

    /* EXT2 vAll */
    uint32_t i_frag_block;

    struct ext2_linux i_osd2;
} __attribute__((packed));

struct ext2_file_handle {
    struct volume *part;
    struct ext2_superblock sb;
    int size;
    struct ext2_inode root_inode;
    struct ext2_inode inode;
    uint64_t block_size;
    uint32_t *alloc_map;
};

int ext2_check_signature(struct volume *part);
bool ext2_get_guid(struct guid *guid, struct volume *part);

bool ext2_open(struct ext2_file_handle *ret, struct volume *part, const char *path);
void ext2_read(struct ext2_file_handle *file, void *buf, uint64_t loc, uint64_t count);
void ext2_close(struct ext2_file_handle *file);

#endif
