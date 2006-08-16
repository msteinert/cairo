/*
 * Copyright © 2005 Mozilla Corporation
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

#include <math.h>
#include "cairo-test.h"
#include <stdio.h>

#define SIZE 40

static cairo_test_draw_function_t draw;

cairo_test_t test = {
    "source-clip-scale",
    "Test a leftover clip on a source surface not affecting compositing",
    SIZE * 2, SIZE,
    draw
};

static cairo_test_status_t
draw (cairo_t *cr, int width, int height)
{
    cairo_surface_t *surf2;
    cairo_t *cr2;

    /* cr: Fill the destination with our red background that should
     * get covered
     */
    cairo_set_source_rgb (cr, 1, 0, 0);
    cairo_paint (cr);

    surf2 = cairo_surface_create_similar (cairo_get_target (cr), CAIRO_CONTENT_COLOR_ALPHA, SIZE, SIZE);
    cr2 = cairo_create (surf2);

    /* cr2: Fill temp surface with green */
    cairo_set_source_rgb (cr2, 0, 1, 0);
    cairo_paint (cr2);

    /* cr2: Make a blue square in the middle */
    cairo_set_source_rgb (cr2, 0, 0, 2);
    cairo_save (cr2);
    cairo_new_path (cr2);
    cairo_rectangle (cr2, 10, 10, SIZE-20, SIZE-20);
    cairo_clip (cr2);
    cairo_paint (cr2);
    cairo_restore (cr2);

    /* If this is uncommented, the test works as expected, because this
     * forces the clip to be reset on surf2.
     */
    /*
       cairo_new_path (cr2);
       cairo_rectangle (cr2, 0, 0, 0, 0);
       cairo_fill (cr2);
    */

    /* If this scale is commented out, the test displays
     * the green-and-blue square on the left side of the result.
     *
     * The correct "pass" image is the green-and-blue square image stretched
     * by 2x.  With this scale, however, only the blue (clipped) portion
     * of the src shows through.
     */
    cairo_scale (cr, 2.0, 1.0);

    cairo_set_source_surface (cr, surf2, 0, 0);
    cairo_paint (cr);

    cairo_destroy (cr2);
    cairo_surface_destroy (surf2);

    return CAIRO_TEST_SUCCESS;
}

int
main (void)
{
    return cairo_test (&test);
}
