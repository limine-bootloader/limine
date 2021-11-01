#ifndef __LIB__BLIB_H__
#define __LIB__BLIB_H__

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <fs/file.h>
#include <lib/part.h>
#include <lib/libc.h>
#if uefi == 1
#  include <efi.h>
#endif

#if uefi == 1
extern EFI_SYSTEM_TABLE *gST;
extern EFI_BOOT_SERVICES *gBS;
extern EFI_RUNTIME_SERVICES *gRT;
extern EFI_HANDLE efi_image_handle;
extern EFI_MEMORY_DESCRIPTOR *efi_mmap;
extern UINTN efi_mmap_size, efi_desc_size;
extern UINT32 efi_desc_ver;

extern bool efi_boot_services_exited;
bool efi_exit_boot_services(void);
#endif

extern const char bsd_2_clause[];

extern struct volume *boot_volume;

#if bios == 1
extern bool stage3_loaded;
#endif

extern bool verbose;

bool parse_resolution(size_t *width, size_t *height, size_t *bpp, const char *buf);

uint32_t get_crc32(void *_stream, size_t len);

uint64_t sqrt(uint64_t a_nInput);
size_t get_trailing_zeros(uint64_t val);

int digit_to_int(char c);
uint8_t bcd_to_int(uint8_t val);
uint8_t int_to_bcd(uint8_t val);

__attribute__((noreturn)) void panic(const char *fmt, ...);

int pit_sleep_and_quit_on_keypress(int seconds);

uint64_t strtoui(const char *s, const char **end, int base);

#if defined (__i386__)
void memcpy32to64(uint64_t, uint64_t, uint64_t);
#elif defined (__x86_64__)
#  define memcpy32to64(X, Y, Z) memcpy((void *)(uintptr_t)(X), (void *)(uintptr_t)(Y), Z)
#endif

#define DIV_ROUNDUP(a, b) (((a) + ((b) - 1)) / (b))

#define ALIGN_UP(x, a) ({ \
    typeof(x) value = x; \
    typeof(a) align = a; \
    value = DIV_ROUNDUP(value, align) * align; \
    value; \
})

#define ALIGN_DOWN(x, a) ({ \
    typeof(x) value = x; \
    typeof(a) align = a; \
    value = (value / align) * align; \
    value; \
})

#define SIZEOF_ARRAY(array) (sizeof(array) / sizeof(array[0]))

typedef char symbol[];

__attribute__((noreturn)) void common_spinup(void *fnptr, int args, ...);

#endif
