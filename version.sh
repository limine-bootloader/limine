#! /bin/sh

[ -f version ] || ( git describe --exact-match --tags $(git log -n1 --pretty='%h') 2>/dev/null || git log -n1 --pretty='%h' ) | xargs printf '%s'
[ -f version ] && ( cat version 2>/dev/null ) | xargs printf '%s'
