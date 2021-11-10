#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <protos/bootboot/initrd.h>
#include <lib/print.h>
#include <lib/libc.h>
#include <lib/blib.h>

struct initrd_file bruteforce_kernel(struct initrd_file file) {
    for (size_t i = 0; i < file.size - 19; i++) {
        if (memcmp(file.data + i, "\177ELF", 4) == 0
         && file.data[i + 18] == 62 && file.data[i + 19] == 0 /* ehdr->e_machine == EM_X86_64 */) {
            printv("bootboot: using bruteforced kernel at initrd offset %X\n", file.data + i);
            return (struct initrd_file){
                .size = file.size - i,
                .data = file.data + i
            };
        }
    }
    return (struct initrd_file){0};
}

enum initrd_format initrd_format(struct initrd_file file) {
    if (file.size >= 5 && file.data[4] == 0xbf) {
        return INITRD_FORMAT_JAMESM;
    }

    if (file.size >= 5 && memcmp("07070", file.data, 5) == 0) {
        return INITRD_FORMAT_CPIO;
    }

    if (file.size >= 262 && memcmp("ustar", file.data + 257, 5) == 0) {
        return INITRD_FORMAT_USTAR;
    }

    return INITRD_FORMAT_UNKNOWN;
}

INITRD_HANDLER(jamesm);
INITRD_HANDLER(ustar);
INITRD_HANDLER(cpio);

INITRD_HANDLER(auto) {
    switch (initrd_format(file)) {
        case INITRD_FORMAT_JAMESM:
            return initrd_open_jamesm(file, path);
        case INITRD_FORMAT_CPIO:
            return initrd_open_cpio(file, path);
        case INITRD_FORMAT_USTAR:
            return initrd_open_ustar(file, path);
        default:
            return (struct initrd_file){0};
    }
}
