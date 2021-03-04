#include <stdint.h>
#include <stddef.h>
#include <protos/linux.h>
#include <fs/file.h>
#include <lib/libc.h>
#include <lib/blib.h>
#include <lib/real.h>
#include <lib/term.h>
#include <lib/config.h>
#include <lib/print.h>
#include <lib/uri.h>
#include <mm/pmm.h>
#include <mm/mtrr.h>

#define KERNEL_LOAD_ADDR ((size_t)0x100000)
#define KERNEL_HEAP_SIZE ((size_t)0x6000)

__attribute__((section(".realmode"), used))
static void spinup(uint16_t real_mode_code_seg, uint16_t kernel_entry_seg,
                   uint16_t stack_pointer) {
    asm volatile (
        "cld\n\t"

        "jmp 0x08:1f\n\t"
        "1: .code16\n\t"
        "mov ax, 0x10\n\t"
        "mov ds, ax\n\t"
        "mov es, ax\n\t"
        "mov fs, ax\n\t"
        "mov gs, ax\n\t"
        "mov ss, ax\n\t"
        "mov eax, cr0\n\t"
        "and al, 0xfe\n\t"
        "mov cr0, eax\n\t"
        "mov eax, OFFSET 1f\n\t"
        "push eax\n\t"
        "push 0\n\t"
        "retf\n\t"
        "1:\n\t"
        "mov ds, bx\n\t"
        "mov es, bx\n\t"
        "mov fs, bx\n\t"
        "mov gs, bx\n\t"
        "mov ss, bx\n\t"
        "mov esp, edx\n\t"

        "push cx\n\t"
        "push 0\n\t"

        "retf\n\t"

        ".code32\n\t"
        :
        : "b" (real_mode_code_seg), "c" (kernel_entry_seg),
          "d" (stack_pointer)
        : "memory"
    );
}

void linux_load(char *config, char *cmdline) {
    struct file_handle *kernel = ext_mem_alloc(sizeof(struct file_handle));

    char *kernel_path = config_get_value(config, 0, "KERNEL_PATH");
    if (kernel_path == NULL)
        panic("KERNEL_PATH not specified");

    if (!uri_open(kernel, kernel_path))
        panic("Could not open kernel resource");

    uint32_t signature;
    fread(kernel, &signature, 0x202, sizeof(uint32_t));

    // validate signature
    if (signature != 0x53726448) {
        panic("Invalid Linux kernel signature");
    }

    size_t setup_code_size = 0;
    fread(kernel, &setup_code_size, 0x1f1, 1);

    if (setup_code_size == 0)
        setup_code_size = 4;

    setup_code_size *= 512;

    size_t real_mode_code_size = 512 + setup_code_size;

    size_t real_mode_and_heap_size = 0x8000 + KERNEL_HEAP_SIZE;

    void *real_mode_code = conv_mem_alloc_aligned(0x10000, 0x1000);

    fread(kernel, real_mode_code, 0, real_mode_code_size);

    uint16_t boot_protocol_ver;
    boot_protocol_ver = *((uint16_t *)(real_mode_code + 0x206));

    print("linux: Boot protocol: %u.%u\n",
          boot_protocol_ver >> 8, boot_protocol_ver & 0xff);

    if (boot_protocol_ver < 0x203) {
        panic("Linux protocols < 2.03 are not supported");
    }

    size_t heap_end_ptr = real_mode_and_heap_size - 0x200;
    *((uint16_t *)(real_mode_code + 0x224)) = (uint16_t)heap_end_ptr;

    char *cmdline_reloc = real_mode_code + real_mode_and_heap_size;
    strcpy(cmdline_reloc, cmdline);

    // vid_mode. 0xffff means "normal"
    *((uint16_t *)(real_mode_code + 0x1fa)) = 0xffff;

    char *kernel_version;
    kernel_version = real_mode_code + *((uint16_t *)(real_mode_code + 0x20e)) + 0x200;

    if (kernel_version) {
        print("linux: Kernel version: %s\n", kernel_version);
    }

    // set type of loader
    *((uint8_t *)(real_mode_code + 0x210)) = 0xff;

    uint8_t loadflags;
    loadflags = *((uint8_t *)(real_mode_code + 0x211));

    if (!(loadflags & (1 << 0))) {
        panic("Linux kernels that load at 0x10000 are not supported");
    }

    loadflags &= ~(1 << 5);     // print early messages
    loadflags |=  (1 << 7);     // can use heap

    *((uint8_t *)(real_mode_code + 0x211)) = loadflags;

    *((uint32_t *)(real_mode_code + 0x228)) = (uint32_t)cmdline_reloc;

    // load kernel
    print("linux: Loading kernel...\n");
    memmap_alloc_range(KERNEL_LOAD_ADDR, kernel->size - real_mode_code_size, 0, true, true);
    fread(kernel, (void *)KERNEL_LOAD_ADDR, real_mode_code_size, kernel->size - real_mode_code_size);

    uint32_t modules_mem_base = *((uint32_t *)(real_mode_code + 0x22c)) + 1;
    if (modules_mem_base == 0)
        modules_mem_base = 0x38000000;

    size_t size_of_all_modules = 0;

    for (size_t i = 0; ; i++) {
        char *module_path = config_get_value(config, i, "MODULE_PATH");
        if (module_path == NULL)
            break;

        struct file_handle module;
        if (!uri_open(&module, module_path))
            panic("Could not open `%s`", module_path);

        size_of_all_modules += module.size;
    }

    modules_mem_base -= size_of_all_modules;
    modules_mem_base = ALIGN_DOWN(modules_mem_base, 4096);

    for (;;) {
        if (memmap_alloc_range(modules_mem_base, size_of_all_modules, 0, true, false))
            break;
        modules_mem_base -= 4096;
    }

    size_t _modules_mem_base = modules_mem_base;
    for (size_t i = 0; ; i++) {
        char *module_path = config_get_value(config, i, "MODULE_PATH");
        if (module_path == NULL)
            break;

        struct file_handle module;
        if (!uri_open(&module, module_path))
            panic("Could not open `%s`", module_path);

        print("linux: Loading module `%s`...\n", module_path);

        fread(&module, (void *)_modules_mem_base, 0, module.size);

        _modules_mem_base += module.size;
    }

    if (size_of_all_modules != 0) {
        *((uint32_t *)(real_mode_code + 0x218)) = (uint32_t)modules_mem_base;
        *((uint32_t *)(real_mode_code + 0x21c)) = (uint32_t)size_of_all_modules;
    }

    uint16_t real_mode_code_seg = rm_seg(real_mode_code);
    uint16_t kernel_entry_seg   = real_mode_code_seg + 0x20;

    term_deinit();

    mtrr_restore();

    spinup(real_mode_code_seg, kernel_entry_seg, real_mode_and_heap_size);
}
