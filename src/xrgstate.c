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
#include <math.h>

#include "xrint.h"

/* Private functions */
static XrGState *
_XrGStateAlloc(void);

static XrGState *
_XrGStateAlloc(void)
{
    return malloc(sizeof(XrGState));
}

XrGState *
XrGStateCreate(Display *dpy)
{
    XrGState *gstate;

    gstate = _XrGStateAlloc();
    XrGStateInit(gstate, dpy);

    return gstate;
}

void
XrGStateInit(XrGState *gstate, Display *dpy)
{
    gstate->dpy = dpy;

    gstate->operator = XR_GSTATE_OPERATOR_DEFAULT;

    gstate->fill_style.winding = XR_GSTATE_WINDING_DEFAULT;

    gstate->stroke_style.line_width = XR_GSTATE_LINE_WIDTH_DEFAULT;
    gstate->stroke_style.line_cap = XR_GSTATE_LINE_CAP_DEFAULT;
    gstate->stroke_style.line_join = XR_GSTATE_LINE_JOIN_DEFAULT;
    gstate->stroke_style.miter_limit = XR_GSTATE_MITER_LIMIT_DEFAULT;

    gstate->solidFormat = XcFindStandardFormat(dpy, PictStandardARGB32);
    gstate->alphaFormat = XcFindStandardFormat(dpy, PictStandardA8);

    XrSurfaceInit(&gstate->surface, dpy);

    XrSurfaceInit(&gstate->src, dpy);
    XrColorInit(&gstate->color);
    XrSurfaceSetSolidColor(&gstate->src, &gstate->color, gstate->solidFormat);

    XrTransformInit(&gstate->ctm);
    XrTransformInit(&gstate->ctm_inverse);

    XrPathInit(&gstate->path);
}

void
XrGStateInitCopy(XrGState *gstate, XrGState *other)
{
    *gstate = *other;

    XrSurfaceInit(&gstate->src, gstate->dpy);
    XrSurfaceSetSolidColor(&gstate->src, &gstate->color, gstate->solidFormat);

    XrPathInitCopy(&gstate->path, &other->path);
}

void
XrGStateDeinit(XrGState *gstate)
{
    XrColorDeinit(&gstate->color);
    XrSurfaceDeinit(&gstate->src);
    XrSurfaceDeinit(&gstate->surface);
    XrTransformDeinit(&gstate->ctm);
    XrTransformDeinit(&gstate->ctm_inverse);

    XrPathDeinit(&gstate->path);
}

void
XrGStateDestroy(XrGState *gstate)
{
    XrGStateDeinit(gstate);
    free(gstate);
}

XrGState*
XrGStateClone(XrGState *gstate)
{
    XrGState *clone;

    clone = _XrGStateAlloc();

    XrGStateInitCopy(clone, gstate);
    return clone;
}

void
XrGStateSetDrawable(XrGState *gstate, Drawable drawable)
{
    XrSurfaceSetDrawable(&gstate->surface, drawable);
}

void
XrGStateSetVisual(XrGState *gstate, Visual *visual)
{
    XrSurfaceSetVisual(&gstate->surface, visual);
}

void
XrGStateSetFormat(XrGState *gstate, XrFormat format)
{
    XrSurfaceSetFormat(&gstate->surface, format);
}

void
XrGStateSetOperator(XrGState *gstate, XrOperator operator)
{
    gstate->operator = operator;
}

void
XrGStateSetRGBColor(XrGState *gstate, double red, double green, double blue)
{
    XrColorSetRGB(&gstate->color, red, green, blue);
    XrSurfaceSetSolidColor(&gstate->src, &gstate->color, gstate->solidFormat);
}

void
XrGStateSetAlpha(XrGState *gstate, double alpha)
{
    XrColorSetAlpha(&gstate->color, alpha);
    XrSurfaceSetSolidColor(&gstate->src, &gstate->color, gstate->solidFormat);
}

void
XrGStateSetLineWidth(XrGState *gstate, double width)
{
    gstate->stroke_style.line_width = width;
}

void
XrGStateSetLineCap(XrGState *gstate, XrLineCap line_cap)
{
    gstate->stroke_style.line_cap = line_cap;
}

