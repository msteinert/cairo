/*
 * $XFree86: $
 *
 * Copyright © 2002 Carl D. Worth
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of Carl
 * D. Worth not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission.  Carl D. Worth makes no representations about the
 * suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * CARL D. WORTH DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL CARL D. WORTH BE LIABLE FOR ANY SPECIAL,
 * INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
 * IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdlib.h>

#include "xrint.h"

XrSurface *
XrSurfaceCreateForDrawable (Display	*dpy,
			    Drawable	drawable,
			    Visual	*visual,
			    XrFormat	format,
			    Colormap	colormap)
{
    XrSurface *surface;

    surface = malloc(sizeof(XrSurface));
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
_XrFormatBPP (XrFormat format)
{
    switch (format) {
    case XrFormatA1:
	return 1;
	break;
    case XrFormatA8:
	return 8;
	break;
    case XrFormatRGB24:
    case XrFormatARGB32:
    default:
	return 32;
	break;
    }
}

XrSurface *
XrSurfaceCreateForImage (char		*data,
			 XrFormat	format,
			 int		width,
			 int		height,
			 int		stride)
{
    XrSurface *surface;
    IcFormat icformat;
    IcImage *image;
    int bpp;

    /* XXX: This all needs to change, (but IcFormatInit interface needs to change first) */
    switch (format) {
    case XrFormatARGB32:
	IcFormatInit (&icformat, PICT_a8r8g8b8);
	bpp = 32;
	break;
    case XrFormatRGB24:
	IcFormatInit (&icformat, PICT_x8r8g8b8);
	bpp = 32;
	break;
    case XrFormatA8:
	IcFormatInit (&icformat, PICT_a8);
	bpp = 8;
	break;
    case XrFormatA1:
	IcFormatInit (&icformat, PICT_a1);
	bpp = 1;
	break;
    default:
	return NULL;
    }

    surface = malloc(sizeof(XrSurface));
    if (surface == NULL)
	return NULL;

    surface->dpy = NULL;
    surface->image_data = NULL;
    image = IcImageCreateForData ((IcBits *) data, &icformat, width, height, _XrFormatBPP (format), stride);
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

XrSurface *
XrSurfaceCreateNextTo (XrSurface *neighbor, XrFormat format, int width, int height)
{
    return XrSurfaceCreateNextToSolid (neighbor, format, width, height, 0, 0, 0, 0);
}

static int
_XrFormatDepth (XrFormat format)
{
    switch (format) {
    case XrFormatA1:
	return 1;
    case XrFormatA8:
	return 8;
    case XrFormatRGB24:
	return 24;
    case XrFormatARGB32:
    default:
	return 32;
    }
}

XrSurface *
XrSurfaceCreateNextToSolid (XrSurface	*neighbor,
			    XrFormat	format,
			    int		width,
			    int		height,
			    double	red,
			    double	green,
			    double	blue,
			    double	alpha)
{
    XrSurface *surface = NULL;
    XrColor color;

    /* XXX: CreateNextTo should perhaps move down to Xc, (then we
       could drop xrsurface->dpy as well) */
    if (neighbor->dpy) {
	Display *dpy = neighbor->dpy;
	int scr = DefaultScreen (dpy);

	Pixmap pix = XCreatePixmap(dpy,
				   DefaultRootWindow (dpy),
				   width, height,
				   _XrFormatDepth (format));

	surface = XrSurfaceCreateForDrawable (dpy, pix,
					      NULL,
					      format,
					      DefaultColormap (dpy, scr));
	XFreePixmap(surface->dpy, pix);
    } else {
	char *data;
	int stride;

	stride = ((width * _XrFormatBPP (format)) + 7) >> 3;
	data = malloc (stride * height);
	if (data == NULL)
	    return NULL;

	surface = XrSurfaceCreateForImage (data, format,
					   width, height, stride);

	/* lodge data in the surface structure to be freed with the surface */
	surface->image_data = data;
    }

    /* XXX: Initializing the color in this way assumes
       non-pre-multiplied alpha. I'm not sure that that's what I want
       to do or not. */
    _XrColorInit (&color);
    _XrColorSetRGB (&color, red, green, blue);
    _XrColorSetAlpha (&color, alpha);
    _XrSurfaceFillRectangle (surface, XrOperatorSrc, &color, 0, 0, width, height);
    return surface;
}

void
_XrSurfaceReference(XrSurface *surface)
{
    if (surface == NULL)
	return;

    surface->ref_count++;
}

void
XrSurfaceDestroy(XrSurface *surface)
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
	free(surface->image_data);
    surface->image_data = NULL;

    free(surface);
}

/* XXX: We may want to move to projective matrices at some point. If
   nothing else, that would eliminate the two different transform data
   structures we have here. */
XrStatus
XrSurfaceSetMatrix(XrSurface *surface, XrMatrix *matrix)
{
    XTransform xtransform;

    xtransform.matrix[0][0] = XDoubleToFixed(matrix->m[0][0]);
    xtransform.matrix[0][1] = XDoubleToFixed(matrix->m[1][0]);
    xtransform.matrix[0][2] = XDoubleToFixed(matrix->m[2][0]);

    xtransform.matrix[1][0] = XDoubleToFixed(matrix->m[0][1]);
    xtransform.matrix[1][1] = XDoubleToFixed(matrix->m[1][1]);
    xtransform.matrix[1][2] = XDoubleToFixed(matrix->m[2][1]);

    xtransform.matrix[2][0] = 0;
    xtransform.matrix[2][1] = 0;
    xtransform.matrix[2][2] = XDoubleToFixed(1);

    XcSurfaceSetTransform(surface->xc_surface,
			  &xtransform);

    return XrStatusSuccess;
}

XrStatus
XrSurfaceGetMatrix (XrSurface *surface, XrMatrix *matrix)
{
    XTransform xtransform;

    XcSurfaceGetTransform (surface->xc_surface, &xtransform);

    matrix->m[0][0] = XFixedToDouble (xtransform.matrix[0][0]);
    matrix->m[1][0] = XFixedToDouble (xtransform.matrix[0][1]);
    matrix->m[2][0] = XFixedToDouble (xtransform.matrix[0][2]);

    matrix->m[0][1] = XFixedToDouble (xtransform.matrix[1][0]);
    matrix->m[1][1] = XFixedToDouble (xtransform.matrix[1][1]);
    matrix->m[2][1] = XFixedToDouble (xtransform.matrix[1][2]);

    return XrStatusSuccess;
}

XrStatus
XrSurfaceSetFilter(XrSurface *surface, XrFilter filter)
{
    XcSurfaceSetFilter(surface->xc_surface, filter);
    return XrStatusSuccess;
}

/* XXX: The Xc version of this function isn't quite working yet
XrStatus
XrSurfaceSetClipRegion (XrSurface *surface, Region region)
{
    XcSurfaceSetClipRegion (surface->xc_surface, region);

    return XrStatusSuccess;
}
*/

XrStatus
XrSurfaceSetRepeat (XrSurface *surface, int repeat)
{
    XcSurfaceSetRepeat (surface->xc_surface, repeat);

    return XrStatusSuccess;
}

/* XXX: This function is going away, right? */
Picture
_XrSurfaceGetPicture(XrSurface *surface)
{
    return XcSurfaceGetPicture(surface->xc_surface);
}

void
_XrSurfaceFillRectangle (XrSurface	*surface,
			 XrOperator	operator,
			 XrColor	*color,
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

