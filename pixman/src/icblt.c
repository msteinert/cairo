/*
 * Id: $
 *
 * Copyright © 1998 Keith Packard
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
/* $XFree86: $ */

#include "icint.h"

#define InitializeShifts(sx,dx,ls,rs) { \
    if (sx != dx) { \
	if (sx > dx) { \
	    ls = sx - dx; \
	    rs = IC_UNIT - ls; \
	} else { \
	    rs = dx - sx; \
	    ls = IC_UNIT - rs; \
	} \
    } \
}

void
IcBlt (IcBits   *srcLine,
       IcStride	srcStride,
       int	srcX,
       
       IcBits   *dstLine,
       IcStride dstStride,
       int	dstX,
       
       int	width, 
       int	height,
       
       int	alu,
       IcBits	pm,
       int	bpp,
       
       Bool	reverse,
       Bool	upsidedown)
{
    IcBits  *src, *dst;
    int	    leftShift, rightShift;
    IcBits  startmask, endmask;
    IcBits  bits, bits1;
    int	    n, nmiddle;
    Bool    destInvarient;
    int	    startbyte, endbyte;
    IcDeclareMergeRop ();

#ifdef IC_24BIT
    if (bpp == 24 && !IcCheck24Pix (pm))
    {
	IcBlt24 (srcLine, srcStride, srcX, dstLine, dstStride, dstX,
		 width, height, alu, pm, reverse, upsidedown);
	return;
    }
#endif
    IcInitializeMergeRop(alu, pm);
    destInvarient = IcDestInvarientMergeRop();
    if (upsidedown)
    {
	srcLine += (height - 1) * (srcStride);
	dstLine += (height - 1) * (dstStride);
	srcStride = -srcStride;
	dstStride = -dstStride;
    }
    IcMaskBitsBytes (dstX, width, destInvarient, startmask, startbyte,
		     nmiddle, endmask, endbyte);
    if (reverse)
    {
	srcLine += ((srcX + width - 1) >> IC_SHIFT) + 1;
	dstLine += ((dstX + width - 1) >> IC_SHIFT) + 1;
	srcX = (srcX + width - 1) & IC_MASK;
	dstX = (dstX + width - 1) & IC_MASK;
    }
    else
    {
	srcLine += srcX >> IC_SHIFT;
	dstLine += dstX >> IC_SHIFT;
	srcX &= IC_MASK;
	dstX &= IC_MASK;
    }
    if (srcX == dstX)
    {
	while (height--)
	{
	    src = srcLine;
	    srcLine += srcStride;
	    dst = dstLine;
	    dstLine += dstStride;
	    if (reverse)
	    {
		if (endmask)
		{
		    bits = *--src;
		    --dst;
		    IcDoRightMaskByteMergeRop(dst, bits, endbyte, endmask);
		}
		n = nmiddle;
		if (destInvarient)
		{
		    while (n--)
			*--dst = IcDoDestInvarientMergeRop(*--src);
		}
		else
		{
		    while (n--)
		    {
			bits = *--src;
			--dst;
			*dst = IcDoMergeRop (bits, *dst);
		    }
		}
		if (startmask)
		{
		    bits = *--src;
		    --dst;
		    IcDoLeftMaskByteMergeRop(dst, bits, startbyte, startmask);
		}
	    }
	    else
	    {
		if (startmask)
		{
		    bits = *src++;
		    IcDoLeftMaskByteMergeRop(dst, bits, startbyte, startmask);
		    dst++;
		}
		n = nmiddle;
		if (destInvarient)
		{
#if 0
		    /*
		     * This provides some speedup on screen->screen blts
		     * over the PCI bus, usually about 10%.  But Ic
		     * isn't usually used for this operation...
		     */
		    if (_ca2 + 1 == 0 && _cx2 == 0)
		    {
			IcBits	t1, t2, t3, t4;
			while (n >= 4)
			{
			    t1 = *src++;
			    t2 = *src++;
			    t3 = *src++;
			    t4 = *src++;
			    *dst++ = t1;
			    *dst++ = t2;
			    *dst++ = t3;
			    *dst++ = t4;
			    n -= 4;
			}
		    }
#endif
		    while (n--)
			*dst++ = IcDoDestInvarientMergeRop(*src++);
		}
		else
		{
		    while (n--)
		    {
			bits = *src++;
			*dst = IcDoMergeRop (bits, *dst);
			dst++;
		    }
		}
		if (endmask)
		{
		    bits = *src;
		    IcDoRightMaskByteMergeRop(dst, bits, endbyte, endmask);
		}
	    }
	}
    }
    else
    {
	if (srcX > dstX)
	{
	    leftShift = srcX - dstX;
	    rightShift = IC_UNIT - leftShift;
	}
	else
	{
	    rightShift = dstX - srcX;
	    leftShift = IC_UNIT - rightShift;
	}
	while (height--)
	{
	    src = srcLine;
	    srcLine += srcStride;
	    dst = dstLine;
	    dstLine += dstStride;
	    
	    bits1 = 0;
	    if (reverse)
	    {
		if (srcX < dstX)
		    bits1 = *--src;
		if (endmask)
		{
		    bits = IcScrRight(bits1, rightShift); 
		    if (IcScrRight(endmask, leftShift))
		    {
			bits1 = *--src;
			bits |= IcScrLeft(bits1, leftShift);
		    }
		    --dst;
		    IcDoRightMaskByteMergeRop(dst, bits, endbyte, endmask);
		}
		n = nmiddle;
		if (destInvarient)
		{
		    while (n--)
		    {
			bits = IcScrRight(bits1, rightShift); 
			bits1 = *--src;
			bits |= IcScrLeft(bits1, leftShift);
			--dst;
			*dst = IcDoDestInvarientMergeRop(bits);
		    }
		}
		else
		{
		    while (n--)
		    {
			bits = IcScrRight(bits1, rightShift); 
			bits1 = *--src;
			bits |= IcScrLeft(bits1, leftShift);
			--dst;
			*dst = IcDoMergeRop(bits, *dst);
		    }
		}
		if (startmask)
		{
		    bits = IcScrRight(bits1, rightShift); 
		    if (IcScrRight(startmask, leftShift))
		    {
			bits1 = *--src;
			bits |= IcScrLeft(bits1, leftShift);
		    }
		    --dst;
		    IcDoLeftMaskByteMergeRop (dst, bits, startbyte, startmask);
		}
	    }
	    else
	    {
		if (srcX > dstX)
		    bits1 = *src++;
		if (startmask)
		{
		    bits = IcScrLeft(bits1, leftShift); 
		    bits1 = *src++;
		    bits |= IcScrRight(bits1, rightShift);
		    IcDoLeftMaskByteMergeRop (dst, bits, startbyte, startmask);
		    dst++;
		}
		n = nmiddle;
		if (destInvarient)
		{
		    while (n--)
		    {
			bits = IcScrLeft(bits1, leftShift); 
			bits1 = *src++;
			bits |= IcScrRight(bits1, rightShift);
			*dst = IcDoDestInvarientMergeRop(bits);
			dst++;
		    }
		}
		else
		{
		    while (n--)
		    {
			bits = IcScrLeft(bits1, leftShift); 
			bits1 = *src++;
			bits |= IcScrRight(bits1, rightShift);
			*dst = IcDoMergeRop(bits, *dst);
			dst++;
		    }
		}
		if (endmask)
		{
		    bits = IcScrLeft(bits1, leftShift); 
		    if (IcScrLeft(endmask, rightShift))
		    {
			bits1 = *src;
			bits |= IcScrRight(bits1, rightShift);
		    }
		    IcDoRightMaskByteMergeRop (dst, bits, endbyte, endmask);
		}
	    }
	}
    }
}

