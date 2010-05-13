/*
 * Copyright © 2010 Red Hat, Inc.
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
 * Author: Benjamin Otte <otte@gnome.org>
 */

#include "cairo-test.h"

#define RECT 10
#define SPACE 5

static void
paint_with_alpha (cairo_t *cr)
{
    cairo_paint_with_alpha (cr, 0.0);
}

static void
mask_with_solid (cairo_t *cr)
{
    cairo_pattern_t *pattern = cairo_pattern_create_rgba (1, 0, 0, 0);

    cairo_mask (cr, pattern);
    
    cairo_pattern_destroy (pattern);
}

static void
mask_with_empty_gradient (cairo_t *cr)
{
    cairo_pattern_t *pattern = cairo_pattern_create_linear (1, 2, 3, 4);

    cairo_mask (cr, pattern);
    
    cairo_pattern_destroy (pattern);
}

static void
mask_with_gradient (cairo_t *cr)
{
    cairo_pattern_t *pattern = cairo_pattern_create_radial (1, 2, 3, 4, 5, 6);

    cairo_pattern_add_color_stop_rgba (pattern, 0, 1, 0, 0, 0);
    cairo_pattern_add_color_stop_rgba (pattern, 0, 0, 0, 1, 0);

    cairo_mask (cr, pattern);
    
    cairo_pattern_destroy (pattern);
}

static void
mask_with_surface (cairo_t *cr)
{
    cairo_surface_t *surface = cairo_surface_create_similar (cairo_get_target (cr),
                                                             CAIRO_CONTENT_COLOR_ALPHA,
                                                             RECT,
                                                             RECT);

    cairo_mask_surface (cr, surface, 0, 0);
    
    cairo_surface_destroy (surface);
}

static void
mask_with_alpha_surface (cairo_t *cr)
{
    cairo_surface_t *surface = cairo_surface_create_similar (cairo_get_target (cr),
                                                             CAIRO_CONTENT_ALPHA,
                                                             RECT / 2,
                                                             RECT / 2);
    cairo_pattern_t *pattern = cairo_pattern_create_for_surface (surface);
    cairo_pattern_set_extend (pattern, CAIRO_EXTEND_REFLECT);

    cairo_mask (cr, pattern);
    
    cairo_pattern_destroy (pattern);
    cairo_surface_destroy (surface);
}

static void
mask_with_nonclear_surface (cairo_t *cr)
{
    static unsigned char data[8 * 4] = { 0, };
    cairo_surface_t *surface = cairo_image_surface_create_for_data (data,
                                                                    CAIRO_FORMAT_A1,
                                                                    16, 8, 4);

    cairo_mask_surface (cr, surface, 0, 0);
    
    cairo_surface_destroy (surface);
}

static void
mask_with_0x0_surface (cairo_t *cr)
{
    cairo_surface_t *surface = cairo_surface_create_similar (cairo_get_target (cr),
                                                             CAIRO_CONTENT_COLOR_ALPHA,
                                                             0, 0);

    cairo_mask_surface (cr, surface, 0, 0);
    
    cairo_surface_destroy (surface);
}

static void
mask_with_extend_none (cairo_t *cr)
{
    cairo_surface_t *surface = cairo_surface_create_similar (cairo_get_target (cr),
                                                             CAIRO_CONTENT_COLOR_ALPHA,
                                                             RECT,
                                                             RECT);

    cairo_mask_surface (cr, surface, 2 * RECT, 2 * RECT);
    
    cairo_surface_destroy (surface);
}

#define ARRAY_LENGTH(array) (sizeof (array) / sizeof ((array)[0]))

typedef void (* mask_func_t) (cairo_t *);

mask_func_t mask_funcs[] = {
  paint_with_alpha,
  mask_with_solid,
  mask_with_empty_gradient,
  mask_with_gradient,
  mask_with_surface,
  mask_with_alpha_surface,
  mask_with_nonclear_surface,
  mask_with_0x0_surface,
  mask_with_extend_none
};

cairo_operator_t operators[] = {
  CAIRO_OPERATOR_CLEAR,
  CAIRO_OPERATOR_SOURCE,
  CAIRO_OPERATOR_OVER,
  CAIRO_OPERATOR_IN,
  CAIRO_OPERATOR_DEST_ATOP,
  CAIRO_OPERATOR_SATURATE,
  CAIRO_OPERATOR_MULTIPLY
};

static cairo_test_status_t
draw (cairo_t *cr, int width, int height)
{
    unsigned int i, op;

    /* 565-compatible gray background */
    cairo_set_source_rgb (cr, 0.51613, 0.55555, 0.51613);
    cairo_paint (cr);

    cairo_set_source_rgb (cr, 0.0, 1.0, 0.0); /* green */
    /* mask with zero-alpha in several ways */

    cairo_translate (cr, SPACE, SPACE);

    for (op = 0; op < ARRAY_LENGTH (operators); op++) {
        cairo_set_operator (cr, operators[op]);
        
        for (i = 0; i < ARRAY_LENGTH (mask_funcs); i++) {
            cairo_save (cr);
            cairo_translate (cr, i * (RECT + SPACE), op * (RECT + SPACE));
            cairo_rectangle (cr, 0, 0, RECT, RECT);
            cairo_clip (cr);
            mask_funcs[i] (cr);
            cairo_restore (cr);
        }
    }

    return CAIRO_TEST_SUCCESS;
}

CAIRO_TEST (zero_mask,
	    "Testing that masking with zero alpha works",
	    "alpha, mask", /* keywords */
	    NULL, /* requirements */
	    SPACE + (RECT + SPACE) * ARRAY_LENGTH (mask_funcs),
	    SPACE + (RECT + SPACE) * ARRAY_LENGTH (operators),
	    NULL, draw)
