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

#define USE_WINAPI

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "cairo-perf.h"

/* timers */

void
timer_start (bench_timer_t *tr) {
    QueryPerformanceCounter(&tr->start);
}

void
timer_stop (bench_timer_t *tr) {
    QueryPerformanceCounter(&tr->stop);
}

double
timer_elapsed (bench_timer_t *tr) {
    double d;
    LARGE_INTEGER freq;

    QueryPerformanceFrequency(&freq);

    d = (tr->stop.QuadPart - tr->start.QuadPart) / (double) freq.QuadPart;
    return d;
}

/* alarms */

void CALLBACK
alarm_handler (void *closure, DWORD dwTimerLowValue, DWORD dwTimerHighValue) {
    alarm_expired = 1;
}

HANDLE hTimer = NULL;
void
set_alarm (int seconds) {
    if (hTimer == NULL)
        hTimer = CreateWaitableTimer(NULL, TRUE, NULL);
    alarm_expired = 0;

    LARGE_INTEGER expTime;
    expTime.QuadPart = - (seconds * 10000000);
    if (!SetWaitableTimer (hTimer, &expTime, 0, alarm_handler, &alarm_expired, FALSE))
        fprintf (stderr, "SetWaitableTimer failed!\n");
}
