#! /bin/sh

set -ex

srcdir="$(realpath $(dirname "$0"))"
test -z "$srcdir" && srcdir=.

cd "$srcdir"

TARGET=x86_64-elf
BINUTILSVERSION=2.37
GCCVERSION=11.2.0

if command -v gmake; then
    export MAKE=gmake
else
    export MAKE=make
fi

export CFLAGS="-O2 -pipe"

unset CC
unset CXX

if [ "$(uname)" = "OpenBSD" ]; then
    # OpenBSD has an awfully ancient GCC which fails to build our toolchain.
    # Force clang/clang++.
    export CC="clang"
    export CXX="clang++"
fi

mkdir -p toolchain && cd toolchain
PREFIX="$(pwd)"

export MAKEFLAGS="-j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 1)"

export PATH="$PREFIX/bin:$PATH"

if [ ! -f binutils-$BINUTILSVERSION.tar.gz ]; then
    curl -o binutils-$BINUTILSVERSION.tar.gz https://ftp.gnu.org/gnu/binutils/binutils-$BINUTILSVERSION.tar.gz
fi
if [ ! -f gcc-$GCCVERSION.tar.gz ]; then
    curl -o gcc-$GCCVERSION.tar.gz https://ftp.gnu.org/gnu/gcc/gcc-$GCCVERSION/gcc-$GCCVERSION.tar.gz
fi

rm -rf build
mkdir build
cd build

tar -zxf ../binutils-$BINUTILSVERSION.tar.gz
tar -zxf ../gcc-$GCCVERSION.tar.gz

mkdir build-binutils
cd build-binutils
../binutils-$BINUTILSVERSION/configure CFLAGS="$CFLAGS" CXXFLAGS="$CFLAGS"  --target=$TARGET --prefix="$PREFIX" --program-prefix=limine- --with-sysroot --disable-nls --disable-werror --enable-targets=x86_64-elf,x86_64-pe
$MAKE
$MAKE install
cd ..

cd gcc-$GCCVERSION
contrib/download_prerequisites
cd ..
mkdir build-gcc
cd build-gcc
../gcc-$GCCVERSION/configure CFLAGS="$CFLAGS" CXXFLAGS="$CFLAGS" --target=$TARGET --prefix="$PREFIX" --program-prefix=limine- --disable-nls --enable-languages=c --without-headers
$MAKE all-gcc
$MAKE all-target-libgcc
$MAKE install-gcc
$MAKE install-target-libgcc
cd ..
