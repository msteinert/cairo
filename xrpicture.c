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
    picture->visual = 0;

    picture->format = 0;
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

static void
_XrPictureFindFormat(XrPicture *picture)
{
    if (picture->format) {
	return;
    }

    picture->format = XRenderFindVisualFormat(picture->dpy, picture->visual);
}

void
XrPictureSetDrawable(XrPicture *picture, Drawable drawable, Visual *visual)
{
    if (picture->picture) {
	XRenderFreePicture(picture->dpy, picture->picture);
    }

    picture->visual = visual;
    picture->drawable = drawable;

    _XrPictureFindFormat(picture);

    picture->picture = XRenderCreatePicture(picture->dpy, drawable,
					    picture->format, picture->pa_mask, &picture->pa);
}

