/*
 * Copyright © 2004 Red Hat, Inc.
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

/* Bug history
 *
 * 2004-10-27  Carl Worth  <cworth@cworth.org>
 *
 *   There's currently a regression bug in the tessellation code. This
 *   causes each of these simple star shapes to be filled incorrectly.
 *
 *   It looks like right now we can get this test to pass by doing:
 *
 *       cvs update -r 1.16 src/cairo_traps.c
 *
 *   But we don't want to revert that change permanently since it
 *   really does correct some bugs. It must be that the old version of
 *   the code is masking some other bugs in the tessellation code. My
 *   current plan is to back this revision up for the next snapshot,
 *   but not to list the test as an expected failure since I'm
 *   planning on doing the new tessellator which should fix this
 *   problem.
 *
 */

#include "cairo_test.h"

#define STAR_SIZE 20

cairo_test_t test = {
    "fill_rule",
    "Tests cairo_set_full_rule with a star shape",
    STAR_SIZE * 2 + 3, STAR_SIZE +2
};


/* Not a perfect star, but one that does show the tessellation bug. */
static void
star_path (cairo_t *cr)
{
    cairo_move_to (cr, 10, 0);
    cairo_rel_line_to (cr, 6, 20);
    cairo_rel_line_to (cr, -16, -12);
    cairo_rel_line_to (cr, 20, 0);
    cairo_rel_line_to (cr, -16, 12);
}

/* Fill the same path twice, once with each fill rule */
static void
draw (cairo_t *cr, int width, int height)
{
    cairo_set_rgb_color (cr, 1, 0, 0);

    cairo_translate (cr, 1, 1);
    star_path (cr);
    cairo_set_fill_rule (cr, CAIRO_FILL_RULE_WINDING);
    cairo_fill (cr);

    cairo_translate (cr, STAR_SIZE + 1, 0);
    star_path (cr);
    cairo_set_fill_rule (cr, CAIRO_FILL_RULE_EVEN_ODD);
    cairo_fill (cr);
}

int
main (void)
{
    return cairo_test (&test, draw);
}
