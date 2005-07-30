/*
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

#include "pixman-xserver-compat.h"

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
		      
static CARD32
fbOver (CARD32 x, CARD32 y)
{
    CARD16  a = ~x >> 24;
    CARD16  t;
    CARD32  m,n,o,p;

    m = FbOverU(x,y,0,a,t);
    n = FbOverU(x,y,8,a,t);
    o = FbOverU(x,y,16,a,t);
    p = FbOverU(x,y,24,a,t);
    return m|n|o|p;
}

static CARD32
fbOver24 (CARD32 x, CARD32 y)
{
    CARD16  a = ~x >> 24;
    CARD16  t;
    CARD32  m,n,o;

    m = FbOverU(x,y,0,a,t);
    n = FbOverU(x,y,8,a,t);
    o = FbOverU(x,y,16,a,t);
    return m|n|o;
}

static CARD32
fbIn (CARD32 x, CARD8 y)
{
    CARD16  a = y;
    CARD16  t;
    CARD32  m,n,o,p;

    m = FbInU(x,0,a,t);
    n = FbInU(x,8,a,t);
    o = FbInU(x,16,a,t);
    p = FbInU(x,24,a,t);
    return m|n|o|p;
}

static CARD32
fbIn24 (CARD32 x, CARD8 y)
{
    CARD16  a = y;
    CARD16  t;
    CARD32  m,n,o,p;

    m = FbInU(x,0,a,t);
    n = FbInU(x,8,a,t);
    o = FbInU(x,16,a,t);
    p = (y << 24);
    return m|n|o|p;
}

#define fbComposeGetSolid(image, bits) { \
    FbBits	*__bits__; \
    FbStride	__stride__; \
    int		__bpp__; \
    int		__xoff__,__yoff__; \
\
    FbGetPixels((image)->pixels,__bits__,__stride__,__bpp__,__xoff__,__yoff__); \
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
    case 8: \
	(bits) = *(CARD8 *) __bits__; \
	(bits) = (bits) << 24; \
	break; \
    case 1: \
	(bits) = *(CARD32 *) __bits__; \
	(bits) = FbLeftStipBits((bits),1) ? 0xff000000 : 0x00000000;\
	break; \
    default: \
	return; \
    } \
    /* manage missing src alpha */ \
    if ((image)->image_format.alphaMask == 0) \
	(bits) |= 0xff000000; \
}

#define fbComposeGetStart(image,x,y,type,stride,line,mul) {\
    FbBits	*__bits__; \
    FbStride	__stride__; \
    int		__bpp__; \
    int		__xoff__,__yoff__; \
\
    FbGetPixels((image)->pixels,__bits__,__stride__,__bpp__,__xoff__,__yoff__); \
    (stride) = __stride__ * sizeof (FbBits) / sizeof (type); \
    (line) = ((type *) __bits__) + (stride) * ((y) + __yoff__) + (mul) * ((x) + __xoff__); \
}

#define genericCombine24(a,b,c,d) (((a)*(c)+(b)*(d)))

#define fastcombine32(alpha, source, destval, destptr, dstrb, dstag, drb, dag) \
	dstrb=destval&0xFF00FF; dstag=(destval>>8)&0xFF00FF; \
	drb=((source&0xFF00FF)-dstrb)*alpha; dag=(((source>>8)&0xFF00FF)-dstag)*alpha; \
	*destptr++=((((drb>>8) + dstrb) & 0x00FF00FF) | ((((dag>>8) + dstag) << 8) & 0xFF00FF00)); \

#define fastcombine32(alpha, source, destval, destptr, dstrb, dstag, drb, dag) \
	dstrb=destval&0xFF00FF; dstag=(destval>>8)&0xFF00FF; \
	drb=((source&0xFF00FF)-dstrb)*alpha; dag=(((source>>8)&0xFF00FF)-dstag)*alpha; \
	*destptr++=((((drb>>8) + dstrb) & 0x00FF00FF) | ((((dag>>8) + dstag) << 8) & 0xFF00FF00)); \
	
// Note: this macro expects 6 bits of alpha, not 8!
#define fastCombine0565(alpha, source, destval, destptr) { \
	CARD16 dstrb = destval & 0xf81f; CARD16 dstg  = destval & 0x7e0; \
	CARD32 drb = ((source&0xf81f)-dstrb)*alpha; CARD32 dg=((source & 0x7e0)-dstg)*alpha; \
	destptr= ((((drb>>6) + dstrb)&0xf81f) | (((dg>>6)  + dstg) & 0x7e0)); \
	}

#if IMAGE_BYTE_ORDER == LSBFirst
	#define setupPackedReader(count,temp,where,workingWhere,workingVal) count=(int)where; \
					temp=count&3; \
					where-=temp; \
					workingWhere=(CARD32 *)where; \
					workingVal=*workingWhere++; \
					count=4-temp; \
					workingVal>>=(8*temp)
	#define readPacked(where,x,y,z) {if(!(x)) { (x)=4; y=*z++; } where=(y)&0xff; (y)>>=8; (x)--;}
	#define readPackedSource(where) readPacked(where,ws,workingSource,wsrc)
	#define readPackedDest(where) readPacked(where,wd,workingiDest,widst)
	#define writePacked(what) workingoDest>>=8; workingoDest|=(what<<24); ww--; if(!ww) { ww=4; *wodst++=workingoDest; } 
