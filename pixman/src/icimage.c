/*
 * $XFree86: $
 *
 * Copyright © 2000 SuSE, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of SuSE not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  SuSE makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * SuSE DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL SuSE
 * BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN 
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author:  Keith Packard, SuSE, Inc.
 */

/* XXX: Do we really need all of these includes? */

#include "icint.h"
#include "icimage.h"

#include "pixmapstr.h"

/*
#include "mipict.h"
#include "mi.h"
#include "picturestr.h"
#include "scrnintstr.h"
*/

#ifndef __GNUC__
#define __inline
#endif


#define cvt8888to0565(s)    ((((s) >> 3) & 0x001f) | \
			     (((s) >> 5) & 0x07e0) | \
			     (((s) >> 8) & 0xf800))
#define cvt0565to8888(s)    (((((s) << 3) & 0xf8) | (((s) >> 2) & 0x7)) | \
			     ((((s) << 5) & 0xfc00) | (((s) >> 1) & 0x300)) | \
			     ((((s) << 8) & 0xf80000) | (((s) << 3) & 0x70000)))

#if IMAGE_BYTE_ORDER == MSBFirst
#define Fetch24(a)  ((unsigned long) (a) & 1 ? \
		     ((*(a) << 16) | *((CARD16 *) ((a)+1))) : \
		     ((*((CARD16 *) (a)) << 8) | *((a)+2)))
#define Store24(a,v) ((unsigned long) (a) & 1 ? \
		      ((*(a) = (CARD8) ((v) >> 16)), \
		       (*((CARD16 *) ((a)+1)) = (CARD16) (v))) : \
		      ((*((CARD16 *) (a)) = (CARD16) ((v) >> 8)), \
		       (*((a)+2) = (CARD8) (v))))
#else
#define Fetch24(a)  ((unsigned long) (a) & 1 ? \
		     ((*(a)) | (*((CARD16 *) ((a)+1)) << 8)) : \
		     ((*((CARD16 *) (a))) | (*((a)+2) << 16)))
#define Store24(a,v) ((unsigned long) (a) & 1 ? \
		      ((*(a) = (CARD8) (v)), \
		       (*((CARD16 *) ((a)+1)) = (CARD16) ((v) >> 8))) : \
		      ((*((CARD16 *) (a)) = (CARD16) (v)),\
		       (*((a)+2) = (CARD8) ((v) >> 16))))
#endif
		      
CARD32
IcOver (CARD32 x, CARD32 y)
{
    CARD16  a = ~x >> 24;
    CARD16  t;
    CARD32  m,n,o,p;

    m = IcOverU(x,y,0,a,t);
    n = IcOverU(x,y,8,a,t);
    o = IcOverU(x,y,16,a,t);
    p = IcOverU(x,y,24,a,t);
    return m|n|o|p;
}

CARD32
IcOver24 (CARD32 x, CARD32 y)
{
    CARD16  a = ~x >> 24;
    CARD16  t;
    CARD32  m,n,o;

    m = IcOverU(x,y,0,a,t);
    n = IcOverU(x,y,8,a,t);
    o = IcOverU(x,y,16,a,t);
    return m|n|o;
}

CARD32
IcIn (CARD32 x, CARD8 y)
{
    CARD16  a = y;
    CARD16  t;
    CARD32  m,n,o,p;

    m = IcInU(x,0,a,t);
    n = IcInU(x,8,a,t);
    o = IcInU(x,16,a,t);
    p = IcInU(x,24,a,t);
    return m|n|o|p;
}

#define IcComposeGetSolid(image, bits) { \
    IcBits	*__bits__; \
    IcStride	__stride__; \
    int		__bpp__; \
    int		__xoff__,__yoff__; \
\
    IcGetPixels((image)->pixels,__bits__,__stride__,__bpp__,__xoff__,__yoff__); \
    switch (__bpp__) { \
    case 32: \
	(bits) = *(CARD32 *) __bits__; \
	break; \
    case 24: \
	(bits) = Fetch24 ((CARD8 *) __bits__); \
	break; \
    case 16: \
	(bits) = *(CARD16 *) __bits__; \
	(bits) = cvt0565to8888(bits); \
	break; \
    default: \
	return; \
    } \
    /* manage missing src alpha */ \
    if ((image)->image_format->direct.alphaMask == 0) \
	(bits) |= 0xff000000; \
}

#define IcComposeGetStart(image,x,y,type,stride,line,mul) {\
    IcBits	*__bits__; \
    IcStride	__stride__; \
    int		__bpp__; \
    int		__xoff__,__yoff__; \
\
    IcGetPixels((image)->pixels,__bits__,__stride__,__bpp__,__xoff__,__yoff__); \
    (stride) = __stride__ * sizeof (IcBits) / sizeof (type); \
    (line) = ((type *) __bits__) + (stride) * ((y) - __yoff__) + (mul) * ((x) - __xoff__); \
}

/*
 * Naming convention:
 *
 *  opSRCxMASKxDST
 */

void
IcCompositeSolidMask_nx8x8888 (CARD8      op,
			       IcImage    *iSrc,
			       IcImage    *iMask,
			       IcImage    *iDst,
			       INT16      xSrc,
			       INT16      ySrc,
			       INT16      xMask,
			       INT16      yMask,
			       INT16      xDst,
			       INT16      yDst,
			       CARD16     width,
			       CARD16     height)
{
    CARD32	src, srca;
    CARD32	*dstLine, *dst, d, dstMask;
    CARD8	*maskLine, *mask, m;
    IcStride	dstStride, maskStride;
    CARD16	w;

    IcComposeGetSolid(iSrc, src);
    
    dstMask = IcFullMask (iDst->pixels->depth);
    srca = src >> 24;
    if (src == 0)
	return;
    
    IcComposeGetStart (iDst, xDst, yDst, CARD32, dstStride, dstLine, 1);
    IcComposeGetStart (iMask, xMask, yMask, CARD8, maskStride, maskLine, 1);
    
    while (height--)
    {
	dst = dstLine;
	dstLine += dstStride;
	mask = maskLine;
	maskLine += maskStride;
	w = width;

	while (w--)
	{
	    m = *mask++;
	    if (m == 0xff)
	    {
		if (srca == 0xff)
		    *dst = src & dstMask;
		else
		    *dst = IcOver (src, *dst) & dstMask;
	    }
	    else if (m)
	    {
		d = IcIn (src, m);
		*dst = IcOver (d, *dst) & dstMask;
	    }
	    dst++;
	}
    }
}

