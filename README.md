# Limine
x86/x86_64 BIOS Bootloader

![Reference screenshot](/screenshot.png?raw=true "Reference screenshot")

### Supported boot protocols
* Linux
* stivale and stivale2 (Limine's native boot protocols, see STIVALE{,2}.md for details)

### Supported filesystems
* ext2
* echfs
* FAT32

### Supported partitioning schemes
* MBR
* GPT

## Building

### Dependencies
To build Limine, it is necessary to have an LLVM/Clang toolchain installed.
More specifically, the following programs need to be present: `clang`, `llvm-objcopy`,
`llvm-link`, `opt`, `ld.lld`.
Furthermore, `nasm` also needs to be installed.
`curl`, `tar`, and `zstd` need to be installed for retrieving `libgcc.a` during build.

### Compiling
A simple `make` and `make install` will suffice. Use the PREFIX variable with
`make install` to specify where to install `limine-install`. It defaults to
`/usr/local`.

## How to use
In order to install Limine on a MBR device (which can just be a raw image file),
run the `limine-install` as such:

```bash
limine-install <path to device/image>
```

If using a GPT formatted device, it will be necessary to create an extra partition
(of at least 32K in size) to store stage 2 code. Then it will be necessary to tell
`limine-install` where this partition is located by specifying the start sector.

```bash
fdisk <device>    # Create bootloader partition using your favourite method
limine-install <path to device/image> <start sector of boot partition>
```

Then make sure the device/image contains at least 1 partition formatted in
a supported filesystem containing a `/limine.cfg` or `/boot/limine.cfg` file
and the kernel/modules one wants to load.

An example `limine.cfg` file can be found in `test/limine.cfg`.

More info on the format of `limine.cfg` can be found in `CONFIG.md`.

For example, to create an empty image file of 64MiB in size, 1 echfs partition
on the image spanning the whole device, format it, copy the relevant files over,
and install Limine, one can do:

```bash
dd if=/dev/zero bs=1M count=0 seek=64 of=test.img
parted -s test.img mklabel msdos
parted -s test.img mkpart primary 1 100%
parted -s test.img set 1 boot on # Workaround for buggy BIOSes

echfs-utils -m -p0 test.img quick-format 32768
echfs-utils -m -p0 test.img import path/to/limine.cfg limine.cfg
echfs-utils -m -p0 test.img import path/to/kernel.elf kernel.elf
echfs-utils -m -p0 test.img import <path to file> <path in image>
...
limine-install test.img

```

One can get `echfs-utils` by installing https://github.com/qword-os/echfs.

## Discord server
We have a Discord server if you need support, info, or you just want to
hang out: https://discord.gg/QEeZMz4
