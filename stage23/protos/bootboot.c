#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <protos/bootboot.h>
#include <protos/bootboot/initrd.h>
#include <lib/libc.h>
#include <lib/elf.h>
#include <lib/blib.h>
#include <lib/acpi.h>
#include <lib/config.h>
#include <lib/time.h>
#include <lib/print.h>
#include <lib/real.h>
#include <lib/uri.h>
#include <lib/fb.h>
#include <lib/term.h>
#include <sys/pic.h>
#include <sys/cpu.h>
#include <sys/gdt.h>
#include <sys/idt.h>
#include <sys/lapic.h>
#include <sys/smp.h>
#include <fs/file.h>
#include <mm/vmm.h>
#include <mm/pmm.h>
#include <drivers/vga_textmode.h>

#define _emitenv(e) do { \
        if (envoff >= 4095) { \
            panic("bootboot: too much env data!"); \
        } \
        env[envoff++] = e; \
    } while (false);

#define KOFFSET(type, off) (type)&kernel[(off)]
#define ESECTION(idx) KOFFSET(struct elf64_shdr*, elf_header->shoff + elf_header->shdr_size * (idx))

__attribute__((noreturn)) void bootboot_spinup_32(
                 uint32_t pagemap_top_lv,
                 uint32_t entry_point_lo, uint32_t entry_point_hi,
                 uint32_t stack_lo, uint32_t stack_hi);

