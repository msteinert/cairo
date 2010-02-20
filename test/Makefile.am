include $(top_srcdir)/build/Makefile.am.common

include $(top_srcdir)/test/Makefile.sources

SUBDIRS=pdiff .

# Then we have a collection of tests that are only run if certain
# features are compiled into cairo
if HAVE_PTHREAD
test_sources += $(pthread_test_sources)
endif

if CAIRO_HAS_FT_FONT
if CAIRO_HAS_FC_FONT
test_sources += $(ft_font_test_sources)
endif
endif

# Need to add quartz-surface-source
if CAIRO_HAS_QUARTZ_SURFACE
test_sources += $(quartz_surface_test_sources)
endif

if CAIRO_HAS_PDF_SURFACE
test_sources += $(pdf_surface_test_sources)
endif

if CAIRO_HAS_PS_SURFACE
test_sources += $(ps_surface_test_sources)
endif

if CAIRO_HAS_SVG_SURFACE
test_sources += $(svg_surface_test_sources)
endif

if CAIRO_HAS_TEST_SURFACES
test_sources += $(test_fallback16_surface_test_sources)
endif

if CAIRO_HAS_XCB_SURFACE
test_sources += $(xcb_surface_test_sources)
endif

if CAIRO_HAS_XLIB_SURFACE
test_sources += $(xlib_surface_test_sources)
endif

if CAIRO_HAS_XLIB_XRENDER_SURFACE
test_sources += $(xlib_xrender_surface_test_sources)
endif

if CAIRO_HAS_MULTI_PAGE_SURFACES
test_sources += $(multi_page_surface_test_sources)
endif

# Include fallback-resolution (once!) if we have any of the vector surfaces
if BUILD_ANY2PPM
if CAIRO_HAS_SVG_SURFACE
test = $(fallback_resolution_test_sources)
endif
if CAIRO_HAS_PDF_SURFACE
test = $(fallback_resolution_test_sources)
endif
if CAIRO_HAS_PS_SURFACE
test = $(fallback_resolution_test_sources)
endif
endif
test_sources += $(test)

TESTS += cairo-test-suite$(EXEEXT)

cairo-test-constructors.c: Makefile $(test_sources) make-cairo-test-constructors.sh
	(cd $(srcdir) && sh ./make-cairo-test-constructors.sh $(test_sources)) > $@

cairo_test_suite_SOURCES = 		\
	$(cairo_test_suite_sources)	\
	$(cairo_test_suite_headers)	\
	$(test_sources)			\
	cairo-test-constructors.c
cairo_test_suite_CFLAGS = $(AM_CFLAGS)
cairo_test_suite_LDADD = 					\
	$(top_builddir)/test/pdiff/libpdiff.la 			\
        $(top_builddir)/boilerplate/libcairoboilerplate.la	\
	$(top_builddir)/src/libcairo.la 			\
	$(CAIRO_LDADD)
cairo_test_suite_DEPENDENCIES = \
	$(top_builddir)/test/pdiff/libpdiff.la 			\
        $(top_builddir)/boilerplate/libcairoboilerplate.la	\
	$(top_builddir)/src/libcairo.la
if BUILD_ANY2PPM
cairo_test_suite_DEPENDENCIES += \
	any2ppm
endif
if HAVE_PTHREAD
cairo_test_suite_CFLAGS += -pthread
cairo_test_suite_LDADD += -lpthread
endif

if HAVE_SHM
EXTRA_PROGRAMS += cairo-test-trace
cairo_test_trace_SOURCES =		\
	cairo-test-trace.c		\
	buffer-diff.c			\
	buffer-diff.h
cairo_test_trace_CFLAGS = $(AM_CFLAGS)
cairo_test_trace_LDADD =		\
	$(top_builddir)/test/pdiff/libpdiff.la 			\
	$(top_builddir)/util/cairo-script/libcairo-script-interpreter.la \
        $(top_builddir)/boilerplate/libcairoboilerplate.la	\
	$(top_builddir)/src/libcairo.la 			\
	$(CAIRO_LDADD) \
	$(SHM_LIBS)
if HAVE_PTHREAD
cairo_test_trace_CFLAGS += -pthread
cairo_test_trace_LDADD += -lpthread
endif
cairo_test_trace_DEPENDENCIES = \
	$(top_builddir)/test/pdiff/libpdiff.la 			\
	$(top_builddir)/util/cairo-script/libcairo-script-interpreter.la \
        $(top_builddir)/boilerplate/libcairoboilerplate.la	\
	$(top_builddir)/src/libcairo.la
endif

BUILT_SOURCES += cairo-test-constructors.c
EXTRA_DIST += $(BUILT_SOURCES) $(noinst_SCRIPTS) COPYING make-cairo-test-constructors.sh
CLEANFILES += $(BUILT_SOURCES)

