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

void
XrStrokerInit(XrStroker *stroker, XrGState *gstate, XrTraps *traps)
{
    stroker->gstate = gstate;
    stroker->traps = traps;
    stroker->have_prev = 0;
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
    switch (gstate->stroke_style.line_join) {
    case XrLineJoinRound: {
    }
    case XrLineJoinMiter: {
	XDouble	c = in->vector.x * out->vector.x + in->vector.y * out->vector.y;
	double ml = gstate->stroke_style.miter_limit;
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

    if (gstate->stroke_style.line_cap == XrLineCapButt)
	return XrErrorSuccess;
    
    XrPolygonInit (&polygon);
    switch (gstate->stroke_style.line_cap) {
    case XrLineCapRound: {
	break;
    }
    case XrLineCapSquare: {
	XPointDouble    vector = f->vector;
	XPointFixed	fvector;
	XPointFixed	outer, occw, ocw;
	vector.x *= gstate->stroke_style.line_width / 2.0;
	vector.y *= gstate->stroke_style.line_width / 2.0;
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

XrError
XrStrokerAddEdge(void *closure, XPointFixed *p1, XPointFixed *p2) 
{
    XrError err;
    XrStroker *stroker = closure;
    XrGState *gstate = stroker->gstate;
    XrStrokeStyle *style = &gstate->stroke_style;
    XrTraps *traps = stroker->traps;
    double mag, tmp;
    XPointDouble vector;
    XPointDouble user_vector;
    XPointFixed offset_ccw, offset_cw;
    XPointFixed quad[4];
    XrStrokeFace    face;

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
    vector.x = - vector.y * (style->line_width / 2.0);
    vector.y = tmp * (style->line_width / 2.0);

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
    
    face.cw = quad[0];
    face.pt = *p1;
    face.ccw = quad[1];
    face.vector.x = -user_vector.x;
    face.vector.y = -user_vector.y;
    
    if (stroker->have_prev) {
	err = _XrStrokerJoin (stroker, &stroker->prev, &face);
	if (err)
	    return err;
    } else {
	stroker->have_prev = 1;
	stroker->first = face;
    }
    
    stroker->prev.ccw = quad[2];
    stroker->prev.pt = *p2;
    stroker->prev.cw = quad[3];
    stroker->prev.vector = user_vector;
    
    return XrTrapsTessellateRectangle(traps, quad);
}

XrError
XrStrokerDoneSubPath (void *closure, XrSubPathDone done)
{
    XrError err;
    XrStroker *stroker = closure;

    switch (done) {
    case XrSubPathDoneCap:
        _XrStrokerCap (stroker, &stroker->first);
        _XrStrokerCap (stroker, &stroker->prev);
	break;
    case XrSubPathDoneJoin:
	err = _XrStrokerJoin (stroker, &stroker->prev, &stroker->first);
	if (err)
	    return err;
	break;
    }

    return XrErrorSuccess;
}
