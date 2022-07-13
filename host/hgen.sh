#! /bin/sh

set -e

LC_ALL=C
export LC_ALL

cat <<EOF
const uint8_t binary_limine_hdd_bin_data[] = {
EOF

od -v -An -t x1 <limine-hdd.bin | "$SED" '/^$/d;s/  */ /g;s/ *$//g;s/ /, 0x/g;s/^, /    /g;s/$/,/g'

cat <<EOF
};
EOF
