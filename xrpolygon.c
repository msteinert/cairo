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

static XrError
_XrPolygonGrowBy(XrPolygon *poly, int additional);

void
XrPolygonInit(XrPolygon *poly)
{
    poly->num_edges = 0;

    poly->edges_size = 0;
    poly->edges = NULL;
}

void
XrPolygonDeinit(XrPolygon *poly)
{
    if (poly->edges_size) {
	free(poly->edges);
	poly->edges_size = 0;
	poly->num_edges = 0;
    }
}

static XrError
_XrPolygonGrowBy(XrPolygon *poly, int additional)
{
    XrEdge *new_edges;
    int old_size = poly->edges_size;
    int new_size = poly->num_edges + additional;

    if (new_size <= poly->edges_size) {
	return XrErrorSuccess;
    }

    poly->edges_size = new_size;
    new_edges = realloc(poly->edges, poly->edges_size * sizeof(XrEdge));

    if (new_edges == NULL) {
	poly->edges_size = old_size;
	return XrErrorNoMemory;
    }

    poly->edges = new_edges;

    return XrErrorSuccess;
}

XrError
XrPolygonAddEdge(void *closure, XPointFixed *p1, XPointFixed *p2)
{
    XrError err;
    XrEdge *edge;
    XrPolygon *poly = closure;

    /* drop horizontal edges */
    if (p1->y == p2->y) {
	return XrErrorSuccess;
    }

    if (poly->num_edges >= poly->edges_size) {
	err = _XrPolygonGrowBy(poly, XR_POLYGON_GROWTH_INC);
	if (err) {
	    return err;
	}
    }

    edge = &poly->edges[poly->num_edges];
    if (p1->y < p2->y) {
	edge->edge.p1 = *p1;
	edge->edge.p2 = *p2;
	edge->clockWise = True;
    } else {
	edge->edge.p1 = *p2;
	edge->edge.p2 = *p1;
	edge->clockWise = False;
    }

    poly->num_edges++;

    return XrErrorSuccess;
}

XrError
XrPolygonDoneSubPath (void *closure, XrSubPathDone done)
{
    return XrErrorSuccess;
}

