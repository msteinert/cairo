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
 * Author: Carl D. Worth <cworth@isi.edu>
 */

#include <stdlib.h>

#include "cairoint.h"

static const XTransform CAIRO_XTRANSFORM_IDENTITY = {
    {
	{65536,     0,     0},
	{    0, 65536,     0},
	{    0,     0, 65536}
    }
};

static IcFormat *
_create_icformat_for_format (cairo_format_t format)
{
    switch (format) {
    case CAIRO_FORMAT_ARGB32:
	return IcFormatCreate (IcFormatNameARGB32);
	break;
    case CAIRO_FORMAT_RGB24:
	return IcFormatCreate (IcFormatNameRGB24);
	break;
    case CAIRO_FORMAT_A8:
	return IcFormatCreate (IcFormatNameA8);
	break;
    case CAIRO_FORMAT_A1:
	return IcFormatCreate (IcFormatNameA1);
	break;
    default:
	return NULL;
    }
}

void
_cairo_surface_init (cairo_surface_t			*surface,
		     int				width,
		     int				height,
		     cairo_format_t			format,
		     const struct cairo_surface_backend	*backend)
{
    surface->width = width;
    surface->height = height;

    surface->image_data = NULL;

    /* XXX: We should really get this value from somewhere like Xft.dpy */
    /* Assume a default until the user lets us know otherwise */
    surface->ppm = 3780;
    surface->ref_count = 1;
    surface->repeat = 0;

    surface->xtransform = CAIRO_XTRANSFORM_IDENTITY;

    surface->icimage = NULL;
    surface->icformat = _create_icformat_for_format (format);

    surface->backend = backend;
}

static int
cairo_format_bpp (cairo_format_t format)
{
    switch (format) {
    case CAIRO_FORMAT_A1:
	return 1;
	break;
    case CAIRO_FORMAT_A8:
	return 8;
	break;
    case CAIRO_FORMAT_RGB24:
    case CAIRO_FORMAT_ARGB32:
    default:
	return 32;
	break;
    }
}

static const struct cairo_surface_backend cairo_icimage_surface_backend;

cairo_surface_t *
cairo_surface_create_for_image (char		*data,
				cairo_format_t	format,
				int		width,
				int		height,
				int		stride)
{
    cairo_surface_t *surface;

    surface = malloc (sizeof (cairo_surface_t));
    if (surface == NULL)
	return NULL;

    _cairo_surface_init (surface, width, height, format, &cairo_icimage_surface_backend);

    surface->icimage = IcImageCreateForData ((IcBits *) data,
					     surface->icformat,
					     width, height,
					     cairo_format_bpp (format),
					     stride);
    if (surface->icimage == NULL) {
	free (surface);
	return NULL;
    }

    return surface;
}
slim_hidden_def(cairo_surface_create_for_image);

cairo_surface_t *
cairo_surface_create_similar (cairo_surface_t	*other,
			      cairo_format_t	format,
			      int		width,
			      int		height)
{
    return cairo_surface_create_similar_solid (other, format, width, height, 0, 0, 0, 0);
}

cairo_surface_t *
cairo_surface_create_similar_solid (cairo_surface_t	*other,
				    cairo_format_t	format,
				    int			width,
				    int			height,
				    double		red,
				    double		green,
				    double		blue,
				    double		alpha)
{
    cairo_surface_t *surface = NULL;
    cairo_color_t color;

    if (other->backend->create_similar)
	surface = other->backend->create_similar (other, format, width, height);

    if (!surface) {
	char *data;
	int stride;

	stride = ((width * cairo_format_bpp (format)) + 7) >> 3;
	data = malloc (stride * height);
	if (data == NULL)
	    return NULL;

	surface = cairo_surface_create_for_image (data, format,
						  width, height, stride);

	/* lodge data in the surface structure to be freed with the surface */
	surface->image_data = data;
    }

    /* XXX: Initializing the color in this way assumes
       non-pre-multiplied alpha. I'm not sure that that's what I want
       to do or not. */
    _cairo_color_init (&color);
    _cairo_color_set_rgb (&color, red, green, blue);
    _cairo_color_set_alpha (&color, alpha);
    _cairo_surface_fill_rectangle (surface, CAIRO_OPERATOR_SRC, &color, 0, 0, width, height);
    return surface;
}
slim_hidden_def(cairo_surface_create_similar_solid);

