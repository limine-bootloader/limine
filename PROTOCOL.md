# The Limine Boot Protocol

The Limine boot protocol is a modern, portable, featureful, and extensible boot
protocol.

This file serves as the official centralised collection of features that
the Limine boot protocol is comprised of. Other bootloaders may support extra
unofficial features, but it is strongly recommended to avoid fragmentation
and submit new features by opening a pull request to the Limine repository.

The [limine.h](/limine.h) file provides an implementation of all the
structures and constants described in this document, for the C and C++
languages.

## General Notes

All pointers are 64-bit wide. All pointers point to the object with the
higher half direct map offset already added to them, unless otherwise noted.

The calling convention matches the C ABI for the specific architecture
(SysV for x86, AAPCS for aarch64).

## Base protocol revisions

The Limine boot protocol comes in several base revisions; so far only 2
base revisions are specified: 0 and 1.

Base protocol revisions change certain behaviours of the Limine boot protocol
outside any specific feature. The specifics are going to be described as
needed throughout this specification.

Base revision 0 is considered deprecated, and it is the default base revision
a kernel is assumed to be requesting and complying to if no base revision tag
is provided by the kernel, for backwards compatibility.

A base revision tag is a set of 3 64-bit values placed somewhere in the kernel
binary on an 8-byte aligned boundary; the first 2 values are a magic number
for the bootloader to be able to identify the tag, and the last value is the
requested base revision number. Lack of base revision tag implies revision 0.

```c
#define LIMINE_BASE_REVISION(N) \
    uint64_t limine_base_revision[3] = { 0xf9562b2d5c95a6c8, 0x6a7b384944536bdc, (N) };
```

If a bootloader drops support for an older base revision, the bootloader must
fail to boot a kernel requesting such base revision. If a bootloader does not yet
support a requested base revision (i.e. if the requested base revision is higher
than the maximum base revision supported), it must boot the kernel using any
arbitrary revision it supports, and communicate failure to comply to the kernel by
*leaving the 3rd component of the base revision tag unchanged*.
On the other hand, if the kernel's requested base revision is supported,
*the 3rd component of the base revision tag must be set to 0 by the bootloader*.

Note: this means that unlike when the bootloader drops support for an older base
revision and *it* is responsible for failing to boot the kernel, in case the
bootloader does not yet support the kernel's requested base revision,
it is up to the kernel itself to fail (or handle the condition otherwise).

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
to boot an executable with multiple of the same request IDs. Alternatively, it is possible to provide a list of requests explicitly via an executable file section. See "Limine Requests Section".
* `revision` - The revision of the request that the kernel provides. This starts at 0 and is
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

## Limine Requests Section

Note: *This behaviour is deprecated as of base protocol revision 1*

For kernels requesting deprecated base revision 0,
if the executable kernel file contains a `.limine_reqs` section, the bootloader
will, instead of scanning the executable for requests, fetch the requests
from a NULL-terminated array of pointers to the provided requests, contained
inside said section.

## Entry memory layout

The protocol mandates kernels to load themselves at or above
`0xffffffff80000000`. Lower half kernels are *not supported*.

At handoff, the kernel will be properly loaded and mapped with appropriate
MMU permissions, as supervisor, at the requested virtual memory address (provided it is at
or above `0xffffffff80000000`).

No specific physical memory placement is guaranteed, except that the kernel
is guaranteed to be physically contiguous. In order to determine
where the kernel is loaded in physical memory, see the Kernel Address feature
below.

Alongside the loaded kernel, the bootloader will set up memory mappings as such:

```
 Base Physical Address |                               | Base Virtual Address
 ----------------------+-------------------------------+-----------------------
                       |    (4 GiB - 0x1000) and any   |
  0x0000000000001000   |  additional memory map region |  0x0000000000001000
                       |    (Base revision 0 only)     |
 ----------------------+-------------------------------+-----------------------
                       |     4 GiB and additional      |
  0x0000000000000000   |  memory map regions depending |      HHDM start
                       |       on base revision        |
```
Where "HHDM start" is returned by the Higher Half Direct Map feature (see below).
These mappings are supervisor, read, write, execute (-rwx).

