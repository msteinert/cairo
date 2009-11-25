/*
 * Copyright 2009 Benjamin Otte
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of
 * Benjamin Otte not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission. Benjamin Otte makes no representations about the
 * suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * BENJAMIN OTTE DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL BENJAMIN OTTE BE LIABLE FOR ANY SPECIAL,
 * INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
 * IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author: Benjamin Otte <otte@gnome.org>
 */

#include "cairo-test.h"
#include <pthread.h>

#define N_THREADS 8

#define WIDTH 64
#define HEIGHT 8

static void *
draw_thread (void *arg)
{
    cairo_surface_t *surface = arg;
    cairo_t *cr;
    int x, y;

    cr = cairo_create (surface);
    cairo_surface_destroy (surface);

    for (y = 0; y < HEIGHT; y++) {
        for (x = 0; x < WIDTH; x++) {
            cairo_rectangle (cr, x, y, 1, 1);
            cairo_set_source_rgba (cr, 0, 0.75, 0.75, (double) x / WIDTH);
            cairo_fill (cr);
        }
    }

    surface = cairo_surface_reference (cairo_get_target (cr));
    cairo_destroy (cr);

    return surface;
}

static cairo_test_status_t
draw (cairo_t *cr, int width, int height)
{
    pthread_t threads[N_THREADS];
    cairo_test_status_t test_status = CAIRO_TEST_SUCCESS;
    int i;

    for (i = 0; i < N_THREADS; i++) {
	cairo_surface_t *surface;

        surface = cairo_surface_create_similar (cairo_get_target (cr),
						CAIRO_CONTENT_COLOR,
						WIDTH, HEIGHT);
        if (pthread_create (&threads[i], NULL, draw_thread, surface) != 0) {
	    threads[i] = pthread_self ();
            test_status = cairo_test_status_from_status (cairo_test_get_context (cr),
							 cairo_surface_status (surface));
            cairo_surface_destroy (surface);
	    break;
        }
    }

    for (i = 0; i < N_THREADS; i++) {
	void *surface;

        if (pthread_equal (threads[i], pthread_self ()))
            break;

        if (pthread_join (threads[i], &surface) != 0) {
            test_status = CAIRO_TEST_FAILURE;
	    break;
	}

        cairo_set_source_surface (cr, surface, 0, 0);
        cairo_surface_destroy (surface);
        cairo_paint (cr);

        cairo_translate (cr, 0, HEIGHT);
    }

    return test_status;
}

CAIRO_TEST (pthread_similar,
	    "Draw lots of 1x1 rectangles on similar surfaces in lots of threads",
	    "threads", /* keywords */
	    NULL, /* requirements */
	    WIDTH, HEIGHT * N_THREADS,
	    NULL, draw)