void bootboot_load(char *config) {
    uint64_t fb_vaddr = BOOTBOOT_FB;
    uint64_t struct_vaddr = BOOTBOOT_INFO;
    uint64_t env_vaddr = BOOTBOOT_ENV;
    uint64_t init_stack_size = (uint64_t)-1;

    /// Config ///
    char *kernel_path = config_get_value(config, 0, "KERNEL_PATH");

    char *initrd = config_get_value(config, 0, "INITRD_PATH");
    if (initrd == NULL) {
        initrd = kernel_path;
        kernel_path = NULL;
    }

    if (kernel_path == NULL && initrd == NULL) {
        panic("bootboot: no KERNEL_PATH or INITRD_PATH specified!");
    }

    /// Initrd loading ///
    struct initrd_file bootboot_initrd_file;
    uint64_t initrd_start = 0, initrd_size = 0;
    if (initrd) {
        struct file_handle *initrd_file;
        if ((initrd_file = uri_open(initrd)) == NULL) {
            panic("bootboot: Failed to open initrd with path `%s`. Is the path correct?", initrd);
        }

        uint8_t *initrd_data = freadall(initrd_file, MEMMAP_KERNEL_AND_MODULES);
        initrd_size = initrd_file->size;
        initrd_start = (uint64_t)(size_t)initrd_data;
        fclose(initrd_file);
        bootboot_initrd_file.size = initrd_size;
        bootboot_initrd_file.data = initrd_data;
    } else {
        panic("bootboot: logic error: no initrd, even though one MUST be present");
    }

    /// Load bootboot config ///
    uint8_t *env = ext_mem_alloc_type_aligned(4096, MEMMAP_BOOTLOADER_RECLAIMABLE, 4096);
    uint64_t envoff = 0;
    do {
        struct initrd_file conf = initrd_open_auto(bootboot_initrd_file, "sys/config");
        if (!conf.data) {
            break;
        }

        uint8_t state = 0;
        uint8_t ipeq = 0;

        // state 0: kv
        // state 1: precomment
        // state 2: comment

        for (uint64_t inoff = 0; inoff < conf.size; inoff++) {
            if (conf.data[inoff] == ' ' && !(state == 0 && ipeq == 2)) continue;
            if (conf.data[inoff] != ' ' && state == 0 && ipeq == 1) { ipeq = 2; }
            if (conf.data[inoff] == '/' && state == 1) { state = 2; continue; }
            if (conf.data[inoff] == '\n' && state == 2) { state = 0; continue; }
            if (conf.data[inoff] == '\n') { ipeq = 0; }
            if (conf.data[inoff] == '/' && state == 0) { state = 1; continue; }
            if (state == 1) { state = 0; _emitenv('/'); _emitenv(conf.data[inoff]); continue; }
            if (state == 2) continue;
            if (state == 0 && conf.data[inoff] == '=') ipeq = 1;
            _emitenv(conf.data[inoff]);
        }
    } while (false);
    _emitenv(0);

    printv("env: ---------\n%s\n--------\n", env);

    /// Kernel loading code ///
    uint8_t *kernel;
    if (kernel_path) {
        print("bootboot: Loading kernel `%s`...\n", kernel_path);
        struct file_handle *kernel_file;
        if ((kernel_file = uri_open(kernel_path)) == NULL)
            panic("bootboot: Failed to open kernel with path `%s`. Is the path correct?", kernel_path);

        kernel = freadall(kernel_file, MEMMAP_KERNEL_AND_MODULES);

        fclose(kernel_file);
    } else {
        const char *corefile = config_get_value((char *)env, 0, "kernel");
        if (!corefile) {
            corefile = "sys/core";
        }
        struct initrd_file file = initrd_open_auto(bootboot_initrd_file, corefile);
        kernel = file.data;
        if (!file.size) {
            panic("bootboot: cannot find the kernel!");
        }
    }

    /// Memory mappings ///
    pagemap_t pmap = new_pagemap(4);

    /// Load kernel ///
    uint64_t entry, top, physbase, virtbase;
    {
        if (elf64_load(
            kernel, &entry, &top, NULL, MEMMAP_KERNEL_AND_MODULES,
            false, false, NULL, NULL, true, &physbase, &virtbase)) {
            panic("bootboot: elf64 load failed");
        }
        for (uint64_t mapvirt = virtbase, mapphys = physbase; mapphys < top; mapvirt += 0x1000, mapphys += 0x1000) {
            map_page(pmap, mapvirt, mapphys, VMM_FLAG_PRESENT | VMM_FLAG_WRITE, false);
        }
    }

    /// Bootboot symbols ///
    struct elf64_hdr *elf_header = (struct elf64_hdr *)kernel;
    struct elf64_shdr *section_header_strings_section = ESECTION(elf_header->shstrndx);
    char *section_header_strings = KOFFSET(char *, section_header_strings_section->sh_offset);

    struct elf64_shdr *symbol_table = NULL;
    struct elf64_shdr *string_table = NULL;
    for (uint32_t i = 0; i < elf_header->sh_num; i++){
        struct elf64_shdr *section_header = ESECTION(i);
        char *secname = &section_header_strings[section_header->sh_name];
        if (!strcmp(secname, ".symtab")) {
            symbol_table = section_header;
        }
        if (!strcmp(secname, ".strtab")) {
            string_table = section_header;
        }
    }

    if (!symbol_table || !string_table) {
        print("bootboot: WARNING: no symbol/string tables in the ELF!\n");
    } else {
        struct elf64_sym *symbols = KOFFSET(struct elf64_sym *, symbol_table->sh_offset);
        char *symbol_strings = KOFFSET(char *, string_table->sh_offset);
        for (uint32_t i = 0, symcount = symbol_table->sh_size / sizeof(struct elf64_sym); i < symcount; i++) {
            char *elf_sym = &symbol_strings[symbols[i].st_name];
            uint64_t symaddr = symbols[i].st_value;

            if (!strcmp(elf_sym, "bootboot")) {
                struct_vaddr = symaddr;
            }
            if (!strcmp(elf_sym, "environment")) {
                env_vaddr = symaddr;
            }
            if (!strcmp(elf_sym, "fb")) {
                fb_vaddr = symaddr;
            }
            if (!strcmp(elf_sym, "initstack")) {
                init_stack_size = symaddr;
            }
        }
    }

    if (init_stack_size == (uint64_t)-1) {
        print("bootboot: WARNING: no init stack size entered, assuming 1024\n");
        print("1024 is really small, specify more using initstack=size ini initrd;\n");
        init_stack_size = 1024;
    }

    printv("bootboot: mapping struct to %X\n", struct_vaddr);
    printv("bootboot: mapping environemnt to %X\n", env_vaddr);
    printv("bootboot: mapping framebuffer to %X\n", fb_vaddr);
    printv("bootboot: the init stack is %X bytes\n", init_stack_size);;

    /// Bootboot structure ///
    BOOTBOOT *bootboot = ext_mem_alloc_type_aligned(4096, MEMMAP_BOOTLOADER_RECLAIMABLE, 4096);
    map_page(pmap, struct_vaddr, (uintptr_t)bootboot, VMM_FLAG_PRESENT | VMM_FLAG_WRITE, false);

    /// Environment ///
    {
        map_page(pmap, env_vaddr, (uintptr_t)env, VMM_FLAG_PRESENT | VMM_FLAG_WRITE, false);
        uint32_t index = 0, offset = 0;
        char *cfgent = NULL;
        do {
            cfgent = config_get_value(config, index++, "BOOTBOOT_ENV");
            if (cfgent) {
                uint32_t off = strlen(cfgent);
                if (offset + off + 1 > 4095) {
                    panic("Too much config options! we only have 4k of env vars!");
                }
                memcpy(&env[offset], cfgent, off);
                offset += off;
                env[offset++] = '\n';
            }
        } while (cfgent);
        cfgent[offset] = 0;
    }

    /// Identity mapping ///
    for (uint64_t i = 0; i < 0x400000000; i += 0x200000) {
        map_page(pmap, i, i, 0x03, true);
    }

    /// Framebuffer init ///
    size_t fbwidth = 0, fbheight = 0, fbbpp = 32;
    struct fb_info fbi;
    char *resolution = config_get_value(config, 0, "RESOLUTION");
    if (resolution != NULL) {
        parse_resolution(&fbwidth, &fbheight, &fbbpp, resolution);
    }

    term_deinit();
    fb_init(&fbi, fbwidth, fbheight, fbbpp);
    uint64_t fb_size = fbi.framebuffer_height * fbi.framebuffer_pitch;

    for (uint64_t current = 0; current < fb_size; current += 0x1000) {
        map_page(pmap, fb_vaddr + current, fbi.framebuffer_addr + current, VMM_FLAG_PRESENT | VMM_FLAG_WRITE, false);
    }

    /// Header info ///
    memcpy(bootboot->magic, "BOOT", 4);

#if bios
    bootboot->protocol = 2 | (0 << 2);
#elif uefi
    bootboot->protocol = 2 | (1 << 2);
#endif

    /// Time stubs ///
    uint32_t year, month, day, hour, minute, second;
    bootboot_time(&day, &month, &year, &second, &minute, &hour);
    bootboot->timezone = 0;
    bootboot->datetime[0] = int_to_bcd(year / 100);
    bootboot->datetime[1] = int_to_bcd(year % 100);
    bootboot->datetime[2] = int_to_bcd(month);
    bootboot->datetime[3] = int_to_bcd(day);
    bootboot->datetime[4] = int_to_bcd(hour);
    bootboot->datetime[5] = int_to_bcd(minute);
    bootboot->datetime[6] = int_to_bcd(second);
    bootboot->datetime[7] = 0;

    /// Ramdisk ///
    bootboot->initrd_ptr = initrd_start;
    bootboot->initrd_size = initrd_size;

    /// Framebuffer ///
    bootboot->fb_ptr = fbi.framebuffer_addr;
    bootboot->fb_size = fb_size;
    bootboot->fb_width = fbi.framebuffer_width;
    bootboot->fb_height = fbi.framebuffer_height;
    bootboot->fb_scanline = fbi.framebuffer_pitch;
    bootboot->fb_type = 1;

    /// SMBIOS and ACPI ///
    uint64_t smbios_entry_32 = 0, smbios_entry_64 = 0;
    acpi_get_smbios((void **)&smbios_entry_32, (void **)&smbios_entry_64);

    bootboot->arch.x86_64.acpi_ptr = (uintptr_t)acpi_get_rsdp();

    if (smbios_entry_64) {
        bootboot->arch.x86_64.smbi_ptr = smbios_entry_64;
    } else if (smbios_entry_32) {
        bootboot->arch.x86_64.smbi_ptr = smbios_entry_32;
    } else {
        bootboot->arch.x86_64.smbi_ptr = 0;
    }

#if uefi == 1
    bootboot->arch.x86_64.efi_ptr = (uintptr_t)gST;
#elif bios == 1
    bootboot->arch.x86_64.efi_ptr = 0;
#endif

    bootboot->arch.x86_64.mp_ptr = 0;

#if uefi == 1
    efi_exit_boot_services();
#endif

    /// SMP info ///
    size_t numcores;
    uint32_t bsplapic;
    volatile struct smp_information *cores;
    init_smp(0, (void **)&cores, &numcores, &bsplapic, true, false, pmap, false, false);
    bootboot->numcores = numcores;
    bootboot->bspid = bsplapic;
    for (size_t i = 0; i < numcores; i++) {
        cores[i].stack_addr = ((uint64_t)(size_t)ext_mem_alloc(init_stack_size)) + init_stack_size;
    }

    /// Memory map ///
    {
        size_t mmapent;
        struct e820_entry_t* e820e = get_memmap(&mmapent);
        if (mmapent > 248) {
            panic("Too many memory map entries");
        }
        for (size_t i = 0; i < mmapent; i++) {
            uint32_t btype;

            switch (e820e[i].type) {
                case 1: btype = 1; break;
                case 3: case 4: btype = 2; break;
                default: btype = 0; break;
            }

            bootboot->mmap[i].size = (e820e[i].length & ~0xF) | btype;
            bootboot->mmap[i].ptr = e820e[i].base;
        }
        bootboot->size = 128 + mmapent * 16;
    }

    /// Spinup ///
#if bios == 1
    // If we're going 64, we might as well call this BIOS interrupt
    // to tell the BIOS that we are entering Long Mode, since it is in
    // the specification.
    struct rm_regs r = {0};
    r.eax = 0xec00;
    r.ebx = 0x02;   // Long mode only
    rm_int(0x15, &r, &r);
#endif

    pic_mask_all();
    io_apic_mask_all();

    irq_flush_type = IRQ_PIC_APIC_FLUSH;

    for (size_t i = 0; i < numcores; i++) {
        cores[i].extra_argument = 0;
        cores[i].goto_address = entry;
    }

    common_spinup(bootboot_spinup_32, 5,
        (uint32_t)(uintptr_t)pmap.top_level,
        (uint32_t)entry, (uint32_t)(entry >> 32),
        (uint32_t)cores[0].stack_addr, (uint32_t)(cores[0].stack_addr >> 32));
}
