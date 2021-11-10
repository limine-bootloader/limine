#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <protos/bootboot/initrd.h>
#include <lib/print.h>
#include <lib/libc.h>
#include <lib/blib.h>

struct initrd_file bruteforce_kernel(struct initrd_file file) {
    for (size_t i = 0; i < file.size; i++) {
        if (memcmp(file.data + i, "\177ELF", 4) == 0) {
            printv("bootboot: using bruteforced kernel at initrd offset %X\n", file.data + i);
            return (struct initrd_file){
                .size = file.size - i,
                .data = file.data + i
            };
        }
    }
    return (struct initrd_file){0};
}

bool known_initrd_format(struct initrd_file file) {
    if (file.size >= 5 && file.data[4] == 0xbf) {
        return true;
    }

    if (file.size >= 5 && memcmp("07070", file.data, 5) == 0) {
        return true;
    }

    if (file.size >= 262 && memcmp("ustar", file.data + 257, 5) == 0) {
        return true;
    }

    return false;
}

INITRD_HANDLER(jamesm);
INITRD_HANDLER(ustar);
INITRD_HANDLER(cpio);

INITRD_HANDLER(auto) {
    if (file.size >= 5 && file.data[4] == 0xbf) {
        return initrd_open_jamesm(file, path);
    }

    if (file.size >= 5 && memcmp("07070", file.data, 5) == 0) {
        return initrd_open_cpio(file, path);
    }

    if (file.size >= 262 && memcmp("ustar", file.data + 257, 5) == 0) {
        return initrd_open_ustar(file, path);
    }

    return (struct initrd_file){0};
}
