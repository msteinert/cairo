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

static XrError
_XrSplineGrowBy(XrSpline *spline, int additional);

static XrError
_XrSplineAddPoint(XrSpline *spline, XPointFixed *pt);

static void
_LerpHalf(XPointFixed *a, XPointFixed *b, XPointFixed *result);

static void
_DeCastlejau(XrSpline *spline, XrSpline *s1, XrSpline *s2);

static double
_XrSplineErrorSquared(XrSpline *spline);

static XrError
_XrSplineDecomposeInto(XrSpline *spline, double tolerance_squared, XrSpline *result);

#define XR_SPLINE_GROWTH_INC 100

void
XrSplineInit(XrSpline *spline, XPointFixed *a,  XPointFixed *b,  XPointFixed *c,  XPointFixed *d)
{
    spline->a = *a;
    spline->b = *b;
    spline->c = *c;
    spline->d = *d;

    if (a->x != b->x || a->y != b->y) {
	ComputeSlope(&spline->a, &spline->b, &spline->initial_slope);
    } else if (a->x != c->x || a->y != c->y) {
	ComputeSlope(&spline->a, &spline->c, &spline->initial_slope);
    } else if (a->x != d->x || a->y != d->y) {
	ComputeSlope(&spline->a, &spline->d, &spline->initial_slope);
    } else {
	/* XXX: Completely degenerate spline (single point). I'm still
           not sure what the fallout from this is. */
	spline->initial_slope.dx = 0;
	spline->initial_slope.dy = 0;
    }

    if (c->x != d->x || c->y != d->y) {
	ComputeSlope(&spline->c, &spline->d, &spline->final_slope);
    } else if (b->x != d->x || b->y != d->y) {
	ComputeSlope(&spline->b, &spline->b, &spline->final_slope);
    } else if (a->x != d->x || a->y != d->y) {
	ComputeSlope(&spline->a, &spline->d, &spline->final_slope);
    } else {
	spline->final_slope.dx = 0;
	spline->final_slope.dy = 0;
    }

    spline->num_pts = 0;
    spline->pts_size = 0;
    spline->pts = NULL;
}

void
XrSplineDeinit(XrSpline *spline)
{
    spline->num_pts = 0;
    spline->pts_size = 0;
    free(spline->pts);
    spline->pts = NULL;
}

static XrError
_XrSplineGrowBy(XrSpline *spline, int additional)
{
    XPointFixed *new_pts;
    int old_size = spline->pts_size;
    int new_size = spline->num_pts + additional;

    if (new_size <= spline->pts_size)
	return XrErrorSuccess;

    spline->pts_size = new_size;
    new_pts = realloc(spline->pts, spline->pts_size * sizeof(XPointFixed));

    if (new_pts == NULL) {
	spline->pts_size = old_size;
	return XrErrorNoMemory;
    }

    spline->pts = new_pts;

    return XrErrorSuccess;
}

static XrError
_XrSplineAddPoint(XrSpline *spline, XPointFixed *pt)
{
    XrError err;

    if (spline->num_pts >= spline->pts_size) {
	err = _XrSplineGrowBy(spline, XR_SPLINE_GROWTH_INC);
	if (err)
	    return err;
    }

    spline->pts[spline->num_pts] = *pt;
    spline->num_pts++;

    return XrErrorSuccess;
}

static void
_LerpHalf(XPointFixed *a, XPointFixed *b, XPointFixed *result)
{
    result->x = a->x + ((b->x - a->x) >> 1);
    result->y = a->y + ((b->y - a->y) >> 1);
}

static void
_DeCastlejau(XrSpline *spline, XrSpline *s1, XrSpline *s2)
{
    XPointFixed ab, bc, cd;
    XPointFixed abbc, bccd;
    XPointFixed final;

    _LerpHalf(&spline->a, &spline->b, &ab);
    _LerpHalf(&spline->b, &spline->c, &bc);
    _LerpHalf(&spline->c, &spline->d, &cd);
    _LerpHalf(&ab, &bc, &abbc);
    _LerpHalf(&bc, &cd, &bccd);
    _LerpHalf(&abbc, &bccd, &final);

    s1->a = spline->a;
    s1->b = ab;
    s1->c = abbc;
    s1->d = final;

    s2->a = final;
    s2->b = bccd;
    s2->c = cd;
    s2->d = spline->d;
}

static double
_PointDistanceSquaredToPoint(XPointFixed *a, XPointFixed *b)
{
    double dx = XFixedToDouble(b->x - a->x);
    double dy = XFixedToDouble(b->y - a->y);

    return dx*dx + dy*dy;
}

static double
_PointDistanceSquaredToSegment(XPointFixed *p, XPointFixed *p1, XPointFixed *p2)
{
    double u;
    double dx, dy;
    double pdx, pdy;
    XPointFixed px;

    /* intersection point (px):

       px = p1 + u(p2 - p1)
       (p - px) . (p2 - p1) = 0

       Thus:

       u = ((p - p1) . (p2 - p1)) / (||(p2 - p1)|| ^ 2);
    */

    dx = XFixedToDouble(p2->x - p1->x);
    dy = XFixedToDouble(p2->y - p1->y);

    if (dx == 0 && dy == 0)
	return _PointDistanceSquaredToPoint(p, p1);

    pdx = XFixedToDouble(p->x - p1->x);
    pdy = XFixedToDouble(p->y - p1->y);

    u = (pdx * dx + pdy * dy) / (dx*dx + dy*dy);

    if (u <= 0)
	return _PointDistanceSquaredToPoint(p, p1);
    else if (u >= 1)
	return _PointDistanceSquaredToPoint(p, p2);

    px.x = p1->x + u * (p2->x - p1->x);
    px.y = p1->y + u * (p2->y - p1->y);

    return _PointDistanceSquaredToPoint(p, &px);
}

/* Return an upper bound on the error (squared) that could result from approximating
   a spline as a line segment connecting the two endpoints */
static double
_XrSplineErrorSquared(XrSpline *spline)
{
    double berr, cerr;

    berr = _PointDistanceSquaredToSegment(&spline->b, &spline->a, &spline->d);
    cerr = _PointDistanceSquaredToSegment(&spline->c, &spline->a, &spline->d);

    if (berr > cerr)
	return berr;
    else
	return cerr;
}

static XrError
_XrSplineDecomposeInto(XrSpline *spline, double tolerance_squared, XrSpline *result)
{
    XrError err;
    XrSpline s1, s2;

    if (_XrSplineErrorSquared(spline) < tolerance_squared) {
	return _XrSplineAddPoint(result, &spline->a);
    }

    _DeCastlejau(spline, &s1, &s2);

    err = _XrSplineDecomposeInto(&s1, tolerance_squared, result);
    if (err)
	return err;
    
    err = _XrSplineDecomposeInto(&s2, tolerance_squared, result);
    if (err)
	return err;

    return XrErrorSuccess;
}

XrError
XrSplineDecompose(XrSpline *spline, double tolerance)
{
    XrError err;

    if (spline->pts_size) {
	XrSplineDeinit(spline);
    }

    err = _XrSplineDecomposeInto(spline, tolerance * tolerance, spline);
    if (err)
	return err;

    err = _XrSplineAddPoint(spline, &spline->d);
    if (err)
	return err;

    return XrErrorSuccess;
}

