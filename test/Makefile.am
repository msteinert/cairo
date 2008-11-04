include $(top_srcdir)/build/Makefile.am.common

SUBDIRS=pdiff .

CLEANFILES += have-similar.*

test_sources = \
	a1-image-sample.c 				\
	a1-mask.c					\
	a1-traps-sample.c				\
	a8-mask.c					\
	alpha-similar.c					\
	big-line.c					\
	big-trap.c					\
	bilevel-image.c					\
	caps-joins.c					\
	caps-joins-alpha.c				\
	caps-sub-paths.c				\
	clip-all.c					\
	clip-empty.c					\
	clip-fill-rule.c				\
	clip-fill-rule-pixel-aligned.c			\
	clip-nesting.c					\
	clip-operator.c					\
	clip-push-group.c				\
	clip-twice.c					\
	clip-zero.c					\
	clipped-group.c					\
	clipped-surface.c				\
	close-path.c					\
	composite-integer-translate-source.c		\
	composite-integer-translate-over.c		\
	composite-integer-translate-over-repeat.c	\
	copy-path.c					\
	create-for-stream.c				\
	create-from-png.c				\
	create-from-png-stream.c			\
	dash-caps-joins.c				\
	dash-curve.c					\
	dash-no-dash.c					\
	dash-offset-negative.c				\
	dash-scale.c					\
	dash-state.c					\
	dash-zero-length.c				\
	degenerate-arc.c				\
	degenerate-dash.c				\
	degenerate-path.c				\
	degenerate-pen.c				\
	device-offset.c					\
	device-offset-fractional.c			\
	device-offset-positive.c			\
	device-offset-scale.c				\
	extend-pad.c					\
	extend-pad-border.c				\
	extend-pad-similar.c				\
	extend-reflect.c				\
	extend-reflect-similar.c			\
	extend-repeat.c					\
	extend-repeat-similar.c				\
	fill-alpha.c					\
	fill-alpha-pattern.c				\
	fill-and-stroke.c				\
	fill-and-stroke-alpha.c				\
	fill-and-stroke-alpha-add.c			\
	fill-degenerate-sort-order.c			\
	fill-missed-stop.c				\
	fill-rule.c					\
	filter-bilinear-extents.c			\
	filter-nearest-offset.c				\
	filter-nearest-transformed.c			\
	finer-grained-fallbacks.c			\
	font-face-get-type.c				\
	font-matrix-translation.c			\
	font-options.c					\
	glyph-cache-pressure.c				\
	get-and-set.c					\
	get-clip.c					\
	get-group-target.c				\
	get-path-extents.c				\
	gradient-alpha.c				\
	gradient-constant-alpha.c			\
	gradient-zero-stops.c				\
	group-paint.c					\
	huge-pattern.c					\
	image-surface-source.c				\
	infinite-join.c					\
	in-fill-empty-trapezoid.c			\
	in-fill-trapezoid.c				\
	invalid-matrix.c				\
	large-clip.c					\
	large-font.c					\
	large-source.c					\
	leaky-dash.c					\
	leaky-dashed-rectangle.c			\
	leaky-dashed-stroke.c				\
	leaky-polygon.c					\
	line-width.c					\
	line-width-scale.c				\
	line-width-zero.c				\
	linear-gradient.c				\
	linear-gradient-reflect.c			\
	long-dashed-lines.c				\
	long-lines.c					\
	mask.c						\
	mask-alpha.c					\
	mask-ctm.c					\
	mask-surface-ctm.c				\
	mask-transformed-image.c			\
	mask-transformed-similar.c			\
	meta-surface-pattern.c				\
	mime-data.c					\
	miter-precision.c				\
	move-to-show-surface.c				\
	new-sub-path.c					\
	nil-surface.c					\
	operator.c					\
	operator-alpha.c				\
	operator-clear.c				\
	operator-source.c				\
	over-above-source.c				\
	over-around-source.c				\
	over-below-source.c				\
	over-between-source.c				\
	paint.c						\
	paint-repeat.c					\
	paint-source-alpha.c				\
	paint-with-alpha.c				\
	pattern-get-type.c				\
	pattern-getters.c				\
	pixman-rotate.c					\
	png.c						\
	push-group.c					\
	radial-gradient.c				\
	random-intersections.c				\
	rectangle-rounding-error.c			\
	rectilinear-fill.c				\
	rectilinear-miter-limit.c			\
	rectilinear-stroke.c				\
	reflected-stroke.c				\
	rel-path.c					\
	rgb24-ignore-alpha.c				\
	rotate-image-surface-paint.c			\
	scale-down-source-surface-paint.c		\
	scale-source-surface-paint.c			\
	stroke-ctm-caps.c				\
	stroke-image.c				        \
	select-font-face.c				\
	select-font-no-show-text.c			\
	self-copy.c					\
	self-copy-overlap.c				\
	self-intersecting.c				\
	set-source.c					\
	show-glyphs-many.c				\
	show-text-current-point.c			\
	skew-extreme.c					\
	smask.c						\
	smask-fill.c					\
	smask-image-mask.c				\
	smask-mask.c					\
	smask-paint.c					\
	smask-stroke.c					\
	smask-text.c					\
	solid-pattern-cache-stress.c			\
	source-clip.c					\
	source-clip-scale.c				\
	source-surface-scale-paint.c			\
	spline-decomposition.c				\
	surface-finish-twice.c				\
	surface-pattern.c				\
	surface-pattern-big-scale-down.c		\
	surface-pattern-scale-down.c			\
	surface-pattern-scale-up.c			\
	text-antialias-gray.c				\
	text-antialias-none.c				\
	text-antialias-subpixel.c			\
	text-cache-crash.c				\
	text-glyph-range.c				\
	text-pattern.c					\
	text-rotate.c					\
	text-transform.c				\
	text-zero-len.c					\
	toy-font-face.c					\
	transforms.c					\
	translate-show-surface.c			\
	trap-clip.c					\
	truetype-tables.c				\
	twin.c						\
	unantialiased-shapes.c				\
	unbounded-operator.c				\
	user-data.c					\
	user-font.c					\
	user-font-mask.c				\
	user-font-proxy.c				\
	user-font-rescale.c				\
	zero-alpha.c

# Then we have a collection of tests that are only run if certain
# features are compiled into cairo
if HAVE_PTHREAD
test_sources += pthread-show-text.c
endif

if CAIRO_HAS_FT_FONT
test_sources += bitmap-font.c
test_sources += ft-font-create-for-ft-face.c
test_sources += ft-show-glyphs-positioning.c
test_sources += ft-show-glyphs-table.c
test_sources += ft-text-vertical-layout-type1.c
test_sources += ft-text-vertical-layout-type3.c
test_sources += ft-text-antialias-none.c
endif

# Need to add win32-surface-source, quartz-surface-source
if CAIRO_HAS_GLITZ_SURFACE
test_sources += glitz-surface-source.c
endif

if CAIRO_HAS_PDF_SURFACE
test_sources += pdf-features.c
test_sources += pdf-mime-data.c
test_sources += pdf-surface-source.c
endif

if CAIRO_HAS_PS_SURFACE
test_sources += ps-features.c
test_sources += ps-surface-source.c
endif

if CAIRO_HAS_SVG_SURFACE
test_sources += svg-surface.c
test_sources += svg-clip.c
test_sources += svg-surface-source.c
endif

if CAIRO_HAS_XLIB_SURFACE
test_sources += xlib-expose-event.c
test_sources += xlib-surface.c
test_sources += xlib-surface-source.c
endif

if CAIRO_HAS_XLIB_XRENDER_SURFACE
test_sources += get-xrender-format.c
endif

