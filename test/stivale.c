#include <stivale.h>
#include <stdint.h>
#include <e9print.h>
#include <stddef.h>

static uint8_t stack[4096] = {0};
void stivale_main(struct stivale_struct *info);

__attribute__((section(".stivalehdr"), used))
struct stivale_header header = {
    .stack              = (uint64_t)(uintptr_t)stack + sizeof(stack),
    .framebuffer_bpp    = 0,
    .framebuffer_width  = 0,
    .framebuffer_height = 0,
    .flags              = 1,
    .entry_point        = (uint64_t)(uintptr_t)stivale_main
};

void stivale_main(struct stivale_struct *info) {
    // Print some info.
    e9_puts("Stivale information passed to the kernel:");
    e9_printf("Cmdline: %s", (char*)info->cmdline);
    e9_printf("Memory map at %x with contents:", info->memory_map_addr);

    struct stivale_mmap_entry *memmap = ((struct stivale_mmap_entry *)(info->memory_map_addr));
    for (size_t i = 0; i < info->memory_map_entries; i++) {
        struct stivale_mmap_entry e = memmap[i];
        e9_printf("\tEntry %d: [%x+%x] %x", i, e.base, e.length, e.type);
    }

    e9_printf("Framebuffer at %x with specifics:", info->framebuffer_addr);
    e9_printf("\tPitch:  %d", info->framebuffer_pitch);
    e9_printf("\tWidth:  %d", info->framebuffer_width);
    e9_printf("\tHeight: %d", info->framebuffer_height);
    e9_printf("\tBPP:    %d", info->framebuffer_bpp);
    e9_printf("RSDP at %x", info->rsdp);

    e9_printf("Module map at %x with modules:", info->modules);
    struct stivale_module *modules = ((struct stivale_module *)(info->modules));
    for (size_t i = 0; i < info->module_count; i++) {
        struct stivale_module e = *modules;
        e9_printf("\tModule %d: [%x+%x] %s", i, e.begin, e.end, e.string);
        modules = (struct stivale_module *)e.next;
    }

    e9_printf("Epoch is  %x", info->epoch);
    e9_printf("Flags are %x", info->flags);

    // Guru meditation.
    for (;;);
}
