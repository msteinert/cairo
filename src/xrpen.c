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

static int
_SlopeClockwise(XrSlopeFixed *a, XrSlopeFixed *b);

static int
_SlopeCounterClockwise(XrSlopeFixed *a, XrSlopeFixed *b);

static int
_XrPenVertexCompareByTheta(const void *a, const void *b);

static XrStatus
_XrPenStrokeSplineHalf(XrPen *pen, XrSpline *spline,
		       XrPenVertexFlag dir, XrPolygon *polygon);

XrStatus
_XrPenInitEmpty(XrPen *pen)
{
    pen->radius = 0;
    pen->tolerance = 0;
    pen->vertex = NULL;
    pen->num_vertices = 0;

    return XrStatusSuccess;
}

XrStatus
_XrPenInit(XrPen *pen, double radius, XrGState *gstate)
{
    int i;
    XrPenVertex *v;
    double dx, dy;

    if (pen->num_vertices) {
	/* XXX: It would be nice to notice that the pen is already properly constructed.
	   However, this test would also have to account for possible changes in the transformation
	   matrix.
	   if (pen->radius == radius && pen->tolerance == tolerance)
	   return XrStatusSuccess;
	*/
	_XrPenDeinit(pen);
    }

    pen->radius = radius;
    pen->tolerance = gstate->tolerance;

    pen->num_vertices = _XrPenVerticesNeeded(radius, gstate->tolerance, &gstate->ctm);
    /* number of vertices must be even */
    if (pen->num_vertices % 2)
	pen->num_vertices++;

    pen->vertex = malloc(pen->num_vertices * sizeof(XrPenVertex));
    if (pen->vertex == NULL) {
	return XrStatusNoMemory;
    }

    for (i=0; i < pen->num_vertices; i++) {
	v = &pen->vertex[i];
	v->theta = 2 * M_PI * i / (double) pen->num_vertices;
	dx = radius * cos(v->theta);
	dy = radius * sin(v->theta);
	_XrTransformDistance(&gstate->ctm, &dx, &dy);
	v->pt.x = XDoubleToFixed(dx);
	v->pt.y = XDoubleToFixed(dy);
	v->flag = XrPenVertexFlagNone;
    }

    _XrPenComputeSlopes(pen);

    return XrStatusSuccess;
}

void
_XrPenDeinit(XrPen *pen)
{
    free(pen->vertex);
    _XrPenInitEmpty(pen);
}

