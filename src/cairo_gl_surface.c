/*
 * Copyright © 2002 University of Southern California
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of the
 * University of Southern California not be used in advertising or
 * publicity pertaining to distribution of the software without
 * specific, written prior permission. The University of Southern
 * California makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 *
 * THE UNIVERSITY OF SOUTHERN CALIFORNIA DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL THE UNIVERSITY OF
 * SOUTHERN CALIFORNIA BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author: David Reveman <c99drn@cs.umu.se>
 */

#include "cairoint.h"

static cairo_surface_t *
_cairo_gl_surface_create (glitz_surface_t *surface,
			  int owns_surface);

void
cairo_set_target_gl (cairo_t *cr, glitz_surface_t *surface)
{
    cairo_surface_t *crsurface;

    if (cr->status && cr->status != CAIRO_STATUS_NO_TARGET_SURFACE)
	return;

    if (glitz_surface_get_hints (surface) & GLITZ_HINT_PROGRAMMATIC_MASK) {
	cr->status = CAIRO_STATUS_NO_TARGET_SURFACE;
	return;
    }

    crsurface = _cairo_gl_surface_create (surface, 0);
    if (crsurface == NULL) {
	cr->status = CAIRO_STATUS_NO_MEMORY;
	return;
    }

    cairo_set_target_surface (cr, crsurface);

    cairo_surface_destroy (crsurface);
}

typedef struct cairo_gl_surface cairo_gl_surface_t;

struct cairo_gl_surface {
    cairo_surface_t base;

    glitz_format_t *format;
    long int features;
    long int hints;
    int owns_surface;
    unsigned short opacity;

    cairo_pattern_t pattern;
    cairo_box_t pattern_box;

    glitz_surface_t *surface;
};

#define CAIRO_GL_MULTITEXTURE_SUPPORT(surface) \
    (surface->features & GLITZ_FEATURE_ARB_MULTITEXTURE_MASK)

#define CAIRO_GL_OFFSCREEN_SUPPORT(surface) \
    (surface->features & GLITZ_FEATURE_OFFSCREEN_DRAWING_MASK)

#define CAIRO_GL_CONVOLUTION_SUPPORT(surface) \
    (surface->features & GLITZ_FEATURE_CONVOLUTION_FILTER_MASK)

#define CAIRO_GL_FRAGMENT_PROGRAM_SUPPORT(surface) \
    (surface->features & GLITZ_FEATURE_ARB_FRAGMENT_PROGRAM_MASK)

#define CAIRO_GL_TEXTURE_NPOT_SUPPORT(surface) \
    (surface->features & GLITZ_FEATURE_TEXTURE_NPOT_MASK)

#define CAIRO_GL_TEXTURE_MIRRORED_REPEAT_SUPPORT(surface) \
    (surface->features & GLITZ_FEATURE_TEXTURE_MIRRORED_REPEAT_MASK)

#define CAIRO_GL_COMPOSITE_TRAPEZOIDS_SUPPORT(surface) \
    ((surface->format->stencil_size >= \
        ((surface->hints & GLITZ_HINT_CLIPPING_MASK)? 2: 1)) || \
    CAIRO_GL_OFFSCREEN_SUPPORT (surface))

#define CAIRO_GL_SURFACE_IS_OFFSCREEN(surface) \
    (surface->hints & GLITZ_HINT_OFFSCREEN_MASK)

#define CAIRO_GL_SURFACE_IS_SOLID(surface) \
    ((surface->hints & GLITZ_HINT_PROGRAMMATIC_MASK) && \
     (surface->pattern.type == CAIRO_PATTERN_SOLID))

static void
_cairo_gl_surface_destroy (void *abstract_surface)
{
    cairo_gl_surface_t *surface = abstract_surface;

    if (surface->owns_surface && surface->surface)
	glitz_surface_destroy (surface->surface);

    _cairo_pattern_fini (&surface->pattern);

    free (surface);
}

static double
_cairo_gl_surface_pixels_per_inch (void *abstract_surface)
{
    return 96.0;
}

