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

#include <stdlib.h>
#include "xrint.h"

#define XR_POLYGON_GROWTH_INC 10

/* private functions */

static XrStatus
_XrPolygonGrowBy(XrPolygon *polygon, int additional);

static void
_XrPolygonSetLastPoint(XrPolygon *polygon, XPointFixed *pt);


void
_XrPolygonInit(XrPolygon *polygon)
{
    polygon->num_edges = 0;

    polygon->edges_size = 0;
    polygon->edges = NULL;

    polygon->first_pt_defined = 0;
    polygon->last_pt_defined = 0;

    polygon->closed = 0;
}

void
_XrPolygonDeinit(XrPolygon *polygon)
{
    if (polygon->edges_size) {
	free(polygon->edges);
	polygon->edges = NULL;
	polygon->edges_size = 0;
	polygon->num_edges = 0;
    }

    polygon->first_pt_defined = 0;
    polygon->last_pt_defined = 0;

    polygon->closed = 0;
}

static XrStatus
_XrPolygonGrowBy(XrPolygon *polygon, int additional)
{
    XrEdge *new_edges;
    int old_size = polygon->edges_size;
    int new_size = polygon->num_edges + additional;

    if (new_size <= polygon->edges_size) {
	return XrStatusSuccess;
    }

    polygon->edges_size = new_size;
    new_edges = realloc(polygon->edges, polygon->edges_size * sizeof(XrEdge));

    if (new_edges == NULL) {
	polygon->edges_size = old_size;
	return XrStatusNoMemory;
    }

    polygon->edges = new_edges;

    return XrStatusSuccess;
}

static void
_XrPolygonSetLastPoint(XrPolygon *polygon, XPointFixed *pt)
{
    polygon->last_pt = *pt;
    polygon->last_pt_defined = 1;
}

XrStatus
_XrPolygonAddEdge(XrPolygon *polygon, XPointFixed *p1, XPointFixed *p2)
{
    XrStatus status;
    XrEdge *edge;

    if (! polygon->first_pt_defined) {
	polygon->first_pt = *p1;
	polygon->first_pt_defined = 1;
    }

    /* drop horizontal edges */
    if (p1->y == p2->y) {
	goto DONE;
    }

    if (polygon->num_edges >= polygon->edges_size) {
	status = _XrPolygonGrowBy(polygon, XR_POLYGON_GROWTH_INC);
	if (status) {
	    return status;
	}
    }

    edge = &polygon->edges[polygon->num_edges];
    if (p1->y < p2->y) {
	edge->edge.p1 = *p1;
	edge->edge.p2 = *p2;
	edge->clockWise = True;
    } else {
	edge->edge.p1 = *p2;
	edge->edge.p2 = *p1;
	edge->clockWise = False;
    }

    polygon->num_edges++;

  DONE:
    _XrPolygonSetLastPoint(polygon, p2);

    return XrStatusSuccess;
}

XrStatus
_XrPolygonAddPoint(XrPolygon *polygon, XPointFixed *pt)
{
    XrStatus status = XrStatusSuccess;

    if (polygon->last_pt_defined) {
	status = _XrPolygonAddEdge(polygon, &polygon->last_pt, pt);
    } else {
	_XrPolygonSetLastPoint(polygon, pt);
    }

    return status;
}

XrStatus
_XrPolygonClose(XrPolygon *polygon)
{
    XrStatus status;

    if (polygon->closed == 0 && polygon->last_pt_defined) {
	status = _XrPolygonAddEdge(polygon, &polygon->last_pt, &polygon->first_pt);
	if (status)
	    return status;

	polygon->closed = 1;
    }

    return XrStatusSuccess;
}
