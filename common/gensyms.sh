#! /bin/sh

set -e

LC_ALL=C
export LC_ALL

TMP0=$(mktemp)

cat >"$TMP0" <<EOF
#! /bin/sh

set -e

set -o pipefail 2>/dev/null
EOF

chmod +x "$TMP0"

"$TMP0" && set -o pipefail

rm "$TMP0"

TMP1=$(mktemp)
TMP2=$(mktemp)
TMP3=$(mktemp)
TMP4=$(mktemp)

"$LIMINE_OBJDUMP" -t "$1" | ( "$SED" '/[[:<:]]d[[:>:]]/d' 2>/dev/null || "$SED" '/\bd\b/d' ) | sort > "$TMP1"
"$GREP" "$4" < "$TMP1" | cut -d' ' -f1 > "$TMP2"
"$GREP" "$4" < "$TMP1" | "$AWK" 'NF{ print $NF }' > "$TMP3"

echo "section .$2_map" > "$TMP4"
echo "global $2_map" >> "$TMP4"
echo "$2_map:" >> "$TMP4"

if [ "$3" = "32" ]; then
    paste -d'$' "$TMP2" "$TMP3" | "$SED" 's/^/dd 0x/g;s/$/", 0/g;s/\$/\
db "/g' >> "$TMP4"
    echo "dd 0xffffffff" >> "$TMP4"
    nasm -f elf32 "$TMP4" -o $2.map.o
elif [ "$3" = "64" ]; then
    paste -d'$' "$TMP2" "$TMP3" | "$SED" 's/^/dq 0x/g;s/$/", 0/g;s/\$/\
db "/g' >> "$TMP4"
    echo "dq 0xffffffffffffffff" >> "$TMP4"
    nasm -f elf64 "$TMP4" -o $2.map.o
fi

rm "$TMP1" "$TMP2" "$TMP3" "$TMP4"
