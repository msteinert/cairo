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
    while (offset >= gstate->dash[i]) {
	offset -= gstate->dash[i];
	on = 1-on;
	if (++i == gstate->num_dashes)
	    i = 0;
    }
    stroker->dash_index = i;
    stroker->dash_on = on;
    stroker->dash_remain = gstate->dash[i] - offset;
}

static void
_XrStrokerStepDash (XrStroker *stroker, double step)
{
    XrGState *gstate = stroker->gstate;
    stroker->dash_remain -= step;
    if (stroker->dash_remain <= 0) {
	stroker->dash_index++;
	if (stroker->dash_index == gstate->num_dashes)
	    stroker->dash_index = 0;
	stroker->dash_on = 1-stroker->dash_on;
	stroker->dash_remain = gstate->dash[stroker->dash_index];
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
    if (gstate->dash)
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
    int		clockwise = _XrStrokerFaceClockwise (out, in);
    XPointFixed	*inpt, *outpt;

    if (in->cw.x == out->cw.x
	&& in->cw.y == out->cw.y
	&& in->ccw.x == out->ccw.x
	&& in->ccw.y == out->ccw.y) {
	return XrStatusSuccess;
    }

    if (clockwise) {
    	inpt = &in->ccw;
    	outpt = &out->ccw;
    } else {
    	inpt = &in->cw;
    	outpt = &out->cw;
    }

    switch (gstate->line_join) {
    case XrLineJoinRound: {
	int i;
	int start, step, stop;
	XPointFixed tri[3], initial, final;
	XrPen *pen = &gstate->pen_regular;

	tri[0] = in->pt;
	if (clockwise) {
	    initial = in->ccw;
	    _XrPenFindActiveCCWVertexIndex(pen, &in->dev_vector, &start);
	    step = -1;
	    _XrPenFindActiveCCWVertexIndex(pen, &out->dev_vector, &stop);
	    final = out->ccw;
	} else {
	    initial = in->cw;
	    _XrPenFindActiveCWVertexIndex(pen, &in->dev_vector, &start);
	    step = +1;
	    _XrPenFindActiveCWVertexIndex(pen, &out->dev_vector, &stop);
	    final = out->cw;
	}

	i = start;
	tri[1] = initial;
	while (i != stop) {
	    tri[2] = in->pt;
	    _TranslatePoint(&tri[2], &pen->vertex[i].pt);
	    _XrTrapsTessellateTriangle(stroker->traps, tri);
	    tri[1] = tri[2];
	    i += step;
	    if (i < 0)
		i = pen->num_vertices - 1;
	    if (i >= pen->num_vertices)
		i = 0;
	}

	tri[2] = final;

	return _XrTrapsTessellateTriangle(stroker->traps, tri);
    }
    case XrLineJoinMiter:
    default: {
	XrPolygon	polygon;
	XDouble	c = (-in->usr_vector.x * out->usr_vector.x)+(-in->usr_vector.y * out->usr_vector.y);
	XDouble ml = gstate->miter_limit;

	_XrPolygonInit (&polygon);

	if (2 <= ml * ml * (1 - c)) {
	    XDouble x1, y1, x2, y2;
	    XDouble mx, my;
	    XDouble dx1, dx2, dy1, dy2;
	    XPointFixed	outer;

	    x1 = XFixedToDouble(inpt->x);
	    y1 = XFixedToDouble(inpt->y);
	    dx1 = in->usr_vector.x;
	    dy1 = in->usr_vector.y;
	    _XrTransformDistance(&gstate->ctm, &dx1, &dy1);
	    
	    x2 = XFixedToDouble(outpt->x);
	    y2 = XFixedToDouble(outpt->y);
	    dx2 = out->usr_vector.x;
	    dy2 = out->usr_vector.y;
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
	    status = _XrTrapsTessellatePolygon (stroker->traps,
						&polygon,
						XrFillRuleWinding);
	    _XrPolygonDeinit (&polygon);

	    return status;
	}
	/* fall through ... */
    }
    case XrLineJoinBevel: {
	XPointFixed tri[3];
	tri[0] = in->pt;
	tri[1] = *inpt;
	tri[2] = *outpt;

	return _XrTrapsTessellateTriangle (stroker->traps, tri);
    }
    }
}

static XrStatus
_XrStrokerCap(XrStroker *stroker, XrStrokeFace *f)
{
    XrStatus	    status;
    XrGState	    *gstate = stroker->gstate;

    if (gstate->line_cap == XrLineCapButt)
	return XrStatusSuccess;
    
    switch (gstate->line_cap) {
    case XrLineCapRound: {
	int i;
	int start, stop;
	XrSlopeFixed slope;
	XPointFixed tri[3];
	XrPen *pen = &gstate->pen_regular;

	slope = f->dev_vector;
	_XrPenFindActiveCWVertexIndex(pen, &slope, &start);
	slope.dx = -slope.dx;
	slope.dy = -slope.dy;
	_XrPenFindActiveCWVertexIndex(pen, &slope, &stop);

	tri[0] = f->pt;
	tri[1] = f->cw;
	for (i=start; i != stop; i = (i+1) % pen->num_vertices) {
	    tri[2] = f->pt;
	    _TranslatePoint(&tri[2], &pen->vertex[i].pt);
	    _XrTrapsTessellateTriangle(stroker->traps, tri);
	    tri[1] = tri[2];
	}
	tri[2] = f->ccw;

	return _XrTrapsTessellateTriangle(stroker->traps, tri);
    }
    case XrLineCapSquare: {
	double dx, dy;
	XrSlopeFixed	fvector;
	XPointFixed	occw, ocw;
	XrPolygon	polygon;

	_XrPolygonInit (&polygon);

	dx = f->usr_vector.x;
	dy = f->usr_vector.y;
	dx *= gstate->line_width / 2.0;
	dy *= gstate->line_width / 2.0;
	_XrTransformDistance(&gstate->ctm, &dx, &dy);
	fvector.dx = XDoubleToFixed(dx);
	fvector.dy = XDoubleToFixed(dy);
	occw.x = f->ccw.x + fvector.dx;
	occw.y = f->ccw.y + fvector.dy;
	ocw.x = f->cw.x + fvector.dx;
	ocw.y = f->cw.y + fvector.dy;

	_XrPolygonAddEdge (&polygon, &f->cw, &ocw);
	_XrPolygonAddEdge (&polygon, &ocw, &occw);
	_XrPolygonAddEdge (&polygon, &occw, &f->ccw);
	_XrPolygonAddEdge (&polygon, &f->ccw, &f->cw);

	status = _XrTrapsTessellatePolygon (stroker->traps, &polygon, XrFillRuleWinding);
	_XrPolygonDeinit (&polygon);

	return status;
    }
    case XrLineCapButt:
    default:
	return XrStatusSuccess;
    }
}

static void
_ComputeFace(XPointFixed *pt, XrSlopeFixed *slope, XrGState *gstate, XrStrokeFace *face)
{
    double mag, tmp;
    double dx, dy;
    XPointDouble usr_vector;
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

    usr_vector.x = dx;
    usr_vector.y = dy;

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

    face->usr_vector.x = usr_vector.x;
    face->usr_vector.y = usr_vector.y;

    face->dev_vector = *slope;
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
    XPointFixed extra_points[4];

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
    
    extra_points[0] = start.cw;
    extra_points[0].x -= start.pt.x;
    extra_points[0].y -= start.pt.y;
    extra_points[1] = start.ccw;
    extra_points[1].x -= start.pt.x;
    extra_points[1].y -= start.pt.y;
    extra_points[2] = end.cw;
    extra_points[2].x -= end.pt.x;
    extra_points[2].y -= end.pt.y;
    extra_points[3] = end.ccw;
    extra_points[3].x -= end.pt.x;
    extra_points[3].y -= end.pt.y;
    
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
	    XPointFixed t;
	    /* The initial cap needs an outward facing vector. Reverse everything */
	    stroker->first.usr_vector.x = -stroker->first.usr_vector.x;
	    stroker->first.usr_vector.y = -stroker->first.usr_vector.y;
	    stroker->first.dev_vector.dx = -stroker->first.dev_vector.dx;
	    stroker->first.dev_vector.dy = -stroker->first.dev_vector.dy;
	    t = stroker->first.cw;
	    stroker->first.cw = stroker->first.ccw;
	    stroker->first.ccw = t;
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