For base revision 0, the above-4GiB identity and HHDM mappings cover any memory
map region.

For base revision 1, the above-4GiB HHDM mappings do not comprise memory map regions
of types:
 - Reserved
 - Bad memory

The bootloader page tables are in bootloader-reclaimable memory (see Memory Map
feature below), and their specific layout is undefined as long as they provide
the above memory mappings.

If the kernel is a position independent executable, the bootloader is free to
relocate it as it sees fit, potentially performing KASLR (as specified by the
config).

## Caching

### x86-64

The kernel executable, loaded at or above `0xffffffff80000000`, sees all of its
segments mapped using write-back (WB) caching at the page tables level.

All HHDM and identity map memory regions are mapped using write-back (WB) caching at the page
tables level, except framebuffer regions which are mapped using write-combining
(WC) caching at the page tables level.

The PAT's (Page Attribute Table) layout is specified to be as follows:
```
PAT0 -> WB
PAT1 -> WT
PAT2 -> UC-
PAT3 -> UC
PAT4 -> WP
PAT5 -> WC
PAT6 -> unspecified
PAT7 -> unspecified
```

The MTRRs are left as the firmware set them up.

### aarch64

The kernel executable, loaded at or above `0xffffffff80000000`, sees all of its
segments mapped using Normal Write-Back RW-Allocate non-transient caching mode.

All HHDM and identity map memory regions are mapped using the Normal Write-Back RW-Allocate
non-transient caching mode, except for the framebuffer regions, which are
mapped in using an unspecified caching mode, correct for use with the
framebuffer on the platform.

The `MAIR_EL1` register will at least contain entries for the above-mentioned
caching modes, in an unspecified order.

In order to access MMIO regions, the kernel must ensure the correct caching mode
is used on its own.

### riscv64

If the `Svpbmt` extension is available, all framebuffer memory regions are mapped
with `PBMT=NC` to enable write-combining optimizations. The kernel executable,
loaded at or above `0xffffffff80000000`, and all HHDM and identity map memory regions are mapped
with the default `PBMT=PMA`.

If the `Svpbmt` extension is not available, no PMAs can be overridden (effectively,
everything is mapped with `PBMT=PMA`).

## Machine state at entry

### x86-64

`rip` will be the entry point as defined as part of the executable file format,
unless the Entry Point feature is requested (see below), in which case, the value
of `rip` is going to be taken from there.

At entry all segment registers are loaded as 64 bit code/data segments, limits
and bases are ignored since this is 64-bit mode.

The GDT register is loaded to point to a GDT, in bootloader-reclaimable memory,
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
WP is enabled (`cr0`), LME is enabled (`EFER`), NX is enabled (`EFER`) if available.
If 5-level paging is requested and available, then 5-level paging is enabled
(LA57 bit in `cr4`).

The A20 gate is opened.

Legacy PIC (if available) and IO APIC IRQs (only those with delivery mode fixed
(0b000) or lowest priority (0b001)) are all masked.

If booted by EFI/UEFI, boot services are exited.

`rsp` is set to point to a stack, in bootloader-reclaimable memory, which is
at least 64KiB (65536 bytes) in size, or the size specified in the Stack
Size Request (see below). An invalid return address of 0 is pushed
to the stack before jumping to the kernel.

All other general purpose registers are set to 0.

### aarch64

`PC` will be the entry point as defined as part of the executable file format,
unless the Entry Point feature is requested (see below), in which case,
the value of `PC` is going to be taken from there.

The contents of the `VBAR_EL1` register are undefined, and the kernel must load
its own.

The `MAIR_EL1` register contents are described above, in the caching section.

All interrupts are masked (`PSTATE.{D, A, I, F}` are set to 1).

The kernel is entered in little-endian AArch64 EL1t (EL1 with `PSTATE.SP` set to
0, `PSTATE.E` set to 0, and `PSTATE.nRW` set to 0).

Other fields of `PSTATE` are undefined.

