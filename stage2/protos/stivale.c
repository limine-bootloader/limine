#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <protos/stivale.h>
#include <lib/elf.h>
#include <lib/blib.h>
#include <lib/acpi.h>
#include <lib/config.h>
#include <lib/time.h>
#include <lib/print.h>
#include <lib/rand.h>
#include <lib/real.h>
#include <lib/uri.h>
#include <drivers/vbe.h>
#include <lib/term.h>
#include <sys/pic.h>
#include <sys/cpu.h>
#include <fs/file.h>
#include <mm/vmm.h>
#include <mm/pmm.h>
#include <mm/mtrr.h>
#include <stivale/stivale.h>

#define KASLR_SLIDE_BITMASK 0x03FFFF000u

struct stivale_struct stivale_struct = {0};

void stivale_load(char *cmdline) {
    char buf[128];

    stivale_struct.flags |= (1 << 0);  // set bit 0 since we are BIOS and not UEFI

    struct file_handle *kernel = conv_mem_alloc(sizeof(struct file_handle));

    if (!config_get_value(buf, 0, 128, "KERNEL_PATH"))
        panic("KERNEL_PATH not specified");

    if (!uri_open(kernel, buf))
        panic("Could not open kernel resource");

    struct stivale_header stivale_hdr;

    int bits = elf_bits(kernel);

    int ret;

    uint64_t slide = 0;

    bool level5pg = false;
    switch (bits) {
        case 64: {
            // Check if 64 bit CPU
            uint32_t eax, ebx, ecx, edx;
            cpuid(0x80000001, 0, &eax, &ebx, &ecx, &edx);
            if (!(edx & (1 << 29))) {
                panic("stivale: This CPU does not support 64-bit mode.");
            }
            // Check if 5-level paging is available
            cpuid(0x00000007, 0, &eax, &ebx, &ecx, &edx);
            if (ecx & (1 << 16)) {
                print("stivale: CPU has 5-level paging support\n");
                level5pg = true;
            }

            ret = elf64_load_section(kernel, &stivale_hdr, ".stivalehdr", sizeof(struct stivale_header), slide);

            if (!ret && ((stivale_hdr.flags >> 2) & 1)) {
                // KASLR is enabled, set the slide
                slide = rand64() & KASLR_SLIDE_BITMASK;

                // Re-read the .stivalehdr with slid relocations
                ret = elf64_load_section(kernel, &stivale_hdr, ".stivalehdr", sizeof(struct stivale_header), slide);
            }

            break;
        }
        case 32:
            ret = elf32_load_section(kernel, &stivale_hdr, ".stivalehdr", sizeof(struct stivale_header));
            break;
        default:
            panic("stivale: Not 32 nor 64 bit x86 ELF file.");
    }

    print("stivale: %u-bit ELF file detected\n", bits);

    switch (ret) {
        case 1:
            panic("stivale: File is not a valid ELF.");
        case 2:
            panic("stivale: Section .stivalehdr not found.");
        case 3:
            panic("stivale: Section .stivalehdr exceeds the size of the struct.");
        case 4:
            panic("stivale: Section .stivalehdr is smaller than size of the struct.");
    }

    print("stivale: Requested stack at %X\n", stivale_hdr.stack);

    uint64_t entry_point   = 0;
    uint64_t top_used_addr = 0;

    switch (bits) {
        case 64:
            elf64_load(kernel, &entry_point, &top_used_addr, slide, 10);
            break;
        case 32:
            elf32_load(kernel, (uint32_t *)&entry_point, (uint32_t *)&top_used_addr, 10);
            break;
    }

    if (stivale_hdr.entry_point != 0)
        entry_point = stivale_hdr.entry_point;

    print("stivale: Kernel slide: %X\n", slide);

    print("stivale: Top used address in ELF: %X\n", top_used_addr);

    stivale_struct.module_count = 0;
    uint64_t *prev_mod_ptr = &stivale_struct.modules;
    for (int i = 0; ; i++) {
        char module_file[64];
        if (!config_get_value(module_file, i, 64, "MODULE_PATH"))
            break;

        stivale_struct.module_count++;

        struct stivale_module *m = conv_mem_alloc(sizeof(struct stivale_module));

        if (!config_get_value(m->string, i, 128, "MODULE_STRING")) {
            m->string[0] = '\0';
        }

        struct file_handle f;
        if (!uri_open(&f, module_file))
            panic("Requested module with path \"%s\" not found!\n", module_file);

        void *module_addr = (void *)(((uint32_t)top_used_addr & 0xfff) ?
            ((uint32_t)top_used_addr & ~((uint32_t)0xfff)) + 0x1000 :
            (uint32_t)top_used_addr);

        print("stivale: Loading module `%s`...\n", module_file);

        memmap_alloc_range((size_t)module_addr, f.size, 10);
        fread(&f, module_addr, 0, f.size);

        m->begin = (uint64_t)(size_t)module_addr;
        m->end   = m->begin + f.size;
        m->next  = 0;

        top_used_addr = (uint64_t)(size_t)m->end;

        *prev_mod_ptr = (uint64_t)(size_t)m;
        prev_mod_ptr  = &m->next;

        print("stivale: Requested module %u:\n", i);
        print("         Path:   %s\n", module_file);
        print("         String: %s\n", m->string);
        print("         Begin:  %X\n", m->begin);
        print("         End:    %X\n", m->end);
    }

    stivale_struct.rsdp = (uint64_t)(size_t)acpi_get_rsdp();

    stivale_struct.cmdline = (uint64_t)(size_t)cmdline;

    stivale_struct.epoch = time();
    print("stivale: Current epoch: %U\n", stivale_struct.epoch);

    stivale_struct.framebuffer_width  = stivale_hdr.framebuffer_width;
    stivale_struct.framebuffer_height = stivale_hdr.framebuffer_height;
    stivale_struct.framebuffer_bpp    = stivale_hdr.framebuffer_bpp;

    term_deinit();

    if (stivale_hdr.flags & (1 << 0)) {
        uint32_t *fb32;
        init_vbe(&fb32,
                 &stivale_struct.framebuffer_pitch,
                 &stivale_struct.framebuffer_width,
                 &stivale_struct.framebuffer_height,
                 &stivale_struct.framebuffer_bpp);
        stivale_struct.framebuffer_addr = (uint64_t)(size_t)fb32;
    }

    bool want_5lv = level5pg && (stivale_hdr.flags & (1 << 1));
    pagemap_t pagemap = stivale_build_pagemap(want_5lv);

    size_t memmap_entries;
    struct e820_entry_t *memmap = get_memmap(&memmap_entries);

    stivale_struct.memory_map_entries = (uint64_t)memmap_entries;
    stivale_struct.memory_map_addr    = (uint64_t)(size_t)memmap;

    stivale_spinup(bits, want_5lv, pagemap,
                   entry_point, &stivale_struct, stivale_hdr.stack);
}