#else
	#warning "I havn't tested fbCompositeTrans_0888xnx0888() on big endian yet!"
	#define setupPackedReader(count,temp,where,workingWhere,workingVal) count=(int)where; \
					temp=count&3; \
					where-=temp; \
					workingWhere=(CARD32 *)where; \
					workingVal=*workingWhere++; \
					count=4-temp; \
					workingVal<<=(8*temp)
	#define readPacked(where,x,y,z) {if(!(x)) { (x)=4; y=*z++; } where=(y)>>24; (y)<<=8; (x)--;}
	#define readPackedSource(where) readPacked(where,ws,workingSource,wsrc)
	#define readPackedDest(where) readPacked(where,wd,workingiDest,widst)
	#define writePacked(what) workingoDest<<=8; workingoDest|=what; ww--; if(!ww) { ww=4; *wodst++=workingoDest; } 
#endif
/*
 * Naming convention:
 *
 *  opSRCxMASKxDST
 */

static void
fbCompositeSolidMask_nx8x8888 (pixman_operator_t   op,
			       PicturePtr pSrc,
			       PicturePtr pMask,
			       PicturePtr pDst,
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
    FbStride	dstStride, maskStride;
    CARD16	w;

    fbComposeGetSolid(pSrc, src);
    
    dstMask = FbFullMask (pDst->pixels->depth);
    srca = src >> 24;
    if (src == 0)
	return;
    
    fbComposeGetStart (pDst, xDst, yDst, CARD32, dstStride, dstLine, 1);
    fbComposeGetStart (pMask, xMask, yMask, CARD8, maskStride, maskLine, 1);
    
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
		    *dst = fbOver (src, *dst) & dstMask;
	    }
	    else if (m)
	    {
		d = fbIn (src, m);
		*dst = fbOver (d, *dst) & dstMask;
	    }
	    dst++;
	}
    }
}

static void
fbCompositeSolidMask_nx8888x8888C (pixman_operator_t   op,
				   PicturePtr pSrc,
				   PicturePtr pMask,
				   PicturePtr pDst,
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
    FbStride	dstStride, maskStride;
    CARD16	w;
    CARD32	m, n, o, p;

    fbComposeGetSolid(pSrc, src);
    
    dstMask = FbFullMask (pDst->pixels->depth);
    srca = src >> 24;
    if (src == 0)
	return;
    
    fbComposeGetStart (pDst, xDst, yDst, CARD32, dstStride, dstLine, 1);
    fbComposeGetStart (pMask, xMask, yMask, CARD32, maskStride, maskLine, 1);
    
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
		    *dst = fbOver (src, *dst) & dstMask;
	    }
	    else if (ma)
	    {
		d = *dst;
#define FbInOverC(src,srca,msk,dst,i,result) { \
    CARD16  __a = FbGet8(msk,i); \
    CARD32  __t, __ta; \
    CARD32  __i; \
    __t = FbIntMult (FbGet8(src,i), __a,__i); \
    __ta = (CARD8) ~FbIntMult (srca, __a,__i); \
    __t = __t + FbIntMult(FbGet8(dst,i),__ta,__i); \
    __t = (CARD32) (CARD8) (__t | (-(__t >> 8))); \
    result = __t << (i); \
}
		FbInOverC (src, srca, ma, d, 0, m);
		FbInOverC (src, srca, ma, d, 8, n);
		FbInOverC (src, srca, ma, d, 16, o);
		FbInOverC (src, srca, ma, d, 24, p);
		*dst = m|n|o|p;
	    }
	    dst++;
	}
    }
}

#define srcAlphaCombine24(a,b) genericCombine24(a,b,srca,srcia)
static void
fbCompositeSolidMask_nx8x0888 (pixman_operator_t   op,
			       PicturePtr pSrc,
			       PicturePtr pMask,
			       PicturePtr pDst,
			       INT16      xSrc,
			       INT16      ySrc,
			       INT16      xMask,
			       INT16      yMask,
			       INT16      xDst,
			       INT16      yDst,
			       CARD16     width,
			       CARD16     height)
{
    CARD32	src, srca, srcia;
    CARD8	*dstLine, *dst, *edst;
    CARD32	d;
    CARD8	*maskLine, *mask, m;
    FbStride	dstStride, maskStride;
    CARD16	w;
	CARD32 rs,gs,bs,rd,gd,bd;

    fbComposeGetSolid(pSrc, src);
    
    srca = src >> 24;
    srcia = 255-srca;
    if (src == 0)
	return;

	rs=src&0xff;
	gs=(src>>8)&0xff;
	bs=(src>>16)&0xff;
    
    fbComposeGetStart (pDst, xDst, yDst, CARD8, dstStride, dstLine, 3);
    fbComposeGetStart (pMask, xMask, yMask, CARD8, maskStride, maskLine, 1);

    while (height--)
	{
		// fixme: cleanup unused
		unsigned int wt,wd;
		CARD32 workingiDest;
		CARD32 *widst;
		
		edst=dst = dstLine;
		dstLine += dstStride;
		mask = maskLine;
		maskLine += maskStride;
		w = width;
		
#ifndef NO_MASKED_PACKED_READ
		setupPackedReader(wd,wt,edst,widst,workingiDest);
#endif
				
		while (w--)
		{
#ifndef NO_MASKED_PACKED_READ
			readPackedDest(rd);
			readPackedDest(gd);
			readPackedDest(bd);
#else
			rd= *edst++;
			gd= *edst++;
			bd= *edst++;
#endif
			m = *mask++;
			if (m == 0xff)
			{
				if (srca == 0xff)
				{
					*dst++=rs;
					*dst++=gs;
					*dst++=bs;
				}
				else
				{
					*dst++=(srcAlphaCombine24(rs, rd)>>8);
					*dst++=(srcAlphaCombine24(gs, gd)>>8);
					*dst++=(srcAlphaCombine24(bs, bd)>>8);
				}
			}
			else if (m)
			{
				int na=(srca*(int)m)>>8;
				int nia=255-na;
				*dst++=(genericCombine24(rs, rd, na, nia)>>8);
				*dst++=(genericCombine24(gs, gd, na, nia)>>8);
				*dst++=(genericCombine24(bs, bd, na, nia)>>8);
			}
			else
			{
				dst+=3;
			}
		}
	}
}