At entry: the MMU (`SCTLR_EL1.M`) is enabled, the I-Cache and D-Cache
(`SCTLR_EL1.{I, C}`) are enabled, data alignment checking (`SCTLR_EL1.A`) is
disabled. SP alignment checking (`SCTLR_EL1.{SA, SA0}`) is enabled. Other fields
of `SCTLR_EL1` are reset to 0 or to their reserved value.

Higher ELs do not interfere with accesses to vector or floating point
instructions or registers.

Higher ELs do not interfere with accesses to the generic timer and counter.

The used translation granule size is undefined.

If booted by EFI/UEFI, boot services are exited.

`SP` is set to point to a stack, in bootloader-reclaimable memory, which is
at least 64KiB (65536 bytes) in size, or the size specified in the Stack
Size Request (see below).

All other general purpose registers (including `X29` and `X30`) are set to 0.
Vector registers are in an undefined state.

### riscv64

At entry the machine is executing in Supervisor mode.

`pc` will be the entry point as defined as part of the executable file format,
unless the Entry Point feature is requested (see below), in which case, the
value of `pc` is going to be taken from there.

`x1`(`ra`) is set to 0, the kernel must not return from the entry point.

`x2`(`sp`) is set to point to a stack, in bootloader-reclaimable memory, which is
at least 64KiB (65536 bytes) in size, or the size specified in the Stack
Size Request (see below).

`x3`(`gp`) is set to 0, kernel must load its own global pointer if needed.

All other general purpose registers, with the exception of `x5`(`t0`), are set to 0.

If booted by EFI/UEFI, boot services are exited.

`stvec` is in an undefined state. `sstatus.SIE` and `sie` are set to 0.

`sstatus.FS` and `sstatus.XS` are both set to `Off`.

Paging is enabled with the paging mode specified by the Paging Mode feature (see below).

The (A)PLIC, if present, is in an undefined state.

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

* `stack_size` - The requested stack size in bytes (also used for SMP processors).

Response:
```c
struct limine_stack_size_response {
    uint64_t revision;
};
```

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

Note: *This feature is deprecated and has been removed as of Limine 5.x, please do not use.*

ID:
```c
#define LIMINE_TERMINAL_REQUEST { LIMINE_COMMON_MAGIC, 0xc8ac59310c2b0844, 0xa68d0c7265d38878 }
```

Request:
```c
typedef void (*limine_terminal_callback)(struct limine_terminal *, uint64_t, uint64_t, uint64_t, uint64_t);

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
typedef void (*limine_terminal_write)(struct limine_terminal *terminal, const char *string, uint64_t length);

struct limine_terminal_response {
    uint64_t revision;
    uint64_t terminal_count;
    struct limine_terminal **terminals;
    limine_terminal_write write;
};
```

* `terminal_count` - How many terminals are present.
* `terminals` - Pointer to an array of `terminal_count` pointers to
`struct limine_terminal` structures.
* `write` - Physical pointer to the terminal write() function.
The function is not thread-safe, nor reentrant, per-terminal.
This means multiple terminals may be called simultaneously, and multiple
callbacks may be handled simultaneously.
The `terminal` parameter points to the `struct limine_terminal` structure to
use to output the string; the `string` parameter points to a
string to print; the `length` parameter contains the length, in bytes, of the
string to print.

```c
struct limine_terminal {
    uint64_t columns;
    uint64_t rows;
    struct limine_framebuffer *framebuffer;
};
```

* `columns` and `rows` - Columns and rows provided by the terminal.
* `framebuffer` - The framebuffer associated with this terminal.

Note: Omitting this request will cause the bootloader to not initialise
the terminal service.

#### Terminal callback

The callback is a function that is part of the kernel, which is called by the
terminal during a `write()` call whenever an event or escape sequence cannot
be handled by the bootloader's terminal alone, and the kernel may want to be
notified in order to handle it itself.

Returning from the callback will resume the `write()` call which will return
to its caller normally.

Not returning from a callback may leave the terminal in an undefined state
and cause issues.

The callback function has the following prototype:
```c
void callback(struct limine_terminal *terminal, uint64_t type, uint64_t, uint64_t, uint64_t);
```

The `terminal` argument is a pointer to the Limine terminal structure which
represents the terminal that caused the callback.

The purpose of the last 3 arguments changes depending on the `type` argument.

