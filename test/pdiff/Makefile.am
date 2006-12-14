EXTRA_PROGRAMS = perceptualdiff

noinst_LTLIBRARIES = libpdiff.la
libpdiff_la_SOURCES = 		\
	pdiff.h			\
	lpyramid.c		\
	lpyramid.h		\
	pdiff.cpp		\
	RGBAImage.cpp		\
	RGBAImage.h

perceptualdiff_SOURCES =	\
	CompareArgs.cpp		\
	CompareArgs.h		\
	PerceptualDiff.cpp

INCLUDES = -I$(top_srcdir)/src
LDADD = libpdiff.la $(top_builddir)/src/libcairo.la
