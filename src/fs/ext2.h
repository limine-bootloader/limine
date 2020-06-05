#ifndef __FS_EXT2_H__
#define __FS_EXT2_H__

#include <stdint.h>
#include <stddef.h>
#include <lib/part.h>

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
    int drive;
    struct part part;
    int size;
    struct ext2_inode root_inode;
    struct ext2_inode inode;
    uint64_t block_size;
};

int ext2_check_signature(int drive, int partition);

int ext2_open(struct ext2_file_handle *ret, int drive, int partition, const char *path);
int ext2_read(struct ext2_file_handle *file, void *buf, uint64_t loc, uint64_t count);

#endif
