/*
 * Copyright © 2005 Red Hat, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of
 * Red Hat, Inc. not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission. Red Hat, Inc. makes no representations about the
 * suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * RED HAT, INC. DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL RED HAT, INC. BE LIABLE FOR ANY SPECIAL,
 * INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
 * IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author: Carl D. Worth <cworth@cworth.org>
 */

#include <stdlib.h>
#include "cairo_test.h"

cairo_test_t test = {
    "path_data",
    "Tests calls to path_data functions: cairo_copy_path_data, cairo_copy_path_data_flat, and cairo_append_path_data",
    45, 53
};

static void
scale_by_two (double *x, double *y)
{
    *x = *x * 2.0;
    *y = *y * 2.0;
}

typedef void (*munge_func_t) (double *x, double *y);

static void
munge_and_set_path (cairo_t	      *cr,
		    cairo_path_data_t *path,
		    munge_func_t       munge)
{
    cairo_path_data_t *p;
    double x1, y1, x2, y2, x3, y3;

    p = path;
    while (1) {
	switch (p->header.type) {
	case CAIRO_PATH_MOVE_TO:
	    x1 = p[1].point.x; y1 = p[1].point.y;
	    (munge) (&x1, &y1);
	    cairo_move_to (cr, x1, y1);
	    break;
	case CAIRO_PATH_LINE_TO:
	    x1 = p[1].point.x; y1 = p[1].point.y;
	    (munge) (&x1, &y1);
	    cairo_line_to (cr, x1, y1);
	    break;
	case CAIRO_PATH_CURVE_TO:
	    x1 = p[1].point.x; y1 = p[1].point.y;
	    x2 = p[2].point.x; y2 = p[2].point.y;
	    x3 = p[3].point.x; y3 = p[3].point.y;
	    (munge) (&x1, &y1);
	    (munge) (&x2, &y2);
	    (munge) (&x3, &y3);
	    cairo_curve_to (cr,
			    x1, y1,
			    x2, y2,
			    x3, y3);
	    break;
	case CAIRO_PATH_CLOSE_PATH:
	    cairo_close_path (cr);
	    break;
	case CAIRO_PATH_END:
	    return;
	}
	p += p->header.length;
    }
}

static void
make_path (cairo_t *cr)
{
    cairo_rectangle (cr, 0, 0, 5, 5);
    cairo_move_to (cr, 15, 2.5);
    cairo_arc (cr, 12.5, 2.5, 2.5, 0, 2 * M_PI);
}

static cairo_test_status_t
draw (cairo_t *cr, int width, int height)
{
    cairo_path_data_t *path;

    /* copy path, munge, and fill */
    cairo_translate (cr, 5, 5);
    make_path (cr);
    path = cairo_copy_path_data (cr);

    cairo_new_path (cr);
    munge_and_set_path (cr, path, scale_by_two);
    free (path);
    cairo_fill (cr);

    /* copy flattened path, munge, and fill */
    cairo_translate (cr, 0, 15);
    make_path (cr);
    path = cairo_copy_path_data_flat (cr);

    cairo_new_path (cr);
    munge_and_set_path (cr, path, scale_by_two);
    free (path);
    cairo_fill (cr);

    /* append two copies of path, and fill */
    cairo_translate (cr, 0, 15);
    cairo_scale (cr, 2.0, 2.0);
    make_path (cr);
    path = cairo_copy_path_data (cr);

    cairo_new_path (cr);
    cairo_append_path_data (cr, path);
    cairo_translate (cr, 2.5, 2.5);
    cairo_append_path_data (cr, path);

    cairo_set_fill_rule (cr, CAIRO_FILL_RULE_EVEN_ODD);
    cairo_fill (cr);

    return CAIRO_TEST_SUCCESS;
}

int
main (void)
{
    cairo_t *cr;
    cairo_path_data_t bogus_path_data;

    /* Test a couple error conditions for cairo_append_path_data */
    cr = cairo_create ();
    cairo_append_path_data (cr, NULL);
    if (cairo_status (cr) != CAIRO_STATUS_NULL_POINTER)
	return 1;
    cairo_destroy (cr);

    cr = cairo_create ();
    bogus_path_data.header.type = CAIRO_PATH_MOVE_TO;
    bogus_path_data.header.length = 1;
    cairo_append_path_data (cr, &bogus_path_data);
    if (cairo_status (cr) != CAIRO_STATUS_INVALID_PATH_DATA)
	return 1;
    cairo_destroy (cr);

    /* And test the degnerate case */
    cr = cairo_create ();
    bogus_path_data.header.type = CAIRO_PATH_END;
    cairo_append_path_data (cr, &bogus_path_data);
    if (cairo_status (cr) != CAIRO_STATUS_SUCCESS)
	return 1;
    cairo_destroy (cr);

    return cairo_test (&test, draw);
}