XrStatus
_XrPenInitCopy(XrPen *pen, XrPen *other)
{
    *pen = *other;

    if (pen->num_vertices) {
	pen->vertex = malloc(pen->num_vertices * sizeof(XrPenVertex));
	if (pen->vertex == NULL) {
	    return XrStatusNoMemory;
	}
	memcpy(pen->vertex, other->vertex, pen->num_vertices * sizeof(XrPenVertex));
    }

    return XrStatusSuccess;
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

XrStatus
_XrPenAddPoints(XrPen *pen, XrPenFlaggedPoint *pt, int num_pts)
{
    int i, j;
    XrPenVertex *v, *v_next, *new_vertex;

    pen->num_vertices += num_pts;
    new_vertex = realloc(pen->vertex, pen->num_vertices * sizeof(XrPenVertex));
    if (new_vertex == NULL) {
	pen->num_vertices -= num_pts;
	return XrStatusNoMemory;
    }
    pen->vertex = new_vertex;

    /* initialize new vertices */
    for (i=0; i < num_pts; i++) {
	v = &pen->vertex[pen->num_vertices-(i+1)];
	v->pt = pt[i].pt;
	v->flag = pt[i].flag;
	v->theta = atan2(v->pt.y, v->pt.x);
	if (v->theta < 0)
	    v->theta += 2 * M_PI;
    }

    qsort(pen->vertex, pen->num_vertices, sizeof(XrPenVertex), _XrPenVertexCompareByTheta);

    /* eliminate any duplicate vertices */
    for (i=0; i < pen->num_vertices - 1; i++ ) {
	v = &pen->vertex[i];
	v_next = &pen->vertex[i+1];
	if (v->pt.x == v_next->pt.x && v->pt.y == v_next->pt.y) {
	    v->flag |= v_next->flag;
	    for (j=i+1; j < pen->num_vertices - 1; j++)
		pen->vertex[j] = pen->vertex[j+1];
	    pen->num_vertices--;
	    /* There may be more of the same duplicate, check again */
	    i--;
	}
    }

    _XrPenComputeSlopes(pen);

    return XrStatusSuccess;
}

static int
_XrPenVerticesNeeded(double radius, double tolerance, XrTransform *matrix)
{
    double e1, e2, emax, theta;

    _XrTransformEigenValues(matrix, &e1, &e2);

    if (fabs(e1) > fabs(e2))
	emax = fabs(e1);
    else
	emax = fabs(e2);

    if (tolerance > emax*radius) {
	return 4;
    }

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

	_ComputeSlope(&prev->pt, &v->pt, &v->slope_cw);
	_ComputeSlope(&v->pt, &next->pt, &v->slope_ccw);
    }
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

static XrStatus
_XrPenStrokeSplineHalf(XrPen *pen, XrSpline *spline,
		       XrPenVertexFlag dir, XrPolygon *polygon)
{
    int i;
    XrStatus status;
    int start, stop, step;
    int active = 0;
    XPointFixed hull_pt;
    XrSlopeFixed slope, final_slope;
    XPointFixed *pt = spline->pts;
    int num_pts = spline->num_pts;

    for (i=0; i < pen->num_vertices; i++) {
	if (pen->vertex[i].flag & dir) {
	    active = i;
	    break;
	}
    }

    if (dir == XrPenVertexFlagForward) {
	start = 0;
	stop = num_pts;
	step = 1;
	final_slope = spline->final_slope;
    } else {
	start = num_pts - 1;
	stop = -1;
	step = -1;
	final_slope = spline->initial_slope;
	final_slope.dx = -final_slope.dx; 
	final_slope.dy = -final_slope.dy; 
    }

    i = start;
    while (i != stop) {
	hull_pt.x = pt[i].x + pen->vertex[active].pt.x;
	hull_pt.y = pt[i].y + pen->vertex[active].pt.y;
	status = _XrPolygonAddPoint(polygon, &hull_pt);
	if (status)
	    return status;

	if (i + step == stop)
	    slope = final_slope;
	else
	    _ComputeSlope(&pt[i], &pt[i+step], &slope);
	if (_SlopeCounterClockwise(&slope, &pen->vertex[active].slope_ccw)) {
	    if (++active == pen->num_vertices)
		active = 0;
	} else if (_SlopeClockwise(&slope, &pen->vertex[active].slope_cw)) {
	    if (--active == -1)
		active = pen->num_vertices - 1;
	} else {
	    i += step;
	}
    }

    return XrStatusSuccess;
}

/* Compute outline of a given spline using the pen.
   The trapezoids needed to fill that outline will be added to traps
*/
XrStatus
_XrPenStrokeSpline(XrPen	*pen,
		   XrSpline	*spline,
		   double	tolerance,
		   XrTraps	*traps)
{
    XrStatus status;
    XrPolygon polygon;

    _XrPolygonInit(&polygon);

    status = _XrSplineDecompose(spline, tolerance);
    if (status)
	return status;

    status = _XrPenStrokeSplineHalf(pen, spline, XrPenVertexFlagForward, &polygon);
    if (status)
	return status;

    status = _XrPenStrokeSplineHalf(pen, spline, XrPenVertexFlagReverse, &polygon);
    if (status)
	return status;

    _XrPolygonClose(&polygon);
    _XrTrapsTessellatePolygon(traps, &polygon, XrFillRuleWinding);
    _XrPolygonDeinit(&polygon);
    
    return XrStatusSuccess;
}
