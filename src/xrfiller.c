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

void
XrFillerInit(XrFiller *filler, XrGState *gstate, XrTraps *traps)
{
    filler->gstate = gstate;
    filler->traps = traps;

    XrPolygonInit(&filler->polygon);

    filler->have_prev = 0;
}

void
XrFillerDeinit(XrFiller *filler)
{
    XrPolygonDeinit(&filler->polygon);
}

XrError
XrFillerAddEdge(void *closure, XPointFixed *p1, XPointFixed *p2)
{
    XrFiller *filler = closure;
    XrPolygon *polygon = &filler->polygon;

    if (filler->have_prev == 0) {
	filler->have_prev = 1;
	filler->first = *p1;
    }
    filler->prev = *p2;

    return XrPolygonAddEdge(polygon, p1, p2);
}

XrError
XrFillerAddSpline (void *closure, XPointFixed *a, XPointFixed *b, XPointFixed *c, XPointFixed *d)
{
    int i;
    XrError err = XrErrorSuccess;
    XrFiller *filler = closure;
    XrPolygon *polygon = &filler->polygon;
    XrGState *gstate = filler->gstate;
    XrSpline spline;

    if (filler->have_prev == 0) {
	filler->have_prev = 1;
	filler->first = *a;
    }
    filler->prev = *d;

    err = XrSplineInit(&spline, a, b, c, d);
    if (err == XrErrorDegenerate)
	return XrErrorSuccess;

    XrSplineDecompose(&spline, gstate->tolerance);
    if (err)
	goto CLEANUP_SPLINE;

    for (i = 0; i < spline.num_pts - 1; i++) {
	err = XrPolygonAddEdge(polygon, &spline.pts[i], &spline.pts[i+1]);
	if (err)
	    goto CLEANUP_SPLINE;
    }

  CLEANUP_SPLINE:
    XrSplineDeinit(&spline);

    return err;
}

XrError
XrFillerDoneSubPath (void *closure, XrSubPathDone done)
{
    XrError err = XrErrorSuccess;
    XrFiller *filler = closure;
    XrPolygon *polygon = &filler->polygon;

    if (filler->have_prev)
	err = XrPolygonAddEdge(polygon, &filler->prev, &filler->first);

    return err;
}

XrError
XrFillerDonePath (void *closure)
{
    XrFiller *filler = closure;

    return XrTrapsTessellatePolygon(filler->traps, &filler->polygon, filler->gstate->winding);
}