pagemap_t stivale_build_pagemap(bool level5pg) {
    pagemap_t pagemap = new_pagemap(level5pg ? 5 : 4);
    uint64_t higher_half_base = level5pg ? 0xff00000000000000 : 0xffff800000000000;

    // Map 0 to 2GiB at 0xffffffff80000000
    for (uint64_t i = 0; i < 0x80000000; i += PAGE_SIZE) {
        map_page(pagemap, 0xffffffff80000000 + i, i, 0x03);
    }

    // Map 0 to 4GiB at higher half base and 0
    for (uint64_t i = 0; i < 0x100000000; i += PAGE_SIZE) {
        map_page(pagemap, i, i, 0x03);
        map_page(pagemap, higher_half_base + i, i, 0x03);
    }

    size_t memmap_entries;
    struct e820_entry_t *memmap = get_memmap(&memmap_entries);

    // Map any other region of memory from the memmap
    for (size_t i = 0; i < memmap_entries; i++) {
        memmap = get_memmap(&memmap_entries);

        uint64_t base   = memmap[i].base;
        uint64_t length = memmap[i].length;
        uint64_t top    = base + length;

        uint64_t aligned_base   = ALIGN_DOWN(base, PAGE_SIZE);
        uint64_t aligned_top    = ALIGN_UP(top, PAGE_SIZE);
        uint64_t aligned_length = aligned_top - aligned_base;

        for (uint64_t i = 0; i < aligned_length; i += PAGE_SIZE) {
            uint64_t page = aligned_base + i;
            map_page(pagemap, page, page, 0x03);
            map_page(pagemap, higher_half_base + page, page, 0x03);
        }
    }

    return pagemap;
}