static void
fbCompositeSolidMask_nx8x0565 (pixman_operator_t      op,
				  PicturePtr pSrc,
				  PicturePtr pMask,
				  PicturePtr pDst,
				  INT16      xSrc,
				  INT16      ySrc,
				  INT16      xMask,
				  INT16      yMask,
				  INT16      xDst,
				  INT16      yDst,
				  CARD16     width,
				  CARD16     height)
{
    CARD32	src, srca,na, rsrca;
    CARD16	*dstLine, *dst;
    CARD16	d;
    CARD8	*maskLine, *mask, m;
    FbStride	dstStride, maskStride;
    CARD16	w,src16;

    fbComposeGetSolid(pSrc, src);
    src16 = cvt8888to0565(src);
    
    rsrca = src >> 24;
	srca=rsrca>>2;
    if (src == 0)
		return;
    
    fbComposeGetStart (pDst, xDst, yDst, CARD16, dstStride, dstLine, 1);
    fbComposeGetStart (pMask, xMask, yMask, CARD8, maskStride, maskLine, 1);
   
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
				{
					*dst=src16;
				}
				else
				{
					d = *dst;
					fastCombine0565(srca, src16, d, *dst++);
				}
			}
			else if (m)
			{
				na=(rsrca*(int)m)>>10;
				d = *dst;
				fastCombine0565(na, src16, d, *dst++);
			}
			else
				dst++;
		}
	}
}


static void
fbCompositeSolidMask_nx8888x0565C (pixman_operator_t   op,
				   PicturePtr pSrc,
				   PicturePtr pMask,
				   PicturePtr pDst,
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
    FbStride	dstStride, maskStride;
    CARD16	w;
    CARD32	m, n, o;

    fbComposeGetSolid(pSrc, src);
    
    srca = src >> 24;
    if (src == 0)
	return;
    
    src16 = cvt8888to0565(src);
    
    fbComposeGetStart (pDst, xDst, yDst, CARD16, dstStride, dstLine, 1);
    fbComposeGetStart (pMask, xMask, yMask, CARD32, maskStride, maskLine, 1);
    
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
		    d = fbOver24 (src, cvt0565to8888(d));
		    *dst = cvt8888to0565(d);
		}
	    }
	    else if (ma)
	    {
		d = *dst;
		d = cvt0565to8888(d);
		FbInOverC (src, srca, ma, d, 0, m);
		FbInOverC (src, srca, ma, d, 8, n);
		FbInOverC (src, srca, ma, d, 16, o);
		d = m|n|o;
		*dst = cvt8888to0565(d);
	    }
	    dst++;
	}
    }
}

static void
fbCompositeSrc_8888x8888 (pixman_operator_t  op,
			 PicturePtr pSrc,
			 PicturePtr pMask,
			 PicturePtr pDst,
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
    FbStride	dstStride, srcStride;
    CARD8	a;
    CARD16	w;
    
    fbComposeGetStart (pDst, xDst, yDst, CARD32, dstStride, dstLine, 1);
    fbComposeGetStart (pSrc, xSrc, ySrc, CARD32, srcStride, srcLine, 1);
    
    dstMask = FbFullMask (pDst->pixels->depth);

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
		*dst = fbOver (s, *dst) & dstMask;
	    dst++;
	}
    }
}

static void
fbCompositeSrc_8888x0888 (pixman_operator_t  op,
			 PicturePtr pSrc,
			 PicturePtr pMask,
			 PicturePtr pDst,
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
    FbStride	dstStride, srcStride;
    CARD16	w;
    
    fbComposeGetStart (pDst, xDst, yDst, CARD8, dstStride, dstLine, 3);
    fbComposeGetStart (pSrc, xSrc, ySrc, CARD32, srcStride, srcLine, 1);
    
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
		    d = fbOver24 (s, Fetch24(dst));
		Store24(dst,d);
	    }
	    dst += 3;
	}
    }
}

static void
fbCompositeSrc_8888x0565 (pixman_operator_t  op,
			 PicturePtr pSrc,
			 PicturePtr pMask,
			 PicturePtr pDst,
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
    FbStride	dstStride, srcStride;
    CARD16	w;
    
    fbComposeGetStart (pSrc, xSrc, ySrc, CARD32, srcStride, srcLine, 1);
    fbComposeGetStart (pDst, xDst, yDst, CARD16, dstStride, dstLine, 1);

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
		    d = fbOver24 (s, cvt0565to8888(d));
		}
		*dst = cvt8888to0565(d);
	    }
	    dst++;
	}
    }
}

