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

#define _XR_CURRENT_GSTATE(xrs) (xrs->stack)

#define XR_TOLERANCE_MINIMUM	0.0002 /* We're limited by 16 bits of sub-pixel precision */

static void
_XrClipValue(double *value, double min, double max);

XrState *
XrCreate(Display *dpy)
{
    return _XrStateCreate(dpy);
}

void
XrDestroy(XrState *xrs)
{
    _XrStateDestroy(xrs);
}

void
XrSave(XrState *xrs)
{
    XrStatus status;

    status = _XrStatePush(xrs);
    if (status)
	xrs->status = status;
}

void
XrRestore(XrState *xrs)
{
    /* XXX: BUG: Calling XrRestore without a matching XrSave shoud
       flag an error. Also, in order to prevent crashes, XrStatePop
       should not be called in that case. */
    _XrStatePop(xrs);
}

void
XrSetDrawable(XrState *xrs, Drawable drawable)
{
    _XrGStateSetDrawable(_XR_CURRENT_GSTATE(xrs), drawable);
}

void
XrSetVisual(XrState *xrs, Visual *visual)
{
    _XrGStateSetVisual(_XR_CURRENT_GSTATE(xrs), visual);
}

void
XrSetFormat(XrState *xrs, XrFormat format)
{
    _XrGStateSetFormat(_XR_CURRENT_GSTATE(xrs), format);
}

void
XrSetOperator(XrState *xrs, XrOperator operator)
{
    _XrGStateSetOperator(_XR_CURRENT_GSTATE(xrs), operator);
}

void
XrSetRGBColor(XrState *xrs, double red, double green, double blue)
{
    _XrClipValue(&red, 0.0, 1.0);
    _XrClipValue(&green, 0.0, 1.0);
    _XrClipValue(&blue, 0.0, 1.0);

    _XrGStateSetRGBColor(_XR_CURRENT_GSTATE(xrs), red, green, blue);
}

void
XrSetTolerance(XrState *xrs, double tolerance)
{
    _XrClipValue(&tolerance, XR_TOLERANCE_MINIMUM, tolerance);

    _XrGStateSetTolerance(_XR_CURRENT_GSTATE(xrs), tolerance);
}

void
XrSetAlpha(XrState *xrs, double alpha)
{
    _XrClipValue(&alpha, 0.0, 1.0);

    _XrGStateSetAlpha(_XR_CURRENT_GSTATE(xrs), alpha);
}

void
XrSetFillRule(XrState *xrs, XrFillRule fill_rule)
{
    _XrGStateSetFillRule(_XR_CURRENT_GSTATE(xrs), fill_rule);
}

void
XrSetLineWidth(XrState *xrs, double width)
{
    _XrGStateSetLineWidth(_XR_CURRENT_GSTATE(xrs), width);
}

void
XrSetLineCap(XrState *xrs, XrLineCap line_cap)
{
    _XrGStateSetLineCap(_XR_CURRENT_GSTATE(xrs), line_cap);
}

void
XrSetLineJoin(XrState *xrs, XrLineJoin line_join)
{
    _XrGStateSetLineJoin(_XR_CURRENT_GSTATE(xrs), line_join);
}

void
XrSetDash(XrState *xrs, double *dashes, int ndash, double offset)
{
    XrStatus status;
    status = _XrGStateSetDash(_XR_CURRENT_GSTATE(xrs), dashes, ndash, offset);
    if (status)
	xrs->status = status;
}

void
XrSetMiterLimit(XrState *xrs, double limit)
{
    _XrGStateSetMiterLimit(_XR_CURRENT_GSTATE(xrs), limit);
}

void
XrTranslate(XrState *xrs, double tx, double ty)
{
    _XrGStateTranslate(_XR_CURRENT_GSTATE(xrs), tx, ty);
}

void
XrScale(XrState *xrs, double sx, double sy)
{
    _XrGStateScale(_XR_CURRENT_GSTATE(xrs), sx, sy);
}

void
XrRotate(XrState *xrs, double angle)
{
    _XrGStateRotate(_XR_CURRENT_GSTATE(xrs), angle);
}

void
XrConcatMatrix(XrState *xrs,
	       double a, double b,
	       double c, double d,
	       double tx, double ty)
{
    _XrGStateConcatMatrix(_XR_CURRENT_GSTATE(xrs), a, b, c, d, tx, ty);
}

void
XrNewPath(XrState *xrs)
{
    _XrGStateNewPath(_XR_CURRENT_GSTATE(xrs));
}

void
XrMoveTo(XrState *xrs, double x, double y)
{
    XrStatus status;

    status = _XrGStateAddUnaryPathOp(_XR_CURRENT_GSTATE(xrs), XrPathOpMoveTo, x, y);
    if (status)
	xrs->status = status;
}

void
XrLineTo(XrState *xrs, double x, double y)
{
    XrStatus status;

    status = _XrGStateAddUnaryPathOp(_XR_CURRENT_GSTATE(xrs), XrPathOpLineTo, x, y);
    if (status)
	xrs->status = status;
}

void
XrCurveTo(XrState *xrs,
	  double x1, double y1,
	  double x2, double y2,
	  double x3, double y3)
{
    XrStatus status;
    XPointDouble pt[3];

    pt[0].x = x1; pt[0].y = y1;
    pt[1].x = x2; pt[1].y = y2;
    pt[2].x = x3; pt[2].y = y3;
    
    status = _XrGStateAddPathOp(_XR_CURRENT_GSTATE(xrs), XrPathOpCurveTo, pt, 3);
    if (status)
	xrs->status = status;
}

void
XrRelMoveTo(XrState *xrs, double dx, double dy)
{
    XrStatus status;

    status = _XrGStateAddUnaryPathOp(_XR_CURRENT_GSTATE(xrs), XrPathOpRelMoveTo, dx, dy);
    if (status)
	xrs->status = status;
}

void
XrRelLineTo(XrState *xrs, double dx, double dy)
{
    XrStatus status;

    status = _XrGStateAddUnaryPathOp(_XR_CURRENT_GSTATE(xrs), XrPathOpRelLineTo, dx, dy);
    if (status)
	xrs->status = status;
}

void
XrRelCurveTo(XrState *xrs,
	     double dx1, double dy1,
	     double dx2, double dy2,
	     double dx3, double dy3)
{
    XrStatus status;
    XPointDouble pt[3];

    pt[0].x = dx1; pt[0].y = dy1;
    pt[1].x = dx2; pt[1].y = dy2;
    pt[2].x = dx3; pt[2].y = dy3;

    status = _XrGStateAddPathOp(_XR_CURRENT_GSTATE(xrs), XrPathOpRelCurveTo, pt, 3);
    if (status)
	xrs->status = status;
}

void
XrClosePath(XrState *xrs)
{
    XrStatus status;

    status = _XrGStateClosePath(_XR_CURRENT_GSTATE(xrs));
    if (status)
	xrs->status = status;
}

void
XrStroke(XrState *xrs)
{
    XrStatus status;

    if (xrs->status)
	return;

    status = _XrGStateStroke(_XR_CURRENT_GSTATE(xrs));
    if (status)
	xrs->status = status;
}

void
XrFill(XrState *xrs)
{
    XrStatus status;

    if (xrs->status)
	return;

    status = _XrGStateFill(_XR_CURRENT_GSTATE(xrs));
    if (status) {
	xrs->status = status;
    }
}

XrStatus
XrGetStatus(XrState *xrs)
{
    return xrs->status;
}

static void
_XrClipValue(double *value, double min, double max)
{
    if (*value < min)
	*value = min;
    else if (*value > max)
	*value = max;
}
