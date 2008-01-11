#!/bin/sh

LANG=C

if ! which readelf 2>/dev/null >/dev/null; then
	echo "'readelf' not found; skipping test"
	exit 0
fi

test -z "$srcdir" && srcdir=.
status=0

has_hidden_symbols=`cat .check-has-hidden-symbols`
if test "x$has_hidden_symbols" != "x1"; then
	echo "Compiler doesn't support symbol visibility; skipping test"
	exit 0
fi

for so in .libs/lib*.so; do
	echo Checking "$so" for local PLT entries
	readelf -W -r "$so" | grep 'JU\?MP_SLO' | grep 'cairo' && status=1
done

exit $status
