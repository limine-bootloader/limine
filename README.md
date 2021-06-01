# Limine

### What is Limine?

Limine is an advanced x86/x86_64 BIOS/UEFI Bootloader which supports *modern* PC features
such as Long Mode, 5-level paging, and SMP (multicore), to name a few.

### Support Limine
Like Limine and want to support it? Donate Bitcoin to `bc1q00d59y75crpapw7qp8sea5j5sx6l4k2ssjylf9` :)

### Limine's boot menu

![Reference screenshot](/screenshot.png?raw=true "Reference screenshot")

[Photo by Nishant Aneja from Pexels](https://www.pexels.com/photo/close-up-photo-of-waterdrops-on-glass-2527248/)

### Supported boot protocols
* Linux
* stivale and stivale2 (Limine's native boot protocols, see [their specifications](https://github.com/stivale/stivale) for details)
* Chainloading

### Supported filesystems
* ext2/3/4
* echfs
* FAT12/16/32
* ISO9660 (CDs/DVDs)

### Supported partitioning schemes
* MBR
* GPT
* Unpartitioned media

## Binary releases

For convenience, for point releases, binaries are distributed. These binaries are
shipped in the `-binary` branches and tags of this repository (see [branches](https://github.com/limine-bootloader/limine/branches/all) and [tags](https://github.com/limine-bootloader/limine/tags)).

For example, to clone the latest binary release of the `v2.x` branch one can do
```bash
git clone https://github.com/limine-bootloader/limine.git --branch=v2.0-branch-binary --depth=1
```
or, to clone a specific binary point release (for example v2.4)
```bash
git clone https://github.com/limine-bootloader/limine.git --branch=v2.4-binary --depth=1
```

Additionally, the absolute latest Limine binary release can be obtained by fetching
the `latest-binary` branch:
```bash
git clone https://github.com/limine-bootloader/limine.git --branch=latest-binary --depth=1
```

`limine-install` binaries are provided for Linux and Windows. In case one wants to
rebuild `limine-install`, simply use `make` in the binary release.

## Building the bootloader

*These steps are not necessary if cloning a binary release. If so, skip to*
*"Installing Limine binaries".*

**Note that the build system does not support building from a directory whose**
**full path contains spaces.**

### Building the toolchain

This step can take a long time, but it will ensure that the compiler will work with
Limine. If on an x86_64 host, with GCC or Clang installed, you can also skip to the next paragraph.

The toolchain building process depends on the following packages: `bash`, `make`,
`wget`, `gcc`, `g++`, `GNU binutils`.

Building the toolchain can be accomplished by running:
```bash
make toolchain
```
*The above step may take a while*

### Building Limine

In order to build Limine, the following packages have to be installed: `bash`, `make`,
`git`, `which`, `nasm`, `mtools` (optional, necessary to build `limine-eltorito-efi.bin`).
Furthermore, either the toolchain must have been built in the previous paragraph,
or `gcc` or `llvm/clang` must also be installed.
`GNU binutils` is necessary in order to build the UEFI port of Limine. A full
LLVM toolchain without `GNU binutils` can be used to build the BIOS port instead.

Both the UEFI and BIOS ports of the bootloader can be built, using `GCC/GNU binutils`, with:
```bash
make
```

It is possible to pass `make` additional flags, most relevantly, `TOOLCHAIN=` which
allows one to specify an alternative toolchain for the build system to use
(the default is `x86_64-elf`, falling back to no-triple, or host, toolchain).

The generated bootloader files are going to be in `bin`.

#### Using clang/LLVM

In order to build the BIOS port fully using clang/LLVM, run `make` as such:
```bash
make limine-bios bin/limine-install CC="clang" TOOLCHAIN="llvm" TOOLCHAIN_CC="clang" TOOLCHAIN_LD="ld.lld"
```
And in order to build the UEFI port using clang/LLVM + `GNU binutils`, run `make` as such:
```bash
make limine-uefi TOOLCHAIN="llvm" TOOLCHAIN_CC="clang" TOOLCHAIN_LD="ld" TOOLCHAIN_OBJCOPY="objcopy"
```

## Installing Limine binaries

This step is optional as the bootloader binaries can be used from the `bin` or
release directory just fine. This step will only install them in a `share` and `bin`
directories in the specified `PREFIX` (default is `/usr/local`).

Use `make install` to install Limine binaries, optionally specifying a prefix with a
`PREFIX=...` option.

## How to use

### UEFI
The `BOOTX64.EFI` file is a vaild EFI application that can be simply copied to the
`/EFI/BOOT` directory of a FAT formatted EFI system partition. This file can be
installed there and coexist with a BIOS installation of Limine (see below) so that
the disk will be bootable by both BIOS and UEFI.

The boot device must to contain the `limine.cfg` file in
either the root or the `boot` directory of one of the partitions, formatted
with a supported file system (the ESP partition is recommended).

### BIOS/MBR
In order to install Limine on a MBR device (which can just be a raw image file),
run `limine-install` as such:

```bash
limine-install <path to device/image>
```

The boot device must to contain the `limine.sys` and `limine.cfg` files in
either the root or the `boot` directory of one of the partitions, formatted
with a supported file system.

### BIOS/GPT
If using a GPT formatted device, there are 2 options one can follow for installation:
* Specifying a dedicated stage 2 partition.
* Letting `limine-install` attempt to embed stage 2 within GPT structures.

In case one wants to specify a stage 2 partition, create a partition on the GPT
device of at least 32KiB in size, and pass the 1-based number of the partition
to `limine-install` as a second argument; such as:

```bash
limine-install <path to device/image> <1-based stage 2 partition number>
```

In case one wants to let `limine-install` embed stage 2 within GPT's structures,
simply omit the partition number, and invoke `limine-install` the same as one would
do for an MBR partitioned device.

The boot device must to contain the `limine.sys` and `limine.cfg` files in
either the root or the `boot` directory of one of the partitions, formatted
with a supported file system.

### BIOS/UEFI hybrid ISO creation
In order to create a hybrid ISO with Limine, place the `limine-eltorito-efi.bin`,
`limine-cd.bin`, `limine.sys`, and `limine.cfg` files into a directory which will
serve as the root of the created ISO.
(`limine.sys` and `limine.cfg` must either be in the root or inside a `boot`
subdirectory; `limine-eltorito-efi.bin` and `limine-cd.bin` can reside anywhere).

Place any other file you want to be on the final ISO in said directory, then run:
```
xorriso -as mkisofs -b <relative path of limine-cd.bin> \
        -no-emul-boot -boot-load-size 4 -boot-info-table \
        --efi-boot <relative path of limine-eltorito-efi.bin> \
        -efi-boot-part --efi-boot-image --protective-msdos-label \
        <root directory> -o image.iso
```

*Note: `xorriso` is required.*

And do not forget to also run `limine-install` on the generated image:
```
limine-install image.iso
```

`<relative path of limine-cd.bin>` is the relative path of
`limine-cd.bin` inside the root directory.
For example, if it was copied in `<root directory>/boot/limine-cd.bin`,
it would be `boot/limine-cd.bin`.

`<relative path of limine-eltorito-efi.bin>` is the relative path of
`limine-eltorito-efi.bin` inside the root directory.
For example, if it was copied in `<root directory>/boot/limine-eltorito-efi.bin`,
it would be `boot/limine-eltorito-efi.bin`.

### BIOS/PXE boot
The `limine-pxe.bin` binary is a valid PXE boot image.
In order to boot Limine from PXE it is necessary to setup a DHCP server with
support for PXE booting. This can either be accomplished using a single DHCP server
or your existing DHCP server and a proxy DHCP server such as dnsmasq.

`limine.cfg` and `limine.sys` are expected to be on the server used for boot.

### Configuration
The `limine.cfg` file contains Limine's configuration.

An example `limine.cfg` file can be found in `test/limine.cfg`.

More info on the format of `limine.cfg` can be found in `CONFIG.md`.

## Acknowledgments
Limine uses a stripped-down version of [tinf](https://github.com/jibsen/tinf).

## Discord server
We have a [Discord server](https://discord.gg/QEeZMz4) if you need support, info, or
you just want to hang out with us.
