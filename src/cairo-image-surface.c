/*
 * Copyright © 2003 University of Southern California
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
 * Author: Carl D. Worth <cworth@isi.edu>
 */

#include "cairoint.h"

static const cairo_surface_backend_t cairo_image_surface_backend;

static int
_cairo_format_bpp (cairo_format_t format)
{
    switch (format) {
    case CAIRO_FORMAT_A1:
	return 1;
    case CAIRO_FORMAT_A8:
	return 8;
    case CAIRO_FORMAT_RGB24:
    case CAIRO_FORMAT_ARGB32:
    default:
	return 32;
    }
}

static cairo_image_surface_t *
_cairo_image_surface_create_for_ic_image (IcImage *ic_image)
{
    cairo_image_surface_t *surface;

    surface = malloc (sizeof (cairo_image_surface_t));
    if (surface == NULL)
	return NULL;

    _cairo_surface_init (&surface->base, &cairo_image_surface_backend);

    surface->ic_image = ic_image;

    surface->data = (char *) IcImageGetData (ic_image);
    surface->owns_data = 0;

    surface->width = IcImageGetWidth (ic_image);
    surface->height = IcImageGetHeight (ic_image);
    surface->stride = IcImageGetStride (ic_image);
    surface->depth = IcImageGetDepth (ic_image);

    return surface;
}

cairo_image_surface_t *
_cairo_image_surface_create_with_masks (char			*data,
					cairo_format_masks_t	*format,
					int			width,
					int			height,
					int			stride)
{
    cairo_image_surface_t *surface;
    IcFormat *ic_format;
    IcImage *ic_image;

    ic_format = IcFormatCreateMasks (format->bpp,
				     format->alpha_mask,
				     format->red_mask,
				     format->green_mask,
				     format->blue_mask);

    if (ic_format == NULL)
	return NULL;

    ic_image = IcImageCreateForData ((IcBits *) data, ic_format,
				     width, height, format->bpp, stride);

    IcFormatDestroy (ic_format);

    if (ic_image == NULL)
	return NULL;

    surface = _cairo_image_surface_create_for_ic_image (ic_image);

    return surface;
}

static IcFormat *
_create_ic_format (cairo_format_t format)
{
    switch (format) {
    case CAIRO_FORMAT_A1:
	return IcFormatCreate (IcFormatNameA1);
	break;
    case CAIRO_FORMAT_A8:
	return IcFormatCreate (IcFormatNameA8);
	break;
    case CAIRO_FORMAT_RGB24:
	return IcFormatCreate (IcFormatNameRGB24);
	break;
    case CAIRO_FORMAT_ARGB32:
    default:
	return IcFormatCreate (IcFormatNameARGB32);
	break;
    }
}

cairo_surface_t *
cairo_image_surface_create (cairo_format_t	format,
			    int			width,
			    int			height)
{
    cairo_image_surface_t *surface;
    IcFormat *ic_format;
    IcImage *ic_image;

    ic_format = _create_ic_format (format);
    if (ic_format == NULL)
	return NULL;

    ic_image = IcImageCreate (ic_format, width, height);

    IcFormatDestroy (ic_format);

    if (ic_image == NULL)
	return NULL;

    surface = _cairo_image_surface_create_for_ic_image (ic_image);

    return &surface->base;
}

cairo_surface_t *
cairo_image_surface_create_for_data (char		*data,
				     cairo_format_t	format,
				     int		width,
				     int		height,
				     int		stride)
{
    cairo_image_surface_t *surface;
    IcFormat *ic_format;
    IcImage *ic_image;

    ic_format = _create_ic_format (format);
    if (ic_format == NULL)
	return NULL;

    ic_image = IcImageCreateForData ((IcBits *) data, ic_format,
				     width, height,
				     _cairo_format_bpp (format),
				     stride);

    IcFormatDestroy (ic_format);

    if (ic_image == NULL)
	return NULL;

    surface = _cairo_image_surface_create_for_ic_image (ic_image);

    return &surface->base;
}

static cairo_surface_t *
_cairo_image_surface_create_similar (void		*abstract_src,
				     cairo_format_t	format,
				     int		width,
				     int		height)
{
    return cairo_image_surface_create (format, width, height);
}

