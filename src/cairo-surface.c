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

static const XTransform CAIRO_XTRANSFORM_IDENTITY = {
    {
	{65536,     0,     0},
	{    0, 65536,     0},
	{    0,     0, 65536}
    }
};

#define CAIRO_SURFACE_RENDER_AT_LEAST(surface, major, minor) \
	(((surface)->render_major > major) ? 1		  \
	 : ((surface)->render_major == major) ? ((surface)->render_minor >= minor) : 0)

#define CAIRO_SURFACE_RENDER_HAS_CREATE_PICTURE(surface)		CAIRO_SURFACE_RENDER_AT_LEAST((surface), 0, 0)
#define CAIRO_SURFACE_RENDER_HAS_COMPOSITE(surface)		CAIRO_SURFACE_RENDER_AT_LEAST((surface), 0, 0)

#define CAIRO_SURFACE_RENDER_HAS_FILL_RECTANGLE(surface)		CAIRO_SURFACE_RENDER_AT_LEAST((surface), 0, 1)
#define CAIRO_SURFACE_RENDER_HAS_FILL_RECTANGLES(surface)		CAIRO_SURFACE_RENDER_AT_LEAST((surface), 0, 1)

#define CAIRO_SURFACE_RENDER_HAS_DISJOINT(surface)			CAIRO_SURFACE_RENDER_AT_LEAST((surface), 0, 2)
#define CAIRO_SURFACE_RENDER_HAS_CONJOINT(surface)			CAIRO_SURFACE_RENDER_AT_LEAST((surface), 0, 2)

#define CAIRO_SURFACE_RENDER_HAS_TRAPEZOIDS(surface)		CAIRO_SURFACE_RENDER_AT_LEAST((surface), 0, 4)
#define CAIRO_SURFACE_RENDER_HAS_TRIANGLES(surface)		CAIRO_SURFACE_RENDER_AT_LEAST((surface), 0, 4)
#define CAIRO_SURFACE_RENDER_HAS_TRISTRIP(surface)			CAIRO_SURFACE_RENDER_AT_LEAST((surface), 0, 4)
#define CAIRO_SURFACE_RENDER_HAS_TRIFAN(surface)			CAIRO_SURFACE_RENDER_AT_LEAST((surface), 0, 4)

#define CAIRO_SURFACE_RENDER_HAS_PICTURE_TRANSFORM(surface)	CAIRO_SURFACE_RENDER_AT_LEAST((surface), 0, 6)

static IcFormat *
_create_icformat_for_visual (Visual *visual)
{
    return IcFormatCreateMasks (32, 0,
				visual->red_mask,
				visual->green_mask,
				visual->blue_mask);
}

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

    /* XXX: We should really get this value from somewhere like Xft.dpy */
    surface->ppm = 3780;
    surface->ref_count = 1;
    surface->repeat = 0;

    surface->dpy = dpy;
    surface->image_data = NULL;
    surface->icimage = NULL;

    surface->type = CAIRO_SURFACE_TYPE_DRAWABLE;
    surface->xtransform = CAIRO_XTRANSFORM_IDENTITY;

    surface->gc = 0;
    surface->drawable = drawable;
    surface->owns_pixmap = 0;
    surface->visual = visual;

    if (! XRenderQueryVersion (dpy, &surface->render_major, &surface->render_minor)) {
	surface->render_major = -1;
	surface->render_minor = -1;
    }

    if (visual)
	surface->icformat = _create_icformat_for_visual (visual);
    else
	surface->icformat = _create_icformat_for_format (format);

    /* XXX: I'm currently ignoring the colormap. Is that bad? */
    if (CAIRO_SURFACE_RENDER_HAS_CREATE_PICTURE (surface))
	surface->picture = XRenderCreatePicture (dpy, drawable,
						 visual ?
						 XRenderFindVisualFormat (dpy, visual) :
						 XRenderFindStandardFormat (dpy, format),
						 0, NULL);
    else
	surface->picture = 0;

    surface->ximage = NULL;

    /* XXX: How to get the proper width/height? Force a roundtrip? And
       how can we track the width/height properly? Shall we give up on
       supporting Windows and only allow drawing to pixmaps? */
    surface->width = 0;
    surface->height = 0;

    return surface;
}
slim_hidden_def(cairo_surface_create_for_drawable);

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

    surface = malloc (sizeof (cairo_surface_t));
    if (surface == NULL)
	return NULL;

    surface->icformat = _create_icformat_for_format (format);

    /* Assume a default until the user lets us know otherwise */
    surface->ppm = 3780;
    surface->ref_count = 1;
    surface->repeat = 0;

    surface->dpy = NULL;
    surface->image_data = NULL;

    surface->width = width;
    surface->height = height;

    surface->icimage = IcImageCreateForData ((IcBits *) data,
					     surface->icformat,
					     width, height,
					     cairo_format_bpp (format),
					     stride);
    if (surface->icimage == NULL) {
	free (surface);
	return NULL;
    }

    surface->type = CAIRO_SURFACE_TYPE_ICIMAGE;
    surface->xtransform = CAIRO_XTRANSFORM_IDENTITY;

    surface->gc = 0;
    surface->drawable = 0;
    surface->owns_pixmap = 0;
    surface->visual = NULL;
    surface->render_major = -1;
    surface->render_minor = -1;

    surface->picture = 0;
    surface->ximage = NULL;

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

    /* XXX: There's a pretty lame heuristic here. This assumes that
     * all non-Render X servers do not support depth-32 pixmaps, (and
     * that they do support depths 1, 8, and 24). Obviously, it would
     * be much better to check the depths that are actually
     * supported. */
    if (other->dpy
	&& (CAIRO_SURFACE_RENDER_HAS_COMPOSITE (other)
	    || format != CAIRO_FORMAT_ARGB32)) {
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
	surface->owns_pixmap = 1;
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

    if (surface->picture)
	XRenderFreePicture (surface->dpy, surface->picture);

    if (surface->owns_pixmap)
	XFreePixmap (surface->dpy, surface->drawable);

    if (surface->icformat)
	IcFormatDestroy (surface->icformat);
	
    if (surface->icimage)
	IcImageDestroy (surface->icimage);

    if (surface->image_data)
	free (surface->image_data);
    surface->image_data = NULL;

    surface->dpy = 0;

    free (surface);
}
slim_hidden_def(cairo_surface_destroy);

