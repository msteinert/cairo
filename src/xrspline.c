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
_XrSplineErrorSquared(XrSpline *spline, XPointFixed *p);

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
_XrSplineErrorSquared(XrSpline *spline, XPointFixed *p)
{
    XPointFixed mid;
    double dx, dy;

    _LerpHalf(&spline->a, &spline->d, &mid);

    dx = XFixedToDouble(mid.x - p->x);
    dy = XFixedToDouble(mid.y - p->y);

    return dx*dx + dy*dy;
}

static XrError
_XrSplineDecomposeInto(XrSpline *spline, double tolerance_squared, XrSpline *result)
{
    XrError err;
    XrSpline s1, s2;

    _DeCastlejau(spline, &s1, &s2);

    if (_XrSplineErrorSquared(spline, &s1.d) < tolerance_squared) {
	err = _XrSplineAddPoint(result, &s1.a);
	if (err)
	    return err;

	err = _XrSplineAddPoint(result, &s1.d);
	if (err)
	    return err;
    } else {
	err = _XrSplineDecomposeInto(&s1, tolerance_squared, result);
	if (err)
	    return err;

	err = _XrSplineDecomposeInto(&s2, tolerance_squared, result);
	if (err)
	    return err;
    }

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

