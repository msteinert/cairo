/* -*- Mode: c; tab-width: 8; c-basic-offset: 4; indent-tabs-mode: t; -*- */
/* Cairo - a vector graphics library with display and print output
 *
 * Copyright © 2007 Mozilla Corporation
 *
 * This library is free software; you can redistribute it and/or
 * modify it either under the terms of the GNU Lesser General Public
 * License version 2.1 as published by the Free Software Foundation
 * (the "LGPL") or, at your option, under the terms of the Mozilla
 * Public License Version 1.1 (the "MPL"). If you do not alter this
 * notice, a recipient may use your version of this file under either
 * the MPL or the LGPL.
 *
 * You should have received a copy of the LGPL along with this library
 * in the file COPYING-LGPL-2.1; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 * You should have received a copy of the MPL along with this library
 * in the file COPYING-MPL-1.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY
 * OF ANY KIND, either express or implied. See the LGPL or the MPL for
 * the specific language governing rights and limitations.
 *
 * The Original Code is the cairo graphics library.
 *
 * The Initial Developer of the Original Code is Mozilla Corporation
 *
 * Contributor(s):
 *	Vladimir Vukicevic <vladimir@pobox.com>
 */

#ifndef CAIRO_REGION_PRIVATE_H
#define CAIRO_REGION_PRIVATE_H

#include "cairo-compiler-private.h"
#include "cairo-types-private.h"

#include <pixman.h>

CAIRO_BEGIN_DECLS

/* #cairo_region_t is defined in cairoint.h */

struct _cairo_region {
    cairo_status_t status;
    
    pixman_region32_t rgn;
};

typedef enum _cairo_region_overlap {
    CAIRO_REGION_OVERLAP_IN,		/* completely inside region */
    CAIRO_REGION_OVERLAP_OUT,		/* completely outside region */
    CAIRO_REGION_OVERLAP_PART,		/* partly inside region */
} cairo_region_overlap_t;

cairo_private cairo_region_t *
cairo_region_create (void);

cairo_private cairo_region_t *
cairo_region_create_rect (cairo_rectangle_int_t *rect);

cairo_private cairo_status_t
cairo_region_status (cairo_region_t *region);

cairo_private void
cairo_region_clear (cairo_region_t *region);

cairo_private cairo_region_t *
cairo_region_create_rectangles (cairo_rectangle_int_t *rects,
				 int count);

cairo_private void
cairo_region_destroy (cairo_region_t *region);

cairo_private cairo_region_t *
cairo_region_copy (cairo_region_t *original);

cairo_private int
cairo_region_num_rectangles (cairo_region_t *region);

cairo_private void
cairo_region_get_rectangle (cairo_region_t *region,
			     int nth_rectangle,
			     cairo_rectangle_int_t *rectangle);

cairo_private void
cairo_region_get_extents (cairo_region_t *region,
			   cairo_rectangle_int_t *extents);

cairo_private cairo_status_t
cairo_region_subtract (cairo_region_t *dst,
			cairo_region_t *other);

cairo_private cairo_status_t
cairo_region_intersect (cairo_region_t *dst,
			 cairo_region_t *other);

cairo_private cairo_status_t
cairo_region_union (cairo_region_t *dst,
		     cairo_region_t *other);

cairo_private cairo_status_t
cairo_region_union_rect (cairo_region_t *dst,
			  cairo_rectangle_int_t *rect);

cairo_private cairo_bool_t
cairo_region_empty (cairo_region_t *region);

cairo_private void
cairo_region_translate (cairo_region_t *region,
			 int dx, int dy);

cairo_private cairo_region_overlap_t
cairo_region_contains_rectangle (cairo_region_t *region,
				  const cairo_rectangle_int_t *rect);

cairo_private cairo_bool_t
cairo_region_contains_point (cairo_region_t *region,
			      int x, int y);

CAIRO_END_DECLS

#endif /* CAIRO_REGION_PRIVATE_H */
