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

XrState *
XrCreate(Display *dpy)
{
    return XrStateCreate(dpy);
}

void
XrDestroy(XrState *xrs)
{
    XrStateDestroy(xrs);
}

void
XrSave(XrState *xrs)
{
    XrError err;

    err = XrStatePush(xrs);
    if (err)
	xrs->error = err;
}

void
XrRestore(XrState *xrs)
{
    /* XXX: BUG: Calling XrRestore without a matching XrSave shoud
       flag an error. Also, in order to prevent crashes, XrStatePop
       should not be called in that case. */
    XrStatePop(xrs);
}

void
XrSetDrawable(XrState *xrs, Drawable drawable)
{
    XrGStateSetDrawable(CURRENT_GSTATE(xrs), drawable);
}

void
XrSetVisual(XrState *xrs, Visual *visual)
{
    XrGStateSetVisual(CURRENT_GSTATE(xrs), visual);
}

void
XrSetFormat(XrState *xrs, XrFormat format)
{
    XrGStateSetFormat(CURRENT_GSTATE(xrs), format);
}

void
XrSetOperator(XrState *xrs, XrOperator operator)
{
    XrGStateSetOperator(CURRENT_GSTATE(xrs), operator);
}

void
XrSetRGBColor(XrState *xrs, double red, double green, double blue)
{
    XrGStateSetRGBColor(CURRENT_GSTATE(xrs), red, green, blue);
}

void
XrSetTolerance(XrState *xrs, double tolerance)
{
    XrGStateSetTolerance(CURRENT_GSTATE(xrs), tolerance);
}

void
XrSetAlpha(XrState *xrs, double alpha)
{
    XrGStateSetAlpha(CURRENT_GSTATE(xrs), alpha);
}

void
XrSetLineWidth(XrState *xrs, double width)
{
    XrGStateSetLineWidth(CURRENT_GSTATE(xrs), width);
}

void
XrSetLineCap(XrState *xrs, XrLineCap line_cap)
{
    XrGStateSetLineCap(CURRENT_GSTATE(xrs), line_cap);
}

void
XrSetLineJoin(XrState *xrs, XrLineJoin line_join)
{
    XrGStateSetLineJoin(CURRENT_GSTATE(xrs), line_join);
}

void
XrSetDash(XrState *xrs, double *dashes, int ndash, double offset)
{
    XrError err;
    err = XrGStateSetDash(CURRENT_GSTATE(xrs), dashes, ndash, offset);
    if (err)
	xrs->error = err;
}

void
XrSetMiterLimit(XrState *xrs, double limit)
{
    XrGStateSetMiterLimit(CURRENT_GSTATE(xrs), limit);
}

void
XrTranslate(XrState *xrs, double tx, double ty)
{
    XrGStateTranslate(CURRENT_GSTATE(xrs), tx, ty);
}

void
XrScale(XrState *xrs, double sx, double sy)
{
    XrGStateScale(CURRENT_GSTATE(xrs), sx, sy);
}

void
XrRotate(XrState *xrs, double angle)
{
    XrGStateRotate(CURRENT_GSTATE(xrs), angle);
}

void
XrNewPath(XrState *xrs)
{
    XrGStateNewPath(CURRENT_GSTATE(xrs));
}

void
XrMoveTo(XrState *xrs, double x, double y)
{
    XrError err;

    err = XrGStateAddUnaryPathOp(CURRENT_GSTATE(xrs), XrPathOpMoveTo, x, y);
    if (err)
	xrs->error = err;
}

void
XrLineTo(XrState *xrs, double x, double y)
{
    XrError err;

    err = XrGStateAddUnaryPathOp(CURRENT_GSTATE(xrs), XrPathOpLineTo, x, y);
    if (err)
	xrs->error = err;
}

void
XrCurveTo(XrState *xrs,
	  double x1, double y1,
	  double x2, double y2,
	  double x3, double y3)
{
    XrError err;
    XPointDouble pt[3];

    pt[0].x = x1; pt[0].y = y1;
    pt[1].x = x2; pt[1].y = y2;
    pt[2].x = x3; pt[2].y = y3;
    
    err = XrGStateAddPathOp(CURRENT_GSTATE(xrs), XrPathOpCurveTo, pt, 3);
    if (err)
	xrs->error = err;
}

void
XrRelMoveTo(XrState *xrs, double x, double y)
{
    XrError err;

    err = XrGStateAddUnaryPathOp(CURRENT_GSTATE(xrs), XrPathOpRelMoveTo, x, y);
    if (err)
	xrs->error = err;
}

void
XrRelLineTo(XrState *xrs, double x, double y)
{
    XrError err;

    err = XrGStateAddUnaryPathOp(CURRENT_GSTATE(xrs), XrPathOpRelLineTo, x, y);
    if (err)
	xrs->error = err;
}

void
XrRelCurveTo(XrState *xrs,
	     double x1, double y1,
	     double x2, double y2,
	     double x3, double y3)
{
    XrError err;
    XPointDouble pt[3];

    pt[0].x = x1; pt[0].y = y1;
    pt[1].x = x2; pt[1].y = y2;
    pt[2].x = x3; pt[2].y = y3;

    err = XrGStateAddPathOp(CURRENT_GSTATE(xrs), XrPathOpRelCurveTo, pt, 3);
    if (err)
	xrs->error = err;
}

void
XrClosePath(XrState *xrs)
{
    XrError err;

    err = XrGStateClosePath(CURRENT_GSTATE(xrs));
    if (err)
	xrs->error = err;
}

void
XrStroke(XrState *xrs)
{
    XrError err;

    if (xrs->error)
	return;

    err = XrGStateStroke(CURRENT_GSTATE(xrs));
    if (err)
	xrs->error = err;
}

void
XrFill(XrState *xrs)
{
    XrError err;

    if (xrs->error)
	return;

    err = XrGStateFill(CURRENT_GSTATE(xrs));
    if (err) {
	xrs->error = err;
    }
}