void
IcCompositeSolidMask_nx8888x8888C (CARD8      op,
				   IcImage    *iSrc,
				   IcImage    *iMask,
				   IcImage    *iDst,
				   INT16      xSrc,
				   INT16      ySrc,
				   INT16      xMask,
				   INT16      yMask,
				   INT16      xDst,
				   INT16      yDst,
				   CARD16     width,
				   CARD16     height)
{
    CARD32	src, srca;
    CARD32	*dstLine, *dst, d, dstMask;
    CARD32	*maskLine, *mask, ma;
    IcStride	dstStride, maskStride;
    CARD16	w;
    CARD32	m, n, o, p;

    IcComposeGetSolid(iSrc, src);
    
    dstMask = IcFullMask (iDst->pixels->depth);
    srca = src >> 24;
    if (src == 0)
	return;
    
    IcComposeGetStart (iDst, xDst, yDst, CARD32, dstStride, dstLine, 1);
    IcComposeGetStart (iMask, xMask, yMask, CARD32, maskStride, maskLine, 1);
    
    while (height--)
    {
	dst = dstLine;
	dstLine += dstStride;
	mask = maskLine;
	maskLine += maskStride;
	w = width;

	while (w--)
	{
	    ma = *mask++;
	    if (ma == 0xffffffff)
	    {
		if (srca == 0xff)
		    *dst = src & dstMask;
		else
		    *dst = IcOver (src, *dst) & dstMask;
	    }
	    else if (ma)
	    {
		d = *dst;
#define IcInOverC(src,srca,msk,dst,i,result) { \
    CARD16  __a = IcGet8(msk,i); \
    CARD32  __t, __ta; \
    CARD32  __i; \
    __t = IcIntMult (IcGet8(src,i), __a,__i); \
    __ta = (CARD8) ~IcIntMult (srca, __a,__i); \
    __t = __t + IcIntMult(IcGet8(dst,i),__ta,__i); \
    __t = (CARD32) (CARD8) (__t | (-(__t >> 8))); \
    result = __t << (i); \
}
		IcInOverC (src, srca, ma, d, 0, m);
		IcInOverC (src, srca, ma, d, 8, n);
		IcInOverC (src, srca, ma, d, 16, o);
		IcInOverC (src, srca, ma, d, 24, p);
		*dst = m|n|o|p;
	    }
	    dst++;
	}
    }
}

void
IcCompositeSolidMask_nx8x0888 (CARD8      op,
			       IcImage    *iSrc,
			       IcImage    *iMask,
			       IcImage    *iDst,
			       INT16      xSrc,
			       INT16      ySrc,
			       INT16      xMask,
			       INT16      yMask,
			       INT16      xDst,
			       INT16      yDst,
			       CARD16     width,
			       CARD16     height)
{
    CARD32	src, srca;
    CARD8	*dstLine, *dst;
    CARD32	d;
    CARD8	*maskLine, *mask, m;
    IcStride	dstStride, maskStride;
    CARD16	w;

    IcComposeGetSolid(iSrc, src);
    
    srca = src >> 24;
    if (src == 0)
	return;
    
    IcComposeGetStart (iDst, xDst, yDst, CARD8, dstStride, dstLine, 3);
    IcComposeGetStart (iMask, xMask, yMask, CARD8, maskStride, maskLine, 1);
    
    while (height--)
    {
	dst = dstLine;
	dstLine += dstStride;
	mask = maskLine;
	maskLine += maskStride;
	w = width;

	while (w--)
	{
	    m = *mask++;
	    if (m == 0xff)
	    {
		if (srca == 0xff)
		    d = src;
		else
		{
		    d = Fetch24(dst);
		    d = IcOver24 (src, d);
		}
		Store24(dst,d);
	    }
	    else if (m)
	    {
		d = IcOver24 (IcIn(src,m), Fetch24(dst));
		Store24(dst,d);
	    }
	    dst += 3;
	}
    }
}

void
IcCompositeSolidMask_nx8x0565 (CARD8      op,
				  IcImage    *iSrc,
				  IcImage    *iMask,
				  IcImage    *iDst,
				  INT16      xSrc,
				  INT16      ySrc,
				  INT16      xMask,
				  INT16      yMask,
				  INT16      xDst,
				  INT16      yDst,
				  CARD16     width,
				  CARD16     height)
{
    CARD32	src, srca;
    CARD16	*dstLine, *dst;
    CARD32	d;
    CARD8	*maskLine, *mask, m;
    IcStride	dstStride, maskStride;
    CARD16	w;

    IcComposeGetSolid(iSrc, src);
    
    srca = src >> 24;
    if (src == 0)
	return;
    
    IcComposeGetStart (iDst, xDst, yDst, CARD16, dstStride, dstLine, 1);
    IcComposeGetStart (iMask, xMask, yMask, CARD8, maskStride, maskLine, 1);
    
    while (height--)
    {
	dst = dstLine;
	dstLine += dstStride;
	mask = maskLine;
	maskLine += maskStride;
	w = width;

	while (w--)
	{
	    m = *mask++;
	    if (m == 0xff)
	    {
		if (srca == 0xff)
		    d = src;
		else
		{
		    d = *dst;
		    d = IcOver24 (src, cvt0565to8888(d));
		}
		*dst = cvt8888to0565(d);
	    }
	    else if (m)
	    {
		d = *dst;
		d = IcOver24 (IcIn(src,m), cvt0565to8888(d));
		*dst = cvt8888to0565(d);
	    }
	    dst++;
	}
    }
}

