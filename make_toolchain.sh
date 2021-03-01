#!/bin/sh

set -e
set -x

TARGET=i386-elf
BINUTILSVERSION=2.36.1
GCCVERSION=11-20210228
NASMVERSION=2.15.05
GZIPVERSION=1.10

mkdir -p "$1" && cd "$1"
PREFIX="$(pwd)"

export MAKEFLAGS="$2"

export PATH="$PREFIX/bin:$PATH"

if [ ! -f binutils-$BINUTILSVERSION.tar.gz ]; then
    wget https://ftp.gnu.org/gnu/binutils/binutils-$BINUTILSVERSION.tar.gz
fi
if [ ! -f gcc-$GCCVERSION.tar.xz ]; then
    wget https://ftp.nluug.nl/languages/gcc/snapshots/$GCCVERSION/gcc-$GCCVERSION.tar.xz
fi
if [ ! -f nasm-$NASMVERSION.tar.gz ]; then
    wget https://www.nasm.us/pub/nasm/releasebuilds/$NASMVERSION/nasm-$NASMVERSION.tar.gz
fi
if [ ! -f gzip-$GZIPVERSION.tar.gz ]; then
    wget https://ftp.gnu.org/gnu/gzip/gzip-$GZIPVERSION.tar.gz
fi

rm -rf build
mkdir build
cd build

tar -xf ../binutils-$BINUTILSVERSION.tar.gz
tar -xf ../gcc-$GCCVERSION.tar.xz
tar -xf ../nasm-$NASMVERSION.tar.gz
tar -xf ../gzip-$GZIPVERSION.tar.gz

mkdir build-binutils
cd build-binutils
../binutils-$BINUTILSVERSION/configure --target=$TARGET --prefix="$PREFIX" --with-sysroot --disable-nls --disable-werror --enable-64-bit-bfd
make
make install
cd ..

cd gcc-$GCCVERSION
contrib/download_prerequisites
cd ..
mkdir build-gcc
cd build-gcc
../gcc-$GCCVERSION/configure --target=$TARGET --prefix="$PREFIX" --disable-nls --enable-languages=c --without-headers
make all-gcc
make all-target-libgcc
make install-gcc
make install-target-libgcc
cd ..

mkdir build-nasm
cd build-nasm
../nasm-$NASMVERSION/configure --prefix="$PREFIX"
make
make install
cd ..

mkdir build-gzip
cd build-gzip
../gzip-$GZIPVERSION/configure --prefix="$PREFIX"
make
make install
cd ..
