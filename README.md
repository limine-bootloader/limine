# qloader2
x86/x86_64 BIOS Bootloader

## How to use
This repository contains a prebuilt version of qloader2 so building it won't
be necessary.

In order to install qloader2 on a device (which can just be a raw image file),
run the provided `qloader2-install` script as such:

```bash
./qloader2-install ./qloader2.bin <path to device/image>
```

Then make sure the device/image contains at least 1 partition formatted in
echfs or ext2 containing a `qloader2.cfg` file and the kernel/modules one wants to load.

An example `qloader2.cfg` file can be found in `test/qloader2.cfg`.

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
