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

typedef struct _XrFiller {
    XrGState *gstate;
    XrTraps *traps;

    XrPolygon polygon;
} XrFiller;

static void
_XrFillerInit(XrFiller *filler, XrGState *gstate, XrTraps *traps);

static void
_XrFillerDeinit(XrFiller *filler);

static XrStatus
_XrFillerAddEdge(void *closure, XPointFixed *p1, XPointFixed *p2);

static XrStatus
_XrFillerAddSpline (void *closure, XPointFixed *a, XPointFixed *b, XPointFixed *c, XPointFixed *d);

static XrStatus
_XrFillerDoneSubPath (void *closure, XrSubPathDone done);

static XrStatus
_XrFillerDonePath (void *closure);

static void
_XrFillerInit(XrFiller *filler, XrGState *gstate, XrTraps *traps)
{
    filler->gstate = gstate;
    filler->traps = traps;

    _XrPolygonInit(&filler->polygon);
}

static void
_XrFillerDeinit(XrFiller *filler)
{
    _XrPolygonDeinit(&filler->polygon);
}

static XrStatus
_XrFillerAddEdge(void *closure, XPointFixed *p1, XPointFixed *p2)
{
    XrFiller *filler = closure;
    XrPolygon *polygon = &filler->polygon;

    return _XrPolygonAddEdge(polygon, p1, p2);
}

static XrStatus
_XrFillerAddSpline (void *closure, XPointFixed *a, XPointFixed *b, XPointFixed *c, XPointFixed *d)
{
    int i;
    XrStatus status = XrStatusSuccess;
    XrFiller *filler = closure;
    XrPolygon *polygon = &filler->polygon;
    XrGState *gstate = filler->gstate;
    XrSpline spline;

    status = _XrSplineInit(&spline, a, b, c, d);
    if (status == XrIntStatusDegenerate)
	return XrStatusSuccess;

    _XrSplineDecompose(&spline, gstate->tolerance);
    if (status)
	goto CLEANUP_SPLINE;

    for (i = 0; i < spline.num_pts - 1; i++) {
	status = _XrPolygonAddEdge(polygon, &spline.pts[i], &spline.pts[i+1]);
	if (status)
	    goto CLEANUP_SPLINE;
    }

  CLEANUP_SPLINE:
    _XrSplineDeinit(&spline);

    return status;
}

static XrStatus
_XrFillerDoneSubPath (void *closure, XrSubPathDone done)
{
    XrStatus status = XrStatusSuccess;
    XrFiller *filler = closure;
    XrPolygon *polygon = &filler->polygon;

    _XrPolygonClose(polygon);

    return status;
}

static XrStatus
_XrFillerDonePath (void *closure)
{
    XrFiller *filler = closure;

    return _XrTrapsTessellatePolygon(filler->traps,
				     &filler->polygon,
				     filler->gstate->fill_rule);
}

XrStatus
_XrPathFillToTraps(XrPath *path, XrGState *gstate, XrTraps *traps)
{
    static const XrPathCallbacks filler_callbacks = {
	_XrFillerAddEdge,
	_XrFillerAddSpline,
	_XrFillerDoneSubPath,
	_XrFillerDonePath
    };

    XrStatus status;
    XrFiller filler;

    _XrFillerInit(&filler, gstate, traps);

    status = _XrPathInterpret(path,
			      XrPathDirectionForward,
			      &filler_callbacks, &filler);
    if (status) {
	_XrFillerDeinit(&filler);
	return status;
    }

    _XrFillerDeinit(&filler);

    return XrStatusSuccess;
}

