#include <stdint.h>
#include <stddef.h>
#include <protos/bootboot/initrd.h>
#include <lib/libc.h>
#include <lib/blib.h>

#define PTR (file.data + offset)

INITRD_HANDLER(ustar) {
    if (file.size < 262) {
        return (struct initrd_file){0};
    }

    if (memcmp(file.data + 257, "ustar", 5)) {
        return (struct initrd_file){0};
    }

    size_t path_size = strlen(path);
    uint32_t offset = 0;

    while (!memcmp(PTR + 257, "ustar", 5)) {
        uint32_t file_size = oct2bin(PTR + 0x7c, 11);
        if (!memcmp(PTR, path, path_size + 1)
         || (PTR[0] == '.' && PTR[1] == '/' && !memcmp(PTR + 2, path, path_size + 1))) {
            return (struct initrd_file){
                file_size,
                PTR + 512,
            };
        }
        offset += (((file_size + 511) / 512) + 1) * 512;
    }

    return (struct initrd_file){0};
}
