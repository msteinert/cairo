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

#include <stdlib.h>
#include <math.h>

#include "xrint.h"

/* Private functions */
static XrGState *
_XrGStateAlloc(void);

static void
_TranslatePoint(XPointDouble *pt, const XPointDouble *offset);

static void
_XrGStateStrokePath(XrGState *gstate, XrPath *path, XrTraps *traps);

static void
_XrGStateStrokeSubPath(XrGState *gstate, XrSubPath *subpath, XrTraps *traps);

static void
_XrGStateStrokeCap(XrGState *gstate,
		   const XPointDouble *p0, const XPointDouble *p1,
		   XrTraps *traps);

static void
_XrGStateStrokeJoin(XrGState *gstate,
		    const XPointDouble *p0, const XPointDouble *p1, const XPointDouble *p2,
		    XrTraps *traps);

static void
_XrGStateStrokeSegment(XrGState *gstate,
		       const XPointDouble *p0, const XPointDouble *p1,
		       XrTraps *traps);

static void
_XrGStateFillPath(XrGState *gstate, XrPath *path);

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
    gstate->winding = XR_GSTATE_WINDING_DEFAULT;
    gstate->line_width = XR_GSTATE_LINE_WIDTH_DEFAULT;
    gstate->line_cap = XR_GSTATE_LINE_CAP_DEFAULT;
    gstate->line_join = XR_GSTATE_LINE_JOIN_DEFAULT;
    gstate->miter_limit = XR_GSTATE_MITER_LIMIT_DEFAULT;

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

XrGState *
XrGStateClone(XrGState *gstate)
{
    XrGState *clone;

    clone = _XrGStateAlloc();

    XrGStateInitCopy(clone, gstate);
    return clone;
}

void
XrGStateGetCurrentPoint(XrGState *gstate, XPointDouble *pt)
{
    XrPathGetCurrentPoint(&gstate->path, pt);
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
    gstate->line_width = width;
}

void
XrGStateSetLineCap(XrGState *gstate, XrLineCap line_cap)
{
    gstate->line_cap = line_cap;
}

void
XrGStateSetLineJoin(XrGState *gstate, XrLineJoin line_join)
{
    gstate->line_join = line_join;
}