static void
fbCompositeSrc_0565x0565 (pixman_operator_t   op,
			  PicturePtr pSrc,
			  PicturePtr pMask,
			  PicturePtr pDst,
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
    FbStride	dstStride, srcStride;
    CARD16	w;
    
    fbComposeGetStart (pSrc, xSrc, ySrc, CARD16, srcStride, srcLine, 1);

    fbComposeGetStart (pDst, xDst, yDst, CARD16, dstStride, dstLine, 1);

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

static void
fbCompositeSrcAdd_8000x8000 (pixman_operator_t	  op,
			     PicturePtr pSrc,
			     PicturePtr pMask,
			     PicturePtr pDst,
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
    FbStride	dstStride, srcStride;
    CARD16	w;
    CARD8	s, d;
    CARD16	t;
    
    fbComposeGetStart (pSrc, xSrc, ySrc, CARD8, srcStride, srcLine, 1);
    fbComposeGetStart (pDst, xDst, yDst, CARD8, dstStride, dstLine, 1);

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
	    if (s)
	    {
		if (s != 0xff)
		{
		    d = *dst;
		    t = d + s;
		    s = t | (0 - (t >> 8));
		}
		*dst = s;
	    }
	    dst++;
	}
    }
}

static void
fbCompositeSrcAdd_8888x8888 (pixman_operator_t   op,
			     PicturePtr pSrc,
			     PicturePtr pMask,
			     PicturePtr pDst,
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
    FbStride	dstStride, srcStride;
    CARD16	w;
    CARD32	s, d;
    CARD16	t;
    CARD32	m,n,o,p;
    
    fbComposeGetStart (pSrc, xSrc, ySrc, CARD32, srcStride, srcLine, 1);
    fbComposeGetStart (pDst, xDst, yDst, CARD32, dstStride, dstLine, 1);

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
	    if (s)
	    {
		if (s != 0xffffffff)
		{
		    d = *dst;
		    if (d)
		    {
			m = FbAdd(s,d,0,t);
			n = FbAdd(s,d,8,t);
			o = FbAdd(s,d,16,t);
			p = FbAdd(s,d,24,t);
			s = m|n|o|p;
		    }
		}
		*dst = s;
	    }
	    dst++;
	}
    }
}

static void
fbCompositeSrcAdd_1000x1000 (pixman_operator_t   op,
			     PicturePtr pSrc,
			     PicturePtr pMask,
			     PicturePtr pDst,
			     INT16      xSrc,
			     INT16      ySrc,
			     INT16      xMask,
			     INT16      yMask,
			     INT16      xDst,
			     INT16      yDst,
			     CARD16     width,
			     CARD16     height)
{
    FbBits	*dstBits, *srcBits;
    FbStride	dstStride, srcStride;
    int		dstBpp, srcBpp;
    int		dstXoff, dstYoff;
    int		srcXoff, srcYoff;
    
    FbGetPixels(pSrc->pixels, srcBits, srcStride, srcBpp, srcXoff, srcYoff);

    FbGetPixels(pDst->pixels, dstBits, dstStride, dstBpp, dstXoff, dstYoff);

    fbBlt (srcBits + srcStride * (ySrc + srcYoff),
	   srcStride,
	   xSrc + srcXoff,

	   dstBits + dstStride * (yDst + dstYoff),
	   dstStride,
	   xDst + dstXoff,

	   width,
	   height,

	   GXor,
	   FB_ALLONES,
	   srcBpp,

	   FALSE,
	   FALSE);
}

static void
fbCompositeSolidMask_nx1xn (pixman_operator_t   op,
			    PicturePtr pSrc,
			    PicturePtr pMask,
			    PicturePtr pDst,
			    INT16      xSrc,
			    INT16      ySrc,
			    INT16      xMask,
			    INT16      yMask,
			    INT16      xDst,
			    INT16      yDst,
			    CARD16     width,
			    CARD16     height)
{
    FbBits	*dstBits;
    FbStip	*maskBits;
    FbStride	dstStride, maskStride;
    int		dstBpp, maskBpp;
    int		dstXoff, dstYoff;
    int		maskXoff, maskYoff;
    FbBits	src;
    
    fbComposeGetSolid(pSrc, src);

    if ((src & 0xff000000) != 0xff000000)
    {
	pixman_compositeGeneral  (op, pSrc, pMask, pDst,
			     xSrc, ySrc, xMask, yMask, xDst, yDst, 
			     width, height);
	return;
    }
    FbGetStipPixels (pMask->pixels, maskBits, maskStride, maskBpp, maskXoff, maskYoff);
    FbGetPixels (pDst->pixels, dstBits, dstStride, dstBpp, dstXoff, dstYoff);

    switch (dstBpp) {
    case 32:
	break;
    case 24:
	break;
    case 16:
	src = cvt8888to0565(src);
	break;
    }

    src = fbReplicatePixel (src, dstBpp);

    fbBltOne (maskBits + maskStride * (yMask + maskYoff),
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
	      FB_ALLONES,
	      0x0);
}

/*
 * Apply a constant alpha value in an over computation
 */