void
IcCompositeSolidMask_nx8888x0565C (CARD8      op,
				   IcImage    *iSrc,
				   IcImage    *iMask,
				   IcImage    *iDst,
				   INT16      xSrc,
				   INT16      ySrc,
				   INT16      xMask,
				   INT16      yMask,
				   INT16      xDst,
				   INT16      yDst,
				   CARD16     width,
				   CARD16     height)
{
    CARD32	src, srca;
    CARD16	src16;
    CARD16	*dstLine, *dst;
    CARD32	d;
    CARD32	*maskLine, *mask, ma;
    IcStride	dstStride, maskStride;
    CARD16	w;
    CARD32	m, n, o;

    IcComposeGetSolid(iSrc, src);
    
    srca = src >> 24;
    if (src == 0)
	return;
    
    src16 = cvt8888to0565(src);
    
    IcComposeGetStart (iDst, xDst, yDst, CARD16, dstStride, dstLine, 1);
    IcComposeGetStart (iMask, xMask, yMask, CARD32, maskStride, maskLine, 1);
    
    while (height--)
    {
	dst = dstLine;
	dstLine += dstStride;
	mask = maskLine;
	maskLine += maskStride;
	w = width;

	while (w--)
	{
	    ma = *mask++;
	    if (ma == 0xffffffff)
	    {
		if (srca == 0xff)
		{
		    *dst = src16;
		}
		else
		{
		    d = *dst;
		    d = IcOver24 (src, cvt0565to8888(d));
		    *dst = cvt8888to0565(d);
		}
	    }
	    else if (ma)
	    {
		d = *dst;
		d = cvt0565to8888(d);
		IcInOverC (src, srca, ma, d, 0, m);
		IcInOverC (src, srca, ma, d, 8, n);
		IcInOverC (src, srca, ma, d, 16, o);
		d = m|n|o;
		*dst = cvt8888to0565(d);
	    }
	    dst++;
	}
    }
}

void
IcCompositeSrc_8888x8888 (CARD8      op,
			 IcImage    *iSrc,
			 IcImage    *iMask,
			 IcImage    *iDst,
			 INT16      xSrc,
			 INT16      ySrc,
			 INT16      xMask,
			 INT16      yMask,
			 INT16      xDst,
			 INT16      yDst,
			 CARD16     width,
			 CARD16     height)
{
    CARD32	*dstLine, *dst, dstMask;
    CARD32	*srcLine, *src, s;
    IcStride	dstStride, srcStride;
    CARD8	a;
    CARD16	w;
    
    IcComposeGetStart (iDst, xDst, yDst, CARD32, dstStride, dstLine, 1);
    IcComposeGetStart (iSrc, xSrc, ySrc, CARD32, srcStride, srcLine, 1);
    
    dstMask = IcFullMask (iDst->pixels->depth);

    while (height--)
    {
	dst = dstLine;
	dstLine += dstStride;
	src = srcLine;
	srcLine += srcStride;
	w = width;

	while (w--)
	{
	    s = *src++;
	    a = s >> 24;
	    if (a == 0xff)
		*dst = s & dstMask;
	    else if (a)
		*dst = IcOver (s, *dst) & dstMask;
	    dst++;
	}
    }
}

void
IcCompositeSrc_8888x0888 (CARD8      op,
			 IcImage    *iSrc,
			 IcImage    *iMask,
			 IcImage    *iDst,
			 INT16      xSrc,
			 INT16      ySrc,
			 INT16      xMask,
			 INT16      yMask,
			 INT16      xDst,
			 INT16      yDst,
			 CARD16     width,
			 CARD16     height)
{
    CARD8	*dstLine, *dst;
    CARD32	d;
    CARD32	*srcLine, *src, s;
    CARD8	a;
    IcStride	dstStride, srcStride;
    CARD16	w;
    
    IcComposeGetStart (iDst, xDst, yDst, CARD8, dstStride, dstLine, 3);
    IcComposeGetStart (iSrc, xSrc, ySrc, CARD32, srcStride, srcLine, 1);
    
    while (height--)
    {
	dst = dstLine;
	dstLine += dstStride;
	src = srcLine;
	srcLine += srcStride;
	w = width;

	while (w--)
	{
	    s = *src++;
	    a = s >> 24;
	    if (a)
	    {
		if (a == 0xff)
		    d = s;
		else
		    d = IcOver24 (s, Fetch24(dst));
		Store24(dst,d);
	    }
	    dst += 3;
	}
    }
}

void
IcCompositeSrc_8888x0565 (CARD8      op,
			 IcImage    *iSrc,
			 IcImage    *iMask,
			 IcImage    *iDst,
			 INT16      xSrc,
			 INT16      ySrc,
			 INT16      xMask,
			 INT16      yMask,
			 INT16      xDst,
			 INT16      yDst,
			 CARD16     width,
			 CARD16     height)
{
    CARD16	*dstLine, *dst;
    CARD32	d;
    CARD32	*srcLine, *src, s;
    CARD8	a;
    IcStride	dstStride, srcStride;
    CARD16	w;
    
    IcComposeGetStart (iSrc, xSrc, ySrc, CARD32, srcStride, srcLine, 1);
    IcComposeGetStart (iDst, xDst, yDst, CARD16, dstStride, dstLine, 1);

    while (height--)
    {
	dst = dstLine;
	dstLine += dstStride;
	src = srcLine;
	srcLine += srcStride;
	w = width;

	while (w--)
	{
	    s = *src++;
	    a = s >> 24;
	    if (a)
	    {
		if (a == 0xff)
		    d = s;
		else
		{
		    d = *dst;
		    d = IcOver24 (s, cvt0565to8888(d));
		}
		*dst = cvt8888to0565(d);
	    }
	    dst++;
	}
    }
}

void
IcCompositeSrc_0565x0565 (CARD8      op,
			  IcImage    *iSrc,
			  IcImage    *iMask,
			  IcImage    *iDst,
			  INT16      xSrc,
			  INT16      ySrc,
			  INT16      xMask,
			  INT16      yMask,
			  INT16      xDst,
			  INT16      yDst,
			  CARD16     width,
			  CARD16     height)
{
    CARD16	*dstLine, *dst;
    CARD16	*srcLine, *src;
    IcStride	dstStride, srcStride;
    CARD16	w;
    
    IcComposeGetStart (iSrc, xSrc, ySrc, CARD16, srcStride, srcLine, 1);

    IcComposeGetStart (iDst, xDst, yDst, CARD16, dstStride, dstLine, 1);

    while (height--)
    {
	dst = dstLine;
	dstLine += dstStride;
	src = srcLine;
	srcLine += srcStride;
	w = width;

	while (w--)
	    *dst++ = *src++;
    }
}

