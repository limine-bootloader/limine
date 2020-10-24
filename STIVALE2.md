# stivale2 boot protocol specification

The stivale2 boot protocol is an improved version of the stivale protocol which
provides the kernel with most of the features one may need in a *modern*
x86_64 context (although 32-bit x86 is also supported).

## General information

In order to have a stivale2 compliant kernel, one must have a kernel executable
in the `elf64` or `elf32` format and have a `.stivale2hdr` section (described below).
Other executable formats are not supported.

stivale2 will recognise whether the ELF file is 32-bit or 64-bit and load the kernel
into the appropriate CPU mode.

stivale2 natively supports (only for 64-bit kernels) and encourages higher half kernels.
The kernel can load itself at `0xffffffff80000000` or higher (as defined in the linker script)
and the bootloader will take care of everything, no AT linker script directives needed.

If the kernel loads itself in the lower half, the bootloader will not perform the
higher half relocation.

*Note: In order to maintain compatibility with Limine and other stivale2-compliant*
*bootloaders it is strongly advised never to load the kernel or any of its*
*sections below the 1 MiB physical memory mark. This may work with some stivale2*
*loaders, but it WILL NOT work with Limine and it's explicitly discouraged.*

## Kernel entry machine state

### 64-bit kernel

`rip` will be the entry point as defined in the ELF file, unless the `entry_point`
field in the stivale2 header is set to a non-0 value, in which case, it is set to
the value of `entry_point`.

At entry, the bootloader will have setup paging mappings as such:

```
 Base Physical Address -                    Size                    ->  Virtual address
  0x0000000000000000   - 4 GiB plus any additional memory map entry -> 0x0000000000000000
  0x0000000000000000   - 4 GiB plus any additional memory map entry -> 0xffff800000000000 (4-level paging only)
  0x0000000000000000   - 4 GiB plus any additional memory map entry -> 0xff00000000000000 (5-level paging only)
  0x0000000000000000   -                 0x80000000                 -> 0xffffffff80000000
```

If the kernel is dynamic and not statically linked, the bootloader will relocate it.
Furthermore if bit 0 of the flags field in the stivale2 header is set, the bootloader
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
If the stivale2 header tag for 5-level paging is present, then, if available,
5-level paging is enabled (LA57 bit in `cr4`).

The A20 gate is enabled.

PIC/APIC IRQs are all masked.

`rsp` is set to the requested stack as per stivale2 header. If the requested value is
non-null, an invalid return address of 0 is pushed to the stack before jumping
to the kernel.

`rdi` will point to the stivale2 structure (described below).

All other general purpose registers are set to 0.

### 32-bit kernel

`eip` will be the entry point as defined in the ELF file, unless the `entry_point`
field in the stivale2 header is set to a non-0 value, in which case, it is set to
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

`esp` is set to the requested stack as per stivale2 header. An invalid return address
of 0 is pushed to the stack before jumping to the kernel.

A pointer to the stivale2 structure (described below) is pushed onto this stack
before the entry point is called.

All other general purpose registers are set to 0.

## Bootloader-reserved memory

In order for stivale2 to function, it needs to reserve memory areas for either internal
usage (such as page tables, GDT, SMP), or for kernel interfacing (such as returned
structures).

stivale2 ensures that none of these areas are found in any of the sections
marked as "usable" in the memory map.

The location of these areas may vary and it is implementation specific;
these areas may be in any non-usable memory map section, or in unmarked memory.

The OS must make sure to be done consuming bootloader information and services
before switching to its own address space, as unmarked memory areas in use by
the bootloader may become unavailable.

Once the OS is done needing the bootloader, memory map areas marked as "bootloader
reclaimable" may be used as usable memory. These areas are not guaranteed to be
aligned, but they are guaranteed to not overlap other sections of the memory map.

## stivale2 header (.stivale2hdr)

The kernel executable shall have a section `.stivale2hdr` which will contain
the header that the bootloader will parse.

