#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <protos/bootboot.h>
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

__attribute__((noreturn)) void bootboot_spinup(
                 pagemap_t *pagemap,
                 uint64_t entry_point, uint64_t stack,
                 size_t numcores, struct smp_information* cores);

#define BOOTBOOT_FB     0xfffffffffc000000
#define BOOTBOOT_INFO   0xffffffffffe00000
#define BOOTBOOT_ENV    0xffffffffffe01000
#define BOOTBOOT_CORE   0xffffffffffe02000


__attribute__((noreturn)) void bootboot_spinup_32(
                 uint32_t pagemap_top_lv,
                 uint32_t entry_point_lo, uint32_t entry_point_hi,
                 uint32_t stack_lo, uint32_t stack_hi);

void bootboot_load(char *config) {
#if bios
    void *efi_system_table = NULL;
#elif uefi
    void *efi_system_table = gST;
#endif
    uint64_t fb_vaddr = BOOTBOOT_FB;
    uint64_t struct_vaddr = BOOTBOOT_INFO;
    uint64_t env_vaddr = BOOTBOOT_ENV;
    uint64_t init_stack_size = 1024;

    /// Config ///
    char *kernel_path = config_get_value(config, 0, "KERNEL_PATH");
    if (kernel_path == NULL)
        panic("bootboot: KERNEL_PATH not specified");
    
    char *initrd = config_get_value(config, 0, "INITRD");
    if (initrd == NULL) {
        print("bootboot: warning: no initrd!\n");
    }

    /// Kernel loading code ///
    print("bootboot: Loading kernel `%s`...\n", kernel_path);
    struct file_handle* kernel_file;
    if ((kernel_file = uri_open(kernel_path)) == NULL)
        panic("bootboot: Failed to open kernel with path `%s`. Is the path correct?\n", kernel_path);

    uint8_t* kernel = freadall(kernel_file, MEMMAP_KERNEL_AND_MODULES);
    
    /// Funky macros ///
#define KOFFSET(type, off) (type)&kernel[(off)]
#define ESECTION(idx) KOFFSET(struct elf64_shdr*, elf_header->shoff + elf_header->shdr_size * (idx))

    /// Bootboot symbols ///
    struct elf64_hdr* elf_header = (struct elf64_hdr*)kernel;
    struct elf64_shdr* section_header_strings_section = ESECTION(elf_header->shstrndx);
    char* section_header_strings = KOFFSET(char*, section_header_strings_section->sh_offset);
    struct elf64_shdr* symbol_table = NULL;
    struct elf64_shdr* string_table = NULL;
    for(uint32_t i = 0; i < elf_header->sh_num; i++){
        struct elf64_shdr* section_header = ESECTION(i);
        char* secname = &section_header_strings[section_header->sh_name];
        if(!strcmp(secname, ".symtab")) symbol_table = section_header;
        if(!strcmp(secname, ".strtab")) string_table = section_header;
    }
    if (!symbol_table || !string_table) {
        print("bootboot: warning: no symbol/string tables in the ELF!");
    } else {
        struct elf64_sym* symbols = KOFFSET(struct elf64_sym*, symbol_table->sh_offset);
        char* symbol_strings = KOFFSET(char*, string_table->sh_offset);
        for (uint32_t i = 0, symcount = symbol_table->sh_size / sizeof(struct elf64_sym);i < symcount;i++) {
            char* elf_sym = &symbol_strings[symbols[i].st_name];
            uint64_t symaddr = symbols[i].st_value;

            if(!strcmp(elf_sym, "bootboot")) struct_vaddr = symaddr;
            if(!strcmp(elf_sym, "environment")) env_vaddr = symaddr;
            if(!strcmp(elf_sym, "fb")) fb_vaddr = symaddr;
            if(!strcmp(elf_sym, "initstack")) init_stack_size = symaddr;
        }
    }

    printv("bootboot: mapping struct to %X", struct_vaddr);
    printv("bootboot: mapping environemnt to %X", env_vaddr);
    printv("bootboot: mapping framebuffer to %X", fb_vaddr);
    printv("bootboot: the init stack is %X bytes", init_stack_size);

    uint64_t entry, top, slide, rangecount, physbase, virtbase = 0;
    struct elf_range* ranges;

    /// Memory mappings ///
    pagemap_t pmap = new_pagemap(4);

    /// Load kernel ///
    {
        if (elf64_load(
            kernel, &entry, &top, &slide, MEMMAP_KERNEL_AND_MODULES,
            false, false, &ranges, &rangecount, true, &physbase, &virtbase)) {
            panic("bootboot: elf64 load failed");
        }
        for (uint64_t mapvirt = virtbase, mapphys = physbase; mapphys < top;mapvirt += 0x1000, mapphys += 0x1000) {
            map_page(pmap, mapvirt, mapphys, VMM_FLAG_PRESENT | VMM_FLAG_WRITE, false);
        }
    }
    BOOTBOOT* bootboot = (BOOTBOOT*)ext_mem_alloc_type_aligned(4096, MEMMAP_BOOTLOADER_RECLAIMABLE, 4096);
    map_page(pmap, struct_vaddr, (uint64_t)(size_t)bootboot, VMM_FLAG_PRESENT | VMM_FLAG_WRITE, false);

    /// Environment ///
    {
        char* env = (char*)ext_mem_alloc_type_aligned(4096, MEMMAP_BOOTLOADER_RECLAIMABLE, 4096);
        map_page(pmap, env_vaddr, (uint64_t)(size_t)env, VMM_FLAG_PRESENT | VMM_FLAG_WRITE, false);
        uint32_t index = 0, offset = 0;
        char* cfgent = NULL;
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
    if (resolution != NULL)
        parse_resolution(&fbwidth, &fbheight, &fbbpp, resolution);
    
    term_deinit();
    fb_init(&fbi, fbwidth, fbheight, fbbpp);
    uint64_t fb_size = fbi.framebuffer_height * fbi.framebuffer_pitch;

    for (uint64_t current = 0;current < fb_size;current += 0x1000) {
        map_page(pmap, fb_vaddr + current, fbi.framebuffer_addr + current, VMM_FLAG_PRESENT | VMM_FLAG_WRITE, false);
    }

    /// Initrd loading ///
    uint64_t initrd_start = 0, initrd_size = 0;
    if (initrd) {
        struct file_handle* initrd_file;
        if ((initrd_file = uri_open(initrd)) == NULL)
            panic("bootboot: Failed to open initrd with path `%s`. Is the path correct?\n", initrd);

        uint8_t* initrd_data = freadall(initrd_file, MEMMAP_KERNEL_AND_MODULES);
        initrd_size = initrd_file->size;
        initrd_start = (uint64_t)(size_t)initrd_data;
        fclose(initrd_file);
    }

    /// Header info ///
    memcpy(bootboot->magic, "BOOT", 4);
#if bios
    bootboot->protocol = 2 | (0 << 2);
#elif uefi
    bootboot->protocol = 2 | (1 << 2);
#endif

    /// SMP info ///
    size_t numcores;
    uint32_t bsplapic;
    struct smp_information* cores;
    init_smp(0, (void**)&cores, &numcores, &bsplapic, true, false, pmap, false, false);
    bootboot->numcores = numcores;
    bootboot->bspid = bsplapic;
    for (size_t i = 0;i < numcores;i++) {
        cores[i].stack_addr = ((uint64_t)(size_t)ext_mem_alloc(init_stack_size)) + init_stack_size;
    }

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

    bootboot->arch.x86_64.acpi_ptr = (uint64_t)(size_t)acpi_get_rsdp();
    if (smbios_entry_64) bootboot->arch.x86_64.smbi_ptr = smbios_entry_64;
    else if (smbios_entry_32) bootboot->arch.x86_64.smbi_ptr = smbios_entry_32;
    else bootboot->arch.x86_64.smbi_ptr = 0;
    bootboot->arch.x86_64.efi_ptr = (uint64_t)(size_t)efi_system_table;
    bootboot->arch.x86_64.mp_ptr = 0;
    
    /// Memory map ///
    {
        size_t mmapent;
        struct e820_entry_t* e820e = get_memmap(&mmapent);
        if (mmapent > 248) {
            panic("Too much memory entries! our god bzt decided that %d entries is too much, max is 248", mmapent);
        }
        for (uint32_t i = 0;i < mmapent;i++) {
            uint32_t btype = 0;
            if (e820e[i].type == 1) btype = 1;
            if (e820e[i].type == 3) btype = 2;
            if (e820e[i].type == 4) btype = 2;

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

    irq_flush_type = IRQ_PIC_ONLY_FLUSH;

    for (size_t i = 0;i < numcores;i++) {
        cores[i].extra_argument = 0;
        cores[i].goto_address = entry;
    }

    common_spinup(bootboot_spinup_32, 10,
        (uint32_t)(uintptr_t)pmap.top_level,
        (uint32_t)entry, (uint32_t)(entry >> 32),
        (uint32_t)cores[0].stack_addr, (uint32_t)(cores[0].stack_addr >> 32));

}
