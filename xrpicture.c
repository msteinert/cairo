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

void
XrPictureInit(XrPicture *picture, Display *dpy)
{
    picture->dpy = dpy;

    picture->drawable = 0;

    picture->depth = 0;

    picture->pa_mask = 0;

    picture->picture = 0;
}

void
XrPictureDeinit(XrPicture *picture)
{
    /* XXX: Leak for now. The problem is that I'm not currently cloning this as I should be sometime during XrSave.
    if (picture->picture) {
	XRenderFreePicture(picture->dpy, picture->picture);
    }
    */
}

void
XrPictureSetSolidColor(XrPicture *picture, XrColor *color, XRenderPictFormat *format)
{
    /* XXX: Special handling for depth==1 ala xftdraw.c? */

    if (picture->picture == 0) {
	Pixmap pix;
	XRenderPictureAttributes pa;

	pix = XCreatePixmap(picture->dpy, DefaultRootWindow(picture->dpy), 1, 1, format->depth);
	pa.repeat = True;
	picture->picture = XRenderCreatePicture(picture->dpy, pix, format, CPRepeat, &pa);
	XFreePixmap(picture->dpy, pix);
    }

    XRenderFillRectangle(picture->dpy, PictOpSrc,
			 picture->picture, &color->render,
			 0, 0, 1, 1);
}

void
XrPictureSetDrawable(XrPicture *picture, Drawable drawable)
{
    if (picture->picture) {
	XRenderFreePicture(picture->dpy, picture->picture);
	picture->picture = 0;
    }

    picture->drawable = drawable;
}

void
XrPictureSetVisual(XrPicture *picture, Visual *visual)
{
    XRenderPictFormat *pict_format;

    if (picture->picture) {
	XRenderFreePicture(picture->dpy, picture->picture);
	picture->picture = 0;
    }

    pict_format = XRenderFindVisualFormat(picture->dpy, visual);

    picture->picture = XRenderCreatePicture(picture->dpy, picture->drawable,
					    pict_format, picture->pa_mask, &picture->pa);
}

void
XrPictureSetFormat(XrPicture *picture, XrFormat format)
{
    XRenderPictFormat *pict_format;
    int std_format;

    if (picture->picture) {
	XRenderFreePicture(picture->dpy, picture->picture);
	picture->picture = 0;
    }

    switch (format) {
    case XrFormatARGB32:
	std_format = PictStandardARGB32;
	break;
    case XrFormatRGB32:
	std_format = PictStandardRGB24;
	break;
    case XrFormatA8:
	std_format = PictStandardA8;
	break;
    case XrFormatA1:
	std_format = PictStandardA1;
	break;
    default:
	return;
    }
    
    pict_format = XRenderFindStandardFormat(picture->dpy, std_format);

    picture->picture = XRenderCreatePicture(picture->dpy, picture->drawable,
					    pict_format, picture->pa_mask, &picture->pa);
}
