#!/bin/sh

set -e
set -x

TARGET=x86_64-elf
BINUTILSVERSION=2.36.1
GCCVERSION=10.3.0

CFLAGS="-O2 -pipe"

mkdir -p "$1" && cd "$1"
PREFIX="$(pwd)"

export MAKEFLAGS="$2"

export PATH="$PREFIX/bin:$PATH"

if [ ! -f binutils-$BINUTILSVERSION.tar.gz ]; then
    wget https://ftp.gnu.org/gnu/binutils/binutils-$BINUTILSVERSION.tar.gz
fi
if [ ! -f gcc-$GCCVERSION.tar.gz ]; then
    wget https://ftp.gnu.org/gnu/gcc/gcc-$GCCVERSION/gcc-$GCCVERSION.tar.gz
fi

rm -rf build
mkdir build
cd build

tar -xf ../binutils-$BINUTILSVERSION.tar.gz
tar -xf ../gcc-$GCCVERSION.tar.gz

mkdir build-binutils
cd build-binutils
../binutils-$BINUTILSVERSION/configure CFLAGS="$CFLAGS" CXXFLAGS="$CFLAGS"  --target=$TARGET --prefix="$PREFIX" --with-sysroot --disable-nls --disable-werror --enable-targets=x86_64-elf,x86_64-pe
make
make install
cd ..

cd gcc-$GCCVERSION
contrib/download_prerequisites
cd ..
mkdir build-gcc
cd build-gcc
../gcc-$GCCVERSION/configure CFLAGS="$CFLAGS" CXXFLAGS="$CFLAGS" --target=$TARGET --prefix="$PREFIX" --disable-nls --enable-languages=c --without-headers
make all-gcc
make all-target-libgcc
make install-gcc
make install-target-libgcc
cd ..

wget 'https://github.com/managarm/mlibc/raw/1f84a68500a2939f8dccd4083f8c111b2610cba6/options/elf/include/elf.h' -O "$PREFIX/lib/gcc/x86_64-elf/$GCCVERSION/include/elf.h"
