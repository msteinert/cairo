/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2009 Eric Anholt
 * Copyright © 2009 Chris Wilson
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

#include "cairo-gl-private.h"

slim_hidden_proto (cairo_gl_context_reference);
slim_hidden_proto (cairo_gl_context_destroy);
slim_hidden_proto (cairo_gl_surface_create);

#define ARRAY_SIZE(array) (sizeof(array) / sizeof(array[0]))

enum cairo_gl_composite_operand_type {
    OPERAND_CONSTANT,
    OPERAND_TEXTURE,
};

/* This union structure describes a potential source or mask operand to the
 * compositing equation.
 */
typedef struct cairo_gl_composite_operand {
    enum cairo_gl_composite_operand_type type;
    union {
	struct {
	    GLuint tex;
	    cairo_gl_surface_t *surface;
	    cairo_surface_attributes_t attributes;
	    cairo_bool_t has_alpha;
	} texture;
	struct {
	    GLfloat color[4];
	} constant;
    } operand;

    const cairo_pattern_t *pattern;
} cairo_gl_composite_operand_t;

typedef struct _cairo_gl_composite_setup {
    cairo_gl_composite_operand_t src;
    cairo_gl_composite_operand_t mask;
} cairo_gl_composite_setup_t;

static const cairo_surface_backend_t _cairo_gl_surface_backend;

static const cairo_gl_context_t _nil_context = {
    CAIRO_REFERENCE_COUNT_INVALID,
    CAIRO_STATUS_NO_MEMORY
};

static cairo_bool_t _cairo_surface_is_gl (cairo_surface_t *surface)
{
    return surface->backend == &_cairo_gl_surface_backend;
}

cairo_gl_context_t *
_cairo_gl_context_create_in_error (cairo_status_t status)
{
    if (status == CAIRO_STATUS_NO_MEMORY)
	return (cairo_gl_context_t *) &_nil_context;

    ASSERT_NOT_REACHED;
    return NULL;
}

