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

#include <X11/Xft/Xft.h>

#include "cairoint.h"
#include "cairo-xlib.h"
#include <X11/Xlibint.h>

static cairo_font_t *
_cairo_xlib_font_create (Display *dpy);

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
    cr->gstate->font = _cairo_xlib_font_create (dpy);

    cairo_surface_destroy (surface);
}

typedef struct cairo_xlib_surface {
    cairo_surface_t base;

    Display *dpy;
    GC gc;
    Drawable drawable;
    int owns_pixmap;
    Visual *visual;

    int render_major;
    int render_minor;

    Picture picture;
    XImage *ximage;
} cairo_xlib_surface;

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

static cairo_xlib_surface *
_cairo_xlib_surface_create_similar (cairo_xlib_surface	*other,
				    cairo_format_t	format,
				    int			width,
				    int			height)
{
    Display *dpy = other->dpy;
    int scr;
    Pixmap pix;
    cairo_xlib_surface *surface;

    /* XXX: There's a pretty lame heuristic here. This assumes that
     * all non-Render X servers do not support depth-32 pixmaps, (and
     * that they do support depths 1, 8, and 24). Obviously, it would
     * be much better to check the depths that are actually
     * supported. */
    if (!dpy
	|| (!CAIRO_SURFACE_RENDER_HAS_COMPOSITE (other)
	    && format == CAIRO_FORMAT_ARGB32))
	return 0;

    scr = DefaultScreen (dpy);

    pix = XCreatePixmap (dpy, DefaultRootWindow (dpy),
			 width, height,
			 _CAIRO_FORMAT_DEPTH (format));

    surface = (cairo_xlib_surface *)
	cairo_xlib_surface_create (dpy, pix, NULL, format, DefaultColormap (dpy, scr));
    surface->owns_pixmap = 1;

    return surface;
}

static void
_cairo_xlib_surface_destroy (cairo_xlib_surface *surface)
{
    if (surface->picture)
	XRenderFreePicture (surface->dpy, surface->picture);

    if (surface->owns_pixmap)
	XFreePixmap (surface->dpy, surface->drawable);

    surface->dpy = 0;
}

static void
_cairo_xlib_surface_pull_image (cairo_xlib_surface *surface)
{
    Window root_ignore;
    int x_ignore, y_ignore, bwidth_ignore, depth_ignore;

    if (surface == NULL)
	return;

    if (surface->base.icimage) {
	IcImageDestroy (surface->base.icimage);
	surface->base.icimage = NULL;
    }

    XGetGeometry(surface->dpy, 
		 surface->drawable, 
		 &root_ignore, &x_ignore, &y_ignore,
		 &surface->base.width, &surface->base.height,
		 &bwidth_ignore, &depth_ignore);

    surface->ximage = XGetImage (surface->dpy,
				 surface->drawable,
				 0, 0,
				 surface->base.width, surface->base.height,
				 AllPlanes, ZPixmap);

    surface->base.icimage = IcImageCreateForData ((IcBits *)(surface->ximage->data),
						  surface->base.icformat,
						  surface->ximage->width, 
						  surface->ximage->height,
						  surface->ximage->bits_per_pixel, 
						  surface->ximage->bytes_per_line);
     
    IcImageSetRepeat (surface->base.icimage, surface->base.repeat);
    /* XXX: Evil cast here... */
    IcImageSetTransform (surface->base.icimage, (IcTransform *) &(surface->base.xtransform));
    
    /* XXX: Add support here for pictures with external alpha. */
}

static void
_cairo_xlib_surface_ensure_gc (cairo_xlib_surface *surface)
{
    if (surface->gc)
	return;

    surface->gc = XCreateGC (surface->dpy, surface->drawable, 0, NULL);
}

static void
_cairo_xlib_surface_push_image (cairo_xlib_surface *surface)
{
    if (surface == NULL)
	return;

    if (surface->ximage == NULL)
	return;

    _cairo_xlib_surface_ensure_gc (surface);
    XPutImage (surface->dpy,
	       surface->drawable,
	       surface->gc,
	       surface->ximage,
	       0, 0,
	       0, 0,
	       surface->base.width,
	       surface->base.height);

    XDestroyImage(surface->ximage);
    surface->ximage = NULL;
}

