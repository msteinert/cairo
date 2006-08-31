/*
 * Copyright © 2006 Mozilla Corporation
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of
 * Mozilla Corporation not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission. Mozilla Corporation makes no representations about the
 * suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * MOZILLA CORPORATION DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL MOZILLA CORPORATION BE LIABLE FOR ANY SPECIAL,
 * INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
 * IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author: Vladimir Vukicevic <vladimir@pobox.com>
 */

#ifndef CAIRO_BENCH_H_
#define CAIRO_BENCH_H_

#ifndef USE_WINAPI
#include <sys/time.h>
#endif

#include <cairo.h>

#include "surface-boilerplate.h"

extern int num_benchmarks;

char *content_name (cairo_content_t content);
cairo_content_t content_for_name (const char *content);

/* results */

typedef struct _bench_result_t bench_result_t;

struct _bench_result_t {
    cairo_test_target_t *target;
    double *results;

    bench_result_t *next;
};

/* timers */

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

extern int alarm_expired;

void timer_start (bench_timer_t *tr);
void timer_stop (bench_timer_t *tr);
double timer_elapsed (bench_timer_t *tr);

void set_alarm (int seconds);
void start_timing (bench_timer_t *tr, long *count);
void stop_timing (bench_timer_t *tr, long count);
double timing_result (bench_timer_t *tr);

#ifdef USE_WINAPI
// Windows needs a SleepEx to put the thread into an alertable state,
// such that the timer expiration callback can fire.  I can't figure
// out how to do an async timer.  On a quiet system, this doesn't
// seem to significantly affect the results.
#define BEGIN_TIMING_LOOP(timervar,countvar)  do {   \
    countvar = 0;                                    \
    start_timing(&(timervar), &(countvar));          \
    while (!alarm_expired) {                         \
        SleepEx(0, TRUE);

#else

#define BEGIN_TIMING_LOOP(timervar,countvar)  do {   \
    countvar = 0;                                    \
    start_timing(&(timervar), &(countvar));          \
    while (!alarm_expired) {

#endif

#define END_TIMING_LOOP(timervar,countvar)      \
    (countvar)++;                               \
    }                                           \
    stop_timing (&(timervar), (countvar));      \
    } while (0);

/* arg parsing */
int parse_args (int argc, char **argv, int **tests, cairo_test_target_t ***targets);

#ifndef CAIRO_HAS_PNG_FUNCTIONS
cairo_status_t cairo_surface_write_to_png (cairo_surface_t *surface, const char *filename);
#endif

#endif /* CAIRO_BENCH_H_ */
