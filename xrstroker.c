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

/* private functions */
static void
_TranslatePoint(XPointFixed *pt, XPointFixed *offset);

static int
_XrStrokerFaceClockwise(XrStrokeFace *in, XrStrokeFace *out);

static XrStatus
_XrStrokerJoin(XrStroker *stroker, XrStrokeFace *in, XrStrokeFace *out);

static void
_XrStrokerStartDash (XrStroker *stroker)
{
    XrGState *gstate = stroker->gstate;
    double offset;
    int	on = 1;
    int	i = 0;

    offset = gstate->dash_offset;
    while (offset >= gstate->dashes[i]) {
	offset -= gstate->dashes[i];
	on = 1-on;
	if (++i == gstate->ndashes)
	    i = 0;
    }
    stroker->dash_index = i;
    stroker->dash_on = on;
    stroker->dash_remain = gstate->dashes[i] - offset;
}

static void
_XrStrokerStepDash (XrStroker *stroker, double step)
{
    XrGState *gstate = stroker->gstate;
    stroker->dash_remain -= step;
    if (stroker->dash_remain <= 0) {
	stroker->dash_index++;
	if (stroker->dash_index == gstate->ndashes)
	    stroker->dash_index = 0;
	stroker->dash_on = 1-stroker->dash_on;
	stroker->dash_remain = gstate->dashes[stroker->dash_index];
    }
}

void
_XrStrokerInit(XrStroker *stroker, XrGState *gstate, XrTraps *traps)
{
    stroker->gstate = gstate;
    stroker->traps = traps;
    stroker->have_prev = 0;
    stroker->have_first = 0;
    stroker->is_first = 1;
    if (gstate->dashes)
	_XrStrokerStartDash (stroker);
}

void
_XrStrokerDeinit(XrStroker *stroker)
{
    /* nothing to do here */
}

static void
_TranslatePoint(XPointFixed *pt, XPointFixed *offset)
{
    pt->x += offset->x;
    pt->y += offset->y;
}

static int
_XrStrokerFaceClockwise(XrStrokeFace *in, XrStrokeFace *out)
{
    XPointDouble    d_in, d_out;

    d_in.x = XFixedToDouble(in->cw.x - in->pt.x);
    d_in.y = XFixedToDouble(in->cw.y - in->pt.y);
    d_out.x = XFixedToDouble(out->cw.x - out->pt.x);
    d_out.y = XFixedToDouble(out->cw.y - out->pt.y);

    return d_out.y * d_in.x > d_in.y * d_out.x;
}

static XrStatus
_XrStrokerJoin(XrStroker *stroker, XrStrokeFace *in, XrStrokeFace *out)
{
    XrStatus	status;
    XrGState	*gstate = stroker->gstate;
    int		clockwise = _XrStrokerFaceClockwise (in, out);
    XrPolygon	polygon;
    XPointFixed	*inpt, *outpt;

    /* XXX: There might be a more natural place to check for the
       degenerate join later in the code, (such as right before
       dividing by zero) */
    if (in->cw.x == out->cw.x
	&& in->cw.y == out->cw.y
	&& in->ccw.x == out->ccw.x
	&& in->ccw.y == out->ccw.y) {
	return XrStatusSuccess;
    }

    if (clockwise) {
    	inpt = &in->cw;
    	outpt = &out->cw;
    } else {
    	inpt = &in->ccw;
    	outpt = &out->ccw;
    }
    _XrPolygonInit (&polygon);
    switch (gstate->line_join) {
    case XrLineJoinRound: {
    }
    case XrLineJoinMiter: {
	XDouble	c = (-in->vector.x * out->vector.x)+(-in->vector.y * out->vector.y);
	XDouble ml = gstate->miter_limit;
	if (2 <= ml * ml * (1 - c)) {
	    XDouble x1, y1, x2, y2;
	    XDouble mx, my;
	    XDouble dx1, dx2, dy1, dy2;
	    XPointFixed	outer;

	    x1 = XFixedToDouble(inpt->x);
	    y1 = XFixedToDouble(inpt->y);
	    dx1 = in->vector.x;
	    dy1 = in->vector.y;
	    _XrTransformDistance(&gstate->ctm, &dx1, &dy1);
	    
	    x2 = XFixedToDouble(outpt->x);
	    y2 = XFixedToDouble(outpt->y);
	    dx2 = out->vector.x;
	    dy2 = out->vector.y;
	    _XrTransformDistance(&gstate->ctm, &dx2, &dy2);
	    
	    my = (((x2 - x1) * dy1 * dy2 - y2 * dx2 * dy1 + y1 * dx1 * dy2) /
		  (dx1 * dy2 - dx2 * dy1));
	    if (dy1)
		mx = (my - y1) * dx1 / dy1 + x1;
	    else
		mx = (my - y2) * dx2 / dy2 + x2;
	    
	    outer.x = XDoubleToFixed(mx);
	    outer.y = XDoubleToFixed(my);
	    _XrPolygonAddEdge (&polygon, &in->pt, inpt);
	    _XrPolygonAddEdge (&polygon, inpt, &outer);
	    _XrPolygonAddEdge (&polygon, &outer, outpt);
	    _XrPolygonAddEdge (&polygon, outpt, &in->pt);
	    break;
	}
	/* fall through ... */
    }
    case XrLineJoinBevel: {
	_XrPolygonAddEdge (&polygon, &in->pt, inpt);
	_XrPolygonAddEdge (&polygon, inpt, outpt);
	_XrPolygonAddEdge (&polygon, outpt, &in->pt);
	break;
    }
    }

    status = _XrTrapsTessellatePolygon (stroker->traps, &polygon, XrFillRuleWinding);
    _XrPolygonDeinit (&polygon);

    return status;
}