static cairo_image_surface_t *
_cairo_gl_surface_get_image (void *abstract_surface)
{
    cairo_gl_surface_t *surface = abstract_surface;
    cairo_image_surface_t *image;
    char *pixels;
    int width, height;
    int rowstride;
    cairo_format_masks_t format;

    if (surface->hints & GLITZ_HINT_PROGRAMMATIC_MASK)
	return _cairo_pattern_get_image (&surface->pattern,
					 &surface->pattern_box);

    width = glitz_surface_get_width (surface->surface);
    height = glitz_surface_get_height (surface->surface);

    rowstride = (width * (surface->format->bpp / 8) + 3) & -4;

    pixels = (char *) malloc (sizeof (char) * height * rowstride);

    glitz_surface_read_pixels (surface->surface, 0, 0, width, height, pixels);

    format.bpp = surface->format->bpp;
    format.red_mask = surface->format->red_mask;
    format.green_mask = surface->format->green_mask;
    format.blue_mask = surface->format->blue_mask;
    format.alpha_mask = surface->format->alpha_mask;

    image = (cairo_image_surface_t *)
        _cairo_image_surface_create_with_masks (pixels,
						&format,
						width, height, rowstride);

    _cairo_image_surface_assume_ownership_of_data (image);

    _cairo_image_surface_set_repeat (image, surface->base.repeat);
    _cairo_image_surface_set_matrix (image, &(surface->base.matrix));

    return image;
}

