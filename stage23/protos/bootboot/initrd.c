#include <protos/bootboot/initrd.h>
#include <lib/print.h>
#include <lib/libc.h>
#include <lib/blib.h>


INITRD_HANDLER(jamesm);
INITRD_HANDLER(ustar);
INITRD_HANDLER(cpio);

INITRD_HANDLER(auto) {
#define DETECT_FAILED panic("bootboot: cannot read file `%s`: cannot detect initrd type (only ustar, cpio and jamesm is supported).", path)
    if (file.size < 4) DETECT_FAILED;
    if (!memcmp(file.data, "ELF\x7f", 4)) {
        if (strcmp("sys/core", path) == 0) {
            printv("bootboot: using ELF as initrd to open sys/core\n");
            return file;
        }
        return (file_t){0, NULL};
    }
    if (file.size < 5) DETECT_FAILED;
    if (file.data[4] == 0xBF) {
        file_t jamesm_attempt = initrd_open_jamesm(file, path);
        if (jamesm_attempt.data) {
            printv("bootboot: jamesm matched when reading file `%s`\n", path);
            return jamesm_attempt;
        }
        panic("bootboot: cannot read file `%s`: no such file or directory", path);
    }
    if (!memcmp("07070", file.data, 5)) {
        file_t cpio_attempt = initrd_open_cpio(file, path);
        if (cpio_attempt.data) {
            printv("bootboot: cpio matched when reading file `%s`\n", path);
            return cpio_attempt;
        }
        panic("bootboot: cannot read file `%s`: no such file or directory", path);
    }
    if (file.size < 262) DETECT_FAILED;
    if (!memcmp("ustar", file.data + 257, 5)) {
        file_t ustar_attempt = initrd_open_ustar(file, path);
        if (ustar_attempt.data) {
            printv("bootboot: ustar matched when reading file `%s`\n", path);
            return ustar_attempt;
        }
        panic("bootboot: cannot read file `%s`: no such file or directory", path);
    }
    DETECT_FAILED;
}

/// Utilities ///
uint32_t oct2bin(uint8_t* str, uint32_t max) {
    uint32_t value = 0;
    while(max-- > 0) {
        value <<= 3;
        value += *str++ - '0';
    }
    return value;
}
uint32_t hex2bin(uint8_t* str, uint32_t size) {
    uint32_t value = 0;
    while(size-- > 0){
        value <<= 4;
        if (*str >= '0' && *str <= '9')
            value += (uint32_t)((*str) - '0');
        else if (*str >= 'A' && *str <= 'F')
            value += (uint32_t)((*str) - 'A' + 10);
        else if (*str >= 'a' && *str <= 'f')
            value += (uint32_t)((*str) - 'a' + 10);
        str++;
    }
    return value;
}