static XrStatus
_XrStrokerCap(XrStroker *stroker, XrStrokeFace *f)
{
    XrStatus	    status;
    XrGState	    *gstate = stroker->gstate;
    XrPolygon	    polygon;

    if (gstate->line_cap == XrLineCapButt)
	return XrStatusSuccess;
    
    _XrPolygonInit (&polygon);
    switch (gstate->line_cap) {
    case XrLineCapRound: {
	break;
    }
    case XrLineCapSquare: {
	double dx, dy;
	XPointFixed	fvector;
	XPointFixed	occw, ocw;
	dx = f->vector.x;
	dy = f->vector.y;
	dx *= gstate->line_width / 2.0;
	dy *= gstate->line_width / 2.0;
	_XrTransformDistance(&gstate->ctm, &dx, &dy);
	fvector.x = XDoubleToFixed(dx);
	fvector.y = XDoubleToFixed(dy);
	occw.x = f->ccw.x + fvector.x;
	occw.y = f->ccw.y + fvector.y;
	ocw.x = f->cw.x + fvector.x;
	ocw.y = f->cw.y + fvector.y;

	_XrPolygonAddEdge (&polygon, &f->cw, &ocw);
	_XrPolygonAddEdge (&polygon, &ocw, &occw);
	_XrPolygonAddEdge (&polygon, &occw, &f->ccw);
	_XrPolygonAddEdge (&polygon, &f->ccw, &f->cw);
	break;
    }
    case XrLineCapButt: {
	break;
    }
    }

    status = _XrTrapsTessellatePolygon (stroker->traps, &polygon, XrFillRuleWinding);
    _XrPolygonDeinit (&polygon);

    return status;
}

static void
_ComputeFace(XPointFixed *pt, XrSlopeFixed *slope, XrGState *gstate, XrStrokeFace *face)
{
    double mag, tmp;
    double dx, dy;
    XPointDouble user_vector;
    XPointFixed offset_ccw, offset_cw;

    dx = XFixedToDouble(slope->dx);
    dy = XFixedToDouble(slope->dy);

    _XrTransformDistance(&gstate->ctm_inverse, &dx, &dy);

    mag = sqrt(dx * dx + dy * dy);
    if (mag == 0) {
	/* XXX: Can't compute other face points. Do we want a tag in the face for this case? */
	return;
    }

    dx /= mag;
    dy /= mag;

    user_vector.x = dx;
    user_vector.y = dy;

    tmp = dx;
    dx = - dy * (gstate->line_width / 2.0);
    dy = tmp * (gstate->line_width / 2.0);

    _XrTransformDistance(&gstate->ctm, &dx, &dy);

    offset_ccw.x = XDoubleToFixed(dx);
    offset_ccw.y = XDoubleToFixed(dy);
    offset_cw.x = -offset_ccw.x;
    offset_cw.y = -offset_ccw.y;

    face->ccw = *pt;
    _TranslatePoint(&face->ccw, &offset_ccw);

    face->pt = *pt;

    face->cw = *pt;
    _TranslatePoint(&face->cw, &offset_cw);

    face->vector.x = user_vector.x;
    face->vector.y = user_vector.y;
}

