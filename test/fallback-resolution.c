/*
 * Copyright © 2006 Red Hat, Inc.
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

#include <stdio.h>
#include <cairo.h>
#include <cairo-pdf.h>
#include <cairo-pdf-test.h>

#include "cairo-test.h"

/* This test exists to test cairo_surface_set_fallback_resolution
 */

#define INCHES_TO_POINTS(in) ((in) * 72.0)
#define SIZE INCHES_TO_POINTS(1)

int
main (void)
{
    cairo_surface_t *surface;
    cairo_t *cr;
    cairo_status_t status;
    char *filename;
    double dpi[] = { 37.5, 75., 150., 300., 600. };
    int i;

    printf("\n");

    filename = "fallback-resolution.pdf";

    surface = cairo_pdf_surface_create (filename, SIZE, SIZE);

    cr = cairo_create (surface);

    /* Force image fallbacks before drawing anything. */
    cairo_pdf_test_force_fallbacks ();

    for (i = 0; i < sizeof(dpi) / sizeof (dpi[0]); i++) {
	cairo_pdf_surface_set_dpi (surface, dpi[i], dpi[i]);
	cairo_arc (cr, SIZE / 2.0, SIZE / 2.0,
		   0.75 * SIZE / 2.0,
		   0, 2.0 * M_PI);
	cairo_fill (cr);

	cairo_show_page (cr);
    }

    status = cairo_status (cr);

    cairo_destroy (cr);
    cairo_surface_destroy (surface);

    if (status) {
	cairo_test_log ("Failed to create pdf surface for file %s: %s\n",
			filename, cairo_status_to_string (status));
	return CAIRO_TEST_FAILURE;
    }

    printf ("fallback-resolution: Please check %s to ensure it looks correct.\n", filename);

    return CAIRO_TEST_SUCCESS;
}
