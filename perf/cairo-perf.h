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

#include "cairo-perf-timer.h"

extern int cairo_perf_duration;
extern int cairo_perf_alarm_expired;

#if CAIRO_HAS_WIN32_SURFACE
/* Windows needs a SleepEx to put the thread into an alertable state,
 * such that the timer expiration callback can fire.  I can't figure
 * out how to do an async timer.  On a quiet system, this doesn't
 * seem to significantly affect the results.
 */
# define CAIRO_PERF_LOOP_INIT(timervar)  do {	\
    timervar.count = 0;				\
    timer_start (&(timervar));			\
    set_alarm (cairo_perf_duration);		\
    while (! cairo_perf_alarm_expired) {	\
        SleepEx(0, TRUE)
#else
# define CAIRO_PERF_LOOP_INIT(timervar)  do {	\
    timervar.count = 0;				\
    timer_start (&(timervar));			\
    set_alarm (cairo_perf_duration);		\
    while (! cairo_perf_alarm_expired) {
#endif

#define CAIRO_PERF_LOOP_FINI(timervar)		\
    (timervar).count++;				\
    }						\
    timer_stop (&(timervar));			\
    } while (0)

#define CAIRO_PERF_LOOP_RATE(timervar)		\
    ((timervar).count) / timer_elapsed (&(timervar))

typedef void (*cairo_perf_func_t) (cairo_t *cr, int width, int height);

#define CAIRO_PERF_DECL(func) void func (cairo_t *cr, int width, int height)

CAIRO_PERF_DECL (paint_setup);
CAIRO_PERF_DECL (paint_alpha_setup);
CAIRO_PERF_DECL (paint);

#endif
