#if defined(__riscv64)

#include <stdint.h>
#include <stddef.h>
#include <stdnoreturn.h>
#include <protos/linux.h>
#include <fs/file.h>
#include <lib/libc.h>
#include <lib/misc.h>
#include <lib/term.h>
#include <lib/config.h>
#include <lib/print.h>
#include <lib/uri.h>
#include <mm/pmm.h>
#include <sys/idt.h>
#include <lib/fb.h>
#include <lib/acpi.h>
#include <lib/fdt.h>
#include <libfdt/libfdt.h>

// The following definitions and struct were copied and adapted from Linux
// kernel headers released under GPL-2.0 WITH Linux-syscall-note
// allowing their inclusion in non GPL compliant code.

struct linux_header {
    uint32_t code0;
    uint32_t code1;
    uint64_t text_offset;
    uint64_t image_size;
    uint64_t flags;
    uint32_t version;
    uint32_t res1;
    uint64_t res2;
    uint64_t res3;          // originally 'magic' field, deprecated
    uint32_t magic2;
    uint32_t res4;
} __attribute__((packed));

// End of Linux code

#define LINUX_HEADER_MAGIC2             0x05435352
#define LINUX_HEADER_MAJOR_VER(ver)     (((ver) >> 16) & 0xffff)
#define LINUX_HEADER_MINOR_VER(ver)     (((ver) >> 0)  & 0xffff)

noreturn void linux_load(char *config, char *cmdline) {
    struct file_handle *kernel_file;

    char *kernel_path = config_get_value(config, 0, "KERNEL_PATH");
    if (kernel_path == NULL) {
        panic(true, "linux: KERNEL_PATH not specified");
    }

    if ((kernel_file = uri_open(kernel_path)) == NULL) {
        panic(true, "linux: failed to open kernel `%s`. Is the path correct?", kernel_path);
    }

    struct linux_header header;
    fread(kernel_file, &header, 0, sizeof(header));

    if (header.magic2 != LINUX_HEADER_MAGIC2) {
        panic(true, "linux: kernel header magic does not match");
    }

    printv("linux: boot protocol version %d.%d\n",
           LINUX_HEADER_MAJOR_VER(header.version),
           LINUX_HEADER_MINOR_VER(header.version));
    if (LINUX_HEADER_MINOR_VER(header.version) < 2) {
        panic(true, "linux: protocols < 0.2 are not supported");
    }

    size_t kernel_size = kernel_file->size;
    void *kernel_base = ext_mem_alloc_type_aligned(
                ALIGN_UP(kernel_size, 4096),
                MEMMAP_KERNEL_AND_MODULES, 2 * 1024 * 1024);
    fread(kernel_file, kernel_base, 0, kernel_size);
    fclose(kernel_file);
    printv("linux: loaded kernel `%s` at %x, size %u\n", kernel_path, kernel_base, kernel_size);

    void *dtb = get_device_tree_blob();
    if (!dtb) {
        panic(true, "linux: no device tree blob found");
    }

    int ret = fdt_set_chosen_string(dtb, "bootargs", cmdline);
    if (ret < 0) {
       printv("linux: cannot set bootargs: `%s`\n", fdt_strerror(ret));
    }

    char *module_path = config_get_value(config, 0, "MODULE_PATH");
    if (module_path) {
        struct file_handle *module_file = uri_open(module_path);
        if (!module_file) {
            panic(true, "linux: failed to open module `%s`. Is the path correct?", module_path);
        }

        size_t module_size = module_file->size;
        void *module_base = ext_mem_alloc_type_aligned(
                        ALIGN_UP(module_size, 4096),
                        MEMMAP_KERNEL_AND_MODULES, 4096);

        fread(module_file, module_base, 0, module_size);
        fclose(module_file);
        printv("linux: loaded module `%s` at %x, size %u\n", module_path, module_base, module_size);

        ret = fdt_set_chosen_uint64(dtb, "linux,initrd-start", (uint64_t)module_base);
        if (ret < 0) {
            printv("linux: cannot set initrd parameter: %s\n", fdt_strerror(ret));
        }

        ret = fdt_set_chosen_uint64(dtb, "linux,initrd-end", (uint64_t)(module_base + module_size));
        if (ret < 0) {
            printv("linux: cannot set initrd parameter: %s\n", fdt_strerror(ret));
        }
    }

    printv("linux: bsp hart %d, device tree blob at %x\n", bsp_hartid, dtb);

    void (*kernel_entry)(uint64_t hartid, uint64_t dtb) = kernel_base;
    asm ("csrci   sstatus, 0x2\n\t"
         "csrw    sie, zero\n\t");
    kernel_entry(bsp_hartid, (uint64_t)dtb);
    __builtin_unreachable();
}

#endif
