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

    surface->dpy = dpy;
    surface->image_data = NULL;
    surface->icimage = NULL;

    surface->type = CAIRO_SURFACE_TYPE_DRAWABLE;
    surface->xtransform = CAIRO_XTRANSFORM_IDENTITY;

    surface->gc = 0;
    surface->drawable = drawable;
    surface->visual = visual;

    if (! XRenderQueryVersion (dpy, &surface->render_major, &surface->render_minor)) {
	surface->render_major = -1;
	surface->render_minor = -1;
    }

    /* XXX: I'm currently ignoring the colormap. Is that bad? */
    if (CAIRO_SURFACE_RENDER_HAS_CREATE_PICTURE (surface))
	surface->picture = XRenderCreatePicture (dpy, drawable,
						 visual
						 ? XRenderFindVisualFormat (dpy, visual)
						 : XRenderFindStandardFormat (dpy, format),
						 0, NULL);

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

    /* Assume a default until the user lets us know otherwise */
    surface->ppm = 3780;
    surface->ref_count = 1;

    surface->dpy = NULL;
    surface->image_data = NULL;

    image = IcImageCreateForData ((IcBits *) data, &icformat, width, height, cairo_format_bpp (format), stride);
    if (image == NULL) {
	free (surface);
	return NULL;
    }

    surface->type = CAIRO_SURFACE_TYPE_ICIMAGE;
    surface->xtransform = CAIRO_XTRANSFORM_IDENTITY;

    surface->gc = 0;
    surface->drawable = 0;
    surface->visual = NULL;
    surface->render_major = -1;
    surface->render_minor = -1;

    surface->picture = 0;

    surface->icimage = image;

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

    if (surface->picture)
	XRenderFreePicture (surface->dpy, surface->picture);
	
    if (surface->icimage)
	IcImageDestroy (surface->icimage);

    if (surface->image_data)
	free (surface->image_data);
    surface->image_data = NULL;

    surface->dpy = 0;

    free (surface);
}

static void
_cairo_surface_ensure_gc (cairo_surface_t *surface)
{
    if (surface->gc)
	return;

    surface->gc = XCreateGC (surface->dpy, surface->drawable, 0, NULL);
}

cairo_status_t
cairo_surface_put_image (cairo_surface_t	*surface,
			 char			*data,
			 int			width,
			 int			height,
			 int			stride)
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

/* XXX: Symmetry demands an cairo_surface_get_image as well. */

void
_cairo_surface_pull_image (cairo_surface_t *surface)
{
/* XXX: NYI (Also needs support for pictures with external alpha.)
    if (surface->type == CAIRO_SURFACE_TYPE_ICIMAGE)
	return;

    if (surface->icimage) {
	IcImageDestroy (surface->icimage);
	surface->icimage = NULL;
    }

    _cairo_surface_ensure_GC (surface);
    surface->ximage = XGetImage (surface->dpy,
				 surface->drawable,
				 surface->gc,
				 0, 0,
				 width, height,
				 AllPlanes, ZPixmap);
    
    surface->icimage = IcImageCreateForData (image->data,
					     IcFormat *format,
					     int width, int height,
					     int bpp, int stride);
*/
}

void
_cairo_surface_push_image (cairo_surface_t *surface)
{
/* XXX: NYI
    if (surface->type == CAIRO_SURFACE_TYPE_ICIMAGE)
	return;

    if (surface->ximage == NULL)
	return;

    _cairo_surface_ensure_GC (surface);
    XPutImage (surface->dpy,
	       surface->drawable,
	       surface->gc,
	       surface->ximage,
	       0, 0,
	       0, 0,
	       width, height);

    * Foolish XDestroyImage thinks it can free my data, but I won't
       stand for it. *
    surface->ximage->data = NULL;
    XDestroyImage(surface->ximage);
    surface->ximage = NULL;
*/
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

cairo_status_t
cairo_surface_get_matrix (cairo_surface_t *surface, cairo_matrix_t *matrix)
{
    XTransform *xtransform = &surface->xtransform;

    matrix->m[0][0] = XFixedToDouble (xtransform->matrix[0][0]);
    matrix->m[1][0] = XFixedToDouble (xtransform->matrix[0][1]);
    matrix->m[2][0] = XFixedToDouble (xtransform->matrix[0][2]);

    matrix->m[0][1] = XFixedToDouble (xtransform->matrix[1][0]);
    matrix->m[1][1] = XFixedToDouble (xtransform->matrix[1][1]);
    matrix->m[2][1] = XFixedToDouble (xtransform->matrix[1][2]);

    return CAIRO_STATUS_SUCCESS;
}

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
	&& src->dpy == dst->dpy
	&& (mask == NULL || mask->dpy == dst->dpy)) {

	XRenderComposite (dst->dpy, operator,
			  src->picture,
			  mask ? mask->picture : 0,
			  dst->picture,
			  src_x, src_y,
			  mask_x, mask_y,
			  dst_x, dst_y,
			  width, height);
    } else {
	_cairo_surface_pull_image (src);
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
				     const cairo_trapezoid_t	*traps,
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

