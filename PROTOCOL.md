# The Limine Boot Protocol

The Limine boot protocol is a modern, minimal, fast, and extensible boot
protocol, with a focus on backwards and forwards compatibility,
created from the experience gained by working on the
[stivale boot protocols](https://github.com/stivale).

This file serves as the official centralised collection of features that
the Limine boot protocol is composed of. Other bootloaders may support extra
unofficial features, but it is strongly recommended to avoid fragmentation
and submit new features by opening a pull request to this repository.

The [limine.h](/limine.h) file provides an implementation of all the
structures and constants described in this document, for the C and C++
languages.

## General Notes

All pointers are 64-bit wide. All pointers point to the object with the
higher half direct map offset already added to them, unless otherwise noted.

### Executable formats

The Limine protocol does not enforce any specific executable format, but
kernels using formats not supported by the bootloader, or using flat binaries,
*must* provide a Executable Layout Feature (see below).

Compliant bootloaders must support at least the ELF 64-bit executable format.

## Features

The protocol is centered around the concept of request/response - collectively
named "features" - where the kernel requests some action or information from
the bootloader, and the bootloader responds accordingly, if it is capable of
doing so.

In C terms, a feature is composed of 2 structure: the request, and the response.

A request has 3 mandatory members at the beginning of the structure:
```c
struct limine_example_request {
    uint64_t id[4];
    uint64_t revision;
    struct limine_example_response *response;
    ... optional members follow ...
};
```
* `id` - The ID of the request. This is an 8-byte aligned magic number that the
bootloader will scan for inside the executable file to find requests. Requests
may be located anywhere inside the executable as long as they are 8-byte
aligned. There may only be 1 of the same request. The bootloader will refuse
to boot an executable with multiple of the same request IDs.
* `revision` - The revision of the request that the kernel provides. This is
bumped whenever new members or functionality are added to the request structure.
Bootloaders process requests in a backwards compatible manner, *always*. This
means that if the bootloader does not support the revision of the request,
it will process the request as if were the highest revision that the bootloader
supports.
* `response` - This field is filled in by the bootloader at load time, with a
pointer to the response structure, if the request was successfully processed.
If the request is unsupported or was not successfully processed, this field
is *left untouched*, meaning that if it was set to `NULL`, it will stay that
way.

A response has only 1 mandatory member at the beginning of the structure:
```c
struct limine_example_response {
    uint64_t revision;
    ... optional members follow ...
};
```
* `revision` - Like for requests, bootloaders will instead mark responses with a
revision number. This revision is not coupled between requests and responses,
as they are bumped individually when new members are added or functionality is
changed. Bootloaders will set the revision to the one they provide, and this is
*always backwards compatible*, meaning higher revisions support all that lower
revisions do.

This is all there is to features. For a list of official Limine features, read
the "Feature List" section below.

## Entry memory layout

The protocol mandates kernels to load themselves at or above
`0xffffffff80000000`. Lower half kernels are *not supported*.

At handoff, the kernel will be properly loaded and mapped with appropriate
MMU permissions at the requested virtual memory address (provided it is at
or above `0xffffffff80000000`).

No specific physical memory placement is guaranteed. In order to determine
where the kernel is loaded in physical memory, see the Kernel Address feature
below.

Alongside the loaded kernel, the bootloader will set up memory mappings as such:
```
 Base Physical Address -                    Size                    ->  Virtual address
  0x0000000000001000   - 4 GiB plus any additional memory map entry -> 0x0000000000001000
  0x0000000000000000   - 4 GiB plus any additional memory map entry -> HHDM start
```
Where HHDM start is returned by the Higher Half Direct Map feature (see below).
These mappings are supervisor, read, write, execute (-rwx).

The bootloader page tables are in bootloader-reclaimable memory (see Memory Map
feature below), and their specific layout is undefined as long as they provide
the above memory mappings.

If the kernel is a position independent executable, the bootloader is free to
relocate it as it sees fit, potentially performing KASLR (as specified by the
config).

## Entry machine state

### x86_64

`rip` will be the entry point as defined as part of the executable file format,
unless the an Entry Point feature is requested (see below), in which case,
the value of `rip` is going to be taken from there.

At entry all segment registers are loaded as 64 bit code/data segments, limits
and bases are ignored since this is 64-bit mode.

The GDT register is loaded to point to a GDT, in bootloader-reserved memory,
with at least the following entries, starting at offset 0:

  - Null descriptor
  - 16-bit code descriptor. Base = `0`, limit = `0xffff`. Readable.
  - 16-bit data descriptor. Base = `0`, limit = `0xffff`. Writable.
  - 32-bit code descriptor. Base = `0`, limit = `0xffffffff`. Readable.
  - 32-bit data descriptor. Base = `0`, limit = `0xffffffff`. Writable.
  - 64-bit code descriptor. Base and limit irrelevant. Readable.
  - 64-bit data descriptor. Base and limit irrelevant. Writable.

The IDT is in an undefined state. Kernel must load its own.

IF flag, VM flag, and direction flag are cleared on entry. Other flags
undefined.

PG is enabled (`cr0`), PE is enabled (`cr0`), PAE is enabled (`cr4`),
WP is enabled (`cr0`), LME is enabled (`EFER`), NX is enabled (`EFER`).
If 5-level paging is requested and available, then 5-level paging is enabled
(LA57 bit in `cr4`).

The A20 gate is opened.

Legacy PIC and IO APIC IRQs are all masked.

If booted by EFI/UEFI, boot services are exited.

`rsp` is set to point to a stack, in bootloader-reserved memory, which is
at least 16KiB (16384 bytes) in size, or the size specified in the Stack
Size Request (see below). An invalid return address of 0 is pushed
to the stack before jumping to the kernel.

All other general purpose registers are set to 0.

## Feature List

Request IDs are composed of 4 64-bit unsigned integers, but the first 2 are
common to every request:
```c
#define LIMINE_COMMON_MAGIC 0xc7b1dd30df4c8b88, 0x0a82e883a194f07b
```

### Bootloader Info Feature

ID:
```c
#define LIMINE_BOOTLOADER_INFO_REQUEST { LIMINE_COMMON_MAGIC, 0xf55038d8e2a1202f, 0x279426fcf5f59740 }
```

Request:
```c
struct limine_bootloader_info_request {
    uint64_t id[4];
    uint64_t revision;
    struct limine_bootloader_info_response *response;
};
```

Response:
```c
struct limine_bootloader_info_response {
    uint64_t revision;
    char *name;
    char *version;
};
```

`name` and `version` are 0-terminated ASCII strings containing the name and
version of the loading bootloader.

### Stack Size Feature

ID:
```c
#define LIMINE_STACK_SIZE_REQUEST { LIMINE_COMMON_MAGIC, 0x224ef0460a8e8926, 0xe1cb0fc25f46ea3d }
```

Request:
```c
struct limine_stack_size_request {
    uint64_t id[4];
    uint64_t revision;
    struct limine_stack_size_response *response;
    uint64_t stack_size;
};
```

* `stack_size` - The requested stack size (also used for SMP processors).

Response:
```c
struct limine_stack_size_response {
    uint64_t revision;
};
```

### Executable Layout Feature

ID:
```c
#define LIMINE_EXECUTABLE_LAYOUT_REQUEST { LIMINE_COMMON_MAGIC, 0xbbd4597377e1fdbb, 0x17540007cfa435ad }
```

Request:
```c
typedef void (*limine_entry_point)(void);

struct limine_executable_layout_request {
    uint64_t id[4];
    uint64_t revision;
    struct limine_executable_layout_response *response;
    limine_entry_point entry_point;
    uint64_t alignment;
    uint64_t text_offset;
    uint64_t text_address;
    uint64_t text_size;
    uint64_t data_offset;
    uint64_t data_address;
    uint64_t data_size;
    uint64_t rodata_offset;
    uint64_t rodata_address;
    uint64_t rodata_size;
    uint64_t bss_address;
    uint64_t bss_size;
};
```

* `entry_point` - The virtual address of the entry point of the kernel. It is
equivalent to an entry point specified in an executable format, and thus it can
be overridden by the Entry Point Feature.
* `alignment` - The requested alignment for the physical base address of the
kernel. It *must* be a power of 2. An alignment of 0 means 4096.
* `{text,data,rodata}_offset` - The offset within the file where the segment
begins.
* `{text,data,rodata,bss}_address` - The virtual address to which to load the
segment to.
* `{text,data,rodata,bss}_size` - The size of the segment both in the file and
in memory, except for bss, where it is only the in-memory size.

Response:
```c
struct limine_executable_layout_response {
    uint64_t revision;
};
```

Notes: This request is parsed if the bootloader does not support the executable
format of the kernel. It is otherwise ignored. If it is parsed and used for
loading the kernel, then the response will be set to a vaild pointer.

### HHDM (Higher Half Direct Map) Feature

ID:
```c
#define LIMINE_HHDM_REQUEST { LIMINE_COMMON_MAGIC, 0x48dcf1cb8ad2b852, 0x63984e959a98244b }
```

Request:
```c
struct limine_hhdm_request {
    uint64_t id[4];
    uint64_t revision;
    struct limine_hhdm_response *response;
};
```

Response:
```c
struct limine_hhdm_response {
    uint64_t revision;
    uint64_t offset;
};
```

* `offset` - the virtual address offset of the beginning of the higher half
direct map.

### Terminal Feature

ID:
```c
#define LIMINE_TERMINAL_REQUEST { LIMINE_COMMON_MAGIC, 0x0785a0aea5d0750f, 0x1c1936fee0d6cf6e }
```

Request:
```c
typedef void (*limine_terminal_callback)(uint64_t, uint64_t, uint64_t, uint64_t);

struct limine_terminal_request {
    uint64_t id[4];
    uint64_t revision;
    struct limine_terminal_response *response;
    limine_terminal_callback callback;
};
```

* `callback` - Pointer to the callback function.

Response:
```c
typedef void (*limine_terminal_write)(const char *, uint64_t);

struct limine_terminal_response {
    uint64_t revision;
    uint32_t columns;
    uint32_t rows;
    limine_terminal_write write;
};
```

* `columns` and `rows` - Columns and rows provided by the terminal.
* `write` - Physical pointer to the terminal write() function.

Note: Omitting this request will cause the bootloader to not initialise
the terminal service. The terminal is further documented in the stivale2
specification.

### Framebuffer Feature

ID:
```c
#define LIMINE_FRAMEBUFFER_REQUEST { LIMINE_COMMON_MAGIC, 0xcbfe81d7dd2d1977, 0x063150319ebc9b71 }
```

Request:
```c
struct limine_framebuffer_request {
    uint64_t id[4];
    uint64_t revision;
    struct limine_framebuffer_response *response;
};
```

Response:
```c
struct limine_framebuffer_response {
    uint64_t revision;
    uint64_t framebuffer_count;
    struct limine_framebuffer **framebuffers;
};
```

* `framebuffer_count` - How many framebuffers are present.
* `framebuffers` - Pointer to an array of `framebuffer_count` pointers to
`struct limine_framebuffer` structures.

```c
// Constants for `memory_model`
#define LIMINE_FRAMEBUFFER_RGB 1

struct limine_framebuffer {
    void *address;
    uint16_t width;
    uint16_t height;
    uint16_t pitch;
    uint16_t bpp;
    uint8_t memory_model;
    uint8_t red_mask_size;
    uint8_t red_mask_shift;
    uint8_t green_mask_size;
    uint8_t green_mask_shift;
    uint8_t blue_mask_size;
    uint8_t blue_mask_shift;
    uint8_t unused;
    uint64_t edid_size;
    void *edid;
};
```

### 5-Level Paging Feature

ID:
```c
#define LIMINE_5_LEVEL_PAGING_REQUEST { LIMINE_COMMON_MAGIC, 0x94469551da9b3192, 0xebe5e86db7382888 }
```

Request:
```c
struct limine_5_level_paging_request {
    uint64_t id[4];
    uint64_t revision;
    struct limine_5_level_paging_response *response;
};
```

Response:
```c
struct limine_5_level_paging_response {
    uint64_t revision;
};
```

Notes: The presence of this request will prompt the bootloader to turn on
x86_64 5-level paging. It will not be turned on if this request is not present.
If the response pointer is unchanged, 5-level paging is engaged.

### SMP (multiprocessor) Feature

ID:
```c
#define LIMINE_SMP_REQUEST { LIMINE_COMMON_MAGIC, 0x95a67b819a1b857e, 0xa0b61b723b6a73e0 }
```

Request:
```c
struct limine_smp_request {
    uint64_t id[4];
    uint64_t revision;
    struct limine_smp_response *response;
    uint64_t flags;
};
```

* `flags` - Bit 0: Enable X2APIC, if possible.

Response:
```c
struct limine_smp_response {
    uint64_t revision;
    uint32_t flags;
    uint32_t bsp_lapic_id;
    uint64_t cpu_count;
    struct limine_smp_info **cpus;
};
```

* `flags` - Bit 0: X2APIC has been enabled.
* `bsp_lapic_id` - The Local APIC ID of the bootstrap processor.
* `cpu_count` - How many CPUs are present. It includes the bootstrap processor.
* `cpus` - Pointer to an array of `cpu_count` pointers to
`struct limine_smp_info` structures.

Notes: The presence of this request will prompt the bootloader to bootstrap
the secondary processors. This will not be done if this request is not present.

```c
struct limine_smp_info;

typedef void (*limine_goto_address)(struct limine_smp_info *);

struct limine_smp_info {
    uint32_t processor_id;
    uint32_t lapic_id;
    uint64_t reserved;
    limine_goto_address goto_address;
    uint64_t extra_argument;
};
```

* `processor_id` - ACPI Processor UID as specified by the MADT
* `lapic_id` - Local APIC ID of the processor as specified by the MADT
* `goto_address` - An atomic write to this field causes the parked CPU to
jump to the written address, on a 16KiB (or Stack Size Request size) stack. A pointer to the
`struct limine_smp_info` structure of the CPU is passed in `RDI`. Other than
that, the CPU state will be the same as described for the bootstrap
processor. This field is unused for the structure describing the bootstrap
processor.
* `extra_argument` - A free for use field.

### Memory Map Feature

ID:
```c
#define LIMINE_MEMMAP_REQUEST { LIMINE_COMMON_MAGIC, 0x67cf3d9d378a806f, 0xe304acdfc50c3c62 }
```

Request:
```c
struct limine_memmap_request {
    uint64_t id[4];
    uint64_t revision;
    struct limine_memmap_response *response;
};
```

Response:
```c
struct limine_memmap_response {
    uint64_t revision;
    uint64_t entry_count;
    struct limine_memmap_entry **entries;
};
```

* `entry_count` - How many memory map entries are present.
* `entries` - Pointer to an array of `entry_count` pointers to
`struct limine_memmap_entry` structures.

```c
// Constants for `type`
#define LIMINE_MEMMAP_USABLE                 0
#define LIMINE_MEMMAP_RESERVED               1
#define LIMINE_MEMMAP_ACPI_RECLAIMABLE       2
#define LIMINE_MEMMAP_ACPI_NVS               3
#define LIMINE_MEMMAP_BAD_MEMORY             4
#define LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE 5
#define LIMINE_MEMMAP_KERNEL_AND_MODULES     6
#define LIMINE_MEMMAP_FRAMEBUFFER            7

struct limine_memmap_entry {
    uint64_t base;
    uint64_t length;
    uint64_t type;
};
```

Note: The kernel and modules loaded are not marked as usable memory.
They are marked as Kernel/Modules. The entries are guaranteed to be sorted by
base address, lowest to highest. Usable and bootloader reclaimable entries
are guaranteed to be 4096 byte aligned for both base and length. Usable and
bootloader reclaimable entries are guaranteed not to overlap with any other
entry. To the contrary, all non-usable entries (including kernel/modules) are
not guaranteed any alignment, nor is it guaranteed that they do not overlap
other entries.

### Entry Point Feature

ID:
```c
#define LIMINE_ENTRY_POINT_REQUEST { LIMINE_COMMON_MAGIC, 0x13d86c035a1cd3e1, 0x2b0caa89d8f3026a }
```

Request:
```c
typedef void (*limine_entry_point)(void);

struct limine_entry_point_request {
    uint64_t id[4];
    uint64_t revision;
    struct limine_entry_point_response *response;
    limine_entry_point entry;
};
```

* `entry` - The requested entry point.

Response:
```c
struct limine_entry_point_response {
    uint64_t revision;
};
```

### Kernel File Feature

ID:
```c
#define LIMINE_KERNEL_FILE_REQUEST { LIMINE_COMMON_MAGIC, 0xad97e90e83f1ed67, 0x31eb5d1c5ff23b69 }
```

Request:
```c
struct limine_kernel_file_request {
    uint64_t id[4];
    uint64_t revision;
    struct limine_kernel_file_response *response;
};
```

Response:
```c
struct limine_kernel_file_response {
    uint64_t revision;
    struct limine_file *kernel_file;
};
```

* `kernel_file` - Pointer to the `struct limine_file` structure (see below)
for the kernel file.

### Module Feature

ID:
```c
#define LIMINE_MODULE_REQUEST { LIMINE_COMMON_MAGIC, 0x3e7e279702be32af, 0xca1c4f3bd1280cee }
```

Request:
```c
struct limine_module_request {
    uint64_t id[4];
    uint64_t revision;
    struct limine_module_response *response;
};
```

Response:
```c
struct limine_module_response {
    uint64_t revision;
    uint64_t module_count;
    struct limine_file **modules;
};
```

* `module_count` - How many modules are present.
* `modules` - Pointer to an array of `module_count` pointers to
`struct limine_file` structures (see below).

### File Structure

```c
struct limine_uuid {
    uint32_t a;
    uint16_t b;
    uint16_t c;
    uint8_t d[8];
};

struct limine_file {
    uint64_t revision;
    void *address;
    uint64_t size;
    char *path;
    char *cmdline;
    uint64_t partition_index;
    uint32_t unused;
    uint32_t tftp_ip;
    uint32_t tftp_port;
    uint32_t mbr_disk_id;
    struct limine_uuid gpt_disk_uuid;
    struct limine_uuid gpt_part_uuid;
    struct limine_uuid part_uuid;
};
```

* `revision` - Revision of the `struct limine_file` structure.
* `address` - The address of the file.
* `size` - The size of the file.
* `path` - The path of the file within the volume, with a leading slash.
* `cmdline` - A command line associated with the file.
* `partition_index` - 1-based partition index of the volume from which the
file was loaded. If 0, it means invalid or unpartitioned.
* `tftp_ip` - If non-0, this is the IP of the TFTP server the file was loaded
from.
* `tftp_port` - Likewise, but port.
* `mbr_disk_id` - If non-0, this is the ID of the disk the file was loaded
from as reported in its MBR.
* `gpt_disk_uuid` - If non-0, this is the UUID of the disk the file was
loaded from as reported in its GPT.
* `gpt_part_uuid` - If non-0, this is the UUID of the partition the file
was loaded from as reported in the GPT.
* `part_uuid` - If non-0, this is the UUID of the filesystem of the partition
the file was loaded from.

### RSDP Feature

ID:
```c
#define LIMINE_RSDP_REQUEST { LIMINE_COMMON_MAGIC, 0xc5e77b6b397e7b43, 0x27637845accdcf3c }
```

Request:
```c
struct limine_rsdp_request {
    uint64_t id[4];
    uint64_t revision;
    struct limine_rsdp_response *response;
};
```

Response:
```c
struct limine_rsdp_response {
    uint64_t revision;
    void *address;
};
```

* `address` - Address of the RSDP table.

### SMBIOS Feature

ID:
```c
#define LIMINE_SMBIOS_REQUEST { LIMINE_COMMON_MAGIC, 0x9e9046f11e095391, 0xaa4a520fefbde5ee }
```

Request:
```c
struct limine_smbios_request {
    uint64_t id[4];
    uint64_t revision;
    struct limine_smbios_response *response;
};
```

Response:
```c
struct limine_smbios_response {
    uint64_t revision;
    void *entry_32;
    void *entry_64;
};
```

* `entry_32` - Address of the 32-bit SMBIOS entry point. NULL if not present.
* `entry_64` - Address of the 64-bit SMBIOS entry point. NULL if not present.

### EFI System Table Feature

ID:
```c
#define LIMINE_EFI_SYSTEM_TABLE_REQUEST { LIMINE_COMMON_MAGIC, 0x5ceba5163eaaf6d6, 0x0a6981610cf65fcc }
```

Request:
```c
struct limine_efi_system_table_request {
    uint64_t id[4];
    uint64_t revision;
    struct limine_efi_system_table_response *response;
};
```

Response:
```c
struct limine_efi_system_table_response {
    uint64_t revision;
    void *address;
};
```

* `address` - Address of EFI system table.

### Boot Time Feature

ID:
```c
#define LIMINE_BOOT_TIME_REQUEST { LIMINE_COMMON_MAGIC, 0x502746e184c088aa, 0xfbc5ec83e6327893 }
```

Request:
```c
struct limine_boot_time_request {
    uint64_t id[4];
    uint64_t revision;
    struct limine_boot_time_response *response;
};
```

Response:
```c
struct limine_boot_time_response {
    uint64_t revision;
    int64_t boot_time;
};
```

* `boot_time` - The UNIX time on boot, in seconds, taken from the system RTC.

### Kernel Address Feature

ID:
```c
#define LIMINE_KERNEL_ADDRESS_REQUEST { LIMINE_COMMON_MAGIC, 0x71ba76863cc55f63, 0xb2644a48c516a487 }
```

Request:
```c
struct limine_kernel_address_request {
    uint64_t id[4];
    uint64_t revision;
    struct limine_kernel_address_response *response;
};
```

Response:
```c
struct limine_kernel_address_response {
    uint64_t revision;
    uint64_t physical_base;
    uint64_t virtual_base;
};
```

* `physical_base` - The physical base address of the kernel.
* `virtual_base` - The virtual base address of the kernel.
