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

typedef struct {
#ifdef USE_WINAPI
    LARGE_INTEGER start;
    LARGE_INTEGER stop;
#else
    struct timeval start;
    struct timeval stop;
#endif
    long count;
} bench_timer_t;

#include "timer-alarm.h"

void
start_timing (bench_timer_t *tr);

void
stop_timing (bench_timer_t *tr);

extern int cairo_perf_duration;
extern int cairo_perf_alarm_expired;

#if CAIRO_HAS_WIN32_SURFACE
/* Windows needs a SleepEx to put the thread into an alertable state,
 * such that the timer expiration callback can fire.  I can't figure
 * out how to do an async timer.  On a quiet system, this doesn't
 * seem to significantly affect the results.
 */
# define PERF_LOOP_INIT(timervar)  do {              \
    start_timing(&(timervar));                       \
    while (! cairo_perf_alarm_expired) {             \
        SleepEx(0, TRUE)
#else
# define PERF_LOOP_INIT(timervar)  do {              \
    start_timing(&(timervar));                       \
    while (! cairo_perf_alarm_expired) {
#endif

#define PERF_LOOP_FINI(timervar)                \
    (timervar).count++;                         \
    }                                           \
    stop_timing (&(timervar));                  \
    } while (0)

#define PERF_LOOP_RATE(timervar)		\
    ((timervar).count) / timer_elapsed (&(timervar))

typedef void (*cairo_perf_func_t) (cairo_t *cr, int width, int height);

#define DECL_PERF_FUNC(func) void func (cairo_t *cr, int width, int height)

DECL_PERF_FUNC (paint_setup);
DECL_PERF_FUNC (paint_alpha_setup);
DECL_PERF_FUNC (paint);

#endif
