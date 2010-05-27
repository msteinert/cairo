/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2009 Eric Anholt
 * Copyright © 2009 Chris Wilson
 * Copyright © 2005,2010 Red Hat, Inc
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
 * The Initial Developer of the Original Code is Red Hat, Inc.
 *
 * Contributor(s):
 *	Benjamin Otte <otte@gnome.org>
 *	Carl Worth <cworth@cworth.org>
 *	Chris Wilson <chris@chris-wilson.co.uk>
 *	Eric Anholt <eric@anholt.net>
 */

#include "cairoint.h"

#include "cairo-error-private.h"
#include "cairo-gl-private.h"

static int
_cairo_gl_gradient_sample_width (const cairo_gradient_pattern_t *gradient)
{
    unsigned int n;
    int width;

    width = 8;
    for (n = 1; n < gradient->n_stops; n++) {
	double dx = gradient->stops[n].offset - gradient->stops[n-1].offset;
	double delta, max;
	int ramp;

	if (dx == 0)
	    continue;

	max = gradient->stops[n].color.red -
	      gradient->stops[n-1].color.red;

	delta = gradient->stops[n].color.green -
	        gradient->stops[n-1].color.green;
	if (delta > max)
	    max = delta;

	delta = gradient->stops[n].color.blue -
	        gradient->stops[n-1].color.blue;
	if (delta > max)
	    max = delta;

	delta = gradient->stops[n].color.alpha -
	        gradient->stops[n-1].color.alpha;
	if (delta > max)
	    max = delta;

	ramp = 128 * max / dx;
	if (ramp > width)
	    width = ramp;
    }

    width = (width + 7) & -8;
    return MIN (width, 1024);
}