void
XrGStateSetLineJoin(XrGState *gstate, XrLineJoin line_join)
{
    gstate->stroke_style.line_join = line_join;
}

void
XrGStateSetMiterLimit(XrGState *gstate, double limit)
{
    gstate->stroke_style.miter_limit = limit;
}

void
XrGStateTranslate(XrGState *gstate, double tx, double ty)
{
    XrTransform tmp;

    XrTransformInitTranslate(&tmp, tx, ty);
    XrTransformMultiplyIntoRight(&tmp, &gstate->ctm);

    XrTransformInitTranslate(&tmp, -tx, -ty);
    XrTransformMultiplyIntoLeft(&gstate->ctm_inverse, &tmp);
}

void
XrGStateScale(XrGState *gstate, double sx, double sy)
{
    XrTransform tmp;

    XrTransformInitScale(&tmp, sx, sy);
    XrTransformMultiplyIntoRight(&tmp, &gstate->ctm);

    XrTransformInitScale(&tmp, -sx, -sy);
    XrTransformMultiplyIntoLeft(&gstate->ctm_inverse, &tmp);
}

void
XrGStateRotate(XrGState *gstate, double angle)
{
    XrTransform tmp;

    XrTransformInitRotate(&tmp, angle);
    XrTransformMultiplyIntoRight(&tmp, &gstate->ctm);

    XrTransformInitRotate(&tmp, -angle);
    XrTransformMultiplyIntoLeft(&gstate->ctm_inverse, &tmp);
}

void
XrGStateNewPath(XrGState *gstate)
{
    XrPathDeinit(&gstate->path);
}

void
XrGStateAddUnaryPathOp(XrGState *gstate, XrPathOp op, double x, double y)
{
    XPointDouble pt;
    XPointFixed pt_fixed;

    pt.x = x;
    pt.y = y;

    switch (op) {
    case XrPathOpMoveTo:
    case XrPathOpLineTo:
	XrTransformPoint(&gstate->ctm, &pt);
	break;
    case XrPathOpRelMoveTo:
    case XrPathOpRelLineTo:
	XrTransformPointWithoutTranslate(&gstate->ctm, &pt);
	break;
    default:
	/* Invalid */
	return;
    }

    pt_fixed.x = XDoubleToFixed(pt.x);
    pt_fixed.y = XDoubleToFixed(pt.y);

    XrPathAdd(&gstate->path, op, &pt_fixed, 1);
}

void
XrGStateClosePath(XrGState *gstate)
{
    XrPathAdd(&gstate->path, XrPathOpClosePath, NULL, 0);
}

void
XrGStateStroke(XrGState *gstate)
{
    static XrPathCallbacks cb = { XrStrokerAddEdge, XrStrokerDoneSubPath };

    XrStroker stroker;
    XrTraps traps;

    XrStrokerInit(&stroker, gstate, &traps);
    XrTrapsInit(&traps);

    XrPathInterpret(&gstate->path, XrPathDirectionForward, &cb, &stroker);

    XcCompositeTrapezoids(gstate->dpy, gstate->operator,
			  gstate->src.xcsurface, gstate->surface.xcsurface,
			  gstate->alphaFormat,
			  0, 0,
			  traps.xtraps,
			  traps.num_xtraps);

    XrTrapsDeinit(&traps);
    XrStrokerDeinit(&stroker);

    XrGStateNewPath(gstate);
}

void
XrGStateFill(XrGState *gstate)
{
    static XrPathCallbacks cb = { XrPolygonAddEdge, XrPolygonDoneSubPath };

    XrPolygon polygon;
    XrTraps traps;

    XrPolygonInit(&polygon);
    XrTrapsInit(&traps);

    XrPathInterpret(&gstate->path, XrPathDirectionForward, &cb, &polygon);
    XrTrapsTessellatePolygon(&traps, &polygon, gstate->fill_style.winding);

    XcCompositeTrapezoids(gstate->dpy, gstate->operator,
			  gstate->src.xcsurface, gstate->surface.xcsurface,
			  gstate->alphaFormat,
			  0, 0,
			  traps.xtraps,
			  traps.num_xtraps);

    XrTrapsDeinit(&traps);
    XrPolygonDeinit(&polygon);

    XrGStateNewPath(gstate);
}