# All tests which have a reference image go here.
REFERENCE_IMAGES = \
	a1-image-sample.ref.png \
	a1-image-sample.gl.xfail.png \
	a1-mask.ref.png \
	a1-traps-sample.ref.png \
	a8-mask.ref.png \
	alpha-similar.gl.argb32.xfail.png \
	alpha-similar.gl.rgb24.xfail.png \
	alpha-similar.pdf.argb32.xfail.png \
	alpha-similar.pdf.rgb24.xfail.png \
	alpha-similar.ps.argb32.xfail.png \
	alpha-similar.ps.rgb24.xfail.png \
	alpha-similar.ref.png \
	alpha-similar.rgb24.ref.png \
	alpha-similar.svg.argb32.xfail.png \
	alpha-similar.svg.rgb24.xfail.png \
	big-line.ref.png \
	big-line.xlib.ref.png \
	bilevel-image.ref.png \
	bitmap-font.ref.png \
	bitmap-font.rgb24.ref.png \
	caps-joins-alpha.quartz.ref.png \
	caps-joins-alpha.ref.png \
	caps-joins-alpha.xlib.ref.png \
	caps-joins-curve.ps.ref.png \
	caps-joins-curve.ref.png \
	caps-joins-curve.xlib.ref.png \
	caps-joins.ps.ref.png \
	caps-joins.ref.png \
	caps-sub-paths.ref.png \
	caps.ps.ref.png \
	caps.ref.png \
	clear.argb32.ref.png \
	clear.rgb24.ref.png \
	clear.pdf.argb32.ref.png \
	clear.ps.argb32.ref.png \
	clear.svg12.argb32.xfail.png \
	clear.svg12.rgb24.xfail.png \
	clear-source.ref.png \
	clip-all.ref.png \
	clip-device-offset.argb32.ref.png \
	clip-device-offset.rgb24.ref.png \
	clip-disjoint.ref.png \
	clip-disjoint.xlib.ref.png \
	clip-empty.ref.png \
	clip-empty-save.ref.png \
	clip-fill.ref.png \
	clip-fill.ps.xfail.png \
	clip-fill.xlib.ref.png \
	clip-fill.xlib-fallback.ref.png \
	clip-fill-no-op.ref.png \
	clip-fill-rule-pixel-aligned.ref.png \
	clip-fill-rule-pixel-aligned.rgb24.ref.png \
	clip-fill-rule.pdf.argb32.ref.png \
	clip-fill-rule.ps2.argb32.ref.png \
	clip-fill-rule.ps2.rgb24.ref.png \
	clip-fill-rule.ps3.argb32.ref.png \
	clip-fill-rule.ps3.rgb24.ref.png \
	clip-fill-rule.ref.png \
	clip-fill-rule.rgb24.ref.png \
	clip-fill-rule.test-paginated.rgb24.ref.png \
	clip-fill-rule.xlib.rgb24.ref.png \
	clip-fill-unbounded.argb32.ref.png \
	clip-fill-unbounded.rgb24.ref.png \
	clip-fill-unbounded.svg12.argb32.xfail.png \
	clip-fill-unbounded.svg12.rgb24.xfail.png \
	clip-fill-unbounded.xlib.argb32.ref.png \
	clip-fill-unbounded.xlib.rgb24.ref.png \
	clip-fill-unbounded.xlib-fallback.rgb24.ref.png \
	clip-image.ref.png \
	clip-nesting.pdf.argb32.ref.png \
	clip-nesting.ps2.argb32.ref.png \
	clip-nesting.ps2.rgb24.ref.png \
	clip-nesting.ps3.argb32.ref.png \
	clip-nesting.ps3.rgb24.ref.png \
	clip-nesting.quartz.ref.png \
	clip-nesting.quartz.rgb24.ref.png \
	clip-nesting.ref.png \
	clip-nesting.rgb24.ref.png \
	clip-nesting.test-paginated.rgb24.ref.png \
	clip-nesting.xlib.rgb24.ref.png \
	clip-operator.gl.argb32.ref.png \
	clip-operator.pdf.argb32.ref.png \
	clip-operator.pdf.rgb24.ref.png \
	clip-operator.ps2.rgb24.ref.png \
	clip-operator.ps3.argb32.ref.png \
	clip-operator.ps3.ref.png \
	clip-operator.ps3.rgb24.ref.png \
	clip-operator.quartz.ref.png \
	clip-operator.quartz.rgb24.ref.png \
	clip-operator.ref.png \
	clip-operator.rgb24.ref.png \
	clip-operator.svg12.argb32.xfail.png \
	clip-operator.svg12.rgb24.xfail.png \
	clip-operator.test-paginated.argb32.ref.png \
	clip-operator.xlib-fallback.ref.png \
	clip-operator.xlib.ref.png \
	clip-operator.xlib.rgb24.ref.png \
	clip-unbounded.ref.png \
	clip-unbounded.rgb24.ref.png \
	clip-unbounded.svg12.rgb24.xfail.png \
	clipped-surface.ref.png \
	clip-push-group.pdf.ref.png \
	clip-push-group.ps2.argb32.ref.png \
	clip-push-group.ps2.rgb24.ref.png \
	clip-push-group.ps3.argb32.ref.png \
	clip-push-group.ps3.rgb24.ref.png \
	clip-push-group.quartz.ref.png \
	clip-push-group.ref.png \
	clip-push-group.svg.ref.png \
	clip-push-group.xlib.ref.png \
	clip-stroke.ref.png \
	clip-stroke.xlib.ref.png \
	clip-stroke.xlib-fallback.ref.png \
	clip-stroke-no-op.ref.png \
	clip-stroke-unbounded.argb32.ref.png \
	clip-stroke-unbounded.rgb24.ref.png \
	clip-stroke-unbounded.svg12.argb32.xfail.png \
	clip-stroke-unbounded.svg12.rgb24.xfail.png \
	clip-stroke-unbounded.xlib.argb32.ref.png \
	clip-stroke-unbounded.xlib.rgb24.ref.png \
	clip-stroke-unbounded.xlib-fallback.rgb24.ref.png \
	clip-text.ref.png \
	clip-text.ps.xfail.png \
	clip-text.svg.ref.png \
	clip-text.xlib.ref.png \
	clip-twice.pdf.argb32.ref.png \
	clip-twice.ps2.argb32.ref.png \
	clip-twice.ps2.rgb24.ref.png \
	clip-twice.ps3.argb32.ref.png \
	clip-twice.ps3.rgb24.ref.png \
	clip-twice.quartz.ref.png \
	clip-twice.quartz.rgb24.ref.png \
	clip-twice.ref.png \
	clip-twice.rgb24.ref.png \
	clip-twice.test-paginated.argb32.ref.png \
	clip-twice.test-paginated.rgb24.ref.png \
	clip-twice.xlib.ref.png \
	clip-twice.xlib.rgb24.ref.png \
	clip-twice-rectangle.ref.png \
	clipped-group.pdf.ref.png \
	clipped-group.ps2.ref.png \
	clipped-group.ps3.ref.png \
	clipped-group.svg.ref.png \
	clipped-group.ref.png \
	clipped-surface.ref.png \
	clipped-trapezoids.ref.png \
	close-path-current-point.ps.ref.png \
	close-path-current-point.ref.png \
	close-path.ps2.ref.png \
	close-path.ps3.ref.png \
	close-path.ref.png \
	composite-integer-translate-over-repeat.ps2.ref.png \
	composite-integer-translate-over-repeat.ps3.ref.png \
	composite-integer-translate-over-repeat.ref.png \
	composite-integer-translate-over.ps2.ref.png \
	composite-integer-translate-over.ps3.ref.png \
	composite-integer-translate-over.ref.png \
	composite-integer-translate-source.ps2.ref.png \
	composite-integer-translate-source.ps3.ref.png \
	composite-integer-translate-source.ref.png \
	composite-integer-translate-source.svg12.argb32.xfail.png \
	composite-integer-translate-source.svg12.rgb24.xfail.png \
	copy-path.ps2.ref.png \
	copy-path.ps3.ref.png \
	copy-path.ref.png \
	create-from-png-stream.ref.png \
	create-from-png.alpha.ref.png \
	create-from-png.gray-alpha.ref.png \
	create-from-png.gray.ref.png \
	create-from-png.indexed-alpha.ref.png \
	create-from-png.indexed.ref.png \
	create-from-png.ref.png \
	culled-glyphs.ps.ref.png \
	culled-glyphs.ref.png \
	curve-to-as-line-to.ps.xfail.png \
	curve-to-as-line-to.ref.png \
	dash-caps-joins.ps2.argb32.ref.png \
	dash-caps-joins.ps2.rgb24.ref.png \
	dash-caps-joins.ps3.argb32.ref.png \
	dash-caps-joins.ps3.rgb24.ref.png \
	dash-caps-joins.quartz.ref.png \
	dash-caps-joins.ref.png \
	dash-curve.ps2.ref.png \
	dash-curve.ps3.ref.png \
	dash-curve.quartz.ref.png \
	dash-curve.ref.png \
	dash-curve.xlib.ref.png \
	dash-infinite-loop.ref.png \
	dash-no-dash.ref.png \
	dash-offset.ref.png \
	dash-offset-negative.ref.png \
	dash-scale.ps2.argb32.ref.png \
	dash-scale.ps2.rgb24.ref.png \
	dash-scale.ps3.argb32.ref.png \
	dash-scale.ps3.rgb24.ref.png \
	dash-scale.quartz.ref.png \
	dash-scale.ref.png \
	dash-state.ps2.ref.png \
	dash-state.ps3.ref.png \
	dash-state.quartz.ref.png \
	dash-state.ref.png \
	dash-zero-length.ps2.ref.png \
	dash-zero-length.ps2.rgb24.ref.png \
	dash-zero-length.ps3.ref.png \
	dash-zero-length.ps3.rgb24.ref.png \
	dash-zero-length.ref.png \
	dash-zero-length.rgb24.ref.png \
	degenerate-arc.ps2.ref.png \
	degenerate-arc.ps3.ref.png \
	degenerate-arc.ref.png \
	degenerate-arc.xlib.ref.png \
	degenerate-arcs.ref.png \
	degenerate-curve-to.ref.png \
	degenerate-curve-to.ps.xfail.png \
	degenerate-dash.ps.xfail.png \
	degenerate-dash.ref.png \
	degenerate-dash.xlib.ref.png \
	degenerate-path.ps.argb32.xfail.png \
	degenerate-path.ps.rgb24.xfail.png \
	degenerate-path.quartz.ref.png \
	degenerate-path.quartz.rgb24.ref.png \
	degenerate-path.argb32.ref.png \
	degenerate-path.rgb24.ref.png \
	degenerate-pen.ps2.ref.png \
	degenerate-pen.ps3.ref.png \
	degenerate-pen.quartz.ref.png \
	degenerate-pen.ref.png \
	degenerate-pen.xlib.ref.png \
	degenerate-rel-curve-to.ref.png \
	degenerate-rel-curve-to.ps.xfail.png \
	device-offset-fractional.gl.xfail.png \
	device-offset-fractional.pdf.xfail.png \
	device-offset-fractional.ps2.ref.png \
	device-offset-fractional.ps3.ref.png \
	device-offset-fractional.ref.png \
	device-offset-positive.ref.png \
	device-offset-positive.rgb24.ref.png \
	device-offset-scale.ref.png \
	device-offset-scale.svg.xfail.png \
	device-offset.ref.png \
	device-offset.rgb24.ref.png \
	extended-blend.argb32.ref.png \
	extended-blend.rgb24.ref.png \
	extended-blend.svg12.argb32.xfail.png \
	extended-blend.svg12.rgb24.xfail.png \
	extended-blend-alpha.argb32.ref.png \
	extended-blend-alpha.rgb24.ref.png \
	extended-blend-alpha.svg12.argb32.xfail.png \
	extended-blend-alpha.svg12.rgb24.xfail.png \
	extend-pad-border.ps.ref.png \
	extend-pad-border.ref.png \
	extend-pad-border.svg.xfail.png \
	extend-pad-similar.ref.png \
	extend-pad-similar.svg.xfail.png \
	extend-pad.ps.ref.png \
	extend-pad.ref.png \
	extend-pad.svg.xfail.png \
	extend-reflect-similar.ps2.ref.png \
	extend-reflect-similar.ps3.ref.png \
	extend-reflect-similar.ref.png \
	extend-reflect.ps2.ref.png \
	extend-reflect.ps3.ref.png \
	extend-reflect.ref.png \
	extend-repeat-similar.ps2.ref.png \
	extend-repeat-similar.ps3.ref.png \
	extend-repeat-similar.ref.png \
	extend-repeat.ps2.ref.png \
	extend-repeat.ps3.ref.png \
	extend-repeat.ref.png \
	fallback-resolution.ppi150x150.ref.png \
	fallback-resolution.ppi150x72.ref.png \
	fallback-resolution.ppi300x300.ref.png \
	fallback-resolution.ppi300x72.ref.png \
	fallback-resolution.ppi37.5x37.5.ref.png \
	fallback-resolution.ppi37.5x72.ref.png \
	fallback-resolution.ppi600x600.ref.png \
	fallback-resolution.ppi600x72.ref.png \
	fallback-resolution.ppi72x150.ref.png \
	fallback-resolution.ppi72x300.ref.png \
	fallback-resolution.ppi72x37.5.ref.png \
	fallback-resolution.ppi72x600.ref.png \
	fallback-resolution.ppi72x72.ref.png \
	fallback-resolution.ppi72x75.ref.png \
	fallback-resolution.ppi75x72.ref.png \
	fallback-resolution.ppi75x75.ref.png \
	fill-alpha-pattern.ps3.argb32.ref.png \
	fill-alpha-pattern.ps3.ref.png \
	fill-alpha-pattern.ps3.rgb24.ref.png \
	fill-alpha-pattern.ref.png \
	fill-alpha-pattern.xlib.ref.png \
	fill-alpha.ref.png \
	fill-alpha.xlib.ref.png \
	fill-and-stroke-alpha-add.quartz.ref.png \
	fill-and-stroke-alpha-add.ref.png \
	fill-and-stroke-alpha-add.svg12.xfail.png \
	fill-and-stroke-alpha.quartz.ref.png \
	fill-and-stroke-alpha.ref.png \
	fill-and-stroke.ps2.argb32.ref.png \
	fill-and-stroke.ps2.rgb24.ref.png \
	fill-and-stroke.ps3.argb32.ref.png \
	fill-and-stroke.ps3.rgb24.ref.png \
	fill-and-stroke.quartz.ref.png \
	fill-and-stroke.quartz.rgb24.ref.png \
	fill-and-stroke.argb32.ref.png \
	fill-and-stroke.rgb24.ref.png \
	fill-and-stroke.xlib.argb32.ref.png \
	fill-and-stroke.xlib.rgb24.ref.png \
	fill-degenerate-sort-order.ps.argb32.xfail.png \
	fill-degenerate-sort-order.ps.rgb24.xfail.png \
	fill-degenerate-sort-order.quartz.ref.png \
	fill-degenerate-sort-order.quartz.rgb24.ref.png \
	fill-degenerate-sort-order.ref.png \
	fill-degenerate-sort-order.rgb24.ref.png \
	fill-degenerate-sort-order.xlib.ref.png \
	fill-degenerate-sort-order.xlib.rgb24.ref.png \
	fill-empty.argb32.ref.png \
	fill-empty.rgb24.ref.png \
	fill-empty.svg12.rgb24.xfail.png \
	fill-image.ps.ref.png \
	fill-image.ref.png \
	fill-image.xlib.ref.png \
	fill-missed-stop.pdf.argb32.ref.png \
	fill-missed-stop.ps2.argb32.ref.png \
	fill-missed-stop.ps2.rgb24.ref.png \
	fill-missed-stop.ps3.argb32.ref.png \
	fill-missed-stop.ps3.rgb24.ref.png \
	fill-missed-stop.ref.png \
	fill-missed-stop.rgb24.ref.png \
	fill-rule.ps2.argb32.ref.png \
	fill-rule.ps2.rgb24.ref.png \
	fill-rule.ps3.argb32.ref.png \
	fill-rule.ps3.rgb24.ref.png \
	fill-rule.quartz.ref.png \
	fill-rule.quartz.rgb24.ref.png \
	fill-rule.ref.png \
	fill-rule.rgb24.ref.png \
	fill-rule.xlib.ref.png \
	fill-rule.xlib.rgb24.ref.png \
	filter-bilinear-extents.pdf.xfail.png \
	filter-bilinear-extents.ps2.ref.png \
	filter-bilinear-extents.ps3.ref.png \
	filter-bilinear-extents.ref.png \
	filter-nearest-offset.gl.xfail.png \
	filter-nearest-offset.pdf.xfail.png \
	filter-nearest-offset.ps2.ref.png \
	filter-nearest-offset.ps3.ref.png \
	filter-nearest-offset.ref.png \
	filter-nearest-offset.svg.xfail.png \
	filter-nearest-transformed.gl.xfail.png \
	filter-nearest-transformed.pdf.xfail.png \
	filter-nearest-transformed.ref.png \
	filter-nearest-transformed.svg.xfail.png \
	finer-grained-fallbacks.gl.argb32.ref.png \
	finer-grained-fallbacks.ps2.argb32.ref.png \
	finer-grained-fallbacks.ps2.ref.png \
	finer-grained-fallbacks.ps2.rgb24.ref.png \
	finer-grained-fallbacks.ps3.argb32.ref.png \
	finer-grained-fallbacks.ps3.ref.png \
	finer-grained-fallbacks.ps3.rgb24.ref.png \
	finer-grained-fallbacks.ref.png \
	finer-grained-fallbacks.rgb24.ref.png \
	finer-grained-fallbacks.svg12.argb32.ref.png \
	finer-grained-fallbacks.svg12.rgb24.ref.png \
	finer-grained-fallbacks.xlib.ref.png \
	finer-grained-fallbacks.xlib.rgb24.ref.png \
	font-matrix-translation.ps2.argb32.ref.png \
	font-matrix-translation.ps2.rgb24.ref.png \
	font-matrix-translation.ps3.argb32.ref.png \
	font-matrix-translation.ps3.rgb24.ref.png \
	font-matrix-translation.quartz.ref.png \
	font-matrix-translation.ref.png \
	font-matrix-translation.svg.ref.png \
	ft-show-glyphs-positioning.pdf.ref.png \
	ft-show-glyphs-positioning.ps2.ref.png \
	ft-show-glyphs-positioning.ps3.ref.png \
	ft-show-glyphs-positioning.ref.png \
	ft-show-glyphs-positioning.svg.ref.png \
	ft-show-glyphs-table.ps2.ref.png \
	ft-show-glyphs-table.ps3.ref.png \
	ft-show-glyphs-table.ref.png \
	ft-show-glyphs-table.svg.ref.png \
	ft-text-antialias-none.ps2.argb32.ref.png \
	ft-text-antialias-none.ps3.argb32.ref.png \
	ft-text-antialias-none.ref.png \
	ft-text-vertical-layout-type1.pdf.ref.png \
	ft-text-vertical-layout-type1.ps.ref.png \
	ft-text-vertical-layout-type1.ref.png \
	ft-text-vertical-layout-type1.svg.ref.png \
	ft-text-vertical-layout-type1.xlib.ref.png \
	ft-text-vertical-layout-type3.pdf.ref.png \
	ft-text-vertical-layout-type3.ps.ref.png \
	ft-text-vertical-layout-type3.ref.png \
	ft-text-vertical-layout-type3.svg.ref.png \
	ft-text-vertical-layout-type3.xlib.ref.png \
	get-group-target.ref.png \
	glyph-cache-pressure.ps2.ref.png \
	glyph-cache-pressure.ps3.ref.png \
	glyph-cache-pressure.quartz.ref.png \
	glyph-cache-pressure.ref.png \
	gradient-alpha.ps2.argb32.ref.png \
	gradient-alpha.ps2.rgb24.ref.png \
	gradient-alpha.ps3.argb32.ref.png \
	gradient-alpha.ps3.rgb24.ref.png \
	gradient-alpha.ref.png \
	gradient-alpha.rgb24.ref.png \
	gradient-constant-alpha.ps3.ref.png \
	gradient-constant-alpha.ps3.rgb24.ref.png \
	gradient-constant-alpha.ref.png \
	gradient-constant-alpha.rgb24.ref.png \
	gradient-zero-stops.ref.png \
	gradient-zero-stops.rgb24.ref.png \
	group-clip.ref.png \
	group-paint.ref.png \
	group-unaligned.pdf.new.png \
	group-unaligned.ps.ref.png \
	group-unaligned.ref.png \
	group-unaligned.svg.argb32.xfail.png \
	group-unaligned.svg.rgb24.xfail.png \
	group-unaligned.xlib-fallback.ref.png \
	group-unaligned.xlib.ref.png \
	huge-linear.pdf.ref.png \
	huge-linear.ps3.ref.png \
	huge-linear.ref.png \
	huge-radial.pdf.argb32.ref.png \
	huge-radial.pdf.rgb24.ref.png \
	huge-radial.ps3.ref.png \
	huge-radial.ref.png \
	image-surface-source.ps2.ref.png \
	image-surface-source.ps3.ref.png \
	image-surface-source.rgb24.ref.png \
	image-surface-source.argb32.ref.png \
	image-surface-source.svg12.argb32.xfail.png \
	image-surface-source.svg12.rgb24.xfail.png \
	implicit-close.ref.png \
	infinite-join.ps2.ref.png \
	infinite-join.ps3.ref.png \
	infinite-join.ref.png \
	joins.ps.ref.png \
	joins.ref.png \
	large-clip.ref.png \
	large-font.ref.png \
	large-source.ref.png \
	large-source-roi.ref.png \
	large-twin-antialias-mixed.ref.png \
	large-twin-antialias-mixed.xlib.ref.png \
	leaky-dash.ps2.argb32.ref.png \
	leaky-dash.ps2.rgb24.ref.png \
	leaky-dash.ps3.argb32.ref.png \
	leaky-dash.ps3.rgb24.ref.png \
	leaky-dash.quartz.ref.png \
	leaky-dash.ref.png \
	leaky-dashed-rectangle.pdf.ref.png \
	leaky-dashed-rectangle.ps.ref.png \
	leaky-dashed-rectangle.ref.png \
	leaky-dashed-rectangle.xlib.ref.png \
	leaky-dashed-stroke.ps2.ref.png \
	leaky-dashed-stroke.ps3.ref.png \
	leaky-dashed-stroke.ref.png \
	leaky-dashed-stroke.xlib.ref.png \
	leaky-polygon.ps2.ref.png \
	leaky-polygon.ps3.ref.png \
	leaky-polygon.ref.png \
	line-width-scale.ps2.ref.png \
	line-width-scale.ps3.ref.png \
	line-width-scale.quartz.ref.png \
	line-width-scale.ref.png \
	line-width.ref.png \
	linear-gradient-reflect.pdf.argb32.ref.png \
	linear-gradient-reflect.pdf.rgb24.ref.png \
	linear-gradient-reflect.ps3.ref.png \
	linear-gradient-reflect.quartz.ref.png \
	linear-gradient-reflect.ref.png \
	linear-gradient.ps3.ref.png \
	linear-gradient.quartz.ref.png \
	linear-gradient.ref.png \
	linear-gradient.xlib.ref.png \
	linear-gradient-subset.ref.png \
	linear-step-function.xfail.png \
	linear-uniform.ref.png \
	long-dashed-lines.ps2.ref.png \
	long-dashed-lines.ps3.ref.png \
	long-dashed-lines.quartz.ref.png \
	long-dashed-lines.ref.png \
	mask-alpha.quartz.argb32.ref.png \
	mask-alpha.ref.png \
	mask-alpha.rgb24.ref.png \
	mask-alpha.svg.rgb24.xfail.png \
	mask-alpha.xlib.ref.png \
	mask-alpha.xlib.rgb24.ref.png \
	mask-ctm.ref.png \
	mask-ctm.rgb24.ref.png \
	mask-glyphs.gl.ref.png \
	mask-glyphs.pdf.ref.png \
	mask-glyphs.ref.png \
	mask-glyphs.svg.ref.png \
	mask-surface-ctm.ref.png \
	mask-surface-ctm.rgb24.ref.png \
	mask-transformed-image.pdf.ref.png \
	mask-transformed-image.ref.png \
	mask-transformed-similar.pdf.ref.png \
	mask-transformed-similar.ref.png \
	mask-transformed-similar.svg.ref.png \
	mask.pdf.argb32.ref.png \
	mask.pdf.rgb24.ref.png \
	mask.quartz.ref.png \
	mask.quartz.rgb24.ref.png \
	mask.ref.png \
	mask.rgb24.ref.png \
	mask.svg.argb32.xfail.png \
	mask.svg.rgb24.xfail.png \
	mask.xlib.ref.png \
	mask.xlib.rgb24.ref.png \
	recording-surface-pattern.gl.argb32.ref.png \
	recording-surface-pattern.pdf.argb32.ref.png \
	recording-surface-pattern.pdf.rgb24.ref.png \
	recording-surface-pattern.ps2.argb32.ref.png \
	recording-surface-pattern.ps2.rgb24.ref.png \
	recording-surface-pattern.ps3.argb32.ref.png \
	recording-surface-pattern.ps3.rgb24.ref.png \
	recording-surface-pattern.quartz.ref.png \
	recording-surface-pattern.quartz.rgb24.ref.png \
	recording-surface-pattern.ref.png \
	recording-surface-pattern.rgb24.ref.png \
	recording-surface-pattern.svg.argb32.ref.png \
	recording-surface-pattern.svg.rgb24.ref.png \
	mime-data.pdf.ref.png \
	mime-data.ps.ref.png \
	mime-data.ref.png \
	mime-data.script.ref.png \
	mime-data.svg.ref.png \
	miter-precision.ps2.ref.png \
	miter-precision.ps3.ref.png \
	miter-precision.ref.png \
	move-to-show-surface.ref.png \
	new-sub-path.pdf.argb32.ref.png \
	new-sub-path.ps2.argb32.ref.png \
	new-sub-path.ps2.rgb24.ref.png \
	new-sub-path.ps3.argb32.ref.png \
	new-sub-path.ps3.rgb24.ref.png \
	new-sub-path.quartz.ref.png \
	new-sub-path.quartz.rgb24.ref.png \
	new-sub-path.argb32.ref.png \
	new-sub-path.rgb24.ref.png \
	nil-surface.ref.png \
	nil-surface.rgb24.ref.png \
	operator-alpha.ref.png \
	operator-alpha.rgb24.ref.png \
	operator-alpha.svg12.argb32.xfail.png \
	operator-alpha.svg12.rgb24.xfail.png \
	operator-alpha-alpha.ref.png \
	operator-alpha-alpha.pdf.xfail.png \
	operator-alpha-alpha.ps.xfail.png \
	operator-alpha-alpha.svg.xfail.png \
	operator-clear.ps2.argb32.ref.png \
	operator-clear.ps3.argb32.ref.png \
	operator-clear.quartz.ref.png \
	operator-clear.quartz.rgb24.ref.png \
	operator-clear.ref.png \
	operator-clear.rgb24.ref.png \
	operator-clear.svg12.argb32.xfail.png \
	operator-clear.svg12.rgb24.xfail.png \
	operator-clear.xlib.argb32.ref.png \
	operator-clear.xlib.rgb24.ref.png \
	operator-source.pdf.rgb24.ref.png \
	operator-source.quartz.ref.png \
	operator-source.quartz.rgb24.ref.png \
	operator-source.ref.png \
	operator-source.rgb24.ref.png \
	operator-source.svg12.argb32.xfail.png \
	operator-source.svg12.rgb24.xfail.png \
	operator-source.xlib-fallback.ref.png \
	operator-source.xlib.ref.png \
	operator-source.xlib.rgb24.ref.png \
	operator.ref.png \
	operator.rgb24.ref.png \
	operator.svg12.argb32.xfail.png \
	operator.svg12.rgb24.xfail.png \
	over-above-source.ps2.argb32.ref.png \
	over-above-source.ps3.argb32.ref.png \
	over-above-source.quartz.ref.png \
	over-above-source.quartz.rgb24.ref.png \
	over-above-source.ref.png \
	over-above-source.rgb24.ref.png \
	over-above-source.svg12.rgb24.xfail.png \
	over-above-source.xlib.ref.png \
	over-above-source.xlib.rgb24.ref.png \
	over-around-source.pdf.argb32.ref.png \
	over-around-source.ps2.argb32.ref.png \
	over-around-source.ps2.rgb24.ref.png \
	over-around-source.ps3.argb32.ref.png \
	over-around-source.ps3.rgb24.ref.png \
	over-around-source.quartz.ref.png \
	over-around-source.quartz.rgb24.ref.png \
	over-around-source.ref.png \
	over-around-source.rgb24.ref.png \
	over-around-source.svg12.argb32.xfail.png \
	over-around-source.svg12.rgb24.xfail.png \
	over-around-source.xlib.ref.png \
	over-around-source.xlib.rgb24.ref.png \
	over-below-source.pdf.argb32.ref.png \
	over-below-source.ps2.argb32.ref.png \
	over-below-source.ps3.argb32.ref.png \
	over-below-source.ref.png \
	over-below-source.rgb24.ref.png \
	over-below-source.svg12.argb32.xfail.png \
	over-below-source.svg12.rgb24.xfail.png \
	over-between-source.ps2.argb32.ref.png \
	over-between-source.ps3.argb32.ref.png \
	over-between-source.quartz.ref.png \
	over-between-source.quartz.rgb24.ref.png \
	over-between-source.ref.png \
	over-between-source.rgb24.ref.png \
	over-between-source.svg12.argb32.xfail.png \
	over-between-source.svg12.rgb24.xfail.png \
	over-between-source.xlib.ref.png \
	over-between-source.xlib.rgb24.ref.png \
	overlapping-glyphs.argb32.ref.png \
	overlapping-glyphs.rgb24.ref.png \
	overlapping-glyphs.pdf.argb32.xfail.png \
	overlapping-glyphs.pdf.rgb24.xfail.png \
	overlapping-glyphs.svg.rgb24.ref.png \
	overlapping-glyphs.svg.argb32.ref.png \
	paint-repeat.ref.png \
	paint-source-alpha.ref.png \
	paint-source-alpha.svg.ref.png \
	paint-with-alpha.ref.png \
	paint-with-alpha.svg.ref.png \
	paint.ref.png \
	pass-through.ref.png \
	pass-through.rgb24.ref.png \
	path-append.ps.ref.png \
	path-append.ref.png \
	path-stroke-twice.ref.png \
	path-stroke-twice.ps.ref.png \
	path-append.test-fallback.ref.png \
	path-append.xlib-fallback.ref.png \
	path-append.xlib.ref.png \
	pattern-getters.ref.png \
	pdf-surface-source.rgb24.ref.png \
	pdf-surface-source.argb32.ref.png \
	pdf-surface-source.svg12.argb32.xfail.png \
	pdf-surface-source.svg12.rgb24.xfail.png \
	pixman-rotate.ref.png \
	pixman-rotate.rgb24.ref.png \
	pthread-same-source.ref.png \
	pthread-similar.ref.png \
	ps-surface-source.rgb24.ref.png \
	ps-surface-source.argb32.ref.png \
	ps-surface-source.svg12.argb32.xfail.png \
	ps-surface-source.svg12.rgb24.xfail.png \
	push-group.ref.png \
	push-group.rgb24.ref.png \
	push-group.xlib.ref.png \
	push-group.xlib.rgb24.ref.png \
	push-group-color.ref.png \
	push-group-color.xlib.ref.png \
	quartz-surface-source.rgb24.ref.png \
	quartz-surface-source.argb32.ref.png \
	quartz-surface-source.ps2.ref.png \
	quartz-surface-source.ps3.ref.png \
	radial-gradient.pdf.ref.png \
	radial-gradient.quartz.ref.png \
	radial-gradient.ref.png \
	radial-gradient.svg.xfail.png \
	radial-gradient-source.argb32.ref.png \
	radial-gradient-source.rgb24.ref.png \
	random-intersections-eo.ps.ref.png \
	random-intersections-eo.quartz.ref.png \
	random-intersections-eo.ref.png \
	random-intersections-eo.xlib.ref.png \
	random-intersections-nonzero.ref.png \
	random-intersections-nonzero.ps.ref.png \
	random-intersections-nonzero.xlib.ref.png \
	random-intersections-curves-eo.ps.ref.png \
	random-intersections-curves-eo.pdf.ref.png \
	random-intersections-curves-eo.ref.png \
	random-intersections-curves-eo.xlib.ref.png \
	random-intersections-curves-eo.xlib-fallback.ref.png \
	random-intersections-curves-nz.ps.ref.png \
	random-intersections-curves-nz.pdf.ref.png \
	random-intersections-curves-nz.ref.png \
	random-intersections-curves-nz.xlib.ref.png \
	random-intersections-curves-nz.xlib-fallback.ref.png \
	rectangle-rounding-error.ref.png \
	rectilinear-dash.ref.png \
	rectilinear-fill.ref.png \
	rectilinear-miter-limit.ps2.ref.png \
	rectilinear-miter-limit.ps3.ref.png \
	rectilinear-miter-limit.ref.png \
	rectilinear-stroke.ref.png \
	reflected-stroke.ps2.ref.png \
	reflected-stroke.ps3.ref.png \
	reflected-stroke.quartz.ref.png \
	reflected-stroke.ref.png \
	rel-path.ps2.rgb24.ref.png \
	rel-path.ps3.rgb24.ref.png \
	rel-path.quartz.ref.png \
	rel-path.quartz.rgb24.ref.png \
	rel-path.ref.png \
	rel-path.rgb24.ref.png \
	rgb24-ignore-alpha.ref.png \
	rotate-image-surface-paint.pdf.xfail.png \
	rotate-image-surface-paint.ps.ref.png \
	rotate-image-surface-paint.quartz.ref.png \
	rotate-image-surface-paint.ref.png \
	rotate-image-surface-paint.svg.xfail.png \
	rotate-image-surface-paint.svg.ref.png \
	rotated-clip.ref.png \
	rotated-clip.ps.ref.png \
	rotated-clip.xlib.ref.png \
	rounded-rectangle-fill.xlib.ref.png \
	rounded-rectangle-stroke.xlib.ref.png \
	scale-down-source-surface-paint.ref.png \
	scale-offset-image.gl.ref.png \
	scale-offset-image.pdf.argb32.ref.png \
	scale-offset-image.pdf.rgb24.ref.png \
	scale-offset-image.ps.ref.png \
	scale-offset-image.ref.png \
	scale-offset-image.xfail.png \
	scale-offset-image.script.xfail.png \
	scale-offset-image.xlib.xfail.png \
	scale-offset-image.xlib-fallback.xfail.png \
	scale-offset-similar.gl.ref.png \
	scale-offset-similar.pdf.argb32.ref.png \
	scale-offset-similar.pdf.rgb24.ref.png \
	scale-offset-similar.ps.ref.png \
	scale-offset-similar.ref.png \
	scale-offset-similar.xfail.png \
	scale-offset-similar.recording.xfail.png \
	scale-offset-similar.script.xfail.png \
	scale-offset-similar.xlib.xfail.png \
	scale-offset-similar.xlib-fallback.xfail.png \
	scale-source-surface-paint.ref.png \
	scale-source-surface-paint.rgb24.ref.png \
	scale-source-surface-paint.svg.argb32.xfail.png \
	scale-source-surface-paint.svg.rgb24.xfail.png \
	select-font-face.ps2.ref.png \
	select-font-face.ps3.ref.png \
	select-font-face.quartz.ref.png \
	select-font-face.ref.png \
	self-copy.ps2.ref.png \
	self-copy.ps3.ref.png \
	self-copy.ref.png \
	self-intersecting.ps.ref.png \
	self-intersecting.ref.png \
	self-intersecting.xlib.ref.png \
	set-source.ref.png \
	set-source.rgb24.ref.png \
	show-glyphs-many.ref.png \
	show-text-current-point.ps2.ref.png \
	show-text-current-point.ps3.ref.png \
	show-text-current-point.quartz.ref.png \
	show-text-current-point.ref.png \
	skew-extreme.ps2.ref.png \
	skew-extreme.ps3.ref.png \
	skew-extreme.ref.png \
	smask-fill.ref.png \
	smask-fill.svg.ref.png \
	smask-fill.xlib.ref.png \
	smask-image-mask.ref.png \
	smask-mask.pdf.xfail.png \
	smask-mask.ref.png \
	smask-mask.svg.ref.png \
	smask-paint.pdf.xfail.png \
	smask-paint.ref.png \
	smask-paint.svg.ref.png \
	smask-stroke.pdf.xfail.png \
	smask-stroke.ref.png \
	smask-stroke.xlib.ref.png \
	smask-text.pdf.ref.png \
	smask-text.ps2.ref.png \
	smask-text.ps3.ref.png \
	smask-text.ref.png \
	smask-text.svg.ref.png \
	smask-text.script.ref.png \
	smask-text.xlib.ref.png \
	smask.pdf.xfail.png \
	smask.ps.ref.png \
	smask.ref.png \
	smask.svg.ref.png \
	smask.script.ref.png \
	smask.xlib.ref.png \
	solid-pattern-cache-stress.ref.png \
	source-clip-scale.gl.ref.png \
	source-clip-scale.pdf.ref.png \
	source-clip-scale.ps2.argb32.ref.png \
	source-clip-scale.ps2.rgb24.ref.png \
	source-clip-scale.ps3.argb32.ref.png \
	source-clip-scale.ps3.rgb24.ref.png \
	source-clip-scale.quartz.ref.png \
	source-clip-scale.ref.png \
	source-clip-scale.svg.ref.png \
	source-clip.ref.png \
	source-surface-scale-paint.ref.png \
	source-surface-scale-paint.rgb24.ref.png \
	spline-decomposition.pdf.ref.png \
	spline-decomposition.ps.ref.png \
	spline-decomposition.ref.png \
	spline-decomposition.svg.ref.png \
	spline-decomposition.xlib.ref.png \
	stroke-ctm-caps.ps2.ref.png \
	stroke-ctm-caps.ps3.ref.png \
	stroke-ctm-caps.quartz.ref.png \
	stroke-ctm-caps.ref.png \
	stroke-image.pdf.ref.png \
	stroke-image.ps.ref.png \
	stroke-image.quartz.ref.png \
	stroke-image.ref.png \
	stroke-image.xlib.ref.png \
	subsurface.ref.png \
	subsurface-repeat.ref.png \
	subsurface-similar-repeat.ref.png \
	surface-pattern-big-scale-down.ref.png \
	surface-pattern-big-scale-down.ps.xfail.png \
	surface-pattern-operator.argb32.ref.png \
	surface-pattern-operator.rgb24.ref.png \
	surface-pattern-operator.xlib.argb32.ref.png \
	surface-pattern-operator.xlib.rgb24.ref.png \
	surface-pattern-scale-down.pdf.ref.png \
	surface-pattern-scale-down.ps2.ref.png \
	surface-pattern-scale-down.ps3.ref.png \
	surface-pattern-scale-down.quartz.ref.png \
	surface-pattern-scale-down.ref.png \
	surface-pattern-scale-up.pdf.ref.png \
	surface-pattern-scale-up.ps2.ref.png \
	surface-pattern-scale-up.ps3.ref.png \
	surface-pattern-scale-up.ref.png \
	surface-pattern.pdf.xfail.png \
	surface-pattern.ps.xfail.png \
	surface-pattern.ref.png \
	surface-pattern.svg.xfail.png \
	svg-surface-source.rgb24.ref.png \
	svg-surface-source.argb32.ref.png \
	svg-surface-source.svg12.argb32.xfail.png \
	svg-surface-source.svg12.rgb24.xfail.png \
	test-fallback16-surface-source.ps.ref.png \
	test-fallback16-surface-source.svg12.argb32.xfail.png \
	test-fallback16-surface-source.svg12.rgb24.xfail.png \
	text-antialias-gray.quartz.ref.png \
	text-antialias-gray.ref.png \
	text-antialias-none.quartz.ref.png \
	text-antialias-none.ref.png \
	text-antialias-subpixel.quartz.ref.png \
	text-antialias-subpixel.ref.png \
	text-glyph-range.ps2.ref.png \
	text-glyph-range.ps3.ref.png \
	text-glyph-range.ref.png \
	text-pattern.pdf.argb32.ref.png \
	text-pattern.pdf.rgb24.ref.png \
	text-pattern.ps3.argb32.ref.png \
	text-pattern.ps3.rgb24.ref.png \
	text-pattern.quartz.ref.png \
	text-pattern.quartz.rgb24.ref.png \
	text-pattern.ref.png \
	text-pattern.rgb24.ref.png \
	text-pattern.svg.argb32.ref.png \
	text-pattern.svg.rgb24.ref.png \
	text-rotate.pdf.ref.png \
	text-rotate.ps2.ref.png \
	text-rotate.ps3.ref.png \
	text-rotate.quartz.ref.png \
	text-rotate.ref.png \
	text-rotate.svg.ref.png \
	text-rotate.xlib.ref.png \
	text-transform.pdf.ref.png \
	text-transform.ps2.ref.png \
	text-transform.ps3.ref.png \
	text-transform.ref.png \
	text-transform.svg.ref.png \
	transforms.ps2.ref.png \
	transforms.ps3.ref.png \
	transforms.ref.png \
	translate-show-surface.ref.png \
	trap-clip.ps2.argb32.ref.png \
	trap-clip.ps2.rgb24.ref.png \
	trap-clip.ps3.argb32.ref.png \
	trap-clip.ps3.rgb24.ref.png \
	trap-clip.quartz.ref.png \
	trap-clip.quartz.rgb24.ref.png \
	trap-clip.ref.png \
	trap-clip.rgb24.ref.png \
	trap-clip.test-paginated.argb32.ref.png \
	trap-clip.xlib.ref.png \
	trap-clip.xlib.rgb24.ref.png \
	twin.ps.ref.png \
	twin.ref.png \
	twin.svg.ref.png \
	twin.xlib.ref.png \
	twin-antialias-gray.ref.png \
	twin-antialias-gray.xlib.ref.png \
	twin-antialias-none.ref.png \
	twin-antialias-none.xlib.ref.png \
	twin-antialias-mixed.ref.png \
	twin-antialias-mixed.xlib.ref.png \
	twin-antialias-subpixel.ref.png \
	twin-antialias-subpixel.xlib.ref.png \
	unantialiased-shapes.quartz.ref.png \
	unantialiased-shapes.ref.png \
	unbounded-operator.gl.argb32.xfail.png \
	unbounded-operator.gl.rgb24.xfail.png \
	unbounded-operator.pdf.argb32.ref.png \
	unbounded-operator.ps2.argb32.ref.png \
	unbounded-operator.ps3.argb32.ref.png \
	unbounded-operator.quartz.ref.png \
	unbounded-operator.quartz.rgb24.ref.png \
	unbounded-operator.ref.png \
	unbounded-operator.rgb24.ref.png \
	unbounded-operator.svg12.argb32.ref.png \
	unbounded-operator.svg12.rgb24.xfail.png \
	unbounded-operator.xlib.rgb24.ref.png \
	user-font-mask.pdf.ref.png \
	user-font-mask.ps2.ref.png \
	user-font-mask.ps3.ref.png \
	user-font-mask.ref.png \
	user-font-mask.svg.ref.png \
	user-font-proxy.pdf.argb32.ref.png \
	user-font-proxy.pdf.ref.png \
	user-font-proxy.pdf.rgb24.ref.png \
	user-font-proxy.ps.ref.png \
	user-font-proxy.ref.png \
	user-font-proxy.svg.ref.png \
	user-font-proxy.xlib.ref.png \
	user-font-rescale.ps2.ref.png \
	user-font-rescale.ps3.ref.png \
	user-font-rescale.ref.png \
	user-font-rescale.svg.ref.png \
	user-font.pdf.ref.png \
	user-font.ps.ref.png \
	user-font.ref.png \
	user-font.svg.ref.png \
	user-font.xlib.ref.png \
	xcb-surface-source.rgb24.ref.png \
	xcb-surface-source.argb32.ref.png \
	xcomposite-projection.ref.png \
	xlib-expose-event.ref.png \
	xlib-surface-source.rgb24.ref.png \
	xlib-surface-source.argb32.ref.png \
	xlib-surface-source.ps2.ref.png \
	xlib-surface-source.ps3.ref.png \
	xlib-surface-source.svg12.argb32.xfail.png \
	xlib-surface-source.svg12.rgb24.xfail.png \
	zero-alpha.ref.png