void
cairo_surface_reference (cairo_surface_t *surface)
{
    if (surface == NULL)
	return;

    surface->ref_count++;
}

void
cairo_surface_destroy (cairo_surface_t *surface)
{
    if (surface == NULL)
	return;

    surface->ref_count--;
    if (surface->ref_count)
	return;

    if (surface->icformat)
	IcFormatDestroy (surface->icformat);

    if (surface->icimage)
	IcImageDestroy (surface->icimage);

    if (surface->backend->destroy)
	surface->backend->destroy (surface);

    if (surface->image_data)
	free (surface->image_data);
    surface->image_data = NULL;

    free (surface);
}
slim_hidden_def(cairo_surface_destroy);

void
_cairo_surface_pull_image (cairo_surface_t *surface)
{
    if (surface->backend->pull_image)
	surface->backend->pull_image (surface);
}

void
_cairo_surface_push_image (cairo_surface_t *surface)
{
    if (surface->backend->push_image)
	surface->backend->push_image (surface);
}

/* XXX: We may want to move to projective matrices at some point. If
   nothing else, that would eliminate the two different transform data
   structures we have here. */
cairo_status_t
cairo_surface_set_matrix (cairo_surface_t *surface, cairo_matrix_t *matrix)
{
    cairo_status_t ret = CAIRO_STATUS_SUCCESS;
    XTransform *xtransform = &surface->xtransform;

    xtransform->matrix[0][0] = XDoubleToFixed (matrix->m[0][0]);
    xtransform->matrix[0][1] = XDoubleToFixed (matrix->m[1][0]);
    xtransform->matrix[0][2] = XDoubleToFixed (matrix->m[2][0]);

    xtransform->matrix[1][0] = XDoubleToFixed (matrix->m[0][1]);
    xtransform->matrix[1][1] = XDoubleToFixed (matrix->m[1][1]);
    xtransform->matrix[1][2] = XDoubleToFixed (matrix->m[2][1]);

    xtransform->matrix[2][0] = 0;
    xtransform->matrix[2][1] = 0;
    xtransform->matrix[2][2] = XDoubleToFixed (1);

    if (surface->backend->set_matrix)
	ret = surface->backend->set_matrix (surface);

    /* XXX: This cast should only occur with a #define hint from libic that it is OK */
    if (surface->icimage) {
	IcImageSetTransform (surface->icimage, (IcTransform *) xtransform);
    }

    return ret;
}
slim_hidden_def(cairo_surface_set_matrix);

cairo_status_t
cairo_surface_get_matrix (cairo_surface_t *surface, cairo_matrix_t *matrix)
{
    XTransform *xtransform = &surface->xtransform;

    matrix->m[0][0] = _cairo_fixed_to_double (xtransform->matrix[0][0]);
    matrix->m[1][0] = _cairo_fixed_to_double (xtransform->matrix[0][1]);
    matrix->m[2][0] = _cairo_fixed_to_double (xtransform->matrix[0][2]);

    matrix->m[0][1] = _cairo_fixed_to_double (xtransform->matrix[1][0]);
    matrix->m[1][1] = _cairo_fixed_to_double (xtransform->matrix[1][1]);
    matrix->m[2][1] = _cairo_fixed_to_double (xtransform->matrix[1][2]);

    return CAIRO_STATUS_SUCCESS;
}
slim_hidden_def(cairo_surface_get_matrix);

cairo_status_t
cairo_surface_set_filter (cairo_surface_t *surface, cairo_filter_t filter)
{
    if (surface->icimage) {
	IcImageSetFilter (surface->icimage, filter);
    }

    if (!surface->backend->set_filter)
	return CAIRO_STATUS_SUCCESS;

    return surface->backend->set_filter (surface, filter);
}