static void
fbCompositeTrans_0565xnx0565(pixman_operator_t      op,
			     PicturePtr pSrc,
			     PicturePtr pMask,
			     PicturePtr pDst,
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
    FbStride	dstStride, srcStride;
    CARD16	w;
    FbBits	mask;
    CARD8	maskAlpha;
    CARD16	s_16, d_16, r_16;
    CARD32	s_32, d_32, i_32, r_32;
    
    fbComposeGetSolid (pMask, mask);
    maskAlpha = mask >> 26;
    
    if (!maskAlpha)
	return;
    if (maskAlpha == 0xff)
    {
	fbCompositeSrc_0565x0565 (op, pSrc, pMask, pDst,
				  xSrc, ySrc, xMask, yMask, xDst, yDst, 
				  width, height);
	return;
    }

    fbComposeGetStart (pSrc, xSrc, ySrc, CARD16, srcStride, srcLine, 1);
    fbComposeGetStart (pDst, xDst, yDst, CARD16, dstStride, dstLine, 1);

    while (height--)
	{
		CARD32 *isrc;
		dst = dstLine;
		dstLine += dstStride;
		src = srcLine;
		srcLine += srcStride;
		w = width;
		
		if(((int)src&1)==1)
		{
			s_16 = *src++;
			d_16 = *dst;
			fastCombine0565(maskAlpha, s_16, d_16, *dst++);
			w--;
		}
		isrc=(CARD32 *)src;
		while (w>1)
		{
			s_32=*isrc++;
#if IMAGE_BYTE_ORDER == LSBFirst
			s_16=s_32&0xffff;
#else
			s_16=s_32>>16;
#endif
			d_16 = *dst;
			fastCombine0565(maskAlpha, s_16, d_16, *dst++);
#if IMAGE_BYTE_ORDER == LSBFirst
			s_16=s_32>>16;
#else
			s_16=s_32&0xffff;
#endif
			d_16 = *dst;
			fastCombine0565(maskAlpha, s_16, d_16, *dst++);
			w-=2;
		}
		src=(CARD16 *)isrc;
		if(w!=0)
		{
			s_16 = *src;
			d_16 = *dst;
			fastCombine0565(maskAlpha, s_16, d_16, *dst);
		}
	}
}



