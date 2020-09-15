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

### Building the bootloader
Building the bootloader is not necessary as a prebuilt copy is shipped in this
repository (`limine.bin`).

Should one want to build the bootloader to make sure the shipped copy is authentic,
to develop, to debug, or any other reason, it is necessary to first build the
set of tools that the bootloader needs in order to be built.

This can be accomplished by running:
```bash
make toolchain
```
*The above step may take a while*

After that is done, the bootloader itself can be built with:
```bash
make
```

A newly generated `limine.bin` image should now be present in the root of the repo.

This newly built image should match 1:1 (aka, same checksum) with the one shipped
with the respective commit.

### Compiling `limine-install`
To build the `limine-install` program, simply run `make limine-install` in the root
of the repo.

## How to use
### MBR
In order to install Limine on a MBR device (which can just be a raw image file),
run the `limine-install` as such:

```bash
limine-install <bootloader image> <path to device/image>
```

Where `<bootloader image>` is the path to a `limine.bin` file.

### GPT
If using a GPT formatted device, it will be necessary to create an extra partition
(of at least 32K in size) to store stage 2 code. Then it will be necessary to tell
`limine-install` where this partition is located by specifying the start sector
number (in decimal).

```bash
fdisk <device>    # Create bootloader partition using your favourite method
limine-install <bootloader image> <path to device/image> <start sector of boot partition>
```

### Configuration
Then make sure the device/image contains at least 1 partition formatted in
a supported filesystem containing a `/limine.cfg` or `/boot/limine.cfg` file
and the kernel/modules one wants to load.

An example `limine.cfg` file can be found in `test/limine.cfg`.

More info on the format of `limine.cfg` can be found in `CONFIG.md`.

### Example
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

## Acknowledgments
Limine uses a stripped-down version of https://github.com/jibsen/tinf

## Discord server
We have a Discord server if you need support, info, or you just want to
hang out: https://discord.gg/QEeZMz4
