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

static XrError
_XrTrapsGrowBy(XrTraps *traps, int additional);

XrError
_XrTrapsAddTrap(XrTraps *traps, XFixed top, XFixed bottom,
		XLineFixed left, XLineFixed right);

XrError
_XrTrapsAddTrapFromPoints(XrTraps *traps, XFixed top, XFixed bottom,
			  XPointFixed left_p1, XPointFixed left_p2,
			  XPointFixed right_p1, XPointFixed right_p2);

static int
_ComparePointFixedByY (const void *av, const void *bv);

static int
_CompareXrEdgeByTop (const void *av, const void *bv);

static XFixed
_ComputeX (XLineFixed *line, XFixed y);

static double
_ComputeInverseSlope (XLineFixed *l);

static double
_ComputeXIntercept (XLineFixed *l, double inverse_slope);

static XFixed
_LinesIntersect (XLineFixed *l1, XLineFixed *l2, XFixed *intersection);

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
	traps->xtraps = NULL;
	traps->xtraps_size = 0;
	traps->num_xtraps = 0;
    }
}

XrError
_XrTrapsAddTrap(XrTraps *traps, XFixed top, XFixed bottom,
		XLineFixed left, XLineFixed right)
{
    XrError err;
    XTrapezoid *trap;

    if (top == bottom) {
	return XrErrorSuccess;
    }

    if (traps->num_xtraps >= traps->xtraps_size) {
	err = _XrTrapsGrowBy(traps, XR_TRAPS_GROWTH_INC);
	if (err)
	    return err;
    }

    trap = &traps->xtraps[traps->num_xtraps];
    trap->top = top;
    trap->bottom = bottom;
    trap->left = left;
    trap->right = right;

    traps->num_xtraps++;

    return XrErrorSuccess;
}

XrError
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

    return _XrTrapsAddTrap(traps, top, bottom, left, right);
}

static XrError
_XrTrapsGrowBy(XrTraps *traps, int additional)
{
    XTrapezoid *new_xtraps;
    int old_size = traps->xtraps_size;
    int new_size = traps->num_xtraps + additional;

    if (new_size <= traps->xtraps_size) {
	return XrErrorSuccess;
    }

    traps->xtraps_size = new_size;
    new_xtraps = realloc(traps->xtraps, traps->xtraps_size * sizeof(XTrapezoid));

    if (new_xtraps == NULL) {
	traps->xtraps_size = old_size;
	return XrErrorNoMemory;
    }

    traps->xtraps = new_xtraps;

    return XrErrorSuccess;
}

static int
_ComparePointFixedByY (const void *av, const void *bv)
{
    const XPointFixed	*a = av, *b = bv;

    int ret = a->y - b->y;
    if (ret == 0) {
	ret = a->x - b->x;
    }
    return ret;
}

XrError
XrTrapsTessellateRectangle (XrTraps *traps, XPointFixed q[4])
{
    XrError err;

    qsort(q, 4, sizeof(XPointFixed), _ComparePointFixedByY);

    if (q[1].x > q[2].x) {
	err = _XrTrapsAddTrapFromPoints(traps, q[0].y, q[1].y, q[0], q[2], q[0], q[1]);
	if (err)
	    return err;
	err = _XrTrapsAddTrapFromPoints(traps, q[1].y, q[2].y, q[0], q[2], q[1], q[3]);
	if (err)
	    return err;
	err = _XrTrapsAddTrapFromPoints(traps, q[2].y, q[3].y, q[2], q[3], q[1], q[3]);
	if (err)
	    return err;
    } else {
	err = _XrTrapsAddTrapFromPoints(traps, q[0].y, q[1].y, q[0], q[1], q[0], q[2]);
	if (err)
	    return err;
	err = _XrTrapsAddTrapFromPoints(traps, q[1].y, q[2].y, q[1], q[3], q[0], q[2]);
	if (err)
	    return err;
	err = _XrTrapsAddTrapFromPoints(traps, q[2].y, q[3].y, q[1], q[3], q[2], q[3]);
	if (err)
	    return err;
    }

    return XrErrorSuccess;
}

static int
_CompareXrEdgeByTop (const void *av, const void *bv)
{
    const XrEdge *a = av, *b = bv;
    int ret;

    ret = a->edge.p1.y - b->edge.p1.y;
    if (ret == 0)
	ret = a->edge.p1.x - b->edge.p1.x;
    return ret;
}

