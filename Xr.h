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

#ifndef _XR_H_
#define _XR_H_

#include <X11/extensions/Xrender.h>

typedef struct _XrState XrState;

/* Functions for manipulating state objects */
XrState *
XrCreate(Display *dpy, Drawable drawable, Visual *visual);

void
XrDestroy(XrState *xrs);

void
XrSave(XrState *xrs);

void
XrRestore(XrState *xrs);

/* XXX: XrClone */

/* Modify state */
void
XrSetDrawable(XrState *xrs, Drawable drawable, Visual *visual);

void
XrSetColorRGB(XrState *xrs, double red, double green, double blue);

void
XrSetAlpha(XrState *xrs, double alpha);

void
XrSetLineWidth(XrState *xrs, double width);

void
XrTranslate(XrState *xrs, double tx, double ty);

void
XrScale(XrState *xrs, double sx, double sy);

void
XrRotate(XrState *xrs, double angle);

/* XXX: XrSetLineCap, XrSetLineJoin, XrSetDash, ... */

/* Path creation */
void
XrNewPath(XrState *xrs);

void
XrMoveTo(XrState *xrs, double x, double y);

void
XrLineTo(XrState *xrs, double x, double y);

void
XrRelMoveTo(XrState *xrs, double x, double y);

void
XrRelLineTo(XrState *xrs, double x, double y);

void
XrClosePath(XrState *xrs);

/* XXX: XrArcTo, XrCurveTo, XrRelCurveTo, ... */

/* Render current path */
void
XrStroke(XrState *xrs);

void
XrFill(XrState *xrs);

#endif
