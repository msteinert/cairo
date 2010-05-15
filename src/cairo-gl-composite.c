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
 *	Carl Worth <cworth@cworth.org>
 */

#include "cairoint.h"

#include "cairo-error-private.h"
#include "cairo-gl-private.h"

static void
_cairo_gl_set_texture_surface (int tex_unit, GLuint tex,
			       cairo_surface_attributes_t *attributes,
			       GLint tex_target)
{

    if (tex_target == GL_TEXTURE_RECTANGLE_EXT) {
	assert (attributes->extend != CAIRO_EXTEND_REPEAT &&
		attributes->extend != CAIRO_EXTEND_REFLECT);
    }

    glActiveTexture (GL_TEXTURE0 + tex_unit);
    glBindTexture (tex_target, tex);
    switch (attributes->extend) {
    case CAIRO_EXTEND_NONE:
	glTexParameteri (tex_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri (tex_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	break;
    case CAIRO_EXTEND_PAD:
	glTexParameteri (tex_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri (tex_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	break;
    case CAIRO_EXTEND_REPEAT:
	glTexParameteri (tex_target, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri (tex_target, GL_TEXTURE_WRAP_T, GL_REPEAT);
	break;
    case CAIRO_EXTEND_REFLECT:
	glTexParameteri (tex_target, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
	glTexParameteri (tex_target, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
	break;
    }
    switch (attributes->filter) {
    case CAIRO_FILTER_FAST:
    case CAIRO_FILTER_NEAREST:
	glTexParameteri (tex_target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri (tex_target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	break;
    case CAIRO_FILTER_GOOD:
    case CAIRO_FILTER_BEST:
    case CAIRO_FILTER_BILINEAR:
	glTexParameteri (tex_target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri (tex_target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	break;
    default:
    case CAIRO_FILTER_GAUSSIAN:
	ASSERT_NOT_REACHED;
    }
    glEnable (tex_target);
}

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
    attributes = &operand->operand.texture.attributes;

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
    operand->operand.texture.surface = surface;
    operand->operand.texture.tex = surface->tex;
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
    operand->operand.constant.color[0] = color->red   * color->alpha;
    operand->operand.constant.color[1] = color->green * color->alpha;
    operand->operand.constant.color[2] = color->blue  * color->alpha;
    operand->operand.constant.color[3] = color->alpha;
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
					   &operand->operand.linear.tex);

	/* Translation matrix from the destination fragment coordinates
	 * (pixels from lower left = 0,0) to the coordinates in the
	 */
	cairo_matrix_init_translate (&operand->operand.linear.m, -x0, -y0);
	cairo_matrix_multiply (&operand->operand.linear.m,
			       &operand->pattern->matrix,
			       &operand->operand.linear.m);
	cairo_matrix_translate (&operand->operand.linear.m, 0, dst->height);
	cairo_matrix_scale (&operand->operand.linear.m, 1.0, -1.0);

	operand->operand.linear.segment_x = x1 - x0;
	operand->operand.linear.segment_y = y1 - y0;

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
					   &operand->operand.radial.tex);

	/* Translation matrix from the destination fragment coordinates
	 * (pixels from lower left = 0,0) to the coordinates in the
	 */
	cairo_matrix_init_translate (&operand->operand.radial.m, -x0, -y0);
	cairo_matrix_multiply (&operand->operand.radial.m,
			       &operand->pattern->matrix,
			       &operand->operand.radial.m);
	cairo_matrix_translate (&operand->operand.radial.m, 0, dst->height);
	cairo_matrix_scale (&operand->operand.radial.m, 1.0, -1.0);

	operand->operand.radial.circle_1_x = x1 - x0;
	operand->operand.radial.circle_1_y = y1 - y0;
	operand->operand.radial.radius_0 = r0;
	operand->operand.radial.radius_1 = r1;

	operand->type = CAIRO_GL_OPERAND_RADIAL_GRADIENT;
        return CAIRO_STATUS_SUCCESS;
    }

    return CAIRO_INT_STATUS_UNSUPPORTED;
}

cairo_int_status_t
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

void
_cairo_gl_operand_destroy (cairo_gl_operand_t *operand)
{
    switch (operand->type) {
    case CAIRO_GL_OPERAND_CONSTANT:
	break;
    case CAIRO_GL_OPERAND_LINEAR_GRADIENT:
	glDeleteTextures (1, &operand->operand.linear.tex);
	break;
    case CAIRO_GL_OPERAND_RADIAL_GRADIENT:
	glDeleteTextures (1, &operand->operand.radial.tex);
	break;
    case CAIRO_GL_OPERAND_TEXTURE:
	if (operand->operand.texture.surface != NULL) {
	    cairo_gl_surface_t *surface = operand->operand.texture.surface;

	    _cairo_pattern_release_surface (operand->pattern,
					    &surface->base,
					    &operand->operand.texture.attributes);
	}
	break;
    default:
    case CAIRO_GL_OPERAND_COUNT:
        ASSERT_NOT_REACHED;
    case CAIRO_GL_OPERAND_NONE:
    case CAIRO_GL_OPERAND_SPANS:
        break;
    }
}

static void
_cairo_gl_set_tex_combine_constant_color (cairo_gl_context_t *ctx,
					  cairo_gl_composite_t *setup,
					  int tex_unit,
					  GLfloat *color)
{
    if (setup->shader) {
	const char *uniform_name;
        cairo_status_t status;

	if (tex_unit == 0)
	    uniform_name = "source_constant";
	else
	    uniform_name = "mask_constant";

	status = bind_vec4_to_shader (ctx,
                                      setup->shader->program,
                                      uniform_name,
                                      color[0],
                                      color[1],
                                      color[2],
                                      color[3]);
        assert (! _cairo_status_is_error (status));

	return;
    }

    /* Fall back to fixed function */
    glActiveTexture (GL_TEXTURE0 + tex_unit);
    /* Have to have a dummy texture bound in order to use the combiner unit. */
    glBindTexture (ctx->tex_target, ctx->dummy_tex);
    glEnable (ctx->tex_target);

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

void
_cairo_gl_set_src_operand (cairo_gl_context_t *ctx,
			   cairo_gl_composite_t *setup)
{
    cairo_surface_attributes_t *src_attributes;
    GLfloat constant_color[4] = {0.0, 0.0, 0.0, 0.0};
    cairo_status_t status;

    src_attributes = &setup->src.operand.texture.attributes;

    switch (setup->src.type) {
    case CAIRO_GL_OPERAND_CONSTANT:
	_cairo_gl_set_tex_combine_constant_color (ctx, setup, 0,
						  setup->src.operand.constant.color);
	break;
    case CAIRO_GL_OPERAND_TEXTURE:
	_cairo_gl_set_texture_surface (0, setup->src.operand.texture.tex,
				       src_attributes, ctx->tex_target);
	if (!setup->shader) {
	    /* Set up the constant color we use to set color to 0 if needed. */
	    glTexEnvfv (GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, constant_color);
	    /* Set up the combiner to just set color to the sampled texture. */
	    glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
	    glTexEnvi (GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_REPLACE);
	    glTexEnvi (GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE);

	    /* Force the src color to 0 if the surface should be
	     * alpha-only.  We may have a teximage with color bits if
	     * the implementation doesn't support GL_ALPHA FBOs.
	     */
	    if (setup->src.operand.texture.surface->base.content !=
		CAIRO_CONTENT_ALPHA)
		glTexEnvi (GL_TEXTURE_ENV, GL_SRC0_RGB, GL_TEXTURE0);
	    else
		glTexEnvi (GL_TEXTURE_ENV, GL_SRC0_RGB, GL_CONSTANT);
	    glTexEnvi (GL_TEXTURE_ENV, GL_SRC0_ALPHA, GL_TEXTURE0);
	    glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
	    glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
	}
	break;

    case CAIRO_GL_OPERAND_LINEAR_GRADIENT:
	glActiveTexture (GL_TEXTURE0);
	glBindTexture (GL_TEXTURE_1D, setup->src.operand.linear.tex);
	glEnable (GL_TEXTURE_1D);

	status = bind_matrix_to_shader (ctx, setup->shader->program,
					"source_matrix",
					&setup->src.operand.linear.m);
	assert (!_cairo_status_is_error (status));

	status = bind_vec2_to_shader (ctx, setup->shader->program,
				      "source_segment",
				      setup->src.operand.linear.segment_x,
				      setup->src.operand.linear.segment_y);
	assert (!_cairo_status_is_error (status));
	break;

    case CAIRO_GL_OPERAND_RADIAL_GRADIENT:
	glActiveTexture (GL_TEXTURE0);
	glBindTexture (GL_TEXTURE_1D, setup->src.operand.linear.tex);
	glEnable (GL_TEXTURE_1D);

        status = bind_matrix_to_shader (ctx, setup->shader->program,
					"source_matrix",
					&setup->src.operand.radial.m);
	assert (!_cairo_status_is_error (status));

        status = bind_vec2_to_shader (ctx, setup->shader->program,
				      "source_circle_1",
				      setup->src.operand.radial.circle_1_x,
				      setup->src.operand.radial.circle_1_y);
	assert (!_cairo_status_is_error (status));

        status = bind_float_to_shader (ctx, setup->shader->program,
				       "source_radius_0",
				       setup->src.operand.radial.radius_0);
	assert (!_cairo_status_is_error (status));

        status = bind_float_to_shader (ctx, setup->shader->program,
				       "source_radius_1",
				       setup->src.operand.radial.radius_1);
	assert (!_cairo_status_is_error (status));
	break;
    default:
    case CAIRO_GL_OPERAND_COUNT:
        ASSERT_NOT_REACHED;
    case CAIRO_GL_OPERAND_NONE:
    case CAIRO_GL_OPERAND_SPANS:
        break;
    }
}

/* This is like _cairo_gl_set_src_operand, but instead swizzles the source
 * for creating the "source alpha" value (src.aaaa * mask.argb) required by
 * component alpha rendering.
 */
void
_cairo_gl_set_src_alpha_operand (cairo_gl_context_t *ctx,
				 cairo_gl_composite_t *setup)
{
    GLfloat constant_color[4] = {0.0, 0.0, 0.0, 0.0};
    cairo_surface_attributes_t *src_attributes;

    src_attributes = &setup->src.operand.texture.attributes;

    switch (setup->src.type) {
    case CAIRO_GL_OPERAND_CONSTANT:
	constant_color[0] = setup->src.operand.constant.color[3];
	constant_color[1] = setup->src.operand.constant.color[3];
	constant_color[2] = setup->src.operand.constant.color[3];
	constant_color[3] = setup->src.operand.constant.color[3];
	_cairo_gl_set_tex_combine_constant_color (ctx, setup, 0,
						  constant_color);
	break;
    case CAIRO_GL_OPERAND_TEXTURE:
	_cairo_gl_set_texture_surface (0, setup->src.operand.texture.tex,
				       src_attributes, ctx->tex_target);
	if (!setup->shader) {
	    /* Set up the combiner to just set color to the sampled texture. */
	    glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
	    glTexEnvi (GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_REPLACE);
	    glTexEnvi (GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE);

	    glTexEnvi (GL_TEXTURE_ENV, GL_SRC0_RGB, GL_TEXTURE0);
	    glTexEnvi (GL_TEXTURE_ENV, GL_SRC0_ALPHA, GL_TEXTURE0);
	    glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_ALPHA);
	    glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
	}
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
_cairo_gl_set_linear_gradient_mask_operand (cairo_gl_context_t *ctx,
                                            cairo_gl_composite_t *setup)
{
    cairo_status_t status;

    assert(setup->shader);

    glActiveTexture (GL_TEXTURE1);
    glBindTexture (GL_TEXTURE_1D, setup->mask.operand.linear.tex);
    glEnable (GL_TEXTURE_1D);

    status = bind_matrix_to_shader (ctx, setup->shader->program,
			   "mask_matrix", &setup->mask.operand.linear.m);
    assert (!_cairo_status_is_error (status));

    status = bind_vec2_to_shader (ctx, setup->shader->program,
			 "mask_segment",
			 setup->mask.operand.linear.segment_x,
			 setup->mask.operand.linear.segment_y);
    assert (!_cairo_status_is_error (status));
}

static void
_cairo_gl_set_radial_gradient_mask_operand (cairo_gl_context_t *ctx,
                                            cairo_gl_composite_t *setup)
{
    cairo_status_t status;

    assert(setup->shader);

    glActiveTexture (GL_TEXTURE1);
    glBindTexture (GL_TEXTURE_1D, setup->mask.operand.radial.tex);
    glEnable (GL_TEXTURE_1D);

    status = bind_matrix_to_shader (ctx, setup->shader->program,
				    "mask_matrix",
				    &setup->mask.operand.radial.m);
    assert (!_cairo_status_is_error (status));

    status = bind_vec2_to_shader (ctx, setup->shader->program,
				  "mask_circle_1",
				  setup->mask.operand.radial.circle_1_x,
				  setup->mask.operand.radial.circle_1_y);
    assert (!_cairo_status_is_error (status));

    status = bind_float_to_shader (ctx, setup->shader->program,
				   "mask_radius_0",
				   setup->mask.operand.radial.radius_0);
    assert (!_cairo_status_is_error (status));

    status = bind_float_to_shader (ctx, setup->shader->program,
				   "mask_radius_1",
				   setup->mask.operand.radial.radius_1);
    assert (!_cairo_status_is_error (status));
}

/* This is like _cairo_gl_set_src_alpha_operand, for component alpha setup
 * of the mask part of IN to produce a "source alpha" value.
 */
void
_cairo_gl_set_component_alpha_mask_operand (cairo_gl_context_t *ctx,
					    cairo_gl_composite_t *setup)
{
    cairo_surface_attributes_t *mask_attributes;
    GLfloat constant_color[4] = {0.0, 0.0, 0.0, 0.0};

    mask_attributes = &setup->mask.operand.texture.attributes;

    if (!setup->shader) {
	glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
	glTexEnvi (GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
	glTexEnvi (GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_MODULATE);

	glTexEnvi (GL_TEXTURE_ENV, GL_SRC1_RGB, GL_PREVIOUS);
	glTexEnvi (GL_TEXTURE_ENV, GL_SRC1_ALPHA, GL_PREVIOUS);
	glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);
	glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND1_ALPHA, GL_SRC_ALPHA);
    }

    switch (setup->mask.type) {
    case CAIRO_GL_OPERAND_CONSTANT:
	/* Have to have a dummy texture bound in order to use the combiner unit. */
	if (setup->shader) {
            cairo_status_t status;
	    status = bind_vec4_to_shader (ctx, setup->shader->program,
                                          "mask_constant",
                                          setup->src.operand.constant.color[0],
                                          setup->src.operand.constant.color[1],
                                          setup->src.operand.constant.color[2],
                                          setup->src.operand.constant.color[3]);
            assert (! _cairo_status_is_error (status));
	} else {
	    glBindTexture (ctx->tex_target, ctx->dummy_tex);
	    glActiveTexture (GL_TEXTURE1);
	    glEnable (ctx->tex_target);

	    glTexEnvfv (GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR,
			setup->mask.operand.constant.color);

	    glTexEnvi (GL_TEXTURE_ENV, GL_SRC0_RGB, GL_CONSTANT);
	    glTexEnvi (GL_TEXTURE_ENV, GL_SRC0_ALPHA, GL_CONSTANT);
	    glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
	    glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
	}
	break;
    case CAIRO_GL_OPERAND_TEXTURE:
	_cairo_gl_set_texture_surface (1, setup->mask.operand.texture.tex,
				       mask_attributes, ctx->tex_target);
	if (!setup->shader) {
	    /* Set up the constant color we use to set color to 0 if needed. */
	    glTexEnvfv (GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, constant_color);

	    /* Force the mask color to 0 if the surface should be
	     * alpha-only.  We may have a teximage with color bits if
	     * the implementation doesn't support GL_ALPHA FBOs.
	     */
	    if (setup->mask.operand.texture.surface->base.content !=
		CAIRO_CONTENT_ALPHA)
		glTexEnvi (GL_TEXTURE_ENV, GL_SRC0_RGB, GL_TEXTURE1);
	    else
		glTexEnvi (GL_TEXTURE_ENV, GL_SRC0_RGB, GL_CONSTANT);
	    glTexEnvi (GL_TEXTURE_ENV, GL_SRC0_ALPHA, GL_TEXTURE1);
	    glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
	    glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
	}
	break;

    case CAIRO_GL_OPERAND_LINEAR_GRADIENT:
	_cairo_gl_set_linear_gradient_mask_operand (ctx, setup);
	break;

    case CAIRO_GL_OPERAND_RADIAL_GRADIENT:
	_cairo_gl_set_radial_gradient_mask_operand (ctx, setup);
	break;
    case CAIRO_GL_OPERAND_NONE:
    case CAIRO_GL_OPERAND_SPANS:
    case CAIRO_GL_OPERAND_COUNT:
    default:
        ASSERT_NOT_REACHED;
        break;
    }
}

void
_cairo_gl_set_mask_operand (cairo_gl_context_t *ctx,
			    cairo_gl_composite_t *setup)
{
    switch (setup->mask.type) {
    case CAIRO_GL_OPERAND_CONSTANT:
        _cairo_gl_set_tex_combine_constant_color (ctx, setup, 1,
                                                  setup->mask.operand.constant.color);
        break;

    case CAIRO_GL_OPERAND_TEXTURE:
        _cairo_gl_set_texture_surface (1, setup->mask.operand.texture.tex,
                                       &setup->mask.operand.texture.attributes,
                                       ctx->tex_target);

        if (! setup->shader) {
            glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
            glTexEnvi (GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
            glTexEnvi (GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_MODULATE);

            glTexEnvi (GL_TEXTURE_ENV, GL_SRC0_RGB, GL_PREVIOUS);
            glTexEnvi (GL_TEXTURE_ENV, GL_SRC0_ALPHA, GL_PREVIOUS);
            glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
            glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);

            /* IN: dst.argb = src.argb * mask.aaaa */
            glTexEnvi (GL_TEXTURE_ENV, GL_SRC1_RGB, GL_TEXTURE1);
            glTexEnvi (GL_TEXTURE_ENV, GL_SRC1_ALPHA, GL_TEXTURE1);
            glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_ALPHA);
            glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND1_ALPHA, GL_SRC_ALPHA);
        }
        break;
    case CAIRO_GL_OPERAND_LINEAR_GRADIENT:
        _cairo_gl_set_linear_gradient_mask_operand (ctx, setup);
        break;
    case CAIRO_GL_OPERAND_RADIAL_GRADIENT:
        _cairo_gl_set_radial_gradient_mask_operand (ctx, setup);
        break;
    case CAIRO_GL_OPERAND_NONE:
        break;
    case CAIRO_GL_OPERAND_SPANS:
    case CAIRO_GL_OPERAND_COUNT:
    default:
        ASSERT_NOT_REACHED;
        break;
    }
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
                          const cairo_pattern_t *src,
                          const cairo_pattern_t *mask,
                          const cairo_rectangle_int_t *rect)
{
    memset (setup, 0, sizeof (cairo_gl_composite_t));

    if (! _cairo_gl_operator_is_supported (op))
	return UNSUPPORTED ("unsupported operator");
    
    return CAIRO_STATUS_SUCCESS;
}