void
IcCompositeSrcAdd_8000x8000 (CARD8	op,
			     IcImage    *iSrc,
			     IcImage    *iMask,
			     IcImage    *iDst,
			     INT16      xSrc,
			     INT16      ySrc,
			     INT16      xMask,
			     INT16      yMask,
			     INT16      xDst,
			     INT16      yDst,
			     CARD16     width,
			     CARD16     height)
{
    CARD8	*dstLine, *dst;
    CARD8	*srcLine, *src;
    IcStride	dstStride, srcStride;
    CARD8	w;
    CARD8	s, d;
    CARD16	t;
    
    IcComposeGetStart (iSrc, xSrc, ySrc, CARD8, srcStride, srcLine, 1);
    IcComposeGetStart (iDst, xDst, yDst, CARD8, dstStride, dstLine, 1);

    while (height--)
    {
	dst = dstLine;
	dstLine += dstStride;
	src = srcLine;
	srcLine += srcStride;
	w = width;

	while (w--)
	{
	    s = *src++;
	    if (s != 0xff)
	    {
		d = *dst;
		t = d + s;
		s = t | (0 - (t >> 8));
	    }
	    *dst++ = s;
	}
    }
}

void
IcCompositeSrcAdd_8888x8888 (CARD8	op,
			     IcImage    *iSrc,
			     IcImage    *iMask,
			     IcImage    *iDst,
			     INT16      xSrc,
			     INT16      ySrc,
			     INT16      xMask,
			     INT16      yMask,
			     INT16      xDst,
			     INT16      yDst,
			     CARD16     width,
			     CARD16     height)
{
    CARD32	*dstLine, *dst;
    CARD32	*srcLine, *src;
    IcStride	dstStride, srcStride;
    CARD16	w;
    CARD32	s, d;
    CARD16	t;
    CARD32	m,n,o,p;
    
    IcComposeGetStart (iSrc, xSrc, ySrc, CARD32, srcStride, srcLine, 1);
    IcComposeGetStart (iDst, xDst, yDst, CARD32, dstStride, dstLine, 1);

    while (height--)
    {
	dst = dstLine;
	dstLine += dstStride;
	src = srcLine;
	srcLine += srcStride;
	w = width;

	while (w--)
	{
	    s = *src++;
	    if (s != 0xffffffff)
	    {
		d = *dst;
		if (d)
		{
		    m = IcAdd(s,d,0,t);
		    n = IcAdd(s,d,8,t);
		    o = IcAdd(s,d,16,t);
		    p = IcAdd(s,d,24,t);
		    s = m|n|o|p;
		}
	    }
	    *dst++ = s;
	}
    }
}

void
IcCompositeSrcAdd_1000x1000 (CARD8	op,
			     IcImage    *iSrc,
			     IcImage    *iMask,
			     IcImage    *iDst,
			     INT16      xSrc,
			     INT16      ySrc,
			     INT16      xMask,
			     INT16      yMask,
			     INT16      xDst,
			     INT16      yDst,
			     CARD16     width,
			     CARD16     height)
{
    IcBits	*dstBits, *srcBits;
    IcStride	dstStride, srcStride;
    int		dstBpp, srcBpp;
    int		dstXoff, dstYoff;
    int		srcXoff, srcYoff;
    
    IcGetPixels(iSrc->pixels, srcBits, srcStride, srcBpp, srcXoff, srcYoff);

    IcGetPixels(iDst->pixels, dstBits, dstStride, dstBpp, dstXoff, dstYoff);

    IcBlt (srcBits + srcStride * (ySrc + srcYoff),
	   srcStride,
	   xSrc + srcXoff,

	   dstBits + dstStride * (yDst + dstYoff),
	   dstStride,
	   xDst + dstXoff,

	   width,
	   height,

	   GXor,
	   IC_ALLONES,
	   srcBpp,

	   FALSE,
	   FALSE);
}

void
IcCompositeSolidMask_nx1xn (CARD8      op,
			    IcImage    *iSrc,
			    IcImage    *iMask,
			    IcImage    *iDst,
			    INT16      xSrc,
			    INT16      ySrc,
			    INT16      xMask,
			    INT16      yMask,
			    INT16      xDst,
			    INT16      yDst,
			    CARD16     width,
			    CARD16     height)
{
    IcBits	*dstBits;
    IcStip	*maskBits;
    IcStride	dstStride, maskStride;
    int		dstBpp, maskBpp;
    int		dstXoff, dstYoff;
    int		maskXoff, maskYoff;
    IcBits	src;
    
    IcComposeGetSolid(iSrc, src);

    if ((src & 0xff000000) != 0xff000000)
    {
	IcCompositeGeneral  (op, iSrc, iMask, iDst,
			     xSrc, ySrc, xMask, yMask, xDst, yDst, 
			     width, height);
	return;
    }
    IcGetStipPixels (iMask->pixels, maskBits, maskStride, maskBpp, maskXoff, maskYoff);
    IcGetPixels (iDst->pixels, dstBits, dstStride, dstBpp, dstXoff, dstYoff);

    switch (dstBpp) {
    case 32:
	break;
    case 24:
	break;
    case 16:
	src = cvt8888to0565(src);
	break;
    }

    src = IcReplicatePixel (src, dstBpp);

    IcBltOne (maskBits + maskStride * (yMask + maskYoff),
	      maskStride,
	      xMask + maskXoff,

	      dstBits + dstStride * (yDst + dstYoff),
	      dstStride,
	      (xDst + dstXoff) * dstBpp,
	      dstBpp,

	      width * dstBpp,
	      height,

	      0x0,
	      src,
	      IC_ALLONES,
	      0x0);
}

# define mod(a,b)	((b) == 1 ? 0 : (a) >= 0 ? (a) % (b) : (b) - (-a) % (b))

