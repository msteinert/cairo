/*
 * Copyright © 2002 USC, Information Sciences Institute
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

cairo_surface_t *
cairo_surface_create_for_drawable (Display		*dpy,
				   Drawable		drawable,
				   Visual		*visual,
				   cairo_format_t	format,
				   Colormap		colormap)
{
    cairo_surface_t *surface;

    surface = malloc (sizeof (cairo_surface_t));
    if (surface == NULL)
	return NULL;

    surface->dpy = dpy;
    surface->image_data = NULL;

    surface->xc_surface = XcSurfaceCreateForDrawable (dpy, drawable, visual, format, colormap);
    if (surface->xc_surface == NULL) {
	free (surface);
	return NULL;
    }

    /* XXX: We should really get this value from somewhere like Xft.dpy */
    surface->ppm = 3780;

    surface->ref_count = 1;

    return surface;
}

/* XXX: These definitions are 100% bogus. The problem that needs to be
   fixed is that Ic needs to export a real API for passing in
   formats. */
#define PICT_FORMAT(bpp,type,a,r,g,b)	(((bpp) << 24) |  \
					 ((type) << 16) | \
					 ((a) << 12) | \
					 ((r) << 8) | \
					 ((g) << 4) | \
					 ((b)))

/*
 * gray/color formats use a visual index instead of argb
 */
#define PICT_VISFORMAT(bpp,type,vi)	(((bpp) << 24) |  \
					 ((type) << 16) | \
					 ((vi)))

#define PICT_TYPE_A	1
#define PICT_TYPE_ARGB	2

#define PICT_FORMAT_COLOR(f)	(PICT_FORMAT_TYPE(f) & 2)

/* 32bpp formats */

#define PICT_a8r8g8b8	PICT_FORMAT(32,PICT_TYPE_ARGB,8,8,8,8)
#define PICT_x8r8g8b8	PICT_FORMAT(32,PICT_TYPE_ARGB,0,8,8,8)
#define PICT_a8		PICT_FORMAT(8,PICT_TYPE_A,8,0,0,0)
#define PICT_a1		PICT_FORMAT(1,PICT_TYPE_A,1,0,0,0)

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

cairo_surface_t *
cairo_surface_create_for_image (char		*data,
				cairo_format_t	format,
				int		width,
				int		height,
				int		stride)
{
    cairo_surface_t *surface;
    IcFormat icformat;
    IcImage *image;
    int bpp;

    /* XXX: This all needs to change, (but IcFormatInit interface needs to change first) */
    switch (format) {
    case CAIRO_FORMAT_ARGB32:
	IcFormatInit (&icformat, PICT_a8r8g8b8);
	bpp = 32;
	break;
    case CAIRO_FORMAT_RGB24:
	IcFormatInit (&icformat, PICT_x8r8g8b8);
	bpp = 32;
	break;
    case CAIRO_FORMAT_A8:
	IcFormatInit (&icformat, PICT_a8);
	bpp = 8;
	break;
    case CAIRO_FORMAT_A1:
	IcFormatInit (&icformat, PICT_a1);
	bpp = 1;
	break;
    default:
	return NULL;
    }

    surface = malloc (sizeof (cairo_surface_t));
    if (surface == NULL)
	return NULL;

    surface->dpy = NULL;
    surface->image_data = NULL;
    image = IcImageCreateForData ((IcBits *) data, &icformat, width, height, cairo_format_bpp (format), stride);
    if (image == NULL) {
	free (surface);
	return NULL;
    }

    surface->xc_surface = XcSurfaceCreateForIcImage (image);
    if (surface->xc_surface == NULL) {
	IcImageDestroy (image);
	free (surface);
	return NULL;
    }

    /* Assume a default until the user lets us know otherwise */
    surface->ppm = 3780;

    surface->ref_count = 1;

    return surface;
}

cairo_surface_t *
cairo_surface_create_similar (cairo_surface_t	*other,
			      cairo_format_t	format,
			      int		width,
			      int		height)
{
    return cairo_surface_create_similar_solid (other, format, width, height, 0, 0, 0, 0);
}

