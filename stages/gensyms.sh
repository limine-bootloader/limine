#!/bin/sh

set -e

TMP1=$(mktemp)
TMP2=$(mktemp)
TMP3=$(mktemp)

$1 -t stage2.elf | sed '/\bd\b/d' | sort > "$TMP1"
grep "\.text" < "$TMP1" | cut -d' ' -f1 > "$TMP2"
grep "\.text" < "$TMP1" | awk 'NF{ print $NF }' > "$TMP3"

paste -d'$' "$TMP2" "$TMP3" | sed 's/^/dd 0x/g' | sed 's/$/", 0/g' | sed 's/\$/\ndb "/g' > symlist.gen

echo "dd 0xffffffff" >> symlist.gen

rm "$TMP1" "$TMP2" "$TMP3"