if CAIRO_HAS_MULTI_PAGE_SURFACES
test_sources += multi-page.c
endif

# Include fallback-resolution (once!) if we have any of the vector surfaces
if CAIRO_HAS_SVG_SURFACE
test = fallback-resolution.c
endif
if CAIRO_HAS_PDF_SURFACE
test = fallback-resolution.c
endif
if CAIRO_HAS_PS_SURFACE
test = fallback-resolution.c
endif
test_sources += $(test)

TESTS += cairo-test-suite$(EXEEXT)

cairo-test-constructors.c: Makefile $(test_sources) make-cairo-test-constructors.pl
	@(cd $(srcdir) && ./make-cairo-test-constructors.pl $(test_sources)) > $@

cairo_test_suite_SOURCES = 		\
	buffer-diff.c			\
	buffer-diff.h			\
	cairo-test.c			\
	cairo-test.h			\
	cairo-test-private.h		\
	cairo-test-runner.c		\
	$(test_sources)
if CAIRO_HAS_CONSTRUCTOR_ATTRIBUTE
else
cairo_test_suite_SOURCES += cairo-test-constructors.c
endif
cairo_test_suite_LDADD = 					\
	$(top_builddir)/test/pdiff/libpdiff.la 			\
        $(top_builddir)/boilerplate/libcairoboilerplate.la	\
	$(top_builddir)/src/libcairo.la 			\
	$(CAIRO_LDADD)
cairo_test_suite_DEPENDENCIES = \
	$(top_builddir)/test/pdiff/libpdiff.la 			\
        $(top_builddir)/boilerplate/libcairoboilerplate.la	\
	$(top_builddir)/src/libcairo.la 			\
	any2ppm
if HAVE_PTHREAD
cairo_test_suite_LDADD += -lpthread
endif

BUILT_SOURCES += cairo-test-constructors.c
noinst_SCRIPTS = make-cairo-test-constructors.pl
EXTRA_DIST += $(BUILT_SOURCES) $(noinst_SCRIPTS) COPYING
CLEANFILES += $(BUILT_SOURCES)