#ifdef IC_24BIT

#undef DEBUG_BLT24
#ifdef DEBUG_BLT24

static unsigned long
getPixel (char *src, int x)
{
    unsigned long   l;

    l = 0;
    memcpy (&l, src + x * 3, 3);
    return l;
}
#endif

static void
IcBlt24Line (IcBits	    *src,
	     int	    srcX,

	     IcBits	    *dst,
	     int	    dstX,

	     int	    width,

	     int	    alu,
	     IcBits	    pm,
	 
	     Bool	    reverse)
{
#ifdef DEBUG_BLT24
    char    *origDst = (char *) dst;
    IcBits  *origLine = dst + ((dstX >> IC_SHIFT) - 1);
    int	    origNlw = ((width + IC_MASK) >> IC_SHIFT) + 3;
    int	    origX = dstX / 24;
#endif
    
    int	    leftShift, rightShift;
    IcBits  startmask, endmask;
    int	    n;
    
    IcBits  bits, bits1;
    IcBits  mask;

    int	    rot;
    IcDeclareMergeRop ();
    
    IcInitializeMergeRop (alu, IC_ALLONES);
    IcMaskBits(dstX, width, startmask, n, endmask);
#ifdef DEBUG_BLT24
    ErrorF ("dstX %d width %d reverse %d\n", dstX, width, reverse);
#endif
    if (reverse)
    {
	src += ((srcX + width - 1) >> IC_SHIFT) + 1;
	dst += ((dstX + width - 1) >> IC_SHIFT) + 1;
	rot = IcFirst24Rot (((dstX + width - 8) & IC_MASK));
	rot = IcPrev24Rot(rot);
#ifdef DEBUG_BLT24
	ErrorF ("dstX + width - 8: %d rot: %d\n", (dstX + width - 8) & IC_MASK, rot);
#endif
	srcX = (srcX + width - 1) & IC_MASK;
	dstX = (dstX + width - 1) & IC_MASK;
    }
    else
    {
	src += srcX >> IC_SHIFT;
	dst += dstX >> IC_SHIFT;
	srcX &= IC_MASK;
	dstX &= IC_MASK;
	rot = IcFirst24Rot (dstX);
#ifdef DEBUG_BLT24
	ErrorF ("dstX: %d rot: %d\n", dstX, rot);
#endif
    }
    mask = IcRot24(pm,rot);
#ifdef DEBUG_BLT24
    ErrorF ("pm 0x%x mask 0x%x\n", pm, mask);
#endif
    if (srcX == dstX)
    {
	if (reverse)
	{
	    if (endmask)
	    {
		bits = *--src;
		--dst;
		*dst = IcDoMaskMergeRop (bits, *dst, mask & endmask);
		mask = IcPrev24Pix (mask);
	    }
	    while (n--)
	    {
		bits = *--src;
		--dst;
		*dst = IcDoMaskMergeRop (bits, *dst, mask);
		mask = IcPrev24Pix (mask);
	    }
	    if (startmask)
	    {
		bits = *--src;
		--dst;
		*dst = IcDoMaskMergeRop(bits, *dst, mask & startmask);
	    }
	}
	else
	{
	    if (startmask)
	    {
		bits = *src++;
		*dst = IcDoMaskMergeRop (bits, *dst, mask & startmask);
		dst++;
		mask = IcNext24Pix(mask);
	    }
	    while (n--)
	    {
		bits = *src++;
		*dst = IcDoMaskMergeRop (bits, *dst, mask);
		dst++;
		mask = IcNext24Pix(mask);
	    }
	    if (endmask)
	    {
		bits = *src;
		*dst = IcDoMaskMergeRop(bits, *dst, mask & endmask);
	    }
	}
    }
    else
    {
	if (srcX > dstX)
	{
	    leftShift = srcX - dstX;
	    rightShift = IC_UNIT - leftShift;
	}
	else
	{
	    rightShift = dstX - srcX;
	    leftShift = IC_UNIT - rightShift;
	}
	
	bits1 = 0;
	if (reverse)
	{
	    if (srcX < dstX)
		bits1 = *--src;
	    if (endmask)
	    {
		bits = IcScrRight(bits1, rightShift); 
		if (IcScrRight(endmask, leftShift))
		{
		    bits1 = *--src;
		    bits |= IcScrLeft(bits1, leftShift);
		}
		--dst;
		*dst = IcDoMaskMergeRop (bits, *dst, mask & endmask);
		mask = IcPrev24Pix(mask);
	    }
	    while (n--)
	    {
		bits = IcScrRight(bits1, rightShift); 
		bits1 = *--src;
		bits |= IcScrLeft(bits1, leftShift);
		--dst;
		*dst = IcDoMaskMergeRop(bits, *dst, mask);
		mask = IcPrev24Pix(mask);
	    }
	    if (startmask)
	    {
		bits = IcScrRight(bits1, rightShift); 
		if (IcScrRight(startmask, leftShift))
		{
		    bits1 = *--src;
		    bits |= IcScrLeft(bits1, leftShift);
		}
		--dst;
		*dst = IcDoMaskMergeRop (bits, *dst, mask & startmask);
	    }
	}
	else
	{
	    if (srcX > dstX)
		bits1 = *src++;
	    if (startmask)
	    {
		bits = IcScrLeft(bits1, leftShift); 
		bits1 = *src++;
		bits |= IcScrRight(bits1, rightShift);
		*dst = IcDoMaskMergeRop (bits, *dst, mask & startmask);
		dst++;
		mask = IcNext24Pix(mask);
	    }
	    while (n--)
	    {
		bits = IcScrLeft(bits1, leftShift); 
		bits1 = *src++;
		bits |= IcScrRight(bits1, rightShift);
		*dst = IcDoMaskMergeRop(bits, *dst, mask);
		dst++;
		mask = IcNext24Pix(mask);
	    }
	    if (endmask)
	    {
		bits = IcScrLeft(bits1, leftShift); 
		if (IcScrLeft(endmask, rightShift))
		{
		    bits1 = *src;
		    bits |= IcScrRight(bits1, rightShift);
		}
		*dst = IcDoMaskMergeRop (bits, *dst, mask & endmask);
	    }
	}
    }
#ifdef DEBUG_BLT24
    {
	int firstx, lastx, x;

	firstx = origX;
	if (firstx)
	    firstx--;
	lastx = origX + width/24 + 1;
	for (x = firstx; x <= lastx; x++)
	    ErrorF ("%06x ", getPixel (origDst, x));
	ErrorF ("\n");
	while (origNlw--)
	    ErrorF ("%08x ", *origLine++);
	ErrorF ("\n");
    }
#endif
}

