# stivale boot protocol specification

The stivale boot protocol aims to be a *simple* to implement protocol which
provides the kernel with most of the features one may need in a *modern*
x86_64 context (although 32-bit x86 is also supported).

## General information

In order to have a stivale compliant kernel, one must have a kernel executable
in the `elf64` or `elf32` format and have a `.stivalehdr` section (described below).
Other executable formats are not supported.

stivale will recognise whether the ELF file is 32-bit or 64-bit and load the kernel
into the appropriate CPU mode.

stivale natively supports (only for 64-bit kernels) and encourages higher half kernels.
The kernel can load itself at `0xffffffff80100000` or higher (as defined in the linker script)
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

### 64-bit kernel

`rip` will be the entry point as defined in the ELF file, unless the `entry_point`
field in the stivale header is set to a non-0 value, in which case, it is set to
the value of `entry_point`.

At entry, the bootloader will have setup paging mappings as such:

```
 Base Physical Address -  Top Physical Address  ->  Virtual address
  0x0000000000000000   -   0x0000000100000000   ->  0x0000000000000000
  0x0000000000000000   -   0x0000000100000000   ->  0xffff800000000000
  0x0000000000000000   -   0x0000000080000000   ->  0xffffffff80000000
```

If the kernel is dynamic and not statically linked, the bootloader will relocate it.
Furthermore if bit 0 of the flags field in the stivale header is set, the bootloader
will perform kernel address space layout randomisation (KASLR).

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
If the stivale header tag for 5-level paging is present, then, if available,
5-level paging is enabled (LA57 bit in `cr4`).

The A20 gate is enabled.

PIC/APIC IRQs are all masked.

`rsp` is set to the requested stack as per stivale header.

`rdi` will point to the stivale structure (described below).

All other general purpose registers are set to 0.

### 32-bit kernel

`eip` will be the entry point as defined in the ELF file, unless the `entry_point`
field in the stivale header is set to a non-0 value, in which case, it is set to
the value of `entry_point`.

At entry all segment registers are loaded as 32 bit code/data segments.
All segment bases are `0x00000000` and all limits are `0xffffffff`.

DO NOT reload segment registers or rely on the provided GDT. The kernel MUST load
its own GDT as soon as possible and not rely on the bootloader's.

The IDT is in an undefined state. Kernel must load its own.

IF flag, VM flag, and direction flag are cleared on entry. Other flags undefined.

PE is enabled (`cr0`).

The A20 gate is enabled.

PIC/APIC IRQs are all masked.

`esp` is set to the requested stack as per stivale header.

A pointer to the stivale structure (described below) is pushed onto this stack
before the entry point is called.

All other general purpose registers are set to 0.

## stivale header (.stivalehdr)

The kernel executable shall have a section `.stivalehdr` which will contain
the header that the bootloader will parse.

Said header looks like this:
```c
struct stivale_header {
    uint64_t entry_point;   // If not 0, this address will be jumped to as the
                            // entry point of the kernel.
                            // If set to 0, the ELF entry point will be used
                            // instead.

    uint64_t stack;         // This is the stack address which will be in RSP
                            // when the kernel is loaded.
                            // It can be set to a non-valid stack address such as 0
                            // as long as the OS is 64-bit and sets up a stack on its
                            // own.

    uint64_t flags;         // Bit 0: if 1, enable KASLR
                            // All other bits undefined

    uint64_t tags;          // Pointer to the first of the linked list of tags.
                            // see "stivale header tags" section.
                            // NULL = no tags.
} __attribute__((packed));
```

### stivale header tags

The stivale header uses a mechanism to avoid having protocol versioning, but
rather, feature-specific support detection.

The kernel executable provides the bootloader with a linked list of structures,
the first of which is pointed to by the `tags` entry of the stivale header.

Each tag shall contain these 2 fields:
```c
struct stivale_hdr_tag {
    uint64_t identifier;
    uint64_t next;
} __attribute__((packed));

```

The `identifier` field identifies what feature the tag is requesting from the
bootloader.

The `next` field points to another tag in the linked list. A NULL value determines
the end of the linked list.

Tag structures can have more than just these 2 members, but these 2 members MUST
appear at the beginning of any given tag.

Tags can have no extra members and just serve as "flags" to enable some behaviour
that does not require extra parameters.

#### Framebuffer header tag

This tag asks the stivale-compliant bootloader to initialise a graphical framebuffer
video mode.
Omitting this tag will make the bootloader default to a CGA-compatible text mode,
if supported.

```c
struct stivale_hdr_tag_framebuffer {
    uint64_t identifier;          // Identifier: 0x3ecc1bc43d0f7971
    uint64_t next;
    uint16_t framebuffer_width;   // If all values are set to 0
    uint16_t framebuffer_height;  // then the bootloader will pick the best possible
    uint16_t framebuffer_bpp;     // video mode automatically.
} __attribute__((packed));
```

#### 5-level paging header tag

