lib_LTLIBRARIES = libXr.la
include_HEADERS = Xr.h

libXr_la_SOURCES = \
	Xr.h \
	xr.c \
	xrint.h \
	xrcolor.c \
	xrfont.c \
	xrgstate.c \
	xrmatrix.c \
	xrmisc.c \
	xrpath.c \
	xrpathbounds.c \
	xrpathfill.c \
	xrpathstroke.c \
	xrpen.c \
	xrpolygon.c \
	xrspline.c \
	xrstate.c \
	xrsurface.c \
	xrtraps.c

libXr_la_LDFLAGS = -version-info @VERSION_INFO@

INCLUDES = $(XR_CFLAGS) $(X_CFLAGS)

libXr_la_LIBADD = $(XR_LIBS) -lm
