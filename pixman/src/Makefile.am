lib_LTLIBRARIES = libpixregion.la
include_HEADERS = pixregion.h

libpixregion_la_SOURCES = \
	pixregion.c \
	pixregion.h \
	pixregionint.h

libpixregion_la_LDFLAGS = -version-info @VERSION_INFO@

INCLUDES = $(LIBPIXREGION_CFLAGS)

libpixregion_la_LIBADD = $(LIBPIXREGION_LIBS)
