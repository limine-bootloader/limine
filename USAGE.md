# Usage

### UEFI
The `BOOT*.EFI` files are valid EFI applications that can be simply copied to
the `/EFI/BOOT` directory of a FAT formatted EFI system partition. These files can
be installed there and coexist with a BIOS installation of Limine (see below) so
that the disk will be bootable on both BIOS and UEFI systems.

The boot device must contain the `limine.conf` files in
either the root, `limine`, `boot`, or `boot/limine` directory of one of the
partitions, formatted with a supported file system (the ESP partition is recommended).

### Secure Boot
Limine can be booted with secure boot if the executable is signed and the key used to
sign it is added to the firmware's keychain. This should be done in combination with enrolling
the BLAKE2B hash of the Limine config file into the Limine EFI executable image itself for
verification purposes.
For more information see the `limine enroll-config` program and [the philosophy](PHILOSOPHY.md).

### BIOS/MBR
In order to install Limine on a MBR device (which can just be a raw image file),
run `limine bios-install` as such:

```bash
limine bios-install <path to device/image>
```

The boot device must contain the `limine-bios.sys` and `limine.conf` files in
either the root, `limine`, `boot`, or `boot/limine` directory of one of the
partitions, formatted with a supported file system.

### BIOS/GPT
If using a GPT formatted device, there are 2 options one can follow for
installation:
* Specifying a dedicated stage 2 partition.
* Letting `limine bios-install` attempt to embed stage 2 within GPT structures.

In case one wants to specify a stage 2 partition, create a partition on the GPT
device of at least 32KiB in size, and pass the 1-based number of the partition
to `limine bios-install` as a second argument; such as:

```bash
limine bios-install <path to device/image> <1-based stage 2 partition number>
```

In case one wants to let `limine bios-install` embed stage 2 within GPT's structures,
simply omit the partition number, and invoke `limine bios-install` the same as one
would do for an MBR partitioned device.

The boot device must contain the `limine-bios.sys` and `limine.conf` files in
either the root, `limine`, `boot`, or `boot/limine` directory of one of the
partitions, formatted with a supported file system.

### BIOS/UEFI hybrid ISO creation
In order to create a hybrid ISO with Limine, place the
`limine-uefi-cd.bin`, `limine-bios-cd.bin`, `limine-bios.sys`, and `limine.conf` files
into a directory which will serve as the root of the created ISO.
(`limine-bios.sys` and `limine.conf` must either be in the root, `limine`, `boot`, or
`boot/limine` directory; `limine-uefi-cd.bin` and `limine-bios-cd.bin` can reside
anywhere).

After that, create a `<ISO root directory>/EFI/BOOT` directory and copy the
relevant Limine EFI executables over (such as `BOOTX64.EFI`).

Place any other file you want to be on the final ISO in said directory, then
run:
```
xorriso -as mkisofs -b <relative path of limine-bios-cd.bin> \
        -no-emul-boot -boot-load-size 4 -boot-info-table \
        --efi-boot <relative path of limine-uefi-cd.bin> \
        -efi-boot-part --efi-boot-image --protective-msdos-label \
        <root directory> -o image.iso
```

*Note: `xorriso` is required.*

And do not forget to also run `limine bios-install` on the generated image:
```
limine bios-install image.iso
```

`<relative path of limine-bios-cd.bin>` is the relative path of
`limine-bios-cd.bin` inside the root directory.
For example, if it was copied in `<root directory>/boot/limine-bios-cd.bin`,
it would be `boot/limine-bios-cd.bin`.

`<relative path of limine-uefi-cd.bin>` is the relative path of
`limine-uefi-cd.bin` inside the root directory.
For example, if it was copied in
`<root directory>/boot/limine-uefi-cd.bin`, it would be
`boot/limine-uefi-cd.bin`.

### BIOS/PXE boot
The `limine-bios-pxe.bin` binary is a valid PXE boot image.
In order to boot Limine from PXE it is necessary to setup a DHCP server with
support for PXE booting. This can either be accomplished using a single DHCP
server or your existing DHCP server and a proxy DHCP server such as dnsmasq.

`limine.conf` and `limine-bios.sys` are expected to be on the server used for boot.

### UEFI/PXE boot
The `BOOT*.EFI` files are compatible with UEFI PXE.
The steps needed to boot Limine are the same as with BIOS PXE,
except that the `limine-bios.sys` file is not needed on the server.

### Configuration
The `limine.conf` file contains Limine's configuration.

More info on the format of `limine.conf` can be found in [`CONFIG.md`](CONFIG.md).
