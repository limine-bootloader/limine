#!/bin/sh

cat version 2>/dev/null || ( git describe --exact-match --tags `git log -n1 --pretty='%h'` 2>/dev/null || git log -n1 --pretty='%h' )
