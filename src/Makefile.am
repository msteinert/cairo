include $(srcdir)/Config.mk

EXTRA_DIST =
CLEANFILES =
DISTCLEANFILES =
MAINTAINERCLEANFILES =
BUILT_SOURCES =

INCLUDES = -I$(srcdir) $(CAIRO_CFLAGS)

if OS_WIN32
export_symbols = -export-symbols cairo.def
cairo_def_dependency = cairo.def
endif

EXTRA_DIST           += cairo-features-win32.h Config.mk.win32
MAINTAINERCLEANFILES += cairo-features-win32.h Config.mk.win32

$(top_builddir)/config.h: $(top_srcdir)/config.h.in
	cd $(top_builddir) && $(MAKE) $(AM_MAKEFLAGS) config.h

all_cairo_files = \
	$(cairo_all_headers) \
	$(cairo_all_private) \
	$(cairo_all_sources) \
	$(NULL)

cairoincludedir = $(includedir)/cairo
cairoinclude_HEADERS = $(enabled_cairo_headers)

lib_LTLIBRARIES = libcairo.la

libcairo_la_SOURCES = \
	$(enabled_cairo_headers) \
	$(enabled_cairo_private) \
	$(enabled_cairo_sources) \
	$(NULL)
libcairo_la_LDFLAGS = -version-info $(CAIRO_LIBTOOL_VERSION_INFO) -no-undefined $(export_symbols)
libcairo_la_LIBADD = $(CAIRO_LIBS) $(CAIRO_LDADD)
libcairo_la_DEPENDENCIES = $(cairo_def_dependency)

# Special headers
cairoinclude_HEADERS += $(top_srcdir)/cairo-version.h
nodist_cairoinclude_HEADERS = cairo-features.h
libcairo_la_SOURCES += cairo-version.h cairo-features.h
BUILT_SOURCES  += cairo-features.h cairo-supported-features.h
DISTCLEANFILES += cairo-features.h cairo-supported-features.h
cairo-features.h cairo-supported-features.h:
	cd $(top_builddir) && ./config.status src/$@

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = $(enabled_cairo_pkgconf)

CLEANFILES += cairo.def
cairo.def: cairo-features.h $(enabled_cairo_headers)
	@echo Generating $@
	@(echo EXPORTS; \
	(cd $(srcdir); cat $(enabled_cairo_headers) || echo 'cairo_ERROR ()' ) | \
	grep -v -E '^# *include' | \
	( cat cairo-features.h - | $(CPP) - || echo 'cairo_ERROR ()' ) | \
	grep -E '^cairo_.* \(' | \
	sed -e 's/[ 	].*//' | \
	sort; \
	echo LIBRARY libcairo-$(CAIRO_VERSION_SONUM).dll; \
	) >$@
	@ ! grep -q cairo_ERROR $@ || ($(RM) $@; false)

TESTS_ENVIRONMENT = \
	srcdir="$(srcdir)" \
	MAKE="$(MAKE) $(AM_MAKEFLAGS)" \
	all_cairo_files="$(all_cairo_files)" \
	enabled_cairo_files="$(enabled_cairo_files)" \
	$(NULL)
TESTS =
TESTS_SH = \
	check-def.sh \
	check-plt.sh \
	check-headers.sh \
	check-cairoint.sh \
	check-doc-syntax.sh\
	$(NULL)
TESTS += $(TESTS_SH)
if CROSS_COMPILING
check: check-link
else
TESTS += check-link
endif

EXTRA_DIST += $(TESTS_SH) check-has-hidden-symbols.c
EXTRA_PROGRAMS = check-link
check_link_LDADD = libcairo.la
CLEANFILES += check-link


# The pre-processed result is used by check-{def,plt}.sh to determine whether
# cairo has been compiled with symbol hiding.
.c.i: $(cairoinclude_HEADERS) $(nodist_cairoinclude_HEADERS) cairoint.h $(top_builddir)/config.h
	$(CPP) -DHAVE_CONFIG_H -I$(top_builddir) -I. $(INCLUDES) $< -o $@
CLEANFILES += *.i *.s

SPARSE = sparse
sparse:
	@status=true; for f in $(enabled_cairo_sources); do \
		echo sparse $$f; \
		$(SPARSE) -I$(top_builddir) $(INCLUDES) -DHAVE_CONFIG_H $$f || status=false; \
	done; $$status

SPLINT = splint -badflag
splint:
	@status=true; for f in $(enabled_cairo_sources); do \
		test "$$f" = "`echo "$$f" | sed 's/[.]h$$//'`" || continue; \
		echo sparse $$f; \
		$(SPLINT) -I$(top_builddir) $(INCLUDES) -DHAVE_CONFIG_H $$f || status=false; \
	done; $$status

UNO = uno
uno:
	@cpp_flags=`echo $(INCLUDES) | sed 's/\(-I.*\) /\1 /g'`; \
	files=`echo $(enabled_cairo_sources) | sed 's/[^ ]*\.h//g'`; \
	$(UNO) -I$(top_builddir) $$cpp_flags -DHAVE_CONFIG_H -U__GNUC__ $$files

EXTRA_DIST += Makefile.win32
