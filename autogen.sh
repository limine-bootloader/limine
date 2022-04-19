#! /bin/sh

set -ex

origdir="$(pwd -P)"

srcdir="$(dirname "$0")"
test -z "$srcdir" && srcdir=.

cd "$srcdir"

[ -d freestanding_headers ] || git clone https://github.com/mintsuki/freestanding_headers.git
[ -d stivale ] || git clone https://github.com/stivale/stivale.git
[ -d reduced-gnu-efi ] || ( git clone https://github.com/limine-bootloader/reduced-gnu-efi.git && cd reduced-gnu-efi && git checkout 456a1a82d2be9e5fd758ae819c3077140ac5aa99 )

automake_libdir="$(automake --print-libdir)"

mkdir -p build-aux
cp "${automake_libdir}/install-sh" build-aux

autoconf

cd "$origdir"

if test -z "$NOCONFIGURE"; then
    exec "$srcdir"/configure "$@"
fi