# All tests which have a reference image go here.
REFERENCE_IMAGES = \
	a1-image-sample.ref.png		\
	a1-mask.ref.png	\
	a1-traps-sample.ref.png		\
	a8-mask.ref.png	\
	alpha-similar.ref.png	\
	alpha-similar.rgb24.ref.png	\
	big-line.ref.png		\
	big-line.rgb24.ref.png		\
	big-line.ps2.ref.png		\
	big-line.ps3.ref.png		\
	big-line.ps2.rgb24.ref.png	\
	big-line.ps3.rgb24.ref.png	\
	big-line.quartz.ref.png		\
	big-line.quartz.rgb24.ref.png	\
	bilevel-image.ref.png		\
	bitmap-font.ref.png	\
	bitmap-font.rgb24.ref.png	\
	caps-joins-alpha.quartz.ref.png	\
	caps-joins-alpha.ref.png	\
	caps-joins-alpha.svg12.ref.png	\
	caps-joins-alpha.svg11.ref.png	\
	caps-joins.ref.png	\
	caps-joins.ps2.ref.png	\
	caps-joins.ps3.ref.png	\
	caps-sub-paths.ref.png	\
	clip-all.ref.png	\
	clip-empty.ref.png	\
	clip-fill-rule-pixel-aligned.ref.png	\
	clip-fill-rule-pixel-aligned.rgb24.ref.png	\
	clip-fill-rule.ps2.argb32.ref.png	\
	clip-fill-rule.ps3.argb32.ref.png	\
	clip-fill-rule.ps2.rgb24.ref.png		\
	clip-fill-rule.ps3.rgb24.ref.png		\
	clip-fill-rule.ref.png	\
	clip-fill-rule.rgb24.ref.png	\
	clip-nesting.ps2.argb32.ref.png	\
	clip-nesting.ps3.argb32.ref.png	\
	clip-nesting.ps2.rgb24.ref.png	\
	clip-nesting.ps3.rgb24.ref.png	\
	clip-nesting.quartz.ref.png	\
	clip-nesting.quartz.rgb24.ref.png	\
	clip-nesting.ref.png	\
	clip-nesting.rgb24.ref.png	\
	clip-operator.ref.png	\
	clip-operator.pdf.argb32.ref.png \
	clip-operator.pdf.rgb24.ref.png \
	clip-operator.ps2.rgb24.ref.png	\
	clip-operator.ps3.rgb24.ref.png	\
	clip-operator.ps3.ref.png	\
	clip-operator.rgb24.ref.png	\
	clip-operator.quartz.ref.png	\
	clip-operator.quartz.rgb24.ref.png	\
	clip-push-group.ps2.argb32.ref.png	\
	clip-push-group.ps3.argb32.ref.png	\
	clip-push-group.ps2.rgb24.ref.png	\
	clip-push-group.ps3.rgb24.ref.png	\
	clip-push-group.quartz.ref.png	\
	clip-push-group.ref.png	\
	clip-twice.ps2.argb32.ref.png	\
	clip-twice.ps3.argb32.ref.png	\
	clip-twice.ps2.rgb24.ref.png	\
	clip-twice.ps3.rgb24.ref.png	\
	clip-twice.quartz.ref.png	\
	clip-twice.quartz.rgb24.ref.png	\
	clip-twice.ref.png	\
	clip-twice.rgb24.ref.png	\
	clipped-group.ref.png	\
	clipped-group.ps2.ref.png	\
	clipped-group.ps3.ref.png	\
	clipped-surface.ref.png	\
	close-path.ref.png	\
	close-path.ps2.ref.png	\
	close-path.ps3.ref.png	\
	composite-integer-translate-over.ref.png	\
	composite-integer-translate-over.ps2.ref.png	\
	composite-integer-translate-over.ps3.ref.png	\
	composite-integer-translate-over-repeat.ref.png	\
	composite-integer-translate-over-repeat.ps2.ref.png	\
	composite-integer-translate-over-repeat.ps3.ref.png	\
	composite-integer-translate-source.ref.png	\
	composite-integer-translate-source.ps2.ref.png	\
	composite-integer-translate-source.ps3.ref.png	\
	copy-path.ps2.ref.png	\
	copy-path.ps3.ref.png	\
	copy-path.ref.png	\
	create-from-png.ref.png			\
	create-from-png.alpha.ref.png		\
	create-from-png.gray.ref.png		\
	create-from-png.gray-alpha.ref.png	\
	create-from-png.indexed.ref.png		\
	create-from-png.indexed-alpha.ref.png	\
	create-from-png-stream.ref.png	\
	dash-caps-joins.ps2.argb32.ref.png	\
	dash-caps-joins.ps3.argb32.ref.png	\
	dash-caps-joins.ps2.rgb24.ref.png	\
	dash-caps-joins.ps3.rgb24.ref.png	\
	dash-caps-joins.quartz.ref.png	\
	dash-caps-joins.ref.png	\
	dash-curve.ref.png	\
	dash-curve.ps2.ref.png \
	dash-curve.ps3.ref.png \
	dash-curve.quartz.ref.png \
	dash-no-dash.ref.png	\
	dash-offset-negative.ref.png	\
	dash-scale.ps2.argb32.ref.png	\
	dash-scale.ps3.argb32.ref.png	\
	dash-scale.ps2.rgb24.ref.png	\
	dash-scale.ps3.rgb24.ref.png	\
	dash-scale.quartz.ref.png	\
	dash-scale.ref.png	\
	dash-state.ps2.ref.png	\
	dash-state.ps3.ref.png	\
	dash-state.ref.png	\
	dash-state.quartz.ref.png	\
	dash-zero-length.ps2.ref.png	\
	dash-zero-length.ps3.ref.png	\
	dash-zero-length.ps2.rgb24.ref.png	\
	dash-zero-length.ps3.rgb24.ref.png	\
	dash-zero-length.ref.png	\
	dash-zero-length.rgb24.ref.png	\
	degenerate-arc.ref.png		\
	degenerate-arc.ps2.ref.png	\
	degenerate-arc.ps3.ref.png	\
	degenerate-dash.ref.png		\
	degenerate-pen.ref.png		\
	degenerate-pen.ps2.ref.png	\
	degenerate-pen.ps3.ref.png	\
	degenerate-pen.quartz.ref.png	\
	degenerate-path.ps2.argb32.ref.png	\
	degenerate-path.ps2.rgb24.ref.png	\
	degenerate-path.ps3.argb32.ref.png	\
	degenerate-path.ps3.rgb24.ref.png	\
	degenerate-path.ref.png	\
	degenerate-path.rgb24.ref.png	\
	degenerate-path.quartz.ref.png	\
	degenerate-path.quartz.rgb24.ref.png	\
	device-offset-fractional.ref.png	\
	device-offset-fractional.pdf.ref.png	\
	device-offset-fractional.ps2.ref.png	\
	device-offset-fractional.ps3.ref.png	\
	device-offset-positive.ref.png	\
	device-offset-positive.rgb24.ref.png	\
	device-offset.ref.png	\
	device-offset.rgb24.ref.png	\
	device-offset-scale.ref.png	\
	extend-pad.ref.png	\
	extend-pad-border.ref.png	\
	extend-pad-similar.ref.png	\
	extend-reflect.ref.png	\
	extend-reflect-similar.ref.png	\
	extend-reflect-similar.ps2.ref.png	\
	extend-reflect-similar.ps3.ref.png	\
	extend-reflect.ps2.ref.png \
	extend-reflect.ps3.ref.png \
	extend-repeat.ref.png	\
	extend-repeat.ps2.ref.png	\
	extend-repeat.ps3.ref.png	\
	extend-repeat-similar.ref.png	\
	extend-repeat-similar.ps2.ref.png	\
	extend-repeat-similar.ps3.ref.png	\
	fallback-resolution.ppi37.5x37.5.ref.png \
	fallback-resolution.ppi37.5x72.ref.png \
	fallback-resolution.ppi37.5x75.ref.png \
	fallback-resolution.ppi37.5x150.ref.png \
	fallback-resolution.ppi37.5x300.ref.png \
	fallback-resolution.ppi37.5x600.ref.png \
	fallback-resolution.ppi72x37.5.ref.png \
	fallback-resolution.ppi72x72.ref.png \
	fallback-resolution.ppi72x75.ref.png \
	fallback-resolution.ppi72x150.ref.png \
	fallback-resolution.ppi72x300.ref.png \
	fallback-resolution.ppi72x600.ref.png \
	fallback-resolution.ppi75x37.5.ref.png \
	fallback-resolution.ppi75x72.ref.png \
	fallback-resolution.ppi75x75.ref.png \
	fallback-resolution.ppi75x150.ref.png \
	fallback-resolution.ppi75x300.ref.png \
	fallback-resolution.ppi75x600.ref.png \
	fallback-resolution.ppi150x37.5.ref.png \
	fallback-resolution.ppi150x72.ref.png \
	fallback-resolution.ppi150x75.ref.png \
	fallback-resolution.ppi150x150.ref.png \
	fallback-resolution.ppi150x300.ref.png \
	fallback-resolution.ppi150x600.ref.png \
	fallback-resolution.ppi300x37.5.ref.png \
	fallback-resolution.ppi300x72.ref.png \
	fallback-resolution.ppi300x75.ref.png \
	fallback-resolution.ppi300x150.ref.png \
	fallback-resolution.ppi300x300.ref.png \
	fallback-resolution.ppi300x600.ref.png \
	fallback-resolution.ppi600x37.5.ref.png \
	fallback-resolution.ppi600x72.ref.png \
	fallback-resolution.ppi600x75.ref.png \
	fallback-resolution.ppi600x150.ref.png \
	fallback-resolution.ppi600x300.ref.png \
	fallback-resolution.ppi600x600.ref.png \
	fill-alpha.ref.png	\
	fill-alpha-pattern.ref.png	\
	fill-alpha-pattern.pdf.argb32.ref.png	\
	fill-alpha-pattern.pdf.rgb24.ref.png	\
	fill-alpha-pattern.ps3.ref.png	\
	fill-and-stroke-alpha-add.quartz.ref.png	\
	fill-and-stroke-alpha-add.ref.png	\
	fill-and-stroke-alpha.quartz.ref.png	\
	fill-and-stroke-alpha.ref.png	\
	fill-and-stroke.ps2.argb32.ref.png	\
	fill-and-stroke.ps3.argb32.ref.png	\
	fill-and-stroke.ps2.rgb24.ref.png	\
	fill-and-stroke.ps3.rgb24.ref.png	\
	fill-and-stroke.quartz.ref.png	\
	fill-and-stroke.quartz.rgb24.ref.png	\
	fill-and-stroke.ref.png	\
	fill-and-stroke.rgb24.ref.png	\
	fill-degenerate-sort-order.quartz.ref.png	\
	fill-degenerate-sort-order.quartz.rgb24.ref.png	\
	fill-degenerate-sort-order.ref.png	\
	fill-degenerate-sort-order.rgb24.ref.png	\
	fill-missed-stop.ps2.argb32.ref.png	\
	fill-missed-stop.ps3.argb32.ref.png	\
	fill-missed-stop.ps2.rgb24.ref.png	\
	fill-missed-stop.ps3.rgb24.ref.png	\
	fill-missed-stop.ref.png	\
	fill-missed-stop.rgb24.ref.png	\
	fill-rule.ps2.argb32.ref.png	\
	fill-rule.ps3.argb32.ref.png	\
	fill-rule.ps2.rgb24.ref.png \
	fill-rule.ps3.rgb24.ref.png \
	fill-rule.quartz.ref.png	\
	fill-rule.quartz.rgb24.ref.png	\
	fill-rule.ref.png	\
	fill-rule.rgb24.ref.png	\
	filter-bilinear-extents.ref.png \
	filter-bilinear-extents.pdf.ref.png \
	filter-bilinear-extents.ps2.ref.png \
	filter-bilinear-extents.ps3.ref.png \
	filter-nearest-offset.ref.png	\
	filter-nearest-offset.pdf.ref.png	\
	filter-nearest-offset.ps2.ref.png	\
	filter-nearest-offset.ps3.ref.png	\
	filter-nearest-offset.svg11.ref.png	\
	filter-nearest-offset.svg12.ref.png	\
	filter-nearest-transformed.ref.png	\
	filter-nearest-transformed.pdf.ref.png	\
	filter-nearest-transformed.svg11.ref.png	\
	filter-nearest-transformed.svg12.ref.png	\
	finer-grained-fallbacks.ref.png			\
	finer-grained-fallbacks.rgb24.ref.png		\
	finer-grained-fallbacks.ps2.ref.png	\
	finer-grained-fallbacks.ps2.rgb24.ref.png	\
	finer-grained-fallbacks.ps3.ref.png	\
	finer-grained-fallbacks.ps3.rgb24.ref.png	\
	font-matrix-translation.ps2.argb32.ref.png	\
	font-matrix-translation.ps3.argb32.ref.png	\
	font-matrix-translation.ps2.rgb24.ref.png	\
	font-matrix-translation.ps3.rgb24.ref.png	\
	font-matrix-translation.ref.png	\
	font-matrix-translation.svg12.ref.png	\
	font-matrix-translation.svg11.ref.png	\
	font-matrix-translation.quartz.ref.png \
	ft-text-antialias-none.ps2.argb32.ref.png	\
	ft-text-antialias-none.ps3.argb32.ref.png	\
	ft-text-antialias-none.ref.png	\
	ft-show-glyphs-positioning.ref.png		\
	ft-show-glyphs-positioning.pdf.ref.png		\
	ft-show-glyphs-positioning.ps2.ref.png		\
	ft-show-glyphs-positioning.ps3.ref.png		\
	ft-show-glyphs-positioning.svg12.ref.png		\
	ft-show-glyphs-positioning.svg11.ref.png		\
	ft-show-glyphs-table.ref.png			\
	ft-show-glyphs-table.ps2.ref.png			\
	ft-show-glyphs-table.ps3.ref.png			\
	ft-text-vertical-layout-type1.pdf.ref.png	\
	ft-text-vertical-layout-type1.ps2.ref.png	\
	ft-text-vertical-layout-type1.ps3.ref.png	\
	ft-text-vertical-layout-type1.ref.png	\
	ft-text-vertical-layout-type1.svg12.ref.png	\
	ft-text-vertical-layout-type1.svg11.ref.png	\
	ft-text-vertical-layout-type3.pdf.ref.png	\
	ft-text-vertical-layout-type3.ps2.ref.png	\
	ft-text-vertical-layout-type3.ps3.ref.png	\
	ft-text-vertical-layout-type3.ref.png	\
	ft-text-vertical-layout-type3.svg12.ref.png	\
	ft-text-vertical-layout-type3.svg11.ref.png	\
	get-group-target.ref.png	\
	glitz-surface-source.ref.png \
	glyph-cache-pressure.ref.png	\
	glyph-cache-pressure.ps2.ref.png	\
	glyph-cache-pressure.ps3.ref.png	\
	glyph-cache-pressure.quartz.ref.png	\
	gradient-alpha.pdf.argb32.ref.png	\
	gradient-alpha.pdf.rgb24.ref.png	\
	gradient-alpha.ps2.argb32.ref.png	\
	gradient-alpha.ps3.argb32.ref.png	\
	gradient-alpha.ps2.rgb24.ref.png	\
	gradient-alpha.ps3.rgb24.ref.png	\
	gradient-alpha.ref.png	\
	gradient-alpha.rgb24.ref.png	\
	gradient-constant-alpha.pdf.argb32.ref.png	\
	gradient-constant-alpha.pdf.rgb24.ref.png	\
	gradient-constant-alpha.ps3.ref.png		\
	gradient-constant-alpha.ps3.rgb24.ref.png	\
	gradient-constant-alpha.ref.png	\
	gradient-constant-alpha.rgb24.ref.png	\
	gradient-zero-stops.ref.png	\
	gradient-zero-stops.rgb24.ref.png	\
	group-paint.ref.png	\
	huge-pattern.ref.png	\
	huge-pattern.ps3.ref.png	\
	huge-pattern.pdf.ref.png	\
	huge-pattern.pdf.rgb24.ref.png	\
	image-surface-source.ref.png \
	image-surface-source.ps2.ref.png \
	image-surface-source.ps3.ref.png \
	infinite-join.ref.png	\
	infinite-join.ps2.ref.png	\
	infinite-join.ps3.ref.png	\
	large-clip.ref.png	\
	large-font.ref.png	\
	large-source.ref.png	\
	leaky-dash.ps2.argb32.ref.png	\
	leaky-dash.ps3.argb32.ref.png	\
	leaky-dash.ps2.rgb24.ref.png	\
	leaky-dash.ps3.rgb24.ref.png	\
	leaky-dash.quartz.ref.png	\
	leaky-dash.ref.png	\
	leaky-dashed-rectangle.ref.png		\
	leaky-dashed-rectangle.ps2.ref.png	\
	leaky-dashed-rectangle.ps3.ref.png	\
	leaky-dashed-stroke.ref.png		\
	leaky-dashed-stroke.ps2.ref.png		\
	leaky-dashed-stroke.ps3.ref.png		\
	leaky-polygon.ref.png	\
	leaky-polygon.ps2.ref.png	\
	leaky-polygon.ps3.ref.png	\
	linear-gradient-reflect.ref.png	\
	linear-gradient-reflect.pdf.argb32.ref.png	\
	linear-gradient-reflect.pdf.rgb24.ref.png	\
	linear-gradient-reflect.ps3.ref.png	\
	linear-gradient-reflect.quartz.ref.png	\
	linear-gradient.pdf.ref.png	\
	linear-gradient.ps3.ref.png	\
	linear-gradient.quartz.ref.png	\
	linear-gradient.ref.png	\
	linear-gradient.svg12.ref.png	\
	linear-gradient.svg11.ref.png	\
	line-width.ref.png	\
	line-width-scale.ps2.ref.png	\
	line-width-scale.ps3.ref.png	\
	line-width-scale.quartz.ref.png	\
	line-width-scale.ref.png	\
	long-dashed-lines.ref.png	\
	long-dashed-lines.ps2.ref.png	\
	long-dashed-lines.ps3.ref.png	\
	long-dashed-lines.quartz.ref.png	\
	long-lines.ref.png	\
	mask-alpha.ref.png	\
	mask-alpha.svg11.argb32.ref.png	\
	mask-alpha.svg12.argb32.ref.png	\
	mask-alpha.rgb24.ref.png	\
	mask-alpha.quartz.argb32.ref.png \
	mask-ctm.ref.png	\
	mask-ctm.rgb24.ref.png	\
	mask-ctm.svg11.argb32.ref.png	\
	mask-ctm.svg12.argb32.ref.png	\
	mask.ref.png	\
	mask.rgb24.ref.png	\
	mask.pdf.argb32.ref.png	\
	mask.pdf.rgb24.ref.png	\
	mask.quartz.ref.png	\
	mask.quartz.rgb24.ref.png	\
	mask-surface-ctm.ref.png	\
	mask-surface-ctm.rgb24.ref.png	\
	mask-surface-ctm.svg11.argb32.ref.png	\
	mask-surface-ctm.svg12.argb32.ref.png	\
	mask.svg11.rgb24.ref.png	\
	mask.svg11.argb32.ref.png	\
	mask.svg12.rgb24.ref.png	\
	mask.svg12.argb32.ref.png	\
	mask-transformed-image.ref.png	\
	mask-transformed-similar.ref.png	\
	mask-transformed-similar.pdf.ref.png	\
	mask-transformed-similar.svg12.ref.png	\
	mask-transformed-similar.svg11.ref.png	\
	meta-surface-pattern.ref.png		\
	meta-surface-pattern.rgb24.ref.png	\
	meta-surface-pattern.pdf.ref.png	\
	meta-surface-pattern.pdf.rgb24.ref.png	\
	meta-surface-pattern.ps2.argb32.ref.png	\
	meta-surface-pattern.ps2.rgb24.ref.png	\
	meta-surface-pattern.ps3.argb32.ref.png	\
	meta-surface-pattern.ps3.rgb24.ref.png	\
	meta-surface-pattern.quartz.ref.png	\
	meta-surface-pattern.quartz.rgb24.ref.png	\
	meta-surface-pattern.svg11.argb32.ref.png	\
	meta-surface-pattern.svg11.rgb24.ref.png	\
	meta-surface-pattern.svg12.argb32.ref.png	\
	meta-surface-pattern.svg12.rgb24.ref.png	\
	mime-data.ref.png	\
	mime-data.ps.ref.png	\
	mime-data.pdf.ref.png	\
	miter-precision.ref.png \
	miter-precision.ps2.ref.png	\
	miter-precision.ps3.ref.png	\
	move-to-show-surface.ref.png	\
	new-sub-path.ps2.argb32.ref.png	\
	new-sub-path.ps3.argb32.ref.png	\
	new-sub-path.ps2.rgb24.ref.png	\
	new-sub-path.ps3.rgb24.ref.png	\
	new-sub-path.quartz.ref.png	\
	new-sub-path.quartz.rgb24.ref.png	\
	new-sub-path.ref.png	\
	new-sub-path.rgb24.ref.png	\
	nil-surface.ref.png	\
	nil-surface.rgb24.ref.png	\
	operator.ref.png	\
	operator.rgb24.ref.png	\
	operator-alpha.ref.png	\
	operator-alpha.rgb24.ref.png	\
	operator-clear.quartz.ref.png	\
	operator-clear.quartz.rgb24.ref.png	\
	operator-clear.ref.png	\
	operator-clear.rgb24.ref.png	\
	operator-clear.pdf.argb32.ref.png	\
	operator-clear.pdf.rgb24.ref.png	\
	operator-source.pdf.argb32.ref.png	\
	operator-source.pdf.rgb24.ref.png	\
	operator-source.ref.png	\
	operator-source.rgb24.ref.png	\
	operator-source.quartz.ref.png \
	operator-source.quartz.rgb24.ref.png \
	over-above-source.ps2.argb32.ref.png       \
	over-above-source.ps3.argb32.ref.png       \
	over-above-source.quartz.ref.png	  \
	over-above-source.quartz.rgb24.ref.png	  \
	over-above-source.ref.png                 \
	over-above-source.rgb24.ref.png           \
	over-around-source.ps2.argb32.ref.png       \
	over-around-source.ps3.argb32.ref.png       \
	over-around-source.ps2.rgb24.ref.png        \
	over-around-source.ps3.rgb24.ref.png        \
	over-around-source.quartz.ref.png	   \
	over-around-source.quartz.rgb24.ref.png	   \
	over-around-source.ref.png                 \
	over-around-source.rgb24.ref.png           \
	over-below-source.ps2.argb32.ref.png       \
	over-below-source.ps3.argb32.ref.png       \
	over-below-source.ref.png                 \
	over-below-source.rgb24.ref.png           \
	over-between-source.ps2.argb32.ref.png     \
	over-between-source.ps3.argb32.ref.png     \
	over-between-source.quartz.ref.png        \
	over-between-source.quartz.rgb24.ref.png  \
	over-between-source.ref.png                 \
	over-between-source.rgb24.ref.png           \
	paint.ref.png	\
	paint-repeat.ref.png	\
	paint-source-alpha.pdf.argb32.ref.png	\
	paint-source-alpha.ref.png	\
	paint-source-alpha.svg12.ref.png	\
	paint-source-alpha.svg11.ref.png	\
	paint-with-alpha.ref.png	\
	paint-with-alpha.svg12.ref.png	\
	paint-with-alpha.svg11.ref.png	\
	paint-source-alpha.pdf.ref.png \
	paint-with-alpha.pdf.ref.png \
	pass-through.ref.png \
	pass-through.rgb24.ref.png \
	pattern-getters.ref.png	\
	pdf-surface-source.ref.png \
	pixman-rotate.ref.png	\
	pixman-rotate.rgb24.ref.png	\
	ps-surface-source.ref.png \
	push-group.pdf.ref.png \
	push-group.pdf.rgb24.ref.png \
	push-group.ref.png	\
	push-group.rgb24.ref.png	\
	push-group.svg11.argb32.ref.png	\
	push-group.svg12.argb32.ref.png	\
	radial-gradient.ref.png	\
	radial-gradient.pdf.ref.png \
	radial-gradient.quartz.ref.png \
	radial-gradient.svg12.ref.png	\
	radial-gradient.svg11.ref.png	\
	random-intersections.ref.png	\
	random-intersections.ps2.ref.png \
	random-intersections.ps3.ref.png \
	random-intersections.quartz.ref.png \
	rgb24-ignore-alpha.ref.png \
	rectangle-rounding-error.ref.png	\
	rectilinear-fill.ref.png	\
	rectilinear-miter-limit.ref.png		\
	rectilinear-miter-limit.ps2.ref.png	\
	rectilinear-miter-limit.ps3.ref.png	\
	rectilinear-stroke.ref.png	\
	reflected-stroke.ref.png	\
	reflected-stroke.ps2.ref.png	\
	reflected-stroke.ps3.ref.png	\
	reflected-stroke.quartz.ref.png	\
	rel-path.quartz.ref.png	\
	rel-path.quartz.rgb24.ref.png	\
	rel-path.ps2.rgb24.ref.png	\
	rel-path.ps3.rgb24.ref.png	\
	rel-path.ref.png	\
	rel-path.rgb24.ref.png	\
	rotate-image-surface-paint.pdf.rgb24.ref.png \
	rotate-image-surface-paint.ps2.ref.png \
	rotate-image-surface-paint.ps3.ref.png \
	rotate-image-surface-paint.pdf.argb32.ref.png	\
	rotate-image-surface-paint.quartz.ref.png	\
	rotate-image-surface-paint.ref.png	\
	rotate-image-surface-paint.svg12.ref.png	\
	rotate-image-surface-paint.svg11.ref.png	\
	scale-down-source-surface-paint.ref.png	\
	scale-source-surface-paint.pdf.argb32.ref.png	\
	scale-source-surface-paint.ref.png	\
	scale-source-surface-paint.rgb24.ref.png	\
	scale-source-surface-paint.svg11.argb32.ref.png	\
	scale-source-surface-paint.svg11.rgb24.ref.png	\
	scale-source-surface-paint.svg12.argb32.ref.png	\
	scale-source-surface-paint.svg12.rgb24.ref.png	\
	scale-source-surface-paint.pdf.rgb24.ref.png \
	stroke-ctm-caps.ref.png \
	stroke-ctm-caps.quartz.ref.png \
	select-font-face.ref.png	\
	select-font-face.ps2.ref.png \
	select-font-face.ps3.ref.png \
	select-font-face.quartz.ref.png \
	self-copy.ref.png	\
	self-copy.ps2.ref.png	\
	self-copy.ps3.ref.png	\
	self-copy-overlap.ref.png	\
	self-copy-overlap.rgb24.ref.png	\
	self-intersecting.ref.png	\
	self-intersecting.rgb24.ref.png	\
	set-source.ref.png	\
	set-source.rgb24.ref.png	\
	set-source.svg11.argb32.ref.png	\
	set-source.svg12.argb32.ref.png	\
	show-glyphs-many.ref.png	\
	show-text-current-point.ref.png	\
	show-text-current-point.ps2.ref.png \
	show-text-current-point.ps3.ref.png \
	show-text-current-point.quartz.ref.png \
	skew-extreme.ref.png \
	skew-extreme.ps2.ref.png \
	skew-extreme.ps3.ref.png \
	smask.ref.png			\
	smask.pdf.ref.png		\
	smask.ps2.ref.png		\
	smask.ps3.ref.png		\
	smask.svg12.ref.png		\
	smask.svg11.ref.png		\
	smask-fill.ref.png		\
	smask-fill.pdf.ref.png		\
	smask-fill.svg12.ref.png	\
	smask-fill.svg11.ref.png	\
	smask-image-mask.ref.png	\
	smask-image-mask.pdf.ref.png	\
	smask-mask.ref.png		\
	smask-mask.pdf.ref.png		\
	smask-mask.svg12.ref.png	\
	smask-mask.svg11.ref.png	\
	smask-paint.ref.png		\
	smask-paint.pdf.ref.png		\
	smask-paint.svg12.ref.png	\
	smask-paint.svg11.ref.png	\
	smask-stroke.ref.png		\
	smask-stroke.pdf.ref.png	\
	smask-text.ref.png		\
	smask-text.pdf.ref.png		\
	smask-text.ps2.ref.png		\
	smask-text.ps3.ref.png		\
	smask-text.svg12.ref.png	\
	smask-text.svg11.ref.png	\
	stroke-image.ref.png \
	stroke-image.pdf.ref.png \
	stroke-image.ps2.ref.png \
	stroke-image.ps3.ref.png \
	stroke-image.quartz.ref.png \
	solid-pattern-cache-stress.ref.png	\
	source-clip.ref.png	\
	source-clip-scale.quartz.ref.png	\
	source-clip-scale.ps2.argb32.ref.png	\
	source-clip-scale.ps3.argb32.ref.png	\
	source-clip-scale.ps2.rgb24.ref.png	\
	source-clip-scale.ps3.rgb24.ref.png	\
	source-clip-scale.ref.png	\
	source-clip-scale.svg12.ref.png	\
	source-clip-scale.svg11.ref.png	\
	source-clip-scale.pdf.ref.png \
	source-surface-scale-paint.ref.png	\
	source-surface-scale-paint.rgb24.ref.png	\
	spline-decomposition.ref.png \
	spline-decomposition.ps2.ref.png \
	spline-decomposition.ps3.ref.png \
	spline-decomposition.pdf.ref.png \
	spline-decomposition.svg11.ref.png \
	spline-decomposition.svg12.ref.png \
	stroke-ctm-caps.ps2.ref.png \
	stroke-ctm-caps.ps3.ref.png \
	surface-pattern-big-scale-down.ref.png	\
	surface-pattern.pdf.ref.png	\
	surface-pattern.ps2.ref.png	\
	surface-pattern.ps3.ref.png	\
	surface-pattern.ref.png	\
	surface-pattern-scale-down.pdf.argb32.ref.png	\
	surface-pattern-scale-down.pdf.rgb24.ref.png	\
	surface-pattern-scale-down.ps2.ref.png	\
	surface-pattern-scale-down.ps3.ref.png	\
	surface-pattern-scale-down.ref.png	\
	surface-pattern-scale-down.quartz.ref.png	\
	surface-pattern-scale-up.pdf.argb32.ref.png	\
	surface-pattern-scale-up.pdf.rgb24.ref.png	\
	surface-pattern-scale-up.ps2.ref.png	\
	surface-pattern-scale-up.ps3.ref.png	\
	surface-pattern-scale-up.ref.png	\
	surface-pattern.svg12.ref.png	\
	surface-pattern.svg11.ref.png	\
	svg-surface-source.ref.png \
	text-antialias-gray.ref.png	\
	text-antialias-gray.quartz.ref.png	\
	text-antialias-none.ref.png	\
	text-antialias-none.quartz.ref.png	\
	text-antialias-subpixel.ref.png	\
	text-antialias-subpixel.quartz.ref.png	\
	text-glyph-range.ref.png	\
	text-glyph-range.ps2.ref.png	\
	text-glyph-range.ps3.ref.png	\
	text-pattern.ps3.argb32.ref.png	\
	text-pattern.ps3.rgb24.ref.png	\
	text-pattern.ref.png	\
	text-pattern.rgb24.ref.png	\
	text-pattern.svg11.argb32.ref.png	\
	text-pattern.svg11.rgb24.ref.png	\
	text-pattern.svg12.argb32.ref.png	\
	text-pattern.svg12.rgb24.ref.png	\
	text-pattern.pdf.argb32.ref.png \
	text-pattern.pdf.rgb24.ref.png \
	text-pattern.quartz.ref.png \
	text-pattern.quartz.rgb24.ref.png \
	text-rotate.ref.png	\
	text-rotate.pdf.ref.png \
	text-rotate.ps2.ref.png	\
	text-rotate.ps3.ref.png	\
	text-rotate.svg12.ref.png \
	text-rotate.svg11.ref.png \
	text-rotate.quartz.ref.png \
	text-transform.ref.png		\
	text-transform.pdf.ref.png	\
	text-transform.ps2.ref.png	\
	text-transform.ps3.ref.png	\
	transforms.ref.png	\
	transforms.ps2.ref.png	\
	transforms.ps3.ref.png	\
	translate-show-surface.ref.png	\
	trap-clip.quartz.ref.png	\
	trap-clip.quartz.rgb24.ref.png	\
	trap-clip.ps3.argb32.ref.png	\
	trap-clip.ps3.rgb24.ref.png	\
	trap-clip.ref.png	\
	trap-clip.rgb24.ref.png	\
	trap-clip.pdf.argb32.ref.png \
	trap-clip.pdf.rgb24.ref.png \
	trap-clip.ps2.argb32.ref.png \
	trap-clip.ps2.rgb24.ref.png \
	twin.ref.png \
	twin.ps2.ref.png \
	twin.ps3.ref.png \
	twin.svg11.ref.png \
	twin.svg12.ref.png \
	unantialiased-shapes.ref.png	\
	unantialiased-shapes.quartz.ref.png \
	unbounded-operator.ref.png	\
	unbounded-operator.rgb24.ref.png	\
	user-font.ref.png	\
	user-font.ps2.ref.png \
	user-font.ps3.ref.png \
	user-font.svg12.ref.png	\
	user-font.svg11.ref.png	\
	user-font-mask.ref.png		\
	user-font-mask.pdf.ref.png	\
	user-font-mask.ps2.ref.png	\
	user-font-mask.ps3.ref.png	\
	user-font-mask.svg11.ref.png	\
	user-font-proxy.ref.png	\
	user-font-proxy.pdf.ref.png	\
	user-font-proxy.ps2.ref.png	\
	user-font-proxy.ps3.ref.png	\
	user-font-proxy.svg12.ref.png	\
	user-font-proxy.svg11.ref.png	\
	user-font-rescale.ref.png	\
	user-font-rescale.ps2.ref.png	\
	user-font-rescale.ps3.ref.png	\
	user-font-rescale.svg12.ref.png	\
	user-font-rescale.svg11.ref.png	\
	unbounded-operator.quartz.ref.png	\
	unbounded-operator.quartz.rgb24.ref.png	\
	xlib-expose-event.ref.png \
	xlib-surface-source.ref.png \
	xlib-surface-source.ps2.ref.png \
	xlib-surface-source.ps3.ref.png \
	zero-alpha.ref.png

