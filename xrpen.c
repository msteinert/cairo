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

static int
_XrPenVerticesNeeded(double radius, double tolerance, XrTransform *matrix);

static void
_XrPenComputeSlopes(XrPen *pen);

static void
_FindSlope(XPointFixed *a, XPointFixed *b, XrSlopeFixed *slope);

static int
_SlopeClockwise(XrSlopeFixed *a, XrSlopeFixed *b);

static int
_SlopeCounterClockwise(XrSlopeFixed *a, XrSlopeFixed *b);

static XrError
_XrPenFindHull(XrPen *pen, XPointFixed *pt, int num_pts, XrPenVertexTag dir, XrPolygon *polygon);

static int
_XrPenVertexCompareByTheta(const void *a, const void *b);

XrError
XrPenInitEmpty(XrPen *pen)
{
    pen->radius = 0;
    pen->tolerance = 0;
    pen->vertex = NULL;
    pen->num_vertices = 0;

    return XrErrorSuccess;
}

XrError
XrPenInit(XrPen *pen, double radius, XrGState *gstate)
{
    int i;
    XrPenVertex *v;
    XPointDouble pt;

    if (pen->num_vertices) {
	/* XXX: It would be nice to notice that the pen is already properly constructed.
	   However, this test would also have to account for possible changes in the transformation
	   matrix.
	if (pen->radius == radius && pen->tolerance == tolerance)
	    return XrErrorSuccess;
	*/
	XrPenDeinit(pen);
    }

    pen->radius = radius;
    pen->tolerance = gstate->tolerance;

    pen->num_vertices = _XrPenVerticesNeeded(radius, gstate->tolerance, &gstate->ctm);

    pen->vertex = malloc(pen->num_vertices * sizeof(XrPenVertex));
    if (pen->vertex == NULL) {
	return XrErrorNoMemory;
    }

    for (i=0; i < pen->num_vertices; i++) {
	v = &pen->vertex[i];
	v->theta = 2 * M_PI * i / (double) pen->num_vertices;
	pt.x = radius * cos(v->theta);
	pt.y = radius * sin(v->theta);
	XrTransformPointWithoutTranslate(&gstate->ctm, &pt);
	v->pt.x = XDoubleToFixed(pt.x);
	v->pt.y = XDoubleToFixed(pt.y);
	v->tag = XrPenVertexTagNone;
    }

    _XrPenComputeSlopes(pen);

    return XrErrorSuccess;
}

void
XrPenDeinit(XrPen *pen)
{
    free(pen->vertex);
    XrPenInitEmpty(pen);
}

XrError
XrPenInitCopy(XrPen *pen, XrPen *other)
{
    *pen = *other;

    pen->vertex = malloc(pen->num_vertices * sizeof(XrPenVertex));
    if (pen->vertex == NULL) {
	return XrErrorNoMemory;
    }

    memcpy(pen->vertex, other->vertex, pen->num_vertices * sizeof(XrPenVertex));

    return XrErrorSuccess;
}

static int
_XrPenVertexCompareByTheta(const void *a, const void *b)
{
    double diff;
    const XrPenVertex *va = a;
    const XrPenVertex *vb = b;

    diff = va->theta - vb->theta;
    if (diff < 0)
	return -1;
    else if (diff > 0)
	return 1;
    else
	return 0;
}

XrError
XrPenAddPoints(XrPen *pen, XrPenTaggedPoint *pt, int num_pts)
{
    int i, j;
    XrPenVertex *v, *new_vertex;
    XrPenVertex *vi, *pi;

    v = malloc(num_pts * sizeof(XrPenVertex));
    if (v == NULL)
	return XrErrorNoMemory;

    pen->num_vertices += num_pts;
    new_vertex = realloc(pen->vertex, pen->num_vertices * sizeof(XrPenVertex));
    if (new_vertex == NULL) {
	free(v);
	pen->num_vertices -= num_pts;
	return XrErrorNoMemory;
    }
    pen->vertex = new_vertex;

    /* initialize and sort new vertices */
    for (i=0; i < num_pts; i++) {
	v[i].pt.x = pt[i].pt.x;
	v[i].pt.y = pt[i].pt.y;
	v[i].tag = pt[i].tag;

	v[i].theta = atan2(v[i].pt.y, v[i].pt.x);
	if (v[i].theta < 0)
	    v[i].theta += 2 * M_PI;
    }

    qsort(v, num_pts, sizeof(XrPenVertex), _XrPenVertexCompareByTheta);

    /* merge new vertices into original */
    pi = pen->vertex + pen->num_vertices - num_pts - 1;
    vi = v + num_pts - 1;
    for (i = pen->num_vertices - 1; vi >= v; i--) {
	if (pi >= pen->vertex
	    && vi->pt.x == pi->pt.x && vi->pt.y == pi->pt.y) {
	    /* Eliminate the duplicate vertex */
	    for (j=i; j < pen->num_vertices - 1; j++)
		pen->vertex[j] = pen->vertex[j+1];
	    pen->vertex[--i] = *vi;
	    pen->num_vertices--;
	    pi--;
	    vi--;
	} else if (pi < pen->vertex || vi->theta >= pi->theta) {
	    pen->vertex[i] = *vi--;
	} else {
	    pen->vertex[i] = *pi--;
	}
    }

    free(v);

    _XrPenComputeSlopes(pen);

    return XrErrorSuccess;
}

