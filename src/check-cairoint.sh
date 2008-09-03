#!/bin/sh

LANG=C

test -z "$srcdir" && srcdir=.
stat=0

echo 'Checking source files for missing or misplaced #include "cairoint.h"'

find "$srcdir" \( -name '*.c' -or -name '*.cpp' \) -and ! -name 'check-*.c' |
while read x; do
	grep '\<include\>' "$x" /dev/null | head -n 1
done |
grep -v '"cairoint.h"' |
grep . && stat=1

exit $stat