EXTRA_DIST +=		\
6x13.pcf		\
make-html.pl		\
jpeg.jpg		\
romedalen.jpg		\
romedalen.png		\
scarab.jpg		\
surface-source.c	\
$(REFERENCE_IMAGES)

# Any test for which the code committed to CVS is expected to fail
# should be listed here.
#
# This way, we can avoid being bothered by reports of bugs we are
# aware of, but users can still report when tests start behaving in
# unexpected ways on their system.
#
# Of course, before any "release" of cairo we should eliminate
# everything from this list by fixing the bugs. (We don't necessarily
# have to be that strict for "snapshots" though.)
#
# Analysis of XFAIL errors:
# alpha-similar         - discrepancy between backends in applying color
#                         components of a pure alpha surface
# big-line              - range overflow of fixed-point
# big-trap              - range overflow of fixed-point
# degenerate-dash       - needs path editing in PS to convert degenerate
#                         end-caps into the shapes as expected by cairo
#                         (Or maybe PS is the correct behaviour?)
# degenerate-path       - undefined behaviour in PS, needs path editing to
#                         convert degenerate segments into circles/rectangles
#                         as expected by cairo
# device-offset-scale   - complication of pre-multiplying device_offset
#                         into the pattern_matrix and then requiring futher
#                         manipulation for SVG
# extend-pad            - lacks implementation in pixman and consequently used
#                         as an excuse for lack of support in other backends
# fallback-resolution   - The essential problem here is that the meta-surface
#                         has recorded a sequence of operations with one device
#                         transformation, and we attempt to replay it with
#                         another (basically a scale-factor for the falback
#                         resolution). Carl begun to look at this with his
#                         chain-of-bugs, but the can of worms is much bigger.
#                         It appears to be a design flaw in the meta-surface
#                         that may spread further...
#                         My solution would be to lock Behad and Adrian in a
#                         room, with Carl as a moderator and not let them out
#                         until they have come up with an interface and
#                         semantics that actually work. :-)
# long-lines            - range overflow of fixed-point
# self-copy-overlap     - vector surfaces take snapshot of patterns in contrast
#                         to the raster backends which don't. One solution
#                         would be to clone overlapping areas of dst/source, so
#                         patterns were effectively snapshotted across all
#                         backends.
# self-intersecting     - incremental trapezoidation of strokes can generate
#                         overlapping traps. Needs self-intersection analysis
#                         on cairo_traps_t after strokes.
#                         Test case should also be expanded to hit special-case
#                         tessellators as well.
# surface-pattern-big...- Strange, unexplained results for SVG/PS.
XFAIL_TESTS =					\
alpha-similar$(EXEEXT)				\
big-line$(EXEEXT)				\
big-trap$(EXEEXT)				\
degenerate-dash$(EXEEXT)			\
degenerate-path$(EXEEXT)			\
device-offset-scale$(EXEEXT)			\
extend-pad$(EXEEXT)				\
fallback-resolution$(EXEEXT)			\
long-lines$(EXEEXT)				\
self-copy-overlap$(EXEEXT)			\
self-intersecting$(EXEEXT)			\
surface-pattern-big-scale-down$(EXEEXT)		\
$(NULL)

