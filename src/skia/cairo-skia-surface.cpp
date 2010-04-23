/* -*- Mode: c++; c-basic-offset: 4; indent-tabs-mode: t; tab-width: 8; -*- */
/* cairo - a vector graphics library with display and print output
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
 * The Initial Developer of the Original Code is Mozilla Corporation.
 *
 * Contributor(s):
 *	Vladimir Vukicevic <vladimir@mozilla.com>
 */

#include "cairoint.h"

#include "cairo-skia.h"
#include "cairo-skia-private.h"

#include "cairo-composite-rectangles-private.h"
#include "cairo-error-private.h"

static cairo_skia_surface_t *
_cairo_skia_surface_create_internal (SkBitmap::Config config,
				     bool opaque,
				     unsigned char *data,
				     int width,
				     int height,
				     int stride);

static cairo_surface_t *
_cairo_skia_surface_create_similar (void *asurface,
				    cairo_content_t content,
				    int width,
				    int height)
{
    cairo_skia_surface_t *surface = (cairo_skia_surface_t *) asurface;
    SkBitmap::Config config;
    bool opaque;

    if (content == surface->image.base.content)
    {
	config = surface->bitmap->getConfig ();
	opaque = surface->bitmap->isOpaque ();
    }
    else if (! format_to_sk_config (_cairo_format_from_content (content),
				    config, opaque))
    {
	return NULL;
    }

    return &_cairo_skia_surface_create_internal (config, opaque,
						 NULL,
						 width, height,
						 0)->image.base;
}