The presence of this tag enables support for 5-level paging, if available.

Identifier: `0x932f477032007e8f`

This tag does not have extra members.

## stivale structure

The stivale structure returned by the bootloader looks like this:
```c
struct stivale_struct {
    char bootloader_brand[64];    // Bootloader null-terminated brand string
    char bootloader_version[64];  // Bootloader null-terminated version string

    uint64_t tags;          // Pointer to the first of the linked list of tags.
                            // see "stivale structure tags" section.
                            // NULL = no tags.
} __attribute__((packed));
```

### stivale structure tags

These tags work *very* similarly to the header tags, with the main difference being
that these tags are returned to the kernel by the bootloader, instead.

See "stivale header tags".

The kernel is responsible for parsing the tags and the identifiers, and interpreting
the tags that it supports, while handling in a graceful manner the tags it does not
recognise.

#### Command line structure tag

This tag reports to the kernel the command line string that was passed to it by
the bootloader.

```c
struct stivale_struct_tag_cmdline {
    uint64_t identifier;          // Identifier: 0xe5e76a1b4597a781
    uint64_t next;
    uint64_t cmdline;             // Pointer to a null-terminated cmdline
} __attribute__((packed));
```

#### Memory map structure tag

This tag reports to the kernel the memory map built by the bootloader.

```c
struct stivale_struct_tag_memmap {
    uint64_t identifier;          // Identifier: 0x2187f79e8612de07
    uint64_t next;
    uint64_t entries;             // Count of memory map entries
    struct stivale_mmap_entry memmap[];  // Array of memory map entries
} __attribute__((packed));
```

###### Memory map entry

```c
struct stivale_mmap_entry {
    uint64_t base;      // Base of the memory section
    uint64_t length;    // Length of the section
    enum stivale_mmap_type type;  // Type (described below)
    uint32_t unused;
} __attribute__((packed));
```

`type` is an enumeration that can have the following values:

```
enum stivale_mmap_type : uint32_t {
    USABLE                 = 1,
    RESERVED               = 2,
    ACPI_RECLAIMABLE       = 3,
    ACPI_NVS               = 4,
    BAD_MEMORY             = 5,
    BOOTLOADER_RECLAIMABLE = 0x1000,
    KERNEL_AND_MODULES     = 0x1001
};
```

All other values are undefined.

The kernel and modules loaded **are not** marked as usable memory. They are marked
as Kernel/Modules (type 0x1001).

Usable RAM chunks are guaranteed to be 4096 byte aligned for both base and length.

The entries are guaranteed to be sorted by base address, lowest to highest.

Usable RAM chunks are guaranteed not to overlap with any other entry.

To the contrary, all non-usable RAM chunks are not guaranteed any alignment, nor
is it guaranteed that they do not overlap each other (except usable RAM).

#### Framebuffer structure tag

This tag reports to the kernel the currently set up framebuffer details, if any.

```c
struct stivale_struct_tag_framebuffer {
    uint64_t identifier;          // Identifier: 0x506461d2950408fa
    uint64_t next;
    uint64_t framebuffer_addr;    // Address of the framebuffer and related info
    uint16_t framebuffer_width;
    uint16_t framebuffer_height;
    uint16_t framebuffer_pitch;
    uint16_t framebuffer_bpp;
} __attribute__((packed));
```

#### Modules structure tag

This tag lists modules that the bootloader loaded alongside the kernel, if any.

```c
struct stivale_struct_tag_modules {
    uint64_t identifier;          // Identifier: 0x4b6fe466aade04ce
    uint64_t next;
    uint64_t module_count;        // Count of loaded modules
    struct stivale_module modules[]; // Array of module descriptors
} __attribute__((packed));
```

```c
struct stivale_module {
    uint64_t begin;         // Address where the module is loaded
    uint64_t end;           // End address of the module
    char string[128];       // 0-terminated string passed to the module
} __attribute__((packed));
```

#### RSDP structure tag

This tag reports to the kernel the location of the ACPI RSDP structure in memory.

```c
struct stivale_struct_tag_rsdp {
    uint64_t identifier;        // Identifier: 0x9e1786930a375e78
    uint64_t next;
    uint64_t rsdp;              // Pointer to the ACPI RSDP structure
} __attribute__((packed));
```

#### Epoch structure tag

This tag reports to the kernel the current UNIX epoch, as per RTC.

```c
struct stivale_struct_tag_epoch {
    uint64_t identifier;        // Identifier: 0x566a7bed888e1407
    uint64_t next;
    uint64_t epoch;             // UNIX epoch at boot, read from system RTC
} __attribute__((packed));
```

#### Firmware structure tag

This tag reports to the kernel info about the firmware.

```c
struct stivale_struct_tag_firmware {
    uint64_t identifier;        // Identifier: 0x359d837855e3858c
    uint64_t next;
    uint64_t flags;             // Bit 0: 0 = UEFI, 1 = BIOS
} __attribute__((packed));
```