void
IcBlt24 (IcBits	    *srcLine,
	 IcStride   srcStride,
	 int	    srcX,

	 IcBits	    *dstLine,
	 IcStride   dstStride,
	 int	    dstX,

	 int	    width, 
	 int	    height,

	 int	    alu,
	 IcBits	    pm,

	 Bool	    reverse,
	 Bool	    upsidedown)
{
    if (upsidedown)
    {
	srcLine += (height-1) * srcStride;
	dstLine += (height-1) * dstStride;
	srcStride = -srcStride;
	dstStride = -dstStride;
    }
    while (height--)
    {
	IcBlt24Line (srcLine, srcX, dstLine, dstX, width, alu, pm, reverse);
	srcLine += srcStride;
	dstLine += dstStride;
    }
#ifdef DEBUG_BLT24
    ErrorF ("\n");
#endif
}
#endif /* IC_24BIT */

#if IC_SHIFT == IC_STIP_SHIFT + 1

/*
 * Could be generalized to IC_SHIFT > IC_STIP_SHIFT + 1 by
 * creating an ring of values stepped through for each line
 */

void
IcBltOdd (IcBits    *srcLine,
	  IcStride  srcStrideEven,
	  IcStride  srcStrideOdd,
	  int	    srcXEven,
	  int	    srcXOdd,

	  IcBits    *dstLine,
	  IcStride  dstStrideEven,
	  IcStride  dstStrideOdd,
	  int	    dstXEven,
	  int	    dstXOdd,

	  int	    width,
	  int	    height,

	  int	    alu,
	  IcBits    pm,
	  int	    bpp)
{
    IcBits  *src;
    int	    leftShiftEven, rightShiftEven;
    IcBits  startmaskEven, endmaskEven;
    int	    nmiddleEven;
    
    IcBits  *dst;
    int	    leftShiftOdd, rightShiftOdd;
    IcBits  startmaskOdd, endmaskOdd;
    int	    nmiddleOdd;

    int	    leftShift, rightShift;
    IcBits  startmask, endmask;
    int	    nmiddle;
    
    int	    srcX, dstX;
    
    IcBits  bits, bits1;
    int	    n;
    
    Bool    destInvarient;
    Bool    even;
    IcDeclareMergeRop ();

    IcInitializeMergeRop (alu, pm);
    destInvarient = IcDestInvarientMergeRop();

    srcLine += srcXEven >> IC_SHIFT;
    dstLine += dstXEven >> IC_SHIFT;
    srcXEven &= IC_MASK;
    dstXEven &= IC_MASK;
    srcXOdd &= IC_MASK;
    dstXOdd &= IC_MASK;

    IcMaskBits(dstXEven, width, startmaskEven, nmiddleEven, endmaskEven);
    IcMaskBits(dstXOdd, width, startmaskOdd, nmiddleOdd, endmaskOdd);
    
    even = TRUE;
    InitializeShifts(srcXEven, dstXEven, leftShiftEven, rightShiftEven);
    InitializeShifts(srcXOdd, dstXOdd, leftShiftOdd, rightShiftOdd);
    while (height--)
    {
	src = srcLine;
	dst = dstLine;
	if (even)
	{
	    srcX = srcXEven;
	    dstX = dstXEven;
	    startmask = startmaskEven;
	    endmask = endmaskEven;
	    nmiddle = nmiddleEven;
	    leftShift = leftShiftEven;
	    rightShift = rightShiftEven;
	    srcLine += srcStrideEven;
	    dstLine += dstStrideEven;
	    even = FALSE;
	}
	else
	{
	    srcX = srcXOdd;
	    dstX = dstXOdd;
	    startmask = startmaskOdd;
	    endmask = endmaskOdd;
	    nmiddle = nmiddleOdd;
	    leftShift = leftShiftOdd;
	    rightShift = rightShiftOdd;
	    srcLine += srcStrideOdd;
	    dstLine += dstStrideOdd;
	    even = TRUE;
	}
	if (srcX == dstX)
	{
	    if (startmask)
	    {
		bits = *src++;
		*dst = IcDoMaskMergeRop (bits, *dst, startmask);
		dst++;
	    }
	    n = nmiddle;
	    if (destInvarient)
	    {
		while (n--)
		{
		    bits = *src++;
		    *dst = IcDoDestInvarientMergeRop(bits);
		    dst++;
		}
	    }
	    else
	    {
		while (n--)
		{
		    bits = *src++;
		    *dst = IcDoMergeRop (bits, *dst);
		    dst++;
		}
	    }
	    if (endmask)
	    {
		bits = *src;
		*dst = IcDoMaskMergeRop(bits, *dst, endmask);
	    }
	}
	else
	{
	    bits = 0;
	    if (srcX > dstX)
		bits = *src++;
	    if (startmask)
	    {
		bits1 = IcScrLeft(bits, leftShift);
		bits = *src++;
		bits1 |= IcScrRight(bits, rightShift);
		*dst = IcDoMaskMergeRop (bits1, *dst, startmask);
		dst++;
	    }
	    n = nmiddle;
	    if (destInvarient)
	    {
		while (n--)
		{
		    bits1 = IcScrLeft(bits, leftShift);
		    bits = *src++;
		    bits1 |= IcScrRight(bits, rightShift);
		    *dst = IcDoDestInvarientMergeRop(bits1);
		    dst++;
		}
	    }
	    else
	    {
		while (n--)
		{
		    bits1 = IcScrLeft(bits, leftShift);
		    bits = *src++;
		    bits1 |= IcScrRight(bits, rightShift);
		    *dst = IcDoMergeRop(bits1, *dst);
		    dst++;
		}
	    }
	    if (endmask)
	    {
		bits1 = IcScrLeft(bits, leftShift);
		if (IcScrLeft(endmask, rightShift))
		{
		    bits = *src;
		    bits1 |= IcScrRight(bits, rightShift);
		}
		*dst = IcDoMaskMergeRop (bits1, *dst, endmask);
	    }
	}
    }
}

