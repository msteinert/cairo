#!/bin/sh

LANG=C

test -z "$srcdir" && srcdir=.
stat=0

echo Checking public headers for missing cairo_public decorators

find "$srcdir" -name '*.h' ! -name '*-private.h' ! -name '*-test.h' ! -name 'cairoint.h' ! -name 'cairo-no-features.h' |
xargs grep -B 1 '^cairo_.*[ 	]\+(' |
awk '
/^--$/ { context=""; public=0; next; }
/:cairo_.*[ 	]+\(/ { if (!public) {print context; print; print "--";} next; }
/-cairo_public.*[ 	]/ {public=1;}
{ context=$0; }
' |
sed 's/[.]h-/.h:/' |
grep . && stat=1

exit $stat
