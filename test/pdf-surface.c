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

#include <stdio.h>

#include "cairo-test.h"

/* Pretty boring test just to make sure things aren't crashing ---
 * no verification that we're getting good results yet.
*/

int
main (void)
{
    cairo_t *cr;
    const char *filename = "pdf-surface.pdf";
    FILE *file;
    cairo_surface_t *surface;

    file = fopen (filename, "w");
    if (!file) {
	cairo_test_log ("Failed to open file %s\n", filename);
	return CAIRO_TEST_FAILURE;
    }

    surface = cairo_pdf_surface_create (file,
					297 / 25.4,
					210 / 25.4);
    cr = cairo_create (surface);

    cairo_rectangle (cr, 10, 10, 100, 100);
    cairo_set_source_rgb (cr, 1, 0, 0);
    cairo_fill (cr);

    cairo_show_page (cr);

    cairo_surface_destroy (surface);
    cairo_destroy (cr);

    fclose (file);

    return 0;
}
