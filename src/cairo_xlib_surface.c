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

#include "cairoint.h"

void
cairo_set_target_drawable (cairo_t	*cr,
			   Display	*dpy,
			   Drawable	drawable)
{
    cairo_surface_t *surface;

    if (cr->status && cr->status != CAIRO_STATUS_NO_TARGET_SURFACE)
	    return;

    surface = cairo_xlib_surface_create (dpy, drawable,
					 DefaultVisual (dpy, DefaultScreen (dpy)),
					 0,
					 DefaultColormap (dpy, DefaultScreen (dpy)));
    if (surface == NULL) {
	cr->status = CAIRO_STATUS_NO_MEMORY;
	return;
    }

    cairo_set_target_surface (cr, surface);

    /* cairo_set_target_surface takes a reference, so we must destroy ours */
    cairo_surface_destroy (surface);
}

typedef struct cairo_xlib_surface {
    cairo_surface_t base;

    Display *dpy;
    GC gc;
    Drawable drawable;
    int owns_pixmap;
    Visual *visual;
    cairo_format_t format;

    int render_major;
    int render_minor;

    int width;
    int height;

    Picture picture;
} cairo_xlib_surface_t;

#define CAIRO_SURFACE_RENDER_AT_LEAST(surface, major, minor)	\
	(((surface)->render_major > major) ||			\
	 (((surface)->render_major == major) && ((surface)->render_minor >= minor)))

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
#define CAIRO_SURFACE_RENDER_HAS_FILTERS(surface)	CAIRO_SURFACE_RENDER_AT_LEAST((surface), 0, 6)

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

static cairo_surface_t *
_cairo_xlib_surface_create_similar (void		*abstract_src,
				    cairo_format_t	format,
				    int			drawable,
				    int			width,
				    int			height)
{
    cairo_xlib_surface_t *src = abstract_src;
    Display *dpy = src->dpy;
    int scr;
    Pixmap pix;
    cairo_xlib_surface_t *surface;

    /* XXX: There's a pretty lame heuristic here. This assumes that
     * all non-Render X servers do not support depth-32 pixmaps, (and
     * that they do support depths 1, 8, and 24). Obviously, it would
     * be much better to check the depths that are actually
     * supported. */
    if (!dpy
	|| (!CAIRO_SURFACE_RENDER_HAS_COMPOSITE (src)
	    && format == CAIRO_FORMAT_ARGB32))
    {
	return NULL;
    }

    scr = DefaultScreen (dpy);

    pix = XCreatePixmap (dpy, DefaultRootWindow (dpy),
			 width, height,
			 _CAIRO_FORMAT_DEPTH (format));
    
    surface = (cairo_xlib_surface_t *)
	cairo_xlib_surface_create (dpy, pix, NULL, format, DefaultColormap (dpy, scr));
    surface->owns_pixmap = 1;

    surface->width = width;
    surface->height = height;

    return &surface->base;
}

static void
_cairo_xlib_surface_destroy (void *abstract_surface)
{
    cairo_xlib_surface_t *surface = abstract_surface;
    if (surface->picture)
	XRenderFreePicture (surface->dpy, surface->picture);

    if (surface->owns_pixmap)
	XFreePixmap (surface->dpy, surface->drawable);

    if (surface->gc)
	XFreeGC (surface->dpy, surface->gc);

    surface->dpy = 0;

    free (surface);
}

static double
_cairo_xlib_surface_pixels_per_inch (void *abstract_surface)
{
    /* XXX: We should really get this value from somewhere like Xft.dpy */
    return 96.0;
}

static cairo_image_surface_t *
_cairo_xlib_surface_get_image (void *abstract_surface)
{
    cairo_xlib_surface_t *surface = abstract_surface;
    cairo_image_surface_t *image;

    XImage *ximage;
    Window root_ignore;
    int x_ignore, y_ignore, bwidth_ignore, depth_ignore;

    XGetGeometry (surface->dpy, 
		  surface->drawable, 
		  &root_ignore, &x_ignore, &y_ignore,
		  &surface->width, &surface->height,
		  &bwidth_ignore, &depth_ignore);

    ximage = XGetImage (surface->dpy,
			surface->drawable,
			0, 0,
			surface->width, surface->height,
			AllPlanes, ZPixmap);

    if (surface->visual) {
	cairo_format_masks_t masks;

	/* XXX: Add support here for pictures with external alpha? */

	masks.bpp = ximage->bits_per_pixel;
	masks.alpha_mask = 0;
	masks.red_mask = surface->visual->red_mask;
	masks.green_mask = surface->visual->green_mask;
	masks.blue_mask = surface->visual->blue_mask;

	image = _cairo_image_surface_create_with_masks (ximage->data,
							&masks,
							ximage->width, 
							ximage->height,
							ximage->bytes_per_line);
    } else {
	image = (cairo_image_surface_t *)
	    cairo_image_surface_create_for_data (ximage->data,
						 surface->format,
						 ximage->width, 
						 ximage->height,
						 ximage->bytes_per_line);
    }

    /* Let the surface take ownership of the data */
    /* XXX: Can probably come up with a cleaner API here. */
    _cairo_image_surface_assume_ownership_of_data (image);
    ximage->data = NULL;
    XDestroyImage (ximage);
     
    _cairo_image_surface_set_repeat (image, surface->base.repeat);
    _cairo_image_surface_set_matrix (image, &(surface->base.matrix));

    return image;
}

