lib_LTLIBRARIES = libXr.la
include_HEADERS = Xr.h

libXr_la_SOURCES = \
	Xr.h \
	xr.c \
	xrint.h \
	xrcolor.c \
	xrfiller.c \
	xrfont.c \
	xrgstate.c \
	xrmisc.c \
	xrpath.c \
	xrpen.c \
	xrpolygon.c \
	xrspline.c \
	xrstate.c \
	xrstroker.c \
	xrsurface.c \
	xrtransform.c \
	xrtraps.c

libXr_la_LDFLAGS = -version-info @VERSION_INFO@

INCLUDES = $(XR_CFLAGS) $(X_CFLAGS)

libXr_la_LIBADD = $(XR_LIBS)