EXTRA_DIST +=		\
6x13.pcf		\
make-html.pl		\
jpeg.jpg		\
png.png			\
jp2.jp2			\
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
# fallback-resolution   - The essential problem here is that the recording-surface
#                         has recorded a sequence of operations with one device
#                         transformation, and we attempt to replay it with
#                         another (basically a scale-factor for the falback
#                         resolution). Carl begun to look at this with his
#                         chain-of-bugs, but the can of worms is much bigger.
#                         It appears to be a design flaw in the recording-surface
#                         that may spread further...
#                         My solution would be to lock Behad and Adrian in a
#                         room, with Carl as a moderator and not let them out
#                         until they have come up with an interface and
#                         semantics that actually work. :-)
# in-fill-empty-trapezoid The cairo_in_fill () function can sometimes
#                         produce false positives when the tessellator
#                         produces empty trapezoids and the query
#                         point lands exactly on a trapezoid edge.
# long-lines            - range overflow of fixed-point
# scale-offset-image    - loss of precision converting a cairo matrix to
# scale-offset-similar    pixman's fixed point format.
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
in-fill-empty-trapezoid$(EXEEXT)		\
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
ps-eps                          \
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
	-I$(top_srcdir)/util/cairo-script	\
	-I$(top_srcdir)/src			\
	-I$(top_builddir)/src			\
	$(CAIRO_CFLAGS)
