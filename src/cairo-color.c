/*
 * Copyright © 2002 USC, Information Sciences Institute
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of the
 * University of Southern California not be used in advertising or
 * publicity pertaining to distribution of the software without
 * specific, written prior permission. The University of Southern
 * California makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 *
 * THE UNIVERSITY OF SOUTHERN CALIFORNIA DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL THE UNIVERSITY OF
 * SOUTHERN CALIFORNIA BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author: Carl D. Worth <cworth@isi.edu>
 */

#include "cairoint.h"

static cairo_color_t CAIRO_COLOR_DEFAULT = { 1.0, 1.0, 1.0, 1.0, {0xffff, 0xffff, 0xffff, 0xffff}};

static void
_cairo_color_compute_xc_color (cairo_color_t *color);

void
_cairo_color_init (cairo_color_t *color)
{
    *color = CAIRO_COLOR_DEFAULT;
}

void
_cairo_color_fini (cairo_color_t *color)
{
    /* Nothing to do here */
}

static void
_cairo_color_compute_xc_color (cairo_color_t *color)
{
    color->xc_color.red = color->red * color->alpha * 0xffff;
    color->xc_color.green = color->green * color->alpha * 0xffff;
    color->xc_color.blue = color->blue * color->alpha * 0xffff;
    color->xc_color.alpha = color->alpha * 0xffff;
}

void
_cairo_color_set_rgb (cairo_color_t *color, double red, double green, double blue)
{
    color->red = red;
    color->green = green;
    color->blue = blue;

    _cairo_color_compute_xc_color (color);
}

void
_cairo_color_get_rgb (cairo_color_t *color, double *red, double *green, double *blue)
{
    *red = color->red;
    *green = color->green;
    *blue = color->blue;
}

void
_cairo_color_set_alpha (cairo_color_t *color, double alpha)
{
    color->alpha = alpha;

    _cairo_color_compute_xc_color (color);
}
