#include <stivale2.h>
#include <stdint.h>
#include <e9print.h>
#include <stddef.h>

typedef uint8_t stack[4096];
static stack stacks[10] = {0};
void stivale2_main(struct stivale2_struct *info);

struct stivale2_header_tag_terminal terminal_request = {
    .tag = {
        .identifier = STIVALE2_HEADER_TAG_TERMINAL_ID,
        .next       = 0
    },
    .flags = 0
};

struct stivale2_header_tag_smp smp_request = {
    .tag = {
        .identifier = STIVALE2_HEADER_TAG_SMP_ID,
        .next       = (uint64_t)&terminal_request
    },
    .flags = 0
};

struct stivale2_tag unmap_null_request = {
    .identifier = STIVALE2_HEADER_TAG_UNMAP_NULL_ID,
    .next       = (uint64_t)&smp_request
};

struct stivale2_header_tag_framebuffer framebuffer_request = {
    .tag = {
        .identifier = STIVALE2_HEADER_TAG_FRAMEBUFFER_ID,
        .next       = (uint64_t)&unmap_null_request
    },
    .framebuffer_width  = 0,
    .framebuffer_height = 0,
    .framebuffer_bpp    = 0,
};

__attribute__((section(".stivale2hdr"), used))
struct stivale2_header header2 = {
    .entry_point = (uint64_t)stivale2_main,
    .stack       = (uintptr_t)stacks[0] + sizeof(stack),
    .flags       = (1 << 1) | (1 << 2),
    .tags        = (uint64_t)&framebuffer_request
};

static volatile int cpu_up = 0;

static void ap_entry(struct stivale2_smp_info *s) {
    e9_printf("            AP %d started", s->lapic_id);
    cpu_up = 1;
    for (;;) asm("hlt");
}

