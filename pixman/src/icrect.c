/*
 * $XFree86: $
 *
 * Copyright © 2000 Keith Packard, member of The XFree86 Project, Inc.
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

static void
IcColorRects (IcImage	 *dst,
	      IcImage	 *clipPict,
	      IcColor	 *color,
	      int	 nRect,
	      xRectangle *rects,
	      int	 xoff,
	      int	 yoff)
{
    CARD32		pixel;
    CARD32		tmpval[4];
    Region		*clip;
    unsigned long	mask;

    IcRenderColorToPixel (dst->image_format, color, &pixel);

    if (clipPict->clientClipType == CT_REGION)
    {
	tmpval[2] = dst->clipOrigin.x - xoff;
	tmpval[3] = dst->clipOrigin.y - yoff;
	mask |= CPClipXOrigin|CPClipYOrigin;
	
	clip = REGION_CREATE (pScreen, NULL, 1);
	REGION_COPY (pScreen, pClip,
		     (RegionPtr) pClipPict->clientClip);
	(*pGC->funcs->ChangeClip) (pGC, CT_REGION, pClip, 0);
    }

    if (xoff || yoff)
    {
	int	i;
	for (i = 0; i < nRect; i++)
	{
	    rects[i].x -= xoff;
	    rects[i].y -= yoff;
	}
    }
    (*pGC->ops->PolyFillRect) (pDst->pDrawable, pGC, nRect, rects);
    if (xoff || yoff)
    {
	int	i;
	for (i = 0; i < nRect; i++)
	{
	    rects[i].x += xoff;
	    rects[i].y += yoff;
	}
    }
}

void
IcCompositeRects (CARD8		op,
		  IcImage	*dst,
		  IcColor	*color,
		  int		nRect,
		  xRectangle    *rects)
{
    if (color->alpha == 0xffff)
    {
	if (op == PictOpOver)
	    op = PictOpSrc;
    }
    if (op == PictOpClear)
	color->red = color->green = color->blue = color->alpha = 0;
    
    if (op == PictOpSrc || op == PictOpClear)
    {
	IcColorRects (dst, dst, color, nRect, rects, 0, 0);
	if (dst->alphaMap)
	    IcColorRects (dst->alphaMap, dst,
			  color, nRect, rects,
			  dst->alphaOrigin.x,
			  dst->alphaOrigin.y);
    }
    else
    {
	IcFormat	*rgbaFormat;
	IcPixels	*pixels;
	IcImage		*src;
	xRectangle	one;
	int		error;
	Pixel		pixel;
	CARD32		tmpval[2];

	rgbaFormat = IcFormatCreate (PICT_a8r8g8b8);
	if (!rgbaFormat)
	    goto bail1;
	
	pixels = IcPixelsCreate (1, 1, rgbaFormat->depth);
	if (!pixels)
	    goto bail2;
	
	IcRenderColorToPixel (rgbaFormat, color, &pixel);

	/* XXX: how to do this?
	one.x = 0;
	one.y = 0;
	one.width = 1;
	one.height = 1;
	(*pGC->ops->PolyFillRect) (&pPixmap->drawable, pGC, 1, &one);
	*/
	
	tmpval[0] = xTrue;
	src = IcImageCreateForPixels (pixels, rgbaFormat,
				      CPRepeat, tmpval, &error, &error_value);
	if (!src)
	    goto bail4;

	while (nRect--)
	{
	    CompositePicture (op, pSrc, 0, pDst, 0, 0, 0, 0, 
			      rects->x,
			      rects->y,
			      rects->width,
			      rects->height);
	    rects++;
	}

	IcImageDestroy (src);
bail4:
bail3:
	IcPixelsDestroy (pixels);
bail2:
bail1:
	;
    }
}
