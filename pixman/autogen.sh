#!/bin/sh
# Run this to generate all the initial makefiles, etc.

set -e

ARGV0=$0

if test -z "$*"; then
  echo "$ARGV0:	Note: \`./configure' will be run with no arguments."
  echo "		If you wish to pass any to it, please specify them on the"
  echo "		\`$0' command line."
  echo
fi

do_cmd() {
    echo "$ARGV0: running \`$@'"
    $@
}

do_cmd libtoolize --force --copy

do_cmd aclocal

do_cmd autoheader

do_cmd automake --add-missing

do_cmd autoconf

do_cmd ./configure ${1+"$@"} && echo "Now type \`make' to compile" || exit 1
