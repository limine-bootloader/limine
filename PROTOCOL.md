# The Limine Boot Protocol

The Limine boot protocol is a modern, minimal, fast, and extensible boot
protocol, with a focus on backwards and forwards compatibility,
created from the experienced gained by working on the
[stivale boot protocols](https://github.com/stivale).

This file serves as the official centralised collection of features that
the Limine boot protocol is composed of. Other bootloaders may support extra
unofficial features, but it is strongly recommended to avoid fragmentation
and submit new features by opening a pull request to this repository.

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

## Executable memory layout

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

IF flag, VM flag, and direction flag are cleared on entry. Other flags undefined.

PG is enabled (`cr0`), PE is enabled (`cr0`), PAE is enabled (`cr4`),
LME is enabled (`EFER`).
If 5-level paging is requested and available, then 5-level paging is enabled
(LA57 bit in `cr4`).
The NX bit will be enabled (NX bit in `EFER`).

The A20 gate is opened.

Legacy PIC and IO APIC IRQs are all masked.

If booted by EFI/UEFI, boot services are exited.

`rsp` is set to point to a stack, in bootloader-reserved memory, which is
at least 8KiB (8192 bytes) in size. An invalid return address of 0 is pushed
to the stack before jumping to the kernel.

All other general purpose registers are set to 0.
