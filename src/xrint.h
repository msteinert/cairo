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

/*
 * These definitions are solely for use by the implementation of Xr
 * and constitute no kind of standard.  If you need any of these
 * functions, please drop me a note.  Either the library needs new
 * functionality, or there's a way to do what you need using the
 * existing published interfaces. cworth@isi.edu
 */

#ifndef _XRINT_H_
#define _XRINT_H_

#include "Xr.h"

typedef struct _XrSubPath {
    int num_pts;
    int pts_size;
    XPointDouble *pts;
    int closed;

    struct _XrSubPath *next;
} XrSubPath;

typedef struct _XrPath {
    XrSubPath *head;
    XrSubPath *tail;
} XrPath;

typedef struct _XrPicture {
    Display *dpy;

    Drawable drawable;

    Visual *visual;
    unsigned int depth;

    XRenderPictFormat *format;
    unsigned long pa_mask;
    XRenderPictureAttributes pa;

    Picture picture;
    Picture alpha;

    /* XXX: Will also need a mechanism for a non-render picture here */
} XrPicture;

typedef struct _XrColor {
    double red;
    double green;
    double blue;
    double alpha;

    XRenderColor render;

    /* XXX: Will also need a mechanism for a non-render color here */
} XrColor;


typedef struct _XrTransform {
    double matrix[6];
} XrTransform;

#define XR_GSTATE_OP_DEFAULT		PictOpOver
#define XR_GSTATE_WINDING_DEFAULT	1
#define XR_GSTATE_LINE_WIDTH_DEFAULT	1.0

typedef struct _XrGState {
    Display *dpy;

    int op;
    int winding;
    double line_width;

    XRenderPictFormat *solidFormat;
    XRenderPictFormat *alphaFormat;

    XrColor color;
    XrPicture src;
    XrPicture picture;
    XrTransform transform;

    XrPath path;
    XrPath outline;

    struct _XrGState *next;
} XrGState;

struct _XrState {
    Display *dpy;
    XrGState *stack;
};

/* xrstate.c */

#define CURRENT_GSTATE(xrs) (xrs->stack)

XrState *
XrStateCreate(Display *dpy);

void
XrStateInit(XrState *state, Display *dpy);

void
XrStateDeinit(XrState *xrs);

void
XrStateDestroy(XrState *state);

void
XrStatePush(XrState *xrs);

void
XrStatePop(XrState *xrs);

/* xrgstate.c */
XrGState *
XrGStateCreate(Display *dpy);

XrGState *
XrGStateClone(XrGState *gstate);

void
XrGStateInit(XrGState *gstate, Display *dpy);

void
XrGStateInitCopy(XrGState *gstate, XrGState *other);

void
XrGStateDeinit(XrGState *gstate);

void
XrGStateDestroy(XrGState *gstate);

void
XrGStateGetCurrentPoint(XrGState *gstate, XPointDouble *pt);

void
XrGStateSetDrawable(XrGState *gstate, Drawable drawable, Visual *visual);

void
XrGStateSetColorRGB(XrGState *gstate, double red, double green, double blue);

void
XrGStateSetAlpha(XrGState *gstate, double alpha);

void
XrGStateSetLineWidth(XrGState *gstate, double width);

void
XrGStateTranslate(XrGState *gstate, double tx, double ty);

void
XrGStateScale(XrGState *gstate, double sx, double sy);

void
XrGStateRotate(XrGState *gstate, double angle);

void
XrGStateNewPath(XrGState *gstate);

void
XrGStateMoveTo(XrGState *gstate, double x, double y);

void
XrGStateLineTo(XrGState *gstate, double x, double y);

void
XrGStateRelMoveTo(XrGState *gstate, double x, double y);

void
XrGStateRelLineTo(XrGState *gstate, double x, double y);

void
XrGStateClosePath(XrGState *gstate);

void
XrGStateStroke(XrGState *gstate);

void
XrGStateFill(XrGState *fill);

/* xrcolor.c */
void
XrColorInit(XrColor *color);

void
XrColorDeinit(XrColor *color);

void
XrColorSetRGB(XrColor *color, double red, double green, double blue);

void
XrColorSetAlpha(XrColor *color, double alpha);

/* xrpath.c */
XrPath *
XrPathCreate(void);

void
XrPathInit(XrPath *path);

void
XrPathInitCopy(XrPath *path, XrPath *other);

void
XrPathReinit(XrPath *path);

void
XrPathDeinit(XrPath *path);

void
XrPathDestroy(XrPath *path);

XrPath *
XrPathClone(XrPath *path);

void
XrPathGetCurrentPoint(XrPath *path, XPointDouble *pt);

int
XrPathNumSubPaths(XrPath *path);

void
XrPathNewSubPath(XrPath *path);

void
XrPathAddPoint(XrPath *path, const XPointDouble *pt);

void
XrPathMoveTo(XrPath *path, const XPointDouble *pt);

void
XrPathLineTo(XrPath *path, const XPointDouble *pt);

void
XrPathClose(XrPath *path);

/* xrsubpath.c */
XrSubPath *
XrSubPathCreate(void);

void
XrSubPathInit(XrSubPath *path);

void
XrSubPathInitCopy(XrSubPath *path, XrSubPath *other);

void
XrSubPathDeinit(XrSubPath *path);

void
XrSubPathDestroy(XrSubPath *path);

XrSubPath *
XrSubPathClone(XrSubPath *path);

void
XrSubPathGetCurrentPoint(XrSubPath *path, XPointDouble *pt);

void
XrSubPathSetCurrentPoint(XrSubPath *path, const XPointDouble *pt);

void
XrSubPathAddPoint(XrSubPath *path, const XPointDouble *pt);

void
XrSubPathClose(XrSubPath *path);

/* xrpicture.c */
void
XrPictureInit(XrPicture *picture, Display *dpy);

void
XrPictureDeinit(XrPicture *picture);

void
XrPictureSetSolidColor(XrPicture *picture, XrColor *color, XRenderPictFormat *format);

void
XrPictureSetDrawable(XrPicture *picture, Drawable drawable, Visual *visual);

/* xrtransform.c */
void
XrTransformInit(XrTransform *transform);

void
XrTransformInitMatrix(XrTransform *transform,
		      double a, double b,
		      double c, double d,
		      double tx, double ty);

void
XrTransformInitTranslate(XrTransform *transform,
			 double tx, double ty);

void
XrTransformInitScale(XrTransform *transform,
		     double sx, double sy);

void
XrTransformInitRotate(XrTransform *transform,
		      double angle);

void
XrTransformDeinit(XrTransform *transform);

void
XrTransformCompose(XrTransform *t1, const XrTransform *t2);

void
XrTransformPointWithoutTranslate(XrTransform *transform, XPointDouble *pt);

void
XrTransformPoint(XrTransform *transform, XPointDouble *pt);

#endif
