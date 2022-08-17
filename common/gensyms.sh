#! /bin/sh

set -e

LC_ALL=C
export LC_ALL

TMP0="$(mktemp)"

cat >"$TMP0" <<EOF
#! /bin/sh

set -e

set -o pipefail 2>/dev/null
EOF

chmod +x "$TMP0"

"$TMP0" && set -o pipefail

rm "$TMP0"

TMP1="$(mktemp)"
TMP2="$(mktemp)"
TMP3="$(mktemp)"
TMP4="$(mktemp)"

trap "rm -f '$TMP1' '$TMP2' '$TMP3' '$TMP4'; trap - EXIT; exit" EXIT INT TERM QUIT HUP

"$CROSS_OBJDUMP" -t "$1" | ( "$SED" '/[[:<:]]d[[:>:]]/d' 2>/dev/null || "$SED" '/\bd\b/d' ) | sort > "$TMP1"
"$GREP" "$4" < "$TMP1" | cut -d' ' -f1 > "$TMP2"
"$GREP" "$4" < "$TMP1" | "$AWK" 'NF{ print $NF }' > "$TMP3"

echo ".section .$2_map" > "$TMP4"
echo ".globl $2_map" >> "$TMP4"
echo "$2_map:" >> "$TMP4"

if [ "$3" = "32" ]; then
    paste -d'#' "$TMP2" "$TMP3" | "$SED" 's/^/.long 0x/g;s/$/"/g;s/\#/\
.asciz "/g' >> "$TMP4"
    echo ".long 0xffffffff" >> "$TMP4"
elif [ "$3" = "64" ]; then
    paste -d'#' "$TMP2" "$TMP3" | "$SED" 's/^/.quad 0x/g;s/$/"/g;s/\#/\
.asciz "/g' >> "$TMP4"
    echo ".quad 0xffffffffffffffff" >> "$TMP4"
fi

mv "$TMP4" "$2.map.S"