# Any test that doesn't generate a log file goes here
NOLOG_TESTS =			\
fallback-resolution		\
font-options			\
multi-page			\
pdf-features			\
png				\
ps-features			\
svg-clip			\
svg-surface			\
toy-font-face			\
user-data

# A target to summarise the failures
check-summary:
	@FAILED_TESTS=""; \
	for t in $(test_sources:.c=.log); do \
	    if grep -e '\<FAIL\>' $$t >/dev/null 2>&1; then \
		FAILED_TESTS="$$FAILED_TESTS $${t%.log}"; \
	    fi; \
	done; \
	if test -n "$$FAILED_TESTS"; then \
	    echo "Failed tests:"; \
	    surfaces=""; \
	    for t in $$FAILED_TESTS; do \
		echo -n "     $$t: "; \
		grep -e '\<FAIL\>' $$t.log | sed -e 's/.*TARGET: \([^ ]*\).*/\1/' | sort | uniq | tr '\n' ' '; \
		echo; \
		for s in `grep -e '\<FAIL\>' $$t.log | sed -e 's/.*TARGET: \([^ ]*\).*/\1/' | sort | uniq`; do \
		    ss=`echo $$s | tr '-' '_'`; \
		    tt=`echo $$t | tr '-' '_'`; \
		    eval $$ss=\""$${!ss} $$tt"\"; \
		    echo $$surfaces | grep $$ss >/dev/null || surfaces="$$surfaces $$ss"; \
		done; \
	    done; \
	    echo -n "Failures per surface - "; \
	    first=""; \
	    for s in $$surfaces; do \
	        ss=`echo $$s | tr '_' '-'`; \
		test -n "$$first" && echo -n ", "; \
		cnt=`echo $${!s} | wc -w`; \
	        echo -n "$$ss: $$cnt"; \
		first="false"; \
	    done; \
	    echo "."; \
	    for s in $$surfaces; do \
	        ss=`echo $$s | tr '_' '-'`; \
		cnt=`echo $${!s} | wc -w`; \
	        echo -n "	$$ss [$$cnt]: "; \
		echo $${!s} | tr '_' '-'; \
	    done; \
	fi