void
XrGStateSetMiterLimit(XrGState *gstate, double limit)
{
    gstate->miter_limit = limit;
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
XrGStateMoveTo(XrGState *gstate, double x, double y)
{
    XPointDouble pt;

    pt.x = x;
    pt.y = y;

    XrTransformPoint(&gstate->ctm, &pt);
    XrPathMoveTo(&gstate->path, &pt);
}

void
XrGStateLineTo(XrGState *gstate, double x, double y)
{
    XPointDouble pt;

    pt.x = x;
    pt.y = y;

    XrTransformPoint(&gstate->ctm, &pt);
    XrPathLineTo(&gstate->path, &pt);
}

static void
_TranslatePoint(XPointDouble *pt, const XPointDouble *offset)
{
    pt->x += offset->x;
    pt->y += offset->y;
}

void
XrGStateRelMoveTo(XrGState *gstate, double x, double y)
{
    XPointDouble pt, current;

    pt.x = x;
    pt.y = y;

    XrTransformPointWithoutTranslate(&gstate->ctm, &pt);
    XrGStateGetCurrentPoint(gstate, &current);
    _TranslatePoint(&pt, &current);
    XrPathMoveTo(&gstate->path, &pt);
}

void
XrGStateRelLineTo(XrGState *gstate, double x, double y)
{
    XPointDouble pt, current;

    pt.x = x;
    pt.y = y;

    XrTransformPointWithoutTranslate(&gstate->ctm, &pt);
    XrGStateGetCurrentPoint(gstate, &current);
    _TranslatePoint(&pt, &current);
    XrPathLineTo(&gstate->path, &pt);
}

void
XrGStateClosePath(XrGState *gstate)
{
    XrPathClose(&gstate->path);
}

void
XrGStateStroke(XrGState *gstate)
{
    XrTraps traps;

    XrTrapsInit(&traps);

    _XrGStateStrokePath(gstate, &gstate->path, &traps);

    XcCompositeTrapezoids(gstate->dpy, gstate->operator,
			  gstate->src.xcsurface, gstate->surface.xcsurface,
			  gstate->alphaFormat,
			  0, 0,
			  traps.xtraps,
			  traps.num_xtraps);

    XrTrapsDeinit(&traps);
    
    XrGStateNewPath(gstate);
}

void
XrGStateFill(XrGState *gstate)
{
    _XrGStateFillPath(gstate, &gstate->path);

    XrGStateNewPath(gstate);
}

static void
_XrGStateStrokePath(XrGState *gstate, XrPath *path, XrTraps *traps)
{
    XrSubPath *subpath;

    for (subpath = path->head; subpath; subpath = subpath->next) {
	if (subpath->num_pts) {
	    _XrGStateStrokeSubPath(gstate, subpath, traps);
	}
    }
}

static void
_XrGStateStrokeSubPath(XrGState *gstate, XrSubPath *subpath, XrTraps *traps)
{
    int i;
    XPointDouble *pt_prev, *pt, *pt_next;

    /* XXX: BUG: Need to consider degenerate paths here, (all paths
       less then 3 points may need special consideration) */

    /* Stroke initial cap or join */
    pt_prev = subpath->pts + subpath->num_pts - 1;
    pt = subpath->pts;
    pt_next = pt + 1;
    if (subpath->closed) {
	_XrGStateStrokeJoin(gstate, pt_prev, pt, pt_next, traps);
    } else {
	_XrGStateStrokeCap(gstate, pt_next, pt, traps);
    }
    _XrGStateStrokeSegment(gstate, pt, pt_next, traps);

    /* Stroke path segments */
    for (i = 1; i < subpath->num_pts - 1; i++) {
	pt_prev = pt;
	pt = pt_next;
	pt_next++;

	_XrGStateStrokeJoin(gstate, pt_prev, pt, pt_next, traps);
	_XrGStateStrokeSegment(gstate, pt, pt_next, traps);
    }

    /* Close path or add final cap as necessary */
    pt_prev = pt;
    pt = pt_next;
    pt_next = subpath->pts;
    if (subpath->closed) {
	_XrGStateStrokeJoin(gstate, pt_prev, pt, pt_next, traps);
	_XrGStateStrokeSegment(gstate, pt, pt_next, traps);
    } else {
	_XrGStateStrokeCap(gstate, pt_prev, pt, traps);
    }

}

static void
_XrGStateStrokeCap(XrGState *gstate,
		   const XPointDouble *p0, const XPointDouble *p1,
		   XrTraps *traps)
{
    switch (gstate->line_cap) {
    case XrLineCapRound:
	/* XXX: NYI */
	break;
    case XrLineCapSquare:
	/* XXX: NYI */
	break;
    case XrLineCapButt:
    default:
	/* XXX: NYI */
	break;
    }
}

static void
_XrGStateStrokeJoin(XrGState *gstate,
		    const XPointDouble *p0, const XPointDouble *p1, const XPointDouble *p2,
		    XrTraps *traps)
{
    switch (gstate->line_join) {
    case XrLineJoinMiter:
	/* XXX: NYI */
	break;
    case XrLineJoinRound:
	/* XXX: NYI */
	break;
    case XrLineJoinBevel:
    default:
	/* XXX: NYI */
	break;
    }
}

static void
_XrGStateStrokeSegment(XrGState *gstate,
		       const XPointDouble *p0, const XPointDouble *p1,
		       XrTraps *traps)
{
    double mag, tmp;
    XPointDouble offset;
    XPointDouble quad[4];

    offset.x = p1->x - p0->x;
    offset.y = p1->y - p0->y;

    mag = sqrt(offset.x * offset.x + offset.y * offset.y);
    if (mag == 0) {
	return;
    }

    offset.x /= mag;
    offset.y /= mag;

    XrTransformPointWithoutTranslate(&gstate->ctm_inverse, &offset);

    tmp = offset.x;
    offset.x = offset.y * (gstate->line_width / 2.0);
    offset.y = - tmp * (gstate->line_width / 2.0);

    XrTransformPointWithoutTranslate(&gstate->ctm, &offset);

    quad[0] = *p0;
    _TranslatePoint(&quad[0], &offset);
    quad[1] = *p1;
    _TranslatePoint(&quad[1], &offset);

    offset.x = - offset.x;
    offset.y = - offset.y;

    quad[2] = *p1;
    _TranslatePoint(&quad[2], &offset);
    quad[3] = *p0;
    _TranslatePoint(&quad[3], &offset);

    XrTrapsTessellateConvexQuad(traps, quad);
}

static void
_XrGStateFillPath(XrGState *gstate, XrPath *path)
{
    XrTraps traps;

    XrTrapsInit(&traps);

    XrTrapsTessellatePath(&traps, path, gstate->winding);
    XcCompositeTrapezoids(gstate->dpy, gstate->operator,
			  gstate->src.xcsurface, gstate->surface.xcsurface,
			  gstate->alphaFormat,
			  0, 0,
			  traps.xtraps,
			  traps.num_xtraps);

    XrTrapsDeinit(&traps);
}


