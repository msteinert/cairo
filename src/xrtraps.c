/*
 * $XFree86: $
 *
 * Copyright © 2002 Keith Packard, member of The XFree86 Project, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Keith Packard not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Keith Packard makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * KEITH PACKARD DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL KEITH PACKARD BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 * 2002-07-15: Converted from XRenderCompositeDoublePoly to XrTrap. Carl D. Worth
 */

#include "xrint.h"

#define XR_TRAPS_GROWTH_INC 10

/* private functions */

static void
_XrTrapsGrowBy(XrTraps *traps, int additional);

void
_XrTrapsAddTrap(XrTraps *traps, XFixed top, XFixed bottom,
		XLineFixed left, XLineFixed right);

void
_XrTrapsAddTrapFromPoints(XrTraps *traps, XFixed top, XFixed bottom,
			  XPointFixed left_p1, XPointFixed left_p2,
			  XPointFixed right_p1, XPointFixed right_p2);

static int
_ComparePointFixedByY (const void *v1, const void *v2);

static int
_CompareXrEdgeByTop (const void *v1, const void *v2);

static XFixed
_ComputeX (XLineFixed *line, XFixed y);

static double
_ComputeInverseSlope (XLineFixed *l);

static double
_ComputeXIntercept (XLineFixed *l, double inverse_slope);

static XFixed
_ComputeIntersect (XLineFixed *l1, XLineFixed *l2);

XrTraps *
XrTrapsCreate(void)
{
    XrTraps *traps;

    traps = Xmalloc(sizeof(XrTraps));

    XrTrapsInit(traps);

    return traps;
}

void
XrTrapsInit(XrTraps *traps)
{
    traps->num_xtraps = 0;

    traps->xtraps_size = 0;
    traps->xtraps = NULL;
}

void
XrTrapsDeinit(XrTraps *traps)
{
    if (traps->xtraps_size) {
	free(traps->xtraps);
	traps->xtraps_size = 0;
	traps->num_xtraps = 0;
    }
}

void
XrTrapsDestroy(XrTraps *traps)
{
    XrTrapsDeinit(traps);
    Xfree(traps);
}

void
_XrTrapsAddTrap(XrTraps *traps, XFixed top, XFixed bottom,
		XLineFixed left, XLineFixed right)
{
    XTrapezoid *trap;

    if (top == bottom) {
	return;
    }

    if (traps->num_xtraps >= traps->xtraps_size) {
	_XrTrapsGrowBy(traps, XR_TRAPS_GROWTH_INC);
    }

    trap = &traps->xtraps[traps->num_xtraps];
    trap->top = top;
    trap->bottom = bottom;
    trap->left = left;
    trap->right = right;

    traps->num_xtraps++;
}

void
_XrTrapsAddTrapFromPoints(XrTraps *traps, XFixed top, XFixed bottom,
			  XPointFixed left_p1, XPointFixed left_p2,
			  XPointFixed right_p1, XPointFixed right_p2)
{
    XLineFixed left;
    XLineFixed right;

    left.p1 = left_p1;
    left.p2 = left_p2;

    right.p1 = right_p1;
    right.p2 = right_p2;

    _XrTrapsAddTrap(traps, top, bottom, left, right);
}

static void
_XrTrapsGrowBy(XrTraps *traps, int additional)
{
    XTrapezoid *new_xtraps;
    int old_size = traps->xtraps_size;
    int new_size = traps->num_xtraps + additional;

    if (new_size <= traps->xtraps_size) {
	return;
    }

    traps->xtraps_size = new_size;
    new_xtraps = realloc(traps->xtraps, traps->xtraps_size * sizeof(XTrapezoid));

    if (new_xtraps) {
	traps->xtraps = new_xtraps;
    } else {
	/* XXX: BUG: How do we really want to handle this out of memory error? */
	traps->xtraps_size = old_size;
    }
}

static int
_ComparePointFixedByY (const void *v1, const void *v2)
{
    const XPointFixed	*p1 = v1, *p2 = v2;

    int ret = p1->y - p2->y;
    if (ret == 0) {
	ret = p1->x - p2->x;
    }
    return ret;
}

void
XrTrapsTessellateRectangle (XrTraps *traps, XPointFixed q[4])
{
    qsort(q, 4, sizeof(XPointFixed), _ComparePointFixedByY);

    if (q[1].x > q[2].x) {
	_XrTrapsAddTrapFromPoints(traps, q[0].y, q[1].y, q[0], q[2], q[0], q[1]);
	_XrTrapsAddTrapFromPoints(traps, q[1].y, q[2].y, q[0], q[2], q[1], q[3]);
	_XrTrapsAddTrapFromPoints(traps, q[2].y, q[3].y, q[2], q[3], q[1], q[3]);
    } else {
	_XrTrapsAddTrapFromPoints(traps, q[0].y, q[1].y, q[0], q[1], q[0], q[2]);
	_XrTrapsAddTrapFromPoints(traps, q[1].y, q[2].y, q[1], q[3], q[0], q[2]);
	_XrTrapsAddTrapFromPoints(traps, q[2].y, q[3].y, q[1], q[3], q[2], q[3]);
    }
}

static int
_CompareXrEdgeByTop (const void *v1, const void *v2)
{
    const XrEdge *e1 = v1, *e2 = v2;
    int ret;

    ret = e1->edge.p1.y - e2->edge.p1.y;
    if (ret == 0)
	ret = e1->edge.p1.x - e2->edge.p1.x;
    return ret;
}