AM_LDFLAGS = $(CAIRO_LDFLAGS)

$(top_builddir)/boilerplate/libcairoboilerplate.la: $(top_builddir)/src/libcairo.la
	cd $(top_builddir)/boilerplate && $(MAKE) $(AM_MAKEFLAGS) libcairoboilerplate.la

$(top_builddir)/src/libcairo.la:
	cd $(top_builddir)/src && $(MAKE) $(AM_MAKEFLAGS) libcairo.la

$(top_builddir)/test/pdiff/libpdiff.la:
	cd $(top_builddir)/test/pdiff && $(MAKE) $(AM_MAKEFLAGS) libpdiff.la

$(top_builddir)/util/cairo-script/libcairo-script-interpreter.la: $(top_builddir)/src/libcairo.la
	cd $(top_builddir)/util/cairo-script && $(MAKE) $(AM_MAKEFLAGS) libcairo-script-interpreter.la

EXTRA_PROGRAMS += imagediff png-flatten

imagediff_SOURCES = \
	imagediff.c	\
	buffer-diff.c	\
	buffer-diff.h
imagediff_LDADD = \
	$(top_builddir)/test/pdiff/libpdiff.la \
	$(top_builddir)/src/libcairo.la

png_flatten_SOURCES = png-flatten.c
png_flatten_LDADD = $(top_builddir)/src/libcairo.la \
		    $(CAIRO_LDADD)

