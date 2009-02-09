/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2009 Eric Anholt
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


#include <X11/Xlib.h>

#define GL_GLEXT_PROTOTYPES
#include <GL/glx.h>
#include <GL/glext.h>

#include "cairoint.h"

#include "cairo-gl.h"

#define ARRAY_SIZE(array) (sizeof(array) / sizeof(array[0]))

typedef struct _cairo_gl_surface {
    cairo_surface_t base;

    cairo_gl_context_t *ctx;
    cairo_content_t content;
    int width, height;

    Window win; /* window if not rendering to FBO */
    GLuint tex; /* GL texture object containing our data. */
    GLuint fb; /* GL framebuffer object wrapping our data. */
} cairo_gl_surface_t;

struct _cairo_gl_context {
    cairo_reference_count_t ref_count;
    cairo_status_t status;

    Display *dpy;
    GLXContext gl_ctx;
    cairo_mutex_t mutex; /* needed? */
    cairo_gl_surface_t *current_target;
};

static const cairo_surface_backend_t _cairo_gl_surface_backend;

const cairo_gl_context_t _nil_context = {
    CAIRO_REFERENCE_COUNT_INVALID,
    CAIRO_STATUS_NO_MEMORY
};

static cairo_bool_t _cairo_surface_is_gl (cairo_surface_t *surface)
{
    return surface->backend == &_cairo_gl_surface_backend;
}

