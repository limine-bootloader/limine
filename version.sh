#! /bin/sh

LC_ALL=C
export LC_ALL

git describe --exact-match --tags $(git log -n1 --pretty='%h') 2>/dev/null || git log -n1 --pretty='%h' | sed 's/^v//g' | xargs printf '%s'
