#! /bin/sh

set -ex

srcdir="$(dirname "$0")"
test -z "$srcdir" && srcdir=.

cd "$srcdir"

TARGET=x86_64-elf
BINUTILSVERSION=2.38
GCCVERSION=11.2.0
NASMVERSION=2.15.05

if command -v gmake; then
    export MAKE=gmake
else
    export MAKE=make
fi

if command -v gtar; then
    export TAR=gtar
else
    export TAR=tar
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
PREFIX="$(pwd -P)"

export MAKEFLAGS="-j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || psrinfo -tc 2>/dev/null || echo 1)"

export PATH="$PREFIX/bin:$PATH"

if [ ! -f binutils-$BINUTILSVERSION.tar.gz ]; then
    curl -o binutils-$BINUTILSVERSION.tar.gz https://ftp.gnu.org/gnu/binutils/binutils-$BINUTILSVERSION.tar.gz
fi
if [ ! -f gcc-$GCCVERSION.tar.gz ]; then
    curl -o gcc-$GCCVERSION.tar.gz https://ftp.gnu.org/gnu/gcc/gcc-$GCCVERSION/gcc-$GCCVERSION.tar.gz
fi
if [ ! -f nasm-$NASMVERSION.tar.gz ]; then
    curl -o nasm-$NASMVERSION.tar.gz https://www.nasm.us/pub/nasm/releasebuilds/$NASMVERSION/nasm-$NASMVERSION.tar.gz
fi

rm -rf build
mkdir build
cd build

$TAR -zxf ../binutils-$BINUTILSVERSION.tar.gz
$TAR -zxf ../gcc-$GCCVERSION.tar.gz
$TAR -zxf ../nasm-$NASMVERSION.tar.gz

mkdir build-binutils
cd build-binutils
../binutils-$BINUTILSVERSION/configure CFLAGS="$CFLAGS" CXXFLAGS="$CFLAGS"  --target=$TARGET --prefix="$PREFIX" --program-prefix=limine- --with-sysroot --disable-nls --disable-werror --enable-targets=x86_64-elf,x86_64-pe
$MAKE
$MAKE install
cd ..

cd gcc-$GCCVERSION
sed 's|http://gcc.gnu|https://gcc.gnu|g' < contrib/download_prerequisites > dp.sed
mv dp.sed contrib/download_prerequisites
chmod +x contrib/download_prerequisites
./contrib/download_prerequisites --no-verify
cd ..
mkdir build-gcc
cd build-gcc
../gcc-$GCCVERSION/configure CFLAGS="$CFLAGS" CXXFLAGS="$CFLAGS" --target=$TARGET --prefix="$PREFIX" --program-prefix=limine- --disable-nls --enable-languages=c --without-headers
$MAKE all-gcc
$MAKE all-target-libgcc
$MAKE install-gcc
$MAKE install-target-libgcc
cd ..

mkdir build-nasm
cd build-nasm
../nasm-$NASMVERSION/configure --prefix="$PREFIX"
MAKEFLAGS="" $MAKE
MAKEFLAGS="" $MAKE install
cd ..