static void
_cairo_image_abstract_surface_destroy (void *abstract_surface)
{
    cairo_image_surface_t *surface = abstract_surface;
    
    if (surface->ic_image)
	IcImageDestroy (surface->ic_image);

    if (surface->owns_data)
	free (surface->data);

    free (surface);
}

void
_cairo_image_surface_assume_ownership_of_data (cairo_image_surface_t *surface)
{
    surface->owns_data = 1;
}

static double
_cairo_image_surface_pixels_per_inch (void *abstract_surface)
{
    /* XXX: We'll want a way to let the user set this. */
    return 96.0;
}

static cairo_image_surface_t *
_cairo_image_surface_get_image (void *abstract_surface)
{
    cairo_image_surface_t *surface = abstract_surface;

    cairo_surface_reference (&surface->base);

    return surface;
}

static cairo_status_t
_cairo_image_surface_set_image (void			*abstract_surface,
				cairo_image_surface_t	*image)
{
    if (image == abstract_surface)
	return CAIRO_STATUS_SUCCESS;

    /* XXX: This case has not yet been implemented. We'll lie for now. */
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_image_abstract_surface_set_matrix (void			*abstract_surface,
					  cairo_matrix_t	*matrix)
{
    cairo_image_surface_t *surface = abstract_surface;
    return _cairo_image_surface_set_matrix (surface, matrix);
}

cairo_status_t
_cairo_image_surface_set_matrix (cairo_image_surface_t	*surface,
				 cairo_matrix_t		*matrix)
{
    IcTransform ic_transform;

    ic_transform.matrix[0][0] = _cairo_fixed_from_double (matrix->m[0][0]);
    ic_transform.matrix[0][1] = _cairo_fixed_from_double (matrix->m[1][0]);
    ic_transform.matrix[0][2] = _cairo_fixed_from_double (matrix->m[2][0]);

    ic_transform.matrix[1][0] = _cairo_fixed_from_double (matrix->m[0][1]);
    ic_transform.matrix[1][1] = _cairo_fixed_from_double (matrix->m[1][1]);
    ic_transform.matrix[1][2] = _cairo_fixed_from_double (matrix->m[2][1]);

    ic_transform.matrix[2][0] = 0;
    ic_transform.matrix[2][1] = 0;
    ic_transform.matrix[2][2] = _cairo_fixed_from_double (1);

    IcImageSetTransform (surface->ic_image, &ic_transform);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_image_abstract_surface_set_filter (void *abstract_surface, cairo_filter_t filter)
{
    cairo_image_surface_t *surface = abstract_surface;

    return _cairo_image_surface_set_filter (surface, filter);
}

cairo_status_t
_cairo_image_surface_set_filter (cairo_image_surface_t *surface, cairo_filter_t filter)
{
    IcFilter ic_filter;

    switch (filter) {
    case CAIRO_FILTER_FAST:
	ic_filter = IcFilterFast;
	break;
    case CAIRO_FILTER_GOOD:
	ic_filter = IcFilterGood;
	break;
    case CAIRO_FILTER_BEST:
	ic_filter = IcFilterBest;
	break;
    case CAIRO_FILTER_NEAREST:
	ic_filter = IcFilterNearest;
	break;
    case CAIRO_FILTER_BILINEAR:
	ic_filter = IcFilterBilinear;
	break;
    default:
	ic_filter = IcFilterBest;
    }

    IcImageSetFilter (surface->ic_image, ic_filter);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_image_abstract_surface_set_repeat (void *abstract_surface, int repeat)
{
    cairo_image_surface_t *surface = abstract_surface;
    return _cairo_image_surface_set_repeat (surface, repeat);
}

cairo_status_t
_cairo_image_surface_set_repeat (cairo_image_surface_t *surface, int repeat)
{
    IcImageSetRepeat (surface->ic_image, repeat);

    return CAIRO_STATUS_SUCCESS;
}

static IcOperator
_ic_operator (cairo_operator_t operator)
{
    switch (operator) {
    case CAIRO_OPERATOR_CLEAR:
	return IcOperatorClear;
    case CAIRO_OPERATOR_SRC:
	return IcOperatorSrc;
    case CAIRO_OPERATOR_DST:
	return IcOperatorDst;
    case CAIRO_OPERATOR_OVER:
	return IcOperatorOver;
    case CAIRO_OPERATOR_OVER_REVERSE:
	return IcOperatorOverReverse;
    case CAIRO_OPERATOR_IN:
	return IcOperatorIn;
    case CAIRO_OPERATOR_IN_REVERSE:
	return IcOperatorInReverse;
    case CAIRO_OPERATOR_OUT:
	return IcOperatorOut;
    case CAIRO_OPERATOR_OUT_REVERSE:
	return IcOperatorOutReverse;
    case CAIRO_OPERATOR_ATOP:
	return IcOperatorAtop;
    case CAIRO_OPERATOR_ATOP_REVERSE:
	return IcOperatorAtopReverse;
    case CAIRO_OPERATOR_XOR:
	return IcOperatorXor;
    case CAIRO_OPERATOR_ADD:
	return IcOperatorAdd;
    case CAIRO_OPERATOR_SATURATE:
	return IcOperatorSaturate;
    default:
	return IcOperatorOver;
    }
}

static cairo_int_status_t
_cairo_image_surface_composite (cairo_operator_t	operator,
				cairo_surface_t		*generic_src,
				cairo_surface_t		*generic_mask,
				void			*abstract_dst,
				int			src_x,
				int			src_y,
				int			mask_x,
				int			mask_y,
				int			dst_x,
				int			dst_y,
				unsigned int		width,
				unsigned int		height)
{
    cairo_image_surface_t *dst = abstract_dst;
    cairo_image_surface_t *src = (cairo_image_surface_t *) generic_src;
    cairo_image_surface_t *mask = (cairo_image_surface_t *) generic_mask;

    if (generic_src->backend != dst->base.backend ||
	(generic_mask && (generic_mask->backend != dst->base.backend)))
    {
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    IcComposite (_ic_operator (operator),
		 src->ic_image,
		 mask ? mask->ic_image : NULL,
		 dst->ic_image,
		 src_x, src_y,
		 mask_x, mask_y,
		 dst_x, dst_y,
		 width, height);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_image_surface_fill_rectangles (void			*abstract_surface,
				      cairo_operator_t		operator,
				      const cairo_color_t	*color,
				      cairo_rectangle_t		*rects,
				      int			num_rects)
{
    cairo_image_surface_t *surface = abstract_surface;

    IcColor ic_color;

    ic_color.red   = color->red_short;
    ic_color.green = color->green_short;
    ic_color.blue  = color->blue_short;
    ic_color.alpha = color->alpha_short;

    /* XXX: The IcRectangle cast is evil... it needs to go away somehow. */
    IcFillRectangles (_ic_operator(operator), surface->ic_image,
		      &ic_color, (IcRectangle *) rects, num_rects);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_image_surface_composite_trapezoids (cairo_operator_t	operator,
					   cairo_surface_t	*generic_src,
					   void			*abstract_dst,
					   int			x_src,
					   int			y_src,
					   cairo_trapezoid_t	*traps,
					   int			num_traps)
{
    cairo_image_surface_t *dst = abstract_dst;
    cairo_image_surface_t *src = (cairo_image_surface_t *) generic_src;

    if (generic_src->backend != dst->base.backend)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    /* XXX: The IcTrapezoid cast is evil and needs to go away somehow. */
    IcCompositeTrapezoids (operator, src->ic_image, dst->ic_image,
			   x_src, y_src, (IcTrapezoid *) traps, num_traps);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_image_surface_show_page (void *abstract_surface)
{
    return CAIRO_INT_STATUS_UNSUPPORTED;
}

static const cairo_surface_backend_t cairo_image_surface_backend = {
    _cairo_image_surface_create_similar,
    _cairo_image_abstract_surface_destroy,
    _cairo_image_surface_pixels_per_inch,
    _cairo_image_surface_get_image,
    _cairo_image_surface_set_image,
    _cairo_image_abstract_surface_set_matrix,
    _cairo_image_abstract_surface_set_filter,
    _cairo_image_abstract_surface_set_repeat,
    _cairo_image_surface_composite,
    _cairo_image_surface_fill_rectangles,
    _cairo_image_surface_composite_trapezoids,
    _cairo_image_surface_show_page
};