// macros for "i can't believe it's not fast" packed pixel handling
#define alphamaskCombine24(a,b) genericCombine24(a,b,maskAlpha,maskiAlpha)
static void
fbCompositeTrans_0888xnx0888(pixman_operator_t      op,
			     PicturePtr pSrc,
			     PicturePtr pMask,
			     PicturePtr pDst,
			     INT16      xSrc,
			     INT16      ySrc,
			     INT16      xMask,
			     INT16      yMask,
			     INT16      xDst,
			     INT16      yDst,
			     CARD16     width,
			     CARD16     height)
{
    CARD8	*dstLine, *dst,*idst;
    CARD8	*srcLine, *src;
    FbStride	dstStride, srcStride;
    CARD16	w;
    FbBits	mask;
    CARD16	maskAlpha,maskiAlpha;
    
    fbComposeGetSolid (pMask, mask);
    maskAlpha = mask >> 24;
	maskiAlpha= 255-maskAlpha;
    
    if (!maskAlpha)
	return;
    //if (maskAlpha == 0xff)
    //{
	//fbCompositeSrc_0888x0888 (op, pSrc, pMask, pDst,
	//			  xSrc, ySrc, xMask, yMask, xDst, yDst, 
	//			  width, height);
	//return;
    //}
	
    fbComposeGetStart (pSrc, xSrc, ySrc, CARD8, srcStride, srcLine, 3);
    fbComposeGetStart (pDst, xDst, yDst, CARD8, dstStride, dstLine, 3);

	{
		unsigned int ws,wt,wd,ww;
		CARD32 workingSource;
		CARD32 *wsrc;
		CARD32 rs,gs,bs;
		CARD32 rd,gd,bd;

		CARD32 workingiDest,workingoDest;
		CARD32 *widst,*wodst;


		// are xSrc and xDst at the same alignment?  if not, we need to be complicated :)
		//if(0==0)
		if( (((xSrc*3)&3)!=((xDst*3)&3)) || (srcStride&3)!=0 || (dstStride&3)!=0)
		{
			while (height--)
			{
				idst=dst = dstLine;
				dstLine += dstStride;
				src = srcLine;
				srcLine += srcStride;
				w = width*3;
				
				setupPackedReader(wd,wt,idst,widst,workingiDest);
				ww=(int)dst;
				wt=ww&3;
				dst-=wt; 
				wodst=(CARD32 *)dst; 
				workingoDest=*wodst; 
				ww=4-wt;
#if IMAGE_BYTE_ORDER == LSBFirst
				workingoDest<<=(8*(ww+1));
#else
				workingoDest>>=(8*(ww+1));
#endif

				// get to word aligned
				switch(!(int)src&3)
				{
					case 1:
						readPackedDest(rd);
						rd=alphamaskCombine24(*src++, rd)>>8;
						writePacked(rd);
						w--; if(w==0) break;
					case 2:
						readPackedDest(rd);
						rd=alphamaskCombine24(*src++, rd)>>8;
						writePacked(rd);
						w--; if(w==0) break;
					case 3:
						readPackedDest(rd);
						rd=alphamaskCombine24(*src++, rd)>>8;
						writePacked(rd);
						w--; if(w==0) break;
				}
				wsrc=(CARD32 *)src;
				while (w>3)
				{
					rs=*wsrc++;
					// FIXME: write a version of readPackedDest() which
					// can collect 4 bytes at once if we're on a boundry (which we're
					// actually guarenteed not to be in this version, but do it anyhow), and can
					// collect as 2 16bit words on a 2byte boundry, and then use the 32bit combine here
#if IMAGE_BYTE_ORDER == LSBFirst
					readPackedDest(rd);
					rd=alphamaskCombine24(rs&0xff, rd)>>8;
					writePacked(rd);

					readPackedDest(rd);
					rd=alphamaskCombine24((rs>>8)&0xff, rd)>>8;
					writePacked(rd);
					
					readPackedDest(rd);
					rd=alphamaskCombine24((rs>>16)&0xff, rd)>>8;
					writePacked(rd);
					
					readPackedDest(rd);
					rd=alphamaskCombine24(rs>>24, rd)>>8;
					writePacked(rd);
#else
					readPackedDest(rd);
					rd=alphamaskCombine24(rs>>24, rd)>>8;
					writePacked(rd);
					
					readPackedDest(rd);
					rd=alphamaskCombine24((rs>>16)&0xff, rd)>>8;
					writePacked(rd);
					
					readPackedDest(rd);
					rd=alphamaskCombine24((rs>>8)&0xff, rd)>>8;
					writePacked(rd);

					readPackedDest(rd);
					rd=alphamaskCombine24(rs&0xff, rd)>>8;
					writePacked(rd);
#endif
					w-=4;
				}
				src=(CARD8 *)wsrc;
				switch(w)
				{
					case 3:
						readPackedDest(rd);
						rd=alphamaskCombine24(*src++, rd)>>8;
						writePacked(rd);
					case 2:
						readPackedDest(rd);
						rd=alphamaskCombine24(*src++, rd)>>8;
						writePacked(rd);
					case 1:
						readPackedDest(rd);
						rd=alphamaskCombine24(*src++, rd)>>8;
						writePacked(rd);
				}
				dst=(CARD8 *)wodst;
				switch(ww)
				{
					case 1:
						dst[2]=(workingoDest>>8)&0xff;
					case 2:
						dst[1]=(workingoDest>>16)&0xff;
					case 3:
						dst[0]=workingoDest>>24;
				}
			}
		}
		else
		{
			while (height--)
			{
				idst=dst = dstLine;
				dstLine += dstStride;
				src = srcLine;
				srcLine += srcStride;
				w = width*3;
				// get to word aligned
				switch(!(int)src&3)
				{
					case 1:
						rd=alphamaskCombine24(*src++, *dst)>>8;
						*dst++=rd;
						w--; if(w==0) break;
					case 2:
						rd=alphamaskCombine24(*src++, *dst)>>8;
						*dst++=rd;
						w--; if(w==0) break;
					case 3:
						rd=alphamaskCombine24(*src++, *dst)>>8;
						*dst++=rd;
						w--; if(w==0) break;
				}
				wsrc=(CARD32 *)src;
				widst=(CARD32 *)dst;

				register CARD32 t1, t2, t3, t4;
				while(w>3)
				{
					rs = *wsrc++;
					rd = *widst;
					fastcombine32(maskAlpha, rs, rd, widst, t1, t2, t3, t4);
					w-=4;
				}
				src=(CARD8 *)wsrc;
				dst=(CARD8 *)widst;
				switch(w)
				{
					case 3:
						rd=alphamaskCombine24(*src++, *dst)>>8;
						*dst++=rd;
					case 2:
						rd=alphamaskCombine24(*src++, *dst)>>8;
						*dst++=rd;
					case 1:
						rd=alphamaskCombine24(*src++, *dst)>>8;
						*dst++=rd;
				}
			}
		}
	}
}

/*
 * Simple bitblt
 */

static void
fbCompositeSrcSrc_nxn  (pixman_operator_t	op,
			PicturePtr pSrc,
			PicturePtr pMask,
			PicturePtr pDst,
			INT16      xSrc,
			INT16      ySrc,
			INT16      xMask,
			INT16      yMask,
			INT16      xDst,
			INT16      yDst,
			CARD16     width,
			CARD16     height)
{
    FbBits	*dst;
    FbBits	*src;
    FbStride	dstStride, srcStride;
    int		srcXoff, srcYoff;
    int		dstXoff, dstYoff;
    int		srcBpp;
    int		dstBpp;
    // these need to be signed now!
    int 	iwidth=width;
    int 	iheight=height;
    Bool	reverse = FALSE;
    Bool	upsidedown = FALSE;
	int initialWidth=width;
	int initialX=xDst;

	// FIXME: this is possibly the worst piece of code I've ever written.
	// My main objection to it, is that it is incrfedibly slow in a few cases, due to the
	// call-per-repeat structure of it - the *correct* solution is to implement
	// repeat into fbBlt(), but that's a nontrivial job, and it's far more 
	// important to get the "requireRepeat" stuff implented functionally
	// first, *then* make it fast.
	//  -- jj
	Bool srcRepeat=pSrc->repeat;
	CARD32 srcHeight=pSrc->pDrawable->height;
	CARD32 srcWidth=pSrc->pDrawable->width;

	FbGetPixels(pSrc->pixels,src,srcStride,srcBpp,srcXoff,srcYoff);
	FbGetPixels(pDst->pixels,dst,dstStride,dstBpp,dstXoff,dstYoff);

	if(srcRepeat)
	{
		xSrc%=srcWidth;
		ySrc%=srcHeight;
	}
	
	while(iheight>0)
	{
		int wheight=iheight;
		if(wheight>(srcHeight-ySrc))
			wheight=(srcHeight-ySrc);
		iwidth=initialWidth;
		xDst=initialX;
		while(iwidth>0)
		{
			int wwidth=iwidth;
			if(wwidth>(srcWidth-xSrc))
				wwidth=(srcWidth-xSrc);

			fbBlt (src + (ySrc + srcYoff) * srcStride,
					srcStride,
					(xSrc + srcXoff) * srcBpp,

					dst + (yDst + dstYoff) * dstStride,
					dstStride,
					(xDst + dstXoff) * dstBpp,

					(wwidth) * dstBpp,
					(wheight),

					GXcopy,
					FB_ALLONES,
					dstBpp,

					reverse,
					upsidedown);
			if(!srcRepeat)
				iwidth=0;
			else
			{
				xDst+=wwidth;
				iwidth-=wwidth;
			}
		}
		if(!srcRepeat)
			iheight=0;
		else
		{
			yDst+=wheight;
			iheight-=wheight;
		}
	}
}

