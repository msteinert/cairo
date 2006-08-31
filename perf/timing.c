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

#include "timing.h"

int cairo_perf_duration = -1;

int alarm_expired = 0;

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