static void
_cairo_surface_ensure_gc (cairo_surface_t *surface)
{
    if (surface->gc)
	return;

    surface->gc = XCreateGC (surface->dpy, surface->drawable, 0, NULL);
}

static cairo_status_t
_cairo_x11_surface_put_image (cairo_surface_t       *surface,
			      char                   *data,
			      int                    width,
			      int                    height,
			      int                    stride)
{
    if (surface->picture) {
	XImage *image;
	unsigned bitmap_pad;
	
	/* XXX: This is obviously bogus. depth needs to be figured out for real */
	int depth = 32;

	if (depth > 16)
	    bitmap_pad = 32;
	else if (depth > 8)
	    bitmap_pad = 16;
	else
	    bitmap_pad = 8;

	image = XCreateImage(surface->dpy,
			     DefaultVisual(surface->dpy, DefaultScreen(surface->dpy)),
			     depth, ZPixmap, 0,
			     data, width, height,
			     bitmap_pad,
			     stride);
	if (image == NULL)
	    return CAIRO_STATUS_NO_MEMORY;

	_cairo_surface_ensure_gc (surface);
	XPutImage(surface->dpy, surface->drawable, surface->gc,
		  image, 0, 0, 0, 0, width, height);

	/* Foolish XDestroyImage thinks it can free my data, but I won't
	   stand for it. */
	image->data = NULL;
	XDestroyImage(image);
    } else {
	/* XXX: Need to implement the IcImage method of setting a picture. memcpy? */
    }
    
    return CAIRO_STATUS_SUCCESS;
}

void
_cairo_surface_pull_image (cairo_surface_t *surface)
{
    Window root_ignore;
    int x_ignore, y_ignore, bwidth_ignore, depth_ignore;

    if (surface == NULL)
	return;

    if (surface->type == CAIRO_SURFACE_TYPE_ICIMAGE)
	return;

    if (surface->icimage) {
	IcImageDestroy (surface->icimage);
	surface->icimage = NULL;
    }

    XGetGeometry(surface->dpy, 
		 surface->drawable, 
		 &root_ignore, &x_ignore, &y_ignore,
		 &surface->width, &surface->height,
		 &bwidth_ignore, &depth_ignore);

    surface->ximage = XGetImage (surface->dpy,
				 surface->drawable,
				 0, 0,
				 surface->width, surface->height,
				 AllPlanes, ZPixmap);

    surface->icimage = IcImageCreateForData ((IcBits *)(surface->ximage->data),
					     surface->icformat,
					     surface->ximage->width, 
					     surface->ximage->height,
					     surface->ximage->bits_per_pixel, 
					     surface->ximage->bytes_per_line);
     
    IcImageSetRepeat (surface->icimage, surface->repeat);
    /* XXX: Evil cast here... */
    IcImageSetTransform (surface->icimage, (IcTransform *) &(surface->xtransform));
    
    /* XXX: Add support here for pictures with external alpha. */
}

void
_cairo_surface_push_image (cairo_surface_t *surface)
{
    if (surface == NULL)
	return;

    if (surface->type == CAIRO_SURFACE_TYPE_ICIMAGE)
	return;

    if (surface->ximage == NULL)
	return;

    _cairo_surface_ensure_gc (surface);
    XPutImage (surface->dpy,
	       surface->drawable,
	       surface->gc,
	       surface->ximage,
	       0, 0,
	       0, 0,
	       surface->width,
	       surface->height);

    XDestroyImage(surface->ximage);
    surface->ximage = NULL;
}

/* XXX: We may want to move to projective matrices at some point. If
   nothing else, that would eliminate the two different transform data
   structures we have here. */
