#!/bin/sh

LANG=C

test -z "$srcdir" && srcdir=.
cd "$srcdir"
stat=0


echo 'Checking that public header files #include "cairo.h" first (or none)'

FILES=$all_cairo_headers
test "x$FILES" = x && FILES=`find . -name 'cairo*.h' ! -name 'cairo*-private.h' ! -name 'cairoint.h'`

for x in $FILES; do
	grep '\<include\>' "$x" /dev/null | head -n 1
done |
grep -v '"cairo[.]h"' |
grep -v 'cairo[.]h:' |
grep . && stat=1


echo 'Checking that private header files #include "some cairo header" first (or none)'

FILES=$all_cairo_private
test "x$FILES" = x && FILES=`find . -name 'cairo*-private.h'`

for x in $FILES; do
	grep '\<include\>' "$x" /dev/null | head -n 1
done |
grep -v '"cairo.*[.]h"' |
grep -v 'cairoint[.]h:' |
grep . && stat=1


echo 'Checking that source files #include "cairoint.h" first (or none)'

FILES=$all_cairo_sources
test "x$FILES" = x && FILES=`find . -name 'cairo*.c' -or -name 'cairo*.cpp'`

for x in $FILES; do
	grep '\<include\>' "$x" /dev/null | head -n 1
done |
grep -v '"cairoint[.]h"' |
grep . && stat=1


exit $stat