if BUILD_ANY2PPM
check_PROGRAMS += any2ppm
any2ppm_CFLAGS = $(AM_CFLAGS) $(POPPLER_CFLAGS) $(LIBRSVG_CFLAGS) $(LIBSPECTRE_CFLAGS)
# add LDADD, so poppler/librsvg uses "our" cairo
any2ppm_LDFLAGS = $(AM_LDFLAGS) $(CAIRO_TEST_UNDEFINED_LDFLAGS)
any2ppm_LDADD = \
		$(top_builddir)/util/cairo-script/libcairo-script-interpreter.la \
		$(top_builddir)/src/libcairo.la \
		$(CAIRO_LDADD) \
		$(CAIROBOILERPLATE_LIBS) \
		$(POPPLER_LIBS) \
		$(LIBRSVG_LIBS) \
		$(LIBSPECTRE_LIBS)
endif

if CAIRO_CAN_TEST_PDF_SURFACE
check_PROGRAMS += pdf2png
pdf2png_CFLAGS = $(AM_CFLAGS) $(POPPLER_CFLAGS)
# add LDADD, so poppler uses "our" cairo
pdf2png_LDFLAGS = $(AM_LDFLAGS) $(CAIRO_TEST_UNDEFINED_LDFLAGS)
pdf2png_LDADD  = $(top_builddir)/src/libcairo.la \
		 $(CAIRO_LDADD) \
		 $(POPPLER_LIBS)