void stivale2_main(struct stivale2_struct *info) {
    // Print the tags.
    struct stivale2_tag *tag = (struct stivale2_tag *)info->tags;

    while (tag != NULL) {
        if (tag->identifier == STIVALE2_STRUCT_TAG_TERMINAL_ID) {
            struct stivale2_struct_tag_terminal *t = (void *)tag;
            stivale2_print = (void *)(uintptr_t)t->term_write;
        }
        tag = (void *)tag->next;
    }

    // Print stuff.
    e9_puts("Stivale2 info passed to the kernel:");
    e9_printf("Bootloader brand:   %s", info->bootloader_brand);
    e9_printf("Bootloader version: %s", info->bootloader_version);

    // Print the tags.
    tag = (struct stivale2_tag *)info->tags;

    while (tag != NULL) {
        switch (tag->identifier) {
            case STIVALE2_STRUCT_TAG_CMDLINE_ID: {
                struct stivale2_struct_tag_cmdline *c = (struct stivale2_struct_tag_cmdline *)tag;
                e9_puts("Commandline tag:");
                e9_printf("\tCmdline: %s", (char*)(c->cmdline));
                break;
            }
            case STIVALE2_STRUCT_TAG_MEMMAP_ID: {
                struct stivale2_struct_tag_memmap *m = (struct stivale2_struct_tag_memmap *)tag;
                e9_puts("Memmap tag:");
                e9_printf("\tEntries: %d", m->entries);
                size_t total_mem = 0;
                for (size_t i = 0; i < m->entries; i++) {
                    struct stivale2_mmap_entry me = m->memmap[i];
                    e9_printf("\t\t[%x+%x] %x", me.base, me.length, me.type);
                    if (m->memmap[i].type == 1) {
                        total_mem += m->memmap[i].length;
                    }
                }
                e9_printf("\tTotal usable memory: %d", total_mem);
                break;
            }
            case STIVALE2_STRUCT_TAG_FRAMEBUFFER_ID: {
                struct stivale2_struct_tag_framebuffer *f = (struct stivale2_struct_tag_framebuffer *)tag;
                e9_puts("Framebuffer tag:");
                e9_printf("\tAddress: %x", f->framebuffer_addr);
                e9_printf("\tWidth:   %d", f->framebuffer_width);
                e9_printf("\tHeight:  %d", f->framebuffer_height);
                e9_printf("\tPitch:   %d", f->framebuffer_pitch);
                e9_printf("\tBPP:     %d", f->framebuffer_bpp);
                e9_printf("\tMemory model:    %d", f->memory_model);
                e9_printf("\tRed mask size:   %d", f->red_mask_size);
                e9_printf("\tRed mask size:   %d", f->red_mask_shift);
                e9_printf("\tGreen mask size: %d", f->green_mask_size);
                e9_printf("\tGreen mask size: %d", f->green_mask_shift);
                e9_printf("\tBlue mask size:  %d", f->blue_mask_size);
                e9_printf("\tBlue mask size:  %d", f->blue_mask_shift);
                break;
            }
            case STIVALE2_STRUCT_TAG_EDID_ID: {
                struct stivale2_struct_tag_edid *edid = (struct stivale2_struct_tag_edid *)tag;

                e9_printf("EDID information at %x:", edid->edid_information);
                e9_printf("\tSize: %d", edid->edid_size);
                break;
            }
            case STIVALE2_STRUCT_TAG_FB_MTRR_ID: {
                e9_puts("Framebuffer WC MTRR tag:");
                e9_puts("\tFramebuffer WC MTRR enabled");
                break;
            }
            case STIVALE2_STRUCT_TAG_TERMINAL_ID: {
                struct stivale2_struct_tag_terminal *term = (struct stivale2_struct_tag_terminal *)tag;

                e9_puts("Terminal tag:");
                e9_printf("\tTerminal write entry point at: %x", term->term_write);
                break;
            }
            case STIVALE2_STRUCT_TAG_MODULES_ID: {
                struct stivale2_struct_tag_modules *m = (struct stivale2_struct_tag_modules *)tag;
                e9_puts("Modules tag:");
                e9_printf("\tCount: %d", m->module_count);
                for (size_t i = 0; i < m->module_count; i++) {
                    struct stivale2_module me = m->modules[i];
                    e9_printf("\t\t[%x+%x] %s", me.begin, me.end, me.string);
                }
                break;
            }
            case STIVALE2_STRUCT_TAG_RSDP_ID: {
                struct stivale2_struct_tag_rsdp *r = (struct stivale2_struct_tag_rsdp *)tag;
                e9_puts("RSDP tag:");
                e9_printf("\tRSDP: %x", r->rsdp);
                break;
            }
            case STIVALE2_STRUCT_TAG_SMBIOS_ID: {
                struct stivale2_struct_tag_smbios *r = (struct stivale2_struct_tag_smbios *)tag;
                e9_puts("SMBIOS tag:");
                e9_printf("\tFlags: %x", r->flags);
                e9_printf("\tSMBIOS 32-bit entry point: %x", r->smbios_entry_32);
                e9_printf("\tSMBIOS 64-bit entry point: %x", r->smbios_entry_64);
                break;
            }
            case STIVALE2_STRUCT_TAG_EPOCH_ID: {
                struct stivale2_struct_tag_epoch *e = (struct stivale2_struct_tag_epoch *)tag;
                e9_puts("Epoch tag:");
                e9_printf("\tEpoch: %x", e->epoch);
                break;
            }
            case STIVALE2_STRUCT_TAG_FIRMWARE_ID: {
                struct stivale2_struct_tag_firmware *f = (struct stivale2_struct_tag_firmware *)tag;
                e9_puts("Firmware tag:");
                e9_printf("\tFlags: %x", f->flags);
                break;
            }
            case STIVALE2_STRUCT_TAG_EFI_SYSTEM_TABLE_ID: {
                struct stivale2_struct_tag_efi_system_table *t = (struct stivale2_struct_tag_efi_system_table *)tag;
                e9_printf("EFI system table at: %x", t->system_table);
                break;
            }
            case STIVALE2_STRUCT_TAG_KERNEL_FILE_ID: {
                struct stivale2_struct_tag_kernel_file *t = (struct stivale2_struct_tag_kernel_file *)tag;
                e9_printf("Raw kernel file loaded at: %x", t->kernel_file);
                break;
            }
            case STIVALE2_STRUCT_TAG_KERNEL_FILE_V2_ID: {
                struct stivale2_struct_tag_kernel_file_v2 *t = (struct stivale2_struct_tag_kernel_file_v2 *)tag;
                e9_puts("Kernel file V2 tag:");
                e9_printf("\tkernel_file: %x", t->kernel_file);
                e9_printf("\tkernel_size: %x", t->kernel_size);
                break;
            }
            case STIVALE2_STRUCT_TAG_KERNEL_SLIDE_ID: {
                struct stivale2_struct_tag_kernel_slide *t = (struct stivale2_struct_tag_kernel_slide *)tag;
                e9_printf("Kernel slide: %x", t->kernel_slide);
                break;
            }
            case STIVALE2_STRUCT_TAG_PMRS_ID: {
                struct stivale2_struct_tag_pmrs *t = (struct stivale2_struct_tag_pmrs *)tag;
                e9_puts("PMRs tag:");
                e9_printf("\t%d ranges:", t->entries);
                for (size_t i = 0; i < t->entries; i++) {
                    e9_printf("\t\tBase: %x  Length: %x  Perms: %x",
                          t->pmrs[i].base, t->pmrs[i].length, t->pmrs[i].permissions);
                }
                break;
            }
            case STIVALE2_STRUCT_TAG_SMP_ID: {
                struct stivale2_struct_tag_smp *s = (struct stivale2_struct_tag_smp *)tag;
                e9_puts("SMP tag:");
                e9_printf("\tFlags:        %x", s->flags);
                e9_printf("\tBSP LAPIC ID: %d", s->bsp_lapic_id);
                e9_printf("\tCPU Count:    %d", s->cpu_count);
                for (size_t i = 0; i < s->cpu_count; i++) {
                    struct stivale2_smp_info *in = &s->smp_info[i];
                    e9_printf("\t\tProcessor ID:   %d", in->processor_id);
                    e9_printf("\t\tLAPIC ID:       %d", in->lapic_id);
                    e9_printf("\t\tTarget Stack:   %x", in->target_stack);
                    e9_printf("\t\tGOTO Address:   %x", in->goto_address);
                    e9_printf("\t\tExtra Argument: %x", in->extra_argument);
                    if (in->lapic_id != s->bsp_lapic_id) {
                        in->target_stack = (uintptr_t)stacks[in->lapic_id] + sizeof(stack);
                        in->goto_address = (uintptr_t)ap_entry;
                        while (cpu_up == 0);
                        cpu_up = 0;
                    }
                }
                break;
            }
            default:
                e9_printf("BUG: Unidentified tag %x", tag->identifier);
        }

        tag = (struct stivale2_tag *)tag->next;
    }

    // Enter our sublime pale slumber.
    for (;;) asm("hlt");
}