static XrStatus
_XrStrokerAddSubEdge (XrStroker *stroker, XPointFixed *p1, XPointFixed *p2,
		      XrStrokeFace *start, XrStrokeFace *end)
{
    XrGState *gstate = stroker->gstate;
    XPointFixed quad[4];
    XrSlopeFixed slope;

    if (p1->x == p2->x && p1->y == p2->y) {
	/* XXX: Need to rethink how this case should be handled, (both
           here and in _ComputeFace). The key behavior is that
           degenerate paths should draw as much as possible. */
	return XrStatusSuccess;
    }

    _ComputeSlope(p1, p2, &slope);
    _ComputeFace(p1, &slope, gstate, start);

    /* XXX: This could be optimized slightly by not calling
       _ComputeFace again but rather  translating the relevant
       fields from start. */
    _ComputeFace(p2, &slope, gstate, end);

    quad[0] = start->cw;
    quad[1] = start->ccw;
    quad[2] = end->ccw;
    quad[3] = end->cw;

    return _XrTrapsTessellateRectangle(stroker->traps, quad);
}

XrStatus
_XrStrokerAddEdge(void *closure, XPointFixed *p1, XPointFixed *p2)
{
    XrStatus status;
    XrStroker *stroker = closure;
    XrStrokeFace start, end;

    if (p1->x == p2->x && p1->y == p2->y) {
	/* XXX: Need to rethink how this case should be handled, (both
           here and in XrStrokerAddSubEdge and in _ComputeFace). The
           key behavior is that degenerate paths should draw as much
           as possible. */
	return XrStatusSuccess;
    }
    
    status = _XrStrokerAddSubEdge (stroker, p1, p2, &start, &end);
    if (status)
	return status;

    if (stroker->have_prev) {
	status = _XrStrokerJoin (stroker, &stroker->prev, &start);
	if (status)
	    return status;
    } else {
	stroker->have_prev = 1;
	if (stroker->is_first) {
	    stroker->have_first = 1;
	    stroker->first = start;
	}
    }
    stroker->prev = end;
    stroker->is_first = 0;

    return XrStatusSuccess;
}

/*
 * Dashed lines.  Cap each dash end, join around turns when on
 */
XrStatus
_XrStrokerAddEdgeDashed (void *closure, XPointFixed *p1, XPointFixed *p2)
{
    XrStatus status = XrStatusSuccess;
    XrStroker *stroker = closure;
    XrGState *gstate = stroker->gstate;
    double mag, remain, tmp;
    double dx, dy;
    double dx2, dy2;
    XPointFixed fd1, fd2;
    int first = 1;
    XrStrokeFace sub_start, sub_end;
    
    dx = XFixedToDouble(p2->x - p1->x);
    dy = XFixedToDouble(p2->y - p1->y);

    _XrTransformDistance(&gstate->ctm_inverse, &dx, &dy);

    mag = sqrt(dx *dx + dy * dy);
    remain = mag;
    fd1 = *p1;
    while (remain) {
	tmp = stroker->dash_remain;
	if (tmp > remain)
	    tmp = remain;
	remain -= tmp;
        dx2 = dx * (mag - remain)/mag;
	dy2 = dy * (mag - remain)/mag;
	_XrTransformDistance (&gstate->ctm, &dx2, &dy2);
	fd2.x = XDoubleToFixed (dx2);
	fd2.y = XDoubleToFixed (dy2);
	fd2.x += p1->x;
	fd2.y += p1->y;
	/*
	 * XXX simplify this case analysis
	 */
	if (stroker->dash_on) {
	    status = _XrStrokerAddSubEdge (stroker, &fd1, &fd2, &sub_start, &sub_end);
	    if (status)
		return status;
	    if (!first) {
		/*
		 * Not first dash in this segment, cap start
		 */
		status = _XrStrokerCap (stroker, &sub_start);
		if (status)
		    return status;
	    } else {
		/*
		 * First in this segment, join to any prev, else
		 * if at start of sub-path, mark position, else
		 * cap
		 */
		if (stroker->have_prev) {
		    status = _XrStrokerJoin (stroker, &stroker->prev, &sub_start);
		    if (status)
			return status;
		} else {
		    if (stroker->is_first) {
			stroker->have_first = 1;
			stroker->first = sub_start;
		    } else {
			status = _XrStrokerCap (stroker, &sub_start);
			if (status)
			    return status;
		    }
		}
	    }
	    if (remain) {
		/*
		 * Cap if not at end of segment
		 */
		status = _XrStrokerCap (stroker, &sub_end);
		if (status)
		    return status;
	    } else {
		/*
		 * Mark previous line face and fix up next time
		 * through
		 */
		stroker->prev = sub_end;
		stroker->have_prev = 1;
	    }
	} else {
	    /*
	     * If starting with off dash, check previous face
	     * and cap if necessary
	     */
	    if (first) {
		if (stroker->have_prev) {
		    status = _XrStrokerCap (stroker, &stroker->prev);
		    if (status)
			return status;
		}
	    }
	    if (!remain)
		stroker->have_prev = 0;
	}
	_XrStrokerStepDash (stroker, tmp);
	fd1 = fd2;
	first = 0;
    }
    stroker->is_first = 0;
    return status;
}