static cairo_status_t
_cairo_gl_surface_set_image (void *abstract_surface,
			     cairo_image_surface_t *image)
{
    cairo_gl_surface_t *surface = abstract_surface;

    glitz_surface_draw_pixels (surface->surface, 0, 0,
			       image->width, image->height, image->data);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_gl_surface_set_matrix (void *abstract_surface, cairo_matrix_t *matrix)
{
    cairo_gl_surface_t *surface = abstract_surface;
    glitz_transform_t transform;

    if (!surface->surface)
	return CAIRO_STATUS_SUCCESS;

    transform.matrix[0][0] = _cairo_fixed_from_double (matrix->m[0][0]);
    transform.matrix[0][1] = _cairo_fixed_from_double (matrix->m[1][0]);
    transform.matrix[0][2] = _cairo_fixed_from_double (matrix->m[2][0]);

    transform.matrix[1][0] = _cairo_fixed_from_double (matrix->m[0][1]);
    transform.matrix[1][1] = _cairo_fixed_from_double (matrix->m[1][1]);
    transform.matrix[1][2] = _cairo_fixed_from_double (matrix->m[2][1]);

    transform.matrix[2][0] = 0;
    transform.matrix[2][1] = 0;
    transform.matrix[2][2] = _cairo_fixed_from_double (1);

    glitz_surface_set_transform (surface->surface, &transform);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_gl_surface_set_filter (void *abstract_surface, cairo_filter_t filter)
{
    static glitz_convolution_t gaussian = {
        {
            {0, 1 << 16, 0},
            {1 << 16, 4 << 16, 1 << 16},
            {0, 1 << 16, 0}
        }
    };
    cairo_gl_surface_t *surface = abstract_surface;
    glitz_filter_t gl_filter;
    glitz_convolution_t *convolution = NULL;

    if (!surface->surface)
	return CAIRO_STATUS_SUCCESS;

    switch (filter) {
    case CAIRO_FILTER_FAST:
	gl_filter = GLITZ_FILTER_FAST;
	break;
    case CAIRO_FILTER_GOOD:
	gl_filter = GLITZ_FILTER_GOOD;
	break;
    case CAIRO_FILTER_BEST:
	gl_filter = GLITZ_FILTER_BEST;
	break;
    case CAIRO_FILTER_NEAREST:
	gl_filter = GLITZ_FILTER_NEAREST;
	break;
    case CAIRO_FILTER_GAUSSIAN:
	if (CAIRO_GL_CONVOLUTION_SUPPORT (surface))
	    convolution = &gaussian;
	/* fall-through */
    case CAIRO_FILTER_BILINEAR:
	gl_filter = GLITZ_FILTER_BILINEAR;
	break;
    default:
	gl_filter = GLITZ_FILTER_BEST;
    }

    glitz_surface_set_filter (surface->surface, gl_filter);
    glitz_surface_set_convolution (surface->surface, convolution);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_gl_surface_set_repeat (void *abstract_surface, int repeat)
{
    cairo_gl_surface_t *surface = abstract_surface;

    if (!surface->surface)
	return CAIRO_STATUS_SUCCESS;

    glitz_surface_set_repeat (surface->surface, repeat);

    return CAIRO_STATUS_SUCCESS;
}

static glitz_operator_t
_glitz_operator (cairo_operator_t operator)
{
    switch (operator) {
    case CAIRO_OPERATOR_CLEAR:
	return GLITZ_OPERATOR_CLEAR;
    case CAIRO_OPERATOR_SRC:
	return GLITZ_OPERATOR_SRC;
    case CAIRO_OPERATOR_DST:
	return GLITZ_OPERATOR_DST;
    case CAIRO_OPERATOR_OVER:
	return GLITZ_OPERATOR_OVER;
    case CAIRO_OPERATOR_OVER_REVERSE:
	return GLITZ_OPERATOR_OVER_REVERSE;
    case CAIRO_OPERATOR_IN:
	return GLITZ_OPERATOR_IN;
    case CAIRO_OPERATOR_IN_REVERSE:
	return GLITZ_OPERATOR_IN_REVERSE;
    case CAIRO_OPERATOR_OUT:
	return GLITZ_OPERATOR_OUT;
    case CAIRO_OPERATOR_OUT_REVERSE:
	return GLITZ_OPERATOR_OUT_REVERSE;
    case CAIRO_OPERATOR_ATOP:
	return GLITZ_OPERATOR_ATOP;
    case CAIRO_OPERATOR_ATOP_REVERSE:
	return GLITZ_OPERATOR_ATOP_REVERSE;
    case CAIRO_OPERATOR_XOR:
	return GLITZ_OPERATOR_XOR;
    case CAIRO_OPERATOR_ADD:
	return GLITZ_OPERATOR_ADD;
    case CAIRO_OPERATOR_SATURATE:
	return GLITZ_OPERATOR_SATURATE;
    default:
	return GLITZ_OPERATOR_OVER;
    }
}

static glitz_format_name_t
_glitz_format (cairo_format_t format)
{
    switch (format) {
    case CAIRO_FORMAT_A1:
	return GLITZ_STANDARD_A1;
	break;
    case CAIRO_FORMAT_A8:
	return GLITZ_STANDARD_A8;
	break;
    case CAIRO_FORMAT_RGB24:
	return GLITZ_STANDARD_RGB24;
	break;
    case CAIRO_FORMAT_ARGB32:
    default:
	return GLITZ_STANDARD_ARGB32;
	break;
    }
}

static cairo_surface_t *
_cairo_gl_surface_create_similar (void *abstract_src,
				  cairo_format_t format,
				  int width,
				  int height)
{
    cairo_gl_surface_t *src = abstract_src;
    glitz_surface_t *surface;
    cairo_surface_t *crsurface;

    surface = glitz_surface_create_similar (src->surface,
					    _glitz_format (format),
					    width, height);
    if (surface == NULL)
	return NULL;

    crsurface = _cairo_gl_surface_create (surface, 1);
    if (crsurface == NULL)
	glitz_surface_destroy (surface);

    return crsurface;
}

static cairo_gl_surface_t *
_cairo_gl_surface_clone_similar (cairo_surface_t *src,
				 cairo_gl_surface_t *template,
				 cairo_format_t format)
{
    cairo_gl_surface_t *clone;
    cairo_image_surface_t *src_image;

    src_image = _cairo_surface_get_image (src);

    clone = (cairo_gl_surface_t *)
        _cairo_gl_surface_create_similar (template, format,
					  src_image->width,
					  src_image->height);
    if (clone == NULL)
	return NULL;

    _cairo_gl_surface_set_filter (clone, cairo_surface_get_filter (src));

    _cairo_gl_surface_set_image (clone, src_image);

    _cairo_gl_surface_set_matrix (clone, &(src_image->base.matrix));

    cairo_surface_destroy (&src_image->base);

    return clone;
}

static cairo_int_status_t
_cairo_gl_surface_composite (cairo_operator_t operator,
			     cairo_surface_t *generic_src,
			     cairo_surface_t *generic_mask,
			     void *abstract_dst,
			     int src_x,
			     int src_y,
			     int mask_x,
			     int mask_y,
			     int dst_x,
			     int dst_y,
			     unsigned int width,
			     unsigned int height)
{
    cairo_gl_surface_t *dst = abstract_dst;
    cairo_gl_surface_t *src = (cairo_gl_surface_t *) generic_src;
    cairo_gl_surface_t *mask = (cairo_gl_surface_t *) generic_mask;
    cairo_gl_surface_t *src_clone = NULL;
    cairo_gl_surface_t *mask_clone = NULL;

    /* Make sure that target surface is OK. */
    if (glitz_surface_get_status (dst->surface))
	return CAIRO_STATUS_NO_TARGET_SURFACE;

    /* If destination surface is offscreen, then offscreen drawing support is
       required. */
    if (CAIRO_GL_SURFACE_IS_OFFSCREEN (dst) &&
	(!CAIRO_GL_OFFSCREEN_SUPPORT (dst)))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    /* We need multi-texturing or offscreen drawing when compositing with
       non-solid mask. */
    if (mask &&
	(!CAIRO_GL_SURFACE_IS_SOLID (mask)) &&
	(!CAIRO_GL_MULTITEXTURE_SUPPORT (dst)) &&
	(!CAIRO_GL_OFFSCREEN_SUPPORT (dst)))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if (generic_src->backend != dst->base.backend) {
	src_clone = _cairo_gl_surface_clone_similar (generic_src, dst,
						     CAIRO_FORMAT_ARGB32);
	if (!src_clone)
	    return CAIRO_INT_STATUS_UNSUPPORTED;
	src = src_clone;
    }
    if (generic_mask && (generic_mask->backend != dst->base.backend)) {
	mask_clone = _cairo_gl_surface_clone_similar (generic_mask, dst,
						      CAIRO_FORMAT_A8);
	if (!mask_clone)
	    return CAIRO_INT_STATUS_UNSUPPORTED;
	mask = mask_clone;
    }

    glitz_composite (_glitz_operator (operator),
		     src->surface,
		     mask ? mask->surface : 0,
		     dst->surface,
		     src_x, src_y,
		     mask_x, mask_y,
		     dst_x, dst_y,
		     width, height);

    if (src_clone)
	cairo_surface_destroy (&src_clone->base);
    if (mask_clone)
	cairo_surface_destroy (&mask_clone->base);

    if (glitz_surface_get_status (dst->surface) == GLITZ_STATUS_NOT_SUPPORTED)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_gl_surface_fill_rectangles (void *abstract_surface,
				   cairo_operator_t operator,
				   const cairo_color_t *color,
				   cairo_rectangle_t *rects,
				   int num_rects)
{
    cairo_gl_surface_t *surface = abstract_surface;
    glitz_color_t glitz_color;

    /* Make sure that target surface is OK. */
    if (glitz_surface_get_status (surface->surface))
	return CAIRO_STATUS_NO_TARGET_SURFACE;

    /* If destination surface is offscreen, then offscreen drawing support is
       required. */
    if (CAIRO_GL_SURFACE_IS_OFFSCREEN (surface) &&
	(!CAIRO_GL_OFFSCREEN_SUPPORT (surface)))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    glitz_color.red = color->red_short;
    glitz_color.green = color->green_short;
    glitz_color.blue = color->blue_short;
    glitz_color.alpha = color->alpha_short;

    glitz_fill_rectangles (_glitz_operator (operator),
			   surface->surface,
			   &glitz_color, (glitz_rectangle_t *) rects,
			   num_rects);

    return CAIRO_STATUS_SUCCESS;
}

/* This function will produce incorrect drawing if alpha is not 1.0. */
static cairo_int_status_t
_cairo_gl_surface_fill_trapezoids (cairo_gl_surface_t *surface,
				   cairo_operator_t operator,
				   const cairo_color_t *color,
				   cairo_trapezoid_t *traps,
				   int num_traps)
{
    glitz_color_t glitz_color;

    glitz_color.red = color->red_short;
    glitz_color.green = color->green_short;
    glitz_color.blue = color->blue_short;
    glitz_color.alpha = color->alpha_short;

    glitz_fill_trapezoids (_glitz_operator (operator),
			   surface->surface,
			   &glitz_color,
			   (glitz_trapezoid_t *) traps, num_traps);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_gl_surface_composite_trapezoids (cairo_operator_t operator,
					cairo_surface_t *generic_src,
					void *abstract_dst,
					int x_src,
					int y_src,
					cairo_trapezoid_t *traps,
					int num_traps)
{
    cairo_gl_surface_t *dst = abstract_dst;
    cairo_gl_surface_t *src = (cairo_gl_surface_t *) generic_src;
    cairo_gl_surface_t *src_clone = NULL;

    /* Make sure that target surface is OK. */
    if (glitz_surface_get_status (dst->surface))
	return CAIRO_STATUS_NO_TARGET_SURFACE;

    /* If destination surface is offscreen, then offscreen drawing support is
       required. */
    if (CAIRO_GL_SURFACE_IS_OFFSCREEN (dst) &&
	(!CAIRO_GL_OFFSCREEN_SUPPORT (dst)))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    /* Need to get current hints as clipping may have changed. */
    dst->hints = glitz_surface_get_hints (dst->surface);

    /* Solid source? */
    if ((generic_src->backend == dst->base.backend) &&
	(src->pattern.type == CAIRO_PATTERN_SOLID)) {

	/* We can use fill trapezoids if only one trapezoid should be drawn or
	   if solid color alpha is 1.0. If composite trapezoid support
	   is missing we always use fill trapezoids. */
	if ((num_traps == 1) ||
            (src->pattern.color.alpha == 1.0) ||
            (!CAIRO_GL_COMPOSITE_TRAPEZOIDS_SUPPORT (dst)))
	    return _cairo_gl_surface_fill_trapezoids (dst, operator,
						      &src->pattern.color,
						      traps, num_traps);
    }

    if (generic_src->backend != dst->base.backend) {
	src_clone = _cairo_gl_surface_clone_similar (generic_src, dst,
						     CAIRO_FORMAT_ARGB32);
	if (!src_clone)
	    return CAIRO_INT_STATUS_UNSUPPORTED;
	src = src_clone;
    }

    glitz_surface_set_polyopacity (dst->surface, src->opacity);
    
    glitz_composite_trapezoids (_glitz_operator (operator),
				src->surface, dst->surface,
				x_src, y_src, (glitz_trapezoid_t *) traps,
				num_traps);
    
    glitz_surface_set_polyopacity (dst->surface, 0xffff);

    if (src_clone)
	cairo_surface_destroy (&src_clone->base);

    if (glitz_surface_get_status (dst->surface) == GLITZ_STATUS_NOT_SUPPORTED)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_gl_surface_copy_page (void *abstract_surface)
{
    return CAIRO_INT_STATUS_UNSUPPORTED;
}

static cairo_int_status_t
_cairo_gl_surface_show_page (void *abstract_surface)
{
    return CAIRO_INT_STATUS_UNSUPPORTED;
}

static void
_cario_gl_uint_to_power_of_two (unsigned int *value)
{
    unsigned int x = 1;

    while (x < *value)
	x <<= 1;

    *value = x;
}

static void
_cairo_gl_create_color_range (cairo_pattern_t *pattern,
			      unsigned char *data,
			      unsigned int size)
{
    unsigned int i, bytes = size * 4;
    cairo_shader_op_t op;
    
    _cairo_pattern_shader_init (pattern, &op);

    for (i = 0; i < bytes; i += 4)
	_cairo_pattern_calc_color_at_pixel (&op,
					    ((double) i / bytes) * 65536,
					    (int *) &data[i]);
}

static cairo_int_status_t
_cairo_gl_surface_create_pattern (void *abstract_surface,
				  cairo_pattern_t *pattern,
				  cairo_box_t *box)
{
    cairo_gl_surface_t *surface = abstract_surface;
    glitz_surface_t *source = NULL;
    cairo_gl_surface_t *src;

    switch (pattern->type) {
    case CAIRO_PATTERN_SOLID: {
	glitz_color_t color;

	color.red = pattern->color.red_short;
	color.green = pattern->color.green_short;
	color.blue = pattern->color.blue_short;
	color.alpha = pattern->color.alpha_short;

	source = glitz_surface_create_solid (&color);
    } break;
    case CAIRO_PATTERN_LINEAR:
    case CAIRO_PATTERN_RADIAL: {
	unsigned int color_range_size;
	glitz_color_range_t *color_range;

	if (!CAIRO_GL_FRAGMENT_PROGRAM_SUPPORT (surface))
	    return CAIRO_INT_STATUS_UNSUPPORTED;

	/* reflect could be emulated for hardware that doesn't support mirrored
	   repeat of textures, but I don't know of any card that support
	   fragment programs but not mirrored repeat, so what's the use. */
	if (pattern->extend == CAIRO_EXTEND_REFLECT &&
            (!CAIRO_GL_TEXTURE_MIRRORED_REPEAT_SUPPORT (surface)))
	    return CAIRO_INT_STATUS_UNSUPPORTED;

	if (pattern->type == CAIRO_PATTERN_LINEAR) {
	    double dx, dy;

	    dx = pattern->u.linear.point1.x - pattern->u.linear.point0.x;
	    dy = pattern->u.linear.point1.y - pattern->u.linear.point0.y;

	    color_range_size = sqrt (dx * dx + dy * dy);
	} else {
	    /* glitz doesn't support inner circle yet. */
	    if (pattern->u.radial.center0.x != pattern->u.radial.center1.x ||
		pattern->u.radial.center0.y != pattern->u.radial.center1.y)
		return CAIRO_INT_STATUS_UNSUPPORTED;

	    color_range_size = pattern->u.radial.radius1;
	}

	if ((!CAIRO_GL_TEXTURE_NPOT_SUPPORT (surface)))
	    _cario_gl_uint_to_power_of_two (&color_range_size);

	color_range = glitz_color_range_create (color_range_size);
	if (!color_range)
	    return CAIRO_STATUS_NO_MEMORY;

	_cairo_gl_create_color_range (pattern,
				      glitz_color_range_get_data (color_range),
				      color_range_size);

	switch (pattern->extend) {
	case CAIRO_EXTEND_REPEAT:
	    glitz_color_range_set_extend (color_range, GLITZ_EXTEND_REPEAT);
	    break;
	case CAIRO_EXTEND_REFLECT:
	    glitz_color_range_set_extend (color_range, GLITZ_EXTEND_REFLECT);
	    break;
	case CAIRO_EXTEND_NONE:
	    glitz_color_range_set_extend (color_range, GLITZ_EXTEND_PAD);
	    break;
	}

	glitz_color_range_set_filter (color_range, GLITZ_FILTER_BILINEAR);

	if (pattern->type == CAIRO_PATTERN_LINEAR) {
	    glitz_point_fixed_t start;
	    glitz_point_fixed_t stop;

	    start.x = _cairo_fixed_from_double (pattern->u.linear.point0.x);
	    start.y = _cairo_fixed_from_double (pattern->u.linear.point0.y);
	    stop.x = _cairo_fixed_from_double (pattern->u.linear.point1.x);
	    stop.y = _cairo_fixed_from_double (pattern->u.linear.point1.y);

	    source = glitz_surface_create_linear (&start, &stop, color_range);
	} else {
	    glitz_point_fixed_t center;
	    
	    center.x = _cairo_fixed_from_double (pattern->u.radial.center1.x);
	    center.y = _cairo_fixed_from_double (pattern->u.radial.center1.y);
	    
	    source = glitz_surface_create_radial
		(&center,
		 _cairo_fixed_from_double (pattern->u.radial.radius0),
		 _cairo_fixed_from_double (pattern->u.radial.radius1),
		 color_range);
	}

	glitz_color_range_destroy (color_range);
    } break;
    case CAIRO_PATTERN_SURFACE:
	if (CAIRO_GL_COMPOSITE_TRAPEZOIDS_SUPPORT (surface)) {
	    cairo_gl_surface_t *src_clone = NULL;
	    cairo_surface_t *generic_src = pattern->u.surface.surface;

	    src = (cairo_gl_surface_t *) generic_src;
	    if (generic_src->backend != surface->base.backend) {
		src_clone =
		    _cairo_gl_surface_clone_similar (generic_src, surface,
						     CAIRO_FORMAT_ARGB32);
		if (!src_clone)
		    return CAIRO_INT_STATUS_UNSUPPORTED;
	    } else {
		src_clone = (cairo_gl_surface_t *)
		    _cairo_gl_surface_create (src->surface, 0);
		if (!src_clone)
		    return CAIRO_STATUS_NO_MEMORY;
		
		cairo_surface_set_filter
		    (&src_clone->base, cairo_surface_get_filter (generic_src));

		cairo_surface_set_matrix (&src_clone->base,
					  &generic_src->matrix);
	    }
	    
	    cairo_surface_set_repeat (&src_clone->base, generic_src->repeat);
	    
	    src_clone->opacity = (unsigned short)
		(pattern->color.alpha * 0xffff);

	    pattern->source = &src_clone->base;
	    
	    return CAIRO_STATUS_SUCCESS;
	}
	return CAIRO_INT_STATUS_UNSUPPORTED;
	break;
    }
    
    if (!source)
	return CAIRO_STATUS_NO_MEMORY;

    src = (cairo_gl_surface_t *) _cairo_gl_surface_create (source, 1);
    if (!src) {
	glitz_surface_destroy (source);
	
	return CAIRO_STATUS_NO_MEMORY;
    }
    
    if (pattern->type == CAIRO_PATTERN_LINEAR ||
	pattern->type == CAIRO_PATTERN_RADIAL)
	cairo_surface_set_matrix (&src->base, &pattern->matrix);

    _cairo_pattern_init_copy (&src->pattern, pattern);
    src->pattern_box = *box;
    
    pattern->source = &src->base;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_gl_surface_set_clip_region (void *abstract_surface,
				   pixman_region16_t *region)
{
    cairo_gl_surface_t *surface = abstract_surface;
    glitz_rectangle_t *clip_rects;
    pixman_box16_t *box;
    int n, i;

    if (region == NULL) {
	glitz_rectangle_t rect;

	rect.x = 0;
	rect.y = 0;
	rect.width = glitz_surface_get_width (surface->surface);
	rect.height = glitz_surface_get_height (surface->surface);

	glitz_surface_clip_rectangles (surface->surface,
				       GLITZ_CLIP_OPERATOR_SET, &rect, 1);
    }

    if (surface->format->stencil_size < 1)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    n = pixman_region_num_rects (region);
    if (n == 0)
	return CAIRO_STATUS_SUCCESS;

    box = pixman_region_rects (region);

    clip_rects = malloc (n * sizeof (glitz_rectangle_t));
    if (!clip_rects)
	return CAIRO_STATUS_NO_MEMORY;

    for (i = 0; i < n; n++, box++) {
	clip_rects[i].x = box->x1;
	clip_rects[i].y = box->y1;
	clip_rects[i].width = (unsigned short) (box->x2 - box->x1);
	clip_rects[i].height = (unsigned short) (box->y2 - box->y1);
    }

    glitz_surface_clip_rectangles (surface->surface,
				   GLITZ_CLIP_OPERATOR_SET, clip_rects, n);

    free (clip_rects);

    return CAIRO_STATUS_SUCCESS;
}

static const struct cairo_surface_backend cairo_gl_surface_backend = {
    _cairo_gl_surface_create_similar,
    _cairo_gl_surface_destroy,
    _cairo_gl_surface_pixels_per_inch,
    _cairo_gl_surface_get_image,
    _cairo_gl_surface_set_image,
    _cairo_gl_surface_set_matrix,
    _cairo_gl_surface_set_filter,
    _cairo_gl_surface_set_repeat,
    _cairo_gl_surface_composite,
    _cairo_gl_surface_fill_rectangles,
    _cairo_gl_surface_composite_trapezoids,
    _cairo_gl_surface_copy_page,
    _cairo_gl_surface_show_page,
    _cairo_gl_surface_set_clip_region,
    _cairo_gl_surface_create_pattern
};

static cairo_surface_t *
_cairo_gl_surface_create (glitz_surface_t *surface, int owns_surface)
{
    cairo_gl_surface_t *crsurface;

    if (!surface)
	return NULL;

    crsurface = malloc (sizeof (cairo_gl_surface_t));
    if (crsurface == NULL)
	return NULL;

    _cairo_surface_init (&crsurface->base, &cairo_gl_surface_backend);
    _cairo_pattern_init (&crsurface->pattern);
    crsurface->pattern.type = CAIRO_PATTERN_SURFACE;
    crsurface->pattern.u.surface.surface = NULL;
    crsurface->format = glitz_surface_get_format (surface);
    crsurface->surface = surface;
    crsurface->features = glitz_surface_get_features (surface);
    crsurface->hints = glitz_surface_get_hints (surface);
    crsurface->owns_surface = owns_surface;
    crsurface->opacity = 0xffff;

    return (cairo_surface_t *) crsurface;
}

cairo_surface_t *
cairo_gl_surface_create (glitz_surface_t *surface)
{
    return _cairo_gl_surface_create (surface, 0);
}
