#include <fs/ext2fs.h>

// determines if the drive is ext2 or not
static uint8_t isEXT2(struct ext2fs_superblock* superblock) {
    // TODO: determine EXT2_READONLY

    if (superblock->signature == 0xEF53) {
        return EXT2;
    }

    return OTHER;
}

// attempt to initialize the ext2 filesystem
uint8_t initEXT2(int disk) {
    struct ext2_superblock superblock;
}