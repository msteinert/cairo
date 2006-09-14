/* -*- Mode: c; c-basic-offset: 4; indent-tabs-mode: t; tab-width: 8; -*- */
/*
 * Copyright © 2006 Mozilla Corporation
 * Copyright © 2006 Red Hat, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of
 * the authors not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission. The authors make no representations about the
 * suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE AUTHORS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY SPECIAL,
 * INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
 * IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Authors: Vladimir Vukicevic <vladimir@pobox.com>
 *          Carl Worth <cworth@cworth.org>
 */

#include "cairo-perf.h"

int cairo_perf_iterations = 100;

typedef struct _cairo_perf {
    const char *name;
    cairo_perf_func_t run;
    unsigned int min_size;
    unsigned int max_size;
} cairo_perf_t;

cairo_perf_t perfs[];

/* Some targets just aren't that interesting for performance testing,
 * (not least because many of these surface types use a meta-surface
 * and as such defer the "real" rendering to later, so our timing
 * loops wouldn't count the real work, just the recording by the
 * meta-surface. */
static cairo_bool_t
target_is_measurable (cairo_boilerplate_target_t *target)
{
    switch (target->expected_type) {
    case CAIRO_SURFACE_TYPE_IMAGE:
	if (strcmp (target->name, "pdf") == 0 ||
	    strcmp (target->name, "ps") == 0)
	{
	    return FALSE;
	}
	else
	{
	    return TRUE;
	}
    case CAIRO_SURFACE_TYPE_XLIB:
    case CAIRO_SURFACE_TYPE_XCB:
    case CAIRO_SURFACE_TYPE_GLITZ:
    case CAIRO_SURFACE_TYPE_QUARTZ:
    case CAIRO_SURFACE_TYPE_WIN32:
    case CAIRO_SURFACE_TYPE_BEOS:
    case CAIRO_SURFACE_TYPE_DIRECTFB:
	return TRUE;
    case CAIRO_SURFACE_TYPE_PDF:
    case CAIRO_SURFACE_TYPE_PS:
    case CAIRO_SURFACE_TYPE_SVG:
    default:
	return FALSE;
    }
}

static const char *
_content_to_string (cairo_content_t content)
{
    switch (content) {
    case CAIRO_CONTENT_COLOR:
	return "rgb";
    case CAIRO_CONTENT_ALPHA:
	return "a";
    case CAIRO_CONTENT_COLOR_ALPHA:
	return "rgba";
    default:
	return "<unknown_content>";
    }
}

typedef struct _stats
{
    double mean;
    double std_dev;
} stats_t;

static int
compare_cairo_perf_ticks (const void *_a, const void *_b)
{
    const cairo_perf_ticks_t *a = _a;
    const cairo_perf_ticks_t *b = _b;

    if (*a > *b)
	return 1;
    if (*a < *b)
	return -1;
    return 0;
}

static void
_compute_stats (cairo_perf_ticks_t *values, int num_values, stats_t *stats)
{
    int i;
    double sum, delta;

    sum = 0.0;
    for (i = 0; i < num_values; i++)
	sum += values[i];

    stats->mean = sum / num_values;

    sum = 0.0;
    for (i = 0; i <  num_values; i++) {
	delta = values[i] - stats->mean;
	sum += delta * delta;
    }

    /* Let's use a std. deviation normalized to the mean for easier
     * comparison. */
    stats->std_dev = sqrt(sum / num_values) / stats->mean;
}

int
main (int argc, char *argv[])
{
    int i, j, k;
    cairo_boilerplate_target_t *target;
    cairo_perf_t *perf;
    cairo_surface_t *surface;
    cairo_t *cr;
    unsigned int size;
    cairo_perf_ticks_t *times;
    stats_t stats;
    const char *cairo_test_target = getenv ("CAIRO_TEST_TARGET");
    double ms;
    int test_number;

    if (getenv("CAIRO_PERF_ITERATIONS"))
	cairo_perf_iterations = strtol(getenv("CAIRO_PERF_ITERATIONS"), NULL, 0);

    times = xmalloc (cairo_perf_iterations * sizeof (cairo_perf_ticks_t));

    for (i = 0; targets[i].name; i++) {
	target = &targets[i];
	if (! target_is_measurable (target))
	    continue;
	if (cairo_test_target && ! strstr (cairo_test_target, target->name))
	    continue;

	test_number = 0;

	for (j = 0; perfs[j].name; j++) {
	    perf = &perfs[j];
	    for (size = perf->min_size; size <= perf->max_size; size *= 2) {
		surface = (target->create_surface) (perf->name,
						    target->content,
						    size, size,
						    CAIRO_BOILERPLATE_MODE_PERF,
						    &target->closure);
		cairo_perf_timer_set_finalize (target->wait_for_rendering, target->closure);
		cr = cairo_create (surface);
		for (k =0; k < cairo_perf_iterations; k++) {
		    cairo_perf_yield ();
		    times[k] = perf->run (cr, size, size);
		}

		qsort (times, cairo_perf_iterations,
		       sizeof (cairo_perf_ticks_t), compare_cairo_perf_ticks);

		/* Assume the slowest 15% are outliers, and ignore */
		_compute_stats (times, .85 * cairo_perf_iterations, &stats);

		if (i==0 && j==0 && size == perf->min_size)
		    printf ("[ # ] %8s-%-4s %27s %9s %5s %s\n",
			    "backend", "content", "test-size", "mean ms",
			    "std dev.", "iterations");

		printf ("[%3d] %8s-%-4s %25s-%-3d ",
			test_number, target->name, _content_to_string (target->content),
			perf->name, size);

		printf ("%#9.3f %#5.2f%% % 5d\n",
			(stats.mean * 1000.0) / cairo_perf_ticks_per_second (),
			stats.std_dev * 100.0, cairo_perf_iterations);

		test_number++;
	    }
	}
    }

    return 0;
}

cairo_perf_t perfs[] = {
    { "paint_over_solid", paint_over_solid, 64, 512 },
    { "paint_over_solid_alpha", paint_over_solid_alpha, 64, 512 },
    { "paint_source_solid", paint_over_solid, 64, 512 },
    { "paint_source_solid_alpha", paint_over_solid_alpha, 64, 512 },

    { "paint_over_surf_rgb24", paint_over_solid, 64, 512 },
    { "paint_over_surf_argb32", paint_over_solid_alpha, 64, 512 },
    { "paint_source_surf_rgb24", paint_over_solid, 64, 512 },
    { "paint_source_surf_argb32", paint_over_solid_alpha, 64, 512 },

    { "tessellate-16",	tessellate_16,	100, 100},
    { "tessellate-64",	tessellate_64,	100, 100},
    { "tessellate-256", tessellate_256, 100, 100},
    { NULL }
};
