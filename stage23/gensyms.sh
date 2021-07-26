#!/bin/sh

set -e

./test_pipefail.sh && set -o pipefail

TMP1=$(mktemp)
TMP2=$(mktemp)
TMP3=$(mktemp)
TMP4=$(mktemp)

$1 -t "$2" | ( sed '/[[:<:]]d[[:>:]]/d' 2>/dev/null || sed '/\bd\b/d' ) | sort > "$TMP1"
grep "\.text" < "$TMP1" | cut -d' ' -f1 > "$TMP2"
grep "\.text" < "$TMP1" | awk 'NF{ print $NF }' > "$TMP3"

echo "section .$3_map" > "$TMP4"
echo "global $3_map" >> "$TMP4"
echo "$3_map:" >> "$TMP4"

if [ "$4" = "32" ]; then
    paste -d'$' "$TMP2" "$TMP3" | sed 's/^/dd 0x/g;s/$/", 0/g;s/\$/\
db "/g' >> "$TMP4"
    echo "dd 0xffffffff" >> "$TMP4"
    nasm -f elf32 "$TMP4" -o $3.map.o
elif [ "$4" = "64" ]; then
    paste -d'$' "$TMP2" "$TMP3" | sed 's/^/dq 0x/g;s/$/", 0/g;s/\$/\
db "/g' >> "$TMP4"
    echo "dq 0xffffffffffffffff" >> "$TMP4"
    nasm -f elf64 "$TMP4" -o $3.map.o
fi

rm "$TMP1" "$TMP2" "$TMP3" "$TMP4"
