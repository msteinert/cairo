/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2004 David Reveman
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of David
 * Reveman not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission. David Reveman makes no representations about the
 * suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * DAVID REVEMAN DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL DAVID REVEMAN BE LIABLE FOR ANY SPECIAL,
 * INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
 * IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author: David Reveman <davidr@novell.com>
 */

#include "cairoint.h"
#include "cairo-glitz.h"

#define GLITZ_FIXED_TO_FLOAT(f) \
  (((glitz_float_t) (f)) / 65536)

#define GLITZ_FIXED_LINE_X_TO_FLOAT(line, v) \
  (((glitz_float_t) \
      ((line).p1.x + (cairo_fixed_16_16_t) \
       (((cairo_fixed_32_32_t) ((v) - (line).p1.y) * \
        ((line).p2.x - (line).p1.x)) / \
	((line).p2.y - (line).p1.y)))) / 65536)

#define GLITZ_FIXED_LINE_X_CEIL_TO_FLOAT(line, v) \
  (((glitz_float_t) \
      ((line).p1.x + (cairo_fixed_16_16_t) \
       (((((line).p2.y - (line).p1.y) - 1) + \
         ((cairo_fixed_32_32_t) ((v) - (line).p1.y) * \
          ((line).p2.x - (line).p1.x))) / \
	((line).p2.y - (line).p1.y)))) / 65536)

void
cairo_set_target_glitz (cairo_t *cr, glitz_surface_t *surface)
{
    cairo_surface_t *crsurface;

    if (cr->status && cr->status != CAIRO_STATUS_NO_TARGET_SURFACE)
	return;

    crsurface = cairo_glitz_surface_create (surface);
    if (crsurface == NULL) {
	cr->status = CAIRO_STATUS_NO_MEMORY;
	return;
    }

    cairo_set_target_surface (cr, crsurface);

    cairo_surface_destroy (crsurface);
}

typedef struct _cairo_glitz_surface {
    cairo_surface_t base;

    glitz_surface_t *surface;
    glitz_format_t  *format;
} cairo_glitz_surface_t;

static void
_cairo_glitz_surface_destroy (void *abstract_surface)
{
    cairo_glitz_surface_t *surface = abstract_surface;
    
    glitz_surface_destroy (surface->surface);
    free (surface);
}

static double
_cairo_glitz_surface_pixels_per_inch (void *abstract_surface)
{
    return 96.0;
}

static cairo_image_surface_t *
_cairo_glitz_surface_get_image (void *abstract_surface)
{
    cairo_glitz_surface_t *surface = abstract_surface;
    cairo_image_surface_t *image;
    char		  *pixels;
    int			  width, height;
    cairo_format_masks_t  format;
    glitz_buffer_t	  *buffer;
    glitz_pixel_format_t  pf;

    width = glitz_surface_get_width (surface->surface);
    height = glitz_surface_get_height (surface->surface);

    if (surface->format->type == GLITZ_FORMAT_TYPE_COLOR) {
	if (surface->format->color.red_size > 0) {
	    format.bpp = 32;
	    
	    if (surface->format->color.alpha_size > 0)
		format.alpha_mask = 0xff000000;
	    else
		format.alpha_mask = 0x0;
	    
	    format.red_mask = 0xff0000;
	    format.green_mask = 0xff00;
	    format.blue_mask = 0xff;
	} else {
	    format.bpp = 8;
	    format.blue_mask = format.green_mask = format.red_mask = 0x0;
	    format.alpha_mask = 0xff;
	}
    } else {
	format.bpp = 32;
	format.alpha_mask = 0xff000000;
	format.red_mask = 0xff0000;
	format.green_mask = 0xff00;
	format.blue_mask = 0xff;
    }

    pf.masks.bpp = format.bpp;
    pf.masks.alpha_mask = format.alpha_mask;
    pf.masks.red_mask = format.red_mask;
    pf.masks.green_mask = format.green_mask;
    pf.masks.blue_mask = format.blue_mask;
    pf.xoffset = 0;
    pf.skip_lines = 0;
    pf.bytes_per_line = (((width * format.bpp) / 8) + 3) & -4;
    pf.scanline_order = GLITZ_PIXEL_SCANLINE_ORDER_TOP_DOWN;

    pixels = malloc (height * pf.bytes_per_line);
    if (!pixels)
	return NULL;

    buffer = glitz_buffer_create_for_data (pixels);
    if (!buffer) {
	free (pixels);
	return NULL;
    }
    
    glitz_get_pixels (surface->surface,
		      0, 0,
		      width, height,
		      &pf,
		      buffer);

    glitz_buffer_destroy (buffer);
    
    image = (cairo_image_surface_t *)
        _cairo_image_surface_create_with_masks (pixels,
						&format,
						width, height,
						pf.bytes_per_line);
    
    _cairo_image_surface_assume_ownership_of_data (image);

    _cairo_image_surface_set_repeat (image, surface->base.repeat);
    _cairo_image_surface_set_matrix (image, &(surface->base.matrix));

    return image;
}