endif

if CAIRO_CAN_TEST_SVG_SURFACE
check_PROGRAMS += svg2png
svg2png_CFLAGS = $(AM_CFLAGS) $(LIBRSVG_CFLAGS)
# add LDADD, so librsvg uses "our" cairo
svg2png_LDFLAGS = $(AM_LDFLAGS) $(CAIRO_TEST_UNDEFINED_LDFLAGS)
svg2png_LDADD  = $(top_builddir)/src/libcairo.la \
		 $(CAIRO_LDADD) \
		 $(LIBRSVG_LIBS)
endif

if CAIRO_HAS_SPECTRE
check_PROGRAMS += ps2png
ps2png_CFLAGS = $(AM_CFLAGS) $(LIBSPECTRE_CFLAGS)
# add LDADD, so ps2png uses "our" cairo
ps2png_LDFLAGS = $(AM_LDFLAGS) $(CAIRO_TEST_UNDEFINED_LDFLAGS)
ps2png_LDADD  = $(top_builddir)/src/libcairo.la \
		$(CAIRO_LDADD) \
		$(LIBSPECTRE_LIBS)
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
	--track-origins=yes \
	--leak-check=yes --show-reachable=yes \
	$(EXTRA_VALGRIND_FLAGS)

CLEANFILES +=					\
	valgrind-log				\
	index.html				\
	ref.hash				\
	png-test.png				\
	png.out.png				\
	create-for-stream.pdf			\
	create-for-stream.ps			\
	create-for-stream.svg			\
	svg-surface-source.out.svg		\
	pdf-surface-source.out.pdf		\
	ps-surface-source.out.ps		\
	pdf-features.pdf			\
	pdf-mime-data.out*			\
	ps-eps.eps				\
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
clean-local:
	rm -rf output
	-${FIND} . -name '*.log'      -print | ${XARGS} ${RM}
	-${FIND} . -name '*.[is]'     -print | ${XARGS} ${RM}
clean-caches:
	-${FIND} output -name '*.fail.*' -print | ${XARGS} ${RM}
	-${FIND} output -name '*.pass.*' -print | ${XARGS} ${RM}

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
	$(MAKE) $(AM_MAKEFLAGS) check TESTS_ENVIRONMENT='$(TESTS_ENVIRONMENT) CAIRO_TEST_MODE="$(MODE),foreground CAIRO_TEST_TIMEOUT=0" $(top_builddir)/libtool --mode=execute valgrind $(VALGRIND_FLAGS)' 2>&1 | tee valgrind-log

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
	REFS=`git ls-files "*.ref.png" "*.xfail.png" "*.new.png"`; \
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
	    REFS=`ls *.ref.png *.xfail.png *.new.png`; \
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