__attribute__((noreturn)) void stivale_spinup(
                 int bits, bool level5pg, pagemap_t pagemap,
                 uint64_t entry_point, void *stivale_struct, uint64_t stack) {
    mtrr_restore();

    if (bits == 64) {
        // If we're going 64, we might as well call this BIOS interrupt
        // to tell the BIOS that we are entering Long Mode, since it is in
        // the specification.
        struct rm_regs r = {0};
        r.eax = 0xec00;
        r.ebx = 0x02;   // Long mode only
        rm_int(0x15, &r, &r);
    }

    pic_mask_all();
    pic_flush();

    if (bits == 64) {
        if (level5pg) {
            // Enable CR4.LA57
            asm volatile (
                "mov eax, cr4\n\t"
                "bts eax, 12\n\t"
                "mov cr4, eax\n\t" ::: "eax", "memory"
            );
        }

        asm volatile (
            "cli\n\t"
            "cld\n\t"
            "mov cr3, eax\n\t"
            "mov eax, cr4\n\t"
            "or eax, 1 << 5\n\t"
            "mov cr4, eax\n\t"
            "mov ecx, 0xc0000080\n\t"
            "rdmsr\n\t"
            "or eax, 1 << 8\n\t"
            "wrmsr\n\t"
            "mov eax, cr0\n\t"
            "or eax, 1 << 31\n\t"
            "mov cr0, eax\n\t"
            "jmp 0x28:1f\n\t"
            "1: .code64\n\t"
            "mov ax, 0x30\n\t"
            "mov ds, ax\n\t"
            "mov es, ax\n\t"
            "mov fs, ax\n\t"
            "mov gs, ax\n\t"
            "mov ss, ax\n\t"

            // Since we don't really know what is now present in the upper
            // 32 bits of the 64 bit registers, clear up the upper bits
            // of the registers we use to store stack pointer and instruction
            // pointer
            "mov esi, esi\n\t"
            "mov ebx, ebx\n\t"
            "mov edi, edi\n\t"

            // Let's pretend we push a return address
            "mov rsi, qword ptr [rsi]\n\t"
            "test rsi, rsi\n\t"
            "jz 1f\n\t"

            "sub rsi, 8\n\t"
            "mov qword ptr [rsi], 0\n\t"

            "1:\n\t"
            "push 0x30\n\t"
            "push rsi\n\t"
            "pushfq\n\t"
            "push 0x28\n\t"
            "push [rbx]\n\t"

            "xor rax, rax\n\t"
            "xor rbx, rbx\n\t"
            "xor rcx, rcx\n\t"
            "xor rdx, rdx\n\t"
            "xor rsi, rsi\n\t"
            "xor rbp, rbp\n\t"
            "xor r8,  r8\n\t"
            "xor r9,  r9\n\t"
            "xor r10, r10\n\t"
            "xor r11, r11\n\t"
            "xor r12, r12\n\t"
            "xor r13, r13\n\t"
            "xor r14, r14\n\t"
            "xor r15, r15\n\t"

            "iretq\n\t"
            ".code32\n\t"
            :
            : "a" (pagemap.top_level), "b" (&entry_point),
              "D" (stivale_struct), "S" (&stack)
            : "memory"
        );
    } else if (bits == 32) {
        asm volatile (
            "cli\n\t"
            "cld\n\t"

            "mov esp, dword ptr [esi]\n\t"
            "push edi\n\t"
            "push 0\n\t"

            "pushfd\n\t"
            "push 0x18\n\t"
            "push [ebx]\n\t"

            "xor eax, eax\n\t"
            "xor ebx, ebx\n\t"
            "xor ecx, ecx\n\t"
            "xor edx, edx\n\t"
            "xor esi, esi\n\t"
            "xor edi, edi\n\t"
            "xor ebp, ebp\n\t"

            "iret\n\t"
            :
            : "b"(&entry_point), "D"(stivale_struct), "S"(&stack)
            : "memory"
        );
    }
    for (;;);
}