Said header looks like this:
```c
struct stivale2_header {
    uint64_t entry_point;   // If not 0, this address will be jumped to as the
                            // entry point of the kernel.
                            // If set to 0, the ELF entry point will be used
                            // instead.

    uint64_t stack;         // This is the stack address which will be in ESP/RSP
                            // when the kernel is loaded.
                            // It can only be set to NULL for 64-bit kernels. 32-bit
                            // kernels are mandated to provide a vaild stack.
                            // 64-bit and 32-bit valid stacks must be at least 256 bytes
                            // in usable space and must be 16 byte aligned addresses.

    uint64_t flags;         // Bit 0: if 1, enable KASLR
                            // All other bits undefined

    uint64_t tags;          // Pointer to the first of the linked list of tags.
                            // see "stivale2 header tags" section.
                            // NULL = no tags.
} __attribute__((packed));
```

### stivale2 header tags

The stivale2 header uses a mechanism to avoid having protocol versioning, but
rather, feature-specific support detection.

The kernel executable provides the bootloader with a linked list of structures,
the first of which is pointed to by the `tags` entry of the stivale2 header.

Each tag shall contain these 2 fields:
```c
struct stivale2_hdr_tag {
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

This tag asks the stivale2-compliant bootloader to initialise a graphical framebuffer
video mode.
Omitting this tag will make the bootloader default to a CGA-compatible text mode,
if supported.

```c
struct stivale2_header_tag_framebuffer {
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

#### SMP header tag

The presence of this tag enables support for booting up application processors.

```c
struct stivale2_header_tag_smp {
    uint64_t identifier;          // Identifier: 0x1ab015085f3273df
    uint64_t next;
    uint64_t flags;               // Flags:
                                  //   bit 0: 0 = use xAPIC, 1 = use x2APIC (if available)
                                  // All other flags are undefined.
} __attribute__((packed));
```

## stivale2 structure

The stivale2 structure returned by the bootloader looks like this:
```c
struct stivale2_struct {
    char bootloader_brand[64];    // Bootloader null-terminated brand string
    char bootloader_version[64];  // Bootloader null-terminated version string

    uint64_t tags;          // Pointer to the first of the linked list of tags.
                            // see "stivale2 structure tags" section.
                            // NULL = no tags.
} __attribute__((packed));
```

### stivale2 structure tags

These tags work *very* similarly to the header tags, with the main difference being
that these tags are returned to the kernel by the bootloader, instead.

See "stivale2 header tags".

The kernel is responsible for parsing the tags and the identifiers, and interpreting
the tags that it supports, while handling in a graceful manner the tags it does not
recognise.

#### Command line structure tag

This tag reports to the kernel the command line string that was passed to it by
the bootloader.

```c
struct stivale2_struct_tag_cmdline {
    uint64_t identifier;          // Identifier: 0xe5e76a1b4597a781
    uint64_t next;
    uint64_t cmdline;             // Pointer to a null-terminated cmdline
} __attribute__((packed));
```

#### Memory map structure tag

This tag reports to the kernel the memory map built by the bootloader.

```c
struct stivale2_struct_tag_memmap {
    uint64_t identifier;          // Identifier: 0x2187f79e8612de07
    uint64_t next;
    uint64_t entries;             // Count of memory map entries
    struct stivale2_mmap_entry memmap[];  // Array of memory map entries
} __attribute__((packed));
```

###### Memory map entry

```c
struct stivale2_mmap_entry {
    uint64_t base;      // Base of the memory section
    uint64_t length;    // Length of the section
    enum stivale2_mmap_type type;  // Type (described below)
    uint32_t unused;
} __attribute__((packed));
```

`type` is an enumeration that can have the following values:

```
enum stivale2_mmap_type : uint32_t {
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
struct stivale2_struct_tag_framebuffer {
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
struct stivale2_struct_tag_modules {
    uint64_t identifier;          // Identifier: 0x4b6fe466aade04ce
    uint64_t next;
    uint64_t module_count;        // Count of loaded modules
    struct stivale2_module modules[]; // Array of module descriptors
} __attribute__((packed));
```

```c
struct stivale2_module {
    uint64_t begin;         // Address where the module is loaded
    uint64_t end;           // End address of the module
    char string[128];       // 0-terminated string passed to the module
} __attribute__((packed));
```

#### RSDP structure tag

This tag reports to the kernel the location of the ACPI RSDP structure in memory.

```c
struct stivale2_struct_tag_rsdp {
    uint64_t identifier;        // Identifier: 0x9e1786930a375e78
    uint64_t next;
    uint64_t rsdp;              // Pointer to the ACPI RSDP structure
} __attribute__((packed));
```

#### Epoch structure tag

This tag reports to the kernel the current UNIX epoch, as per RTC.

```c
struct stivale2_struct_tag_epoch {
    uint64_t identifier;        // Identifier: 0x566a7bed888e1407
    uint64_t next;
    uint64_t epoch;             // UNIX epoch at boot, read from system RTC
} __attribute__((packed));
```

#### Firmware structure tag

This tag reports to the kernel info about the firmware.

```c
struct stivale2_struct_tag_firmware {
    uint64_t identifier;        // Identifier: 0x359d837855e3858c
    uint64_t next;
    uint64_t flags;             // Bit 0: 0 = UEFI, 1 = BIOS
} __attribute__((packed));
```

#### SMP structure tag

This tag reports to the kernel info about a multiprocessor environment.

```c
struct stivale2_struct_tag_smp {
    uint64_t identifier;        // Identifier: 0x34d1d96339647025
    uint64_t next;
    uint64_t flags;             // Flags:
                                //   bit 0: Set if x2APIC was requested and it
                                //          was supported and enabled.
                                //  All other bits undefined.
    uint64_t cpu_count;         // Total number of logical CPUs (including BSP)
    struct stivale2_smp_info smp_info[];  // Array of smp_info structs, one per
                                          // logical processor, including BSP.
} __attribute__((packed));
```

*Note: In the code below, the BSP refers to the bootstrap processor,*
*AKA the processor that the system was started with, and the one whose*
*control is handed to by stivale2 first.*

*The LAPIC ID of the BSP is in most cases `0`, but this is not guaranteed.*
*To get the LAPIC ID of the BSP, see `CPUID` leaf `1`, and in case the*
*x2APIC is used, see `CPUID` leaves `0x1f` and `0xb`. Note that the `CPUID`*
*instruction has to be executed on the BSP itself.*

```c
struct stivale2_smp_info {
    uint32_t acpi_processor_uid; // ACPI Processor UID as specified by MADT
    uint32_t lapic_id;           // LAPIC ID as specified by MADT
    uint64_t target_stack;       // The stack that will be loaded in ESP/RSP
                                 // once the goto_address field is loaded.
                                 // This MUST point to a valid stack of at least
                                 // 256 bytes in size, and 16-byte aligned.
                                 // target_stack is an unused field for the
                                 // struct describing the BSP (lapic_id == 0)
    uint64_t goto_address;       // This address is polled by the started APs
                                 // until the kernel on another CPU performs an
                                 // atomic write to this field.
                                 // When that happens, bootloader code will
                                 // load up ESP/RSP with the stack value as
                                 // specified in target_stack.
                                 // It will then proceed to load a pointer to
                                 // this very structure into either register
                                 // RDI for 64-bit or on the stack for 32-bit,
                                 // then, goto_address is called (a bogus return
                                 // address is pushed onto the stack) and execution
                                 // is handed off.
                                 // The CPU state will be the same as described
                                 // in kernel entry machine state, with the exception
                                 // of ESP/RSP and RDI/stack arg being set up as
                                 // above.
                                 // goto_address is an unused field for the
                                 // struct describing the BSP.
    uint64_t extra_argument;     // This field is here for the kernel to use
                                 // for whatever it wants. Writes here should
                                 // be performed before writing to goto_address
                                 // so that the receiving processor can safely
                                 // retrieve the data.
                                 // extra_argument is an unused field for the
                                 // struct describing the BSP.
} __attribute__((packed));
```

#### MMIO32 UART tag

This tag reports that there is a memory mapped UART port and its address. To write to this port, write the character, zero extended to a 32 bit unsigned integer to the address provided.

```c
struct stivale2_struct_tag_mmio32_uart {
    uint64_t identifier;        // Identifier: 0xb813f9b8dbc78797
    uint64_t next;
    uint64_t addr;              // The address of the UART port
} __attribute__((packed));
```

#### Device tree blob tag

This tag describes a device tree blob for the platform.

```c
struct stivale2_struct_tag_dtb {
    uint64_t identifier;        // Identifier: 0xabb29bd49a2833fa
    uint64_t next;
    uint64_t addr;              // The address of the dtb
    uint64_t size;              // The size of the dtb
} __attribute__((packed));
```
