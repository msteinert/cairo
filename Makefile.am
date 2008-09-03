DIST_SUBDIRS = src boilerplate test perf doc util
SUBDIRS = src doc
# libpng is required for our test programs
if CAIRO_HAS_PNG_FUNCTIONS
SUBDIRS += boilerplate test perf
endif

configure: cairo-version.h

doc:
	cd doc && $(MAKE) $(AM_MAKEFLAGS) $@
test retest recheck: all
	cd test && $(MAKE) $(AM_MAKEFLAGS) $@
perf: all
	cd perf && $(MAKE) $(AM_MAKEFLAGS) $@
check-valgrind: all
	cd test && $(MAKE) $(AM_MAKEFLAGS) check-valgrind
	cd perf && $(MAKE) $(AM_MAKEFLAGS) check-valgrind
.PHONY: doc test retest recheck perf check-valgrind


EXTRA_DIST = \
	AUTHORS \
	BIBLIOGRAPHY \
	BUGS \
	CODING_STYLE \
	COPYING \
	COPYING-LGPL-2.1 \
	COPYING-MPL-1.1 \
	HACKING \
	INSTALL \
	Makefile.win32 \
	NEWS \
	PORTING_GUIDE \
	README \
	RELEASING \
	autogen.sh \
	cairo-version.h \
	$(NULL)

DISTCLEANFILES = config.cache
MAINTAINERCLEANFILES = \
	$(srcdir)/aclocal.m4 \
	$(srcdir)/autoscan.log \
	$(srcdir)/compile \
	$(srcdir)/config.guess \
	$(srcdir)/config.h.in \
	$(srcdir)/config.sub \
	$(srcdir)/configure.scan \
	$(srcdir)/depcomp \
	$(srcdir)/install-sh \
	$(srcdir)/ltmain.sh \
	$(srcdir)/missing \
	$(srcdir)/mkinstalldirs \
	`find "$(srcdir)" -type f -name Makefile.in -print`

DISTCHECK_CONFIGURE_FLAGS = \
	--enable-gtk-doc \
	--enable-test-surfaces

include $(srcdir)/ChangeLog.mk
include $(srcdir)/Releasing.mk
include $(srcdir)/Analysis.mk
