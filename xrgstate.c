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
_XrGStateStrokePath(XrGState *gstate, XrPath *path, XrPath *outline);

static void
_XrGStateStrokeSubPath(XrGState *gstate, XrSubPath *subpath, XrPath *outline);

static void
_XrGStateStrokeSegment(XrGState *gstate, const XPointDouble *p0, const XPointDouble *p1, XrPath *outline);

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

    gstate->op = XR_GSTATE_OP_DEFAULT;
    gstate->winding = XR_GSTATE_WINDING_DEFAULT;
    gstate->line_width = XR_GSTATE_LINE_WIDTH_DEFAULT;

    gstate->solidFormat = XRenderFindStandardFormat(dpy, PictStandardARGB32);
    gstate->alphaFormat = XRenderFindStandardFormat(dpy, PictStandardA8);

    XrPictureInit(&gstate->picture, dpy);

    XrPictureInit(&gstate->src, dpy);
    XrColorInit(&gstate->color);
    XrPictureSetSolidColor(&gstate->src, &gstate->color, gstate->solidFormat);

    XrTransformInit(&gstate->transform);

    XrPathInit(&gstate->path);
    XrPathInit(&gstate->outline);
}

void
XrGStateInitCopy(XrGState *gstate, XrGState *other)
{
    *gstate = *other;

    XrPathInitCopy(&gstate->path, &other->path);
    XrPathInitCopy(&gstate->outline, &other->outline);
}

void
XrGStateDeinit(XrGState *gstate)
{
    XrColorDeinit(&gstate->color);
    XrPictureDeinit(&gstate->src);
    XrPictureDeinit(&gstate->picture);
    XrTransformInit(&gstate->transform);

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
XrGStateSetDrawable(XrGState *gstate, Drawable drawable, Visual *visual)
{
    XrPictureSetDrawable(&gstate->picture, drawable, visual);
}

void
XrGStateSetColorRGB(XrGState *gstate, double red, double green, double blue)
{
    XrColorSetRGB(&gstate->color, red, green, blue);
    XrPictureSetSolidColor(&gstate->src, &gstate->color, gstate->solidFormat);
}

void
XrGStateSetAlpha(XrGState *gstate, double alpha)
{
    XrColorSetAlpha(&gstate->color, alpha);
    XrPictureSetSolidColor(&gstate->src, &gstate->color, gstate->solidFormat);
}

void
XrGStateSetLineWidth(XrGState *gstate, double width)
{
    gstate->line_width = width;
}

void
XrGStateTranslate(XrGState *gstate, double tx, double ty)
{
    XrTransform new;

    XrTransformInitTranslate(&new, tx, ty);
    XrTransformCompose(&gstate->transform, &new);
}

void
XrGStateScale(XrGState *gstate, double sx, double sy)
{
    XrTransform new;

    XrTransformInitScale(&new, sx, sy);
    XrTransformCompose(&gstate->transform, &new);
}

void
XrGStateRotate(XrGState *gstate, double angle)
{
    XrTransform new;

    XrTransformInitRotate(&new, angle);
    XrTransformCompose(&gstate->transform, &new);
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

    XrTransformPoint(&gstate->transform, &pt);
    XrPathMoveTo(&gstate->path, &pt);
}

void
XrGStateLineTo(XrGState *gstate, double x, double y)
{
    XPointDouble pt;

    pt.x = x;
    pt.y = y;

    XrTransformPoint(&gstate->transform, &pt);
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

    XrTransformPointWithoutTranslate(&gstate->transform, &pt);
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

    XrTransformPointWithoutTranslate(&gstate->transform, &pt);
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
    int winding_save = gstate->winding;

    gstate->winding = 1;
    XrPathInit(&gstate->outline);
    _XrGStateStrokePath(gstate, &gstate->path, &gstate->outline);
    _XrGStateFillPath(gstate, &gstate->outline);
    XrPathDeinit(&gstate->outline);
    gstate->winding = winding_save;
}

void
XrGStateFill(XrGState *gstate)
{
    _XrGStateFillPath(gstate, &gstate->path);
}

static void
_XrGStateStrokePath(XrGState *gstate, XrPath *path, XrPath *outline)
{
    XrSubPath *sub;

    for (sub = path->head; sub; sub = sub->next) {
	if (sub->num_pts) {
	    _XrGStateStrokeSubPath(gstate, sub, outline);
	}
    }
}

static void
_XrGStateStrokeSubPath(XrGState *gstate, XrSubPath *subpath, XrPath *outline)
{
    int i;
    XPointDouble *p0, *p1;

    XrPathNewSubPath(outline);

    /* Stroke right-side of path forward */
    for (i = 0; i < subpath->num_pts - 1; i++) {
	p0 = subpath->pts + i;
	p1 = p0 + 1;

	_XrGStateStrokeSegment(gstate, p0, p1, outline);
    }

    /* Close path or add cap as necessary */
    if (subpath->closed) {
	p0 = subpath->pts + subpath->num_pts - 1;
	p1 = subpath->pts;
	_XrGStateStrokeSegment(gstate, p0, p1, outline);
	XrPathClose(outline);
    } else {
	/* XXX: NYI: Add cap here */
    }

    /* Stroke right-side of path in reverse */
    for (i = subpath->num_pts - 1; i > 0; i--) {
	p0 = subpath->pts + i;
	p1 = p0 - 1;
	
	_XrGStateStrokeSegment(gstate, p0, p1, outline);
    }

    /* Close path or add cap as necessary */
    if (subpath->closed) {
	p0 = subpath->pts;
	p1 = subpath->pts + subpath->num_pts - 1;
	_XrGStateStrokeSegment(gstate, p0, p1, outline);
	XrPathClose(outline);
    } else {
	/* XXX: NYI: Add cap here */
    }
}

static void
_XrGStateStrokeSegment(XrGState *gstate, const XPointDouble *p0, const XPointDouble *p1, XrPath *outline)
{
    double dx, dy, mag;
    XPointDouble offset;
    XPointDouble p0_off = *p0;
    XPointDouble p1_off = *p1;

    dx = p1->x - p0->x;
    dy = p1->y - p0->y;
    mag = (gstate->line_width / 2) / sqrt(dx * dx + dy *dy);

    offset.x = -dy * mag;
    offset.y = dx * mag;

    XrTransformPointWithoutTranslate(&gstate->transform, &offset);

    _TranslatePoint(&p0_off, &offset);
    XrPathAddPoint(outline, &p0_off);

    _TranslatePoint(&p1_off, &offset);
    XrPathAddPoint(outline, &p1_off);
}

static void
_XrGStateFillPath(XrGState *gstate, XrPath *path)
{
    XPolygonDouble *polys;
    int i, npolys;
    XrSubPath *subpath;

    npolys = XrPathNumSubPaths(path);

    polys = malloc(npolys * sizeof(XPolygonDouble));
    if (polys == NULL) {
	return;
    }

    for (i=0, subpath = path->head; i < npolys && subpath; i++, subpath = subpath->next) {
	polys[i].points = subpath->pts;
	polys[i].npoints = subpath->num_pts;
    }

    XRenderCompositeDoublePolys(gstate->dpy, gstate->op,
			       gstate->src.picture, gstate->picture.picture,
			       gstate->alphaFormat,
			       0, 0, 0, 0,
			       polys, npolys,
			       gstate->winding);

    free(polys);
}
