# Limine

<p align="center">
    <img src="https://github.com/limine-bootloader/limine/blob/trunk/logo.png?raw=true" alt="Limine's logo"/>
</p>

### What is Limine?

Limine (pronounced as demonstrated [here](https://www.merriam-webster.com/dictionary/in%20limine))
is a modern, advanced, portable, multiprotocol bootloader and boot manager, also used
as the reference implementation for the [Limine boot protocol](PROTOCOL.md).

### Donate

If you want to support the work I ([@mintsuki](https://github.com/mintsuki)) do on Limine, feel free to donate to me on Liberapay:
<a href="https://liberapay.com/mintsuki/donate"><img alt="Donate using Liberapay" src="https://liberapay.com/assets/widgets/donate.svg"></a>

Donations welcome, but absolutely not mandatory!

### Limine's boot menu

![Reference screenshot](screenshot.png?raw=true "Reference screenshot")

[Photo by Mo Eid](https://www.pexels.com/photo/a-pavilion-n-the-middle-of-a-sea-8832898/)

### Supported architectures
* IA-32 (32-bit x86)
* x86-64
* aarch64 (arm64)
* riscv64

### Supported boot protocols
* Linux
* [Limine](PROTOCOL.md)
* Multiboot 1
* Multiboot 2
* Chainloading

### Supported partitioning schemes
* MBR
* GPT
* Unpartitioned media

### Supported filesystems
* FAT12/16/32
* ISO9660 (CDs/DVDs)

If your filesystem isn't listed here, please read [the philosophy](PHILOSOPHY.md) first, especially before
opening issues or pull requests related to this.

### Minimum system requirements
For 32-bit x86 systems, support is only ensured starting with those with
Pentium Pro (i686) class CPUs.

All x86-64, aarch64, and riscv64 (UEFI) systems are supported.

## Packaging status

All Limine releases since 7.x use [Semantic Versioning](https://semver.org/spec/v2.0.0.html) for their naming.

[![Packaging status](https://repology.org/badge/vertical-allrepos/limine.svg?columns=3)](https://repology.org/project/limine/versions)

## Binary releases

For convenience, for point releases, binaries are distributed. These binaries
are shipped in the `-binary` branches and tags of this repository
(see [branches](https://github.com/limine-bootloader/limine/branches/all) and
[tags](https://github.com/limine-bootloader/limine/tags)).

For example, to clone the latest binary release of the `7.x` branch, one can do:
```bash
git clone https://github.com/limine-bootloader/limine.git --branch=v7.x-binary --depth=1
```
or, to clone a specific binary point release (for example `7.10.3`):
```bash
git clone https://github.com/limine-bootloader/limine.git --branch=v7.10.3-binary --depth=1
```

In order to rebuild host utilities like `limine`, simply run `make` in the binary
release directory.

Host utility binaries are provided for Windows.

## Building the bootloader

*The following steps are not necessary if cloning a binary release. If so, skip to*
*"Installing Limine binaries".*

### Building with Nix

This repository provides [Nix](https://nixos.org/)-based tooling for a convenient
development environment and building Limine using Nix.

To use the regular build flow using a toolchain obtained by Nix, simply
run `$ nix develop` to open a Nix shell. Then follow the guide below,
skipping the "Prerequisites" step.

### Regular build

#### Prerequisites

In order to build Limine, the following programs have to be installed:
common UNIX tools (also known as `coreutils`),
`GNU make`, `grep`, `sed`, `find`, `awk`, `gzip`, `nasm`, `mtools`
(optional, necessary to build `limine-uefi-cd.bin`).
Furthermore, `gcc` or `llvm/clang` must also be installed, alongside
the respective binutils.

#### Configure

If using a release tarball (recommended, see https://github.com/limine-bootloader/limine/releases),
run `./configure` directly.

If checking out from the repository, run `./bootstrap` first in order to download the
necessary dependencies and generate the configure script (`GNU autoconf` required).

`./configure` takes arguments and environment variables; for more information on
these, run `./configure --help`.

**`./configure` by default does not build any Limine port. Make sure to read the**
**output of `./configure --help` and enable any or all ports!**

Limine supports both in-tree and out-of-tree builds. Simply run the `configure`
script from the directory you wish to execute the build in. The following `make`
commands are supposed to be run inside the build directory.

#### Building Limine

To build Limine, run:
```bash
make    # (or gmake where applicable)
```

## Installing Limine

This step will install Limine files to `share`, `include`, and
`bin` directories in the specified prefix (default is `/usr/local`, see
`./configure --help`, or the `PREFIX` variable if installing from a binary release).

To install Limine, run:
```bash
make install    # (or gmake where applicable)
```

## Usage

See [USAGE.md](USAGE.md).

## Acknowledgments
Limine uses a stripped-down version of [tinf](https://github.com/jibsen/tinf) for early GZIP decompression.

Limine relies on [stb_image](https://github.com/nothings/stb/blob/master/stb_image.h) for runtime GZIP decompression and image loading.

Limine uses a patched version of libfdt (can be found in Linux's source tree) for manipulating FDTs.

## Discord server
We have a [Discord server](https://discord.gg/QEeZMz4) if you need support,
info, or you just want to hang out with us.
