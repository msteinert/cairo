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
 * 2004-11-03 Steve Chaplin <stevech1097@yahoo.com.au>
 *
 *   Reported bug on mailing list:
 *
 *	From: Steve Chaplin <stevech1097@yahoo.com.au>
 *	To: cairo@cairographics.org
 *	Date: Thu, 04 Nov 2004 00:00:17 +0800
 *	Subject: [cairo] Rotated text bug on drawable target
 *
 * 	The attached file draws text rotated 90 degrees first to a PNG file and
 *	then to a drawable. The PNG file looks fine, the text on the drawable is
 *	unreadable.
 *
 *	Steve
 *
 * 2004-11-03 Carl Worth <cworth@cworth.org>
 *
 *   Looks like the major problems with this bg appeared in the great
 *   font rework between 0.1.23 and 0.2.0. And it looks like we need
 *   to fix the regression test suite to test the xlib target (since
 *   the bug does not show up in the png backend).
 *
 *   Hmm... Actually, things don't look perfect even in the PNG
 *   output. Look at how that 'o' moves around. It's particularly off
 *   in the case where it's rotated by PI.
 *
 *   And I'm still not sure about what to do for test cases with
 *   text--a new version of freetype will change everything. We may
 *   need to add a simple backend for stroked fonts and add a simple
 *   builtin font to cairo for pixel-perfect tests with text.
 */

#include "cairo_test.h"

#define WIDTH  100
#define HEIGHT 100
#define NUM_TEXT 8
#define TEXT_SIZE 10

cairo_test_t test = {
    "text_rotate",
    "Tests show_text under various rotations",
    WIDTH, HEIGHT
};

/* Draw the word cairo at NUM_TEXT different angles */
static void
draw (cairo_t *cr, int width, int height)
{
    int i;
    cairo_text_extents_t extents;
    static char text[] = "cairo";

    cairo_select_font (cr, "Bitstream Vera Sans",
		       CAIRO_FONT_SLANT_NORMAL,
		       CAIRO_FONT_WEIGHT_NORMAL);
    cairo_scale_font (cr, TEXT_SIZE);

    cairo_set_rgb_color (cr, 0,0,0);

    cairo_translate (cr, WIDTH/2.0, HEIGHT/2.0);

    cairo_text_extents (cr, text, &extents);
  
    for (i=0; i < 8; i++) {
	cairo_save (cr);
	cairo_rotate (cr, 2*M_PI*i/NUM_TEXT);
	/* XXX: extents.height / 4.0 gets the right result here, but I
	 * would think it should be extents.height / 2.0. Perhaps I'm
	 * using the extents incorrectly, (really need to go write
	 * that reference on cairo_text_extents with a good
	 * diagram...).
	 */
	cairo_move_to (cr,
		       extents.height / (2 * tan (2*M_PI/NUM_TEXT)),
		       extents.height / 4.0);
	cairo_show_text (cr, "cairo");
	cairo_restore (cr);
    }
}

int
main (void)
{
    return cairo_test (&test, draw);
}