/* Return value is:
   > 0 if a is "clockwise" from b, (in a mathematical, not a graphical sense)
   == 0 if slope(a) == slope(b)
   < 0 if a is "counter-clockwise" from b
*/
static int
_CompareXrEdgeBySlope (const void *av, const void *bv)
{
    const XrEdge *a = av, *b = bv;

    double a_dx = XFixedToDouble(a->edge.p2.x - a->edge.p1.x);
    double a_dy = XFixedToDouble(a->edge.p2.y - a->edge.p1.y);
    double b_dx = XFixedToDouble(b->edge.p2.x - b->edge.p1.x);
    double b_dy = XFixedToDouble(b->edge.p2.y - b->edge.p1.y);

    return b_dy * a_dx - a_dy * b_dx;
}

static int
_CompareXrEdgeByCurrentXThenSlope (const void *av, const void *bv)
{
    const XrEdge *a = av, *b = bv;
    int ret;

    ret = a->current_x - b->current_x;
    if (ret == 0)
	ret = _CompareXrEdgeBySlope(a, b);
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

static int
_LinesIntersect (XLineFixed *l1, XLineFixed *l2, XFixed *intersection)
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

    if (m1 == m2)
	return 0;

    *intersection = XDoubleToFixed ((b2 - b1) / (m1 - m2));
    return 1;
}

static void
_SortEdgeList(XrEdge **active)
{
    XrEdge	*e, *en, *next;

    /* sort active list */
    for (e = *active; e; e = next)
    {
	next = e->next;
	/*
	 * Find one later in the list that belongs before the
	 * current one
	 */
	for (en = next; en; en = en->next)
	{
	    if (_CompareXrEdgeByCurrentXThenSlope(e, en) > 0)
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
		    *active = en;
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
}

/* The algorithm here is pretty simple:

   inactive = [edges]
   y = min_p1_y(inactive)

   while (num_active || num_inactive) {
   	active = all edges containing y

	next_y = min( min_p2_y(active), min_p1_y(inactive), min_intersection(active) )

	fill_traps(active, y, next_y, winding)

	y = next_y
   }

   The invariants that hold during fill_traps are:

   	All edges in active contain both y and next_y
	No edges in active intersect within y and next_y

   These invariants mean that fill_traps is as simple as sorting the
   active edges, forming a trapezoid between each adjacent pair. Then,
   either the even-odd or winding rule is used to determine whether to
   emit each of these trapezoids.
*/
XrError
XrTrapsTessellatePolygon (XrTraps	*traps,
			  XrPolygon	*poly,
			  int		winding)
{
    XrError	err;
    int		inactive;
    XrEdge	*active;
    XrEdge	*e, *en, *next;
    XFixed	y, next_y, intersect;
    int		in_out, num_edges = poly->num_edges;
    XrEdge	*edges = poly->edges;

    if (num_edges == 0)
	return XrErrorSuccess;

    qsort (edges, num_edges, sizeof (XrEdge), _CompareXrEdgeByTop);
    
    y = edges[0].edge.p1.y;
    active = 0;
    inactive = 0;
    while (active || inactive < num_edges)
    {
	for (e = active; e; e = e->next) {
	    e->current_x = _ComputeX (&e->edge, y);
	}

	/* insert new active edges into list */
	while (inactive < num_edges)
	{
	    e = &edges[inactive];
	    if (e->edge.p1.y > y)
		break;
	    /* move this edge into the active list */
	    inactive++;
	    e->current_x = _ComputeX (&e->edge, y);

	    /* insert e at head of list */
	    e->next = active;
	    e->prev = NULL;
	    if (active)
		active->prev = e;
	    active = e;
	}

	_SortEdgeList(&active);

	/* find next inflection point */
	next_y = active->edge.p2.y;
	for (e = active; e; e = en)
	{
	    en = e->next;

	    if (e->edge.p2.y < next_y)
		next_y = e->edge.p2.y;
	    /* check intersect */
	    if (en && e->current_x != en->current_x)
	    {
		if (_LinesIntersect (&e->edge, &en->edge, &intersect))
		    if (intersect > y) {
			/* Need to guarantee that we get all the way past
			   the intersection point so that the edges sort
			   properly next time through the loop. */
			while (_ComputeX(&e->edge, intersect) < _ComputeX(&en->edge, intersect))
			    intersect++;
			if (intersect < next_y)
			    next_y = intersect;
		    }
	    }
	}
	/* check next inactive point */
	if (inactive < num_edges && edges[inactive].edge.p1.y < next_y)
	    next_y = edges[inactive].edge.p1.y;

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
	    err = _XrTrapsAddTrap(traps, y, next_y, e->edge, en->edge);
	    if (err)
		return err;
	}

	/* delete inactive edges from list */
	for (e = active; e; e = next)
	{
	    next = e->next;
	    if (e->edge.p2.y <= next_y)
	    {
		if (e->prev)
		    e->prev->next = e->next;
		else
		    active = e->next;
		if (e->next)
		    e->next->prev = e->prev;
	    }
	}

	y = next_y;
    }
    return XrErrorSuccess;
}