void
IcComposite (char	op,
	     IcImage	*iSrc,
	     IcImage	*iMask,
	     IcImage	*iDst,
	     int	xSrc,
	     int	ySrc,
	     int	xMask,
	     int	yMask,
	     int	xDst,
	     int	yDst,
	     int	width,
	     int	height)
{
    Region	    region;
    int		    n;
    BoxPtr	    pbox;
    CompositeFunc   func;
    Bool	    srcRepeat = iSrc->repeat;
    Bool	    maskRepeat = FALSE;
    Bool	    srcAlphaMap = iSrc->alphaMap != 0;
    Bool	    maskAlphaMap = FALSE;
    Bool	    dstAlphaMap = iDst->alphaMap != 0;
    int		    x_msk, y_msk, x_src, y_src, x_dst, y_dst;
    int		    w, h, w_this, h_this;
    XRectangle	    rect;
    
    xDst += iDst->pixels->x;
    yDst += iDst->pixels->y;
    xSrc += iSrc->pixels->x;
    ySrc += iSrc->pixels->y;
    if (iMask)
    {
	xMask += iMask->pixels->x;
	yMask += iMask->pixels->y;
	maskRepeat = iMask->repeat;
	maskAlphaMap = iMask->alphaMap != 0;
    }

    region = XCreateRegion();
    rect.x = xDst;
    rect.y = yDst;
    rect.width = width;
    rect.height = height;
    XUnionRectWithRegion (&rect, region, region);
    
    if (!IcComputeCompositeRegion (region,
				   iSrc,
				   iMask,
				   iDst,
				   xSrc,
				   ySrc,
				   xMask,
				   yMask,
				   xDst,
				   yDst,
				   width,
				   height))
	return;
				   
    func = IcCompositeGeneral;
    if (!iSrc->transform && !(iMask && iMask->transform))
    if (!maskAlphaMap && !srcAlphaMap && !dstAlphaMap)
    switch (op) {
    case PictOpOver:
	if (iMask)
	{
	    if (srcRepeat && 
		iSrc->pixels->width == 1 &&
		iSrc->pixels->height == 1)
	    {
		srcRepeat = FALSE;
		if (PICT_FORMAT_COLOR(iSrc->format)) {
		    switch (iMask->format) {
		    case PICT_a8:
			switch (iDst->format) {
			case PICT_r5g6b5:
			case PICT_b5g6r5:
			    func = IcCompositeSolidMask_nx8x0565;
			    break;
			case PICT_r8g8b8:
			case PICT_b8g8r8:
			    func = IcCompositeSolidMask_nx8x0888;
			    break;
			case PICT_a8r8g8b8:
			case PICT_x8r8g8b8:
			case PICT_a8b8g8r8:
			case PICT_x8b8g8r8:
			    func = IcCompositeSolidMask_nx8x8888;
			    break;
			}
			break;
		    case PICT_a8r8g8b8:
			if (iMask->componentAlpha) {
			    switch (iDst->format) {
			    case PICT_a8r8g8b8:
			    case PICT_x8r8g8b8:
				func = IcCompositeSolidMask_nx8888x8888C;
				break;
			    case PICT_r5g6b5:
				func = IcCompositeSolidMask_nx8888x0565C;
				break;
			    }
			}
			break;
		    case PICT_a8b8g8r8:
			if (iMask->componentAlpha) {
			    switch (iDst->format) {
			    case PICT_a8b8g8r8:
			    case PICT_x8b8g8r8:
				func = IcCompositeSolidMask_nx8888x8888C;
				break;
			    case PICT_b5g6r5:
				func = IcCompositeSolidMask_nx8888x0565C;
				break;
			    }
			}
			break;
		    case PICT_a1:
			switch (iDst->format) {
			case PICT_r5g6b5:
			case PICT_b5g6r5:
			case PICT_r8g8b8:
			case PICT_b8g8r8:
			case PICT_a8r8g8b8:
			case PICT_x8r8g8b8:
			case PICT_a8b8g8r8:
			case PICT_x8b8g8r8:
			    func = IcCompositeSolidMask_nx1xn;
			    break;
			}
		    }
		}
	    }
	}
	else
	{
	    switch (iSrc->format) {
	    case PICT_a8r8g8b8:
	    case PICT_x8r8g8b8:
		switch (iDst->format) {
		case PICT_a8r8g8b8:
		case PICT_x8r8g8b8:
		    func = IcCompositeSrc_8888x8888;
		    break;
		case PICT_r8g8b8:
		    func = IcCompositeSrc_8888x0888;
		    break;
		case PICT_r5g6b5:
		    func = IcCompositeSrc_8888x0565;
		    break;
		}
		break;
	    case PICT_a8b8g8r8:
	    case PICT_x8b8g8r8:
		switch (iDst->format) {
		case PICT_a8b8g8r8:
		case PICT_x8b8g8r8:
		    func = IcCompositeSrc_8888x8888;
		    break;
		case PICT_b8g8r8:
		    func = IcCompositeSrc_8888x0888;
		    break;
		case PICT_b5g6r5:
		    func = IcCompositeSrc_8888x0565;
		    break;
		}
		break;
	    case PICT_r5g6b5:
		switch (iDst->format) {
		case PICT_r5g6b5:
		    func = IcCompositeSrc_0565x0565;
		    break;
		}
		break;
	    case PICT_b5g6r5:
		switch (iDst->format) {
		case PICT_b5g6r5:
		    func = IcCompositeSrc_0565x0565;
		    break;
		}
		break;
	    }
	}
	break;
    case PictOpAdd:
	if (iMask == 0)
	{
	    switch (iSrc->format) {
	    case PICT_a8r8g8b8:
		switch (iDst->format) {
		case PICT_a8r8g8b8:
		    func = IcCompositeSrcAdd_8888x8888;
		    break;
		}
		break;
	    case PICT_a8b8g8r8:
		switch (iDst->format) {
		case PICT_a8b8g8r8:
		    func = IcCompositeSrcAdd_8888x8888;
		    break;
		}
		break;
	    case PICT_a8:
		switch (iDst->format) {
		case PICT_a8:
		    func = IcCompositeSrcAdd_8000x8000;
		    break;
		}
		break;
	    case PICT_a1:
		switch (iDst->format) {
		case PICT_a1:
		    func = IcCompositeSrcAdd_1000x1000;
		    break;
		}
		break;
	    }
	}
	break;
    }
    n = region->numRects;
    pbox = region->rects;
    while (n--)
    {
	h = pbox->y2 - pbox->y1;
	y_src = pbox->y1 - yDst + ySrc;
	y_msk = pbox->y1 - yDst + yMask;
	y_dst = pbox->y1;
	while (h)
	{
	    h_this = h;
	    w = pbox->x2 - pbox->x1;
	    x_src = pbox->x1 - xDst + xSrc;
	    x_msk = pbox->x1 - xDst + xMask;
	    x_dst = pbox->x1;
	    if (maskRepeat)
	    {
		y_msk = mod (y_msk, iMask->pixels->height);
		if (h_this > iMask->pixels->height - y_msk)
		    h_this = iMask->pixels->height - y_msk;
	    }
	    if (srcRepeat)
	    {
		y_src = mod (y_src, iSrc->pixels->height);
		if (h_this > iSrc->pixels->height - y_src)
		    h_this = iSrc->pixels->height - y_src;
	    }
	    while (w)
	    {
		w_this = w;
		if (maskRepeat)
		{
		    x_msk = mod (x_msk, iMask->pixels->width);
		    if (w_this > iMask->pixels->width - x_msk)
			w_this = iMask->pixels->width - x_msk;
		}
		if (srcRepeat)
		{
		    x_src = mod (x_src, iSrc->pixels->width);
		    if (w_this > iSrc->pixels->width - x_src)
			w_this = iSrc->pixels->width - x_src;
		}
		(*func) (op, iSrc, iMask, iDst,
			 x_src, y_src, x_msk, y_msk, x_dst, y_dst, 
			 w_this, h_this);
		w -= w_this;
		x_src += w_this;
		x_msk += w_this;
		x_dst += w_this;
	    }
	    h -= h_this;
	    y_src += h_this;
	    y_msk += h_this;
	    y_dst += h_this;
	}
	pbox++;
    }
    XDestroyRegion (region);
}

