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

static XrError
_XrStrokerJoin(XrStroker *stroker, XrStrokeFace *in, XrStrokeFace *out);

static void
XrStrokerStartDash (XrStroker *stroker)
{
    XrGState *gstate = stroker->gstate;
    double offset;
    int	on = 1;
    int	i = 0;

    offset = gstate->dash_offset;
    while (offset >= gstate->dashes[i])
    {
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
XrStrokerStepDash (XrStroker *stroker, double step)
{
    XrGState *gstate = stroker->gstate;
    stroker->dash_remain -= step;
    if (stroker->dash_remain <= 0)
    {
	stroker->dash_index++;
	if (stroker->dash_index == gstate->ndashes)
	    stroker->dash_index = 0;
	stroker->dash_on = 1-stroker->dash_on;
	stroker->dash_remain = gstate->dashes[stroker->dash_index];
    }
}

void
XrStrokerInit(XrStroker *stroker, XrGState *gstate, XrTraps *traps)
{
    stroker->gstate = gstate;
    stroker->traps = traps;
    stroker->have_prev = 0;
    stroker->have_first = 0;
    stroker->is_first = 1;
    if (gstate->dashes)
	XrStrokerStartDash (stroker);
}

void
XrStrokerDeinit(XrStroker *stroker)
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

static XrError
_XrStrokerJoin(XrStroker *stroker, XrStrokeFace *in, XrStrokeFace *out)
{
    XrError	err;
    XrGState	*gstate = stroker->gstate;
    int		clockwise = _XrStrokerFaceClockwise (in, out);
    XrPolygon	polygon;
    XPointFixed	*inpt, *outpt;

    if (clockwise)
    {
    	inpt = &in->cw;
    	outpt = &out->cw;
    }
    else
    {
    	inpt = &in->ccw;
    	outpt = &out->ccw;
    }
    XrPolygonInit (&polygon);
    switch (gstate->line_join) {
    case XrLineJoinRound: {
    }
    case XrLineJoinMiter: {
	XDouble	c = in->vector.x * out->vector.x + in->vector.y * out->vector.y;
	double ml = gstate->miter_limit;
	if (2 <= ml * ml * (1 - c))
	{
	    XDouble x1, y1, x2, y2;
	    XDouble mx, my;
	    XDouble dx1, dx2, dy1, dy2;
	    XPointFixed	outer;
	    XPointDouble    v1, v2;

	    x1 = XFixedToDouble(inpt->x);
	    y1 = XFixedToDouble(inpt->y);
	    v1 = in->vector;
	    XrTransformPointWithoutTranslate(&gstate->ctm, &v1);
	    dx1 = v1.x;
	    dy1 = v1.y;
	    
	    x2 = XFixedToDouble(outpt->x);
	    y2 = XFixedToDouble(outpt->y);
	    v2 = out->vector;
	    XrTransformPointWithoutTranslate(&gstate->ctm, &v2);
	    dx2 = v2.x;
	    dy2 = v2.y;
	    
	    my = (((x2 - x1) * dy1 * dy2 - y2 * dx2 * dy1 + y1 * dx1 * dy2) /
		  (dx1 * dy2 - dx2 * dy1));
	    if (dy1)
		mx = (my - y1) * dx1 / dy1 + x1;
	    else
		mx = (my - y2) * dx2 / dy2 + x2;
	    
	    outer.x = XDoubleToFixed(mx);
	    outer.y = XDoubleToFixed(my);
	    XrPolygonAddEdge (&polygon, &in->pt, inpt);
	    XrPolygonAddEdge (&polygon, inpt, &outer);
	    XrPolygonAddEdge (&polygon, &outer, outpt);
	    XrPolygonAddEdge (&polygon, outpt, &in->pt);
	    break;
	}
	/* fall through ... */
    }
    case XrLineJoinBevel: {
	XrPolygonAddEdge (&polygon, &in->pt, inpt);
	XrPolygonAddEdge (&polygon, inpt, outpt);
	XrPolygonAddEdge (&polygon, outpt, &in->pt);
	break;
    }
    }

    err = XrTrapsTessellatePolygon (stroker->traps, &polygon, 1);
    XrPolygonDeinit (&polygon);

    return err;
}

static XrError
_XrStrokerCap(XrStroker *stroker, XrStrokeFace *f)
{
    XrError	    err;
    XrGState	    *gstate = stroker->gstate;
    XrPolygon	    polygon;

    if (gstate->line_cap == XrLineCapButt)
	return XrErrorSuccess;
    
    XrPolygonInit (&polygon);
    switch (gstate->line_cap) {
    case XrLineCapRound: {
	break;
    }
    case XrLineCapSquare: {
	XPointDouble    vector = f->vector;
	XPointFixed	fvector;
	XPointFixed	occw, ocw;
	vector.x *= gstate->line_width / 2.0;
	vector.y *= gstate->line_width / 2.0;
	XrTransformPointWithoutTranslate(&gstate->ctm, &vector);
	fvector.x = XDoubleToFixed(vector.x);
	fvector.y = XDoubleToFixed(vector.y);
	occw.x = f->ccw.x + fvector.x;
	occw.y = f->ccw.y + fvector.y;
	ocw.x = f->cw.x + fvector.x;
	ocw.y = f->cw.y + fvector.y;

	XrPolygonAddEdge (&polygon, &f->cw, &ocw);
	XrPolygonAddEdge (&polygon, &ocw, &occw);
	XrPolygonAddEdge (&polygon, &occw, &f->ccw);
	XrPolygonAddEdge (&polygon, &f->ccw, &f->cw);
	break;
    }
    case XrLineCapButt: {
	break;
    }
    }

    err = XrTrapsTessellatePolygon (stroker->traps, &polygon, 1);
    XrPolygonDeinit (&polygon);

    return err;
}

static XrError
XrStrokerAddSubEdge (XrStroker *stroker, XPointFixed *p1, XPointFixed *p2,
		     XrStrokeFace *start, XrStrokeFace *end)
{
    XrError err;
    XrGState *gstate = stroker->gstate;
    XrTraps *traps = stroker->traps;
    double mag, tmp;
    XPointDouble vector;
    XPointDouble user_vector;
    XPointFixed offset_ccw, offset_cw;
    XPointFixed quad[4];

    vector.x = XFixedToDouble(p2->x - p1->x);
    vector.y = XFixedToDouble(p2->y - p1->y);

    XrTransformPointWithoutTranslate(&gstate->ctm_inverse, &vector);

    mag = sqrt(vector.x * vector.x + vector.y * vector.y);
    if (mag == 0) {
	return XrErrorSuccess;
    }

    vector.x /= mag;
    vector.y /= mag;

    user_vector = vector;

    tmp = vector.x;
    vector.x = - vector.y * (gstate->line_width / 2.0);
    vector.y = tmp * (gstate->line_width / 2.0);

    XrTransformPointWithoutTranslate(&gstate->ctm, &vector);

    offset_ccw.x = XDoubleToFixed(vector.x);
    offset_ccw.y = XDoubleToFixed(vector.y);
    offset_cw.x = -offset_ccw.x;
    offset_cw.y = -offset_ccw.y;

    quad[0] = *p1;
    _TranslatePoint(&quad[0], &offset_cw);
    
    quad[1] = *p1;
    _TranslatePoint(&quad[1], &offset_ccw);

    quad[2] = *p2;
    _TranslatePoint(&quad[2], &offset_ccw);

    quad[3] = *p2;
    _TranslatePoint(&quad[3], &offset_cw);
    
    start->cw = quad[0];
    start->pt = *p1;
    start->ccw = quad[1];
    start->vector.x = -user_vector.x;
    start->vector.y = -user_vector.y;
    
    end->ccw = quad[2];
    end->pt = *p2;
    end->cw = quad[3];
    end->vector = user_vector;
    
    return XrTrapsTessellateRectangle(traps, quad);
}

XrError
XrStrokerAddEdge(void *closure, XPointFixed *p1, XPointFixed *p2)
{
    XrError err;
    XrStroker *stroker = closure;
    XrStrokeFace start, end;
    
    err = XrStrokerAddSubEdge (stroker, p1, p2, &start, &end);
    if (err)
	return err;

    if (stroker->have_prev) {
	err = _XrStrokerJoin (stroker, &stroker->prev, &start);
	if (err)
	    return err;
    } else {
	stroker->have_prev = 1;
	if (stroker->is_first) {
	    stroker->have_first = 1;
	    stroker->first = start;
	}
    }
    stroker->prev = end;
    stroker->is_first = 0;
    return XrErrorSuccess;
}

/*
 * Dashed lines.  Cap each dash end, join around turns when on
 */
XrError
XrStrokerAddEdgeDashed (void *closure, XPointFixed *p1, XPointFixed *p2)
{
    XrError err = XrErrorSuccess;
    XrStroker *stroker = closure;
    XrGState *gstate = stroker->gstate;
    double mag, remain, tmp;
    XPointDouble vector, d1, d2;
    XPointFixed fd1, fd2;
    int first = 1;
    XrStrokeFace start, end, sub_start, sub_end;
    
    vector.x = XFixedToDouble(p2->x - p1->x);
    vector.y = XFixedToDouble(p2->y - p1->y);

    XrTransformPointWithoutTranslate(&gstate->ctm_inverse, &vector);

    mag = sqrt(vector.x * vector.x + vector.y * vector.y);
    remain = mag;
    fd1 = *p1;
    while (remain)
    {
	tmp = stroker->dash_remain;
	if (tmp > remain)
	    tmp = remain;
	remain -= tmp;
        d2.x = vector.x * (mag - remain)/mag;
	d2.y = vector.y * (mag - remain)/mag;
	XrTransformPointWithoutTranslate (&gstate->ctm, &d2);
	fd2.x = XDoubleToFixed (d2.x);
	fd2.y = XDoubleToFixed (d2.y);
	fd2.x += p1->x;
	fd2.y += p1->y;
	/*
	 * XXX simplify this case analysis
	 */
	if (stroker->dash_on) {
	    err = XrStrokerAddSubEdge (stroker, &fd1, &fd2, &sub_start, &sub_end);
	    if (err)
		return err;
	    if (!first) {
		/*
		 * Not first dash in this segment, cap start
		 */
		err = _XrStrokerCap (stroker, &sub_start);
		if (err)
		    return err;
	    } else {
		/*
		 * First in this segment, join to any prev, else
		 * if at start of sub-path, mark position, else
		 * cap
		 */
		if (stroker->have_prev) {
		    err = _XrStrokerJoin (stroker, &stroker->prev, &sub_start);
		    if (err)
			return err;
		} else {
		    if (stroker->is_first) {
			stroker->have_first = 1;
			stroker->first = sub_start;
		    } else {
			err = _XrStrokerCap (stroker, &sub_start);
			if (err)
			    return err;
		    }
		}
	    }
	    if (remain) {
		/*
		 * Cap if not at end of segment
		 */
		err = _XrStrokerCap (stroker, &sub_end);
		if (err)
		    return err;
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
		    err = _XrStrokerCap (stroker, &stroker->prev);
		    if (err)
			return err;
		}
	    }
	    if (!remain)
		stroker->have_prev = 0;
	}
	XrStrokerStepDash (stroker, tmp);
	fd1 = fd2;
	first = 0;
    }
    stroker->is_first = 0;
    return err;
}

XrError
XrStrokerAddSpline (void *closure, XPointFixed *a, XPointFixed *b, XPointFixed *c, XPointFixed *d)
{
    XrError err = XrErrorSuccess;
    XrStroker *stroker = closure;
    XrSpline spline;
    XrPolygon polygon;
    XrPen pen;

    XrSplineInit(&spline, a, b, c, d);
    err = XrSplineDecompose(&spline, stroker->gstate->tolerance);
    if (err)
	goto CLEANUP_SPLINE;

    XrPolygonInit(&polygon);

    err = XrPenInitCopy(&pen, &stroker->gstate->pen_regular);
    if (err)
	goto CLEANUP_POLYGON;

    XrPenAddPointsForSlopes(&pen, a, b, c, d);

    err = XrPenStrokePoints(&pen, spline.pt, spline.num_pts, &polygon);
    if (err)
	goto CLEANUP_PEN;

    err = XrTrapsTessellatePolygon(stroker->traps, &polygon, 1);

  CLEANUP_PEN:
    XrPenDeinit(&pen);
  CLEANUP_POLYGON:
    XrPolygonDeinit(&polygon);
  CLEANUP_SPLINE:
    XrSplineDeinit(&spline);

    return err;
}

XrError
XrStrokerDoneSubPath (void *closure, XrSubPathDone done)
{
    XrError err;
    XrStroker *stroker = closure;

    switch (done) {
    case XrSubPathDoneJoin:
	if (stroker->have_first && stroker->have_prev) {
	    err = _XrStrokerJoin (stroker, &stroker->prev, &stroker->first);
	    if (err)
		return err;
	    break;
	}
	/* fall through... */
    case XrSubPathDoneCap:
	if (stroker->have_first) {
	    err = _XrStrokerCap (stroker, &stroker->first);
	    if (err)
		return err;
	}
	if (stroker->have_prev) {
	    err = _XrStrokerCap (stroker, &stroker->prev);
	    if (err)
		return err;
	}
	break;
    }

    return XrErrorSuccess;
}

XrError
XrStrokerDonePath (void *closure)
{
    return XrErrorSuccess;
}