/*
 * Solid fill
void
fbCompositeSolidSrc_nxn  (CARD8	op,
			  PicturePtr pSrc,
			  PicturePtr pMask,
			  PicturePtr pDst,
			  INT16      xSrc,
			  INT16      ySrc,
			  INT16      xMask,
			  INT16      yMask,
			  INT16      xDst,
			  INT16      yDst,
			  CARD16     width,
			  CARD16     height)
{
    
}
 */

void
pixman_composite (pixman_operator_t	op,
	     PicturePtr pSrc,
	     PicturePtr pMask,
	     PicturePtr pDst,
	     int	xSrc,
	     int	ySrc,
	     int	xMask,
	     int	yMask,
	     int	xDst,
	     int	yDst,
	     int	width,
	     int	height)
{
    pixman_region16_t	    *region;
    int		    n;
    pixman_box16_t    *pbox;
    CompositeFunc   func;
    int	    srcRepeat = pSrc->repeat;
    int	    maskRepeat = FALSE;
    int	    srcAlphaMap = pSrc->alphaMap != 0;
    int	    maskAlphaMap = FALSE;
    int	    dstAlphaMap = pDst->alphaMap != 0;
    int		    x_msk, y_msk, x_src, y_src, x_dst, y_dst;
    int		    w, h, w_this, h_this;

    if (pSrc->pixels->width == 0 ||
	pSrc->pixels->height == 0)
    {
	return;
    }
    
    xDst += pDst->pixels->x;
    yDst += pDst->pixels->y;
    xSrc += pSrc->pixels->x;
    ySrc += pSrc->pixels->y;
    if (pMask)
    {
	xMask += pMask->pixels->x;
	yMask += pMask->pixels->y;
	maskRepeat = pMask->repeat;
	maskAlphaMap = pMask->alphaMap != 0;
    }

    region = pixman_region_create();
    pixman_region_union_rect (region, region, xDst, yDst, width, height);
    
    if (!FbComputeCompositeRegion (region,
				   pSrc,
				   pMask,
				   pDst,
				   xSrc,
				   ySrc,
				   xMask,
				   yMask,
				   xDst,
				   yDst,
				   width,
				   height))
	return;
				   
    func = pixman_compositeGeneral;
    if (!pSrc->transform && !(pMask && pMask->transform))
    if (!maskAlphaMap && !srcAlphaMap && !dstAlphaMap)
    switch (op) {
    case PIXMAN_OPERATOR_OVER:
	if (pMask)
	{
	    if (srcRepeat && 
		pSrc->pixels->width == 1 &&
		pSrc->pixels->height == 1)
	    {
		if (PICT_FORMAT_COLOR(pSrc->format_code)) {
		    switch (pMask->format_code) {
		    case PICT_a8:
			switch (pDst->format_code) {
			case PICT_r5g6b5:
			case PICT_b5g6r5:
			    func = fbCompositeSolidMask_nx8x0565;
			    break;
			case PICT_r8g8b8:
			case PICT_b8g8r8:
			    func = fbCompositeSolidMask_nx8x0888;
			    break;
			case PICT_a8r8g8b8:
			case PICT_x8r8g8b8:
			case PICT_a8b8g8r8:
			case PICT_x8b8g8r8:
			    func = fbCompositeSolidMask_nx8x8888;
			    break;
			}
			break;
		    case PICT_a8r8g8b8:
			if (pMask->componentAlpha) {
			    switch (pDst->format_code) {
			    case PICT_a8r8g8b8:
			    case PICT_x8r8g8b8:
				func = fbCompositeSolidMask_nx8888x8888C;
				break;
			    case PICT_r5g6b5:
				func = fbCompositeSolidMask_nx8888x0565C;
				break;
			    }
			}
			break;
		    case PICT_a8b8g8r8:
			if (pMask->componentAlpha) {
			    switch (pDst->format_code) {
			    case PICT_a8b8g8r8:
			    case PICT_x8b8g8r8:
				func = fbCompositeSolidMask_nx8888x8888C;
				break;
			    case PICT_b5g6r5:
				func = fbCompositeSolidMask_nx8888x0565C;
				break;
			    }
			}
			break;
		    case PICT_a1:
			switch (pDst->format_code) {
			case PICT_r5g6b5:
			case PICT_b5g6r5:
			case PICT_r8g8b8:
			case PICT_b8g8r8:
			case PICT_a8r8g8b8:
			case PICT_x8r8g8b8:
			case PICT_a8b8g8r8:
			case PICT_x8b8g8r8:
			    func = fbCompositeSolidMask_nx1xn;
			    break;
			}
		    }
		}
		if (func != pixman_compositeGeneral)
		    srcRepeat = FALSE;
	    }
	    else if (maskRepeat &&
		     pMask->pDrawable->width == 1 &&
		     pMask->pDrawable->height == 1)
	    {
		switch (pSrc->format_code) {
		case PICT_r5g6b5:
		case PICT_b5g6r5:
		    if (pDst->format_code == pSrc->format_code)
		        func = fbCompositeTrans_0565xnx0565;
		    break;
		case PICT_r8g8b8:
		case PICT_b8g8r8:
		    if (pDst->format_code == pSrc->format_code)
		        func = fbCompositeTrans_0888xnx0888;
		    break;
		}

		if (func != pixman_compositeGeneral)
		    maskRepeat = FALSE;
	    }
	}
	else
	{
	    switch (pSrc->format_code) {
	    case PICT_a8r8g8b8:
		switch (pDst->format_code) {
		case PICT_a8r8g8b8:
		case PICT_x8r8g8b8:
		    func = fbCompositeSrc_8888x8888;
		    break;
		case PICT_r8g8b8:
		    func = fbCompositeSrc_8888x0888;
		    break;
		case PICT_r5g6b5:
		    func = fbCompositeSrc_8888x0565;
		    break;
		}
		break;
	    case PICT_a8b8g8r8:
		switch (pDst->format_code) {
		case PICT_a8b8g8r8:
		case PICT_x8b8g8r8:
		    func = fbCompositeSrc_8888x8888;
		    break;
		case PICT_b8g8r8:
		    func = fbCompositeSrc_8888x0888;
		    break;
		case PICT_b5g6r5:
		    func = fbCompositeSrc_8888x0565;
		    break;
		}
		break;
	    case PICT_r5g6b5:
		switch (pDst->format_code) {
		case PICT_r5g6b5:
		    func = fbCompositeSrc_0565x0565;
		    break;
		}
		break;
	    case PICT_b5g6r5:
		switch (pDst->format_code) {
		case PICT_b5g6r5:
		    func = fbCompositeSrc_0565x0565;
		    break;
		}
		break;
	    }
	}
	break;
    case PIXMAN_OPERATOR_ADD:
	if (pMask == 0)
	{
	    switch (pSrc->format_code) {
	    case PICT_a8r8g8b8:
		switch (pDst->format_code) {
		case PICT_a8r8g8b8:
		    func = fbCompositeSrcAdd_8888x8888;
		    break;
		}
		break;
	    case PICT_a8b8g8r8:
		switch (pDst->format_code) {
		case PICT_a8b8g8r8:
		    func = fbCompositeSrcAdd_8888x8888;
		    break;
		}
		break;
	    case PICT_a8:
		switch (pDst->format_code) {
		case PICT_a8:
		    func = fbCompositeSrcAdd_8000x8000;
		    break;
		}
		break;
	    case PICT_a1:
		switch (pDst->format_code) {
		case PICT_a1:
		    func = fbCompositeSrcAdd_1000x1000;
		    break;
		}
		break;
	    }
	}
	break;
    case PIXMAN_OPERATOR_SRC:
	if (pMask == 0)
	{
	    if (pSrc->format_code == pDst->format_code)
		func = fbCompositeSrcSrc_nxn;
	}
    default:
	func = pixman_compositeGeneral;
	break;
    }
    /* if we are transforming, we handle repeats in
     * FbFetch[a]_transform
     */
    if (pSrc->transform)
      srcRepeat = 0;
    if (pMask && pMask->transform)
      maskRepeat = 0;
    
    n = pixman_region_num_rects (region);
    pbox = pixman_region_rects (region);
    // FIXME: this is bascially a "white list" of composites that work
    // with repeat until they are all implented.  Once that's done, we
    // remove the checks below entirely
    if(func==fbCompositeSrcSrc_nxn)
    {
	    srcRepeat=maskRepeat=FALSE;
    }
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
		y_msk = MOD (y_msk, pMask->pixels->height);
		if (h_this > pMask->pixels->height - y_msk)
		    h_this = pMask->pixels->height - y_msk;
	    }
	    if (srcRepeat)
	    {
		y_src = MOD (y_src, pSrc->pixels->height);
		if (h_this > pSrc->pixels->height - y_src)
		    h_this = pSrc->pixels->height - y_src;
	    }
	    while (w)
	    {
		w_this = w;
		if (maskRepeat)
		{
		    x_msk = MOD (x_msk, pMask->pixels->width);
		    if (w_this > pMask->pixels->width - x_msk)
			w_this = pMask->pixels->width - x_msk;
		}
		if (srcRepeat)
		{
		    x_src = MOD (x_src, pSrc->pixels->width);
		    if (w_this > pSrc->pixels->width - x_src)
			w_this = pSrc->pixels->width - x_src;
		}
		(*func) (op, pSrc, pMask, pDst,
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
    pixman_region_destroy (region);
}
slim_hidden_def(pixman_composite);
