#! /bin/sh

set -ex

origdir="$(pwd -P)"

srcdir="$(dirname "$0")"
test -z "$srcdir" && srcdir=.

cd "$srcdir"

[ -d stivale ] || git clone https://github.com/stivale/stivale.git
[ -d reduced-gnu-efi ] || git clone https://github.com/limine-bootloader/reduced-gnu-efi.git

automake --add-missing || true
autoconf

cd "$origdir"

if test -z "$NOCONFIGURE"; then
    exec "$srcdir"/configure "$@"
fi
