/*
 * $XFree86: $
 *
 * Copyright © 2002 University of Southern California
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of University
 * of Southern California not be used in advertising or publicity
 * pertaining to distribution of the software without specific,
 * written prior permission.  University of Southern California makes
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied
 * warranty.
 *
 * UNIVERSITY OF SOUTHERN CALIFORNIA DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL UNIVERSITY OF
 * SOUTHERN CALIFORNIA BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author: Carl Worth, USC, Information Sciences Institute */

#include "xrint.h"

static void
_XrSurfaceCreateXcSurface(XrSurface *surface);

static void
_XrSurfaceDestroyXcSurface(XrSurface *surface);


void
XrSurfaceInit(XrSurface *surface, Display *dpy)
{
    surface->dpy = dpy;

    surface->drawable = 0;

    surface->depth = 0;

    surface->sa_mask = 0;

    surface->xcformat = 0;
    surface->xcsurface = 0;
    surface->alpha = 0;
}

void
XrSurfaceDeinit(XrSurface *surface)
{
    /* XXX: BUG: I'm not sure how to correctly deal with this. With the
       semantics of XrSave, we can share the surface, but we do want
       the last one freed eventually. Maybe I can reference count --
       or maybe I can free the surface when I finally push to the
       bottom of the stack.

    if (surface->surface) {
	XcFreeSurface(surface->dpy, surface->surface);
    } */
}

void
XrSurfaceSetSolidColor(XrSurface *surface, XrColor *color, XcFormat *format)
{
    /* XXX: QUESTION: Special handling for depth==1 ala xftdraw.c? */

    if (surface->xcsurface == 0) {
	Pixmap pix;
	XcSurfaceAttributes sa;

	pix = XCreatePixmap(surface->dpy, DefaultRootWindow(surface->dpy), 1, 1, format->depth);
	sa.repeat = True;
	surface->xcsurface = XcCreateDrawableSurface(surface->dpy, pix, format, CPRepeat, &sa);
	XFreePixmap(surface->dpy, pix);
    }
    
    XcFillRectangle(surface->dpy, PictOpSrc,
		    surface->xcsurface, &color->xccolor,
		    0, 0, 1, 1);
}

static void
_XrSurfaceCreateXcSurface(XrSurface *surface)
{
    if (surface->drawable && surface->xcformat) {
	surface->xcsurface = XcCreateDrawableSurface(surface->dpy, surface->drawable,
						     surface->xcformat, surface->sa_mask, &surface->sa);
    }
}

static void
_XrSurfaceDestroyXcSurface(XrSurface *surface)
{
    if (surface->xcsurface) {
	XcFreeSurface(surface->dpy, surface->xcsurface);
	surface->xcsurface = 0;
    }
}

void
XrSurfaceSetDrawable(XrSurface *surface, Drawable drawable)
{
    _XrSurfaceDestroyXcSurface(surface)

    surface->drawable = drawable;

    _XrSurfaceCreateXcSurface(surface);
}

void
XrSurfaceSetVisual(XrSurface *surface, Visual *visual)
{
    _XrSurfaceDestroyXcSurface(surface);

    surface->xcformat = XcFindVisualFormat(surface->dpy, visual);

    _XrSurfaceCreateXcSurface(surface);
}

void
XrSurfaceSetFormat(XrSurface *surface, XrFormat format)
{
    _XrSurfaceDestroyXcSurface(surface);
    
    surface->xcformat = XcFindStandardFormat(surface->dpy, format);

    _XrSurfaceCreateXcSurface(surface);
}
