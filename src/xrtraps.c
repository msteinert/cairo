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

typedef struct _Edge Edge;

struct _Edge {
    XLineFixed	edge;
    XFixed	current_x;
    XFixed	next_x;
    Bool	clockWise;
    Edge	*next, *prev;
};

/* private functions */

static void
_XrTrapsGrowBy(XrTraps *traps, int additional);

static int
_ComparePointFixedByY (const void *v1, const void *v2);

static int
_CompareEdgeByTop (const void *v1, const void *v2);

static XFixed
_ComputeX (XLineFixed *line, XFixed y);

static double
_ComputeInverseSlope (XLineFixed *l);

static double
_ComputeXIntercept (XLineFixed *l, double inverse_slope);

static XFixed
_ComputeIntersect (XLineFixed *l1, XLineFixed *l2);

static void
_XrTrapsTessellateEdges (XrTraps	*traps,
			 Edge		*edges,
			 int		nedges,
			 int		winding);

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
XrTrapsAddTrap(XrTraps *traps, XFixed top, XFixed bottom,
	       XLineFixed left, XLineFixed right)
{
    XTrapezoid *trap;

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
XrTrapsAddTrapFromPoints(XrTraps *traps, XFixed top, XFixed bottom,
			 XPointFixed left_p1, XPointFixed left_p2,
			 XPointFixed right_p1, XPointFixed right_p2)
{
    XLineFixed left;
    XLineFixed right;

    left.p1 = left_p1;
    left.p2 = left_p2;

    right.p1 = right_p1;
    right.p2 = right_p2;

    XrTrapsAddTrap(traps, top, bottom, left, right);
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

    return p1->y - p2->y;
}

void
XrTrapsTessellateTriangle (XrTraps *traps, XPointDouble *tri)
{
    XPointFixed t[4];
    int i;

    for (i = 0; i < 3; i++) {
	t[i].x = XDoubleToFixed(tri[i].x);
	t[i].y = XDoubleToFixed(tri[i].y);
    }

    qsort(t, 3, sizeof(XPointFixed), _ComparePointFixedByY);

    if (t[1].x > t[2].x) {
	XrTrapsAddTrapFromPoints(traps, t[0].y, t[1].y, t[0], t[2], t[0], t[1]);
	XrTrapsAddTrapFromPoints(traps, t[1].y, t[2].y, t[0], t[2], t[1], t[2]);
    } else {
	XrTrapsAddTrapFromPoints(traps, t[0].y, t[1].y, t[0], t[1], t[0], t[2]);
	XrTrapsAddTrapFromPoints(traps, t[1].y, t[2].y, t[1], t[2], t[0], t[2]);
    }
}

void
XrTrapsTessellateConvexQuad (XrTraps *traps, XPointDouble *quad)
{
    XPointFixed q[4];
    int i;

    for (i = 0; i < 4; i++) {
	q[i].x = XDoubleToFixed(quad[i].x);
	q[i].y = XDoubleToFixed(quad[i].y);
    }

    qsort(q, 4, sizeof(XPointFixed), _ComparePointFixedByY);

    if (q[1].x > q[2].x) {
	XrTrapsAddTrapFromPoints(traps, q[0].y, q[1].y, q[0], q[2], q[0], q[1]);
	XrTrapsAddTrapFromPoints(traps, q[1].y, q[2].y, q[0], q[2], q[1], q[3]);
	XrTrapsAddTrapFromPoints(traps, q[2].y, q[3].y, q[2], q[3], q[1], q[3]);
    } else {
	XrTrapsAddTrapFromPoints(traps, q[0].y, q[1].y, q[0], q[1], q[0], q[2]);
	XrTrapsAddTrapFromPoints(traps, q[1].y, q[2].y, q[1], q[3], q[0], q[2]);
	XrTrapsAddTrapFromPoints(traps, q[2].y, q[3].y, q[1], q[3], q[2], q[3]);
    }
}

void
XrTrapsTessellatePath (XrTraps *traps, XrPath *path, int winding)
{
    XrSubPath *subpath;

    for (subpath = path->head; subpath; subpath = subpath->next) {
	XrTrapsTessellateSubPath(traps, subpath, winding);
    }
}

void
XrTrapsTessellateSubPath (XrTraps *traps, XrSubPath *subpath, int winding)
{
    Edge	    *edges;
    int		    i, nedges, npoints = subpath->num_pts;
    XFixed	    x, y, prevx = 0, prevy = 0, firstx = 0, firsty = 0;
    XFixed	    top = 0, bottom = 0;	/* GCCism */

    edges = (Edge *) Xmalloc (npoints * sizeof (Edge));
    if (!edges)
	return;

    _XrTrapsGrowBy(traps, npoints * npoints);

    /* XXX: CLEANUP: This code is still in the same form it was in
       when I brought it over from Xrender. The right thing to do here
       is to probably move the edge information into XrPath/XrSubPath
       and construct all edges during path construction. */
    nedges = 0;
    for (i = 0; i <= npoints; i++) {
	if (i == npoints) {
	    x = firstx;
	    y = firsty;
	}
	else {
	    x = XDoubleToFixed (subpath->pts[i].x);
	    y = XDoubleToFixed (subpath->pts[i].y);
	}
	if (i) {
	    if (y < top) {
		top = y;
	    } else if (y > bottom) {
		bottom = y;
	    }
	    if (prevy < y) {
		edges[nedges].edge.p1.x = prevx;
		edges[nedges].edge.p1.y = prevy;
		edges[nedges].edge.p2.x = x;
		edges[nedges].edge.p2.y = y;
		edges[nedges].clockWise = True;
		nedges++;
	    }
	    else if (prevy > y)	{
		edges[nedges].edge.p1.x = x;
		edges[nedges].edge.p1.y = y;
		edges[nedges].edge.p2.x = prevx;
		edges[nedges].edge.p2.y = prevy;
		edges[nedges].clockWise = False;
		nedges++;
	    }
	    /* drop horizontal edges */
	} else {
	    top = y;
	    bottom = y;
	    firstx = x;
	    firsty = y;
	}
	prevx = x;
	prevy = y;
    }
    
    _XrTrapsTessellateEdges (traps, edges, nedges, winding);
    Xfree (edges);
}

static int
_CompareEdgeByTop (const void *v1, const void *v2)
{
    const Edge	*e1 = v1, *e2 = v2;

    return e1->edge.p1.y - e2->edge.p1.y;
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

static void
_XrTrapsTessellateEdges (XrTraps	*traps,
			 Edge		*edges,
			 int		nedges,
			 int		winding)
{
    int		inactive;
    Edge	*active;
    Edge	*e, *en, *next;
    XFixed	y, next_y, intersect;
    int		in_out;
    
    qsort (edges, nedges, sizeof (Edge), _CompareEdgeByTop);
    
    y = edges[0].edge.p1.y;
    active = 0;
    inactive = 0;
    while (active || inactive < nedges)
    {
	/* insert new active edges into list */
	while (inactive < nedges)
	{
	    e = &edges[inactive];
	    if (e->edge.p1.y > y)
		break;
	    /* move this edge into the active list */
	    inactive++;
	    e->next_x = _ComputeX (&e->edge, y);
	    e->next = active;
	    e->prev = 0;
	    if (active)
		active->prev = e;
	    active = e;
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
	if (inactive < nedges && edges[inactive].edge.p1.y < next_y)
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
	    XrTrapsAddTrap(traps, y, next_y, e->edge, en->edge);
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
