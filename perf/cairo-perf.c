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

int cairo_perf_duration = -1;

int alarm_expired = 0;

typedef struct _cairo_perf {
    const char *name;
    cairo_perf_func_t setup;
    cairo_perf_func_t run;
    unsigned int min_size;
    unsigned int max_size;
} cairo_perf_t;

cairo_perf_t perfs[] = {
    { "paint", paint_setup, paint, 32, 1024 },
    { "paint_alpha", paint_alpha_setup, paint, 32, 1024 },
    { NULL }
};

/* Some targets just aren't that interesting for performance testing,
 * (not least because many of these surface types use a meta-surface
 * and as such defer the "real" rendering to later, so our timing
 * loops wouldn't count the real work, just the recording by the
 * meta-surface. */
static cairo_bool_t
target_is_measurable (cairo_test_target_t *target)
{
    switch (target->expected_type) {
    case CAIRO_SURFACE_TYPE_IMAGE:
	if (strcmp (target->name, "pdf") ||
	    strcmp (target->name, "ps"))
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

void
start_timing (bench_timer_t *tr, long *count) {
    if (cairo_perf_duration == -1) {
        if (getenv("CAIRO_PERF_DURATION"))
            cairo_perf_duration = strtol(getenv("CAIRO_PERF_DURATION"), NULL, 0);
        else
            cairo_perf_duration = 5;
    }
    *count = 0;
    timer_start (tr);
    set_alarm (cairo_perf_duration);
}

void
stop_timing (bench_timer_t *tr, long count) {
    timer_stop (tr);
    tr->count = count;
}

double
timing_result (bench_timer_t *tr) {
    return tr->count / timer_elapsed (tr);
}

int
main (int argc, char *argv[])
{
    int i, j;
    cairo_test_target_t *target;
    cairo_perf_t *perf;
    cairo_surface_t *surface;
    cairo_t *cr;
    unsigned int size;

    for (i = 0; targets[i].name; i++) {
	target = &targets[i];
	if (! target_is_measurable (target))
	    continue;
	for (j = 0; perfs[j].name; j++) {
	    perf = &perfs[j];
	    for (size = perf->min_size; size <= perf->max_size; size *= 2) {
		surface = (target->create_target_surface) (perf->name,
							   target->content,
							   size, size,
							   &target->closure);
		cr = cairo_create (surface);
		perf->setup (cr, size, size);
		perf->run (cr, size, size);
	    }
	}
    }

    return 0;
}