AM_CPPFLAGS =					\
	-I$(srcdir)				\
	-I$(srcdir)/pdiff			\
	-I$(top_srcdir)/boilerplate		\
	-I$(top_srcdir)/src			\
	-I$(top_builddir)/src			\
	$(CAIRO_CFLAGS)

$(top_builddir)/boilerplate/libcairoboilerplate.la: $(top_builddir)/src/libcairo.la
	cd $(top_builddir)/boilerplate && $(MAKE) $(AM_MAKEFLAGS) libcairoboilerplate.la

$(top_builddir)/src/libcairo.la:
	cd $(top_builddir)/src && $(MAKE) $(AM_MAKEFLAGS) libcairo.la

$(top_builddir)/test/pdiff/libpdiff.la:
	cd $(top_builddir)/test/pdiff && $(MAKE) $(AM_MAKEFLAGS) libpdiff.la

EXTRA_PROGRAMS += imagediff png-flatten

imagediff_SOURCES = \
	imagediff.c	\
	buffer-diff.c	\
	buffer-diff.h
imagediff_LDADD = \
	$(top_builddir)/test/pdiff/libpdiff.la \
	$(top_builddir)/src/libcairo.la

png_flatten_SOURCES = png-flatten.c
png_flatten_LDADD = $(top_builddir)/src/libcairo.la $(CAIRO_LDADD)