static XFixed
_ComputeX (XLineFixed *line, XFixed y)
{
    XFixed  dx = line->p2.x - line->p1.x;
    double  ex = (double) (y - line->p1.y) * (double) dx;
    XFixed  dy = line->p2.y - line->p1.y;

    return (XFixed) line->p1.x + (XFixed) (ex / dy);
}

static double
_ComputeInverseSlope (XLineFixed *l)
{
    return (XFixedToDouble (l->p2.x - l->p1.x) / 
	    XFixedToDouble (l->p2.y - l->p1.y));
}

static double
_ComputeXIntercept (XLineFixed *l, double inverse_slope)
{
    return XFixedToDouble (l->p1.x) - inverse_slope * XFixedToDouble (l->p1.y);
}

static XFixed
_ComputeIntersect (XLineFixed *l1, XLineFixed *l2)
{
    /*
     * x = m1y + b1
     * x = m2y + b2
     * m1y + b1 = m2y + b2
     * y * (m1 - m2) = b2 - b1
     * y = (b2 - b1) / (m1 - m2)
     */
    double  m1 = _ComputeInverseSlope (l1);
    double  b1 = _ComputeXIntercept (l1, m1);
    double  m2 = _ComputeInverseSlope (l2);
    double  b2 = _ComputeXIntercept (l2, m2);

    return XDoubleToFixed ((b2 - b1) / (m1 - m2));
}

void
XrTrapsTessellatePolygon (XrTraps	*traps,
			  XrPolygon	*poly,
			  int		winding)
{
    int		inactive;
    XrEdge	*active;
    XrEdge	*e, *en, *ep, *next;
    XFixed	y, next_y, intersect;
    int		in_out, num_edges = poly->num_edges;
    XrEdge	*edges = poly->edges;

    if (num_edges == 0)
	return;

    qsort (edges, num_edges, sizeof (XrEdge), _CompareXrEdgeByTop);
    
    y = edges[0].edge.p1.y;
    active = 0;
    inactive = 0;
    while (active || inactive < num_edges)
    {
	/* insert new active edges into list */
	while (inactive < num_edges)
	{
	    e = &edges[inactive];
	    if (e->edge.p1.y > y)
		break;
	    /* move this edge into the active list */
	    inactive++;
	    e->next_x = _ComputeX (&e->edge, y);

	    /* insert e at sorted position */
	    for (en=active, ep=0; en; ep=en, en=en->next)
	    {
		if (en->next_x > e->next_x)
		    break;
	    }
	    e->next = en;
	    e->prev = ep;
	    if (ep)
		ep->next = e;
	    else
		active = e;
	    if (en)
		en->prev = e;
	}

	/* find next inflection point */
	next_y = active->edge.p2.y;
	for (e = active; e; e = en)
	{
	    if (e->edge.p2.y < next_y)
		next_y = e->edge.p2.y;
	    en = e->next;
	    /* check intersect */
	    if (en && e->edge.p2.x > en->edge.p2.x) 
	    {
		intersect = _ComputeIntersect (&e->edge, &e->next->edge);
		/* make sure this point is below the actual intersection */
		intersect = intersect + 1;
		if (intersect < next_y && intersect > y)
		    next_y = intersect;
	    }
	}
	/* check next inactive point */
	if (inactive < num_edges && edges[inactive].edge.p1.y < next_y)
	    next_y = edges[inactive].edge.p1.y;

	/* compute x coordinates along this group */
	for (e = active; e; e = e->next) {
	    e->current_x = e->next_x;
	    e->next_x = _ComputeX (&e->edge, next_y);
	}
	
	/* sort active list */
	for (e = active; e; e = next)
	{
	    next = e->next;
	    /*
	     * Find one later in the list that belongs before the
	     * current one
	     */
	    for (en = next; en; en = en->next)
	    {
		if (en->current_x < e->current_x ||
		    (en->current_x == e->current_x &&
		     en->next_x < e->next_x))
		{
		    /*
		     * insert en before e
		     *
		     * extract en
		     */
		    en->prev->next = en->next;
		    if (en->next)
			en->next->prev = en->prev;
		    /*
		     * insert en
		     */
		    if (e->prev)
			e->prev->next = en;
		    else
			active = en;
		    en->prev = e->prev;
		    e->prev = en;
		    en->next = e;
		    /*
		     * start over at en
		     */
		    next = en;
		    break;
		}
	    }
	}
#if 0
	printf ("y: %6.3g:", y / 65536.0);
	for (e = active; e; e = e->next)
	{
	    printf (" %6.3g", e->current_x / 65536.0);
	}
	printf ("\n");
#endif
	
	/* walk the list generating trapezoids */
	in_out = 0;
	for (e = active; e && (en = e->next); e = e->next)
	{
	    if (winding) {
		if (e->clockWise) {
		    in_out++;
		} else {
		    in_out--;
		}
		if (in_out == 0) {
		    continue;
		}
	    } else {
		in_out++;
		if (in_out % 2 == 0) {
		    continue;
		}
	    }
	    _XrTrapsAddTrap(traps, y, next_y, e->edge, en->edge);
	}

	y = next_y;
	
	/* delete inactive edges from list */
	for (e = active; e; e = next)
	{
	    next = e->next;
	    if (e->edge.p2.y <= y)
	    {
		if (e->prev)
		    e->prev->next = e->next;
		else
		    active = e->next;
		if (e->next)
		    e->next->prev = e->prev;
	    }
	}
    }
}