The callback types are as follows:

* `LIMINE_TERMINAL_CB_DEC` - (type value: `10`)

This callback is triggered whenever a DEC Private Mode (DECSET/DECRST)
sequence is encountered that the terminal cannot handle alone. The arguments
to this callback are: `terminal`, `type`, `values_count`, `values`, `final`.

`values_count` is a count of how many values are in the array pointed to by
`values`. `values` is a pointer to an array of `uint32_t` values, which are
the values passed to the DEC private escape.
`final` is the final character in the DEC private escape sequence (typically
`l` or `h`).

* `LIMINE_TERMINAL_CB_BELL` - (type value: `20`)

This callback is triggered whenever a bell event is determined to be
necessary (such as when a bell character `\a` is encountered). The arguments
to this callback are: `terminal`, `type`, `unused1`, `unused2`, `unused3`.

* `LIMINE_TERMINAL_CB_PRIVATE_ID` - (type value: `30`)

This callback is triggered whenever the kernel has to respond to a DEC
private identification request. The arguments to this callback are:
`terminal`, `type`, `unused1`, `unused2`, `unused3`.

* `LIMINE_TERMINAL_CB_STATUS_REPORT` - (type value `40`)

This callback is triggered whenever the kernel has to respond to a ECMA-48
status report request. The arguments to this callback are: `terminal`,
`type`, `unused1`, `unused2`, `unused3`.

* `LIMINE_TERMINAL_CB_POS_REPORT` - (type value `50`)

This callback is triggered whenever the kernel has to respond to a ECMA-48
cursor position report request. The arguments to this callback are:
`terminal`, `type`, `x`, `y`, `unused3`. Where `x` and `y` represent the
cursor position at the time the callback is triggered.

* `LIMINE_TERMINAL_CB_KBD_LEDS` - (type value `60`)

This callback is triggered whenever the kernel has to respond to a keyboard
LED state change request. The arguments to this callback are: `terminal`,
`type`, `led_state`, `unused2`, `unused3`. `led_state` can have one of the
following values: `0, 1, 2, or 3`. These values mean: clear all LEDs, set
scroll lock, set num lock, and set caps lock LED, respectively.

* `LIMINE_TERMINAL_CB_MODE` - (type value: `70`)

This callback is triggered whenever an ECMA-48 Mode Switch sequence
is encountered that the terminal cannot handle alone. The arguments to this
callback are: `terminal`, `type`, `values_count`, `values`, `final`.

`values_count` is a count of how many values are in the array pointed to by
`values`. `values` is a pointer to an array of `uint32_t` values, which are
the values passed to the mode switch escape.
`final` is the final character in the mode switch escape sequence (typically
`l` or `h`).

* `LIMINE_TERMINAL_CB_LINUX` - (type value `80`)

This callback is triggered whenever a private Linux escape sequence
is encountered that the terminal cannot handle alone. The arguments to this
callback are: `terminal`, `type`, `values_count`, `values`, `unused3`.

`values_count` is a count of how many values are in the array pointed to by
`values`. `values` is a pointer to an array of `uint32_t` values, which are
the values passed to the Linux private escape.

#### Terminal context control

The `write()` function can additionally be used to set and restore terminal
context, and refresh the terminal fully.

In order to achieve this, special values for the `length` argument are
passed. These values are:
```c
/* Response revision 0 */
#define LIMINE_TERMINAL_CTX_SIZE ((uint64_t)(-1))
#define LIMINE_TERMINAL_CTX_SAVE ((uint64_t)(-2))
#define LIMINE_TERMINAL_CTX_RESTORE ((uint64_t)(-3))
#define LIMINE_TERMINAL_FULL_REFRESH ((uint64_t)(-4))
/* Response revision 1 */
#define LIMINE_TERMINAL_OOB_OUTPUT_GET ((uint64_t)(-10))
#define LIMINE_TERMINAL_OOB_OUTPUT_SET ((uint64_t)(-11))
```

For `CTX_SIZE`, the `ptr` variable has to point to a location to which the
terminal will *write* a single `uint64_t` which contains the size of the
terminal context.