if BUILD_ANY2PPM
check_PROGRAMS += any2ppm
any2ppm_CFLAGS = $(POPPLER_CFLAGS) $(LIBRSVG_CFLAGS) $(LIBSPECTRE_CFLAGS)
# add LDADD, so poppler/librsvg uses "our" cairo
any2ppm_LDFLAGS = $(CAIRO_TEST_UNDEFINED_LDFLAGS)
any2ppm_LDADD  = $(top_builddir)/src/libcairo.la $(CAIRO_LDADD) $(POPPLER_LIBS) $(LIBRSVG_LIBS) $(LIBSPECTRE_LIBS)
endif

if CAIRO_CAN_TEST_PDF_SURFACE
check_PROGRAMS += pdf2png
pdf2png_CFLAGS = $(POPPLER_CFLAGS)
# add LDADD, so poppler uses "our" cairo
pdf2png_LDFLAGS = $(CAIRO_TEST_UNDEFINED_LDFLAGS)
pdf2png_LDADD  = $(top_builddir)/src/libcairo.la $(CAIRO_LDADD) $(POPPLER_LIBS)
endif

if CAIRO_CAN_TEST_SVG_SURFACE
check_PROGRAMS += svg2png
svg2png_CFLAGS = $(LIBRSVG_CFLAGS)
# add LDADD, so librsvg uses "our" cairo
svg2png_LDFLAGS = $(CAIRO_TEST_UNDEFINED_LDFLAGS)
svg2png_LDADD  = $(top_builddir)/src/libcairo.la $(CAIRO_LDADD) $(LIBRSVG_LIBS)
endif

if CAIRO_HAS_SPECTRE
check_PROGRAMS += ps2png
ps2png_CFLAGS = $(LIBSPECTRE_CFLAGS)
# add LDADD, so ps2png uses "our" cairo
ps2png_LDFLAGS = $(CAIRO_TEST_UNDEFINED_LDFLAGS)
ps2png_LDADD  = $(top_builddir)/src/libcairo.la $(CAIRO_LDADD) $(LIBSPECTRE_LIBS)
endif