#ifdef IC_24BIT
void
IcBltOdd24 (IcBits	*srcLine,
	    IcStride	srcStrideEven,
	    IcStride	srcStrideOdd,
	    int		srcXEven,
	    int		srcXOdd,

	    IcBits	*dstLine,
	    IcStride	dstStrideEven,
	    IcStride	dstStrideOdd,
	    int		dstXEven,
	    int		dstXOdd,

	    int		width,
	    int		height,

	    int		alu,
	    IcBits	pm)
{
    Bool    even = TRUE;
    
    while (height--)
    {
	if (even)
	{
	    IcBlt24Line (srcLine, srcXEven, dstLine, dstXEven,
			 width, alu, pm, FALSE);
	    srcLine += srcStrideEven;
	    dstLine += dstStrideEven;
	    even = FALSE;
	}
	else
	{
	    IcBlt24Line (srcLine, srcXOdd, dstLine, dstXOdd,
			 width, alu, pm, FALSE);
	    srcLine += srcStrideOdd;
	    dstLine += dstStrideOdd;
	    even = TRUE;
	}
    }
#if 0
    fprintf (stderr, "\n");
#endif
}
#endif

#endif

#if IC_STIP_SHIFT != IC_SHIFT
void
IcSetBltOdd (IcStip	*stip,
	     IcStride	stipStride,
	     int	srcX,
	     IcBits	**bits,
	     IcStride	*strideEven,
	     IcStride	*strideOdd,
	     int	*srcXEven,
	     int	*srcXOdd)
{
    int	    srcAdjust;
    int	    strideAdjust;

    /*
     * bytes needed to align source
     */
    srcAdjust = (((int) stip) & (IC_MASK >> 3));
    /*
     * IcStip units needed to align stride
     */
    strideAdjust = stipStride & (IC_MASK >> IC_STIP_SHIFT);

    *bits = (IcBits *) ((char *) stip - srcAdjust);
    if (srcAdjust)
    {
	*strideEven = IcStipStrideToBitsStride (stipStride + 1);
	*strideOdd = IcStipStrideToBitsStride (stipStride);

	*srcXEven = srcX + (srcAdjust << 3);
	*srcXOdd = srcX + (srcAdjust << 3) - (strideAdjust << IC_STIP_SHIFT);
    }
    else
    {
	*strideEven = IcStipStrideToBitsStride (stipStride);
	*strideOdd = IcStipStrideToBitsStride (stipStride + 1);
	
	*srcXEven = srcX;
	*srcXOdd = srcX + (strideAdjust << IC_STIP_SHIFT);
    }
}
#endif

