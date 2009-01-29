/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2005 Red Hat, Inc
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
 * The Initial Developer of the Original Code is Red Hat, Inc.
 *
 * Contributor(s):
 *	Carl Worth <cworth@cworth.org>
 */

#include "cairoint.h"

#include "cairo-gl.h"

typedef struct _cairo_gl_surface {
    cairo_surface_t base;

    /* This is a cairo_image_surface to hold the actual contents. */
    cairo_surface_t *backing;
} cairo_gl_surface_t;

struct _cairo_gl_context {
    cairo_reference_count_t ref_count;
    cairo_status_t status;

    Display *dpy;
    GLXContext gl_ctx;
    cairo_mutex_t mutex; /* needed? */
    cairo_gl_surface_t *current_target;
};

static const cairo_surface_backend_t cairo_gl_surface_backend;

const cairo_gl_context_t _nil_context = {
    CAIRO_REFERENCE_COUNT_INVALID,
    CAIRO_STATUS_NO_MEMORY
};

static cairo_gl_context_t *
_cairo_gl_context_create_in_error (cairo_status_t status)
{
    if (status == CAIRO_STATUS_NO_MEMORY)
	return (cairo_gl_context_t *) &_nil_context;

    ASSERT_NOT_REACHED;
}

cairo_gl_context_t *
cairo_gl_glx_context_create (Display *dpy, GLXContext gl_ctx)
{
    cairo_gl_context_t *ctx;

    ctx = calloc (1, sizeof(cairo_gl_context_t));
    if (ctx == NULL)
	return _cairo_gl_context_create_in_error (CAIRO_STATUS_NO_MEMORY);

    CAIRO_REFERENCE_COUNT_INIT (&ctx->ref_count, 1);
    ctx->dpy = dpy;
    ctx->gl_ctx = gl_ctx;

    /* Make our GL context active.  While we'll be setting the destination
     * drawable with each rendering operation, in order to set the context
     * we have to choose a drawable.  The root window happens to be convenient
     * for this.
     */
    glXMakeCurrent(dpy, RootWindow (dpy, DefaultScreen (dpy)), gl_ctx);

    return ctx;
}

void
cairo_gl_context_destroy (cairo_gl_context_t *context)
{
    if (context == NULL ||
	CAIRO_REFERENCE_COUNT_IS_INVALID (&context->ref_count))
    {
	return;
    }

    assert (CAIRO_REFERENCE_COUNT_HAS_REFERENCE (&context->ref_count));
    if (! _cairo_reference_count_dec_and_test (&context->ref_count))
	return;

    free (context);
}

cairo_surface_t *
cairo_gl_surface_create (cairo_gl_context_t   *ctx,
			 cairo_content_t	content,
			 int			width,
			 int			height)
{
    cairo_gl_surface_t *surface;
    cairo_surface_t *backing;

    backing = _cairo_image_surface_create_with_content (content, width, height);
    if (cairo_surface_status (backing))
	return backing;

    surface = malloc (sizeof (cairo_gl_surface_t));
    if (unlikely (surface == NULL)) {
	cairo_surface_destroy (backing);
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));
    }

    _cairo_surface_init (&surface->base, &cairo_gl_surface_backend,
			 content);

    surface->backing = backing;

    return &surface->base;
}

static cairo_surface_t *
_cairo_gl_surface_create_similar (void		 *abstract_surface,
				  cairo_content_t  content,
				  int		  width,
				  int		  height)
{
    assert (CAIRO_CONTENT_VALID (content));

    return cairo_gl_surface_create (NULL, content, width, height);
}