static cairo_gl_context_t *
_cairo_gl_context_create_in_error (cairo_status_t status)
{
    if (status == CAIRO_STATUS_NO_MEMORY)
	return (cairo_gl_context_t *) &_nil_context;

    ASSERT_NOT_REACHED;
    return NULL;
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

cairo_gl_context_t *
cairo_gl_context_reference (cairo_gl_context_t *context)
{
    if (context == NULL ||
	CAIRO_REFERENCE_COUNT_IS_INVALID (&context->ref_count))
    {
	return context;
    }

    assert (CAIRO_REFERENCE_COUNT_HAS_REFERENCE (&context->ref_count));
    _cairo_reference_count_inc (&context->ref_count);

    return context;
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

static cairo_gl_context_t *
_cairo_gl_context_acquire (cairo_gl_context_t *ctx)
{
    CAIRO_MUTEX_LOCK (ctx->mutex);
    return ctx;
}

static void
_cairo_gl_context_release (cairo_gl_context_t *ctx)
{
    CAIRO_MUTEX_UNLOCK (ctx->mutex);
}

static void
_cairo_gl_set_destination (cairo_gl_surface_t *surface)
{
    cairo_gl_context_t *ctx = surface->ctx;

    if (ctx->current_target != surface) {
	ctx->current_target = surface;

	if (surface->fb) {
	    glBindFramebufferEXT (GL_FRAMEBUFFER_EXT, surface->fb);
	    glDrawBuffer (GL_COLOR_ATTACHMENT0_EXT);
	    glReadBuffer (GL_COLOR_ATTACHMENT0_EXT);
	} else {
	    /* Set the window as the target of our context. */
	    glXMakeCurrent (ctx->dpy, surface->win, ctx->gl_ctx);
	    glBindFramebufferEXT (GL_FRAMEBUFFER_EXT, 0);
	    glDrawBuffer (GL_BACK_LEFT);
	    glReadBuffer (GL_BACK_LEFT);
	}
    }

    glViewport (0, 0, surface->width, surface->height);

    glMatrixMode (GL_PROJECTION);
    glLoadIdentity();
    if (surface->fb)
	glOrtho(0, surface->width, 0, surface->height, -1.0, 1.0);
    else
	glOrtho(0, surface->width, surface->height, 0, -1.0, 1.0);

    glMatrixMode (GL_MODELVIEW);
    glLoadIdentity();
}

static int
_cairo_gl_set_operator (cairo_operator_t op)
{
    struct {
	GLenum src;
	GLenum dst;
    } blend_factors[] = {
	{ GL_ZERO, GL_ZERO }, /* Clear */
	{ GL_ONE, GL_ZERO }, /* Source */
	{ GL_ONE, GL_ONE_MINUS_SRC_ALPHA }, /* Over */
	{ GL_DST_ALPHA, GL_ZERO }, /* In */
	{ GL_ONE_MINUS_DST_ALPHA, GL_ZERO }, /* Out */
	{ GL_DST_ALPHA, GL_ONE_MINUS_SRC_ALPHA }, /* Atop */

	{ GL_ZERO, GL_ONE }, /* Dest */
	{ GL_ONE_MINUS_DST_ALPHA, GL_ONE }, /* DestOver */
	{ GL_ZERO, GL_SRC_ALPHA }, /* DestIn */
	{ GL_ZERO, GL_ONE_MINUS_SRC_ALPHA }, /* DestOut */
	{ GL_ONE_MINUS_DST_ALPHA, GL_SRC_ALPHA }, /* DestAtop */

	{ GL_ONE_MINUS_DST_ALPHA, GL_ONE_MINUS_SRC_ALPHA }, /* Xor */
	{ GL_ONE, GL_ONE }, /* Add */
    };

    if (op >= ARRAY_SIZE (blend_factors))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    glBlendFunc (blend_factors[op].src, blend_factors[op].dst);

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_gl_set_texture_surface (int tex_unit, cairo_gl_surface_t *surface,
			       cairo_surface_attributes_t *attributes)
{
    assert(surface->base.backend = &_cairo_gl_surface_backend);
    glActiveTexture (GL_TEXTURE0 + tex_unit);
    glBindTexture (GL_TEXTURE_2D, surface->tex);
    switch (attributes->extend) {
    case CAIRO_EXTEND_NONE:
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	break;
    case CAIRO_EXTEND_PAD:
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	break;
    case CAIRO_EXTEND_REPEAT:
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	break;
    case CAIRO_EXTEND_REFLECT:
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
	break;
    }
    switch (attributes->filter) {
    case CAIRO_FILTER_FAST:
    case CAIRO_FILTER_NEAREST:
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	break;
    case CAIRO_FILTER_GOOD:
    case CAIRO_FILTER_BEST:
    case CAIRO_FILTER_BILINEAR:
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	break;
    default:
    case CAIRO_FILTER_GAUSSIAN:
	ASSERT_NOT_REACHED;
    }
    glEnable (GL_TEXTURE_2D);
}

cairo_surface_t *
cairo_gl_surface_create (cairo_gl_context_t   *ctx,
			 cairo_content_t	content,
			 int			width,
			 int			height)
{
    cairo_gl_surface_t *surface;
    GLenum err, format;
    cairo_status_t status;

    if (!CAIRO_CONTENT_VALID (content))
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_CONTENT));

    if (ctx == NULL) {
	return cairo_image_surface_create (_cairo_format_from_content (content),
					   width, height);
    }
    if (ctx->status)
	return _cairo_surface_create_in_error (ctx->status);

    surface = calloc (1, sizeof (cairo_gl_surface_t));
    if (unlikely (surface == NULL))
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    _cairo_surface_init (&surface->base,
			 &_cairo_gl_surface_backend,
			 content);

    surface->ctx = cairo_gl_context_reference (ctx);
    surface->content = content;

    switch (content) {
    default:
	ASSERT_NOT_REACHED;
    case CAIRO_CONTENT_COLOR_ALPHA:
	format = GL_RGBA;
	break;
    case CAIRO_CONTENT_ALPHA:
	format = GL_RGBA;
	break;
    case CAIRO_CONTENT_COLOR:
	format = GL_RGB;
	break;
    }

    /* Create the texture used to store the surface's data. */
    glGenTextures (1, &surface->tex);
    glBindTexture (GL_TEXTURE_2D, surface->tex);
    glTexImage2D (GL_TEXTURE_2D, 0, format, width, height, 0,
		  format, GL_UNSIGNED_BYTE, NULL);

    /* Create a framebuffer object wrapping the texture so that we can render
     * to it.
     */
    glGenFramebuffersEXT(1, &surface->fb);
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, surface->fb);
    glFramebufferTexture2DEXT (GL_FRAMEBUFFER_EXT,
			       GL_COLOR_ATTACHMENT0_EXT,
			       GL_TEXTURE_2D,
			       surface->tex,
			       0);

    while ((err = glGetError ())) {
	fprintf(stderr, "GL error in surface create: 0x%08x\n", err);
    }

    status = glCheckFramebufferStatusEXT (GL_FRAMEBUFFER_EXT);
    if (status != GL_FRAMEBUFFER_COMPLETE_EXT)
	fprintf(stderr, "destination is framebuffer incomplete\n");

    surface->width = width;
    surface->height = height;

    /* Cairo surfaces start out initialized to transparent (black) */
    ctx = _cairo_gl_context_acquire (surface->ctx);
    _cairo_gl_set_destination (surface);
    glClearColor (0.0, 0.0, 0.0, 0.0);
    glClear (GL_COLOR_BUFFER_BIT);
    _cairo_gl_context_release (ctx);

    return &surface->base;
}


