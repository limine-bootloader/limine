#ifndef _LIMINE_H
#define _LIMINE_H 1

#include <stdint.h>

#ifdef LIMINE_NO_POINTERS
#  define LIMINE_CHARPTR uint64_t
#  define LIMINE_VOIDPTR uint64_t
#  define LIMINE_VOIDPTRPTR uint64_t
#else
#  define LIMINE_CHARPTR char *
#  define LIMINE_VOIDPTR void *
#  define LIMINE_VOIDPTRPTR void **
#endif

#define LIMINE_MAGIC { 0xc7b1dd30df4c8b88, 0x0a82e883a194f07b }

struct limine_header {
    uint64_t magic[2];
    LIMINE_VOIDPTR entry;
    uint64_t features_count;
    LIMINE_VOIDPTRPTR features;
};

// Boot info

#define LIMINE_BOOT_INFO_REQUEST ((LIMINE_VOIDPTR) 1 )

struct limine_boot_info_response {
    uint64_t flags;
    LIMINE_CHARPTR loader;
};

// Framebuffer

#define LIMINE_FRAMEBUFFER_REQUEST ((LIMINE_VOIDPTR) 2 )

struct limine_framebuffer_request {
    LIMINE_VOIDPTR id;

#define LIMINE_FRAMEBUFFER_PREFER_LFB 0
#define LIMINE_FRAMEBUFFER_PREFER_TEXT 1
#define LIMINE_FRAMEBUFFER_ENFORCE_PREFER (1 << 8)
    uint64_t flags;

    uint16_t width;
    uint16_t height;
    uint16_t bpp;

    uint16_t unused;
};

// 5-level paging

#define LIMINE_5_LEVEL_PAGING_REQUEST ((LIMINE_VOIDPTR) 3 )

struct limine_5_level_paging_response {
    uint64_t flags;
};

// PMRs

#define LIMINE_PMR_REQUEST ((LIMINE_VOIDPTR) 4 )

#endif
