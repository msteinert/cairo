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

#include <signal.h>
#include <sys/time.h>
#include <unistd.h>

#include "cairo-perf.h"

/* timers */

void
timer_start (cairo_perf_timer_t *tr) {
    gettimeofday (&tr->start, NULL);
}

void
timer_stop (cairo_perf_timer_t *tr) {
    gettimeofday (&tr->stop, NULL);
}

double
timer_elapsed (cairo_perf_timer_t *tr) {
    double d;

    d = tr->stop.tv_sec - tr->start.tv_sec;
    d += (tr->stop.tv_usec - tr->start.tv_usec) / 1000000.0;

    return d;
}

/* alarms */

void
alarm_handler (int signal) {
    if (signal == SIGALRM) {
        cairo_perf_alarm_expired = 1;
    }
}

void
set_alarm (int seconds) {
    cairo_perf_alarm_expired = 0;
    signal (SIGALRM, alarm_handler);
    alarm (seconds);
}