/* XXX: Still need to port this
Bool
IcImageInit (ScreenPtr pScreen, PictFormatPtr formats, int nformats)
{

    PictureScreenPtr    ps;

    if (!miPictureInit (pScreen, formats, nformats))
	return FALSE;
    ps = GetPictureScreen(pScreen);
    ps->Composite = IcComposite;
    ps->Glyphs = miGlyphs;
    ps->CompositeRects = miCompositeRects;
    ps->RasterizeTrapezoid = IcRasterizeTrapezoid;

    return TRUE;
}
*/

void
IcImageDestroy (IcImage *image)
{
    if (image->freeCompClip)
	XDestroyRegion (image->pCompositeClip);
}

void
IcImageDestroyClip (IcImage *image)
{
    switch (image->clientClipType) {
    case CT_NONE:
	return;
    case CT_PIXMAP:
	IcImageDestroy (image->clientClip);
	break;
    default:
	XDestroyRegion (image->clientClip);
	break;
    }
    image->clientClip = NULL;
    image->clientClipType = CT_NONE;
}    

int
IcImageChangeClip (IcImage	*image,
		   int		type,
		   pointer	value,
		   int		n)
{
    pointer	clientClip;
    int		clientClipType;

    switch (type) {
    case CT_PIXMAP:
	return Success;
	/* XXX: Still need to figure out how to handle this case
	clientClip = (pointer) BITMAP_TO_REGION(pScreen, (PixmapPtr) value);
	if (!clientClip)
	    return BadAlloc;
	clientClipType = CT_REGION;
	(*pScreen->DestroyPixmap) ((PixmapPtr) value);
	break;
	*/
    case CT_REGION:
	clientClip = value;
	clientClipType = CT_REGION;
	break;
    case CT_NONE:
	clientClip = 0;
	clientClipType = CT_NONE;
	break;
    default:
	return Success;
	/* XXX: I don't see an Xlib version of RECTS_TO_REGION
	clientClip = (pointer) RECTS_TO_REGION(pScreen, n,
					       (xRectangle *) value,
					       type);
	if (!clientClip)
	    return BadAlloc;
	clientClipType = CT_REGION;
	free(value);
	break;
	*/
    }
    IcImageDestroyClip (image);
    image->clientClip = clientClip;
    image->clientClipType = clientClipType;
    image->stateChanges |= CPClipMask;
    return Success;
}

