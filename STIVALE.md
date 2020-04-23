# stivale boot protocol specification

The stivale boot protocol aims to be a *simple* to implement protocol which
provides the kernel with most of the features one may need in a *modern*
x86_64 context.

## General information

In order to have a stivale compliant kernel, one must have a kernel executable
in the `elf64` format and have a `.stivalehdr` section (described below).
Other executable formats are not supported.

stivale natively supports and encourages higher half kernels.
The kernel can load itself at `0xffffffff80100000` (as defined in the linker script)
and the bootloader will take care of everything, no AT linker script directives needed.

If the kernel loads itself in the lower half (`0x100000` or higher), the bootloader
will not perform the higher half relocation.

The kernel MUST NOT overwrite anything below `0x100000` (physical memory) as that
is where the bootloader memory structures reside.
Once the kernel is DONE depending on the bootloader (for page tables, structures, ...)
then these areas can be reclaimed if one wants.

The kernel MUST NOT request to load itself at an address lower than `0x100000`
(or `0xffffffff80100000` for higher half kernels) for the same reasons as above.

## Kernel entry machine state

`rip` will be the entry point as defined in the ELF file.

At entry, the bootloader will have setup paging such that there is a 4GiB identity
mapped block of memory at `0x0000000000000000`, a 2GiB mapped area of memory
that maps from `0x0000000000000000` physical to `0x0000000080000000` physical
to `0xffffffff80000000` virtual. This area is for the higher half kernels.
Further more, a 4GiB area of memory from `0x0000000000000000` physical to
`0x0000000100000000` physical to `0xffff800000000000` virtual is mapped.

The kernel should NOT modify the bootloader page tables, and it should only use them
to bootstrap its own virtual memory manager and its own page tables.

At entry all segment registers are loaded as 64 bit code/data segments, limits and
bases are ignored since this is Long Mode.

DO NOT reload segment registers or rely on the provided GDT. The kernel MUST load
its own GDT as soon as possible and not rely on the bootloader's.

The IDT is in an undefined state. Kernel must load its own.

IF flag, VM flag, and direction flag are cleared on entry. Other flags undefined.

PG is enabled (`cr0`), PE is enabled (`cr0`), PAE is enabled (`cr4`),
LME is enabled (`EFER`).

The A20 gate is enabled.

`rsp` is set to the requested stack as per stivale header.

`rdi` will point to the stivale structure (described below).

## stivale header (.stivalehdr)

The kernel executable shall have a section `.stivalehdr` which will contain
the header that the bootloader will parse.

Said header looks like this:
```c
struct stivale_header {
    uint64_t stack;   // This is the stack address which will be in RSP
                      // when the kernel is loaded.

    uint16_t flags;   // Flags
                      // bit 0   0 = text mode,   1 = graphics mode
                      // All other bits undefined.

    uint16_t framebuffer_width;   // These 3 values are parsed if a graphics mode
    uint16_t framebuffer_height;  // is requested. If all values are set to 0
    uint16_t framebuffer_bpp;     // then the bootloader will pick the best possible
                                  // video mode automatically (recommended).
} __attribute__((packed));
```

## stivale structure

The stivale structure returned by the bootloader looks like this:
```c
struct stivale_struct {
    uint64_t cmdline;               // Pointer to a null-terminated cmdline
    uint64_t memory_map_addr;       // Pointer to the memory map (entries described below)
    uint64_t memory_map_entries;    // Count of memory map entries
    uint64_t framebuffer_addr;      // Address of the framebuffer and related info
    uint16_t framebuffer_pitch;
    uint16_t framebuffer_width;
    uint16_t framebuffer_height;
    uint16_t framebuffer_bpp;
    uint64_t rsdp;                  // Pointer to the ACPI RSDP structure
    uint64_t module_count;          // Count of modules that stivale loaded according to config
    uint64_t modules;               // Pointer to the first entry in the linked list of modules (described below)
} __attribute__((packed));
```

## Memory map entry

```c
struct mmap_entry {
    uint64_t base;      // Base of the memory section
    uint64_t length;    // Length of the section
    uint32_t type;      // Type (described below)
    uint32_t unused;
} __attribute__((packed));
```

`type` is an enumeration that can have the following values:

1. Usable RAM
2. Reserved
3. ACPI reclaimable
4. ACPI NVS
5. Bad memory

All other values are undefined.

## Modules

The `modules` variable points to the first entry of the linked list of module
structures.
A module structure looks like this:
```c
struct stivale_module {
    uint64_t begin;         // Address where the module is loaded
    uint64_t end;           // End address of the module
    char     string[128];   // String passed to the module (by config file)
    uint64_t next;          // Pointer to the next module (if any), check module_count
                            // in the stivale_struct
} __attribute__((packed));
```