static cairo_status_t
_render_gradient (const cairo_gl_context_t *ctx,
		  cairo_gradient_pattern_t *pattern,
		  void *bytes,
		  int width)
{
    pixman_image_t *gradient, *image;
    pixman_gradient_stop_t pixman_stops_stack[32];
    pixman_gradient_stop_t *pixman_stops;
    pixman_point_fixed_t p1, p2;
    unsigned int i;

    pixman_stops = pixman_stops_stack;
    if (unlikely (pattern->n_stops > ARRAY_LENGTH (pixman_stops_stack))) {
	pixman_stops = _cairo_malloc_ab (pattern->n_stops,
					 sizeof (pixman_gradient_stop_t));
	if (unlikely (pixman_stops == NULL))
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);
    }

    for (i = 0; i < pattern->n_stops; i++) {
	pixman_stops[i].x = _cairo_fixed_16_16_from_double (pattern->stops[i].offset);
	pixman_stops[i].color.red   = pattern->stops[i].color.red_short;
	pixman_stops[i].color.green = pattern->stops[i].color.green_short;
	pixman_stops[i].color.blue  = pattern->stops[i].color.blue_short;
	pixman_stops[i].color.alpha = pattern->stops[i].color.alpha_short;
    }

    p1.x = 0;
    p1.y = 0;
    p2.x = width << 16;
    p2.y = 0;

    gradient = pixman_image_create_linear_gradient (&p1, &p2,
						    pixman_stops,
						    pattern->n_stops);
    if (pixman_stops != pixman_stops_stack)
	free (pixman_stops);

    if (unlikely (gradient == NULL))
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    pixman_image_set_filter (gradient, PIXMAN_FILTER_BILINEAR, NULL, 0);
    pixman_image_set_repeat (gradient, PIXMAN_REPEAT_PAD);

    image = pixman_image_create_bits (PIXMAN_a8r8g8b8, width, 1,
				      bytes, sizeof(uint32_t)*width);
    if (unlikely (image == NULL)) {
	pixman_image_unref (gradient);
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);
    }

    pixman_image_composite32 (PIXMAN_OP_SRC,
                              gradient, NULL, image,
                              0, 0,
                              0, 0,
                              0, 0,
                              width, 1);

    pixman_image_unref (gradient);
    pixman_image_unref (image);
    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_gl_create_gradient_texture (const cairo_gl_context_t *ctx,
				   cairo_gl_surface_t *surface,
				   cairo_gradient_pattern_t *pattern,
				   GLuint *tex)
{
    int tex_width;
    GLubyte *data;

    assert (pattern->n_stops != 0);

    tex_width = _cairo_gl_gradient_sample_width (pattern);

    glBindBufferARB (GL_PIXEL_UNPACK_BUFFER_ARB, ctx->texture_load_pbo);
    glBufferDataARB (GL_PIXEL_UNPACK_BUFFER_ARB, tex_width * sizeof (uint32_t), 0, GL_STREAM_DRAW);
    data = glMapBufferARB (GL_PIXEL_UNPACK_BUFFER_ARB, GL_WRITE_ONLY);

    _render_gradient (ctx, pattern, data, tex_width);

    glUnmapBufferARB (GL_PIXEL_UNPACK_BUFFER_ARB);

    glGenTextures (1, tex);
    glBindTexture (GL_TEXTURE_1D, *tex);
    glTexImage1D (GL_TEXTURE_1D, 0, GL_RGBA8, tex_width, 0,
                  GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, 0);

    glBindBufferARB (GL_PIXEL_UNPACK_BUFFER_ARB, 0);

    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    switch (pattern->base.extend) {
    case CAIRO_EXTEND_NONE:
	glTexParameteri (GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	break;
    case CAIRO_EXTEND_PAD:
	glTexParameteri (GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	break;
    case CAIRO_EXTEND_REPEAT:
	glTexParameteri (GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	break;
    case CAIRO_EXTEND_REFLECT:
	glTexParameteri (GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
	break;
    }
}

/**
 * Like cairo_pattern_acquire_surface(), but returns a matrix that transforms
 * from dest to src coords.
 */
static cairo_status_t
_cairo_gl_pattern_texture_setup (cairo_gl_context_t *ctx,
                                 cairo_gl_operand_t *operand,
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
    attributes = &operand->texture.attributes;

    status = _cairo_pattern_acquire_surface (src, &dst->base,
					     src_x, src_y,
					     width, height,
					     CAIRO_PATTERN_ACQUIRE_NONE,
					     (cairo_surface_t **)
					     &surface,
					     attributes);
    if (unlikely (status))
	return status;

    if (ctx->tex_target == GL_TEXTURE_RECTANGLE_EXT &&
	(attributes->extend == CAIRO_EXTEND_REPEAT ||
	 attributes->extend == CAIRO_EXTEND_REFLECT))
    {
	_cairo_pattern_release_surface (operand->pattern,
					&surface->base,
					attributes);
	return UNSUPPORTED ("EXT_texture_rectangle with repeat/reflect");
    }

    assert (surface->base.backend == &_cairo_gl_surface_backend);

    operand->type = CAIRO_GL_OPERAND_TEXTURE;
    operand->texture.surface = surface;
    operand->texture.tex = surface->tex;
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
     * (unnormalized dst -> unnormalized src) to
     * (unnormalized dst -> normalized src)
     */
    if (ctx->tex_target == GL_TEXTURE_RECTANGLE_EXT) {
	cairo_matrix_init_scale (&m,
				 1.0,
				 1.0);
    } else {
	cairo_matrix_init_scale (&m,
				 1.0 / surface->width,
				 1.0 / surface->height);
    }
    cairo_matrix_multiply (&attributes->matrix,
			   &attributes->matrix,
			   &m);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_gl_solid_operand_init (cairo_gl_operand_t *operand,
	                      const cairo_color_t *color)
{
    operand->type = CAIRO_GL_OPERAND_CONSTANT;
    operand->constant.color[0] = color->red   * color->alpha;
    operand->constant.color[1] = color->green * color->alpha;
    operand->constant.color[2] = color->blue  * color->alpha;
    operand->constant.color[3] = color->alpha;
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_gl_gradient_operand_init (cairo_gl_context_t *ctx,
                                 cairo_gl_operand_t *operand,
				 cairo_gl_surface_t *dst)
{
    cairo_gradient_pattern_t *gradient = (cairo_gradient_pattern_t *)operand->pattern;

    if (! _cairo_gl_device_has_glsl (&ctx->base))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if (gradient->base.type == CAIRO_PATTERN_TYPE_LINEAR) {
	cairo_linear_pattern_t *linear = (cairo_linear_pattern_t *) gradient;
        double x0, y0, x1, y1;

	x0 = _cairo_fixed_to_double (linear->p1.x);
	x1 = _cairo_fixed_to_double (linear->p2.x);
	y0 = _cairo_fixed_to_double (linear->p1.y);
	y1 = _cairo_fixed_to_double (linear->p2.y);

        if ((unsigned int)ctx->max_texture_size / 2 <= gradient->n_stops) {
            return CAIRO_INT_STATUS_UNSUPPORTED;
        }

        _cairo_gl_create_gradient_texture (ctx,
					   dst,
					   gradient,
					   &operand->linear.tex);

	/* Translation matrix from the destination fragment coordinates
	 * (pixels from lower left = 0,0) to the coordinates in the
	 */
	cairo_matrix_init_translate (&operand->linear.m, -x0, -y0);
	cairo_matrix_multiply (&operand->linear.m,
			       &operand->pattern->matrix,
			       &operand->linear.m);
	cairo_matrix_translate (&operand->linear.m, 0, dst->height);
	cairo_matrix_scale (&operand->linear.m, 1.0, -1.0);

	operand->linear.segment_x = x1 - x0;
	operand->linear.segment_y = y1 - y0;

	operand->type = CAIRO_GL_OPERAND_LINEAR_GRADIENT;
        return CAIRO_STATUS_SUCCESS;
    } else {
	cairo_radial_pattern_t *radial = (cairo_radial_pattern_t *) gradient;
        double x0, y0, r0, x1, y1, r1;

	x0 = _cairo_fixed_to_double (radial->c1.x);
	x1 = _cairo_fixed_to_double (radial->c2.x);
	y0 = _cairo_fixed_to_double (radial->c1.y);
	y1 = _cairo_fixed_to_double (radial->c2.y);
	r0 = _cairo_fixed_to_double (radial->r1);
	r1 = _cairo_fixed_to_double (radial->r2);

        if ((unsigned int)ctx->max_texture_size / 2 <= gradient->n_stops)
            return CAIRO_INT_STATUS_UNSUPPORTED;

        _cairo_gl_create_gradient_texture (ctx,
					   dst,
					   gradient,
					   &operand->radial.tex);

	/* Translation matrix from the destination fragment coordinates
	 * (pixels from lower left = 0,0) to the coordinates in the
	 */
	cairo_matrix_init_translate (&operand->radial.m, -x0, -y0);
	cairo_matrix_multiply (&operand->radial.m,
			       &operand->pattern->matrix,
			       &operand->radial.m);
	cairo_matrix_translate (&operand->radial.m, 0, dst->height);
	cairo_matrix_scale (&operand->radial.m, 1.0, -1.0);

	operand->radial.circle_1_x = x1 - x0;
	operand->radial.circle_1_y = y1 - y0;
	operand->radial.radius_0 = r0;
	operand->radial.radius_1 = r1;

	operand->type = CAIRO_GL_OPERAND_RADIAL_GRADIENT;
        return CAIRO_STATUS_SUCCESS;
    }

    return CAIRO_INT_STATUS_UNSUPPORTED;
}

static void
_cairo_gl_operand_destroy (cairo_gl_operand_t *operand)
{
    switch (operand->type) {
    case CAIRO_GL_OPERAND_CONSTANT:
	break;
    case CAIRO_GL_OPERAND_LINEAR_GRADIENT:
	glDeleteTextures (1, &operand->linear.tex);
	break;
    case CAIRO_GL_OPERAND_RADIAL_GRADIENT:
	glDeleteTextures (1, &operand->radial.tex);
	break;
    case CAIRO_GL_OPERAND_TEXTURE:
	if (operand->texture.surface != NULL) {
	    cairo_gl_surface_t *surface = operand->texture.surface;

	    _cairo_pattern_release_surface (operand->pattern,
					    &surface->base,
					    &operand->texture.attributes);
	}
	break;
    default:
    case CAIRO_GL_OPERAND_COUNT:
        ASSERT_NOT_REACHED;
    case CAIRO_GL_OPERAND_NONE:
    case CAIRO_GL_OPERAND_SPANS:
        break;
    }

    operand->type = CAIRO_GL_OPERAND_NONE;
}

static cairo_int_status_t
_cairo_gl_operand_init (cairo_gl_context_t *ctx,
                        cairo_gl_operand_t *operand,
		        const cairo_pattern_t *pattern,
		        cairo_gl_surface_t *dst,
		        int src_x, int src_y,
		        int dst_x, int dst_y,
		        int width, int height)
{
    cairo_status_t status;

    operand->pattern = pattern;

    switch (pattern->type) {
    case CAIRO_PATTERN_TYPE_SOLID:
	return _cairo_gl_solid_operand_init (operand,
		                             &((cairo_solid_pattern_t *) pattern)->color);
    case CAIRO_PATTERN_TYPE_LINEAR:
    case CAIRO_PATTERN_TYPE_RADIAL:
	status = _cairo_gl_gradient_operand_init (ctx, operand, dst);
	if (status != CAIRO_INT_STATUS_UNSUPPORTED)
	    return status;

	/* fall through */

    default:
    case CAIRO_PATTERN_TYPE_SURFACE:
	return _cairo_gl_pattern_texture_setup (ctx, operand,
						pattern, dst,
						src_x, src_y,
						dst_x, dst_y,
						width, height);
    }
}

cairo_int_status_t
_cairo_gl_composite_set_source (cairo_gl_context_t *ctx,
                                cairo_gl_composite_t *setup,
			        const cairo_pattern_t *pattern,
                                int src_x, int src_y,
                                int dst_x, int dst_y,
                                int width, int height)
{
    _cairo_gl_operand_destroy (&setup->src);
    return _cairo_gl_operand_init (ctx, &setup->src, pattern,
                                   setup->dst,
                                   src_x, src_y,
                                   dst_x, dst_y,
                                   width, height);
}

cairo_int_status_t
_cairo_gl_composite_set_mask (cairo_gl_context_t *ctx,
                              cairo_gl_composite_t *setup,
			      const cairo_pattern_t *pattern,
                              int src_x, int src_y,
                              int dst_x, int dst_y,
                              int width, int height)
{
    _cairo_gl_operand_destroy (&setup->mask);
    setup->has_component_alpha = pattern && pattern->has_component_alpha;
    if (pattern == NULL)
        return CAIRO_STATUS_SUCCESS;

    return _cairo_gl_operand_init (ctx, &setup->mask, pattern,
                                   setup->dst,
                                   src_x, src_y,
                                   dst_x, dst_y,
                                   width, height);
}

void
_cairo_gl_composite_set_mask_spans (cairo_gl_context_t *ctx,
                                    cairo_gl_composite_t *setup)
{
    _cairo_gl_operand_destroy (&setup->mask);
    setup->mask.type = CAIRO_GL_OPERAND_SPANS;
    setup->has_component_alpha = FALSE;
}

void
_cairo_gl_composite_set_clip_region (cairo_gl_context_t *ctx,
                                     cairo_gl_composite_t *setup,
                                     cairo_region_t *clip_region)
{
    setup->clip_region = clip_region;
}

static void
_cairo_gl_operand_bind_to_shader (cairo_gl_context_t *ctx,
                                  cairo_gl_operand_t *operand,
                                  const char         *name)
{
    char uniform_name[50];
    char *custom_part;

    strcpy (uniform_name, name);
    custom_part = uniform_name + strlen (name);

    switch (operand->type) {
    default:
    case CAIRO_GL_OPERAND_COUNT:
        ASSERT_NOT_REACHED;
    case CAIRO_GL_OPERAND_NONE:
    case CAIRO_GL_OPERAND_SPANS:
    case CAIRO_GL_OPERAND_TEXTURE:
        break;
    case CAIRO_GL_OPERAND_CONSTANT:
        strcpy (custom_part, "_constant");
	_cairo_gl_shader_bind_vec4 (ctx,
                                    uniform_name,
                                    operand->constant.color[0],
                                    operand->constant.color[1],
                                    operand->constant.color[2],
                                    operand->constant.color[3]);
        break;
    case CAIRO_GL_OPERAND_LINEAR_GRADIENT:
        strcpy (custom_part, "_matrix");
	_cairo_gl_shader_bind_matrix (ctx,
                                      uniform_name,
				      &operand->linear.m);
        strcpy (custom_part, "_segment");
	_cairo_gl_shader_bind_vec2   (ctx,
                                      uniform_name,
				      operand->linear.segment_x,
				      operand->linear.segment_y);
        break;
    case CAIRO_GL_OPERAND_RADIAL_GRADIENT:
        strcpy (custom_part, "_matrix");
        _cairo_gl_shader_bind_matrix (ctx,
                                      uniform_name,
                                      &operand->radial.m);
        strcpy (custom_part, "_circle_1");
        _cairo_gl_shader_bind_vec2   (ctx,
                                      uniform_name,
                                      operand->radial.circle_1_x,
                                      operand->radial.circle_1_y);
        strcpy (custom_part, "_radius_0");
        _cairo_gl_shader_bind_float  (ctx,
                                      uniform_name,
                                      operand->radial.radius_0);
        strcpy (custom_part, "_radius_1");
        _cairo_gl_shader_bind_float  (ctx,
                                      uniform_name,
                                      operand->radial.radius_1);
        break;
    }
}

static void
_cairo_gl_composite_bind_to_shader (cairo_gl_context_t   *ctx,
				    cairo_gl_composite_t *setup)
{
    if (ctx->current_shader == NULL)
        return;

    _cairo_gl_operand_bind_to_shader (ctx, &setup->src, "source");
    _cairo_gl_operand_bind_to_shader (ctx, &setup->mask, "mask");
}

static void
_cairo_gl_texture_set_attributes (cairo_gl_context_t         *ctx,
                                  cairo_surface_attributes_t *attributes)
{
    if (ctx->tex_target == GL_TEXTURE_RECTANGLE_EXT) {
	assert (attributes->extend != CAIRO_EXTEND_REPEAT &&
		attributes->extend != CAIRO_EXTEND_REFLECT);
    }

    switch (attributes->extend) {
    case CAIRO_EXTEND_NONE:
	glTexParameteri (ctx->tex_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri (ctx->tex_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	break;
    case CAIRO_EXTEND_PAD:
	glTexParameteri (ctx->tex_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri (ctx->tex_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	break;
    case CAIRO_EXTEND_REPEAT:
	glTexParameteri (ctx->tex_target, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri (ctx->tex_target, GL_TEXTURE_WRAP_T, GL_REPEAT);
	break;
    case CAIRO_EXTEND_REFLECT:
	glTexParameteri (ctx->tex_target, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
	glTexParameteri (ctx->tex_target, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
	break;
    }

    switch (attributes->filter) {
    case CAIRO_FILTER_FAST:
    case CAIRO_FILTER_NEAREST:
	glTexParameteri (ctx->tex_target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri (ctx->tex_target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	break;
    case CAIRO_FILTER_GOOD:
    case CAIRO_FILTER_BEST:
    case CAIRO_FILTER_BILINEAR:
	glTexParameteri (ctx->tex_target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri (ctx->tex_target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	break;
    default:
    case CAIRO_FILTER_GAUSSIAN:
	ASSERT_NOT_REACHED;
    }
}

static void
_cairo_gl_operand_setup_texture (cairo_gl_context_t *ctx,
                                 cairo_gl_operand_t *operand,
                                 GLuint              tex_unit,
                                 unsigned int        vertex_offset)
{
    switch (operand->type) {
    default:
    case CAIRO_GL_OPERAND_COUNT:
        ASSERT_NOT_REACHED;
    case CAIRO_GL_OPERAND_NONE:
        break;
    case CAIRO_GL_OPERAND_SPANS:
	glColorPointer (4, GL_UNSIGNED_BYTE, ctx->vertex_size,
                        (void *) (uintptr_t) vertex_offset);
	glEnableClientState (GL_COLOR_ARRAY);
        /* fall through */
    case CAIRO_GL_OPERAND_CONSTANT:
        if (ctx->current_shader == NULL) {
            glActiveTexture (GL_TEXTURE0 + tex_unit);
            /* Have to have a dummy texture bound in order to use the combiner unit. */
            glBindTexture (ctx->tex_target, ctx->dummy_tex);
            glEnable (ctx->tex_target);
        } else {
            glDisable (ctx->tex_target);
        }
        break;
    case CAIRO_GL_OPERAND_TEXTURE:
        glActiveTexture (GL_TEXTURE0 + tex_unit);
        glBindTexture (ctx->tex_target, operand->texture.tex);
        glEnable (ctx->tex_target);
        _cairo_gl_texture_set_attributes (ctx, &operand->texture.attributes);

	glClientActiveTexture (GL_TEXTURE0 + tex_unit);
	glTexCoordPointer (2, GL_FLOAT, ctx->vertex_size,
                           (void *) (uintptr_t) vertex_offset);
	glEnableClientState (GL_TEXTURE_COORD_ARRAY);
        break;
    case CAIRO_GL_OPERAND_LINEAR_GRADIENT:
        glActiveTexture (GL_TEXTURE0 + tex_unit);
        glBindTexture (GL_TEXTURE_1D, operand->linear.tex);
        glEnable (GL_TEXTURE_1D);
        break;
    case CAIRO_GL_OPERAND_RADIAL_GRADIENT:
        glActiveTexture (GL_TEXTURE0 + tex_unit);
        glBindTexture (GL_TEXTURE_1D, operand->radial.tex);
        glEnable (GL_TEXTURE_1D);
        break;
    }
}

static void
_cairo_gl_set_src_operand (cairo_gl_context_t *ctx,
			   cairo_gl_composite_t *setup)
{
    GLfloat constant_color[4] = {0.0, 0.0, 0.0, 0.0};

    if (ctx->current_shader)
        return;

    glActiveTexture (GL_TEXTURE0);

    glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
    glTexEnvi (GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_REPLACE);
    glTexEnvi (GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE);
    glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
    glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);

    switch (setup->src.type) {
    case CAIRO_GL_OPERAND_CONSTANT:
        glTexEnvfv (GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, setup->src.constant.color);
        glTexEnvi (GL_TEXTURE_ENV, GL_SRC0_RGB, GL_CONSTANT);
        glTexEnvi (GL_TEXTURE_ENV, GL_SRC0_ALPHA, GL_CONSTANT);
	break;
    case CAIRO_GL_OPERAND_TEXTURE:
        /* Set up the constant color we use to set color to 0 if needed. */
        glTexEnvfv (GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, constant_color);
        /* Set up the combiner to just set color to the sampled texture. */

        /* Force the src color to 0 if the surface should be
         * alpha-only.  We may have a teximage with color bits if
         * the implementation doesn't support GL_ALPHA FBOs.
         */
        if (setup->src.texture.surface->base.content !=
            CAIRO_CONTENT_ALPHA)
            glTexEnvi (GL_TEXTURE_ENV, GL_SRC0_RGB, GL_TEXTURE0);
        else
            glTexEnvi (GL_TEXTURE_ENV, GL_SRC0_RGB, GL_CONSTANT);
        glTexEnvi (GL_TEXTURE_ENV, GL_SRC0_ALPHA, GL_TEXTURE0);
	break;

    default:
    case CAIRO_GL_OPERAND_COUNT:
        ASSERT_NOT_REACHED;
    case CAIRO_GL_OPERAND_NONE:
    case CAIRO_GL_OPERAND_SPANS:
    case CAIRO_GL_OPERAND_LINEAR_GRADIENT:
    case CAIRO_GL_OPERAND_RADIAL_GRADIENT:
        break;
    }
}

/* This is like _cairo_gl_set_src_operand, but instead swizzles the source
 * for creating the "source alpha" value (src.aaaa * mask.argb) required by
 * component alpha rendering.
 */
static void
_cairo_gl_set_src_alpha_operand (cairo_gl_context_t *ctx,
				 cairo_gl_composite_t *setup)
{
    GLfloat constant_color[4] = {0.0, 0.0, 0.0, 0.0};

    if (ctx->current_shader)
        return;

    glActiveTexture (GL_TEXTURE0);

    glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
    glTexEnvi (GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_REPLACE);
    glTexEnvi (GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE);

    switch (setup->src.type) {
    case CAIRO_GL_OPERAND_CONSTANT:
	constant_color[0] = setup->src.constant.color[3];
	constant_color[1] = setup->src.constant.color[3];
	constant_color[2] = setup->src.constant.color[3];
	constant_color[3] = setup->src.constant.color[3];
        glTexEnvfv (GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, constant_color);
        glTexEnvi (GL_TEXTURE_ENV, GL_SRC0_RGB, GL_CONSTANT);
        glTexEnvi (GL_TEXTURE_ENV, GL_SRC0_ALPHA, GL_CONSTANT);
	glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
	glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
	break;
    case CAIRO_GL_OPERAND_TEXTURE:
        /* Set up the combiner to just set color to the sampled texture. */
        glTexEnvi (GL_TEXTURE_ENV, GL_SRC0_RGB, GL_TEXTURE0);
        glTexEnvi (GL_TEXTURE_ENV, GL_SRC0_ALPHA, GL_TEXTURE0);
        glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_ALPHA);
        glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
	break;
    case CAIRO_GL_OPERAND_LINEAR_GRADIENT:
    case CAIRO_GL_OPERAND_RADIAL_GRADIENT:
    case CAIRO_GL_OPERAND_NONE:
    case CAIRO_GL_OPERAND_SPANS:
    case CAIRO_GL_OPERAND_COUNT:
    default:
        ASSERT_NOT_REACHED;
        break;
    }
}

/* This is like _cairo_gl_set_src_alpha_operand, for component alpha setup
 * of the mask part of IN to produce a "source alpha" value.
 */
static void
_cairo_gl_set_component_alpha_mask_operand (cairo_gl_context_t *ctx,
					    cairo_gl_composite_t *setup)
{
    GLfloat constant_color[4] = {0.0, 0.0, 0.0, 0.0};

    if (ctx->current_shader)
        return;

    glActiveTexture (GL_TEXTURE1);

    glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
    glTexEnvi (GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
    glTexEnvi (GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_MODULATE);

    glTexEnvi (GL_TEXTURE_ENV, GL_SRC0_RGB, GL_PREVIOUS);
    glTexEnvi (GL_TEXTURE_ENV, GL_SRC0_ALPHA, GL_PREVIOUS);
    glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
    glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);

    glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);
    glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND1_ALPHA, GL_SRC_ALPHA);

    switch (setup->mask.type) {
    case CAIRO_GL_OPERAND_CONSTANT:
        glTexEnvfv (GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR,
                    setup->mask.constant.color);

        glTexEnvi (GL_TEXTURE_ENV, GL_SRC1_RGB, GL_CONSTANT);
        glTexEnvi (GL_TEXTURE_ENV, GL_SRC1_ALPHA, GL_CONSTANT);
	break;
    case CAIRO_GL_OPERAND_TEXTURE:
        /* Set up the constant color we use to set color to 0 if needed. */
        glTexEnvfv (GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, constant_color);

        /* Force the mask color to 0 if the surface should be
         * alpha-only.  We may have a teximage with color bits if
         * the implementation doesn't support GL_ALPHA FBOs.
         */
        if (setup->mask.texture.surface == NULL ||
            setup->mask.texture.surface->base.content != CAIRO_CONTENT_ALPHA)
            glTexEnvi (GL_TEXTURE_ENV, GL_SRC1_RGB, GL_TEXTURE1);
        else
            glTexEnvi (GL_TEXTURE_ENV, GL_SRC1_RGB, GL_CONSTANT);
        glTexEnvi (GL_TEXTURE_ENV, GL_SRC1_ALPHA, GL_TEXTURE1);
	break;

    case CAIRO_GL_OPERAND_LINEAR_GRADIENT:
    case CAIRO_GL_OPERAND_RADIAL_GRADIENT:
    case CAIRO_GL_OPERAND_NONE:
    case CAIRO_GL_OPERAND_SPANS:
    case CAIRO_GL_OPERAND_COUNT:
    default:
        ASSERT_NOT_REACHED;
        break;
    }
}

static void
_cairo_gl_set_mask_operand (cairo_gl_context_t *ctx,
			    cairo_gl_composite_t *setup)
{
    if (ctx->current_shader)
        return;

    glActiveTexture (GL_TEXTURE1);

    glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
    glTexEnvi (GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
    glTexEnvi (GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_MODULATE);

    glTexEnvi (GL_TEXTURE_ENV, GL_SRC0_RGB, GL_PREVIOUS);
    glTexEnvi (GL_TEXTURE_ENV, GL_SRC0_ALPHA, GL_PREVIOUS);
    glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
    glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);

    glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_ALPHA);
    glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND1_ALPHA, GL_SRC_ALPHA);

    switch (setup->mask.type) {
    case CAIRO_GL_OPERAND_CONSTANT:
        glTexEnvfv (GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, setup->mask.constant.color);
        glTexEnvi (GL_TEXTURE_ENV, GL_SRC1_RGB, GL_CONSTANT);
        glTexEnvi (GL_TEXTURE_ENV, GL_SRC1_ALPHA, GL_CONSTANT);
        break;
    case CAIRO_GL_OPERAND_TEXTURE:
        /* IN: dst.argb = src.argb * mask.aaaa */
        glTexEnvi (GL_TEXTURE_ENV, GL_SRC1_RGB, GL_TEXTURE1);
        glTexEnvi (GL_TEXTURE_ENV, GL_SRC1_ALPHA, GL_TEXTURE1);
        break;
    case CAIRO_GL_OPERAND_SPANS:
        glTexEnvi (GL_TEXTURE_ENV, GL_SRC1_RGB, GL_PRIMARY_COLOR);
        glTexEnvi (GL_TEXTURE_ENV, GL_SRC1_ALPHA, GL_PRIMARY_COLOR);
        break;
    case CAIRO_GL_OPERAND_COUNT:
    default:
        ASSERT_NOT_REACHED;
    case CAIRO_GL_OPERAND_LINEAR_GRADIENT:
    case CAIRO_GL_OPERAND_RADIAL_GRADIENT:
    case CAIRO_GL_OPERAND_NONE:
        break;
    }
}

static void
_cairo_gl_set_operator (cairo_gl_surface_t *dst, cairo_operator_t op,
			cairo_bool_t component_alpha)
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

    assert (op < ARRAY_LENGTH (blend_factors));

    src_factor = blend_factors[op].src;
    dst_factor = blend_factors[op].dst;

    /* Even when the user requests CAIRO_CONTENT_COLOR, we use GL_RGBA
     * due to texture filtering of GL_CLAMP_TO_BORDER.  So fix those
     * bits in that case.
     */
    if (dst->base.content == CAIRO_CONTENT_COLOR) {
	if (src_factor == GL_ONE_MINUS_DST_ALPHA)
	    src_factor = GL_ZERO;
	if (src_factor == GL_DST_ALPHA)
	    src_factor = GL_ONE;
    }

    if (component_alpha) {
	if (dst_factor == GL_ONE_MINUS_SRC_ALPHA)
	    dst_factor = GL_ONE_MINUS_SRC_COLOR;
	if (dst_factor == GL_SRC_ALPHA)
	    dst_factor = GL_SRC_COLOR;
    }

    glEnable (GL_BLEND);
    if (dst->base.content == CAIRO_CONTENT_ALPHA) {
        glBlendFuncSeparate (GL_ZERO, GL_ZERO, src_factor, dst_factor);
    } else {
        glBlendFunc (src_factor, dst_factor);
    }
}

static unsigned int
_cairo_gl_operand_get_vertex_size (cairo_gl_operand_type_t type)
{
    switch (type) {
    default:
    case CAIRO_GL_OPERAND_COUNT:
        ASSERT_NOT_REACHED;
    case CAIRO_GL_OPERAND_NONE:
    case CAIRO_GL_OPERAND_CONSTANT:
    case CAIRO_GL_OPERAND_LINEAR_GRADIENT:
    case CAIRO_GL_OPERAND_RADIAL_GRADIENT:
        return 0;
    case CAIRO_GL_OPERAND_SPANS:
        return 4 * sizeof (GLbyte);
    case CAIRO_GL_OPERAND_TEXTURE:
        return 2 * sizeof (GLfloat);
    }
}

static cairo_status_t
_cairo_gl_composite_begin_component_alpha  (cairo_gl_context_t *ctx,
                                            cairo_gl_composite_t *setup)
{
    cairo_status_t status;

    /* For CLEAR, cairo's rendering equation (quoting Owen's description in:
     * http://lists.cairographics.org/archives/cairo/2005-August/004992.html)
     * is:
     *     mask IN clip ? src OP dest : dest
     * or more simply:
     *     mask IN CLIP ? 0 : dest
     *
     * where the ternary operator A ? B : C is (A * B) + ((1 - A) * C).
     *
     * The model we use in _cairo_gl_set_operator() is Render's:
     *     src IN mask IN clip OP dest
     * which would boil down to:
     *     0 (bounded by the extents of the drawing).
     *
     * However, we can do a Render operation using an opaque source
     * and DEST_OUT to produce:
     *    1 IN mask IN clip DEST_OUT dest
     * which is
     *    mask IN clip ? 0 : dest
     */
    if (setup->op == CAIRO_OPERATOR_CLEAR) {
        _cairo_gl_solid_operand_init (&setup->src, CAIRO_COLOR_WHITE);
	setup->op = CAIRO_OPERATOR_DEST_OUT;
    }

    /**
     * implements component-alpha %CAIRO_OPERATOR_OVER using two passes of
     * the simpler operations %CAIRO_OPERATOR_DEST_OUT and %CAIRO_OPERATOR_ADD.
     *
     * From http://anholt.livejournal.com/32058.html:
     *
     * The trouble is that component-alpha rendering requires two different sources
     * for blending: one for the source value to the blender, which is the
     * per-channel multiplication of source and mask, and one for the source alpha
     * for multiplying with the destination channels, which is the multiplication
     * of the source channels by the mask alpha. So the equation for Over is:
     *
     * dst.A = src.A * mask.A + (1 - (src.A * mask.A)) * dst.A
     * dst.R = src.R * mask.R + (1 - (src.A * mask.R)) * dst.R
     * dst.G = src.G * mask.G + (1 - (src.A * mask.G)) * dst.G
     * dst.B = src.B * mask.B + (1 - (src.A * mask.B)) * dst.B
     *
     * But we can do some simpler operations, right? How about PictOpOutReverse,
     * which has a source factor of 0 and dest factor of (1 - source alpha). We
     * can get the source alpha value (srca.X = src.A * mask.X) out of the texture
     * blenders pretty easily. So we can do a component-alpha OutReverse, which
     * gets us:
     *
     * dst.A = 0 + (1 - (src.A * mask.A)) * dst.A
     * dst.R = 0 + (1 - (src.A * mask.R)) * dst.R
     * dst.G = 0 + (1 - (src.A * mask.G)) * dst.G
     * dst.B = 0 + (1 - (src.A * mask.B)) * dst.B
     *
     * OK. And if an op doesn't use the source alpha value for the destination
     * factor, then we can do the channel multiplication in the texture blenders
     * to get the source value, and ignore the source alpha that we wouldn't use.
     * We've supported this in the Radeon driver for a long time. An example would
     * be PictOpAdd, which does:
     *
     * dst.A = src.A * mask.A + dst.A
     * dst.R = src.R * mask.R + dst.R
     * dst.G = src.G * mask.G + dst.G
     * dst.B = src.B * mask.B + dst.B
     *
     * Hey, this looks good! If we do a PictOpOutReverse and then a PictOpAdd right
     * after it, we get:
     *
     * dst.A = src.A * mask.A + ((1 - (src.A * mask.A)) * dst.A)
     * dst.R = src.R * mask.R + ((1 - (src.A * mask.R)) * dst.R)
     * dst.G = src.G * mask.G + ((1 - (src.A * mask.G)) * dst.G)
     * dst.B = src.B * mask.B + ((1 - (src.A * mask.B)) * dst.B)
     *
     * This two-pass trickery could be avoided using a new GL extension that
     * lets two values come out of the shader and into the blend unit.
     */
    if (setup->op == CAIRO_OPERATOR_OVER) {
	setup->op = CAIRO_OPERATOR_ADD;
	status = _cairo_gl_set_shader_by_type (ctx,
                                               setup->src.type,
                                               setup->mask.type,
                                               CAIRO_GL_SHADER_IN_CA_SOURCE_ALPHA);
        if (unlikely (status))
            return status;

        _cairo_gl_composite_bind_to_shader (ctx, setup);
        setup->pre_shader = ctx->current_shader;
    }

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gl_composite_begin (cairo_gl_context_t *ctx,
                           cairo_gl_composite_t *setup)
{
    unsigned int dst_size, src_size, mask_size;
    cairo_status_t status;

    assert (! _cairo_gl_context_is_in_progress (ctx));

    /* Do various magic for component alpha */
    if (setup->has_component_alpha) {
        status = _cairo_gl_composite_begin_component_alpha (ctx, setup);
        if (unlikely (status))
            return status;
    }

    status = _cairo_gl_set_shader_by_type (ctx,
                                           setup->src.type,
                                           setup->mask.type,
                                           setup->has_component_alpha ? CAIRO_GL_SHADER_IN_CA_SOURCE
                                                                      : CAIRO_GL_SHADER_IN_NORMAL);
    if (unlikely (status)) {
        setup->pre_shader = NULL;
	return status;
    }

    status = CAIRO_STATUS_SUCCESS;

    dst_size  = 2 * sizeof (GLfloat);
    src_size  = _cairo_gl_operand_get_vertex_size (setup->src.type);
    mask_size = _cairo_gl_operand_get_vertex_size (setup->mask.type);

    ctx->vertex_size = dst_size + src_size + mask_size;

    _cairo_gl_context_set_destination (ctx, setup->dst);
    _cairo_gl_set_operator (setup->dst,
                            setup->op,
                            setup->has_component_alpha);

    _cairo_gl_composite_bind_to_shader (ctx, setup);

    glBindBufferARB (GL_ARRAY_BUFFER_ARB, ctx->vbo);

    glVertexPointer (2, GL_FLOAT, ctx->vertex_size, NULL);
    glEnableClientState (GL_VERTEX_ARRAY);

    _cairo_gl_operand_setup_texture (ctx, &setup->src, 0, dst_size);
    _cairo_gl_operand_setup_texture (ctx, &setup->mask, 1, dst_size + src_size);

    _cairo_gl_set_src_operand (ctx, setup);
    if (setup->has_component_alpha)
        _cairo_gl_set_component_alpha_mask_operand (ctx, setup);
    else
        _cairo_gl_set_mask_operand (ctx, setup);

    if (setup->clip_region)
	glEnable (GL_SCISSOR_TEST);

    return status;
}

static inline void
_cairo_gl_composite_draw (cairo_gl_context_t *ctx,
                          cairo_gl_composite_t *setup,
			  unsigned int count)
{
    if (! setup->pre_shader) {
        glDrawArrays (GL_TRIANGLES, 0, count);
    } else {
        cairo_gl_shader_t *prev_shader = ctx->current_shader;

        _cairo_gl_set_shader (ctx, setup->pre_shader);
        _cairo_gl_set_operator (setup->dst, CAIRO_OPERATOR_DEST_OUT, TRUE);
        _cairo_gl_set_src_alpha_operand (ctx, setup);
        _cairo_gl_set_component_alpha_mask_operand (ctx, setup);
        glDrawArrays (GL_TRIANGLES, 0, count);

        _cairo_gl_set_shader (ctx, prev_shader);
        _cairo_gl_set_operator (setup->dst, setup->op, TRUE);
        _cairo_gl_set_src_operand (ctx, setup);
        _cairo_gl_set_component_alpha_mask_operand (ctx, setup);
        glDrawArrays (GL_TRIANGLES, 0, count);
    }
}

void
_cairo_gl_composite_flush (cairo_gl_context_t *ctx,
                           cairo_gl_composite_t *setup)
{
    unsigned int count;

    if (ctx->vb_offset == 0)
        return;

    count = ctx->vb_offset / ctx->vertex_size;

    glUnmapBufferARB (GL_ARRAY_BUFFER_ARB);
    ctx->vb = NULL;
    ctx->vb_offset = 0;

    if (setup->clip_region) {
	int i, num_rectangles = cairo_region_num_rectangles (setup->clip_region);

	for (i = 0; i < num_rectangles; i++) {
	    cairo_rectangle_int_t rect;

	    cairo_region_get_rectangle (setup->clip_region, i, &rect);

	    glScissor (rect.x, rect.y, rect.width, rect.height);
            _cairo_gl_composite_draw (ctx, setup, count);
	}
    } else {
        _cairo_gl_composite_draw (ctx, setup, count);
    }
}

static void
_cairo_gl_composite_prepare_buffer (cairo_gl_context_t *ctx,
                                    cairo_gl_composite_t *setup,
                                    unsigned int n_vertices)
{
    if (ctx->vb_offset + n_vertices * ctx->vertex_size > CAIRO_GL_VBO_SIZE)
	_cairo_gl_composite_flush (ctx, setup);

    if (ctx->vb == NULL) {
	glBufferDataARB (GL_ARRAY_BUFFER_ARB, CAIRO_GL_VBO_SIZE,
			 NULL, GL_STREAM_DRAW_ARB);
	ctx->vb = glMapBufferARB (GL_ARRAY_BUFFER_ARB, GL_WRITE_ONLY_ARB);
    }
}

static inline void
_cairo_gl_operand_emit (cairo_gl_operand_t *operand,
                        GLfloat ** vb,
                        GLfloat x,
                        GLfloat y,
                        uint8_t alpha)
{
    switch (operand->type) {
    default:
    case CAIRO_GL_OPERAND_COUNT:
        ASSERT_NOT_REACHED;
    case CAIRO_GL_OPERAND_NONE:
    case CAIRO_GL_OPERAND_CONSTANT:
    case CAIRO_GL_OPERAND_LINEAR_GRADIENT:
    case CAIRO_GL_OPERAND_RADIAL_GRADIENT:
        break;
    case CAIRO_GL_OPERAND_SPANS:
        {
            union fi {
                float f;
                GLbyte bytes[4];
            } fi;

            fi.bytes[0] = 0;
            fi.bytes[1] = 0;
            fi.bytes[2] = 0;
            fi.bytes[3] = alpha;
            *(*vb)++ = fi.f;
        }
        break;
    case CAIRO_GL_OPERAND_TEXTURE:
        {
            cairo_surface_attributes_t *src_attributes = &operand->texture.attributes;
            double s = x;
            double t = y;

            cairo_matrix_transform_point (&src_attributes->matrix, &s, &t);
            *(*vb)++ = s;
            *(*vb)++ = t;
        }
        break;
    }
}

static inline void
_cairo_gl_composite_emit_vertex (cairo_gl_context_t *ctx,
                                 cairo_gl_composite_t *setup,
                                 GLfloat x,
                                 GLfloat y,
                                 uint8_t alpha)
{
    GLfloat *vb = (GLfloat *) (void *) &ctx->vb[ctx->vb_offset];

    *vb++ = x;
    *vb++ = y;

    _cairo_gl_operand_emit (&setup->src, &vb, x, y, alpha);
    _cairo_gl_operand_emit (&setup->mask, &vb, x, y, alpha);

    ctx->vb_offset += ctx->vertex_size;
}

void
_cairo_gl_composite_emit_rect (cairo_gl_context_t *ctx,
                               cairo_gl_composite_t *setup,
                               GLfloat x1,
                               GLfloat y1,
                               GLfloat x2,
                               GLfloat y2,
                               uint8_t alpha)
{
    _cairo_gl_composite_prepare_buffer (ctx, setup, 6);

    _cairo_gl_composite_emit_vertex (ctx, setup, x1, y1, alpha);
    _cairo_gl_composite_emit_vertex (ctx, setup, x2, y1, alpha);
    _cairo_gl_composite_emit_vertex (ctx, setup, x1, y2, alpha);

    _cairo_gl_composite_emit_vertex (ctx, setup, x2, y1, alpha);
    _cairo_gl_composite_emit_vertex (ctx, setup, x2, y2, alpha);
    _cairo_gl_composite_emit_vertex (ctx, setup, x1, y2, alpha);
}

static inline void
_cairo_gl_composite_emit_glyph_vertex (cairo_gl_context_t *ctx,
                                       cairo_gl_composite_t *setup,
                                       GLfloat x,
                                       GLfloat y,
                                       GLfloat glyph_x,
                                       GLfloat glyph_y)
{
    GLfloat *vb = (GLfloat *) (void *) &ctx->vb[ctx->vb_offset];

    *vb++ = x;
    *vb++ = y;

    _cairo_gl_operand_emit (&setup->src, &vb, x, y, 0);

    *vb++ = glyph_x;
    *vb++ = glyph_y;

    ctx->vb_offset += ctx->vertex_size;
}

void
_cairo_gl_composite_emit_glyph (cairo_gl_context_t *ctx,
                                cairo_gl_composite_t *setup,
                                GLfloat x1,
                                GLfloat y1,
                                GLfloat x2,
                                GLfloat y2,
                                GLfloat glyph_x1,
                                GLfloat glyph_y1,
                                GLfloat glyph_x2,
                                GLfloat glyph_y2)
{
    _cairo_gl_composite_prepare_buffer (ctx, setup, 6);

    _cairo_gl_composite_emit_glyph_vertex (ctx, setup, x1, y1, glyph_x1, glyph_y1);
    _cairo_gl_composite_emit_glyph_vertex (ctx, setup, x2, y1, glyph_x2, glyph_y1);
    _cairo_gl_composite_emit_glyph_vertex (ctx, setup, x1, y2, glyph_x1, glyph_y2);

    _cairo_gl_composite_emit_glyph_vertex (ctx, setup, x2, y1, glyph_x2, glyph_y1);
    _cairo_gl_composite_emit_glyph_vertex (ctx, setup, x2, y2, glyph_x2, glyph_y2);
    _cairo_gl_composite_emit_glyph_vertex (ctx, setup, x1, y2, glyph_x1, glyph_y2);
}

void
_cairo_gl_composite_end (cairo_gl_context_t *ctx,
                         cairo_gl_composite_t *setup)
{
    _cairo_gl_composite_flush (ctx, setup);

    if (setup->clip_region)
	glDisable (GL_SCISSOR_TEST);

    glBindBufferARB (GL_ARRAY_BUFFER_ARB, 0);

    _cairo_gl_set_shader (ctx, NULL);
    glDisable (GL_BLEND);

    glDisableClientState (GL_VERTEX_ARRAY);
    glDisableClientState (GL_COLOR_ARRAY);

    glClientActiveTexture (GL_TEXTURE0);
    glDisableClientState (GL_TEXTURE_COORD_ARRAY);
    glActiveTexture (GL_TEXTURE0);
    glDisable (GL_TEXTURE_1D);
    glDisable (ctx->tex_target);

    glClientActiveTexture (GL_TEXTURE1);
    glDisableClientState (GL_TEXTURE_COORD_ARRAY);
    glActiveTexture (GL_TEXTURE1);
    glDisable (GL_TEXTURE_1D);
    glDisable (ctx->tex_target);

    setup->pre_shader = NULL;

    ctx->vertex_size = 0;
}

void
_cairo_gl_composite_fini (cairo_gl_context_t *ctx,
                          cairo_gl_composite_t *setup)
{
    _cairo_gl_operand_destroy (&setup->src);
    _cairo_gl_operand_destroy (&setup->mask);
}

cairo_status_t
_cairo_gl_composite_init (cairo_gl_context_t *ctx,
                          cairo_gl_composite_t *setup,
                          cairo_operator_t op,
                          cairo_gl_surface_t *dst,
                          cairo_bool_t assume_component_alpha,
                          const cairo_rectangle_int_t *rect)
{
    memset (setup, 0, sizeof (cairo_gl_composite_t));

    if (assume_component_alpha) {
        if (op != CAIRO_OPERATOR_CLEAR &&
            op != CAIRO_OPERATOR_OVER &&
            op != CAIRO_OPERATOR_ADD)
            return UNSUPPORTED ("unsupported component alpha operator");
    } else {
        if (! _cairo_gl_operator_is_supported (op))
            return UNSUPPORTED ("unsupported operator");
    }

    setup->dst = dst;
    setup->op = op;

    return CAIRO_STATUS_SUCCESS;
}