cairo_surface_t *
cairo_gl_surface_create_for_window (cairo_gl_context_t   *ctx,
				    Window                win,
				    int                   width,
				    int                   height)
{
    cairo_gl_surface_t *surface;
    cairo_content_t content = CAIRO_CONTENT_COLOR_ALPHA;

    surface = calloc (1, sizeof (cairo_gl_surface_t));
    if (unlikely (surface == NULL))
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    _cairo_surface_init (&surface->base,
			 &_cairo_gl_surface_backend,
			 content);

    surface->ctx = cairo_gl_context_reference (ctx);
    surface->content = content;
    surface->width = width;
    surface->height = height;
    surface->win = win;

    return &surface->base;
}

void
cairo_gl_surface_set_size (cairo_surface_t *abstract_surface,
			   int              width,
			   int              height)
{
    cairo_gl_surface_t *surface = (cairo_gl_surface_t *) abstract_surface;
    cairo_status_t status;

    if (! _cairo_surface_is_gl (abstract_surface) || surface->fb) {
	status = _cairo_surface_set_error (abstract_surface,
		                           CAIRO_STATUS_SURFACE_TYPE_MISMATCH);
	return;
    }

    surface->width = width;
    surface->height = height;
}

void
cairo_gl_surface_swapbuffers (cairo_surface_t *abstract_surface)
{
    cairo_gl_surface_t *surface = (cairo_gl_surface_t *) abstract_surface;
    cairo_status_t status;

    if (! _cairo_surface_is_gl (abstract_surface) || surface->fb) {
	status = _cairo_surface_set_error (abstract_surface,
		                           CAIRO_STATUS_SURFACE_TYPE_MISMATCH);
	return;
    }

    glXSwapBuffers(surface->ctx->dpy, surface->win);
}

static cairo_surface_t *
_cairo_gl_surface_create_similar (void		 *abstract_surface,
				  cairo_content_t  content,
				  int		  width,
				  int		  height)
{
    cairo_gl_surface_t *surface = abstract_surface;

    assert (CAIRO_CONTENT_VALID (content));

    if (width < 1)
	width = 1;
    if (height < 1)
	height = 1;

    return cairo_gl_surface_create (surface->ctx, content, width, height);
}

static cairo_bool_t
_cairo_gl_surface_draw_image_format_compatible (cairo_image_surface_t *surface)
{
    if (surface->pixman_format == PIXMAN_a8r8g8b8 ||
	surface->pixman_format == PIXMAN_x8r8g8b8 ||
	surface->pixman_format == PIXMAN_a8)
	return TRUE;
    else
	return FALSE;
}

