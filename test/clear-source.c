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

#define ARRAY_LENGTH(x) (sizeof(x) / sizeof(x[0]))
typedef enum {
  CLEAR,
  CLEARED,
  PAINTED
} surface_type_t;

#define SIZE 10
#define SPACE 5

static cairo_surface_t *
create_surface (cairo_t *target, cairo_content_t content, surface_type_t type)
{
    cairo_surface_t *surface;
    cairo_t *cr;

    surface = cairo_surface_create_similar (cairo_get_target (target),
                                            content,
                                            SIZE, SIZE);

    if (type == CLEAR)
        return surface;

    cr = cairo_create (surface);
    cairo_set_source_rgb (cr, 0.75, 0, 0);
    cairo_paint (cr);
    cairo_destroy (cr);

    if (type == PAINTED)
        return surface;

    cr = cairo_create (surface);
    cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint (cr);
    cairo_destroy (cr);

    return surface;
}

static void
paint (cairo_t *cr, cairo_surface_t *surface)
{
    cairo_set_source_surface (cr, surface, 0, 0);
    cairo_paint (cr);
}

static void
fill (cairo_t *cr, cairo_surface_t *surface)
{
    cairo_set_source_surface (cr, surface, 0, 0);
    cairo_rectangle (cr, -SPACE, -SPACE, SIZE + 2 * SPACE, SIZE + 2 * SPACE);
    cairo_fill (cr);
}

static void
stroke (cairo_t *cr, cairo_surface_t *surface)
{
    cairo_set_source_surface (cr, surface, 0, 0);
    cairo_set_line_width (cr, 2.0);
    cairo_rectangle (cr, 1, 1, SIZE - 2, SIZE - 2);
    cairo_stroke (cr);
}

static void
mask (cairo_t *cr, cairo_surface_t *surface)
{
    cairo_set_source_rgb (cr, 0, 0, 0.75);
    cairo_mask_surface (cr, surface, 0, 0);
}

static void
mask_self (cairo_t *cr, cairo_surface_t *surface)
{
    cairo_set_source_surface (cr, surface, 0, 0);
    cairo_mask_surface (cr, surface, 0, 0);
}

static void
glyphs (cairo_t *cr, cairo_surface_t *surface)
{
    cairo_set_source_surface (cr, surface, 0, 0);
    cairo_select_font_face (cr,
			    "@cairo:",
			    CAIRO_FONT_SLANT_NORMAL,
			    CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size (cr, 16);
    cairo_translate (cr, 0, SIZE);
    cairo_show_text (cr, "C");
}

typedef void (* operation_t) (cairo_t *cr, cairo_surface_t *surface);
static operation_t operations[] = {
  paint,
  fill,
  stroke,
  mask,
  mask_self,
  glyphs
};

static cairo_test_status_t
draw (cairo_t *cr, int width, int height)
{
    cairo_content_t contents[] = { CAIRO_CONTENT_COLOR_ALPHA, CAIRO_CONTENT_COLOR, CAIRO_CONTENT_ALPHA };
    unsigned int content, type, ops;
    cairo_surface_t *surface;

    cairo_set_source_rgb (cr, 0.5, 0.5, 0.5);
    cairo_paint (cr);
    cairo_translate (cr, SPACE, SPACE);

    for (type = 0; type <= PAINTED; type++) {
        for (content = 0; content < ARRAY_LENGTH (contents); content++) {
            surface = create_surface (cr, contents[content], type);

            cairo_save (cr);
            for (ops = 0; ops < ARRAY_LENGTH (operations); ops++) {
                cairo_save (cr);
                operations[ops] (cr, surface);
                cairo_restore (cr);
                cairo_translate (cr, 0, SIZE + SPACE);
            }
            cairo_restore (cr);
            cairo_translate (cr, SIZE + SPACE, 0);
        }
    }

    return CAIRO_TEST_SUCCESS;
}

CAIRO_TEST (clear_source,
	    "Check painting with cleared surfaces works as expected",
	    NULL, /* keywords */
	    NULL, /* requirements */
	    (SIZE + SPACE) * 9 + SPACE, ARRAY_LENGTH (operations) * (SIZE + SPACE) + SPACE,
	    NULL, draw)