XrStatus
_XrStrokerAddSpline (void *closure, XPointFixed *a, XPointFixed *b, XPointFixed *c, XPointFixed *d)
{
    XrStatus status = XrStatusSuccess;
    XrStroker *stroker = closure;
    XrGState *gstate = stroker->gstate;
    XrSpline spline;
    XrPen pen;
    XrStrokeFace start, end;
    XrPenFlaggedPoint extra_points[4];

    status = _XrSplineInit(&spline, a, b, c, d);
    if (status == XrIntStatusDegenerate)
	return XrStatusSuccess;

    status = _XrPenInitCopy(&pen, &gstate->pen_regular);
    if (status)
	goto CLEANUP_SPLINE;

    _ComputeFace(a, &spline.initial_slope, gstate, &start);
    _ComputeFace(d, &spline.final_slope, gstate, &end);

    if (stroker->have_prev) {
	status = _XrStrokerJoin (stroker, &stroker->prev, &start);
	if (status)
	    return status;
    } else {
	stroker->have_prev = 1;
	if (stroker->is_first) {
	    stroker->have_first = 1;
	    stroker->first = start;
	}
    }
    stroker->prev = end;
    stroker->is_first = 0;
    
    extra_points[0].pt = start.cw;  extra_points[0].flag = XrPenVertexFlagForward;
    extra_points[0].pt.x -= start.pt.x;
    extra_points[0].pt.y -= start.pt.y;
    extra_points[1].pt = start.ccw; extra_points[1].flag = XrPenVertexFlagNone;
    extra_points[1].pt.x -= start.pt.x;
    extra_points[1].pt.y -= start.pt.y;
    extra_points[2].pt = end.cw;  extra_points[2].flag = XrPenVertexFlagNone;
    extra_points[2].pt.x -= end.pt.x;
    extra_points[2].pt.y -= end.pt.y;
    extra_points[3].pt = end.ccw; extra_points[3].flag = XrPenVertexFlagReverse;
    extra_points[3].pt.x -= end.pt.x;
    extra_points[3].pt.y -= end.pt.y;
    
    status = _XrPenAddPoints(&pen, extra_points, 4);
    if (status)
	goto CLEANUP_PEN;

    status = _XrPenStrokeSpline(&pen, &spline, gstate->tolerance, stroker->traps);
    if (status)
	goto CLEANUP_PEN;

  CLEANUP_PEN:
    _XrPenDeinit(&pen);
  CLEANUP_SPLINE:
    _XrSplineDeinit(&spline);

    return status;
}

XrStatus
_XrStrokerDoneSubPath (void *closure, XrSubPathDone done)
{
    XrStatus status;
    XrStroker *stroker = closure;

    switch (done) {
    case XrSubPathDoneJoin:
	if (stroker->have_first && stroker->have_prev) {
	    status = _XrStrokerJoin (stroker, &stroker->prev, &stroker->first);
	    if (status)
		return status;
	    break;
	}
	/* fall through... */
    case XrSubPathDoneCap:
	if (stroker->have_first) {
	    status = _XrStrokerCap (stroker, &stroker->first);
	    if (status)
		return status;
	}
	if (stroker->have_prev) {
	    status = _XrStrokerCap (stroker, &stroker->prev);
	    if (status)
		return status;
	}
	break;
    }

    stroker->have_prev = 0;
    stroker->have_first = 0;
    stroker->is_first = 1;

    return XrStatusSuccess;
}

XrStatus
_XrStrokerDonePath (void *closure)
{
    return XrStatusSuccess;
}
