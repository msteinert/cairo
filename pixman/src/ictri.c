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
 */

#include "icint.h"

void
IcRasterizeTriangle (IcImage		*image,
		     const IcTriangle	*tri,
		     int		x_off,
		     int		y_off);

static void
IcPointFixedBounds (int npoint, const IcPointFixed *points, PixRegionBox *bounds)
{
    bounds->x1 = xFixedToInt (points->x);
    bounds->x2 = xFixedToInt (xFixedCeil (points->x));
    bounds->y1 = xFixedToInt (points->y);
    bounds->y2 = xFixedToInt (xFixedCeil (points->y));
    points++;
    npoint--;
    while (npoint-- > 0)
    {
	int	x1 = xFixedToInt (points->x);
	int	x2 = xFixedToInt (xFixedCeil (points->x));
	int	y1 = xFixedToInt (points->y);
	int	y2 = xFixedToInt (xFixedCeil (points->y));

	if (x1 < bounds->x1)
	    bounds->x1 = x1;
	else if (x2 > bounds->x2)
	    bounds->x2 = x2;
	if (y1 < bounds->y1)
	    bounds->y1 = y1;
	else if (y2 > bounds->y2)
	    bounds->y2 = y2;
	points++;
    }
}

static void
IcTriangleBounds (int ntri, const IcTriangle *tris, PixRegionBox *bounds)
{
    IcPointFixedBounds (ntri * 3, (IcPointFixed *) tris, bounds);
}

void
IcRasterizeTriangle (IcImage		*image,
		     const IcTriangle	*tri,
		     int		x_off,
		     int		y_off)
{
    const IcPointFixed	*top, *left, *right, *t;
    IcTrapezoid		trap[2];

    top = &tri->p1;
    left = &tri->p2;
    right = &tri->p3;
    if (left->y < top->y) {
	t = left; left = top; top = t;
    }
    if (right->y < top->y) {
	t = right; right = top; top = t;
    }
    /* XXX: This code is broken, left and right must be determined by
       comparing the angles of the two edges, (eg. we can only compare
       X coordinates if we've already intersected each edge with the
       same Y coordinate) */
    if (right->x < left->x) {
	t = right; right = left; left = t;
    }

    /*
     * Two cases:
     *
     *		+		+
     *	       / \             / \
     *	      /   \           /   \
     *	     /     +         +     \
     *      /    --           --    \
     *     /   --               --   \
     *    / ---                   --- \
     *	 +--                         --+
     */
    
    trap[0].top = top->y;
    
    trap[0].left.p1.x = top->x;
    trap[0].left.p1.y = trap[0].top;
    trap[0].left.p2.x = left->x;
    trap[0].left.p2.y = left->y;
    
    trap[0].right.p1 = trap[0].left.p1;
    trap[0].right.p2.x = right->x;
    trap[0].right.p2.y = right->y;
    
    if (right->y < left->y)
    {
	trap[0].bottom = trap[0].right.p2.y;

	trap[1].top = trap[0].bottom;
	trap[1].bottom = trap[0].left.p2.y;
	
	trap[1].left = trap[0].left;
	trap[1].right.p1 = trap[0].right.p2;
	trap[1].right.p2 = trap[0].left.p2;
    }
    else
    {
	trap[0].bottom = trap[0].left.p2.y;
	
	trap[1].top = trap[0].bottom;
	trap[1].bottom = trap[0].right.p2.y;
	
	trap[1].right = trap[0].right;
	trap[1].left.p1 = trap[0].left.p2;
	trap[1].left.p2 = trap[0].right.p2;
    }
    if (trap[0].top != trap[0].bottom)
	IcRasterizeTrapezoid (image, &trap[0], x_off, y_off);
    if (trap[1].top != trap[1].bottom)
	IcRasterizeTrapezoid (image, &trap[1], x_off, y_off);
}

void
IcCompositeTriangles (char		op,
		      IcImage		*src,
		      IcImage		*dst,
		      int		xSrc,
		      int		ySrc,
		      const IcTriangle	*tris,
		      int		ntris)
{
    PixRegionBox	bounds;
    IcImage		*image = NULL;
    int		xDst, yDst;
    int		xRel, yRel;
    IcFormat	*format;
    
    xDst = tris[0].p1.x >> 16;
    yDst = tris[0].p1.y >> 16;

    format = _IcFormatCreate (PICT_a8);
    
    if (format)
    {
	IcTriangleBounds (ntris, tris, &bounds);
	if (bounds.x2 <= bounds.x1 || bounds.y2 <= bounds.y1)
	    return;
	image = IcCreateAlphaPicture (dst,
				      format,
				      bounds.x2 - bounds.x1,
				      bounds.y2 - bounds.y1);
	if (!image)
	    return;
    }
    for (; ntris; ntris--, tris++)
    {
	if (!format)
	{
	    IcTriangleBounds (1, tris, &bounds);
	    if (bounds.x2 <= bounds.x1 || bounds.y2 <= bounds.y1)
		continue;
	    image = IcCreateAlphaPicture (dst,
					  format,
					  bounds.x2 - bounds.x1,
					  bounds.y2 - bounds.y1);
	    if (!image)
		break;
	}
	IcRasterizeTriangle (image, tris, -bounds.x1, -bounds.y1);
	if (!format)
	{
	    xRel = bounds.x1 + xSrc - xDst;
	    yRel = bounds.y1 + ySrc - yDst;
	    IcComposite (op, src, image, dst,
			 xRel, yRel, 0, 0, bounds.x1, bounds.y1,
			 bounds.x2 - bounds.x1, bounds.y2 - bounds.y1);
	    IcImageDestroy (image);
	}
	/* XXX adjust xSrc and ySrc */
    }
    if (format)
    {
	xRel = bounds.x1 + xSrc - xDst;
	yRel = bounds.y1 + ySrc - yDst;
	IcComposite (op, src, image, dst,
		     xRel, yRel, 0, 0, bounds.x1, bounds.y1,
		     bounds.x2 - bounds.x1, bounds.y2 - bounds.y1);
	IcImageDestroy (image);
    }

    _IcFormatDestroy (format);
}

