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

void
XrStrokerInit(XrStroker *stroker, XrGState *gstate, XrTraps *traps)
{
    stroker->gstate = gstate;
    stroker->traps = traps;
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

void
XrStrokerAddEdge(void *closure, XPointFixed *p1, XPointFixed *p2) 
{
    XrStroker *stroker = closure;
    XrGState *gstate = stroker->gstate;
    XrStrokeStyle *style = &gstate->stroke_style;
    XrTraps *traps = stroker->traps;
    double mag, tmp;
    XPointDouble vector;
    XPointFixed offset;
    XPointFixed quad[4];

    vector.x = XFixedToDouble(p2->x - p1->x);
    vector.y = XFixedToDouble(p2->y - p1->y);

    mag = sqrt(vector.x * vector.x + vector.y * vector.y);
    if (mag == 0) {
	return;
    }

    vector.x /= mag;
    vector.y /= mag;

    XrTransformPointWithoutTranslate(&gstate->ctm_inverse, &vector);

    tmp = vector.x;
    vector.x = vector.y * (style->line_width / 2.0);
    vector.y = - tmp * (style->line_width / 2.0);

    XrTransformPointWithoutTranslate(&gstate->ctm, &vector);

    offset.x = XDoubleToFixed(vector.x);
    offset.y = XDoubleToFixed(vector.y);

    quad[0] = *p1;
    _TranslatePoint(&quad[0], &offset);
    quad[1] = *p2;
    _TranslatePoint(&quad[1], &offset);

    offset.x = - offset.x;
    offset.y = - offset.y;

    quad[2] = *p2;
    _TranslatePoint(&quad[2], &offset);
    quad[3] = *p1;
    _TranslatePoint(&quad[3], &offset);

    XrTrapsTessellateConvexQuad(traps, quad);
}

/* These functions aren't written yet... */
#if 0
static void
_XrGStateStrokerCap(XrGState *gstate,
		   const XPointDouble *p1, const XPointDouble *p2,
		   XrTraps *traps)
{
    switch (gstate->line_cap) {
    case XrLineCapRound:
	/* XXX: NYI */
	break;
    case XrLineCapSquare:
	/* XXX: NYI */
	break;
    case XrLineCapButt:
    default:
	/* XXX: NYI */
	break;
    }
}

static void
_XrGStateStrokerJoin(XrGState *gstate,
		    const XPointDouble *p1, const XPointDouble *p2, const XPointDouble *p3,
		    XrTraps *traps)
{
    switch (gstate->line_join) {
    case XrLineJoinMiter:
	/* XXX: NYI */
	break;
    case XrLineJoinRound:
	/* XXX: NYI */
	break;
    case XrLineJoinBevel:
    default:
	/* XXX: NYI */
	break;
    }
}

#endif