/* XXX: NYI
cairo_status_t
cairo_surface_clip_rectangle (cairo_surface_t *surface,
			      int x, int y,
			      int width, int height)
{

}
*/

/* XXX: NYI
cairo_status_t
cairo_surface_clip_restore (cairo_surface_t *surface);
*/

cairo_status_t
cairo_surface_set_repeat (cairo_surface_t *surface, int repeat)
{
    surface->repeat = repeat;

    if (surface->icimage) {
	IcImageSetRepeat (surface->icimage, repeat);
    }

    if (!surface->backend->set_repeat)
	return CAIRO_STATUS_SUCCESS;

    return surface->backend->set_repeat (surface, repeat);
}
slim_hidden_def(cairo_surface_set_repeat);

void
_cairo_surface_composite (cairo_operator_t	operator,
			  cairo_surface_t	*src,
			  cairo_surface_t	*mask,
			  cairo_surface_t	*dst,
			  int			src_x,
			  int			src_y,
			  int			mask_x,
			  int			mask_y,
			  int			dst_x,
			  int			dst_y,
			  unsigned int		width,
			  unsigned int		height)
{
    cairo_int_status_t status;

    status = dst->backend->composite (operator,
				      src, mask, dst,
				      src_x, src_y,
				      mask_x, mask_y,
				      dst_x, dst_y,
				      width, height);
    if (status == CAIRO_STATUS_SUCCESS)
	return;

    _cairo_surface_pull_image (src);
    if (mask)
	_cairo_surface_pull_image (mask);
    _cairo_surface_pull_image (dst);

    IcComposite (operator,
		 src->icimage,
		 mask ? mask->icimage : NULL,
		 dst->icimage,
		 src_x, src_y,
		 mask_x, mask_y,
		 dst_x, dst_y,
		 width, height);

    _cairo_surface_push_image (dst);
}

void
_cairo_surface_fill_rectangle (cairo_surface_t	*surface,
			       cairo_operator_t	operator,
			       cairo_color_t	*color,
			       int		x,
			       int		y,
			       int		width,
			       int		height)
{
    cairo_rectangle_t rect;

    rect.x = x;
    rect.y = y;
    rect.width = width;
    rect.height = height;

    _cairo_surface_fill_rectangles (surface, operator, color, &rect, 1);
}

void
_cairo_surface_fill_rectangles (cairo_surface_t		*surface,
				cairo_operator_t	operator,
				const cairo_color_t	*color,
				cairo_rectangle_t	*rects,
				int			num_rects)
{
    cairo_int_status_t status;
    IcColor ic_color;

    if (num_rects == 0)
	return;

    status = surface->backend->fill_rectangles (surface,
						operator,
						color,
						rects, num_rects);
    if (status == CAIRO_STATUS_SUCCESS)
	return;

    ic_color.red   = color->red_short;
    ic_color.green = color->green_short;
    ic_color.blue  = color->blue_short;
    ic_color.alpha = color->alpha_short;

    _cairo_surface_pull_image (surface);

    /* XXX: The IcRectangle cast is evil... it needs to go away somehow. */
    IcFillRectangles (operator, surface->icimage,
		      &ic_color, (IcRectangle *) rects, num_rects);

    _cairo_surface_push_image (surface);
}

void
_cairo_surface_composite_trapezoids (cairo_operator_t		operator,
				     cairo_surface_t		*src,
				     cairo_surface_t		*dst,
				     int			xSrc,
				     int			ySrc,
				     cairo_trapezoid_t		*traps,
				     int			num_traps)
{
    cairo_int_status_t status;

    status = dst->backend->composite_trapezoids (operator,
						 src, dst,
						 xSrc, ySrc,
						 traps, num_traps);
    if (status == CAIRO_STATUS_SUCCESS)
	return;

    _cairo_surface_pull_image (src);
    _cairo_surface_pull_image (dst);

    /* XXX: The IcTrapezoid cast is evil and needs to go away somehow. */
    IcCompositeTrapezoids (operator, src->icimage, dst->icimage,
			   xSrc, ySrc, (IcTrapezoid *) traps, num_traps);

    _cairo_surface_push_image (dst);
}

