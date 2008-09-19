#!/bin/sh

LANG=C

test -z "$srcdir" && srcdir=.
stat=0

echo 'Checking source files for missing or misplaced #include "cairoint.h"'

cd "$srcdir"
FILES=$all_cairo_sources
if test "x$FILES" = x; then
	FILES=`find . -name 'cairo*.c' -or -name 'cairo*.cpp'`
fi

for x in $FILES; do
	grep '\<include\>' "$x" /dev/null | head -n 1
done |
grep -v '"cairoint[.]h"' |
grep . && stat=1

exit $stat
