#!/bin/sh

set -e

TMP1=$(mktemp)
TMP2=$(mktemp)
TMP3=$(mktemp)
TMP4=$(mktemp)

$1 -t "$2" | sed '/\bd\b/d' | sort > "$TMP1"
grep "\.text" < "$TMP1" | cut -d' ' -f1 > "$TMP2"
grep "\.text" < "$TMP1" | awk 'NF{ print $NF }' > "$TMP3"

echo "section .map" > "$TMP4"
echo "global $3_map" >> "$TMP4"
echo "$3_map:" >> "$TMP4"

paste -d'$' "$TMP2" "$TMP3" | sed 's/^/dq 0x/g' | sed 's/$/", 0/g' | sed 's/\$/\ndb "/g' >> "$TMP4"

echo "dq 0xffffffffffffffff" >> "$TMP4"

nasm -f elf64 "$TMP4" -o $3.map.o

rm "$TMP1" "$TMP2" "$TMP3" "$TMP4"
