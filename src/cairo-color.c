/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2002 University of Southern California
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Carl D. Worth <cworth@isi.edu>
 */

#include "cairoint.h"

static cairo_color_t const CAIRO_COLOR_DEFAULT = {
    1.0, 1.0, 1.0, 1.0,
    0xffff, 0xffff, 0xffff, 0xffff
};

static void
_cairo_color_compute_shorts (cairo_color_t *color);

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

void
_cairo_color_set_rgb (cairo_color_t *color, double red, double green, double blue)
{
    color->red   = red;
    color->green = green;
    color->blue  = blue;

    _cairo_color_compute_shorts (color);
}

void
_cairo_color_get_rgb (cairo_color_t *color, double *red, double *green, double *blue)
{
    *red   = color->red;
    *green = color->green;
    *blue  = color->blue;
}

void
_cairo_color_set_alpha (cairo_color_t *color, double alpha)
{
    color->alpha = alpha;

    _cairo_color_compute_shorts (color);
}

static void
_cairo_color_compute_shorts (cairo_color_t *color)
{
    color->red_short   = (color->red   * color->alpha) * 0xffff;
    color->green_short = (color->green * color->alpha) * 0xffff;
    color->blue_short  = (color->blue  * color->alpha) * 0xffff;
    color->alpha_short =  color->alpha * 0xffff;
}