For `CTX_SAVE` and `CTX_RESTORE`, the `ptr` variable has to point to a
location to which the terminal will *save* or *restore* its context from,
respectively.
This location must have a size congruent to the value received from
`CTX_SIZE`.

For `FULL_REFRESH`, the `ptr` variable is unused. This routine is to be used
after control of the framebuffer is taken over and the bootloader's terminal
has to *fully* repaint the framebuffer to avoid inconsistencies.

If the response revision is equal or greater than 1
`OOB_OUTPUT_GET` and `OOB_OUTPUT_SET` allow getting and setting the
out-of-band terminal output settings. `ptr` points to a location to where
the terminal will *write* or *read* a single `uint64_t` value containing the
bits representing the settings.
The possible settings are as follows:
```c
#define LIMINE_TERMINAL_OOB_OUTPUT_OCRNL (1 << 0)
#define LIMINE_TERMINAL_OOB_OUTPUT_OFDEL (1 << 1)
#define LIMINE_TERMINAL_OOB_OUTPUT_OFILL (1 << 2)
#define LIMINE_TERMINAL_OOB_OUTPUT_OLCUC (1 << 3)
#define LIMINE_TERMINAL_OOB_OUTPUT_ONLCR (1 << 4)
#define LIMINE_TERMINAL_OOB_OUTPUT_ONLRET (1 << 5)
#define LIMINE_TERMINAL_OOB_OUTPUT_ONOCR (1 << 6)
#define LIMINE_TERMINAL_OOB_OUTPUT_OPOST (1 << 7)
```
The effect of each of these options matches the effect of the `stty(1)`
options by the same name.

#### x86-64

Additionally, the kernel must ensure, when calling `write()`, that:

* Either the GDT provided by the bootloader is still properly loaded, or a
custom GDT is loaded with at least the following descriptors in this specific
order:

  - Null descriptor
  - 16-bit code descriptor. Base = `0`, limit = `0xffff`. Readable.
  - 16-bit data descriptor. Base = `0`, limit = `0xffff`. Writable.
  - 32-bit code descriptor. Base = `0`, limit = `0xffffffff`. Readable.
  - 32-bit data descriptor. Base = `0`, limit = `0xffffffff`. Writable.
  - 64-bit code descriptor. Base and limit irrelevant. Readable.
  - 64-bit data descriptor. Base and limit irrelevant. Writable.

* The currently loaded virtual address space is still the one provided at
entry by the bootloader, or a custom virtual address space is loaded which
identity maps the framebuffer memory region associated with the terminal, and
all the bootloader reclaimable memory regions, with read, write, and execute
permissions.

* The routine is called *by its physical address* (the value of the function
pointer is already physical), which should be identity mapped.

* Bootloader-reclaimable memory entries are left untouched until after the
kernel is done utilising bootloader-provided facilities (this terminal being
one of them).

Notes regarding segment registers and FPU:

The values of the FS and GS segments are guaranteed preserved across the
call. All other segment registers may have their "hidden" portion
overwritten, but Limine guarantees that the "visible" portion is going to
be restored to the one used at the time of call before returning.

No registers other than the segment registers and general purpose registers
are going to be used. Especially, this means that there is no need to save
and restore FPU, SSE, or AVX state when calling the terminal write function.

#### aarch64

Additionally, the kernel must ensure, when calling `write()`, that:

* The currently loaded virtual address space is still the one provided at
entry by the bootloader, or a custom virtual address space is loaded which
identity maps the framebuffer memory region associated with the terminal, and
all the bootloader reclaimable memory regions, with read, write, and execute
permissions.

* The routine is called *by its physical address* (the value of the function
pointer is already physical), which should be identity mapped.

* Bootloader-reclaimable memory entries are left untouched until after the
kernel is done utilising bootloader-provided facilities (this terminal being
one of them).

Notes regarding the usage of registers:

No registers other than the general purpose registers are going to be used.
Especially, this means that there is no need to save and restore SIMD state
when calling the terminal write function.

#### Terminal characteristics

The terminal should strive for Linux console compatibility.

### Framebuffer Feature