static int
_CAIRO_FORMAT_DEPTH (cairo_format_t format)
{
    switch (format) {
    case CAIRO_FORMAT_A1:
	return 1;
    case CAIRO_FORMAT_A8:
	return 8;
    case CAIRO_FORMAT_RGB24:
	return 24;
    case CAIRO_FORMAT_ARGB32:
    default:
	return 32;
    }
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

    /* XXX: create_similar should perhaps move down to Xc, (then we
       could drop xrsurface->dpy as well) */
    if (other->dpy) {
	Display *dpy = other->dpy;
	int scr = DefaultScreen (dpy);

	Pixmap pix = XCreatePixmap (dpy,
				    DefaultRootWindow (dpy),
				    width, height,
				    _CAIRO_FORMAT_DEPTH (format));

	surface = cairo_surface_create_for_drawable (dpy, pix,
						     NULL,
						     format,
						     DefaultColormap (dpy, scr));
/* XXX: huh? This should be fine since we already created a picture
	from the pixmap, right?? (Somehow, it seems to be causing some
	breakage).
	XFreePixmap (surface->dpy, pix);
*/
    } else {
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

void
_cairo_surface_reference (cairo_surface_t *surface)
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

    surface->dpy = 0;

    XcSurfaceDestroy (surface->xc_surface);
    surface->xc_surface = NULL;

    if (surface->image_data)
	free (surface->image_data);
    surface->image_data = NULL;

    free (surface);
}

cairo_status_t
cairo_surface_put_image (cairo_surface_t	*surface,
		   char		*data,
		   int		width,
		   int		height,
		   int		stride)
{
    XcSurfacePutImage (surface->xc_surface, data,
		       width, height, stride);

    return CAIRO_STATUS_SUCCESS;
}

/* XXX: Symmetry demands an cairo_surface_get_image as well */

/* XXX: We may want to move to projective matrices at some point. If
   nothing else, that would eliminate the two different transform data
   structures we have here. */
cairo_status_t
cairo_surface_set_matrix (cairo_surface_t *surface, cairo_matrix_t *matrix)
{
    XTransform xtransform;

    xtransform.matrix[0][0] = XDoubleToFixed (matrix->m[0][0]);
    xtransform.matrix[0][1] = XDoubleToFixed (matrix->m[1][0]);
    xtransform.matrix[0][2] = XDoubleToFixed (matrix->m[2][0]);

    xtransform.matrix[1][0] = XDoubleToFixed (matrix->m[0][1]);
    xtransform.matrix[1][1] = XDoubleToFixed (matrix->m[1][1]);
    xtransform.matrix[1][2] = XDoubleToFixed (matrix->m[2][1]);

    xtransform.matrix[2][0] = 0;
    xtransform.matrix[2][1] = 0;
    xtransform.matrix[2][2] = XDoubleToFixed (1);

    XcSurfaceSetTransform (surface->xc_surface,
			   &xtransform);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
cairo_surface_get_matrix (cairo_surface_t *surface, cairo_matrix_t *matrix)
{
    XTransform xtransform;

    XcSurfaceGetTransform (surface->xc_surface, &xtransform);

    matrix->m[0][0] = XFixedToDouble (xtransform.matrix[0][0]);
    matrix->m[1][0] = XFixedToDouble (xtransform.matrix[0][1]);
    matrix->m[2][0] = XFixedToDouble (xtransform.matrix[0][2]);

    matrix->m[0][1] = XFixedToDouble (xtransform.matrix[1][0]);
    matrix->m[1][1] = XFixedToDouble (xtransform.matrix[1][1]);
    matrix->m[2][1] = XFixedToDouble (xtransform.matrix[1][2]);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
cairo_surface_set_filter (cairo_surface_t *surface, cairo_filter_t filter)
{
    XcSurfaceSetFilter (surface->xc_surface, filter);
    return CAIRO_STATUS_SUCCESS;
}

/* XXX: The Xc version of this function isn't quite working yet
cairo_status_t
cairo_surface_set_clip_region (cairo_surface_t *surface, Region region)
{
    XcSurfaceSetClipRegion (surface->xc_surface, region);

    return CAIRO_STATUS_SUCCESS;
}
*/

cairo_status_t
cairo_surface_set_repeat (cairo_surface_t *surface, int repeat)
{
    XcSurfaceSetRepeat (surface->xc_surface, repeat);

    return CAIRO_STATUS_SUCCESS;
}

/* XXX: This function is going away, right? */
Picture
_cairo_surface_get_picture (cairo_surface_t *surface)
{
    return XcSurfaceGetPicture (surface->xc_surface);
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
    XcFillRectangle (operator,
		     surface->xc_surface,
		     &color->xc_color,
		     x, y,
		     width, height);
}

