#!/bin/sh

LANG=C

if ! grep --version 2>/dev/null | grep GNU >/dev/null; then
	echo "GNU grep not found; skipping test"
	exit 0
fi

test -z "$srcdir" && srcdir=.
status=0

echo Checking documentation for incorrect syntax

# Note: this test is also run from doc/public/ to check the SGML files

if test "x$SGML_DOCS" = x; then
    FILES=$cairo_all_source_files
    if test "x$FILES" = x; then
        FILES=`find "$srcdir" -name '*.h' -or -name '*.c' -or -name '*.cpp'`
    fi
fi

enum_regexp='\([^%@]\|^\)\<\(FALSE\|TRUE\|NULL\|CAIRO_[0-9A-Z_]*\)\($\|[^(A-Za-z0-9_]\)'
if test "x$SGML_DOCS" = x; then
	enum_regexp='^[^:]*:[/ ][*] .*'$enum_regexp
fi
if grep . /dev/null $FILES | sed -e '/<programlisting>/,/<\/programlisting>/d' | grep "$enum_regexp" | grep -v '#####'; then
	status=1
	echo Error: some macros in the docs are not prefixed by percent sign.
	echo Fix this by searching for the following regexp in the above files:
	echo "	'$enum_regexp'"
fi

type_regexp='\( .*[^#]\| \|^\)\<cairo[0-9a-z_]*_t\>\($\|[^:]$\|[^:].\)'
if test "x$SGML_DOCS" = x; then
	type_regexp='^[^:]*:[/ ][*]'$type_regexp
else
	type_regexp='\(.'$type_regexp'\)\|\('$type_regexp'.\)'
fi

if grep . /dev/null $FILES | sed -e '/<programlisting>/,/<\/programlisting>/d' | grep "$type_regexp" | grep -v '#####'; then
	status=1
	echo Error: some type names in the docs are not prefixed by hash sign,
	echo neither are the only token in the doc line followed by colon.
	echo Fix this by searching for the following regexp in the above files:
	echo "	'$type_regexp'"
fi

func_regexp='\([^#]\|^\)\<\(cairo_[][<>/0-9a-z_]*\> \?[^][ <>(]\)'
if test "x$SGML_DOCS" = x; then
	func_regexp='^[^:]*:[/ ][*] .*'$func_regexp
fi

# We need to filter out gtk-doc markup errors for program listings.
if grep . /dev/null $FILES | sed -e '/<programlisting>/,/<\/programlisting>/d' | grep "$func_regexp" | grep -v '#####'; then
	status=1
	echo Error: some function names in the docs are not followed by parentheses.
	echo Fix this by searching for the following regexp in the above files:
	echo "	'$func_regexp'"
fi

note_regexp='NOTE'
if grep "$note_regexp" $FILES; then
	status=1
	echo Error: some source files contain the string 'NOTE'.
	echo Be civil and replace it by 'Note' please.
fi

exit $status