void
IcCompositeTriStrip (char		op,
		     IcImage		*src,
		     IcImage		*dst,
		     int		xSrc,
		     int		ySrc,
		     const IcPointFixed	*points,
		     int		npoints)
{
    IcTriangle		tri;
    PixRegionBox	bounds;
    IcImage		*image = NULL;
    int		xDst, yDst;
    int		xRel, yRel;
    IcFormat	*format;
    
    xDst = points[0].x >> 16;
    yDst = points[0].y >> 16;

    format = _IcFormatCreate (PICT_a8);
    
    if (npoints < 3)
	return;
    if (format)
    {
	IcPointFixedBounds (npoints, points, &bounds);
	if (bounds.x2 <= bounds.x1 || bounds.y2 <= bounds.y1)
	    return;
	image = IcCreateAlphaPicture (dst,
				      format,
				      bounds.x2 - bounds.x1,
				      bounds.y2 - bounds.y1);
	if (!image)
	    return;
    }
    for (; npoints >= 3; npoints--, points++)
    {
	tri.p1 = points[0];
	tri.p2 = points[1];
	tri.p3 = points[2];
	if (!format)
	{
	    IcTriangleBounds (1, &tri, &bounds);
	    if (bounds.x2 <= bounds.x1 || bounds.y2 <= bounds.y1)
		continue;
	    image = IcCreateAlphaPicture (dst,
					  format, 
					  bounds.x2 - bounds.x1,
					  bounds.y2 - bounds.y1);
	    if (!image)
		continue;
	}
	IcRasterizeTriangle (image, &tri, -bounds.x1, -bounds.y1);
	if (!format)
	{
	    xRel = bounds.x1 + xSrc - xDst;
	    yRel = bounds.y1 + ySrc - yDst;
	    IcComposite (op, src, image, dst,
			 xRel, yRel, 0, 0, bounds.x1, bounds.y1,
			 bounds.x2 - bounds.x1, bounds.y2 - bounds.y1);
	    IcImageDestroy (image);
	}
    }
    if (format)
    {
	xRel = bounds.x1 + xSrc - xDst;
	yRel = bounds.y1 + ySrc - yDst;
	IcComposite (op, src, image, dst,
		     xRel, yRel, 0, 0, bounds.x1, bounds.y1,
		     bounds.x2 - bounds.x1, bounds.y2 - bounds.y1);
	IcImageDestroy (image);
    }

    _IcFormatDestroy (format);
}

void
IcCompositeTriFan (char			op,
		   IcImage		*src,
		   IcImage		*dst,
		   int			xSrc,
		   int			ySrc,
		   const IcPointFixed	*points,
		   int			npoints)
{
    IcTriangle		tri;
    PixRegionBox	bounds;
    IcImage		*image = NULL;
    const IcPointFixed	*first;
    int		xDst, yDst;
    int		xRel, yRel;
    IcFormat	*format;
    
    xDst = points[0].x >> 16;
    yDst = points[0].y >> 16;

    format = _IcFormatCreate (PICT_a8);
    
    if (npoints < 3)
	return;
    if (format)
    {
	IcPointFixedBounds (npoints, points, &bounds);
	if (bounds.x2 <= bounds.x1 || bounds.y2 <= bounds.y1)
	    return;
	image = IcCreateAlphaPicture (dst,
				      format,
				      bounds.x2 - bounds.x1,
				      bounds.y2 - bounds.y1);
	if (!image)
	    return;
    }
    first = points++;
    npoints--;
    for (; npoints >= 2; npoints--, points++)
    {
	tri.p1 = *first;
	tri.p2 = points[0];
	tri.p3 = points[1];
	if (!format)
	{
	    IcTriangleBounds (1, &tri, &bounds);
	    if (bounds.x2 <= bounds.x1 || bounds.y2 <= bounds.y1)
		continue;
	    image = IcCreateAlphaPicture (dst,
					  format, 
					  bounds.x2 - bounds.x1,
					  bounds.y2 - bounds.y1);
	    if (!image)
		continue;
	}
	IcRasterizeTriangle (image, &tri, -bounds.x1, -bounds.y1);
	if (!format)
	{
	    xRel = bounds.x1 + xSrc - xDst;
	    yRel = bounds.y1 + ySrc - yDst;
	    IcComposite (op, src, image, dst,
			 xRel, yRel, 0, 0, bounds.x1, bounds.y1,
			 bounds.x2 - bounds.x1, bounds.y2 - bounds.y1);
	    IcImageDestroy (image);
	}
    }
    if (format)
    {
	xRel = bounds.x1 + xSrc - xDst;
	yRel = bounds.y1 + ySrc - yDst;
	IcComposite (op, src, image, dst,
		     xRel, yRel, 0, 0, bounds.x1, bounds.y1,
		     bounds.x2 - bounds.x1, bounds.y2 - bounds.y1);
	IcImageDestroy (image);
    }

    _IcFormatDestroy (format);
}