#ifdef XXX_STILL_NEED_TO_PORT_THIS
void
miValidatePicture (PicturePtr pPicture,
		   Mask       mask)
{
    DrawablePtr	    pDrawable = pPicture->pDrawable;

    if ((mask & (CPClipXOrigin|CPClipYOrigin|CPClipMask|CPSubwindowMode)) ||
	(pDrawable->serialNumber != (pPicture->serialNumber & DRAWABLE_SERIAL_BITS)))
    {
	if (pDrawable->type == DRAWABLE_WINDOW)
	{
	    WindowPtr       pWin = (WindowPtr) pDrawable;
	    RegionPtr       pregWin;
	    Bool            freeTmpClip, freeCompClip;

	    if (pPicture->subWindowMode == IncludeInferiors)
	    {
		pregWin = NotClippedByChildren(pWin);
		freeTmpClip = TRUE;
	    }
	    else
	    {
		pregWin = &pWin->clipList;
		freeTmpClip = FALSE;
	    }
	    freeCompClip = pPicture->freeCompClip;

	    /*
	     * if there is no client clip, we can get by with just keeping the
	     * pointer we got, and remembering whether or not should destroy
	     * (or maybe re-use) it later.  this way, we avoid unnecessary
	     * copying of regions.  (this wins especially if many clients clip
	     * by children and have no client clip.)
	     */
	    if (pPicture->clientClipType == CT_NONE)
	    {
		if (freeCompClip)
		    REGION_DESTROY(pScreen, pPicture->pCompositeClip);
		pPicture->pCompositeClip = pregWin;
		pPicture->freeCompClip = freeTmpClip;
	    }
	    else
	    {
		/*
		 * we need one 'real' region to put into the composite clip. if
		 * pregWin the current composite clip are real, we can get rid of
		 * one. if pregWin is real and the current composite clip isn't,
		 * use pregWin for the composite clip. if the current composite
		 * clip is real and pregWin isn't, use the current composite
		 * clip. if neither is real, create a new region.
		 */

		REGION_TRANSLATE(pScreen, pPicture->clientClip,
				 pDrawable->x + pPicture->clipOrigin.x,
				 pDrawable->y + pPicture->clipOrigin.y);

		if (freeCompClip)
		{
		    REGION_INTERSECT(pScreen, pPicture->pCompositeClip,
				     pregWin, pPicture->clientClip);
		    if (freeTmpClip)
			REGION_DESTROY(pScreen, pregWin);
		}
		else if (freeTmpClip)
		{
		    REGION_INTERSECT(pScreen, pregWin, pregWin, pPicture->clientClip);
		    pPicture->pCompositeClip = pregWin;
		}
		else
		{
		    pPicture->pCompositeClip = REGION_CREATE(pScreen, NullBox, 0);
		    REGION_INTERSECT(pScreen, pPicture->pCompositeClip,
				     pregWin, pPicture->clientClip);
		}
		pPicture->freeCompClip = TRUE;
		REGION_TRANSLATE(pScreen, pPicture->clientClip,
				 -(pDrawable->x + pPicture->clipOrigin.x),
				 -(pDrawable->y + pPicture->clipOrigin.y));
	    }
	}	/* end of composite clip for a window */
	else
	{
	    BoxRec          pixbounds;

	    /* XXX should we translate by drawable.x/y here ? */
	    /* If you want pixmaps in offscreen memory, yes */
	    pixbounds.x1 = pDrawable->x;
	    pixbounds.y1 = pDrawable->y;
	    pixbounds.x2 = pDrawable->x + pDrawable->width;
	    pixbounds.y2 = pDrawable->y + pDrawable->height;

	    if (pPicture->freeCompClip)
	    {
		REGION_RESET(pScreen, pPicture->pCompositeClip, &pixbounds);
	    }
	    else
	    {
		pPicture->freeCompClip = TRUE;
		pPicture->pCompositeClip = REGION_CREATE(pScreen, &pixbounds, 1);
	    }

	    if (pPicture->clientClipType == CT_REGION)
	    {
		if(pDrawable->x || pDrawable->y) {
		    REGION_TRANSLATE(pScreen, pPicture->clientClip,
				     pDrawable->x + pPicture->clipOrigin.x, 
				     pDrawable->y + pPicture->clipOrigin.y);
		    REGION_INTERSECT(pScreen, pPicture->pCompositeClip,
				     pPicture->pCompositeClip, pPicture->clientClip);
		    REGION_TRANSLATE(pScreen, pPicture->clientClip,
				     -(pDrawable->x + pPicture->clipOrigin.x), 
				     -(pDrawable->y + pPicture->clipOrigin.y));
		} else {
		    REGION_TRANSLATE(pScreen, pPicture->pCompositeClip,
				     -pPicture->clipOrigin.x, -pPicture->clipOrigin.y);
		    REGION_INTERSECT(pScreen, pPicture->pCompositeClip,
				     pPicture->pCompositeClip, pPicture->clientClip);
		    REGION_TRANSLATE(pScreen, pPicture->pCompositeClip,
				     pPicture->clipOrigin.x, pPicture->clipOrigin.y);
		}
	    }
	}	/* end of composite clip for pixmap */
    }
}
#endif

#define BOUND(v)	(INT16) ((v) < MINSHORT ? MINSHORT : (v) > MAXSHORT ? MAXSHORT : (v))

static __inline Bool
IcClipImageReg (Region		region,
		Region		clip,
		int		dx,
		int		dy)
{
    if (region->numRects == 1 &&
	clip->numRects == 1)
    {
	BoxPtr  pRbox = region->rects;
	BoxPtr  pCbox = clip->rects;
	int	v;

	if (pRbox->x1 < (v = pCbox->x1 + dx))
	    pRbox->x1 = BOUND(v);
	if (pRbox->x2 > (v = pCbox->x2 + dx))
	    pRbox->x2 = BOUND(v);
	if (pRbox->y1 < (v = pCbox->y1 + dy))
	    pRbox->y1 = BOUND(v);
	if (pRbox->y2 > (v = pCbox->y2 + dy))
	    pRbox->y2 = BOUND(v);
	if (pRbox->x1 >= pRbox->x2 ||
	    pRbox->y1 >= pRbox->y2)
	{
	    EMPTY_REGION (region);
	}
    }
    else
    {
	XOffsetRegion (region, dx, dy);
	XIntersectRegion (region, clip, region);
	XOffsetRegion (region, -dx, -dy);
    }
    return TRUE;
}
		  
static __inline Bool
IcClipImageSrc (Region		region,
		IcImage		*image,
		int		dx,
		int		dy)
{
    /* XXX what to do with clipping from transformed pictures? */
    if (image->transform)
	return TRUE;
    if (image->repeat)
    {
	if (image->clientClipType != CT_NONE)
	{
	    XOffsetRegion (region, 
			   dx - image->clipOrigin.x,
			   dy - image->clipOrigin.y);
	    XIntersectRegion (region, image->clientClip, region);
	    XOffsetRegion (region, 
			   - (dx - image->clipOrigin.x),
			   - (dy - image->clipOrigin.y));
	}
	return TRUE;
    }
    else
    {
	return IcClipImageReg (region,
			       image->pCompositeClip,
			       dx,
			       dy);
    }
    return TRUE;
}

