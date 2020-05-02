# qloader2
x86/x86_64 BIOS Bootloader

### Supported boot protocols
* Linux
* stivale (qloader2's native boot protocol, see STIVALE.md for details)

### Supported filesystems
* ext2
* echfs
* FAT32

### Supported partitioning schemes
* MBR
* GPT

## How to use
This repository contains a prebuilt version of qloader2 so building it won't
be necessary.

In order to install qloader2 on a MBR device (which can just be a raw image file),
run the provided `qloader2-install` script as such:

```bash
./qloader2-install ./qloader2.bin <path to device/image>
```

If using a GPT formatted device, it will be necessary to create an extra partition
(of at least 32K in size) to store stage 2 code. Then it will be necessary to tell
the install script where this partition is located by specifying the start sector.

```bash
fdisk <device>    # Create bootloader partition using your favourite method
./qloader2-install ./qloader2.bin <path to device/image> <start sector of boot partition>
```

Then make sure the device/image contains at least 1 partition formatted in
a supported filesystem containing a `/qloader2.cfg` or `/boot/qloader2.cfg` file
and the kernel/modules one wants to load.

An example `qloader2.cfg` file can be found in `test/qloader2.cfg`.

More info on the format of `qloader2.cfg` can be found in `CONFIG.md`.

For example, to create an empty image file of 64MiB in size, 1 echfs partition
on the image spanning the whole device, format it, copy the relevant files over,
and install qloader2, one can do:

```bash
dd if=/dev/zero bs=1M count=0 seek=64 of=test.img
parted -s test.img mklabel msdos
parted -s test.img mkpart primary 1 100%
echfs-utils -m -p0 test.img quick-format 32768
echfs-utils -m -p0 test.img import path/to/qloader2.cfg qloader2.cfg
echfs-utils -m -p0 test.img import path/to/kernel.elf kernel.elf
echfs-utils -m -p0 test.img import <path to file> <path in image>
...
./qloader2-install $THIS_REPO/qloader2.bin test.img

```

One can get `echfs-utils` by installing https://github.com/qword-os/echfs.

## Building from source
In order to hack qloader2, one must build the GCC toolchain from source first.

To do so, run the `make_toolchain.sh` script from within the `toolchain` directory;
keep in mind that the script takes `MAKEFLAGS` as an argument.

```bash
cd toolchain
./make_toolchain.sh -j4
```

After that is done, simply run `make` in the root of the repo to generate
`src/qloader2.bin`.

## Discord server
We have a Discord server if you need support, info, or you just want to
hang out: https://discord.gg/QEeZMz4
