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

#ifndef _CAIRO_PERF_H_
#define _CAIRO_PERF_H_

#include "cairo-boilerplate.h"

/* timers */

void
cairo_perf_timer_start (void);

void
cairo_perf_timer_stop (void);

typedef void
(*cairo_perf_timer_finalize_t) (void *closure);

void
cairo_perf_timer_set_finalize (cairo_perf_timer_finalize_t	 finalize,
			       void				*closure);

typedef uint64_t cairo_perf_ticks_t;

cairo_perf_ticks_t
cairo_perf_timer_elapsed (void);

cairo_perf_ticks_t
cairo_perf_ticks_per_second (void);

/* yield */

void
cairo_perf_yield (void);

/* running a test case */
typedef struct _cairo_perf cairo_perf_t;

typedef cairo_perf_ticks_t
(*cairo_perf_func_t) (cairo_t *cr, int width, int height);

void
cairo_perf_run (cairo_perf_t		*perf,
		const char		*name,
		cairo_perf_func_t	 perf_func);

#define CAIRO_PERF_DECL(func) void (func) (cairo_perf_t *perf, cairo_t *cr, int width, int height);

/* paint.c */
CAIRO_PERF_DECL (paint);

/* tessellate.c */
CAIRO_PERF_DECL (tessellate);

#endif
