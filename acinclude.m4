dnl -*- mode: autoconf -*-

dnl [m4_newline] didn't appear until autoconf 2.62
m4_ifdef([m4_newline],,m4_define([m4_newline],[
]))

dnl ==========================================================================

dnl This has to be in acinclude.m4 as it includes other files

dnl Parse Version.mk and declare m4 variables out of it
m4_define([CAIRO_PARSE_VERSION],dnl
		m4_translit(dnl
		m4_bpatsubst(m4_include(cairo-version.h),
			     [^.define \([a-zA-Z0-9_]*\)  *\([0-9][0-9]*\)],
			     [[m4_define(\1, \[\2\])]]),
			    [A-Z], [a-z])dnl
)dnl