ID:
```c
#define LIMINE_FRAMEBUFFER_REQUEST { LIMINE_COMMON_MAGIC, 0x9d5827dcd881dd75, 0xa3148604f6fab11b }
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
    uint64_t width;
    uint64_t height;
    uint64_t pitch;
    uint16_t bpp; // Bits per pixel
    uint8_t memory_model;
    uint8_t red_mask_size;
    uint8_t red_mask_shift;
    uint8_t green_mask_size;
    uint8_t green_mask_shift;
    uint8_t blue_mask_size;
    uint8_t blue_mask_shift;
    uint8_t unused[7];
    uint64_t edid_size;
    void *edid;

    /* Response revision 1 */
    uint64_t mode_count;
    struct limine_video_mode **modes;
};
```

`modes` is an array of `mode_count` pointers to `struct limine_video_mode` describing the
available video modes for the given framebuffer.

```c
struct limine_video_mode {
    uint64_t pitch;
    uint64_t width;
    uint64_t height;
    uint16_t bpp;
    uint8_t memory_model;
    uint8_t red_mask_size;
    uint8_t red_mask_shift;
    uint8_t green_mask_size;
    uint8_t green_mask_shift;
    uint8_t blue_mask_size;
    uint8_t blue_mask_shift;
};
```

### Paging Mode Feature

The Paging Mode feature allows the kernel to control which paging mode is enabled
before control is passed to it.

ID:
```c
#define LIMINE_PAGING_MODE_REQUEST { LIMINE_COMMON_MAGIC, 0x95c1a0edab0944cb, 0xa4e5cb3842f7488a }
```

Request:
```c
struct limine_paging_mode_request {
    uint64_t id[4];
    uint64_t revision;
    struct limine_paging_mode_response *response;
    uint64_t mode;
    uint64_t flags;
};
```

Both the `mode` and `flags` fields are architecture-specific.

The `LIMINE_PAGING_MODE_DEFAULT` macro is provided by all architectures to select
the default paging mode (see below).

Response:
```c
struct limine_paging_mode_response {
    uint64_t revision;
    uint64_t mode;
    uint64_t flags;
};
```

The response indicates which paging mode was actually enabled by the bootloader.
Kernels must be prepared to handle the case where the requested paging mode is
not supported by the hardware.

#### x86-64

Values for `mode`:
```c
#define LIMINE_PAGING_MODE_X86_64_4LVL 0
#define LIMINE_PAGING_MODE_X86_64_5LVL 1

#define LIMINE_PAGING_MODE_DEFAULT LIMINE_PAGING_MODE_X86_64_4LVL
```

No `flags` are currently defined.

The default mode (when this request is not provided) is `LIMINE_PAGING_MODE_X86_64_4LVL`.

#### aarch64

Values for `mode`:
```c
#define LIMINE_PAGING_MODE_AARCH64_4LVL 0
#define LIMINE_PAGING_MODE_AARCH64_5LVL 1

#define LIMINE_PAGING_MODE_DEFAULT LIMINE_PAGING_MODE_AARCH64_4LVL
```

No `flags` are currently defined.

The default mode (when this request is not provided) is `LIMINE_PAGING_MODE_AARCH64_4LVL`.

#### riscv64

Values for `mode`:
```c
#define LIMINE_PAGING_MODE_RISCV_SV39 0
#define LIMINE_PAGING_MODE_RISCV_SV48 1
#define LIMINE_PAGING_MODE_RISCV_SV57 2

#define LIMINE_PAGING_MODE_DEFAULT LIMINE_PAGING_MODE_RISCV_SV48
```

No `flags` are currently defined.

The default mode (when this request is not provided) is `LIMINE_PAGING_MODE_RISCV_SV48`.

### 5-Level Paging Feature