static cairo_status_t
_cairo_gl_surface_finish (void *abstract_surface)
{
    cairo_gl_surface_t *surface = abstract_surface;

    cairo_surface_destroy (surface->backing);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_gl_surface_acquire_source_image (void		       *abstract_surface,
					cairo_image_surface_t **image_out,
					void		      **image_extra)
{
    cairo_gl_surface_t *surface = abstract_surface;

    return _cairo_surface_acquire_source_image (surface->backing,
						image_out, image_extra);
}

static void
_cairo_gl_surface_release_source_image (void		      *abstract_surface,
					cairo_image_surface_t *image,
					void		      *image_extra)
{
    cairo_gl_surface_t *surface = abstract_surface;

    _cairo_surface_release_source_image (surface->backing,
					 image, image_extra);
}

static cairo_status_t
_cairo_gl_surface_acquire_dest_image (void		      *abstract_surface,
				      cairo_rectangle_int_t   *interest_rect,
				      cairo_image_surface_t  **image_out,
				      cairo_rectangle_int_t   *image_rect_out,
				      void		     **image_extra)
{
    cairo_gl_surface_t *surface = abstract_surface;

    return _cairo_surface_acquire_dest_image (surface->backing,
					      interest_rect,
					      image_out,
					      image_rect_out,
					      image_extra);
}

static void
_cairo_gl_surface_release_dest_image (void		      *abstract_surface,
				      cairo_rectangle_int_t   *interest_rect,
				      cairo_image_surface_t   *image,
				      cairo_rectangle_int_t   *image_rect,
				      void		      *image_extra)
{
    cairo_gl_surface_t *surface = abstract_surface;

    _cairo_surface_release_dest_image (surface->backing,
				       interest_rect,
				       image,
				       image_rect,
				       image_extra);
}

static cairo_status_t
_cairo_gl_surface_clone_similar (void		     *abstract_surface,
				 cairo_surface_t     *src,
				 int                  src_x,
				 int                  src_y,
				 int                  width,
				 int                  height,
				 int                 *clone_offset_x,
				 int                 *clone_offset_y,
				 cairo_surface_t    **clone_out)
{
    cairo_gl_surface_t *surface = abstract_surface;

    if (src->backend == surface->base.backend) {
	*clone_offset_x = 0;
	*clone_offset_y = 0;
	*clone_out = cairo_surface_reference (src);

	return CAIRO_STATUS_SUCCESS;
    }

    return CAIRO_INT_STATUS_UNSUPPORTED;
}

static cairo_int_status_t
_cairo_gl_surface_get_extents (void		     *abstract_surface,
			       cairo_rectangle_int_t *rectangle)
{
    cairo_gl_surface_t *surface = abstract_surface;

    return _cairo_surface_get_extents (surface->backing, rectangle);
}

static const cairo_surface_backend_t cairo_gl_surface_backend = {
    CAIRO_SURFACE_TYPE_GL,
    _cairo_gl_surface_create_similar,
    _cairo_gl_surface_finish,
    _cairo_gl_surface_acquire_source_image,
    _cairo_gl_surface_release_source_image,
    _cairo_gl_surface_acquire_dest_image,
    _cairo_gl_surface_release_dest_image,
    _cairo_gl_surface_clone_similar,
    NULL, /* composite */
    NULL, /* fill_rectangles */
    NULL, /* composite_trapezoids */
    NULL, /* create_span_renderer */
    NULL, /* check_span_renderer */
    NULL, /* copy_page */
    NULL, /* show_page */
    NULL, /* set_clip_region */
    NULL, /* intersect_clip_path */
    _cairo_gl_surface_get_extents,
    NULL, /* old_show_glyphs */
    NULL, /* get_font_options */
    NULL, /* flush */
    NULL, /* mark_dirty_rectangle */
    NULL, /* scaled_font_fini */
    NULL, /* scaled_glyph_fini */
    NULL, /* paint */
    NULL, /* mask */
    NULL, /* stroke */
    NULL, /* fill */
    NULL, /* show_glyphs */
    NULL  /* snapshot */
};

/** Call glFinish(), used for accurate performance testing. */
cairo_status_t
cairo_gl_surface_glfinish (cairo_surface_t *surface)
{
    glFinish();

    return CAIRO_STATUS_SUCCESS;
}
