#!/bin/sh
# Run this to generate all the initial makefiles, etc.

test -n "$srcdir" || srcdir=`dirname "$0"`
test -n "$srcdir" || srcdir=.

ORIGDIR=`pwd`
cd $srcdir

AUTORECONF=`which autoreconf`
if test -z $AUTORECONF; then
        echo "*** No autoreconf found, please intall it ***"
        exit 1
fi

GTKDOCIZE=`which gtkdocize`
if test -z $GTKDOCIZE; then
        echo "*** No GTK-Doc found, please install it ***"
        exit 1
fi

# create dummy */Makefile.am.features and ChangeLog to make automake happy
> boilerplate/Makefile.am.features
> src/Makefile.am.features
touch ChangeLog

gtkdocize || exit $?
autoreconf --force --install --verbose || exit $?

cd $ORIGDIR
test -n "$NOCONFIGURE" || "$srcdir/configure" "$@"