EXTRA_PROGRAMS += $(TESTS)

# Do a funny transition of CAIRO_TEST_TARGET through TARGETS such that
# one can limit tested targets both through CAIRO_TEST_TARGET env var
# and TARGETS make var on the command line.  Same for the rest.
TARGETS = $(CAIRO_TEST_TARGET)
TARGETS_EXCLUDE = $(CAIRO_TEST_TARGET_EXCLUDE)
NUM_THREADS = $(CAIRO_TEST_NUM_THREADS)
MODE = $(CAIRO_TEST_MODE)

# Same about ENV vs CAIRO_TEST_ENV.  ENV is used with "make run" only
ENV = $(CAIRO_TEST_ENV)

TESTS_ENVIRONMENT = CAIRO_TEST_MODE="$(MODE)" CAIRO_TEST_TARGET="$(TARGETS)" CAIRO_TEST_TARGET_EXCLUDE="$(TARGETS_EXCLUDE)" CAIRO_TEST_NUM_THREADS="$(NUM_THREADS)" $(ENV)

EXTRA_VALGRIND_FLAGS = $(CAIRO_EXTRA_VALGRIND_FLAGS)
VALGRIND_FLAGS = \
	--tool=memcheck --suppressions=$(srcdir)/.valgrind-suppressions \
	--leak-check=yes --show-reachable=yes $(EXTRA_VALGRIND_FLAGS)

CLEANFILES +=					\
	valgrind-log				\
	index.html				\
	ref.hash				\
	png-test.png				\
	create-for-stream.pdf			\
	create-for-stream.ps			\
	create-for-stream.svg			\
	svg-surface-source.svg			\
	pdf-surface-source.pdf			\
	ps-surface-source.ps			\
	pdf-features.pdf			\
	pdf-mime-data.out*			\
	ps-features.ps				\
	svg-clip.svg				\
	svg-surface.svg				\
	multi-page.pdf				\
	multi-page.ps				\
	$(NULL)

# This used to be a simple 'echo ${RM} *.ps *.pdf *.svg *.etc', but
# most systems cannot handle all of our clean files together.
# Then it became a fancy find using many GNU extensions, but then the ugly
# reality of portability was raised and it became....
clean-local: clean-caches
	rm -rf output
	-${FIND} . -name '*.out.*'    -print | ${XARGS} ${RM}
	-${FIND} . -name '*.log'      -print | ${XARGS} ${RM}
	-${FIND} . -name '*.[is]'     -print | ${XARGS} ${RM}
clean-caches:
	-${FIND} . -name '*.pass.*'   -print | ${XARGS} ${RM}
	-${FIND} . -name '*.fail.*'   -print | ${XARGS} ${RM}

# The following definitions both should work.
#FAILED_TESTS = `grep -l '\<FAIL\>' $(test_sources:.c=.log) 2>/dev/null | sed -e 's/[.]log$$//' | xargs echo`
FAILED_TESTS = `grep -l '\<FAIL\>' $(test_sources:.c=.log) 2>/dev/null | tr '\n' ' ' | sed -e 's/[.]log  */ /g; s/^ //; s/ $$//'`

recheck = check CAIRO_TESTS="$(FAILED_TESTS)"

# Re-checks all failed tests, i.e. tests with a log file that has a failure
recheck:
	@echo Re-checking failed tests
	@$(MAKE) $(AM_MAKEFLAGS) $(recheck)

# Checks tests and creates index.html.
# Target doesn't fail if tests fail.
test:
	@$(MAKE) $(AM_MAKEFLAGS) check; \
	$(MAKE) $(AM_MAKEFLAGS) html

# Re-checks tests and creates index.html.
# Target doesn't fail if tests fail.
retest:
	@CAIRO_TESTS="$(FAILED_TESTS)"; \
	$(MAKE) $(AM_MAKEFLAGS) check; \
	$(MAKE) $(AM_MAKEFLAGS) html

# Make index.html with no dependency tracking.
html-local:
	@echo Creating index.html
	@perl $(srcdir)/make-html.pl $(test_sources:.c=.log) > index.html

# Make index.html with no dependency tracking, containing only the failed tests.
rehtml:
	@CAIRO_TESTS="$(FAILED_TESTS)" $(MAKE) $(AM_MAKEFLAGS) html

# Run tests under a tool specified by TOOL.  For example, make run TOOL=gdb
run:
	$(MAKE) $(AM_MAKEFLAGS) check TESTS_ENVIRONMENT='$(TESTS_ENVIRONMENT) $(top_builddir)/libtool --mode=execute env $(TOOL)'

# Check tests under valgrind.  Saves log to valgrind-log
check-valgrind:
	$(MAKE) $(AM_MAKEFLAGS) check TESTS_ENVIRONMENT='$(TESTS_ENVIRONMENT) $(top_builddir)/libtool --mode=execute valgrind $(VALGRIND_FLAGS)' 2>&1 | tee valgrind-log

%.log: %.c cairo-test-suite
	-./cairo-test-suite $(<:.c=)

NOLOG_TESTS_LOG = $(NOLOG_TESTS:=.log)

$(NOLOG_TESTS_LOG):
	@echo dummy > $@

index.html: $(srcdir)/make-html.pl $(test_sources:.c=.log)
	@echo Creating index.html
	@perl $^ > $@


# Identify identical reference images
check-ref-dups:
	@LANG=C; \
	( cd "$(scrdir)" && sha1sum *.ref.png | sort ) > ref.hash; \
	join ref.hash ref.hash | grep -v -E '( .*.ref.png).*\1' | cut -d' ' -f 1-2 | sort -u

# Not exactly the best script in the world...
check-ref-missing:
	@cd "$(srcdir)"; \
	REFS=`git ls-files "*.ref.png"`; \
	ret=true; \
	missing=""; \
	for i in $$REFS; do \
	    echo "" $(REFERENCE_IMAGES) "" | grep -sq " $$i " || missing="$$missing $$i" ; \
	done ; \
	if test -n "$$missing"; then \
		echo "*** Error: Sanity check failed"; \
		echo "Some reference files are not included in the distribution."; \
		echo "You probably need to add these to Makefile.am's REFERENCE_IMAGES."; \
		echo "Missing: $$missing"; \
		ret=false; \
	fi >&2; \
	if $$ret; then \
	    REFS=`ls *.ref.png`; \
	    for i in $$REFS; do \
		echo "" $(REFERENCE_IMAGES) "" | grep -sq " $$i " || missing="$$missing $$i" ; \
	    done ; \
	    if test -n "$$missing"; then \
		    echo "*** Error: Sanity check failed"; \
		    echo "Some local reference files are not included in the distribution or been added to git."; \
		    echo "You probably need to add these to git and Makefile.am's REFERENCE_IMAGES."; \
		    echo "Missing: $$missing"; \
		    ret=false; \
	    fi >&2; \
	fi >&2; \
	missing=""; \
	for i in $(REFERENCE_IMAGES); do \
	    echo "" $$REFS "" | grep -sq " $$i " || missing="$$missing $$i" ; \
	done ; \
	if test -n "$$missing"; then \
		echo "*** Error: Sanity check failed"; \
		echo "Some reference files included in the distribution do not exist"; \
		echo "or are not in git.  You probably want to add these to git first."; \
		echo "Missing: $$missing"; \
		ret=false; \
	fi >&2; \
	$$ret

release-verify-sane-tests: check-ref-missing

.PHONY: check-valgrind test recheck retest rehtml check-ref-dups check-ref-missing release-verify-sane-tests

EXTRA_DIST += Makefile.win32
