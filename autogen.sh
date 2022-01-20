#! /bin/sh

set -ex

srcdir="$(realpath $(dirname "$0"))"
test -z "$srcdir" && srcdir=.

cd "$srcdir"

[ -d stivale ] || git clone https://github.com/stivale/stivale.git
[ -d reduced-gnu-efi ] || git clone https://github.com/limine-bootloader/reduced-gnu-efi.git

autoconf

if test -z "$NOCONFIGURE"; then
    exec "$srcdir"/configure "$@"
fi
