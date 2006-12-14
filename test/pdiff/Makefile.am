EXTRA_PROGRAMS = perceptualdiff

noinst_LTLIBRARIES = libpdiff.la
libpdiff_la_SOURCES = 		\
	pdiff.h			\
	lpyramid.c		\
	lpyramid.h		\
	pdiff.cpp

perceptualdiff_SOURCES =	\
	args.cpp		\
	args.h			\
	PerceptualDiff.cpp

INCLUDES = -I$(top_srcdir)/src
LDADD = libpdiff.la $(top_builddir)/src/libcairo.la