Note: *This feature has been deprecated in favor of the [Paging Mode feature](#paging-mode-feature)
and has been removed as of Limine 5.x.*

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

Note: The presence of this request will prompt the bootloader to turn on
x86-64 5-level paging. It will not be turned on if this request is not present.
If the response pointer is changed to a valid pointer, 5-level paging is engaged.

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

* `flags` - Bit 0: Enable X2APIC, if possible. (x86-64 only)

#### x86-64:

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

Note: The presence of this request will prompt the bootloader to bootstrap
the secondary processors. This will not be done if this request is not present.

Note: The MTRRs of APs will be synchronised by the bootloader to match
the BSP, as Intel SDM requires (Vol. 3A, 12.11.5).

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
jump to the written address, on a 64KiB (or Stack Size Request size) stack. A pointer to the
`struct limine_smp_info` structure of the CPU is passed in `RDI`. Other than
that, the CPU state will be the same as described for the bootstrap
processor. This field is unused for the structure describing the bootstrap
processor. For all CPUs, this field is guaranteed to be NULL when control is first passed
to the bootstrap processor.
* `extra_argument` - A free for use field.

#### aarch64:

Response:

```c
struct limine_smp_response {
    uint64_t revision;
    uint64_t flags;
    uint64_t bsp_mpidr;
    uint64_t cpu_count;
    struct limine_smp_info **cpus;
};
```

* `flags` - Always zero
* `bsp_mpidr` - MPIDR of the bootstrap processor (as read from `MPIDR_EL1`, with Res1 masked off).
* `cpu_count` - How many CPUs are present. It includes the bootstrap processor.
* `cpus` - Pointer to an array of `cpu_count` pointers to
`struct limine_smp_info` structures.

Note: The presence of this request will prompt the bootloader to bootstrap
the secondary processors. This will not be done if this request is not present.

```c
struct limine_smp_info;

typedef void (*limine_goto_address)(struct limine_smp_info *);

struct limine_smp_info {
    uint32_t processor_id;
    uint32_t gic_iface_no;
    uint64_t mpidr;
    uint64_t reserved;
    limine_goto_address goto_address;
    uint64_t extra_argument;
};
```

* `processor_id` - ACPI Processor UID as specified by the MADT
* `gic_iface_no` - GIC CPU Interface number of the processor as specified by the MADT (possibly always 0)
* `mpidr` - MPIDR of the processor as specified by the MADT or device tree
* `goto_address` - An atomic write to this field causes the parked CPU to
jump to the written address, on a 64KiB (or Stack Size Request size) stack. A pointer to the
`struct limine_smp_info` structure of the CPU is passed in `X0`. Other than
that, the CPU state will be the same as described for the bootstrap
processor. This field is unused for the structure describing the bootstrap
processor.
* `extra_argument` - A free for use field.

#### riscv64

Response:

```c
struct limine_smp_response {
    uint64_t revision;
    uint64_t flags;
    uint64_t bsp_hartid;
    uint64_t cpu_count;
    struct limine_smp_info **cpus;
};
```

* `flags` - Always zero
* `bsp_hartid` - Hart ID of the bootstrap processor as reported by the UEFI RISC-V Boot Protocol or the SBI.
* `cpu_count` - How many CPUs are present. It includes the bootstrap processor.
* `cpus` - Pointer to an array of `cpu_count` pointers to
`struct limine_smp_info` structures.

Note: The presence of this request will prompt the bootloader to bootstrap
the secondary processors. This will not be done if this request is not present.

```c
struct limine_smp_info;

typedef void (*limine_goto_address)(struct limine_smp_info *);

struct limine_smp_info {
    uint64_t processor_id;
    uint64_t hartid;
    uint64_t reserved;
    limine_goto_address goto_address;
    uint64_t extra_argument;
};
```

* `processor_id` - ACPI Processor UID as specified by the MADT (always 0 on non-ACPI systems).
* `hartid` - Hart ID of the processor as specified by the MADT or Device Tree.
* `goto_address` - An atomic write to this field causes the parked CPU to
jump to the written address, on a 64KiB (or Stack Size Request size) stack. A pointer to the
`struct limine_smp_info` structure of the CPU is passed in `x10`(`a0`). Other than
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

Note: Memory between 0 and 0x1000 is never marked as usable memory.
The kernel and modules loaded are not marked as usable memory.
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
#define LIMINE_INTERNAL_MODULE_REQUIRED (1 << 0)
#define LIMINE_INTERNAL_MODULE_COMPRESSED (1 << 1)

struct limine_internal_module {
    const char *path;
    const char *cmdline;
    uint64_t flags;
};

struct limine_module_request {
    uint64_t id[4];
    uint64_t revision;
    struct limine_module_response *response;

    /* Request revision 1 */
    uint64_t internal_module_count;
    struct limine_internal_module **internal_modules;
};
```

* `internal_module_count` - How many internal modules are passed by the kernel.
* `internal_modules` - Pointer to an array of `internal_module_count` pointers to
`struct limine_internal_module` structures.

Note: Internal modules are honoured if the module response has revision >= 1.

As part of `struct limine_internal_module`:

* `path` - Path to the module to load. This path is *relative* to the location of
the kernel.
* `cmdline` - Command line for the given module.
* `flags` - Flags changing module loading behaviour:
  - `LIMINE_INTERNAL_MODULE_REQUIRED`: Fail if the requested module is not found.
  - `LIMINE_INTERNAL_MODULE_COMPRESSED`: The module is GZ-compressed and should be decompressed by the bootloader. This is honoured if the response is revision 2 or greater.

Internal Limine modules are guaranteed to be loaded *before* user-specified
(configuration) modules, and thus they are guaranteed to appear before user-specified
modules in the `modules` array in the response.

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

#define LIMINE_MEDIA_TYPE_GENERIC 0
#define LIMINE_MEDIA_TYPE_OPTICAL 1
#define LIMINE_MEDIA_TYPE_TFTP 2

struct limine_file {
    uint64_t revision;
    void *address;
    uint64_t size;
    char *path;
    char *cmdline;
    uint32_t media_type;
    uint32_t unused;
    uint32_t tftp_ip;
    uint32_t tftp_port;
    uint32_t partition_index;
    uint32_t mbr_disk_id;
    struct limine_uuid gpt_disk_uuid;
    struct limine_uuid gpt_part_uuid;
    struct limine_uuid part_uuid;
};
```

* `revision` - Revision of the `struct limine_file` structure.
* `address` - The address of the file. This is always at least 4KiB aligned.
* `size` - The size of the file.
* `path` - The path of the file within the volume, with a leading slash.
* `cmdline` - A command line associated with the file.
* `media_type` - Type of media file resides on.
* `tftp_ip` - If non-0, this is the IP of the TFTP server the file was loaded
from.
* `tftp_port` - Likewise, but port.
* `partition_index` - 1-based partition index of the volume from which the
file was loaded. If 0, it means invalid or unpartitioned.
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

### EFI Memory Map Feature

ID:
```c
#define LIMINE_EFI_MEMMAP_REQUEST { LIMINE_COMMON_MAGIC, 0x7df62a431d6872d5, 0xa4fcdfb3e57306c8 }
```

Request:
```c
struct limine_efi_memmap_request {
    uint64_t id[4];
    uint64_t revision;
    struct limine_efi_memmap_response *response;
};
```

Response:
```c
struct limine_efi_memmap_response {
    uint64_t revision;
    void *memmap;
    uint64_t memmap_size;
    uint64_t desc_size;
    uint64_t desc_version;
};
```

* `memmap` - Address (HHDM) of the EFI memory map.
* `memmap_size` - Size in bytes of the EFI memory map.
* `desc_size` - EFI memory map descriptor size in bytes.
* `desc_version` - Version of EFI memory map descriptors.

Note: This feature provides data suitable for use with RT->SetVirtualAddressMap(), provided
HHDM offset is subtracted from `memmap`.

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

### Device Tree Blob Feature

ID:
```c
#define LIMINE_DTB_REQUEST { LIMINE_COMMON_MAGIC, 0xb40ddb48fb54bac7, 0x545081493f81ffb7 }
```

Request:
```c
struct limine_dtb_request {
    uint64_t id[4];
    uint64_t revision;
    struct limine_dtb_response *response;
};
```

Response:
```c
struct limine_dtb_response {
    uint64_t revision;
    void *dtb_ptr;
};
```

* `dtb_ptr` - Virtual pointer to the device tree blob.

Note: If the DTB cannot be found, the response will *not* be generated.

Note: Information contained in the `/chosen` node may not reflect the information
given by bootloader tags, and as such the `/chosen` node properties should be ignored.
