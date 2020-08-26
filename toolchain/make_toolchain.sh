#!/usr/bin/env bash

set -e
set -x

PREFIX="$(pwd)"
TARGET=i386-elf
BINUTILSVERSION=2.35
GCCVERSION=10.2.0

if [ -z "$MAKEFLAGS" ]; then
	MAKEFLAGS="$1"
fi
export MAKEFLAGS

export PATH="$PREFIX/bin:$PATH"

if [ -x "$(command -v gmake)" ]; then
    mkdir -p "$PREFIX/bin"
    cat <<EOF >"$PREFIX/bin/make"
#!/usr/bin/env sh
gmake "\$@"
EOF
    chmod +x "$PREFIX/bin/make"
fi

mkdir -p build
cd build

if [ ! -f binutils-$BINUTILSVERSION.tar.gz ]; then
    wget -4 https://ftp.gnu.org/gnu/binutils/binutils-$BINUTILSVERSION.tar.gz # Force IPv4 otherwise wget hangs
fi
if [ ! -f gcc-$GCCVERSION.tar.gz ]; then
    wget -4 https://ftp.gnu.org/gnu/gcc/gcc-$GCCVERSION/gcc-$GCCVERSION.tar.gz # Same as above
fi

tar -xf binutils-$BINUTILSVERSION.tar.gz
tar -xf gcc-$GCCVERSION.tar.gz

rm -rf build-gcc build-binutils

mkdir build-binutils
cd build-binutils
../binutils-$BINUTILSVERSION/configure --target=$TARGET --prefix="$PREFIX" --with-sysroot --disable-nls --disable-werror
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
