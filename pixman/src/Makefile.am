lib_LTLIBRARIES = libpixman.la
include_HEADERS = pixman.h

libpixman_la_SOURCES = \
	pixman.h \
	pixregion.c \
	pixregionint.h \
	ic.c \
	icblt.c \
	icbltone.c \
	iccolor.c \
	iccompose.c \
	icformat.c \
	icimage.c \
	icimage.h \
	icint.h \
	icpixels.c \
	icrect.c \
	icrop.h \
	icstipple.c \
	ictrap.c \
	ictransform.c \
	ictri.c \
	icutil.c \
	slim_internal.h

libpixman_la_LDFLAGS = -version-info @VERSION_INFO@ -no-undefined

INCLUDES = -I$(srcdir)