static cairo_status_t
_cairo_gl_surface_draw_image (cairo_gl_surface_t *dst,
			      cairo_image_surface_t *src,
			      int src_x, int src_y,
			      int width, int height,
			      int dst_x, int dst_y)
{
    char *temp_data;
    int y;
    unsigned int cpp;
    GLenum format, type;
    char *src_data_start;

    /* Want to use a switch statement here but the compiler gets whiny. */
    if (src->pixman_format == PIXMAN_a8r8g8b8) {
	format = GL_BGRA;
	type = GL_UNSIGNED_INT_8_8_8_8_REV;
	cpp = 4;
    } else if (src->pixman_format == PIXMAN_x8r8g8b8) {
	assert(dst->content != CAIRO_CONTENT_COLOR_ALPHA);
	assert(dst->content != CAIRO_CONTENT_ALPHA);
	format = GL_BGRA;
	type = GL_UNSIGNED_INT_8_8_8_8_REV;
	cpp = 4;
    } else if (src->pixman_format == PIXMAN_a8) {
	format = GL_ALPHA;
	type = GL_UNSIGNED_BYTE;
	cpp = 1;
    } else {
	fprintf(stderr, "draw_image fallback\n");
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    /* Write the data to a temporary as GL wants bottom-to-top data
     * screen-wise, and we want top-to-bottom.
     */
    temp_data = malloc (width * height * cpp);
    if (temp_data == NULL)
	return CAIRO_STATUS_NO_MEMORY;

    src_data_start = (char *)src->data + (src_y * src->stride) + (src_x * cpp);
    for (y = 0; y < height; y++) {
	memcpy (temp_data + y * width * cpp, src_data_start +
		y * src->stride,
		width * cpp);
    }

    _cairo_gl_set_destination (dst);
    glRasterPos2i (dst_x, dst_y);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glDrawPixels (width, height, format, type, temp_data);

    free (temp_data);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_gl_surface_get_image (cairo_gl_surface_t      *surface,
			     cairo_rectangle_int_t   *interest,
			     cairo_image_surface_t  **image_out,
			     cairo_rectangle_int_t   *rect_out)
{
    cairo_image_surface_t *image;
    cairo_rectangle_int_t extents;
    GLenum err;
    char *temp_data;
    unsigned int y;
    unsigned int cpp;
    GLenum format, type;
    cairo_format_t cairo_format;

    extents.x = 0;
    extents.y = 0;
    extents.width  = surface->width;
    extents.height = surface->height;

    if (interest != NULL) {
	if (! _cairo_rectangle_intersect (&extents, interest)) {
	    *image_out = NULL;
	    return CAIRO_STATUS_SUCCESS;
	}
    }

    if (rect_out != NULL)
	*rect_out = extents;

    /* Want to use a switch statement here but the compiler gets whiny. */
    if (surface->content == CAIRO_CONTENT_COLOR_ALPHA) {
	format = GL_BGRA;
	cairo_format = CAIRO_FORMAT_ARGB32;
	type = GL_UNSIGNED_INT_8_8_8_8_REV;
	cpp = 4;
    } else if (surface->content == CAIRO_CONTENT_COLOR) {
	format = GL_BGRA;
	cairo_format = CAIRO_FORMAT_RGB24;
	type = GL_UNSIGNED_INT_8_8_8_8_REV;
	cpp = 4;
    } else if (surface->content == CAIRO_CONTENT_ALPHA) {
	format = GL_ALPHA;
	cairo_format = CAIRO_FORMAT_A8;
	type = GL_UNSIGNED_BYTE;
	cpp = 1;
    } else {
	fprintf(stderr, "get_image fallback: %d\n", surface->content);
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    image = (cairo_image_surface_t*)
	cairo_image_surface_create (cairo_format,
				    extents.width, extents.height);
    if (image->base.status)
	return image->base.status;

    /* This is inefficient, as we'd rather just read the thing without making
     * it the destination.  But then, this is the fallback path, so let's not
     * fall back instead.
     */
    _cairo_gl_set_destination(surface);

    /* Read the data to a temporary as GL gives us bottom-to-top data
     * screen-wise, and we want top-to-bottom.
     */
    temp_data = malloc (extents.width * extents.height * cpp);
    if (temp_data == NULL)
	return CAIRO_STATUS_NO_MEMORY;

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(extents.x, extents.y,
		 extents.width, extents.height,
		 format, type, temp_data);

    for (y = 0; y < extents.height; y++) {
	memcpy ((char *)image->data + y * image->stride,
		temp_data + y * extents.width * cpp,
		extents.width * cpp);
    }
    free (temp_data);

    *image_out = image;

    while ((err = glGetError ()))
	fprintf(stderr, "GL error 0x%08x\n", (int) err);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_gl_surface_finish (void *abstract_surface)
{
    cairo_gl_surface_t *surface = abstract_surface;

    glDeleteFramebuffersEXT (1, &surface->fb);
    glDeleteTextures (1, &surface->tex);

    if (surface->ctx->current_target == surface)
	surface->ctx->current_target = NULL;

    cairo_gl_context_destroy(surface->ctx);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_gl_surface_acquire_source_image (void		       *abstract_surface,
					cairo_image_surface_t **image_out,
					void		      **image_extra)
{
    cairo_gl_surface_t *surface = abstract_surface;

    *image_extra = NULL;

    return _cairo_gl_surface_get_image (surface, NULL, image_out, NULL);
}

static void
_cairo_gl_surface_release_source_image (void		      *abstract_surface,
					cairo_image_surface_t *image,
					void		      *image_extra)
{
    cairo_surface_destroy (&image->base);
}

static cairo_status_t
_cairo_gl_surface_acquire_dest_image (void		      *abstract_surface,
				      cairo_rectangle_int_t   *interest_rect,
				      cairo_image_surface_t  **image_out,
				      cairo_rectangle_int_t   *image_rect_out,
				      void		     **image_extra)
{
    cairo_gl_surface_t *surface = abstract_surface;

    *image_extra = NULL;
    return _cairo_gl_surface_get_image (surface, interest_rect, image_out,
					image_rect_out);
}

static void
_cairo_gl_surface_release_dest_image (void		      *abstract_surface,
				      cairo_rectangle_int_t   *interest_rect,
				      cairo_image_surface_t   *image,
				      cairo_rectangle_int_t   *image_rect,
				      void		      *image_extra)
{
    cairo_status_t status;

    status = _cairo_gl_surface_draw_image (abstract_surface, image,
					   0, 0,
					   image->width, image->height,
					   image_rect->x, image_rect->y);
    if (status)
	status = _cairo_surface_set_error (abstract_surface, status);

    cairo_surface_destroy (&image->base);
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
    } else if (_cairo_surface_is_image (src)) {
	cairo_gl_surface_t *clone;
	cairo_image_surface_t *image_src = (cairo_image_surface_t *)src;
	cairo_status_t status;

	if (!_cairo_gl_surface_draw_image_format_compatible (image_src))
	    return CAIRO_INT_STATUS_UNSUPPORTED;

	clone = (cairo_gl_surface_t *)
	    _cairo_gl_surface_create_similar (&surface->base, src->content,
					      width, height);
	if (clone == NULL)
	    return CAIRO_INT_STATUS_UNSUPPORTED;
	if (clone->base.status)
	    return clone->base.status;

	status = _cairo_gl_surface_draw_image (clone, image_src,
					       src_x, src_y,
					       width, height,
					       0, 0);
	if (status) {
	    cairo_surface_destroy (&clone->base);
	    return status;
	}

	*clone_out = &clone->base;
	*clone_offset_x = src_x;
	*clone_offset_y = src_y;

	return CAIRO_STATUS_SUCCESS;
    }

    return CAIRO_INT_STATUS_UNSUPPORTED;
}

/** Creates a cairo-gl pattern surface for the given trapezoids */
static cairo_status_t
_cairo_gl_get_traps_pattern (cairo_gl_surface_t *dst,
			     int dst_x, int dst_y,
			     int width, int height,
			     cairo_trapezoid_t *traps,
			     int num_traps,
			     cairo_antialias_t antialias,
			     cairo_pattern_t **pattern)
{
    pixman_trapezoid_t stack_traps[CAIRO_STACK_ARRAY_LENGTH (pixman_trapezoid_t)];
    pixman_trapezoid_t *pixman_traps;
    cairo_image_surface_t *image_surface;
    cairo_surface_t *gl_surface;
    cairo_status_t status;
    int gl_offset_x, gl_offset_y;
    int i;

    /* Convert traps to pixman traps */
    pixman_traps = stack_traps;
    if (num_traps > ARRAY_LENGTH (stack_traps)) {
	pixman_traps = _cairo_malloc_ab (num_traps, sizeof (pixman_trapezoid_t));
	if (unlikely (pixman_traps == NULL))
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);
    }

    for (i = 0; i < num_traps; i++) {
	pixman_traps[i].top = _cairo_fixed_to_16_16 (traps[i].top);
	pixman_traps[i].bottom = _cairo_fixed_to_16_16 (traps[i].bottom);
	pixman_traps[i].left.p1.x = _cairo_fixed_to_16_16 (traps[i].left.p1.x);
	pixman_traps[i].left.p1.y = _cairo_fixed_to_16_16 (traps[i].left.p1.y);
	pixman_traps[i].left.p2.x = _cairo_fixed_to_16_16 (traps[i].left.p2.x);
	pixman_traps[i].left.p2.y = _cairo_fixed_to_16_16 (traps[i].left.p2.y);
	pixman_traps[i].right.p1.x = _cairo_fixed_to_16_16 (traps[i].right.p1.x);
	pixman_traps[i].right.p1.y = _cairo_fixed_to_16_16 (traps[i].right.p1.y);
	pixman_traps[i].right.p2.x = _cairo_fixed_to_16_16 (traps[i].right.p2.x);
	pixman_traps[i].right.p2.y = _cairo_fixed_to_16_16 (traps[i].right.p2.y);
    }

    if (antialias != CAIRO_ANTIALIAS_NONE) {
	image_surface = (cairo_image_surface_t*)
	    cairo_image_surface_create (CAIRO_FORMAT_A8, width, height);
    } else {
	image_surface = (cairo_image_surface_t*)
	    cairo_image_surface_create (CAIRO_FORMAT_A1, width, height);
    }

    if (image_surface->base.status) {
	if (pixman_traps != stack_traps)
	    free (pixman_traps);
	return image_surface->base.status;
    }

    pixman_add_trapezoids (image_surface->pixman_image, -dst_x, -dst_y,
			   num_traps, pixman_traps);

    if (pixman_traps != stack_traps)
	free (pixman_traps);

    status = _cairo_surface_clone_similar (&dst->base, &image_surface->base,
					   0, 0, width, height,
					   &gl_offset_x, &gl_offset_y,
					   &gl_surface);
    cairo_surface_destroy (&image_surface->base);
    if (status)
	return status;

    *pattern = cairo_pattern_create_for_surface (gl_surface);
    cairo_surface_destroy (gl_surface);

    return CAIRO_STATUS_SUCCESS;
}

/**
 * Like cairo_pattern_acquire_surface, but returns a matrix that transforms
 * from dest to src coords.
 */
static cairo_status_t
_cairo_gl_pattern_acquire_surface (const cairo_pattern_t *src,
				   cairo_gl_surface_t *dst,
				   int src_x, int src_y,
				   int dst_x, int dst_y,
				   int width, int height,
				   cairo_gl_surface_t **out_surface,
				   cairo_surface_attributes_t *out_attributes)
{
    cairo_status_t status;
    cairo_matrix_t m;
    cairo_gl_surface_t *surface;

    /* Avoid looping in acquire surface fallback -> clone similar -> paint ->
     * gl_composite -> acquire surface -> fallback.
     */
    if (src->type == CAIRO_PATTERN_TYPE_SURFACE) {
	cairo_surface_pattern_t *surface_pattern;
	cairo_image_surface_t *image_surface;

	surface_pattern = (cairo_surface_pattern_t *)src;
	if (_cairo_surface_is_image (surface_pattern->surface)) {
	    image_surface = (cairo_image_surface_t *)surface_pattern->surface;
	    if (!_cairo_gl_surface_draw_image_format_compatible (image_surface))
		return CAIRO_INT_STATUS_UNSUPPORTED;
	}
    }

    status = _cairo_pattern_acquire_surface (src, &dst->base,
					     src_x, src_y,
					     width, height,
					     (cairo_surface_t **)
					     &surface,
					     out_attributes);
    if (unlikely (status))
	return status;

    assert(surface->base.backend == &_cairo_gl_surface_backend);
    *out_surface = surface;

    /* Translate the matrix from
     * (unnormalized src -> unnormalized src) to
     * (unnormalized dst -> unnormalized src)
     */
    cairo_matrix_init_translate (&m,
				 src_x - dst_x + out_attributes->x_offset,
				 src_y - dst_y + out_attributes->y_offset);
    cairo_matrix_multiply (&out_attributes->matrix,
			   &m,
			   &out_attributes->matrix);


    /* Translate the matrix from
     * (unnormalized src -> unnormalized src) to
     * (unnormalized dst -> normalized src)
     */
    cairo_matrix_init_scale (&m,
			     1.0 / surface->width,
			     1.0 / surface->height);
    cairo_matrix_multiply (&out_attributes->matrix,
			   &out_attributes->matrix,
			   &m);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_gl_surface_composite (cairo_operator_t		  op,
			     const cairo_pattern_t	 *src,
			     const cairo_pattern_t	 *mask,
			     void			 *abstract_dst,
			     int			  src_x,
			     int			  src_y,
			     int			  mask_x,
			     int			  mask_y,
			     int			  dst_x,
			     int			  dst_y,
			     unsigned int		  width,
			     unsigned int		  height)
{
    cairo_gl_surface_t	*dst = abstract_dst;
    cairo_gl_surface_t *src_surface, *mask_surface;
    cairo_surface_attributes_t src_attributes, mask_attributes;
    cairo_gl_context_t *ctx;
    GLfloat vertices[4][2];
    GLfloat texcoord_src[4][2];
    GLfloat texcoord_mask[4][2];
    cairo_status_t status;
    int i;
    GLenum err;
    GLfloat constant_color[4] = {0.0, 0.0, 0.0, 1.0};

    status = _cairo_gl_pattern_acquire_surface(src, dst,
					       src_x, src_y,
					       dst_x, dst_y,
					       width, height,
					       &src_surface,
					       &src_attributes);
    if (unlikely (status))
	return status;

    if (mask != NULL && _cairo_pattern_is_opaque(mask))
	mask = NULL;

    if (mask != NULL) {
	status = _cairo_gl_pattern_acquire_surface(mask, dst,
						   mask_x, mask_y,
						   dst_x, dst_y,
						   width, height,
						   &mask_surface,
						   &mask_attributes);
	if (unlikely (status)) {
	    _cairo_pattern_release_surface(src,
					   &src_surface->base,
					   &src_attributes);
	    return status;
	}
    }

    ctx = _cairo_gl_context_acquire (dst->ctx);
    _cairo_gl_set_destination (dst);
    status = _cairo_gl_set_operator (op);
    if (status != CAIRO_STATUS_SUCCESS) {
	_cairo_gl_context_release (ctx);
	return status;
    }

    glEnable (GL_BLEND);

    /* Set up the constant color we use to set alpha to 1 if needed. */
    glTexEnvfv (GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, constant_color);
    _cairo_gl_set_texture_surface (0, src_surface, &src_attributes);
    glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
    glTexEnvi (GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_REPLACE);
    glTexEnvi (GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE);

    glTexEnvi (GL_TEXTURE_ENV, GL_SRC0_RGB, GL_TEXTURE0);
    /* Wire the src alpha to 1 if the surface doesn't have it.
     * We may have a teximage with alpha bits even though we didn't ask
     * for it and we don't pay attention to setting alpha to 1 in a dest
     * that has inadvertent alpha.
     */
    if (src_surface->base.content != CAIRO_CONTENT_COLOR)
	glTexEnvi (GL_TEXTURE_ENV, GL_SRC0_ALPHA, GL_TEXTURE0);
    else
	glTexEnvi (GL_TEXTURE_ENV, GL_SRC0_ALPHA, GL_CONSTANT);
    glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
    glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
    if (mask != NULL) {
	_cairo_gl_set_texture_surface (1, mask_surface, &mask_attributes);

	/* IN: dst.argb = src.argb * mask.aaaa */
	glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
	glTexEnvi (GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
	glTexEnvi (GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_MODULATE);

	glTexEnvi (GL_TEXTURE_ENV, GL_SRC0_RGB, GL_PREVIOUS);
	glTexEnvi (GL_TEXTURE_ENV, GL_SRC0_ALPHA, GL_PREVIOUS);
	glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
	glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);

	glTexEnvi (GL_TEXTURE_ENV, GL_SRC1_RGB, GL_TEXTURE1);
	glTexEnvi (GL_TEXTURE_ENV, GL_SRC1_ALPHA, GL_TEXTURE1);
	glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_ALPHA);
	glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND1_ALPHA, GL_SRC_ALPHA);
    }

    vertices[0][0] = dst_x;
    vertices[0][1] = dst_y;
    vertices[1][0] = dst_x + width;
    vertices[1][1] = dst_y;
    vertices[2][0] = dst_x + width;
    vertices[2][1] = dst_y + height;
    vertices[3][0] = dst_x;
    vertices[3][1] = dst_y + height;

    glVertexPointer (2, GL_FLOAT, sizeof(GLfloat) * 2, vertices);
    glEnableClientState (GL_VERTEX_ARRAY);

    for (i = 0; i < 4; i++) {
	double s, t;

	s = vertices[i][0];
	t = vertices[i][1];
	cairo_matrix_transform_point (&src_attributes.matrix, &s, &t);
	texcoord_src[i][0] = s;
	texcoord_src[i][1] = t;
    }

    glClientActiveTexture (GL_TEXTURE0);
    glTexCoordPointer (2, GL_FLOAT, sizeof(GLfloat) * 2, texcoord_src);
    glEnableClientState (GL_TEXTURE_COORD_ARRAY);

    if (mask != NULL) {
	for (i = 0; i < 4; i++) {
	    double s, t;

	    s = vertices[i][0];
	    t = vertices[i][1];
	    cairo_matrix_transform_point (&mask_attributes.matrix, &s, &t);
	    texcoord_mask[i][0] = s;
	    texcoord_mask[i][1] = t;
	}

	glClientActiveTexture (GL_TEXTURE1);
	glTexCoordPointer (2, GL_FLOAT, sizeof(GLfloat) * 2, texcoord_mask);
	glEnableClientState (GL_TEXTURE_COORD_ARRAY);
    }

    glDrawArrays (GL_TRIANGLE_FAN, 0, 4);

    glDisable (GL_BLEND);

    glDisableClientState (GL_VERTEX_ARRAY);

    glClientActiveTexture (GL_TEXTURE0);
    glDisableClientState (GL_TEXTURE_COORD_ARRAY);
    glActiveTexture (GL_TEXTURE0);
    glDisable (GL_TEXTURE_2D);

    glClientActiveTexture (GL_TEXTURE1);
    glDisableClientState (GL_TEXTURE_COORD_ARRAY);
    glActiveTexture (GL_TEXTURE1);
    glDisable (GL_TEXTURE_2D);

    while ((err = glGetError ()))
	fprintf(stderr, "GL error 0x%08x\n", (int) err);

    _cairo_gl_context_release (ctx);

    _cairo_pattern_release_surface (src, &src_surface->base, &src_attributes);
    if (mask != NULL)
	_cairo_pattern_release_surface (mask, &mask_surface->base, &mask_attributes);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_gl_surface_composite_trapezoids (cairo_operator_t op,
					const cairo_pattern_t *pattern,
					void *abstract_dst,
					cairo_antialias_t antialias,
					int src_x, int src_y,
					int dst_x, int dst_y,
					unsigned int width,
					unsigned int height,
					cairo_trapezoid_t *traps,
					int num_traps)
{
    cairo_gl_surface_t *dst = abstract_dst;
    cairo_pattern_t *traps_pattern;
    cairo_int_status_t status;

    status = _cairo_gl_get_traps_pattern (dst,
					  dst_x, dst_y, width, height,
					  traps, num_traps, antialias,
					  &traps_pattern);
    if (status) {
	fprintf(stderr, "traps falllback\n");
	return status;
    }

    status = _cairo_gl_surface_composite (op, pattern, traps_pattern, dst,
					  src_x, src_y,
					  0, 0,
					  dst_x, dst_y,
					  width, height);
    cairo_pattern_destroy (traps_pattern);

    return status;
}

static cairo_int_status_t
_cairo_gl_surface_fill_rectangles (void			   *abstract_surface,
				   cairo_operator_t	    op,
				   const cairo_color_t     *color,
				   cairo_rectangle_int_t   *rects,
				   int			    num_rects)
{
    cairo_gl_surface_t *surface = abstract_surface;
    cairo_gl_context_t *ctx;
    cairo_int_status_t status;
    int i;
    GLfloat *vertices;
    GLfloat *colors;

    ctx = _cairo_gl_context_acquire (surface->ctx);

    _cairo_gl_set_destination (surface);
    status = _cairo_gl_set_operator (op);
    if (status != CAIRO_STATUS_SUCCESS) {
	_cairo_gl_context_release (ctx);
	return status;
    }

    vertices = _cairo_malloc_ab(num_rects, sizeof(GLfloat) * 4 * 2);
    colors = _cairo_malloc_ab(num_rects, sizeof(GLfloat) * 4 * 4);
    if (!vertices || !colors) {
	_cairo_gl_context_release(ctx);
	free(vertices);
	free(colors);
	return CAIRO_STATUS_NO_MEMORY;
    }

    /* This should be loaded in as either a blend constant and an operator
     * setup specific to this, or better, a fragment shader constant.
     */
    for (i = 0; i < num_rects * 4; i++) {
	colors[i * 4 + 0] = color->red * color->alpha;
	colors[i * 4 + 1] = color->green * color->alpha;
	colors[i * 4 + 2] = color->blue * color->alpha;
	colors[i * 4 + 3] = color->alpha;
    }

    for (i = 0; i < num_rects; i++) {
	vertices[i * 8 + 0] = rects[i].x;
	vertices[i * 8 + 1] = rects[i].y;
	vertices[i * 8 + 2] = rects[i].x + rects[i].width;
	vertices[i * 8 + 3] = rects[i].y;
	vertices[i * 8 + 4] = rects[i].x + rects[i].width;
	vertices[i * 8 + 5] = rects[i].y + rects[i].height;
	vertices[i * 8 + 6] = rects[i].x;
	vertices[i * 8 + 7] = rects[i].y + rects[i].height;
    }

    glEnable (GL_BLEND);
    glVertexPointer (2, GL_FLOAT, sizeof(GLfloat) * 2, vertices);
    glEnableClientState (GL_VERTEX_ARRAY);
    glColorPointer (4, GL_FLOAT, sizeof(GLfloat) * 4, colors);
    glEnableClientState (GL_COLOR_ARRAY);
    glDrawArrays (GL_QUADS, 0, 4 * num_rects);

    glDisableClientState (GL_COLOR_ARRAY);
    glDisableClientState (GL_VERTEX_ARRAY);
    glDisable (GL_BLEND);

    _cairo_gl_context_release (ctx);
    free(vertices);
    free(colors);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_gl_surface_get_extents (void		     *abstract_surface,
			       cairo_rectangle_int_t *rectangle)
{
    cairo_gl_surface_t *surface = abstract_surface;

    rectangle->x = 0;
    rectangle->y = 0;
    rectangle->width  = surface->width;
    rectangle->height = surface->height;

    return CAIRO_STATUS_SUCCESS;
}

static const cairo_surface_backend_t _cairo_gl_surface_backend = {
    CAIRO_SURFACE_TYPE_GL,
    _cairo_gl_surface_create_similar,
    _cairo_gl_surface_finish,
    _cairo_gl_surface_acquire_source_image,
    _cairo_gl_surface_release_source_image,
    _cairo_gl_surface_acquire_dest_image,
    _cairo_gl_surface_release_dest_image,
    _cairo_gl_surface_clone_similar,
    _cairo_gl_surface_composite,
    _cairo_gl_surface_fill_rectangles,
    _cairo_gl_surface_composite_trapezoids,
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
