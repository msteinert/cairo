/* -*- Mode: c; tab-width: 8; c-basic-offset: 4; indent-tabs-mode: t; -*- */
/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2002 University of Southern California
 * Copyright © 2005 Red Hat, Inc.
 * Copyright © 2011 Intel Corporation
 * Copyright © 2011 Samsung Electronics
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
 * The Initial Developer of the Original Code is University of Southern
 * California.
 *
 * Contributor(s):
 *	Henry Song <hsong@sisa.samsung.com>
 *	Martin Robinson <mrobinson@igalia.com>
 */

#include "cairoint.h"

#include "cairo-clip-private.h"
#include "cairo-composite-rectangles-private.h"
#include "cairo-compositor-private.h"
#include "cairo-gl-private.h"
#include "cairo-traps-private.h"

static cairo_int_status_t
_cairo_gl_msaa_compositor_paint (const cairo_compositor_t	*compositor,
				 cairo_composite_rectangles_t	*composite)
{
    return CAIRO_INT_STATUS_UNSUPPORTED;
}

static cairo_int_status_t
_cairo_gl_msaa_compositor_mask (const cairo_compositor_t	*compositor,
				cairo_composite_rectangles_t	*composite)
{
    return CAIRO_INT_STATUS_UNSUPPORTED;
}

static cairo_int_status_t
_cairo_gl_msaa_compositor_stroke (const cairo_compositor_t	*compositor,
				  cairo_composite_rectangles_t	*composite,
				  const cairo_path_fixed_t	*path,
				  const cairo_stroke_style_t	*style,
				  const cairo_matrix_t		*ctm,
				  const cairo_matrix_t		*ctm_inverse,
				  double			 tolerance,
				  cairo_antialias_t		 antialias)
{
    return CAIRO_INT_STATUS_UNSUPPORTED;
}

static cairo_int_status_t
_cairo_gl_msaa_compositor_fill (const cairo_compositor_t	*compositor,
				cairo_composite_rectangles_t	*composite,
				const cairo_path_fixed_t	*path,
				cairo_fill_rule_t		 fill_rule,
				double				 tolerance,
				cairo_antialias_t		 antialias)
{
    return CAIRO_INT_STATUS_UNSUPPORTED;
}

static void
_cairo_gl_msaa_compositor_init (cairo_compositor_t	 *compositor,
				const cairo_compositor_t *delegate)
{
    compositor->delegate = delegate;

    compositor->paint = _cairo_gl_msaa_compositor_paint;
    compositor->mask = _cairo_gl_msaa_compositor_mask;
    compositor->fill = _cairo_gl_msaa_compositor_fill;
    compositor->stroke = _cairo_gl_msaa_compositor_stroke;
    /* always fallback to common glyph cache implmentation */
}

const cairo_compositor_t *
_cairo_gl_msaa_compositor_get (void)
{
    static cairo_compositor_t compositor;
    if (compositor.delegate == NULL)
	_cairo_gl_msaa_compositor_init (&compositor,
					_cairo_gl_span_compositor_get ());

    return &compositor;
}
