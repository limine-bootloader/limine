#include <stdint.h>
#include <stddef.h>
#include <limine.h>
#include <e9print.h>

static struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,

    .flags = LIMINE_FRAMEBUFFER_PREFER_LFB | LIMINE_FRAMEBUFFER_ENFORCE_PREFER,

    .height = 0, .width = 0, .bpp = 0
};

static void *features_array[] = {
    LIMINE_BOOT_INFO_REQUEST,
    &framebuffer_request,
    LIMINE_5_LEVEL_PAGING_REQUEST,
    LIMINE_PMR_REQUEST
};

static void limine_main(void);

__attribute__((used, aligned(16)))
static struct limine_header limine_header = {
    .magic = LIMINE_MAGIC,
    .entry = limine_main,
    .features_count = sizeof(features_array) / sizeof(void *),
    .features = features_array
};

#define FEAT_START do {
#define FEAT_END } while (0);

static void limine_main(void) {
    e9_printf("We're alive");

FEAT_START
    if (features_array[0] == NULL) {
        break;
    }
    struct limine_boot_info_response *boot_info_response = features_array[0];
    e9_printf("Boot info response:");
    e9_printf("Bootloader name: %s", boot_info_response->loader);

FEAT_END

    for (;;);
}