static cairo_status_t
_cairo_skia_surface_finish (void *asurface)
{
    cairo_skia_surface_t *surface = (cairo_skia_surface_t *) asurface;

    cairo_surface_finish (&surface->image.base);
    delete surface->bitmap;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_surface_t *
_cairo_skia_surface_map_to_image (void *asurface,
				  const cairo_rectangle_int_t *extents)
{
    cairo_skia_surface_t *surface = (cairo_skia_surface_t *) asurface;

    surface->bitmap->lockPixels ();

    if (extents->width < surface->image.width ||
	extents->height < surface->image.height)
    {
	return _cairo_surface_create_for_rectangle_int (&surface->image.base,
							extents);
    }

    return cairo_surface_reference (&surface->image.base);
}

static cairo_int_status_t
_cairo_skia_surface_unmap_image (void *asurface,
				 cairo_image_surface_t *image)
{
    cairo_skia_surface_t *surface = (cairo_skia_surface_t *) asurface;

    surface->bitmap->unlockPixels ();
    return CAIRO_INT_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_skia_surface_acquire_source_image (void *asurface,
					  cairo_image_surface_t **image_out,
					  void **image_extra)
{
    cairo_skia_surface_t *surface = (cairo_skia_surface_t *) asurface;

    surface->bitmap->lockPixels ();

    *image_out = &surface->image;
    *image_extra = NULL;
    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_skia_surface_release_source_image (void *asurface,
					  cairo_image_surface_t *image,
					  void *image_extra)
{
    cairo_skia_surface_t *surface = (cairo_skia_surface_t *) asurface;

    surface->bitmap->unlockPixels ();
}

static cairo_bool_t
_cairo_skia_surface_get_extents (void *asurface,
				  cairo_rectangle_int_t *extents)
{
    cairo_skia_surface_t *surface = (cairo_skia_surface_t *) asurface;
    extents->x = extents->y = 0;
    extents->width  = surface->image.width;
    extents->height = surface->image.height;
    return TRUE;
}

static void
_cairo_skia_surface_get_font_options (void                  *abstract_surface,
				       cairo_font_options_t  *options)
{
    _cairo_font_options_init_default (options);

    cairo_font_options_set_hint_metrics (options, CAIRO_HINT_METRICS_ON);
    _cairo_font_options_set_round_glyph_positions (options, CAIRO_ROUND_GLYPH_POS_ON);
}

static cairo_rectangle_t *
to_rectangle (cairo_rectangle_t *rf,
	      cairo_rectangle_int_t *ri)
{
    rf->x = ri->x;
    rf->y = ri->y;
    rf->width = ri->width;
    rf->height = ri->height;
    return rf;
}

static cairo_int_status_t
_cairo_foreign_surface_paint (void			*abstract_surface,
			      cairo_operator_t		 op,
			      const cairo_pattern_t	*source,
			      const cairo_clip_t	*clip)
{
    cairo_surface_t *surface = (cairo_surface_t *) abstract_surface;
    cairo_surface_t *image;
    cairo_rectangle_int_t extents;
    cairo_rectangle_t rect;
    cairo_composite_rectangles_t composite;
    cairo_int_status_t status;

    _cairo_surface_get_extents (surface, &extents);
    status = _cairo_composite_rectangles_init_for_paint (&composite, &extents,
							 op, source,
							 clip);
    if (unlikely (status))
	return status;

    image = cairo_surface_map_to_image (surface,
					to_rectangle(&rect, &composite.unbounded));
    status = (cairo_int_status_t)
	_cairo_surface_paint (image, op, source, clip);
    cairo_surface_unmap_image (surface, image);

    _cairo_composite_rectangles_fini (&composite);

    return status;
}

static cairo_int_status_t
_cairo_foreign_surface_mask (void			*abstract_surface,
			      cairo_operator_t		 op,
			      const cairo_pattern_t	*source,
			      const cairo_pattern_t	*mask,
			      const cairo_clip_t	*clip)
{
    cairo_surface_t *surface =(cairo_surface_t *) abstract_surface;
    cairo_surface_t *image;
    cairo_rectangle_int_t extents;
    cairo_rectangle_t rect;
    cairo_composite_rectangles_t composite;
    cairo_int_status_t status;

    _cairo_surface_get_extents (surface, &extents);
    status = _cairo_composite_rectangles_init_for_mask (&composite, &extents,
							op, source, mask,
							clip);
    if (unlikely (status))
	return status;

    image = cairo_surface_map_to_image (surface,
					to_rectangle(&rect, &composite.unbounded));
    status = (cairo_int_status_t)
	_cairo_surface_mask (image, op, source, mask, clip);
    cairo_surface_unmap_image (surface, image);

    _cairo_composite_rectangles_fini (&composite);

    return status;
}

static cairo_int_status_t
_cairo_foreign_surface_stroke (void			*abstract_surface,
			       cairo_operator_t		 op,
			       const cairo_pattern_t	*source,
			       const cairo_path_fixed_t	*path,
			       const cairo_stroke_style_t*style,
			       const cairo_matrix_t	*ctm,
			       const cairo_matrix_t	*ctm_inverse,
			       double			 tolerance,
			       cairo_antialias_t	 antialias,
			       const cairo_clip_t	*clip)
{
    cairo_surface_t *surface =(cairo_surface_t *) abstract_surface;
    cairo_surface_t *image;
    cairo_composite_rectangles_t composite;
    cairo_rectangle_int_t extents;
    cairo_rectangle_t rect;
    cairo_int_status_t status;

    _cairo_surface_get_extents (surface, &extents);
    status = _cairo_composite_rectangles_init_for_stroke (&composite, &extents,
							  op, source,
							  path, style, ctm,
							  clip);
    if (unlikely (status))
	return status;

    image = cairo_surface_map_to_image (surface,
					to_rectangle(&rect, &composite.unbounded));
    status = (cairo_int_status_t)
	_cairo_surface_stroke (image, op, source, path, style, ctm, ctm_inverse, tolerance, antialias, clip);
    cairo_surface_unmap_image (surface, image);

    _cairo_composite_rectangles_fini (&composite);

    return status;
}

static cairo_int_status_t
_cairo_foreign_surface_fill (void				*abstract_surface,
			     cairo_operator_t		 op,
			     const cairo_pattern_t	*source,
			     const cairo_path_fixed_t	*path,
			     cairo_fill_rule_t		 fill_rule,
			     double			 tolerance,
			     cairo_antialias_t		 antialias,
			     const cairo_clip_t		*clip)
{
    cairo_surface_t *surface =(cairo_surface_t *) abstract_surface;
    cairo_surface_t *image;
    cairo_composite_rectangles_t composite;
    cairo_rectangle_int_t extents;
    cairo_rectangle_t rect;
    cairo_int_status_t status;

    _cairo_surface_get_extents (surface, &extents);
    status = _cairo_composite_rectangles_init_for_fill (&composite, &extents,
							op, source, path,
							clip);
    if (unlikely (status))
	return status;

    image = cairo_surface_map_to_image (surface,
					to_rectangle(&rect, &composite.unbounded));
    status = (cairo_int_status_t)
	_cairo_surface_fill (image, op, source, path, fill_rule, tolerance, antialias, clip);
    cairo_surface_unmap_image (surface, image);

    _cairo_composite_rectangles_fini (&composite);

    return status;
}

static cairo_int_status_t
_cairo_foreign_surface_glyphs (void			*abstract_surface,
			       cairo_operator_t		 op,
			       const cairo_pattern_t	*source,
			       cairo_glyph_t		*glyphs,
			       int			 num_glyphs,
			       cairo_scaled_font_t	*scaled_font,
			       const cairo_clip_t	*clip,
			       int *num_remaining)
{
    cairo_surface_t *surface =(cairo_surface_t *) abstract_surface;
    cairo_surface_t *image;
    cairo_composite_rectangles_t composite;
    cairo_rectangle_int_t extents;
    cairo_rectangle_t rect;
    cairo_int_status_t status;
    cairo_bool_t overlap;

    _cairo_surface_get_extents (surface, &extents);
    status = _cairo_composite_rectangles_init_for_glyphs (&composite, &extents,
							  op, source,
							  scaled_font,
							  glyphs, num_glyphs,
							  clip,
							  &overlap);
    if (unlikely (status))
	return status;

    image = cairo_surface_map_to_image (surface,
					to_rectangle(&rect, &composite.unbounded));
    status = (cairo_int_status_t)
	_cairo_surface_show_text_glyphs (image,
					 op, source,
					 NULL, 0,
					 glyphs, num_glyphs,
					 NULL, 0, (cairo_text_cluster_flags_t)0,
					 scaled_font,
					 clip);
    cairo_surface_unmap_image (surface, image);
    _cairo_composite_rectangles_fini (&composite);

    *num_remaining = 0;
    return status;
}

static const struct _cairo_surface_backend
cairo_skia_surface_backend = {
    CAIRO_SURFACE_TYPE_SKIA,
    _cairo_skia_surface_finish,

    _cairo_skia_context_create,

    _cairo_skia_surface_create_similar,
    NULL, //_cairo_skia_surface_create_similar_image,
    _cairo_skia_surface_map_to_image,
    _cairo_skia_surface_unmap_image,

    _cairo_skia_surface_acquire_source_image,
    _cairo_skia_surface_release_source_image,

    NULL, NULL,
    NULL, /* clone similar */
    NULL, /* composite */
    NULL, /* fill_rectangles */
    NULL, /* composite_trapezoids */
    NULL, /* create_span_renderer */
    NULL, /* check_span_renderer */

    NULL, /* copy_page */
    NULL, /* show_page */

    _cairo_skia_surface_get_extents,
    NULL, /* old_show_glyphs */
    _cairo_skia_surface_get_font_options,
    NULL, /* flush */
    NULL, /* mark_dirty_rectangle */
    NULL, /* scaled_font_fini */
    NULL, /* scaled_glyph_fini */

    /* XXX native surface functions? */
    _cairo_foreign_surface_paint,
    _cairo_foreign_surface_mask,
    _cairo_foreign_surface_stroke,
    _cairo_foreign_surface_fill,
    _cairo_foreign_surface_glyphs
};

/*
 * Surface constructors
 */

static inline pixman_format_code_t
sk_config_to_pixman_format_code (SkBitmap::Config config,
				 bool opaque)
{
    switch (config) {
    case SkBitmap::kARGB_8888_Config:
	return opaque ? PIXMAN_x8r8g8b8 : PIXMAN_a8r8g8b8;

    case SkBitmap::kA8_Config:
	return PIXMAN_a8;

    case SkBitmap::kA1_Config:
	return PIXMAN_a1;
    case SkBitmap::kRGB_565_Config:
	return PIXMAN_r5g6b5;
    case SkBitmap::kARGB_4444_Config:
	return PIXMAN_a4r4g4b4;

    case SkBitmap::kNo_Config:
    case SkBitmap::kIndex8_Config:
    case SkBitmap::kRLE_Index8_Config:
    case SkBitmap::kConfigCount:
    default:
	ASSERT_NOT_REACHED;
	return (pixman_format_code_t) -1;
    }
}
static cairo_skia_surface_t *
_cairo_skia_surface_create_internal (SkBitmap::Config config,
				     bool opaque,
				     unsigned char *data,
				     int width,
				     int height,
				     int stride)
{
    cairo_skia_surface_t *surface;
    pixman_image_t *pixman_image;
    pixman_format_code_t pixman_format;

    surface = (cairo_skia_surface_t *) malloc (sizeof (cairo_skia_surface_t));
    if (unlikely (surface == NULL))
	return (cairo_skia_surface_t *) _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    pixman_format = sk_config_to_pixman_format_code (config, opaque);
    pixman_image = pixman_image_create_bits (pixman_format,
					     width, height,
					     (uint32_t *) data, stride);
    if (unlikely (pixman_image == NULL))
	return (cairo_skia_surface_t *) _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    _cairo_surface_init (&surface->image.base,
			 &cairo_skia_surface_backend,
			 NULL, /* device */
			 _cairo_content_from_pixman_format (pixman_format));

    _cairo_image_surface_init (&surface->image, pixman_image, pixman_format);

    surface->bitmap = new SkBitmap;
    surface->bitmap->setConfig (config, width, height, surface->image.stride);
    surface->bitmap->setIsOpaque (opaque);
    surface->bitmap->setPixels (surface->image.data);

    surface->image.base.is_clear = data == NULL;

    return surface;
}

cairo_surface_t *
cairo_skia_surface_create (cairo_format_t format,
			   int width,
			   int height)
{
    SkBitmap::Config config;
    bool opaque;

    if (! CAIRO_FORMAT_VALID (format) ||
	! format_to_sk_config (format, config, opaque))
    {
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_FORMAT));
    }

    return &_cairo_skia_surface_create_internal (config, opaque, NULL, width, height, 0)->image.base;
}

cairo_surface_t *
cairo_skia_surface_create_for_data (unsigned char *data,
				    cairo_format_t format,
				    int width,
				    int height,
				    int stride)
{
    SkBitmap::Config config;
    bool opaque;

    if (! CAIRO_FORMAT_VALID (format) ||
	! format_to_sk_config (format, config, opaque))
    {
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_FORMAT));
    }

    return &_cairo_skia_surface_create_internal (config, opaque, data, width, height, stride)->image.base;
}

/***

Todo:

*** Skia:

- mask()

*** Sk:

High:
- antialiased clipping?

Medium:
- implement clip path reset (to avoid restore/save)
- implement complex radial patterns (2 centers and 2 radii)

Low:
- implement EXTEND_NONE

***/