cairo_status_t
_cairo_gl_context_init (cairo_gl_context_t *ctx)
{
    ctx->status = CAIRO_STATUS_SUCCESS;
    CAIRO_REFERENCE_COUNT_INIT (&ctx->ref_count, 1);
    CAIRO_MUTEX_INIT (ctx->mutex);

    if (glewInit () != GLEW_OK) {
	return _cairo_error (CAIRO_STATUS_INVALID_FORMAT); /* XXX */
    }

    if (! GLEW_EXT_framebuffer_object ||
	! GLEW_ARB_texture_env_combine ||
	! GLEW_ARB_texture_non_power_of_two)
    {
	fprintf (stderr,
		 "Required GL extensions not available:\n");
	if (! GLEW_EXT_framebuffer_object)
	    fprintf (stderr, "    GL_EXT_framebuffer_object\n");
	if (! GLEW_ARB_texture_env_combine)
	    fprintf (stderr, "    GL_ARB_texture_env_combine\n");
	if (! GLEW_ARB_texture_non_power_of_two)
	    fprintf (stderr, "    GL_ARB_texture_non_power_of_two\n");

	return _cairo_error (CAIRO_STATUS_INVALID_FORMAT); /* XXX */
    }

    /* Set up the dummy texture for tex_env_combine with constant color. */
    glGenTextures (1, &ctx->dummy_tex);
    glBindTexture (GL_TEXTURE_2D, ctx->dummy_tex);
    glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0,
		  GL_RGBA, GL_UNSIGNED_BYTE, NULL);

    return CAIRO_STATUS_SUCCESS;
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
slim_hidden_def (cairo_gl_context_reference);

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

    glDeleteTextures (1, &context->dummy_tex);

    context->destroy (context);

    free (context);
}
slim_hidden_def (cairo_gl_context_destroy);

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
	    ctx->make_current (ctx, surface);
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
_cairo_gl_set_operator (cairo_gl_surface_t *dst, cairo_operator_t op)
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
    GLenum src_factor, dst_factor;

    if (op >= ARRAY_SIZE (blend_factors))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    src_factor = blend_factors[op].src;
    dst_factor = blend_factors[op].dst;

    /* We may have a visual with alpha bits despite the user requesting
     * CAIRO_CONTENT_COLOR.  So clear out those bits in that case.
     */
    if (dst->base.content == CAIRO_CONTENT_COLOR) {
	if (src_factor == GL_ONE_MINUS_DST_ALPHA)
	    src_factor = GL_ZERO;
	if (src_factor == GL_DST_ALPHA)
	    src_factor = GL_ONE;
    }

    glBlendFunc (src_factor, dst_factor);

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_gl_set_texture_surface (int tex_unit, GLuint tex,
			       cairo_surface_attributes_t *attributes)
{
    glActiveTexture (GL_TEXTURE0 + tex_unit);
    glBindTexture (GL_TEXTURE_2D, tex);
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

void
_cairo_gl_surface_init (cairo_gl_context_t *ctx,
			cairo_gl_surface_t *surface,
			cairo_content_t content,
			int width, int height)
{
    _cairo_surface_init (&surface->base,
			 &_cairo_gl_surface_backend,
			 content);

    surface->ctx = cairo_gl_context_reference (ctx);
    surface->width = width;
    surface->height = height;
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

    _cairo_gl_surface_init (ctx, surface, content, width, height);

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
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
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

    /* Cairo surfaces start out initialized to transparent (black) */
    ctx = _cairo_gl_context_acquire (surface->ctx);
    _cairo_gl_set_destination (surface);
    glClearColor (0.0, 0.0, 0.0, 0.0);
    glClear (GL_COLOR_BUFFER_BIT);
    _cairo_gl_context_release (ctx);

    return &surface->base;
}
slim_hidden_def (cairo_gl_surface_create);

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

int
cairo_gl_surface_get_width (cairo_surface_t *abstract_surface)
{
    cairo_gl_surface_t *surface = (cairo_gl_surface_t *) abstract_surface;

    if (! _cairo_surface_is_gl (abstract_surface))
	return 0;

    return surface->width;
}

int
cairo_gl_surface_get_height (cairo_surface_t *abstract_surface)
{
    cairo_gl_surface_t *surface = (cairo_gl_surface_t *) abstract_surface;

    if (! _cairo_surface_is_gl (abstract_surface))
	return 0;

    return surface->height;
}


void
cairo_gl_surface_swapbuffers (cairo_surface_t *abstract_surface)
{
    cairo_gl_surface_t *surface = (cairo_gl_surface_t *) abstract_surface;
    cairo_status_t status;

    if (! _cairo_surface_is_gl (abstract_surface)) {
	status = _cairo_surface_set_error (abstract_surface,
		                           CAIRO_STATUS_SURFACE_TYPE_MISMATCH);
	return;
    }

    if (! surface->fb)
	surface->ctx->swap_buffers (surface->ctx, surface);
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
	assert(dst->base.content != CAIRO_CONTENT_COLOR_ALPHA);
	assert(dst->base.content != CAIRO_CONTENT_ALPHA);
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
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

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
    if (surface->base.content == CAIRO_CONTENT_COLOR_ALPHA) {
	format = GL_BGRA;
	cairo_format = CAIRO_FORMAT_ARGB32;
	type = GL_UNSIGNED_INT_8_8_8_8_REV;
	cpp = 4;
    } else if (surface->base.content == CAIRO_CONTENT_COLOR) {
	format = GL_BGRA;
	cairo_format = CAIRO_FORMAT_RGB24;
	type = GL_UNSIGNED_INT_8_8_8_8_REV;
	cpp = 4;
    } else if (surface->base.content == CAIRO_CONTENT_ALPHA) {
	format = GL_ALPHA;
	cairo_format = CAIRO_FORMAT_A8;
	type = GL_UNSIGNED_BYTE;
	cpp = 1;
    } else {
	fprintf(stderr, "get_image fallback: %d\n", surface->base.content);
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
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

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
	    _cairo_gl_surface_create_similar (&surface->base,
		                              src->content,
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

    *pattern = cairo_pattern_create_for_surface (&image_surface->base);
    cairo_surface_destroy (&image_surface->base);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_gl_pattern_image_texture_setup (cairo_gl_composite_operand_t *operand,
				       const cairo_pattern_t *src,
				       cairo_gl_surface_t *dst,
				       int src_x, int src_y,
				       int dst_x, int dst_y,
				       int width, int height)
{
    cairo_surface_pattern_t *surface_pattern = (cairo_surface_pattern_t *)src;
    cairo_image_surface_t *image_surface;
    cairo_matrix_t m;
    cairo_surface_attributes_t *attributes;
    GLuint tex;
    GLenum format, internal_format, type;

    if (src->type != CAIRO_PATTERN_TYPE_SURFACE)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if (!_cairo_surface_is_image (surface_pattern->surface))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    image_surface = (cairo_image_surface_t *)surface_pattern->surface;

    switch (image_surface->pixman_format) {
    case PIXMAN_a8r8g8b8:
	internal_format = GL_RGBA;
	format = GL_BGRA;
	type = GL_UNSIGNED_INT_8_8_8_8_REV;
	break;
    case PIXMAN_x8r8g8b8:
	internal_format = GL_RGB;
	format = GL_BGR;
	type = GL_UNSIGNED_INT_8_8_8_8_REV;
	break;
    case PIXMAN_a8b8g8r8:
	internal_format = GL_RGBA;
	format = GL_RGBA;
	type = GL_UNSIGNED_INT_8_8_8_8_REV;
	break;
    case PIXMAN_x8b8g8r8:
	internal_format = GL_RGB;
	format = GL_RGB;
	type = GL_UNSIGNED_INT_8_8_8_8_REV;
	break;
    case PIXMAN_b8g8r8a8:
	internal_format = GL_BGRA;
	format = GL_BGRA;
	type = GL_UNSIGNED_INT_8_8_8_8;
	break;
    case PIXMAN_r8g8b8:
	internal_format = GL_RGB;
	format = GL_RGB;
	type = GL_UNSIGNED_BYTE;
	break;
    case PIXMAN_b8g8r8:
	internal_format = GL_RGB;
	format = GL_BGR;
	type = GL_UNSIGNED_BYTE;
	break;
    case PIXMAN_r5g6b5:
	internal_format = GL_RGB;
	format = GL_RGB;
	type = GL_UNSIGNED_SHORT_5_6_5;
	break;
    case PIXMAN_b5g6r5:
	internal_format = GL_RGB;
	format = GL_BGR;
	type = GL_UNSIGNED_SHORT_5_6_5;
	break;
    case PIXMAN_a8:
	internal_format = GL_ALPHA;
	format = GL_ALPHA;
	type = GL_UNSIGNED_BYTE;
	break;

    case PIXMAN_a1r5g5b5:
    case PIXMAN_x1r5g5b5:
    case PIXMAN_a1b5g5r5:
    case PIXMAN_x1b5g5r5:
    case PIXMAN_a2b10g10r10:
    case PIXMAN_x2b10g10r10:
    case PIXMAN_a4r4g4b4:
    case PIXMAN_x4r4g4b4:
    case PIXMAN_a4b4g4r4:
    case PIXMAN_x4b4g4r4:
    case PIXMAN_r3g3b2:
    case PIXMAN_b2g3r3:
    case PIXMAN_a2r2g2b2:
    case PIXMAN_a2b2g2r2:
    case PIXMAN_c8:
    case PIXMAN_x4a4:
    /* case PIXMAN_x4c4: */
    case PIXMAN_x4g4:
    case PIXMAN_a4:
    case PIXMAN_r1g2b1:
    case PIXMAN_b1g2r1:
    case PIXMAN_a1r1g1b1:
    case PIXMAN_a1b1g1r1:
    case PIXMAN_c4:
    case PIXMAN_g4:
    case PIXMAN_a1:
    case PIXMAN_g1:
    case PIXMAN_yuy2:
    case PIXMAN_yv12:
    case PIXMAN_b8g8r8x8:
    default:
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    glGenTextures (1, &tex);
    glBindTexture (GL_TEXTURE_2D, tex);
    glPixelStorei (GL_UNPACK_ALIGNMENT, 1);
    assert(((image_surface->stride * image_surface->depth) %
	    image_surface->depth) == 0);
    glPixelStorei (GL_UNPACK_ROW_LENGTH,
		   image_surface->stride / (image_surface->depth / 8));
    /* The filter will be correctly set up later, but for now we want to
     * hint to glTexImage that we're not mipmapping.
     */
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D (GL_TEXTURE_2D, 0, internal_format,
		  image_surface->width, image_surface->height, 0,
		  format, type, image_surface->data);
    glPixelStorei (GL_UNPACK_ROW_LENGTH, 0);

    attributes = &operand->operand.texture.attributes;

    operand->type = OPERAND_TEXTURE;
    operand->operand.texture.tex = tex;
    operand->operand.texture.surface = NULL;
    /* The textures we create always have appropriate alpha channels --
     * we aren't uploading x8 channels to a8 channels.
     */
    operand->operand.texture.has_alpha = TRUE;
    attributes->matrix = src->matrix;
    attributes->extend = src->extend;
    attributes->filter = src->filter;
    /* Demote the filter if we're doing a 1:1 mapping of pixels. */
    if ((src->filter == CAIRO_FILTER_GOOD ||
	 src->filter == CAIRO_FILTER_BEST ||
	 src->filter == CAIRO_FILTER_BILINEAR) &&
	_cairo_matrix_is_pixel_exact (&src->matrix)) {
	attributes->filter = CAIRO_FILTER_NEAREST;
    }

    attributes->x_offset = 0;
    attributes->y_offset = 0;


    /* Set up translation matrix for
     * (unnormalized dst -> unnormalized src)
     */
    cairo_matrix_init_translate (&m,
				 src_x - dst_x,
				 src_y - dst_y);
    cairo_matrix_multiply(&attributes->matrix,
			  &m,
			  &attributes->matrix);

    /* Translate the matrix from
     * (unnormalized src -> unnormalized src) to
     * (unnormalized dst -> normalized src)
     */
    cairo_matrix_init_scale (&m,
			     1.0 / image_surface->width,
			     1.0 / image_surface->height);
    cairo_matrix_multiply (&attributes->matrix,
			   &attributes->matrix,
			   &m);

    return CAIRO_STATUS_SUCCESS;
}

/**
 * Like cairo_pattern_acquire_surface(), but returns a matrix that transforms
 * from dest to src coords.
 */
static cairo_status_t
_cairo_gl_pattern_texture_setup (cairo_gl_composite_operand_t *operand,
				 const cairo_pattern_t *src,
				 cairo_gl_surface_t *dst,
				 int src_x, int src_y,
				 int dst_x, int dst_y,
				 int width, int height)
{
    cairo_status_t status;
    cairo_matrix_t m;
    cairo_gl_surface_t *surface;
    cairo_surface_attributes_t *attributes;

    attributes = &operand->operand.texture.attributes;

    /* First, try to just upload it to a texture if it's an image surface. */
    status = _cairo_gl_pattern_image_texture_setup (operand, src, dst,
						    src_x, src_y,
						    dst_x, dst_y,
						    width, height);
    if (status == CAIRO_STATUS_SUCCESS)
	return CAIRO_STATUS_SUCCESS;

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
					     attributes);
    operand->operand.texture.surface = surface;
    if (unlikely (status))
	return status;

    assert(surface->base.backend == &_cairo_gl_surface_backend);

    operand->operand.texture.tex = surface->tex;
    switch (surface->base.content) {
    case CAIRO_CONTENT_ALPHA:
    case CAIRO_CONTENT_COLOR_ALPHA:
	operand->operand.texture.has_alpha = TRUE;
	break;
    case CAIRO_CONTENT_COLOR:
	operand->operand.texture.has_alpha = FALSE;
	break;
    }

    /* Translate the matrix from
     * (unnormalized src -> unnormalized src) to
     * (unnormalized dst -> unnormalized src)
     */
    cairo_matrix_init_translate (&m,
				 src_x - dst_x + attributes->x_offset,
				 src_y - dst_y + attributes->y_offset);
    cairo_matrix_multiply (&attributes->matrix,
			   &m,
			   &attributes->matrix);


    /* Translate the matrix from
     * (unnormalized src -> unnormalized src) to
     * (unnormalized dst -> normalized src)
     */
    cairo_matrix_init_scale (&m,
			     1.0 / surface->width,
			     1.0 / surface->height);
    cairo_matrix_multiply (&attributes->matrix,
			   &attributes->matrix,
			   &m);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_gl_operand_init(cairo_gl_composite_operand_t *operand,
		       const cairo_pattern_t *pattern,
		       cairo_gl_surface_t *dst,
		       int src_x, int src_y,
		       int dst_x, int dst_y,
		       int width, int height)
{
    cairo_solid_pattern_t *solid = (cairo_solid_pattern_t *)pattern;

    operand->pattern = pattern;

    switch (pattern->type) {
    case CAIRO_PATTERN_TYPE_SOLID:
	operand->type = OPERAND_CONSTANT;
	operand->operand.constant.color[0] = solid->color.red * solid->color.alpha;
	operand->operand.constant.color[1] = solid->color.green * solid->color.alpha;
	operand->operand.constant.color[2] = solid->color.blue * solid->color.alpha;
	operand->operand.constant.color[3] = solid->color.alpha;
	return CAIRO_STATUS_SUCCESS;
    default:
    case CAIRO_PATTERN_TYPE_SURFACE:
    case CAIRO_PATTERN_TYPE_LINEAR:
    case CAIRO_PATTERN_TYPE_RADIAL:
	operand->type = OPERAND_TEXTURE;
	return _cairo_gl_pattern_texture_setup(operand,
					       pattern, dst,
					       src_x, src_y,
					       dst_x, dst_y,
					       width, height);
    }
}

static void
_cairo_gl_operand_destroy (cairo_gl_composite_operand_t *operand)
{
    switch (operand->type) {
    case OPERAND_CONSTANT:
	break;
    case OPERAND_TEXTURE:
	if (operand->operand.texture.surface != NULL) {
	    cairo_gl_surface_t *surface = operand->operand.texture.surface;

	    _cairo_pattern_release_surface (operand->pattern,
					    &surface->base,
					    &operand->operand.texture.attributes);
	} else {
	    glDeleteTextures (1, &operand->operand.texture.tex);
	}
	break;
    }
}

static void
_cairo_gl_set_tex_combine_constant_color (cairo_gl_context_t *ctx, int tex_unit,
					  GLfloat *color)
{
    glActiveTexture (GL_TEXTURE0 + tex_unit);
    /* Have to have a dummy texture bound in order to use the combiner unit. */
    glBindTexture (GL_TEXTURE_2D, ctx->dummy_tex);
    glEnable (GL_TEXTURE_2D);

    glTexEnvfv (GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, color);
    glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
    if (tex_unit == 0) {
	glTexEnvi (GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_REPLACE);
	glTexEnvi (GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE);
    } else {
	glTexEnvi (GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
	glTexEnvi (GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_MODULATE);
    }
    glTexEnvi (GL_TEXTURE_ENV, GL_SRC0_RGB, GL_CONSTANT);
    glTexEnvi (GL_TEXTURE_ENV, GL_SRC0_ALPHA, GL_CONSTANT);
    if (tex_unit == 0) {
	glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
	glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
    } else {
	glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_ALPHA);
	glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);

	glTexEnvi (GL_TEXTURE_ENV, GL_SRC1_RGB, GL_PREVIOUS);
	glTexEnvi (GL_TEXTURE_ENV, GL_SRC1_ALPHA, GL_PREVIOUS);
	glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);
	glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND1_ALPHA, GL_SRC_ALPHA);
    }
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
    cairo_surface_attributes_t *src_attributes, *mask_attributes = NULL;
    cairo_gl_context_t *ctx;
    GLfloat vertices[4][2];
    GLfloat texcoord_src[4][2];
    GLfloat texcoord_mask[4][2];
    cairo_status_t status;
    int i;
    GLenum err;
    GLfloat constant_color[4] = {0.0, 0.0, 0.0, 1.0};
    cairo_gl_composite_setup_t setup;

    memset(&setup, 0, sizeof(setup));

    status = _cairo_gl_operand_init (&setup.src, src, dst,
				     src_x, src_y,
				     dst_x, dst_y,
				     width, height);
    if (unlikely (status))
	return status;
    src_attributes = &setup.src.operand.texture.attributes;

    if (mask != NULL && _cairo_pattern_is_opaque(mask))
	mask = NULL;

    if (mask != NULL) {
	status = _cairo_gl_operand_init (&setup.mask, mask, dst,
					 mask_x, mask_y,
					 dst_x, dst_y,
					 width, height);
	if (unlikely (status)) {
	    _cairo_gl_operand_destroy (&setup.src);
	    return status;
	}
	mask_attributes = &setup.mask.operand.texture.attributes;
    }

    ctx = _cairo_gl_context_acquire (dst->ctx);
    _cairo_gl_set_destination (dst);
    status = _cairo_gl_set_operator (dst, op);
    if (status != CAIRO_STATUS_SUCCESS) {
	_cairo_gl_operand_destroy (&setup.src);
	if (mask != NULL)
	    _cairo_gl_operand_destroy (&setup.mask);
	_cairo_gl_context_release (ctx);
	return status;
    }

    glEnable (GL_BLEND);

    switch (setup.src.type) {
    case OPERAND_CONSTANT:
	_cairo_gl_set_tex_combine_constant_color (ctx, 0,
	    setup.src.operand.constant.color);
	break;
    case OPERAND_TEXTURE:
	_cairo_gl_set_texture_surface (0, setup.src.operand.texture.tex,
				       src_attributes);
	/* Set up the constant color we use to set alpha to 1 if needed. */
	glTexEnvfv (GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, constant_color);
	/* Set up the combiner to just set color to the sampled texture. */
	glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
	glTexEnvi (GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_REPLACE);
	glTexEnvi (GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE);

	glTexEnvi (GL_TEXTURE_ENV, GL_SRC0_RGB, GL_TEXTURE0);
	/* Wire the src alpha to 1 if the surface doesn't have it.
	 * We may have a teximage with alpha bits even though we didn't ask
	 * for it and we don't pay attention to setting alpha to 1 in a dest
	 * that has inadvertent alpha.
	 */
	if (setup.src.operand.texture.has_alpha)
	    glTexEnvi (GL_TEXTURE_ENV, GL_SRC0_ALPHA, GL_TEXTURE0);
	else
	    glTexEnvi (GL_TEXTURE_ENV, GL_SRC0_ALPHA, GL_CONSTANT);
	glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
	glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
	break;
    }

    if (mask != NULL) {
	switch (setup.mask.type) {
	case OPERAND_CONSTANT:
	    _cairo_gl_set_tex_combine_constant_color (ctx, 1,
		setup.mask.operand.constant.color);
	    break;
	case OPERAND_TEXTURE:
	    _cairo_gl_set_texture_surface (1, setup.mask.operand.texture.tex,
					   mask_attributes);

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
	    break;
	}
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

    if (setup.src.type == OPERAND_TEXTURE) {
	for (i = 0; i < 4; i++) {
	    double s, t;

	    s = vertices[i][0];
	    t = vertices[i][1];
	    cairo_matrix_transform_point (&src_attributes->matrix, &s, &t);
	    texcoord_src[i][0] = s;
	    texcoord_src[i][1] = t;
	}

	glClientActiveTexture (GL_TEXTURE0);
	glTexCoordPointer (2, GL_FLOAT, sizeof(GLfloat) * 2, texcoord_src);
	glEnableClientState (GL_TEXTURE_COORD_ARRAY);
    }

    if (mask != NULL) {
	if (setup.mask.type == OPERAND_TEXTURE) {
	    for (i = 0; i < 4; i++) {
		double s, t;

		s = vertices[i][0];
		t = vertices[i][1];
		cairo_matrix_transform_point (&mask_attributes->matrix, &s, &t);
		texcoord_mask[i][0] = s;
		texcoord_mask[i][1] = t;
	    }

	    glClientActiveTexture (GL_TEXTURE1);
	    glTexCoordPointer (2, GL_FLOAT, sizeof(GLfloat) * 2, texcoord_mask);
	    glEnableClientState (GL_TEXTURE_COORD_ARRAY);
	}
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

    _cairo_gl_operand_destroy (&setup.src);
    if (mask != NULL)
	_cairo_gl_operand_destroy (&setup.mask);

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
    status = _cairo_gl_set_operator (surface, op);
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
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);
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
    glDrawArrays (GL_TRIANGLE_FAN, 0, 4 * num_rects);

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
