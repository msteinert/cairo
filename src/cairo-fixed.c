/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2003 University of Southern California
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

cairo_fixed_t
_cairo_fixed_from_int (int i)
{
    return i << 16;
}

cairo_fixed_t
_cairo_fixed_from_double (double d)
{
    return (cairo_fixed_t) (d * 65536);
}

cairo_fixed_t
_cairo_fixed_from_26_6 (uint32_t i)
{
    return i << 10;
}

double
_cairo_fixed_to_double (cairo_fixed_t f)
{
    return ((double) f) / 65536.0;
}

int
_cairo_fixed_is_integer (cairo_fixed_t f)
{
    return (f & 0xFFFF) == 0;
}

int
_cairo_fixed_integer_part (cairo_fixed_t f)
{
    return f >> 16;
}