static cairo_status_t
_cairo_xlib_surface_set_matrix (cairo_xlib_surface *surface)
{
    if (!surface->picture)
	return CAIRO_STATUS_SUCCESS;

    if (CAIRO_SURFACE_RENDER_HAS_PICTURE_TRANSFORM (surface))
    {
	XRenderSetPictureTransform (surface->dpy, surface->picture, &surface->base.xtransform);
    } else {
	/* XXX: Need support here if using an old RENDER without support
	   for SetPictureTransform */
    }

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

static cairo_status_t
_cairo_xlib_surface_set_filter (cairo_xlib_surface *surface, cairo_filter_t filter)
{
    if (!surface->picture)
	return CAIRO_STATUS_SUCCESS;

    XRenderSetPictureFilter (surface->dpy, surface->picture,
			     _render_filter_name (filter), NULL, 0);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_xlib_surface_set_repeat (cairo_xlib_surface *surface, int repeat)
{
    unsigned long mask;
    XRenderPictureAttributes pa;

    if (!surface->picture)
	return CAIRO_STATUS_SUCCESS;
    
    mask = CPRepeat;
    pa.repeat = repeat;

    XRenderChangePicture (surface->dpy, surface->picture, mask, &pa);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_xlib_surface_put_image (cairo_xlib_surface	*surface,
			      char			*data,
			      int			width,
			      int			height,
			      int			stride)
{
    XImage *image;
    unsigned bitmap_pad;
    
    if (!surface->picture)
	return CAIRO_STATUS_SUCCESS;

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

    _cairo_xlib_surface_ensure_gc (surface);
    XPutImage(surface->dpy, surface->drawable, surface->gc,
	      image, 0, 0, 0, 0, width, height);

    /* Foolish XDestroyImage thinks it can free my data, but I won't
       stand for it. */
    image->data = NULL;
    XDestroyImage(image);
    
    return CAIRO_STATUS_SUCCESS;
}

static cairo_xlib_surface *
_cairo_xlib_surface_clone_from (cairo_surface_t *src, cairo_xlib_surface *tmpl)
{
    cairo_matrix_t matrix;
    cairo_xlib_surface *src_on_server;

    _cairo_surface_pull_image (src);

    src_on_server = _cairo_xlib_surface_create_similar (tmpl, CAIRO_FORMAT_ARGB32,
							IcImageGetWidth (src->icimage),
							IcImageGetHeight (src->icimage));
    if (src_on_server == NULL)
	return NULL;

    cairo_surface_get_matrix (src, &matrix);
    cairo_surface_set_matrix (&src_on_server->base, &matrix);

    _cairo_xlib_surface_put_image (src_on_server,
				  (char *) IcImageGetData (src->icimage),
				  IcImageGetWidth (src->icimage),
				  IcImageGetHeight (src->icimage),
				  IcImageGetStride (src->icimage));
    return src_on_server;
}

static int
_cairo_xlib_surface_composite (cairo_operator_t		operator,
			       cairo_xlib_surface	*src,
			       cairo_xlib_surface	*mask,
			       cairo_xlib_surface	*dst,
			       int			src_x,
			       int			src_y,
			       int			mask_x,
			       int			mask_y,
			       int			dst_x,
			       int			dst_y,
			       unsigned int		width,
			       unsigned int		height)
{
    if (!CAIRO_SURFACE_RENDER_HAS_COMPOSITE (dst))
	return -1;

    if (src->base.backend != dst->base.backend || src->dpy != dst->dpy) {
	src = _cairo_xlib_surface_clone_from (&src->base, dst);
	if (!src)
	    return -1;
    }
    if (mask && (mask->base.backend != dst->base.backend || mask->dpy != dst->dpy)) {
	mask = _cairo_xlib_surface_clone_from (&mask->base, dst);
	if (!mask)
	    return -1;
    }

    XRenderComposite (dst->dpy, operator,
		      src->picture,
		      mask ? mask->picture : 0,
		      dst->picture,
		      src_x, src_y,
		      mask_x, mask_y,
		      dst_x, dst_y,
		      width, height);

    return 0;
}

static int
_cairo_xlib_surface_fill_rectangles (cairo_xlib_surface		*surface,
				     cairo_operator_t		operator,
				     const cairo_color_t	*color,
				     cairo_rectangle_t		*rects,
				     int			num_rects)
{
    XRenderColor render_color;

    if (!CAIRO_SURFACE_RENDER_HAS_FILL_RECTANGLE (surface))
	return -1;

    render_color.red   = color->red_short;
    render_color.green = color->green_short;
    render_color.blue  = color->blue_short;
    render_color.alpha = color->alpha_short;

    /* XXX: This XRectangle cast is evil... it needs to go away somehow. */
    XRenderFillRectangles (surface->dpy, operator, surface->picture,
			   &render_color, (XRectangle *) rects, num_rects);

    return 0;
}

static int
_cairo_xlib_surface_composite_trapezoids (cairo_operator_t	operator,
					  cairo_xlib_surface	*src,
					  cairo_xlib_surface	*dst,
					  int			xSrc,
					  int			ySrc,
					  cairo_trapezoid_t	*traps,
					  int			num_traps)
{
    if (!CAIRO_SURFACE_RENDER_HAS_TRAPEZOIDS (dst))
	return -1;

    if (src->base.backend != dst->base.backend || src->dpy != dst->dpy) {
	src = _cairo_xlib_surface_clone_from (&src->base, dst);
	if (!src)
	    return -1;
    }

    /* XXX: The XTrapezoid cast is evil and needs to go away somehow. */
    XRenderCompositeTrapezoids (dst->dpy, operator, src->picture, dst->picture,
				XRenderFindStandardFormat (dst->dpy, PictStandardA8),
				xSrc, ySrc, (XTrapezoid *) traps, num_traps);

    return 0;
}

static const struct cairo_surface_backend cairo_xlib_surface_backend = {
    create_similar:		(void *) _cairo_xlib_surface_create_similar,
    destroy:			(void *) _cairo_xlib_surface_destroy,
    pull_image:			(void *) _cairo_xlib_surface_pull_image,
    push_image:			(void *) _cairo_xlib_surface_push_image,
    set_matrix:			(void *) _cairo_xlib_surface_set_matrix,
    set_filter:			(void *) _cairo_xlib_surface_set_filter,
    set_repeat:			(void *) _cairo_xlib_surface_set_repeat,
    composite:			(void *) _cairo_xlib_surface_composite,
    fill_rectangles:		(void *) _cairo_xlib_surface_fill_rectangles,
    composite_trapezoids:	(void *) _cairo_xlib_surface_composite_trapezoids,
};

Picture
_cairo_xlib_surface_get_picture (cairo_surface_t *surface)
{
    if (surface->backend != &cairo_xlib_surface_backend)
	return 0;

    return ((cairo_xlib_surface *) surface)->picture;
}

cairo_surface_t *
cairo_xlib_surface_create (Display		*dpy,
			   Drawable		drawable,
			   Visual		*visual,
			   cairo_format_t	format,
			   Colormap		colormap)
{
    cairo_xlib_surface *surface;

    surface = malloc (sizeof (cairo_xlib_surface));
    if (surface == NULL)
	return NULL;

    /* XXX: How to get the proper width/height? Force a roundtrip? And
       how can we track the width/height properly? Shall we give up on
       supporting Windows and only allow drawing to pixmaps? */
    _cairo_surface_init (&surface->base, 0, 0, format, &cairo_xlib_surface_backend);

    if (visual) {
	if (surface->base.icformat)
	    IcFormatDestroy (surface->base.icformat);
	surface->base.icformat = IcFormatCreateMasks (32, 0,
						      visual->red_mask,
						      visual->green_mask,
						      visual->blue_mask);
    }

    surface->dpy = dpy;

    surface->gc = 0;
    surface->drawable = drawable;
    surface->owns_pixmap = 0;
    surface->visual = visual;

    if (! XRenderQueryVersion (dpy, &surface->render_major, &surface->render_minor)) {
	surface->render_major = -1;
	surface->render_minor = -1;
    }

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

    return (cairo_surface_t *) surface;
}
DEPRECATE (cairo_surface_create_for_drawable, cairo_xlib_surface_create);


typedef struct cairo_xlib_font {
    cairo_font_t base;

    Display *dpy;
    XftFont *xft_font;
} cairo_xlib_font_t;

static cairo_font_t *
_cairo_xlib_font_copy (cairo_xlib_font_t *other)
{
    cairo_xlib_font_t *font;
    font = malloc (sizeof (cairo_xlib_font_t));
    if (!font)
	return 0;

    if (_cairo_font_init_copy (&font->base, &other->base))
	goto abort;

    font->dpy = other->dpy;

    if (other->xft_font) {
	font->xft_font = XftFontCopy (other->dpy, other->xft_font);
	if (font->xft_font == NULL)
	    goto abort;
    } else
	font->xft_font = NULL;

    return &font->base;

abort:
    _cairo_font_fini (&font->base);
    return 0;
}

static void
_cairo_xlib_font_close (cairo_xlib_font_t *font)
{
    if (font->xft_font)
	XftFontClose (font->dpy, font->xft_font);
    font->xft_font = NULL;
}

static cairo_status_t
_cairo_xlib_font_resolve (cairo_xlib_font_t *font, cairo_matrix_t *ctm)
{
    FcPattern	*pattern;
    FcPattern	*match;
    FcResult	result;
    cairo_matrix_t	matrix;
    FcMatrix	fc_matrix;
    double	expansion;
    double	font_size;
    
    if (font->xft_font)
	return CAIRO_STATUS_SUCCESS;
    
    pattern = FcNameParse (font->base.key);

    matrix = *ctm;
    cairo_matrix_multiply (&matrix, &font->base.matrix, &matrix);

    /* Pull the scale factor out of the final matrix and use it to set
       the direct pixelsize of the font. This enables freetype to
       perform proper hinting at any size. */

    /* XXX: The determinant gives an area expansion factor, so the
       math below should be correct for the (common) case of uniform
       X/Y scaling. Is there anything different we would want to do
       for non-uniform X/Y scaling?

       XXX: Actually, the reasoning above is bogus. A transformation
       such as scale (N, 1/N) will give an expansion_factor of 1. So,
       with the code below we'll end up with font_size == 1 instead of
       N, (so the hinting will be all wrong). I think we want to use
       the maximum eigen value rather than the square root of the
       determinant. */
    _cairo_matrix_compute_determinant (&matrix, &expansion);
    font_size = sqrt (fabs (expansion));

    FcPatternAddDouble (pattern, "pixelsize", font_size);
    cairo_matrix_scale (&matrix, 1.0 / font_size, 1.0 / font_size);

    fc_matrix.xx = matrix.m[0][0];
    fc_matrix.xy = matrix.m[0][1];
    fc_matrix.yx = matrix.m[1][0];
    fc_matrix.yy = matrix.m[1][1];

    FcPatternAddMatrix (pattern, "matrix", &fc_matrix);

    match = XftFontMatch (font->dpy, DefaultScreen (font->dpy), pattern, &result);
    if (!match)
	return 0;
    
    font->xft_font = XftFontOpenPattern (font->dpy, match);

    FcPatternDestroy (pattern);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_xlib_font_text_extents (cairo_xlib_font_t *font,
			       cairo_matrix_t *ctm,
			       const unsigned char *utf8,
			       double *x, double *y,
			       double *width, double *height,
			       double *dx, double *dy)
{
    XGlyphInfo extents;

    _cairo_xlib_font_resolve (font, ctm);

    XftTextExtentsUtf8 (font->dpy,
			font->xft_font,
			utf8,
			strlen ((char *) utf8),
			&extents);

    /* XXX: What are the semantics of XftTextExtents? Specifically,
       what does it do with x/y? I think we actually need to use the
       gstate's current point in here somewhere. */
    *x = extents.x;
    *y = extents.y;
    *width = extents.width;
    *height = extents.height;
    *dx = extents.xOff;
    *dy = extents.yOff;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_xlib_font_show_text (cairo_xlib_font_t		*font,
			    cairo_matrix_t		*ctm,
			    cairo_operator_t		operator,
			    cairo_surface_t		*source,
			    cairo_surface_t		*surface,
			    double			x,
			    double			y,
			    const unsigned char	*utf8)
{
    Picture source_picture, surface_picture;

    _cairo_xlib_font_resolve (font, ctm);

    source_picture = _cairo_xlib_surface_get_picture (source);
    surface_picture = _cairo_xlib_surface_get_picture (surface);

    XftTextRenderUtf8 (font->dpy,
		       operator,
		       source_picture,
		       font->xft_font,
		       surface_picture,
		       0, 0,
		       x, y,
		       utf8,
		       strlen ((char *) utf8));

    return CAIRO_STATUS_SUCCESS;
}

static const struct cairo_font_backend cairo_xlib_font_backend = {
    copy:		(void *) _cairo_xlib_font_copy,
    close:		(void *) _cairo_xlib_font_close,
    text_extents:	(void *) _cairo_xlib_font_text_extents,
    show_text:		(void *) _cairo_xlib_font_show_text,
};

static cairo_font_t *
_cairo_xlib_font_create (Display *dpy)
{
    cairo_xlib_font_t *font;
    font = malloc (sizeof (cairo_xlib_font_t));
    if (!font)
	return 0;

    _cairo_font_init (&font->base, &cairo_xlib_font_backend);

    font->dpy = dpy;
    font->xft_font = NULL;

    return &font->base;
}
