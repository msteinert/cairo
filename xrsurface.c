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

#include "xrint.h"

void
_XrSurfaceInit(XrSurface *surface, Display *dpy)
{
    surface->dpy = dpy;

    surface->drawable = 0;
    surface->gc = 0;

    surface->depth = 0;

    surface->xc_sa_mask = 0;

    surface->xc_format = XcFindStandardFormat(dpy, PictStandardARGB32);

    surface->xc_surface = 0;
    surface->needs_new_xc_surface = 1;

    surface->ref_count = 0;
}

void
_XrSurfaceReference(XrSurface *surface)
{
    surface->ref_count++;
}

void
_XrSurfaceDereference(XrSurface *surface)
{
    if (surface->ref_count == 0)
	_XrSurfaceDeinit(surface);
    else
	surface->ref_count--;
}

void
_XrSurfaceDereferenceDestroy(XrSurface *surface)
{
    _XrSurfaceDereference(surface);

    if (surface->ref_count == 0)
	free(surface);
}

void
_XrSurfaceDeinit(XrSurface *surface)
{
    if (surface->xc_surface)
	XcFreeSurface(surface->dpy, surface->xc_surface);
}

void
_XrSurfaceSetSolidColor(XrSurface *surface, XrColor *color)
{
    /* XXX: QUESTION: Special handling for depth==1 ala xftdraw.c? */
    if (surface->xc_surface == 0) {
	Pixmap pix = XCreatePixmap(surface->dpy,
				   DefaultRootWindow(surface->dpy),
				   1, 1,
				   surface->xc_format->depth);
	_XrSurfaceSetDrawable(surface, pix);
	surface->xc_sa_mask |= CPRepeat;
	surface->xc_sa.repeat = True;
	_XrSurfaceGetXcSurface(surface);
	XFreePixmap(surface->dpy, pix);
    }
    
    XcFillRectangle(surface->dpy, PictOpSrc,
		    surface->xc_surface, &color->xc_color,
		    0, 0, 1, 1);
}

XrStatus
_XrSurfaceSetImage(XrSurface	*surface,
		   char		*data,
		   unsigned int	width,
		   unsigned int	height,
		   unsigned int	stride)
{
    XImage *image;
    unsigned int depth, bitmap_pad;
    Pixmap pix;

    depth = surface->xc_format->depth;

    if (depth > 16)
	bitmap_pad = 32;
    else if (depth > 8)
	bitmap_pad = 16;
    else
	bitmap_pad = 8;

    pix = XCreatePixmap(surface->dpy,
			DefaultRootWindow(surface->dpy),
			width, height,
			depth);
    _XrSurfaceSetDrawable(surface, pix);

    image = XCreateImage(surface->dpy,
			 DefaultVisual(surface->dpy, DefaultScreen(surface->dpy)),
			 depth, ZPixmap, 0,
			 data, width, height,
			 bitmap_pad,
			 stride);
    if (image == NULL)
	return XrStatusNoMemory;

    XPutImage(surface->dpy, surface->drawable, surface->gc,
	      image, 0, 0, 0, 0, width, height);

    /* Foolish XDestroyImage thinks it can free my data, but I won't
       stand for it. */
    image->data = NULL;
    XDestroyImage(image);

    return XrStatusSuccess;
}

/* XXX: We may want to move to projective matrices at some point. If
   nothing else, that would eliminate the two different transform data
   structures we have here. */
XrStatus
_XrSurfaceSetTransform(XrSurface *surface, XrTransform *transform)
{
    XTransform xtransform;

    xtransform.matrix[0][0] = XDoubleToFixed(transform->m[0][0]);
    xtransform.matrix[0][1] = 0;
    xtransform.matrix[0][2] = 0;

    xtransform.matrix[1][0] = 0;
    xtransform.matrix[1][1] = XDoubleToFixed(transform->m[1][1]);
    xtransform.matrix[1][2] = 0;

    xtransform.matrix[2][0] = 0;
    xtransform.matrix[2][1] = 0;
    xtransform.matrix[2][2] = XDoubleToFixed(1);
    
    XcSetSurfaceTransform(surface->dpy,
			  _XrSurfaceGetXcSurface(surface),
			  &xtransform);

    return XrStatusSuccess;
}

void
_XrSurfaceSetDrawable(XrSurface *surface, Drawable drawable)
{
    if (surface->gc)
	XFreeGC(surface->dpy, surface->gc);

    surface->drawable = drawable;
    surface->gc = XCreateGC(surface->dpy, surface->drawable, 0, 0);

    surface->needs_new_xc_surface = 1;
}

void
_XrSurfaceSetVisual(XrSurface *surface, Visual *visual)
{
    surface->xc_format = XcFindVisualFormat(surface->dpy, visual);
    surface->needs_new_xc_surface = 1;
}

void
_XrSurfaceSetFormat(XrSurface *surface, XrFormat format)
{
    surface->xc_format = XcFindStandardFormat(surface->dpy, format);
    surface->needs_new_xc_surface = 1;
}

XcSurface *
_XrSurfaceGetXcSurface(XrSurface *surface)
{
    if (surface == NULL)
	return NULL;

    if (! surface->needs_new_xc_surface)
	return surface->xc_surface;

    if (surface->xc_surface)
	XcFreeSurface(surface->dpy, surface->xc_surface);
    
    if (surface->drawable)
	surface->xc_surface = XcCreateDrawableSurface(surface->dpy,
						      surface->drawable,
						      surface->xc_format,
						      surface->xc_sa_mask,
						      &surface->xc_sa);
    else
	/* XXX: Is this what we wnat to do here? */
	surface->xc_surface = 0;

    surface->needs_new_xc_surface = 0;

    return surface->xc_surface;
}

Picture
_XrSurfaceGetPicture(XrSurface *surface)
{
    return XcSurfaceGetPicture(_XrSurfaceGetXcSurface(surface));
}
