DIST_SUBDIRS = src boilerplate test perf doc util
SUBDIRS = src doc
# libpng is required for our test programs
if CAIRO_HAS_PNG_FUNCTIONS
SUBDIRS += boilerplate test perf
endif

configure: cairo-version.h

.PHONY: doc test retest recheck check-valgrind
# We have some generated header files, so we really need to build the
# library before we can build the docs
doc: all
	cd doc && $(MAKE) $(AM_MAKEFLAGS) doc
test: all
	cd test && $(MAKE) $(AM_MAKEFLAGS) test
retest: all
	cd test && $(MAKE) $(AM_MAKEFLAGS) retest
recheck: all
	cd test && $(MAKE) $(AM_MAKEFLAGS) recheck
check-valgrind: all
	cd test && $(MAKE) $(AM_MAKEFLAGS) check-valgrind
	cd perf && $(MAKE) $(AM_MAKEFLAGS) check-valgrind
perf: all
	cd perf && $(MAKE) $(AM_MAKEFLAGS) perf

if CAIRO_HAS_LCOV
# use recursive makes in order to ignore errors during check/perf
lcov:
	-$(MAKE) $(AM_MAKEFLAGS) check
	$(MAKE) $(AM_MAKEFLAGS) genlcov
lcov-perf:
	-$(MAKE) $(AM_MAKEFLAGS) perf
	$(MAKE) $(AM_MAKEFLAGS) genlcov

# we have to massage the lcov.info file slightly to hide the effect of libtool
# placing the objects files in the .libs/ directory separate from the *.c
genlcov:
	$(LTP) --directory $(top_builddir) --path $(top_builddir) --capture --output-file cairo-lcov.info --test-name CAIRO_TEST --no-checksum
	$(SED) -e 's#.libs/##' \
	       -e 's#boilerplate/src#src#' \
	       -e 's#$(shell pwd)#$(shell cd $(top_srcdir) && pwd)#' \
	       < cairo-lcov.info > cairo-lcov.info.tmp
	LANG=C $(LTP_GENHTML) --prefix $(top_builddir) --output-directory cairo-lcov --title "Cairo Code Coverage" --show-details cairo-lcov.info.tmp
	$(RM) cairo-lcov.info.tmp
else
lcov lcov-perf genlcov:
	@echo You need to configure Cairo with support for gcov enabled.
	@echo e.g, ./configure --enable-gcov
endif

lcov-clean:
if CAIRO_HAS_LCOV
	-$(LTP) --directory $(top_builddir) -z
endif
	-$(RM) -r cairo-lcov.info cairo-lcov
	-$(FIND) -name '*.gcda' -print | $(XARGS) $(RM)

.PHONY: lcov lcov-perf lcov-clean genlcov


EXTRA_DIST = \
	autogen.sh \
	cairo-version.h \
	AUTHORS \
	BIBLIOGRAPHY \
	BUGS \
	CODING_STYLE \
	COPYING \
	COPYING-LGPL-2.1 \
	COPYING-MPL-1.1 \
	HACKING \
	INSTALL \
	NEWS \
	PORTING_GUIDE \
	README \
	RELEASING
DISTCLEANFILES = config.cache
distclean-local: lcov-clean

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

DISTCHECK_CONFIGURE_FLAGS = --enable-gtk-doc --enable-test-surfaces

include $(srcdir)/ChangeLog.mk
include $(srcdir)/Releasing.mk

EXTRA_DIST += Makefile.win32
