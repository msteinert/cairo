/* Cairo - a vector graphics library with display and print output
 *
 * Copyright © 2013 EchoStar Corporation
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
 * Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA 02110-1335, USA
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
 * The Initial Developer of the Original Code is Mike Steinert
 *
 * Contributors(s):
 *	Mike Steinert <mike.steinert@gmail.com>
 */

#ifndef CAIRO_DIRECTFB_PRIVATE_H
#define CAIRO_DIRECTFB_PRIVATE_H

#include "cairo-directfb.h"
#include "cairoint.h"
#include <directfb.h>

/* surface interface */

cairo_private cairo_int_status_t
_cairo_dfb_surface_fill_boxes (cairo_surface_t		*base,
			       cairo_operator_t		 op,
			       const cairo_pattern_t	*src,
			       cairo_boxes_t		*boxes);

cairo_private cairo_int_status_t
_cairo_dfb_surface_draw_glyphs (cairo_surface_t		*base,
				cairo_operator_t	 op,
				const cairo_color_t	*color,
				cairo_scaled_font_t	*scaled_font,
				cairo_glyph_t		*glyphs,
				int			 num_glyphs);

cairo_private cairo_int_status_t
_cairo_dfb_surface_set_clip (cairo_surface_t	*base,
			     cairo_clip_t	*clip);

cairo_private void
_cairo_dfb_surface_reset_clip (cairo_surface_t *base);

cairo_private cairo_int_status_t
_cairo_dfb_surface_set_mask (cairo_surface_t *base,
			     cairo_surface_t *mask);

cairo_private void
_cairo_dfb_surface_reset_mask (cairo_surface_t *base);

/* compositor interface */

cairo_private void
_cairo_dfb_compositor_get (cairo_compositor_t			*self,
			   cairo_directfb_acceleration_flags_t	 flags);

/* font interface */

typedef struct cairo_dfb_font cairo_dfb_font_t;

cairo_private cairo_dfb_font_t *
_cairo_dfb_font_create (cairo_scaled_font_t	*scaled_font,
			IDirectFB		*dfb);

cairo_private void
_cairo_dfb_font_destroy (cairo_dfb_font_t *self);

cairo_private cairo_int_status_t
_cairo_dfb_font_get_rectangles (cairo_dfb_font_t	 *self,
				cairo_glyph_t		 *glyphs,
				int			  num_glyphs,
				DFBRectangle		**rects,
				DFBPoint		**points,
				IDirectFBSurface	**cache);

#endif /* CAIRO_DIRECTFB_PRIVATE_H */