static void
_cairo_xlib_surface_ensure_gc (cairo_xlib_surface_t *surface)
{
    if (surface->gc)
	return;

    surface->gc = XCreateGC (surface->dpy, surface->drawable, 0, NULL);
}

static cairo_status_t
_cairo_xlib_surface_set_image (void			*abstract_surface,
			       cairo_image_surface_t	*image)
{
    cairo_xlib_surface_t *surface = abstract_surface;
    XImage *ximage;
    unsigned bitmap_pad;

    if (image->depth > 16)
	bitmap_pad = 32;
    else if (image->depth > 8)
	bitmap_pad = 16;
    else
	bitmap_pad = 8;

    ximage = XCreateImage (surface->dpy,
			   DefaultVisual(surface->dpy, DefaultScreen(surface->dpy)),
			   image->depth,
			   ZPixmap,
			   0,
			   image->data,
			   image->width,
			   image->height,
			   bitmap_pad,
			   image->stride);
    if (ximage == NULL)
	return CAIRO_STATUS_NO_MEMORY;

    _cairo_xlib_surface_ensure_gc (surface);
    XPutImage(surface->dpy, surface->drawable, surface->gc,
	      ximage, 0, 0, 0, 0,
	      surface->width,
	      surface->height);

    /* Foolish XDestroyImage thinks it can free my data, but I won't
       stand for it. */
    ximage->data = NULL;
    XDestroyImage (ximage);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_xlib_surface_set_matrix (void *abstract_surface, cairo_matrix_t *matrix)
{
    cairo_xlib_surface_t *surface = abstract_surface;
    XTransform xtransform;

    if (!surface->picture)
	return CAIRO_STATUS_SUCCESS;

    xtransform.matrix[0][0] = _cairo_fixed_from_double (matrix->m[0][0]);
    xtransform.matrix[0][1] = _cairo_fixed_from_double (matrix->m[1][0]);
    xtransform.matrix[0][2] = _cairo_fixed_from_double (matrix->m[2][0]);

    xtransform.matrix[1][0] = _cairo_fixed_from_double (matrix->m[0][1]);
    xtransform.matrix[1][1] = _cairo_fixed_from_double (matrix->m[1][1]);
    xtransform.matrix[1][2] = _cairo_fixed_from_double (matrix->m[2][1]);

    xtransform.matrix[2][0] = 0;
    xtransform.matrix[2][1] = 0;
    xtransform.matrix[2][2] = _cairo_fixed_from_double (1);

    if (CAIRO_SURFACE_RENDER_HAS_PICTURE_TRANSFORM (surface))
    {
	XRenderSetPictureTransform (surface->dpy, surface->picture, &xtransform);
    } else {
	/* XXX: Need support here if using an old RENDER without support
	   for SetPictureTransform */
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_xlib_surface_set_filter (void *abstract_surface, cairo_filter_t filter)
{
    cairo_xlib_surface_t *surface = abstract_surface;
    char *render_filter;

    if (!(surface->picture 
	  && CAIRO_SURFACE_RENDER_HAS_FILTERS(surface)))
	return CAIRO_STATUS_SUCCESS;
    
    switch (filter) {
    case CAIRO_FILTER_FAST:
	render_filter = FilterFast;
	break;
    case CAIRO_FILTER_GOOD:
	render_filter = FilterGood;
	break;
    case CAIRO_FILTER_BEST:
	render_filter = FilterBest;
	break;
    case CAIRO_FILTER_NEAREST:
	render_filter = FilterNearest;
	break;
    case CAIRO_FILTER_BILINEAR:
	render_filter = FilterBilinear;
	break;
    default:
	render_filter = FilterBest;
	break;
    }

    XRenderSetPictureFilter (surface->dpy, surface->picture,
			     render_filter, NULL, 0);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_xlib_surface_set_repeat (void *abstract_surface, int repeat)
{
    cairo_xlib_surface_t *surface = abstract_surface;
    unsigned long mask;
    XRenderPictureAttributes pa;

    if (!surface->picture)
	return CAIRO_STATUS_SUCCESS;
    
    mask = CPRepeat;
    pa.repeat = repeat;

    XRenderChangePicture (surface->dpy, surface->picture, mask, &pa);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_xlib_surface_t *
_cairo_xlib_surface_clone_similar (cairo_surface_t	*src,
				   cairo_xlib_surface_t	*template,
				   cairo_format_t	format,
				   int			depth)
{
    cairo_xlib_surface_t *clone;
    cairo_image_surface_t *src_image;

    src_image = _cairo_surface_get_image (src);

    clone = (cairo_xlib_surface_t *)
	_cairo_xlib_surface_create_similar (template, format, 0,
					    src_image->width,
					    src_image->height);
    if (clone == NULL)
	return NULL;

    _cairo_xlib_surface_set_filter (clone, cairo_surface_get_filter(src));

    _cairo_xlib_surface_set_image (clone, src_image);

    _cairo_xlib_surface_set_matrix (clone, &(src_image->base.matrix));

    cairo_surface_destroy (&src_image->base);

    return clone;
}

static int
_render_operator (cairo_operator_t operator)
{
    switch (operator) {
    case CAIRO_OPERATOR_CLEAR:
	return PictOpClear;
    case CAIRO_OPERATOR_SRC:
	return PictOpSrc;
    case CAIRO_OPERATOR_DST:
	return PictOpDst;
    case CAIRO_OPERATOR_OVER:
	return PictOpOver;
    case CAIRO_OPERATOR_OVER_REVERSE:
	return PictOpOverReverse;
    case CAIRO_OPERATOR_IN:
	return PictOpIn;
    case CAIRO_OPERATOR_IN_REVERSE:
	return PictOpInReverse;
    case CAIRO_OPERATOR_OUT:
	return PictOpOut;
    case CAIRO_OPERATOR_OUT_REVERSE:
	return PictOpOutReverse;
    case CAIRO_OPERATOR_ATOP:
	return PictOpAtop;
    case CAIRO_OPERATOR_ATOP_REVERSE:
	return PictOpAtopReverse;
    case CAIRO_OPERATOR_XOR:
	return PictOpXor;
    case CAIRO_OPERATOR_ADD:
	return PictOpAdd;
    case CAIRO_OPERATOR_SATURATE:
	return PictOpSaturate;
    default:
	return PictOpOver;
    }
}

static cairo_int_status_t
_cairo_xlib_surface_composite (cairo_operator_t		operator,
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
    cairo_xlib_surface_t *dst = abstract_dst;
    cairo_xlib_surface_t *src = (cairo_xlib_surface_t *) generic_src;
    cairo_xlib_surface_t *mask = (cairo_xlib_surface_t *) generic_mask;
    cairo_xlib_surface_t *src_clone = NULL;
    cairo_xlib_surface_t *mask_clone = NULL;
    XGCValues gc_values;
    int src_x_off, src_y_off, dst_x_off, dst_y_off;

    if (!CAIRO_SURFACE_RENDER_HAS_COMPOSITE (dst))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if (generic_src->backend != dst->base.backend || src->dpy != dst->dpy) {
	src_clone = _cairo_xlib_surface_clone_similar (generic_src, dst,
						       CAIRO_FORMAT_ARGB32, 32);
	if (!src_clone)
	    return CAIRO_INT_STATUS_UNSUPPORTED;
	src = src_clone;
    }
    if (generic_mask && (generic_mask->backend != dst->base.backend || mask->dpy != dst->dpy)) {
	mask_clone = _cairo_xlib_surface_clone_similar (generic_mask, dst,
							CAIRO_FORMAT_A8, 8);
	if (!mask_clone)
	    return CAIRO_INT_STATUS_UNSUPPORTED;
	mask = mask_clone;
    }

    if (operator == CAIRO_OPERATOR_SRC 
	&& !mask
	&& _cairo_matrix_is_integer_translation(&(src->base.matrix), 
						&src_x_off, &src_y_off)
	&& _cairo_matrix_is_integer_translation(&(dst->base.matrix), 
						&dst_x_off, &dst_y_off)) {
	/* Fast path for copying "raw" areas. */
 	_cairo_xlib_surface_ensure_gc (dst); 
	XGetGCValues(dst->dpy, dst->gc, GCGraphicsExposures, &gc_values);
	XSetGraphicsExposures(dst->dpy, dst->gc, False);
	XCopyArea(dst->dpy, 
		  src->drawable, 
		  dst->drawable, 
		  dst->gc, 
		  src_x + src_x_off, 
		  src_y + src_y_off, 
		  width, height, 
		  dst_x + dst_x_off, 
		  dst_y + dst_y_off);
	XSetGraphicsExposures(dst->dpy, dst->gc, gc_values.graphics_exposures);

    } else {	
    XRenderComposite (dst->dpy,
		      _render_operator (operator),
		      src->picture,
		      mask ? mask->picture : 0,
		      dst->picture,
		      src_x, src_y,
		      mask_x, mask_y,
		      dst_x, dst_y,
		      width, height);
    }

    /* XXX: This is messed up. If I can xlib_surface_create, then I
       should be able to xlib_surface_destroy. */
    if (src_clone)
	cairo_surface_destroy (&src_clone->base);
    if (mask_clone)
	cairo_surface_destroy (&mask_clone->base);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_xlib_surface_fill_rectangles (void			*abstract_surface,
				     cairo_operator_t		operator,
				     const cairo_color_t	*color,
				     cairo_rectangle_t		*rects,
				     int			num_rects)
{
    cairo_xlib_surface_t *surface = abstract_surface;
    XRenderColor render_color;

    if (!CAIRO_SURFACE_RENDER_HAS_FILL_RECTANGLE (surface))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    render_color.red   = color->red_short;
    render_color.green = color->green_short;
    render_color.blue  = color->blue_short;
    render_color.alpha = color->alpha_short;

    /* XXX: This XRectangle cast is evil... it needs to go away somehow. */
    XRenderFillRectangles (surface->dpy,
			   _render_operator (operator),
			   surface->picture,
			   &render_color, (XRectangle *) rects, num_rects);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_xlib_surface_composite_trapezoids (cairo_operator_t	operator,
					  cairo_surface_t	*generic_src,
					  void			*abstract_dst,
					  int			xSrc,
					  int			ySrc,
					  cairo_trapezoid_t	*traps,
					  int			num_traps)
{
    cairo_xlib_surface_t *dst = abstract_dst;
    cairo_xlib_surface_t *src = (cairo_xlib_surface_t *) generic_src;
    cairo_xlib_surface_t *src_clone = NULL;

    if (!CAIRO_SURFACE_RENDER_HAS_TRAPEZOIDS (dst))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if (generic_src->backend != dst->base.backend || src->dpy != dst->dpy) {
	src_clone = _cairo_xlib_surface_clone_similar (generic_src, dst,
						       CAIRO_FORMAT_ARGB32, 32);
	if (!src_clone)
	    return CAIRO_INT_STATUS_UNSUPPORTED;
	src = src_clone;
    }

    /* XXX: The XTrapezoid cast is evil and needs to go away somehow. */
    XRenderCompositeTrapezoids (dst->dpy,
				_render_operator (operator),
				src->picture, dst->picture,
				XRenderFindStandardFormat (dst->dpy, PictStandardA8),
				xSrc, ySrc, (XTrapezoid *) traps, num_traps);

    /* XXX: This is messed up. If I can xlib_surface_create, then I
       should be able to xlib_surface_destroy. */
    if (src_clone)
	cairo_surface_destroy (&src_clone->base);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_xlib_surface_copy_page (void *abstract_surface)
{
    return CAIRO_INT_STATUS_UNSUPPORTED;
}

static cairo_int_status_t
_cairo_xlib_surface_show_page (void *abstract_surface)
{
    return CAIRO_INT_STATUS_UNSUPPORTED;
}

static cairo_int_status_t
_cairo_xlib_surface_set_clip_region (void *abstract_surface,
				     pixman_region16_t *region)
{

    Region xregion;
    XRectangle xr;
    XRectangle *rects = NULL;
    XGCValues gc_values;
    pixman_box16_t *box;
    cairo_xlib_surface_t *surf;
    int n, m;

    surf = (cairo_xlib_surface_t *) abstract_surface;

    if (region == NULL) {
	/* NULL region == reset the clip */
	xregion = XCreateRegion();
	xr.x = 0;
	xr.y = 0;
	xr.width = surf->width;
	xr.height = surf->height;
	XUnionRectWithRegion (&xr, xregion, xregion);
	rects = malloc(sizeof(XRectangle));
	rects[0] = xr;
	m = 1;

    } else {
	n = pixman_region_num_rects (region);
	/* XXX: Are we sure these are the semantics we want for an
	 * empty, (not null) region? */
	if (n == 0)
	    return CAIRO_STATUS_SUCCESS;
	rects = malloc(sizeof(XRectangle) * n);
	box = pixman_region_rects (region);
	xregion = XCreateRegion();
	
	m = n;
	for (; n > 0; --n, ++box) {
	    xr.x = (short) box->x1;
	    xr.y = (short) box->y1;
	    xr.width = (unsigned short) (box->x2 - box->x1);
	    xr.height = (unsigned short) (box->y2 - box->y1);
	    rects[n-1] = xr;
	    XUnionRectWithRegion (&xr, xregion, xregion);
	}    
    }
    
    _cairo_xlib_surface_ensure_gc (surf); 
    XGetGCValues(surf->dpy, surf->gc, GCGraphicsExposures, &gc_values);
    XSetGraphicsExposures(surf->dpy, surf->gc, False);
    XSetClipRectangles(surf->dpy, surf->gc, 0, 0, rects, m, Unsorted);
    free(rects);
    if (surf->picture)
    XRenderSetPictureClipRegion (surf->dpy, surf->picture, xregion);
    XDestroyRegion(xregion);
    XSetGraphicsExposures(surf->dpy, surf->gc, gc_values.graphics_exposures);
    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_xlib_surface_create_pattern (void *abstract_surface,
				    cairo_pattern_t *pattern,
				    cairo_box_t *extents)
{
    return CAIRO_INT_STATUS_UNSUPPORTED;
}

static const struct cairo_surface_backend cairo_xlib_surface_backend = {
    _cairo_xlib_surface_create_similar,
    _cairo_xlib_surface_destroy,
    _cairo_xlib_surface_pixels_per_inch,
    _cairo_xlib_surface_get_image,
    _cairo_xlib_surface_set_image,
    _cairo_xlib_surface_set_matrix,
    _cairo_xlib_surface_set_filter,
    _cairo_xlib_surface_set_repeat,
    _cairo_xlib_surface_composite,
    _cairo_xlib_surface_fill_rectangles,
    _cairo_xlib_surface_composite_trapezoids,
    _cairo_xlib_surface_copy_page,
    _cairo_xlib_surface_show_page,
    _cairo_xlib_surface_set_clip_region,
    _cairo_xlib_surface_create_pattern
};

cairo_surface_t *
cairo_xlib_surface_create (Display		*dpy,
			   Drawable		drawable,
			   Visual		*visual,
			   cairo_format_t	format,
			   Colormap		colormap)
{
    cairo_xlib_surface_t *surface;
    int render_standard;
    Window w;
    unsigned int ignore;

    surface = malloc (sizeof (cairo_xlib_surface_t));
    if (surface == NULL)
	return NULL;

    _cairo_surface_init (&surface->base, &cairo_xlib_surface_backend);

    surface->visual = visual;
    surface->format = format;

    surface->dpy = dpy;

    surface->gc = 0;
    surface->drawable = drawable;
    surface->owns_pixmap = 0;
    surface->visual = visual;

    if (! XRenderQueryVersion (dpy, &surface->render_major, &surface->render_minor)) {
	surface->render_major = -1;
	surface->render_minor = -1;
    }

    switch (format) {
    case CAIRO_FORMAT_A1:
	render_standard = PictStandardA1;
	break;
    case CAIRO_FORMAT_A8:
	render_standard = PictStandardA8;
	break;
    case CAIRO_FORMAT_RGB24:
	render_standard = PictStandardRGB24;
	break;
    case CAIRO_FORMAT_ARGB32:
    default:
	render_standard = PictStandardARGB32;
	break;
    }

    XGetGeometry(dpy, drawable, 
		 &w, &ignore, &ignore, 
		 &surface->width,
		 &surface->height,
		 &ignore, &ignore);

    /* XXX: I'm currently ignoring the colormap. Is that bad? */
    if (CAIRO_SURFACE_RENDER_HAS_CREATE_PICTURE (surface))
	surface->picture = XRenderCreatePicture (dpy, drawable,
						 visual ?
						 XRenderFindVisualFormat (dpy, visual) :
						 XRenderFindStandardFormat (dpy, render_standard),
						 0, NULL);
    else
	surface->picture = 0;

    return (cairo_surface_t *) surface;
}
DEPRECATE (cairo_surface_create_for_drawable, cairo_xlib_surface_create);