void
IcBltStip (IcStip   *src,
	   IcStride srcStride,	    /* in IcStip units, not IcBits units */
	   int	    srcX,
	   
	   IcStip   *dst,
	   IcStride dstStride,	    /* in IcStip units, not IcBits units */
	   int	    dstX,

	   int	    width, 
	   int	    height,

	   int	    alu,
	   IcBits   pm,
	   int	    bpp)
{
#if IC_STIP_SHIFT != IC_SHIFT
    if (IC_STIP_ODDSTRIDE(srcStride) || IC_STIP_ODDPTR(src) ||
	IC_STIP_ODDSTRIDE(dstStride) || IC_STIP_ODDPTR(dst))
    {
	IcStride    srcStrideEven, srcStrideOdd;
	IcStride    dstStrideEven, dstStrideOdd;
	int	    srcXEven, srcXOdd;
	int	    dstXEven, dstXOdd;
	IcBits	    *s, *d;
	int	    sx, dx;
	
	src += srcX >> IC_STIP_SHIFT;
	srcX &= IC_STIP_MASK;
	dst += dstX >> IC_STIP_SHIFT;
	dstX &= IC_STIP_MASK;
	
	IcSetBltOdd (src, srcStride, srcX,
		     &s,
		     &srcStrideEven, &srcStrideOdd,
		     &srcXEven, &srcXOdd);
		     
	IcSetBltOdd (dst, dstStride, dstX,
		     &d,
		     &dstStrideEven, &dstStrideOdd,
		     &dstXEven, &dstXOdd);
		     
#ifdef IC_24BIT
	if (bpp == 24 && !IcCheck24Pix (pm))
	{
	    IcBltOdd24  (s, srcStrideEven, srcStrideOdd,
			 srcXEven, srcXOdd,

			 d, dstStrideEven, dstStrideOdd,
			 dstXEven, dstXOdd,

			 width, height, alu, pm);
	}
	else
#endif
	{
	    IcBltOdd (s, srcStrideEven, srcStrideOdd,
		      srcXEven, srcXOdd,
    
		      d, dstStrideEven, dstStrideOdd,
		      dstXEven, dstXOdd,
    
		      width, height, alu, pm, bpp);
	}
    }
    else
#endif
    {
	IcBlt ((IcBits *) src, IcStipStrideToBitsStride (srcStride), 
	       srcX, 
	       (IcBits *) dst, IcStipStrideToBitsStride (dstStride), 
	       dstX, 
	       width, height,
	       alu, pm, bpp, FALSE, FALSE);
    }
}