static cairo_status_t
_cairo_glitz_surface_set_image (void		      *abstract_surface,
				cairo_image_surface_t *image)
{
    cairo_glitz_surface_t *surface = abstract_surface;
    glitz_buffer_t	  *buffer;
    glitz_pixel_format_t  pf;
    pixman_format_t	  *format;
    int			  am, rm, gm, bm;
    
    format = pixman_image_get_format (image->pixman_image);
    if (!format)
	return CAIRO_STATUS_NO_MEMORY;

    pixman_format_get_masks (format, &pf.masks.bpp, &am, &rm, &gm, &bm);

    pf.masks.alpha_mask = am;
    pf.masks.red_mask = rm;
    pf.masks.green_mask = gm;
    pf.masks.blue_mask = bm;
    pf.xoffset = 0;
    pf.skip_lines = 0;
    pf.bytes_per_line = image->stride;
    pf.scanline_order = GLITZ_PIXEL_SCANLINE_ORDER_TOP_DOWN;

    buffer = glitz_buffer_create_for_data (image->data);
    if (!buffer)
	return CAIRO_STATUS_NO_MEMORY;
    
    glitz_set_pixels (surface->surface,
		      0, 0,
		      image->width, image->height,
		      &pf,
		      buffer);
    
    glitz_buffer_destroy (buffer);
    
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_glitz_surface_set_matrix (void		*abstract_surface,
				 cairo_matrix_t *matrix)
{
    cairo_glitz_surface_t *surface = abstract_surface;
    glitz_transform_t	  transform;

    transform.matrix[0][0] = _cairo_fixed_from_double (matrix->m[0][0]);
    transform.matrix[0][1] = _cairo_fixed_from_double (matrix->m[1][0]);
    transform.matrix[0][2] = _cairo_fixed_from_double (matrix->m[2][0]);

    transform.matrix[1][0] = _cairo_fixed_from_double (matrix->m[0][1]);
    transform.matrix[1][1] = _cairo_fixed_from_double (matrix->m[1][1]);
    transform.matrix[1][2] = _cairo_fixed_from_double (matrix->m[2][1]);

    transform.matrix[2][0] = 0;
    transform.matrix[2][1] = 0;
    transform.matrix[2][2] = 1 << 16;

    glitz_surface_set_transform (surface->surface, &transform);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_glitz_surface_set_filter (void		*abstract_surface,
				 cairo_filter_t filter)
{
    cairo_glitz_surface_t *surface = abstract_surface;
    glitz_filter_t	  glitz_filter;

    switch (filter) {
    case CAIRO_FILTER_FAST:
    case CAIRO_FILTER_NEAREST:
	glitz_filter = GLITZ_FILTER_NEAREST;
	break;
    case CAIRO_FILTER_GOOD:
    case CAIRO_FILTER_BEST:
    case CAIRO_FILTER_BILINEAR:
    default:
	glitz_filter = GLITZ_FILTER_BILINEAR;
	break;
    }

    glitz_surface_set_filter (surface->surface, glitz_filter, NULL, 0);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_glitz_surface_set_repeat (void *abstract_surface,
				 int  repeat)
{
    cairo_glitz_surface_t *surface = abstract_surface;

    glitz_surface_set_fill (surface->surface,
			    (repeat)? GLITZ_FILL_REPEAT:
			    GLITZ_FILL_TRANSPARENT);

    return CAIRO_STATUS_SUCCESS;
}

static glitz_operator_t
_glitz_operator (cairo_operator_t op)
{
    switch (op) {
    case CAIRO_OPERATOR_CLEAR:
	return GLITZ_OPERATOR_CLEAR;
    case CAIRO_OPERATOR_SRC:
	return GLITZ_OPERATOR_SRC;
    case CAIRO_OPERATOR_DST:
	return GLITZ_OPERATOR_DST;
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
    case CAIRO_OPERATOR_OVER:
    default:
	return GLITZ_OPERATOR_OVER;
    }
}

static glitz_status_t
_glitz_ensure_target (glitz_surface_t *surface)
{
    glitz_drawable_t *drawable;

    drawable = glitz_surface_get_attached_drawable (surface);
    if (!drawable) {
	glitz_drawable_format_t *dformat;
	glitz_drawable_format_t templ;
	glitz_format_t *format;
	glitz_drawable_t *pbuffer;
	unsigned long mask;
	int i;
	
	format = glitz_surface_get_format (surface);
	if (format->type != GLITZ_FORMAT_TYPE_COLOR)
	    return CAIRO_INT_STATUS_UNSUPPORTED;

	drawable = glitz_surface_get_drawable (surface);
	dformat = glitz_drawable_get_format (drawable);

	templ.types.pbuffer = 1;
	mask = GLITZ_FORMAT_PBUFFER_MASK;

	templ.samples = dformat->samples;
	mask |= GLITZ_FORMAT_SAMPLES_MASK;

	i = 0;
	do {
	    dformat = glitz_find_similar_drawable_format (drawable,
							  mask, &templ, i++);

	    if (dformat) {
		int sufficient = 1;
		
		if (format->color.red_size) {
		    if (dformat->color.red_size < format->color.red_size)
			sufficient = 0;
		}
		if (format->color.alpha_size) {
		    if (dformat->color.alpha_size < format->color.alpha_size)
			sufficient = 0;
		}

		if (sufficient)
		    break;
	    }
	} while (dformat);
	
	if (!dformat)
	    return CAIRO_INT_STATUS_UNSUPPORTED;

	pbuffer =
	    glitz_create_pbuffer_drawable (drawable, dformat,
					   glitz_surface_get_width (surface),
					   glitz_surface_get_height (surface));
	if (!pbuffer)
	    return CAIRO_INT_STATUS_UNSUPPORTED;

	glitz_surface_attach (surface, pbuffer,
			      GLITZ_DRAWABLE_BUFFER_FRONT_COLOR,
			      0, 0);

	glitz_drawable_destroy (pbuffer);
    }

    return CAIRO_STATUS_SUCCESS;
}

static glitz_format_name_t
_glitz_format (cairo_format_t format)
{
    switch (format) {
    default:
    case CAIRO_FORMAT_ARGB32:
	return GLITZ_STANDARD_ARGB32;
    case CAIRO_FORMAT_RGB24:
	return GLITZ_STANDARD_RGB24;
    case CAIRO_FORMAT_A8:
	return GLITZ_STANDARD_A8;
    case CAIRO_FORMAT_A1:
	return GLITZ_STANDARD_A1;
    }
}

static cairo_surface_t *
_cairo_glitz_surface_create_similar (void	    *abstract_src,
				     cairo_format_t format,
				     int	    draw,
				     int	    width,
				     int	    height)
{
    cairo_glitz_surface_t *src = abstract_src;
    cairo_surface_t	  *crsurface;
    glitz_drawable_t	  *drawable;
    glitz_surface_t	  *surface;
    glitz_format_t	  *gformat;

    drawable = glitz_surface_get_drawable (src->surface);
    
    gformat = glitz_find_standard_format (drawable, _glitz_format (format));
    if (!gformat)
	return NULL;
    
    surface = glitz_surface_create (drawable, gformat, width, height, 0, NULL);
    if (!surface)
	return NULL;

    crsurface = cairo_glitz_surface_create (surface);
    
    glitz_surface_destroy (surface);

    return crsurface;
}

static cairo_glitz_surface_t *
_cairo_glitz_surface_clone_similar (cairo_glitz_surface_t *templ,
				    cairo_surface_t	  *src,
				    cairo_format_t	  format)
{
    cairo_glitz_surface_t *clone;
    cairo_image_surface_t *src_image;

    src_image = _cairo_surface_get_image (src);

    clone = (cairo_glitz_surface_t *)
        _cairo_glitz_surface_create_similar (templ, format, 0,
					     src_image->width,
					     src_image->height);
    if (clone == NULL)
	return NULL;
    
    _cairo_glitz_surface_set_filter (clone, cairo_surface_get_filter (src));
    
    _cairo_glitz_surface_set_image (clone, src_image);
    
    _cairo_glitz_surface_set_matrix (clone, &(src_image->base.matrix));
    
    cairo_surface_destroy (&src_image->base);

    return clone;
}

static cairo_int_status_t
_cairo_glitz_composite (cairo_operator_t	op,
			cairo_surface_t		*generic_src,
			cairo_surface_t		*generic_mask,
			cairo_glitz_surface_t	*dst,
			int			src_x,
			int			src_y,
			int			mask_x,
			int			mask_y,
			int			dst_x,
			int			dst_y,
			int			width,
			int		        height,
			glitz_buffer_t		*geometry,
			glitz_geometry_format_t *format,
			int			count)
{
    cairo_glitz_surface_t *src_clone = NULL;
    cairo_glitz_surface_t *mask_clone = NULL;
    cairo_glitz_surface_t *src = (cairo_glitz_surface_t *) generic_src;
    cairo_glitz_surface_t *mask = (cairo_glitz_surface_t *) generic_mask;
	
    if (_glitz_ensure_target (dst->surface))
	return CAIRO_INT_STATUS_UNSUPPORTED;
	
    if (glitz_surface_get_status (dst->surface))
	return CAIRO_STATUS_NO_TARGET_SURFACE;

    if (generic_src->backend != dst->base.backend) {
	src_clone = _cairo_glitz_surface_clone_similar (dst, generic_src,
							CAIRO_FORMAT_ARGB32);
	if (!src_clone)
	    return CAIRO_INT_STATUS_UNSUPPORTED;
	
	src = src_clone;
    }

    if (generic_mask)
    {
	if ((generic_mask->backend != dst->base.backend)) {
	    mask_clone = _cairo_glitz_surface_clone_similar (dst, generic_mask,
							     CAIRO_FORMAT_A8);
	    if (!mask_clone)
		return CAIRO_INT_STATUS_UNSUPPORTED;
	    
	    mask = mask_clone;
	}
    }

    if (geometry)
    {
	glitz_set_geometry (dst->surface, GLITZ_GEOMETRY_TYPE_VERTEX,
			    format, geometry);
	glitz_set_array (dst->surface, 0, 2, count, 0, 0);
    } else
	glitz_set_geometry (dst->surface, GLITZ_GEOMETRY_TYPE_NONE,
			    NULL, NULL);

    glitz_composite (_glitz_operator (op),
		     src->surface,
		     (mask) ? mask->surface : NULL,
		     dst->surface,
		     src_x, src_y,
		     mask_x, mask_y,
		     dst_x, dst_y,
		     width, height);

    if (geometry)
	glitz_set_geometry (dst->surface, GLITZ_GEOMETRY_TYPE_NONE,
			    NULL, NULL);

    if (src_clone)
	cairo_surface_destroy (&src_clone->base);
    if (mask_clone)
	cairo_surface_destroy (&mask_clone->base);

    if (glitz_surface_get_status (dst->surface) == GLITZ_STATUS_NOT_SUPPORTED)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_surface_t *
_cairo_glitz_pattern_create_source_surface (cairo_glitz_surface_t *dst,
					    cairo_pattern_t	  *pattern,
					    int			  x,
					    int			  y,
					    unsigned int	  width,
					    unsigned int	  height)
{
    cairo_glitz_surface_t *src;
    
    switch (pattern->type) {
    case CAIRO_PATTERN_SOLID:
	src = (cairo_glitz_surface_t *)
	    _cairo_surface_create_similar_solid (&dst->base,
						 CAIRO_FORMAT_ARGB32, 1, 1,
						 &pattern->color);
	if (!src)
	    return NULL;

	cairo_surface_set_repeat (&src->base, 1);
	
	return &src->base;
    case CAIRO_PATTERN_RADIAL:
	/* glitz doesn't support inner and outer circle with different
	   center points. */
	if (pattern->u.radial.center0.x != pattern->u.radial.center1.x ||
	    pattern->u.radial.center0.y != pattern->u.radial.center1.y)
	    return NULL;
	/* fall-through */
    case CAIRO_PATTERN_LINEAR: {
	glitz_drawable_t *drawable;
	glitz_fixed16_16_t *params;
	int i, n_params;

	drawable = glitz_surface_get_drawable (dst->surface);
	if (!(glitz_drawable_get_features (drawable) &
	      GLITZ_FEATURE_FRAGMENT_PROGRAM_MASK))
	    break;

	if (pattern->filter != CAIRO_FILTER_BILINEAR)
	    break;
	
	n_params = pattern->n_stops * 3 + 4;

	params = malloc (sizeof (glitz_fixed16_16_t) * n_params);
	if (!params)
	    return NULL;

	src = (cairo_glitz_surface_t *)
	    _cairo_surface_create_similar_scratch (&dst->base,
						   CAIRO_FORMAT_ARGB32, 0,
						   pattern->n_stops, 1);
	if (!src)
	{
	    free (params);
	    return NULL;
	}

	for (i = 0; i < pattern->n_stops; i++) {
	    glitz_color_t color;

	    color.alpha = pattern->stops[i].color_char[3];
	    color.red = pattern->stops[i].color_char[0] * color.alpha;
	    color.green = pattern->stops[i].color_char[1] * color.alpha;
	    color.blue = pattern->stops[i].color_char[2] * color.alpha;
	    color.alpha *= 256;
	
	    glitz_set_rectangle (src->surface, &color, i, 0, 1, 1);

	    params[4 + 3 * i] = pattern->stops[i].offset;
	    params[5 + 3 * i] = i << 16;
	    params[6 + 3 * i] = 0;
	}

	if (pattern->type == CAIRO_PATTERN_LINEAR) {
	    params[0] = _cairo_fixed_from_double (pattern->u.linear.point0.x);
	    params[1] = _cairo_fixed_from_double (pattern->u.linear.point0.y);
	    params[2] = _cairo_fixed_from_double (pattern->u.linear.point1.x);
	    params[3] = _cairo_fixed_from_double (pattern->u.linear.point1.y);

	    glitz_surface_set_filter (src->surface,
				      GLITZ_FILTER_LINEAR_GRADIENT,
				      params, n_params);	    
	} else {
	    params[0] = _cairo_fixed_from_double (pattern->u.radial.center0.x);
	    params[1] = _cairo_fixed_from_double (pattern->u.radial.center0.y);
	    params[2] = _cairo_fixed_from_double (pattern->u.radial.radius0);
	    params[3] = _cairo_fixed_from_double (pattern->u.radial.radius1);
	    
	    glitz_surface_set_filter (src->surface,
				      GLITZ_FILTER_RADIAL_GRADIENT,
				      params, n_params);
	}

	switch (pattern->extend) {
	case CAIRO_EXTEND_REPEAT:
	    glitz_surface_set_fill (src->surface, GLITZ_FILL_REPEAT);
	    break;
	case CAIRO_EXTEND_REFLECT:
	    glitz_surface_set_fill (src->surface, GLITZ_FILL_REFLECT);
	    break;
	case CAIRO_EXTEND_NONE:
	default:
	    glitz_surface_set_fill (src->surface, GLITZ_FILL_NEAREST);
	    break;
	}

	free (params);
	
	cairo_surface_set_matrix (&src->base, &pattern->matrix);
	return &src->base;
    } break;
    case CAIRO_PATTERN_SURFACE:
	cairo_surface_reference (pattern->u.surface.surface);
	return pattern->u.surface.surface;
    }
    
    return NULL;
}

static cairo_surface_t *
_cairo_glitz_pattern_create_mask_surface (cairo_glitz_surface_t *dst,
					  cairo_pattern_t	*pattern,
					  int			x,
					  int			y,
					  unsigned int		width,
					  unsigned int		height)
{
    cairo_glitz_surface_t *mask;
    
    switch (pattern->type) {
    case CAIRO_PATTERN_SOLID:
	break;
    case CAIRO_PATTERN_RADIAL:
    case CAIRO_PATTERN_LINEAR:
    case CAIRO_PATTERN_SURFACE:
	if (pattern->color.alpha != 1.0)
	{
	    mask = (cairo_glitz_surface_t *)
		_cairo_surface_create_similar_solid (&dst->base,
						     CAIRO_FORMAT_A8, 1, 1,
						     &pattern->color);
	    if (!mask)
		return NULL;
	    
	    glitz_surface_set_fill (mask->surface, GLITZ_FILL_REPEAT);
	    return &mask->base;
	}
	break;
    }

    return NULL;
}

static cairo_int_status_t
_cairo_glitz_surface_composite (cairo_operator_t op,
				cairo_pattern_t  *pattern,
				cairo_surface_t  *generic_mask,
				void		 *abstract_dst,
				int		 src_x,
				int		 src_y,
				int		 mask_x,
				int		 mask_y,
				int		 dst_x,
				int		 dst_y,
				unsigned int	 width,
				unsigned int	 height)
{
    cairo_glitz_surface_t *dst = abstract_dst;
    cairo_surface_t	  *generic_src;
    cairo_int_status_t    status;

    if (op == CAIRO_OPERATOR_SATURATE)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    generic_src =
	_cairo_glitz_pattern_create_source_surface (dst, pattern,
						    src_x, src_y,
						    width, height);
    if (!generic_src)
    {
	int x_offset, y_offset;
	
	generic_src = (cairo_surface_t *)
	    _cairo_pattern_get_surface (pattern, &dst->base,
					src_x, src_y, width, height,
					&x_offset, &y_offset);
	if (!generic_src)
	    return CAIRO_STATUS_NO_MEMORY;

	src_x -= x_offset;
	src_y -= y_offset;
    }
    
    if (generic_mask)
	cairo_surface_reference (generic_mask);
    else
	generic_mask =
	    _cairo_glitz_pattern_create_mask_surface (dst, pattern,
						      0, 0, width, height);

    _cairo_pattern_prepare_surface (pattern, generic_src);
    
    status = _cairo_glitz_composite (_glitz_operator (op),
				     generic_src,
				     generic_mask,
				     dst,
				     src_x, src_y,
				     mask_x, mask_y,
				     dst_x, dst_y,
				     width, height,
				     NULL, NULL, 0);

    _cairo_pattern_restore_surface (pattern, generic_src);

    cairo_surface_destroy (generic_src);
    if (generic_mask)
	cairo_surface_destroy (generic_mask);

    return status;
}

static cairo_int_status_t
_cairo_glitz_surface_fill_rectangles (void		  *abstract_dst,
				      cairo_operator_t	  op,
				      const cairo_color_t *color,
				      cairo_rectangle_t	  *rects,
				      int		  n_rects)
{
    cairo_glitz_surface_t *dst = abstract_dst;

    if (op == CAIRO_OPERATOR_SRC)
    {
	glitz_color_t glitz_color;

	glitz_color.red = color->red_short;
	glitz_color.green = color->green_short;
	glitz_color.blue = color->blue_short;
	glitz_color.alpha = color->alpha_short;
	    
	if (glitz_surface_get_width (dst->surface) != 1 ||
	    glitz_surface_get_height (dst->surface) != 1)
	    _glitz_ensure_target (dst->surface);
	
	glitz_set_rectangles (dst->surface, &glitz_color,
			      (glitz_rectangle_t *) rects, n_rects);
    }
    else
    {
	cairo_surface_t	   *generic_src;
	cairo_int_status_t status;
	
	if (op == CAIRO_OPERATOR_SATURATE)
	    return CAIRO_INT_STATUS_UNSUPPORTED;

	generic_src =
	    _cairo_surface_create_similar_solid (&dst->base,
						 CAIRO_FORMAT_ARGB32, 1, 1,
						 (cairo_color_t *) color);
	if (!generic_src)
	    return CAIRO_STATUS_NO_MEMORY;
	
	while (n_rects--)
	{
	    status = _cairo_glitz_composite (op,
					     generic_src,
					     NULL,
					     abstract_dst,
					     0, 0,
					     0, 0,
					     rects->x, rects->y,
					     rects->width, rects->height,
					     NULL, NULL, 0);
	    if (status)
	    {
		cairo_surface_destroy (generic_src);
		return status;
	    }
	    
	    rects++;
	}
	
	cairo_surface_destroy (generic_src);
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_glitz_surface_composite_trapezoids (cairo_operator_t  op,
					   cairo_pattern_t   *pattern,
					   void		     *abstract_dst,
					   int		     src_x,
					   int		     src_y,
					   int		     dst_x,
					   int		     dst_y,
					   unsigned int	     width,
					   unsigned int	     height,
					   cairo_trapezoid_t *traps,
					   int		     n_traps)
{
    cairo_glitz_surface_t   *dst = abstract_dst;
    cairo_surface_t	    *generic_src;
    cairo_surface_t	    *generic_mask;
    glitz_float_t	    *vertices;
    glitz_buffer_t	    *buffer;
    glitz_geometry_format_t gf;
    cairo_int_status_t	    status;
    int			    count;
    void		    *data;

    if (op == CAIRO_OPERATOR_SATURATE)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    gf.vertex.primitive = GLITZ_PRIMITIVE_QUADS;
    gf.vertex.type = GLITZ_DATA_TYPE_FLOAT;
    gf.vertex.bytes_per_vertex = 2 * sizeof (glitz_float_t);
    gf.vertex.attributes = 0;

    count = n_traps * 4;

    data = malloc (n_traps * 8 * sizeof (glitz_float_t));
    if (!data)
	return CAIRO_STATUS_NO_MEMORY;

    buffer = glitz_buffer_create_for_data (data);
    if (buffer == NULL) {
	free (data);
	return CAIRO_STATUS_NO_MEMORY;
    }
	
    vertices = glitz_buffer_map (buffer, GLITZ_BUFFER_ACCESS_WRITE_ONLY);
    for (; n_traps; traps++, n_traps--) {
	glitz_float_t top, bottom;

	top = GLITZ_FIXED_TO_FLOAT (traps->top);
	bottom = GLITZ_FIXED_TO_FLOAT (traps->bottom);
	
	*vertices++ = GLITZ_FIXED_LINE_X_TO_FLOAT (traps->left, traps->top);
	*vertices++ = top;
	*vertices++ =
	    GLITZ_FIXED_LINE_X_CEIL_TO_FLOAT (traps->right, traps->top);
	*vertices++ = top;
	*vertices++ =
	    GLITZ_FIXED_LINE_X_CEIL_TO_FLOAT (traps->right, traps->bottom);
	*vertices++ = bottom;
	*vertices++ = GLITZ_FIXED_LINE_X_TO_FLOAT (traps->left, traps->bottom);
	*vertices++ = bottom;
    }
    glitz_buffer_unmap (buffer);

    generic_src =
	_cairo_glitz_pattern_create_source_surface (dst, pattern,
						    src_x, src_y,
						    width, height);
    if (!generic_src)
    {
	int x_offset, y_offset;
	
	generic_src = (cairo_surface_t *)
	    _cairo_pattern_get_surface (pattern, &dst->base,
					src_x, src_y, width, height,
					&x_offset, &y_offset);
	if (!generic_src)
	{
	    status = CAIRO_STATUS_NO_MEMORY;
	    goto bail;
	}

	src_x -= x_offset;
	src_y -= y_offset;
    }
    
    generic_mask =
	_cairo_glitz_pattern_create_mask_surface (dst, pattern,
						  0, 0, width, height);

    _cairo_pattern_prepare_surface (pattern, generic_src);
    
    status = _cairo_glitz_composite (op,
				     generic_src,
				     generic_mask,
				     dst,
				     src_x, src_y,
				     0, 0,
				     dst_x, dst_y,
				     width, height,
				     buffer, &gf, count);

    _cairo_pattern_restore_surface (pattern, generic_src);

    cairo_surface_destroy (generic_src);
    if (generic_mask)
	cairo_surface_destroy (generic_mask);

 bail:
    glitz_buffer_destroy (buffer);
    free (data);
    
    return status;
}

static cairo_int_status_t
_cairo_glitz_surface_copy_page (void *abstract_surface)
{
    return CAIRO_INT_STATUS_UNSUPPORTED;
}

static cairo_int_status_t
_cairo_glitz_surface_show_page (void *abstract_surface)
{
    return CAIRO_INT_STATUS_UNSUPPORTED;
}

static cairo_int_status_t
_cairo_glitz_surface_set_clip_region (void		*abstract_surface,
				      pixman_region16_t *region)
{
    return CAIRO_INT_STATUS_UNSUPPORTED;
}

static const cairo_surface_backend_t cairo_glitz_surface_backend = {
    _cairo_glitz_surface_create_similar,
    _cairo_glitz_surface_destroy,
    _cairo_glitz_surface_pixels_per_inch,
    _cairo_glitz_surface_get_image,
    _cairo_glitz_surface_set_image,
    _cairo_glitz_surface_set_matrix,
    _cairo_glitz_surface_set_filter,
    _cairo_glitz_surface_set_repeat,
    _cairo_glitz_surface_composite,
    _cairo_glitz_surface_fill_rectangles,
    _cairo_glitz_surface_composite_trapezoids,
    _cairo_glitz_surface_copy_page,
    _cairo_glitz_surface_show_page,
    _cairo_glitz_surface_set_clip_region,
    NULL /* show_glyphs */
};

cairo_surface_t *
cairo_glitz_surface_create (glitz_surface_t *surface)
{
    cairo_glitz_surface_t *crsurface;

    if (!surface)
	return NULL;

    crsurface = malloc (sizeof (cairo_glitz_surface_t));
    if (crsurface == NULL)
	return NULL;

    _cairo_surface_init (&crsurface->base, &cairo_glitz_surface_backend);

    glitz_surface_reference (surface);
    crsurface->surface = surface;
    crsurface->format = glitz_surface_get_format (surface);
    
    return (cairo_surface_t *) crsurface;
}
