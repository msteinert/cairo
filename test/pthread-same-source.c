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

typedef struct {
  cairo_surface_t *target;
  cairo_surface_t *source;
  int id;
} thread_data_t;

static void *
draw_thread (void *arg)
{
    thread_data_t *thread_data = arg;
    cairo_surface_t *surface;
    cairo_pattern_t *pattern;
    cairo_matrix_t pattern_matrix = { 2, 0, 0, 2, 0, 0 };
    cairo_t *cr;
    int x, y;

    cr = cairo_create (thread_data->target);
    cairo_surface_destroy (thread_data->target);

    pattern = cairo_pattern_create_for_surface (thread_data->source);
    cairo_surface_destroy (thread_data->source);
    cairo_pattern_set_extend (pattern, thread_data->id % 4);
    cairo_pattern_set_filter (pattern, thread_data->id >= 4 ? CAIRO_FILTER_BILINEAR : CAIRO_FILTER_NEAREST);
    cairo_pattern_set_matrix (pattern, &pattern_matrix);

    for (y = 0; y < HEIGHT; y++) {
        for (x = 0; x < WIDTH; x++) {
            cairo_save (cr);
            cairo_translate (cr, 4 * x + 1, 4 * y + 1);
            cairo_rectangle (cr, 0, 0, 2, 2);
            cairo_set_source (cr, pattern);
            cairo_fill (cr);
            cairo_restore (cr);
        }
    }
    cairo_pattern_destroy (pattern);

    surface = cairo_surface_reference (cairo_get_target (cr));
    cairo_destroy (cr);

    return surface;
}

static cairo_surface_t *
create_source (cairo_surface_t *similar)
{
    cairo_surface_t *source;
    cairo_t *cr;
    double colors[4][3] = {
      { 0.75, 0,    0    },
      { 0,    0.75, 0    },
      { 0,    0,    0.75 },
      { 0.75, 0.75, 0    }
    };
    int i;

    source = cairo_surface_create_similar (similar,
                                           CAIRO_CONTENT_COLOR_ALPHA,
                                           2, 2);

    cr = cairo_create (source);
    for (i = 0; i < 4; i++) {
      cairo_set_source_rgb (cr, colors[i][0], colors[i][1], colors[i][2]);
      cairo_rectangle (cr, i % 2, i / 2, 1, 1);
      cairo_fill (cr);
    }
    cairo_destroy (cr);

    return source;
}

static cairo_test_status_t
draw (cairo_t *cr, int width, int height)
{
    pthread_t threads[N_THREADS];
    thread_data_t thread_data[N_THREADS];
    cairo_test_status_t test_status = CAIRO_TEST_SUCCESS;
    cairo_surface_t *source;
    int i;

    source = create_source (cairo_get_target (cr));

    for (i = 0; i < N_THREADS; i++) {
        thread_data[i].target = cairo_surface_create_similar (cairo_get_target (cr),
                                                              CAIRO_CONTENT_COLOR_ALPHA,
                                                              4 * WIDTH, 4 * HEIGHT);
        thread_data[i].source = cairo_surface_reference (source);
        thread_data[i].id = i;
        if (pthread_create (&threads[i], NULL, draw_thread, &thread_data[i]) != 0) {
	    threads[i] = pthread_self (); /* to indicate error */
            cairo_surface_destroy (thread_data[i].target);
            cairo_surface_destroy (thread_data[i].source);
            test_status = CAIRO_TEST_FAILURE;
	    break;
        }
    }

    cairo_set_source_rgb (cr, 0.5, 0.5, 0.5);
    cairo_paint (cr);

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

        cairo_translate (cr, 0, 4 * HEIGHT);
    }

    return test_status;
}

CAIRO_TEST (pthread_same_source,
	    "Use the same source for drawing in different threads",
	    "threads", /* keywords */
	    NULL, /* requirements */
	    4 * WIDTH, 4 * HEIGHT * N_THREADS,
	    NULL, draw)