cairo_status_t
cairo_surface_set_matrix (cairo_surface_t *surface, cairo_matrix_t *matrix)
{
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

    if (surface->picture) {
	if (CAIRO_SURFACE_RENDER_HAS_PICTURE_TRANSFORM (surface))
	    XRenderSetPictureTransform (surface->dpy, surface->picture, xtransform);
	/* XXX: Need support here if using an old RENDER without support
           for SetPictureTransform */
    }

    /* XXX: This cast should only occur with a #define hint from libic that it is OK */
    if (surface->icimage) {
	IcImageSetTransform (surface->icimage, (IcTransform *) xtransform);
    }

    return CAIRO_STATUS_SUCCESS;
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

/* XXX: The Render specification has capitalized versions of these
   strings. However, the current implementation is case-sensitive and
   expects lowercase versions. */
static char *
_render_filter_name (cairo_filter_t filter)
{
    switch (filter) {
    case CAIRO_FILTER_FAST:
	return "fast";
    case CAIRO_FILTER_GOOD:
	return "good";
    case CAIRO_FILTER_BEST:
	return "best";
    case CAIRO_FILTER_NEAREST:
	return "nearest";
    case CAIRO_FILTER_BILINEAR:
	return "bilinear";
    default:
	return "best";
    }
}

cairo_status_t
cairo_surface_set_filter (cairo_surface_t *surface, cairo_filter_t filter)
{
    if (surface->picture) {
	XRenderSetPictureFilter (surface->dpy, surface->picture,
				 _render_filter_name (filter), NULL, 0);
    }

    if (surface->icimage) {
	IcImageSetFilter (surface->icimage, filter);
    }

    return CAIRO_STATUS_SUCCESS;
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

    if (surface->picture) {
	unsigned long mask;
	XRenderPictureAttributes pa;
	
	mask = CPRepeat;
	pa.repeat = repeat;

	XRenderChangePicture (surface->dpy, surface->picture, mask, &pa);
    }

    if (surface->icimage) {
	IcImageSetRepeat (surface->icimage, repeat);
    }

    return CAIRO_STATUS_SUCCESS;
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
    if (dst->type == CAIRO_SURFACE_TYPE_DRAWABLE
	&& CAIRO_SURFACE_RENDER_HAS_COMPOSITE (dst)
	&& (mask == NULL || mask->dpy == dst->dpy)
	&& (src->type == CAIRO_SURFACE_TYPE_ICIMAGE || src->dpy == dst->dpy)) {

	cairo_surface_t *src_on_server = NULL;

	if (src->type == CAIRO_SURFACE_TYPE_ICIMAGE) {
	    cairo_matrix_t matrix;
	    src_on_server = cairo_surface_create_similar (dst, CAIRO_FORMAT_ARGB32,
							  IcImageGetWidth (src->icimage),
							  IcImageGetHeight (src->icimage));
	    if (src_on_server == NULL)
		return;

	    cairo_surface_get_matrix (src, &matrix);
	    cairo_surface_set_matrix (src_on_server, &matrix);

	    _cairo_x11_surface_put_image (src_on_server,
					  (char *) IcImageGetData (src->icimage),
					  IcImageGetWidth (src->icimage),
					  IcImageGetHeight (src->icimage),
					  IcImageGetStride (src->icimage));
	}

	XRenderComposite (dst->dpy, operator,
			  src_on_server ? src_on_server->picture : src->picture,
			  mask ? mask->picture : 0,
			  dst->picture,
			  src_x, src_y,
			  mask_x, mask_y,
			  dst_x, dst_y,
			  width, height);
	
	
    } else {
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
    if (num_rects == 0)
	return;

    if (surface->type == CAIRO_SURFACE_TYPE_DRAWABLE
	&& CAIRO_SURFACE_RENDER_HAS_FILL_RECTANGLE (surface)) {

	XRenderColor render_color;
	render_color.red   = color->red_short;
	render_color.green = color->green_short;
	render_color.blue  = color->blue_short;
	render_color.alpha = color->alpha_short;

	/* XXX: This XRectangle cast is evil... it needs to go away somehow. */
	XRenderFillRectangles (surface->dpy, operator, surface->picture,
			       &render_color, (XRectangle *) rects, num_rects);

    } else {
	IcColor ic_color;

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
    if (dst->type == CAIRO_SURFACE_TYPE_DRAWABLE
	&& CAIRO_SURFACE_RENDER_HAS_TRAPEZOIDS (dst)
	&& src->dpy == dst->dpy) {

	/* XXX: The XTrapezoid cast is evil and needs to go away somehow. */
	XRenderCompositeTrapezoids (dst->dpy, operator, src->picture, dst->picture,
				    XRenderFindStandardFormat (dst->dpy, PictStandardA8),
				    xSrc, ySrc, (XTrapezoid *) traps, num_traps);
    } else {
	_cairo_surface_pull_image (src);
	_cairo_surface_pull_image (dst);

	/* XXX: The IcTrapezoid cast is evil and needs to go away somehow. */
	IcCompositeTrapezoids (operator, src->icimage, dst->icimage,
			       xSrc, ySrc, (IcTrapezoid *) traps, num_traps);

	_cairo_surface_push_image (dst);
    }
}

