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

static void
_XrSurfaceCreateXcSurface(XrSurface *surface);

static void
_XrSurfaceDestroyXcSurface(XrSurface *surface);


void
_XrSurfaceInit(XrSurface *surface, Display *dpy)
{
    surface->dpy = dpy;

    surface->drawable = 0;

    surface->depth = 0;

    surface->xc_sa_mask = 0;

    surface->xc_format = 0;
    surface->xc_surface = 0;

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
_XrSurfaceDeinit(XrSurface *surface)
{
    if (surface->xc_surface)
	XcFreeSurface(surface->dpy, surface->xc_surface);
}

void
_XrSurfaceSetSolidColor(XrSurface *surface, XrColor *color, XcFormat *format)
{
    /* XXX: QUESTION: Special handling for depth==1 ala xftdraw.c? */

    if (surface->xc_surface == 0) {
	Pixmap pix;
	XcSurfaceAttributes sa;

	pix = XCreatePixmap(surface->dpy, DefaultRootWindow(surface->dpy), 1, 1, format->depth);
	sa.repeat = True;
	surface->xc_surface = XcCreateDrawableSurface(surface->dpy, pix, format, CPRepeat, &sa);
	XFreePixmap(surface->dpy, pix);
    }
    
    XcFillRectangle(surface->dpy, PictOpSrc,
		    surface->xc_surface, &color->xc_color,
		    0, 0, 1, 1);
}

static void
_XrSurfaceCreateXcSurface(XrSurface *surface)
{
    if (surface->drawable && surface->xc_format) {
	surface->xc_surface = XcCreateDrawableSurface(surface->dpy,
						      surface->drawable,
						      surface->xc_format,
						      surface->xc_sa_mask,
						      &surface->xc_sa);
    }
}

static void
_XrSurfaceDestroyXcSurface(XrSurface *surface)
{
    if (surface->xc_surface) {
	XcFreeSurface(surface->dpy, surface->xc_surface);
	surface->xc_surface = 0;
    }
}

/* XXX: These should probably be made to use lazy evaluation.
   A new API will be needed, something like _XrSurfacePrepare
*/
void
_XrSurfaceSetDrawable(XrSurface *surface, Drawable drawable)
{
    _XrSurfaceDestroyXcSurface(surface);

    surface->drawable = drawable;

    _XrSurfaceCreateXcSurface(surface);
}

void
_XrSurfaceSetVisual(XrSurface *surface, Visual *visual)
{
    _XrSurfaceDestroyXcSurface(surface);

    surface->xc_format = XcFindVisualFormat(surface->dpy, visual);

    _XrSurfaceCreateXcSurface(surface);
}

void
_XrSurfaceSetFormat(XrSurface *surface, XrFormat format)
{
    _XrSurfaceDestroyXcSurface(surface);
    
    surface->xc_format = XcFindStandardFormat(surface->dpy, format);

    _XrSurfaceCreateXcSurface(surface);
}

Picture
_XrSurfaceGetPicture(XrSurface *surface)
{
    return XcSurfaceGetPicture(surface->xc_surface);
}
