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

#define _GNU_SOURCE

#ifdef USE_WINAPI
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#ifndef USE_WINAPI
#include <sys/time.h>
#include <unistd.h>
#endif

#include "cairo-bench.h"

/* helpers */

char *
content_name (cairo_content_t content)
{
    if (content == CAIRO_CONTENT_COLOR) return "rgb";
    if (content == CAIRO_CONTENT_COLOR_ALPHA) return "argb";
    if (content == CAIRO_CONTENT_ALPHA) return "a8";
    assert (0);
    return NULL;
}

cairo_content_t
content_for_name (const char *content)
{
    if (strcmp(content, "rgb") == 0) return CAIRO_CONTENT_COLOR;
    if (strcmp(content, "argb") == 0) return CAIRO_CONTENT_COLOR_ALPHA;
    if (strcmp(content, "a8") == 0) return CAIRO_CONTENT_ALPHA;
    return (cairo_content_t) -1;
}

/* timers */

#ifdef USE_WINAPI
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
#else
void
timer_start (bench_timer_t *tr) {
    gettimeofday (&tr->start, NULL);
}

void
timer_stop (bench_timer_t *tr) {
    gettimeofday (&tr->stop, NULL);
}

double
timer_elapsed (bench_timer_t *tr) {
    double d;

    d = tr->stop.tv_sec - tr->start.tv_sec;
    d += (tr->stop.tv_usec - tr->start.tv_usec) / 1000000.0;

    return d;
}
#endif

/* alarms */
int test_seconds = -1;

int alarm_expired = 0;

#ifdef USE_WINAPI
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
#else
void
alarm_handler (int signal) {
    if (signal == SIGALRM) {
        alarm_expired = 1;
    }
}

void
set_alarm (int seconds) {
    alarm_expired = 0;
    signal (SIGALRM, alarm_handler);
    alarm (seconds);
}
#endif

/* timers + alarms! */

void
start_timing (bench_timer_t *tr, long *count) {
    if (test_seconds == -1) {
        if (getenv("TEST_SECONDS"))
            test_seconds = strtol(getenv("TEST_SECONDS"), NULL, 0);
        else
            test_seconds = 5;
    }
    *count = 0;
    timer_start (tr);
    set_alarm (test_seconds);
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