static int
_XrPenVerticesNeeded(double radius, double tolerance, XrTransform *matrix)
{
    double e1, e2, emax, theta;

    if (tolerance > radius) {
	return 4;
    } 

    XrTransformEigenValues(matrix, &e1, &e2);

    if (fabs(e1) > fabs(e2))
	emax = fabs(e1);
    else
	emax = fabs(e2);

    theta = acos(1 - tolerance/(emax * radius));
    return ceil(M_PI / theta);
}

static void
_XrPenComputeSlopes(XrPen *pen)
{
    int i, i_prev;
    XrPenVertex *prev, *v, *next;

    for (i=0, i_prev = pen->num_vertices - 1;
	 i < pen->num_vertices;
	 i_prev = i++) {
	prev = &pen->vertex[i_prev];
	v = &pen->vertex[i];
	next = &pen->vertex[(i + 1) % pen->num_vertices];

	_FindSlope(&prev->pt, &v->pt, &v->slope_cw);
	_FindSlope(&v->pt, &next->pt, &v->slope_ccw);
    }
}

static void
_FindSlope(XPointFixed *a, XPointFixed *b, XrSlopeFixed *slope)
{
    slope->dx = b->x - a->x;
    slope->dy = b->y - a->y;
}

static int
_SlopeClockwise(XrSlopeFixed *a, XrSlopeFixed *b)
{
    double a_dx = XFixedToDouble(a->dx);
    double a_dy = XFixedToDouble(a->dy);
    double b_dx = XFixedToDouble(b->dx);
    double b_dy = XFixedToDouble(b->dy);

    return b_dy * a_dx > a_dy * b_dx;
}

static int
_SlopeCounterClockwise(XrSlopeFixed *a, XrSlopeFixed *b)
{
    return ! _SlopeClockwise(a, b);
}

static XrError
_XrPenFindHull(XrPen *pen, XPointFixed *pt, int num_pts, XrPenVertexTag dir, XrPolygon *polygon)
{
    int i;
    XrError err;
    int start, stop, step;
    int active = 0;
    XPointFixed hull_pt;
    XrSlopeFixed exit_slope;

    for (i=0; i < pen->num_vertices; i++) {
	if (pen->vertex[i].tag == dir) {
	    active = i;
	    break;
	}
    }

    if (dir == XrPenVertexTagForward) {
	start = 0;
	stop = num_pts - 1;
	step = 1;
    } else {
	start = num_pts - 1;
	stop = 0;
	step = -1;
    }

    i = start;
    while (i != stop) {
	hull_pt.x = pt[i].x + pen->vertex[active].pt.x;
	hull_pt.y = pt[i].y + pen->vertex[active].pt.y;
	err = XrPolygonAddPoint(polygon, &hull_pt);
	if (err)
	    return err;

	_FindSlope(&pt[i], &pt[i+step], &exit_slope);
	if (_SlopeCounterClockwise(&exit_slope, &pen->vertex[active].slope_ccw)) {
	    active = active + 1;
	    if (active == pen->num_vertices)
		active = 0;
	} else if (_SlopeClockwise(&exit_slope, &pen->vertex[active].slope_cw)) {
	    active = active - 1;
	    if (active == -1)
		active = pen->num_vertices - 1;
	} else {
	    i += step;
	}
    }

    hull_pt.x = pt[i].x + pen->vertex[active].pt.x;
    hull_pt.y = pt[i].y + pen->vertex[active].pt.y;
    return XrPolygonAddPoint(polygon, &hull_pt);
}

/* Compute convex hull formed by placing a copy of the pen at each of
   the given points. The hull is stored in the provided polygon. */
XrError
XrPenStrokePoints(XrPen *pen, XPointFixed *pt, int num_pts, XrPolygon *polygon)
{
    XrError err;

    err = _XrPenFindHull(pen, pt, num_pts, XrPenVertexTagForward, polygon);
    if (err)
	return err;

    err = _XrPenFindHull(pen, pt, num_pts, XrPenVertexTagReverse, polygon);
    if (err)
	return err;

    XrPolygonClose(polygon);

    return XrErrorSuccess;
}
