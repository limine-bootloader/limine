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
#define INITRD_LOAD_ADDR ((size_t)0x1000000)

__attribute__((section(".realmode"), used))
static void spinup(uint16_t real_mode_code_seg, uint16_t kernel_entry_seg) {
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
        "jmp 0x0000:1f\n\t"
        "1:\n\t"
        "mov ds, bx\n\t"
        "mov es, bx\n\t"
        "mov fs, bx\n\t"
        "mov gs, bx\n\t"
        "mov ss, bx\n\t"
        "mov esp, 0xfdf0\n\t"

        "sti\n\t"

        "push cx\n\t"
        "push 0\n\t"
        "retf\n\t"

        ".code32\n\t"
        :
        : "b" (real_mode_code_seg), "c" (kernel_entry_seg)
        : "memory"
    );
}

void linux_load(char *config, char *cmdline) {
    // The command line needs to be before address 0xa0000, we can use
    // a conv_mem_alloc() allocated buffer for that.
    // Allocate the relocation buffer for the command line early so it's allocated
    // before the real mode code.
    size_t cmdline_len = strlen(cmdline);
    char *cmdline_reloc = conv_mem_alloc(cmdline_len + 1);
    strcpy(cmdline_reloc, cmdline);

    struct file_handle *kernel = conv_mem_alloc(sizeof(struct file_handle));

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

    print("linux: Setup code size: %x\n", setup_code_size);

    size_t real_mode_code_size = 512 + setup_code_size;

    print("linux: Real Mode code size: %x\n", real_mode_code_size);

    void *real_mode_code = conv_mem_alloc_aligned(real_mode_code_size, 0x1000);

    fread(kernel, real_mode_code, 0, real_mode_code_size);

    size_t heap_end_ptr = ((real_mode_code_size & 0x0f) + 0x10) - 0x200;
    *((uint16_t *)(real_mode_code + 0x224)) = (uint16_t)heap_end_ptr;

    // vid_mode. 0xffff means "normal"
    *((uint16_t *)(real_mode_code + 0x1fa)) = 0xffff;

    uint16_t boot_protocol_ver;
    boot_protocol_ver = *((uint16_t *)(real_mode_code + 0x206));

    print("linux: Boot protocol: %u.%u\n",
          boot_protocol_ver >> 8, boot_protocol_ver & 0xff);

    char *kernel_version;
    kernel_version = real_mode_code + *((uint16_t *)(real_mode_code + 0x20e)) + 0x200;

    if (kernel_version) {
        print("linux: Kernel version: %s\n", kernel_version);
    }

    // set type of loader
    *((uint8_t *)(real_mode_code + 0x210)) = 0xff;

    uint8_t loadflags;
    loadflags = *((uint8_t *)(real_mode_code + 0x211));

    loadflags |=  (1 << 0);     // kernel is loaded at 0x100000
    loadflags &= ~(1 << 5);     // print early messages
    loadflags |=  (1 << 7);     // can use heap

    *((uint8_t *)(real_mode_code + 0x211)) = loadflags;

    *((uint32_t *)(real_mode_code + 0x228)) = (uint32_t)cmdline_reloc;

    // load kernel
    print("Loading kernel...\n");
    memmap_alloc_range(KERNEL_LOAD_ADDR, kernel->size - real_mode_code_size, 0);
    fread(kernel, (void *)KERNEL_LOAD_ADDR, real_mode_code_size, kernel->size - real_mode_code_size);

    size_t modules_mem_base = INITRD_LOAD_ADDR;
    for (size_t i = 0; ; i++) {
        char *module_path = config_get_value(config, i, "MODULE_PATH");
        if (module_path == NULL)
            break;

        struct file_handle module;
        if (!uri_open(&module, module_path))
            panic("Could not open `%s`", module_path);

        print("Loading module `%s`...\n", module_path);

        memmap_alloc_range(modules_mem_base, module.size, 0);
        fread(&module, (void *)modules_mem_base, 0, module.size);

        modules_mem_base += module.size;
    }

    if (modules_mem_base != INITRD_LOAD_ADDR) {
        *((uint32_t *)(real_mode_code + 0x218)) = (uint32_t)INITRD_LOAD_ADDR;
        *((uint32_t *)(real_mode_code + 0x21c)) = (uint32_t)(modules_mem_base - INITRD_LOAD_ADDR);
    }

    uint16_t real_mode_code_seg = rm_seg(real_mode_code);
    uint16_t kernel_entry_seg   = real_mode_code_seg + 0x20;

    term_deinit();

    mtrr_restore();

    spinup(real_mode_code_seg, kernel_entry_seg);
}