Bool
IcComputeCompositeRegion (Region	region,
			  IcImage	*iSrc,
			  IcImage	*iMask,
			  IcImage	*iDst,
			  INT16		xSrc,
			  INT16		ySrc,
			  INT16		xMask,
			  INT16		yMask,
			  INT16		xDst,
			  INT16		yDst,
			  CARD16	width,
			  CARD16	height)
{
    int		v;

    region->extents.x1 = xDst;
    v = xDst + width;
    region->extents.x2 = BOUND(v);
    region->extents.y1 = yDst;
    v = yDst + height;
    region->extents.y2 = BOUND(v);
    /* Check for empty operation */
    if (region->extents.x1 >= region->extents.x2 ||
	region->extents.y1 >= region->extents.y2)
    {
	EMPTY_REGION (region);
	return TRUE;
    }
    /* clip against src */
    if (!IcClipImageSrc (region, iSrc, xDst - xSrc, yDst - ySrc))
    {
	XDestroyRegion (region);
	return FALSE;
    }
    if (iSrc->alphaMap)
    {
	if (!IcClipImageSrc (region, iSrc->alphaMap,
			     xDst - (xSrc + iSrc->alphaOrigin.x),
			     yDst - (ySrc + iSrc->alphaOrigin.y)))
	{
	    XDestroyRegion (region);
	    return FALSE;
	}
    }
    /* clip against mask */
    if (iMask)
    {
	if (!IcClipImageSrc (region, iMask, xDst - xMask, yDst - yMask))
	{
	    XDestroyRegion (region);
	    return FALSE;
	}	
	if (iMask->alphaMap)
	{
	    if (!IcClipImageSrc (region, iMask->alphaMap,
				 xDst - (xMask + iMask->alphaOrigin.x),
				 yDst - (yMask + iMask->alphaOrigin.y)))
	    {
		XDestroyRegion (region);
		return FALSE;
	    }
	}
    }
    if (!IcClipImageReg (region, iDst->pCompositeClip, 0, 0))
    {
	XDestroyRegion (region);
	return FALSE;
    }
    if (iDst->alphaMap)
    {
	if (!IcClipImageReg (region, iDst->alphaMap->pCompositeClip,
			     -iDst->alphaOrigin.x,
			     -iDst->alphaOrigin.y))
	{
	    XDestroyRegion (region);
	    return FALSE;
	}
    }
    return TRUE;
}

/* XXX: Do we need these functions?
void
miRenderColorToPixel (PictFormatPtr format,
		      xRenderColor  *color,
		      CARD32	    *pixel)
{
    CARD32	    r, g, b, a;
    miIndexedPtr    pIndexed;

    switch (format->type) {
    case PictTypeDirect:
	r = color->red >> (16 - Ones (format->direct.redMask));
	g = color->green >> (16 - Ones (format->direct.greenMask));
	b = color->blue >> (16 - Ones (format->direct.blueMask));
	a = color->alpha >> (16 - Ones (format->direct.alphaMask));
	r = r << format->direct.red;
	g = g << format->direct.green;
	b = b << format->direct.blue;
	a = a << format->direct.alpha;
	*pixel = r|g|b|a;
	break;
    case PictTypeIndexed:
	pIndexed = (miIndexedPtr) (format->index.devPrivate);
	if (pIndexed->color)
	{
	    r = color->red >> 11;
	    g = color->green >> 11;
	    b = color->blue >> 11;
	    *pixel = miIndexToEnt15 (pIndexed, (r << 10) | (g << 5) | b);
	}
	else
	{
	    r = color->red >> 8;
	    g = color->green >> 8;
	    b = color->blue >> 8;
	    *pixel = miIndexToEntY24 (pIndexed, (r << 16) | (g << 8) | b);
	}
	break;
    }
}

static CARD16
miFillColor (CARD32 pixel, int bits)
{
    while (bits < 16)
    {
	pixel |= pixel << bits;
	bits <<= 1;
    }
    return (CARD16) pixel;
}

void
miRenderPixelToColor (PictFormatPtr format,
		      CARD32	    pixel,
		      xRenderColor  *color)
{
    CARD32	    r, g, b, a;
    miIndexedPtr    pIndexed;
    
    switch (format->type) {
    case PictTypeDirect:
	r = (pixel >> format->direct.red) & format->direct.redMask;
	g = (pixel >> format->direct.green) & format->direct.greenMask;
	b = (pixel >> format->direct.blue) & format->direct.blueMask;
	a = (pixel >> format->direct.alpha) & format->direct.alphaMask;
	color->red = miFillColor (r, Ones (format->direct.redMask));
	color->green = miFillColor (r, Ones (format->direct.greenMask));
	color->blue = miFillColor (r, Ones (format->direct.blueMask));
	color->alpha = miFillColor (r, Ones (format->direct.alphaMask));
	break;
    case PictTypeIndexed:
	pIndexed = (miIndexedPtr) (format->index.devPrivate);
	pixel = pIndexed->rgba[pixel & (MI_MAX_INDEXED-1)];
	r = (pixel >> 16) & 0xff;
	g = (pixel >>  8) & 0xff;
	b = (pixel      ) & 0xff;
	color->red = miFillColor (r, 8);
	color->green = miFillColor (g, 8);
	color->blue = miFillColor (b, 8);
	color->alpha = 0xffff;
	break;
    }
}

Bool
miPictureInit (ScreenPtr pScreen, PictFormatPtr formats, int nformats)
{
    PictureScreenPtr    ps;
    
    if (!PictureInit (pScreen, formats, nformats))
	return FALSE;
    ps = GetPictureScreen(pScreen);
    ps->CreatePicture = miCreatePicture;
    ps->DestroyPicture = miDestroyPicture;
    ps->ChangePictureClip = miChangePictureClip;
    ps->DestroyPictureClip = miDestroyPictureClip;
    ps->ChangePicture = miChangePicture;
    ps->ValidatePicture = miValidatePicture;
    ps->InitIndexed = miInitIndexed;
    ps->CloseIndexed = miCloseIndexed;
    ps->UpdateIndexed = miUpdateIndexed;

    ps->Composite	= 0;
    ps->Glyphs		= miGlyphs;
    ps->CompositeRects	= miCompositeRects;
    ps->Trapezoids	= miTrapezoids;
    ps->Triangles	= miTriangles;
    ps->TriStrip	= miTriStrip;
    ps->TriFan		= miTriFan;
    
    return TRUE;
}
*/
