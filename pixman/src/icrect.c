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

/* XXX: I haven't ported this yet
static void
IcColorRects (IcImage	 *dst,
	      IcImage	 *clipPict,
	      IcColor	 *color,
	      int	 nRect,
	      IcRectangle *rects,
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
	
	clip = PixRegionCreate ();
	PixRegionCopy (clip, pClipPict->clientClip);
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
*/

void IcFillRectangle (char		op,
		      IcImage		*dst,
		      const IcColor	*color,
		      int		x,
		      int		y,
		      unsigned int	width,
		      unsigned int	height)
{
    IcRectangle rect;

    rect.x = x;
    rect.y = y;
    rect.width = width;
    rect.height = height;

    IcFillRectangles (op, dst, color, &rect, 1);
}

void
IcFillRectangles (char			op,
		  IcImage		*dst,
		  const IcColor		*color,
		  const IcRectangle	*rects,
		  int			nRects)
{
    IcColor color_s = *color;

    if (color_s.alpha == 0xffff)
    {
	if (op == PictOpOver)
	    op = PictOpSrc;
    }
    if (op == PictOpClear)
	color_s.red = color_s.green = color_s.blue = color_s.alpha = 0;

/* XXX: Really need this to optimize solid rectangles
    if (op == PictOpSrc || op == PictOpClear)
    {
	IcColorRects (dst, dst, &color_s, nRects, rects, 0, 0);
	if (dst->alphaMap)
	    IcColorRects (dst->alphaMap, dst,
			  &color_s, nRects, rects,
			  dst->alphaOrigin.x,
			  dst->alphaOrigin.y);
    }
    else
*/
    {
	IcFormat	rgbaFormat;
	IcPixels	*pixels;
	IcImage		*src;
	IcBits		pixel;

	IcFormatInit (&rgbaFormat, PICT_a8r8g8b8);
	
	pixels = IcPixelsCreate (1, 1, rgbaFormat.depth);
	if (!pixels)
	    goto bail1;
	
	IcColorToPixel (&rgbaFormat, &color_s, &pixel);

	/* XXX: Originally, fb had the following:

	   (*pGC->ops->PolyFillRect) (&pPixmap->drawable, pGC, 1, &one);

	   I haven't checked to see what I might be breaking with a
	   trivial assignment instead.
	*/
	pixels->data[0] = pixel;

	src = IcImageCreateForPixels (pixels, &rgbaFormat);
	if (!src)
	    goto bail2;

	IcImageSetRepeat (src, 1);

	while (nRects--)
	{
	    IcComposite (op, src, 0, dst, 0, 0, 0, 0, 
			 rects->x,
			 rects->y,
			 rects->width,
			 rects->height);
	    rects++;
	}

	IcImageDestroy (src);
bail2:
	IcPixelsDestroy (pixels);
bail1:
	;
    }
}
slim_hidden_def(IcFillRectangles)
