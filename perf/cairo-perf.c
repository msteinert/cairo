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

#define CAIRO_PERF_ITERATIONS_DEFAULT	100
#define CAIRO_PERF_LOW_STD_DEV		0.03
#define CAIRO_PERF_STABLE_STD_DEV_COUNT	5

typedef struct _cairo_perf_case {
    CAIRO_PERF_DECL (*run);
    unsigned int min_size;
    unsigned int max_size;
} cairo_perf_case_t;

cairo_perf_case_t perf_cases[];

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
    case CAIRO_SURFACE_TYPE_NQUARTZ:
    case CAIRO_SURFACE_TYPE_OS2:
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

void
cairo_perf_run (cairo_perf_t		*perf,
		const char		*name,
		cairo_perf_func_t	 perf_func)
{
    static cairo_bool_t first_run = TRUE;

    unsigned int i;
    cairo_perf_ticks_t *times;
    stats_t stats = {0.0, 0.0};
    int low_std_dev_count;

    times = xmalloc (perf->iterations * sizeof (cairo_perf_ticks_t));

    low_std_dev_count = 0;
    for (i =0; i < perf->iterations; i++) {
	cairo_perf_yield ();
	times[i] = (perf_func) (perf->cr, perf->size, perf->size);

	if (i > 0) {
	    qsort (times, i+1,
		   sizeof (cairo_perf_ticks_t), compare_cairo_perf_ticks);

	    /* Assume the slowest 15% are outliers, and ignore */
	    _compute_stats (times, .85 * (i+1), &stats);

	    if (stats.std_dev <= CAIRO_PERF_LOW_STD_DEV) {
		low_std_dev_count++;
		if (low_std_dev_count >= CAIRO_PERF_STABLE_STD_DEV_COUNT)
		    break;
	    } else {
		low_std_dev_count = 0;
	    }
	}
    }

    if (first_run) {
	printf ("[ # ] %8s-%-4s %28s %7s %5s %s\n",
		"backend", "content", "test-size", "mean(ms)",
		"stddev.", "iterations");
	first_run = FALSE;
    }

    printf ("[%3d] %8s-%-4s %26s-%-3d ",
	    perf->test_number, perf->target->name,
	    _content_to_string (perf->target->content),
	    name, perf->size);

    printf ("%#8.3f %#5.2f%% %3d\n",
	    (stats.mean * 1000.0) / cairo_perf_ticks_per_second (),
	    stats.std_dev * 100.0, i);

    perf->test_number++;
}

int
main (int argc, char *argv[])
{
    int i, j;
    cairo_perf_case_t *perf_case;
    cairo_perf_t perf;
    const char *cairo_test_target = getenv ("CAIRO_TEST_TARGET");
    cairo_boilerplate_target_t *target;
    cairo_surface_t *surface;

    if (getenv("CAIRO_PERF_ITERATIONS"))
	perf.iterations = strtol(getenv("CAIRO_PERF_ITERATIONS"), NULL, 0);
    else
	perf.iterations = CAIRO_PERF_ITERATIONS_DEFAULT;

    for (i = 0; targets[i].name; i++) {
	perf.target = target = &targets[i];
	perf.test_number = 0;

	if (! target_is_measurable (target))
	    continue;
	if (cairo_test_target && ! strstr (cairo_test_target, target->name))
	    continue;

	for (j = 0; perf_cases[j].run; j++) {

	    perf_case = &perf_cases[j];

	    for (perf.size = perf_case->min_size;
		 perf.size <= perf_case->max_size;
		 perf.size *= 2)
	    {
		surface = (target->create_surface) (NULL,
						    target->content,
						    perf.size, perf.size,
						    CAIRO_BOILERPLATE_MODE_PERF,
						    &target->closure);
		cairo_perf_timer_set_synchronize (target->synchronize,
						  target->closure);

		perf.cr = cairo_create (surface);

		perf_case->run (&perf, perf.cr, perf.size, perf.size);

		if (cairo_status (perf.cr)) {
		    fprintf (stderr, "Error: Test left cairo in an error state: %s\n",
			     cairo_status_to_string (cairo_status (perf.cr)));
		    exit (1);
		}

		cairo_destroy (perf.cr);
		cairo_surface_destroy (surface);
	    }
	}
    }

    return 0;
}

cairo_perf_case_t perf_cases[] = {
    { paint,  256, 512},
    { fill,   64, 256},
    { stroke, 64, 256},
    { text,   64, 256},
    { tessellate, 100, 100},
    { subimage_copy, 16, 512},
    { NULL }
};
