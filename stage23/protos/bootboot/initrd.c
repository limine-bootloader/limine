#include <stdint.h>
#include <stddef.h>
#include <protos/bootboot/initrd.h>
#include <lib/print.h>
#include <lib/libc.h>
#include <lib/blib.h>

INITRD_HANDLER(jamesm);
INITRD_HANDLER(ustar);
INITRD_HANDLER(cpio);

#define DETECT_FAILED panic("bootboot: cannot read file `%s`: cannot detect initrd type (only ustar, cpio and jamesm is supported).", path)

INITRD_HANDLER(auto) {
    if (file.size < 4) {
        DETECT_FAILED;
    }

    if (!memcmp(file.data, "\177ELF", 4)) {
        if (strcmp("sys/core", path) == 0) {
            printv("bootboot: using ELF as initrd to open sys/core\n");
            return file;
        }
        return (struct initrd_file){0};
    }

    if (file.size < 5) {
        DETECT_FAILED;
    }

    if (file.data[4] == 0xBF) {
        struct initrd_file jamesm_attempt = initrd_open_jamesm(file, path);
        if (jamesm_attempt.data) {
            printv("bootboot: jamesm matched when reading file `%s`\n", path);
            return jamesm_attempt;
        }
        panic("bootboot: cannot read file `%s`: no such file or directory", path);
    }

    if (!memcmp("07070", file.data, 5)) {
        struct initrd_file cpio_attempt = initrd_open_cpio(file, path);
        if (cpio_attempt.data) {
            printv("bootboot: cpio matched when reading file `%s`\n", path);
            return cpio_attempt;
        }
        panic("bootboot: cannot read file `%s`: no such file or directory", path);
    }

    if (file.size < 262) {
        DETECT_FAILED;
    }

    if (!memcmp("ustar", file.data + 257, 5)) {
        struct initrd_file ustar_attempt = initrd_open_ustar(file, path);
        if (ustar_attempt.data) {
            printv("bootboot: ustar matched when reading file `%s`\n", path);
            return ustar_attempt;
        }
        panic("bootboot: cannot read file `%s`: no such file or directory", path);
    }

    DETECT_FAILED;
}
