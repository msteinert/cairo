/*
 * $XFree86: $
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

#ifndef _IC_H_
#define _IC_H_

/* XXX: Hack: It would be nice to figure out a cleaner way to
   successfully include both Xlib/server header files without having
   major clashes over the definition of BoxRec and BoxPtr. */
#define EXCLUDE_SERVER_BOXPTR 1

#include "X.h"
#include "servermd.h"

#include "misc.h"
#include "picture.h"
#include "X11/Xprotostr.h"
#include "X11/extensions/Xrender.h"

typedef struct _IcPixels	IcPixels;
typedef struct _IcImage		IcImage;
typedef struct _IcFormat	IcFormat;

/* XXX: Most of this file is straight from fb.h and I imagine we can
   drop quite a bit of it. Once the real ic code starts to come
   together I can probably figure out what is not needed here. */

/*
 * This single define controls the basic size of data manipulated
 * by this software; it must be log2(sizeof (IcBits) * 8)
 */

#ifndef IC_SHIFT
#define IC_SHIFT    LOG2_BITMAP_PAD
#endif

#if IC_SHIFT < LOG2_BITMAP_PAD
    error IC_SHIFT must be >= LOG2_BITMAP_PAD
#endif
    
#define IC_UNIT	    (1 << IC_SHIFT)
#define IC_HALFUNIT (1 << (IC_SHIFT-1))
#define IC_MASK	    (IC_UNIT - 1)
#define IC_ALLONES  ((IcBits) -1)
    
#if GLYPHPADBYTES != 4
#error "GLYPHPADBYTES must be 4"
#endif
#if GETLEFTBITS_ALIGNMENT != 1
#error "GETLEFTBITS_ALIGNMENT must be 1"
#endif
/* whether to bother to include 24bpp support */
#ifndef ICNO24BIT
#define IC_24BIT
#endif

/*
 * Unless otherwise instructed, ic includes code to advertise 24bpp
 * windows with 32bpp image format for application compatibility
 */

#ifdef IC_24BIT
#ifndef ICNO24_32
#define IC_24_32BIT
#endif
#endif

#define IC_STIP_SHIFT	LOG2_BITMAP_PAD
#define IC_STIP_UNIT	(1 << IC_STIP_SHIFT)
#define IC_STIP_MASK	(IC_STIP_UNIT - 1)
#define IC_STIP_ALLONES	((IcStip) -1)
    
#define IC_STIP_ODDSTRIDE(s)	(((s) & (IC_MASK >> IC_STIP_SHIFT)) != 0)
#define IC_STIP_ODDPTR(p)	((((long) (p)) & (IC_MASK >> 3)) != 0)
    
#define IcStipStrideToBitsStride(s) (((s) >> (IC_SHIFT - IC_STIP_SHIFT)))
#define IcBitsStrideToStipStride(s) (((s) << (IC_SHIFT - IC_STIP_SHIFT)))
    
#define IcFullMask(n)   ((n) == IC_UNIT ? IC_ALLONES : ((((IcBits) 1) << n) - 1))
    
#if IC_SHIFT == 6
# ifdef WIN32
typedef unsigned __int64    IcBits;
# else
#  if defined(__alpha__) || defined(__alpha) || \
      defined(ia64) || defined(__ia64__) || \
      defined(__sparc64__) || \
      defined(__s390x__) || \
      defined(x86_64) || defined (__x86_64__)
typedef unsigned long	    IcBits;
#  else
typedef unsigned long long  IcBits;
#  endif
# endif
#endif

#if IC_SHIFT == 5
typedef CARD32		    IcBits;
#endif

#if IC_SHIFT == 4
typedef CARD16		    IcBits;
#endif

#if LOG2_BITMAP_PAD == IC_SHIFT
typedef IcBits		    IcStip;
#else
# if LOG2_BITMAP_PAD == 5
typedef CARD32		    IcStip;
# endif
#endif

typedef int		    IcStride;


#ifdef IC_DEBUG
extern void IcValidateDrawable(DrawablePtr d);
extern void IcInitializeDrawable(DrawablePtr d);
extern void IcSetBits (IcStip *bits, int stride, IcStip data);
#define IC_HEAD_BITS   (IcStip) (0xbaadf00d)
#define IC_TAIL_BITS   (IcStip) (0xbaddf0ad)
#else
#define IcValidateDrawable(d)
#define fdInitializeDrawable(d)
#endif

#if BITMAP_BIT_ORDER == LSBFirst
#define IcScrLeft(x,n)	((x) >> (n))
#define IcScrRight(x,n)	((x) << (n))
/* #define IcLeftBits(x,n)	((x) & ((((IcBits) 1) << (n)) - 1)) */
#define IcLeftStipBits(x,n) ((x) & ((((IcStip) 1) << (n)) - 1))
#define IcStipMoveLsb(x,s,n)	(IcStipRight (x,(s)-(n)))
#define IcPatternOffsetBits	0
#else
#define IcScrLeft(x,n)	((x) << (n))
#define IcScrRight(x,n)	((x) >> (n))
/* #define IcLeftBits(x,n)	((x) >> (IC_UNIT - (n))) */
#define IcLeftStipBits(x,n) ((x) >> (IC_STIP_UNIT - (n)))
#define IcStipMoveLsb(x,s,n)	(x)
#define IcPatternOffsetBits	(sizeof (IcBits) - 1)
#endif

/* XXX: Don't think we need this
 #include "micoord.h"
*/

#define IcStipLeft(x,n)	IcScrLeft(x,n)
#define IcStipRight(x,n) IcScrRight(x,n)

#define IcRotLeft(x,n)	IcScrLeft(x,n) | (n ? IcScrRight(x,IC_UNIT-n) : 0)
#define IcRotRight(x,n)	IcScrRight(x,n) | (n ? IcScrLeft(x,IC_UNIT-n) : 0)

#define IcRotStipLeft(x,n)  IcStipLeft(x,n) | (n ? IcStipRight(x,IC_STIP_UNIT-n) : 0)
#define IcRotStipRight(x,n)  IcStipRight(x,n) | (n ? IcStipLeft(x,IC_STIP_UNIT-n) : 0)

#define IcLeftMask(x)	    ( ((x) & IC_MASK) ? \
			     IcScrRight(IC_ALLONES,(x) & IC_MASK) : 0)
#define IcRightMask(x)	    ( ((IC_UNIT - (x)) & IC_MASK) ? \
			     IcScrLeft(IC_ALLONES,(IC_UNIT - (x)) & IC_MASK) : 0)

#define IcLeftStipMask(x)   ( ((x) & IC_STIP_MASK) ? \
			     IcStipRight(IC_STIP_ALLONES,(x) & IC_STIP_MASK) : 0)
#define IcRightStipMask(x)  ( ((IC_STIP_UNIT - (x)) & IC_STIP_MASK) ? \
			     IcScrLeft(IC_STIP_ALLONES,(IC_STIP_UNIT - (x)) & IC_STIP_MASK) : 0)

#define IcBitsMask(x,w)	(IcScrRight(IC_ALLONES,(x) & IC_MASK) & \
			 IcScrLeft(IC_ALLONES,(IC_UNIT - ((x) + (w))) & IC_MASK))

#define IcStipMask(x,w)	(IcStipRight(IC_STIP_ALLONES,(x) & IC_STIP_MASK) & \
			 IcStipLeft(IC_STIP_ALLONES,(IC_STIP_UNIT - ((x)+(w))) & IC_STIP_MASK))


#define IcMaskBits(x,w,l,n,r) { \
    n = (w); \
    r = IcRightMask((x)+n); \
    l = IcLeftMask(x); \
    if (l) { \
	n -= IC_UNIT - ((x) & IC_MASK); \
	if (n < 0) { \
	    n = 0; \
	    l &= r; \
	    r = 0; \
	} \
    } \
    n >>= IC_SHIFT; \
}

#ifdef ICNOPIXADDR
#define IcMaskBitsBytes(x,w,copy,l,lb,n,r,rb) IcMaskBits(x,w,l,n,r)
#define IcDoLeftMaskByteRRop(dst,lb,l,and,xor) { \
    *dst = IcDoMaskRRop(*dst,and,xor,l); \
}
#define IcDoRightMaskByteRRop(dst,rb,r,and,xor) { \
    *dst = IcDoMaskRRop(*dst,and,xor,r); \
}
#else

#define IcByteMaskInvalid   0x10

#define IcPatternOffset(o,t)  ((o) ^ (IcPatternOffsetBits & ~(sizeof (t) - 1)))

#define IcPtrOffset(p,o,t)		((t *) ((CARD8 *) (p) + (o)))
#define IcSelectPatternPart(xor,o,t)	((xor) >> (IcPatternOffset (o,t) << 3))
#define IcStorePart(dst,off,t,xor)	(*IcPtrOffset(dst,off,t) = \
					 IcSelectPart(xor,off,t))
#ifndef IcSelectPart
#define IcSelectPart(x,o,t) IcSelectPatternPart(x,o,t)
#endif

#define IcMaskBitsBytes(x,w,copy,l,lb,n,r,rb) { \
    n = (w); \
    lb = 0; \
    rb = 0; \
    r = IcRightMask((x)+n); \
    if (r) { \
	/* compute right byte length */ \
	if ((copy) && (((x) + n) & 7) == 0) { \
	    rb = (((x) + n) & IC_MASK) >> 3; \
	} else { \
	    rb = IcByteMaskInvalid; \
	} \
    } \
    l = IcLeftMask(x); \
    if (l) { \
	/* compute left byte length */ \
	if ((copy) && ((x) & 7) == 0) { \
	    lb = ((x) & IC_MASK) >> 3; \
	} else { \
	    lb = IcByteMaskInvalid; \
	} \
	/* subtract out the portion painted by leftMask */ \
	n -= IC_UNIT - ((x) & IC_MASK); \
	if (n < 0) { \
	    if (lb != IcByteMaskInvalid) { \
		if (rb == IcByteMaskInvalid) { \
		    lb = IcByteMaskInvalid; \
		} else if (rb) { \
		    lb |= (rb - lb) << (IC_SHIFT - 3); \
		    rb = 0; \
		} \
	    } \
	    n = 0; \
	    l &= r; \
	    r = 0; \
	}\
    } \
    n >>= IC_SHIFT; \
}

#if IC_SHIFT == 6
#define IcDoLeftMaskByteRRop6Cases(dst,xor) \
    case (sizeof (IcBits) - 7) | (1 << (IC_SHIFT - 3)): \
	IcStorePart(dst,sizeof (IcBits) - 7,CARD8,xor); \
	break; \
    case (sizeof (IcBits) - 7) | (2 << (IC_SHIFT - 3)): \
	IcStorePart(dst,sizeof (IcBits) - 7,CARD8,xor); \
	IcStorePart(dst,sizeof (IcBits) - 6,CARD8,xor); \
	break; \
    case (sizeof (IcBits) - 7) | (3 << (IC_SHIFT - 3)): \
	IcStorePart(dst,sizeof (IcBits) - 7,CARD8,xor); \
	IcStorePart(dst,sizeof (IcBits) - 6,CARD16,xor); \
	break; \
    case (sizeof (IcBits) - 7) | (4 << (IC_SHIFT - 3)): \
	IcStorePart(dst,sizeof (IcBits) - 7,CARD8,xor); \
	IcStorePart(dst,sizeof (IcBits) - 6,CARD16,xor); \
	IcStorePart(dst,sizeof (IcBits) - 4,CARD8,xor); \
	break; \
    case (sizeof (IcBits) - 7) | (5 << (IC_SHIFT - 3)): \
	IcStorePart(dst,sizeof (IcBits) - 7,CARD8,xor); \
	IcStorePart(dst,sizeof (IcBits) - 6,CARD16,xor); \
	IcStorePart(dst,sizeof (IcBits) - 4,CARD16,xor); \
	break; \
    case (sizeof (IcBits) - 7) | (6 << (IC_SHIFT - 3)): \
	IcStorePart(dst,sizeof (IcBits) - 7,CARD8,xor); \
	IcStorePart(dst,sizeof (IcBits) - 6,CARD16,xor); \
	IcStorePart(dst,sizeof (IcBits) - 4,CARD16,xor); \
	IcStorePart(dst,sizeof (IcBits) - 2,CARD8,xor); \
	break; \
    case (sizeof (IcBits) - 7): \
	IcStorePart(dst,sizeof (IcBits) - 7,CARD8,xor); \
	IcStorePart(dst,sizeof (IcBits) - 6,CARD16,xor); \
	IcStorePart(dst,sizeof (IcBits) - 4,CARD32,xor); \
	break; \
    case (sizeof (IcBits) - 6) | (1 << (IC_SHIFT - 3)): \
	IcStorePart(dst,sizeof (IcBits) - 6,CARD8,xor); \
	break; \
    case (sizeof (IcBits) - 6) | (2 << (IC_SHIFT - 3)): \
	IcStorePart(dst,sizeof (IcBits) - 6,CARD16,xor); \
	break; \
    case (sizeof (IcBits) - 6) | (3 << (IC_SHIFT - 3)): \
	IcStorePart(dst,sizeof (IcBits) - 6,CARD16,xor); \
	IcStorePart(dst,sizeof (IcBits) - 4,CARD8,xor); \
	break; \
    case (sizeof (IcBits) - 6) | (4 << (IC_SHIFT - 3)): \
	IcStorePart(dst,sizeof (IcBits) - 6,CARD16,xor); \
	IcStorePart(dst,sizeof (IcBits) - 4,CARD16,xor); \
	break; \
    case (sizeof (IcBits) - 6) | (5 << (IC_SHIFT - 3)): \
	IcStorePart(dst,sizeof (IcBits) - 6,CARD16,xor); \
	IcStorePart(dst,sizeof (IcBits) - 4,CARD16,xor); \
	IcStorePart(dst,sizeof (IcBits) - 2,CARD8,xor); \
	break; \
    case (sizeof (IcBits) - 6): \
	IcStorePart(dst,sizeof (IcBits) - 6,CARD16,xor); \
	IcStorePart(dst,sizeof (IcBits) - 4,CARD32,xor); \
	break; \
    case (sizeof (IcBits) - 5) | (1 << (IC_SHIFT - 3)): \
	IcStorePart(dst,sizeof (IcBits) - 5,CARD8,xor); \
	break; \
    case (sizeof (IcBits) - 5) | (2 << (IC_SHIFT - 3)): \
	IcStorePart(dst,sizeof (IcBits) - 5,CARD8,xor); \
	IcStorePart(dst,sizeof (IcBits) - 4,CARD8,xor); \
	break; \
    case (sizeof (IcBits) - 5) | (3 << (IC_SHIFT - 3)): \
	IcStorePart(dst,sizeof (IcBits) - 5,CARD8,xor); \
	IcStorePart(dst,sizeof (IcBits) - 4,CARD16,xor); \
	break; \
    case (sizeof (IcBits) - 5) | (4 << (IC_SHIFT - 3)): \
	IcStorePart(dst,sizeof (IcBits) - 5,CARD8,xor); \
	IcStorePart(dst,sizeof (IcBits) - 4,CARD16,xor); \
	IcStorePart(dst,sizeof (IcBits) - 2,CARD8,xor); \
	break; \
    case (sizeof (IcBits) - 5): \
	IcStorePart(dst,sizeof (IcBits) - 5,CARD8,xor); \
	IcStorePart(dst,sizeof (IcBits) - 4,CARD32,xor); \
	break; \
    case (sizeof (IcBits) - 4) | (1 << (IC_SHIFT - 3)): \
	IcStorePart(dst,sizeof (IcBits) - 4,CARD8,xor); \
	break; \
    case (sizeof (IcBits) - 4) | (2 << (IC_SHIFT - 3)): \
	IcStorePart(dst,sizeof (IcBits) - 4,CARD16,xor); \
	break; \
    case (sizeof (IcBits) - 4) | (3 << (IC_SHIFT - 3)): \
	IcStorePart(dst,sizeof (IcBits) - 4,CARD16,xor); \
	IcStorePart(dst,sizeof (IcBits) - 2,CARD8,xor); \
	break; \
    case (sizeof (IcBits) - 4): \
	IcStorePart(dst,sizeof (IcBits) - 4,CARD32,xor); \
	break;

#define IcDoRightMaskByteRRop6Cases(dst,xor) \
    case 4: \
	IcStorePart(dst,0,CARD32,xor); \
	break; \
    case 5: \
	IcStorePart(dst,0,CARD32,xor); \
	IcStorePart(dst,4,CARD8,xor); \
	break; \
    case 6: \
	IcStorePart(dst,0,CARD32,xor); \
	IcStorePart(dst,4,CARD16,xor); \
	break; \
    case 7: \
	IcStorePart(dst,0,CARD32,xor); \
	IcStorePart(dst,4,CARD16,xor); \
	IcStorePart(dst,6,CARD8,xor); \
	break;
#else
#define IcDoLeftMaskByteRRop6Cases(dst,xor)
#define IcDoRightMaskByteRRop6Cases(dst,xor)
#endif

#define IcDoLeftMaskByteRRop(dst,lb,l,and,xor) { \
    switch (lb) { \
    IcDoLeftMaskByteRRop6Cases(dst,xor) \
    case (sizeof (IcBits) - 3) | (1 << (IC_SHIFT - 3)): \
	IcStorePart(dst,sizeof (IcBits) - 3,CARD8,xor); \
	break; \
    case (sizeof (IcBits) - 3) | (2 << (IC_SHIFT - 3)): \
	IcStorePart(dst,sizeof (IcBits) - 3,CARD8,xor); \
	IcStorePart(dst,sizeof (IcBits) - 2,CARD8,xor); \
	break; \
    case (sizeof (IcBits) - 2) | (1 << (IC_SHIFT - 3)): \
	IcStorePart(dst,sizeof (IcBits) - 2,CARD8,xor); \
	break; \
    case sizeof (IcBits) - 3: \
	IcStorePart(dst,sizeof (IcBits) - 3,CARD8,xor); \
    case sizeof (IcBits) - 2: \
	IcStorePart(dst,sizeof (IcBits) - 2,CARD16,xor); \
	break; \
    case sizeof (IcBits) - 1: \
	IcStorePart(dst,sizeof (IcBits) - 1,CARD8,xor); \
	break; \
    default: \
	*dst = IcDoMaskRRop(*dst, and, xor, l); \
	break; \
    } \
}


#define IcDoRightMaskByteRRop(dst,rb,r,and,xor) { \
    switch (rb) { \
    case 1: \
	IcStorePart(dst,0,CARD8,xor); \
	break; \
    case 2: \
	IcStorePart(dst,0,CARD16,xor); \
	break; \
    case 3: \
	IcStorePart(dst,0,CARD16,xor); \
	IcStorePart(dst,2,CARD8,xor); \
	break; \
    IcDoRightMaskByteRRop6Cases(dst,xor) \
    default: \
	*dst = IcDoMaskRRop (*dst, and, xor, r); \
    } \
}
#endif

#define IcMaskStip(x,w,l,n,r) { \
    n = (w); \
    r = IcRightStipMask((x)+n); \
    l = IcLeftStipMask(x); \
    if (l) { \
	n -= IC_STIP_UNIT - ((x) & IC_STIP_MASK); \
	if (n < 0) { \
	    n = 0; \
	    l &= r; \
	    r = 0; \
	} \
    } \
    n >>= IC_STIP_SHIFT; \
}

/*
 * These macros are used to transparently stipple
 * in copy mode; the expected usage is with 'n' constant
 * so all of the conditional parts collapse into a minimal
 * sequence of partial word writes
 *
 * 'n' is the bytemask of which bytes to store, 'a' is the address
 * of the IcBits base unit, 'o' is the offset within that unit
 *
 * The term "lane" comes from the hardware term "byte-lane" which
 */

#define IcLaneCase1(n,a,o)  ((n) == 0x01 ? \
			     (*(CARD8 *) ((a)+IcPatternOffset(o,CARD8)) = \
			      fgxor) : 0)
#define IcLaneCase2(n,a,o)  ((n) == 0x03 ? \
			     (*(CARD16 *) ((a)+IcPatternOffset(o,CARD16)) = \
			      fgxor) : \
			     ((void)IcLaneCase1((n)&1,a,o), \
				    IcLaneCase1((n)>>1,a,(o)+1)))
#define IcLaneCase4(n,a,o)  ((n) == 0x0f ? \
			     (*(CARD32 *) ((a)+IcPatternOffset(o,CARD32)) = \
			      fgxor) : \
			     ((void)IcLaneCase2((n)&3,a,o), \
				    IcLaneCase2((n)>>2,a,(o)+2)))
#define IcLaneCase8(n,a,o)  ((n) == 0x0ff ? (*(IcBits *) ((a)+(o)) = fgxor) : \
			     ((void)IcLaneCase4((n)&15,a,o), \
				    IcLaneCase4((n)>>4,a,(o)+4)))

#if IC_SHIFT == 6
#define IcLaneCase(n,a)   IcLaneCase8(n,(CARD8 *) (a),0)
#endif

#if IC_SHIFT == 5
#define IcLaneCase(n,a)   IcLaneCase4(n,(CARD8 *) (a),0)
#endif

/* Rotate a filled pixel value to the specified alignement */
#define IcRot24(p,b)	    (IcScrRight(p,b) | IcScrLeft(p,24-(b)))
#define IcRot24Stip(p,b)    (IcStipRight(p,b) | IcStipLeft(p,24-(b)))

/* step a filled pixel value to the next/previous IC_UNIT alignment */
#define IcNext24Pix(p)	(IcRot24(p,(24-IC_UNIT%24)))
#define IcPrev24Pix(p)	(IcRot24(p,IC_UNIT%24))
#define IcNext24Stip(p)	(IcRot24(p,(24-IC_STIP_UNIT%24)))
#define IcPrev24Stip(p)	(IcRot24(p,IC_STIP_UNIT%24))

/* step a rotation value to the next/previous rotation value */
#if IC_UNIT == 64
#define IcNext24Rot(r)        ((r) == 16 ? 0 : (r) + 8)
#define IcPrev24Rot(r)        ((r) == 0 ? 16 : (r) - 8)

#if IMAGE_BYTE_ORDER == MSBFirst
#define IcFirst24Rot(x)		(((x) + 8) % 24)
#else
#define IcFirst24Rot(x)		((x) % 24)
#endif

#endif

#if IC_UNIT == 32
#define IcNext24Rot(r)        ((r) == 0 ? 16 : (r) - 8)
#define IcPrev24Rot(r)        ((r) == 16 ? 0 : (r) + 8)

#if IMAGE_BYTE_ORDER == MSBFirst
#define IcFirst24Rot(x)		(((x) + 16) % 24)
#else
#define IcFirst24Rot(x)		((x) % 24)
#endif
#endif

#define IcNext24RotStip(r)        ((r) == 0 ? 16 : (r) - 8)
#define IcPrev24RotStip(r)        ((r) == 16 ? 0 : (r) + 8)

/* Whether 24-bit specific code is needed for this filled pixel value */
#define IcCheck24Pix(p)	((p) == IcNext24Pix(p))

/* Macros for dealing with dashing */

#define IcDashDeclare	\
    unsigned char	*__dash, *__firstDash, *__lastDash
    
#define IcDashInit(pGC,pPriv,dashOffset,dashlen,even) {	    \
    (even) = TRUE;					    \
    __firstDash = (pGC)->dash;				    \
    __lastDash = __firstDash + (pGC)->numInDashList;	    \
    (dashOffset) %= (pPriv)->dashLength;		    \
							    \
    __dash = __firstDash;				    \
    while ((dashOffset) >= ((dashlen) = *__dash))	    \
    {							    \
	(dashOffset) -= (dashlen);			    \
	(even) = 1-(even);				    \
	if (++__dash == __lastDash)			    \
	    __dash = __firstDash;			    \
    }							    \
    (dashlen) -= (dashOffset);				    \
}

#define IcDashNext(dashlen) {				    \
    if (++__dash == __lastDash)				    \
	__dash = __firstDash;				    \
    (dashlen) = *__dash;				    \
}

/* as numInDashList is always even, this case can skip a test */

#define IcDashNextEven(dashlen) {			    \
    (dashlen) = *++__dash;				    \
}

#define IcDashNextOdd(dashlen)	IcDashNext(dashlen)

#define IcDashStep(dashlen,even) {			    \
    if (!--(dashlen)) {					    \
	IcDashNext(dashlen);				    \
	(even) = 1-(even);				    \
    }							    \
}

extern int	IcGCPrivateIndex;
#ifndef IC_NO_WINDOW_PIXMAPS
extern int	IcWinPrivateIndex;
#endif

#ifdef TEKX11
#define IC_OLD_GC
#define IC_OLD_SCREEN
#endif

#ifdef IC_OLD_SCREEN
extern WindowPtr    *WindowTable;
#endif

#ifdef IC_24_32BIT
#define IC_SCREEN_PRIVATE
#endif

#ifdef IC_SCREEN_PRIVATE
extern int	IcScreenPrivateIndex;

/* private field of a screen */
typedef struct {
    unsigned char	win32bpp;	/* window bpp for 32-bpp images */
    unsigned char	pix32bpp;	/* pixmap bpp for 32-bpp images */
} IcScreenPrivRec, *IcScreenPrivPtr;

#define IcGetScreenPrivate(pScreen) ((icScreenPrivPtr) \
				     (pScreen)->devPrivates[icScreenPrivateIndex].ptr)
#endif

/* private field of GC */
typedef struct {
#ifdef IC_OLD_GC
    unsigned char       pad1;
    unsigned char       pad2;
    unsigned char       pad3;
    unsigned		fExpose:1;
    unsigned		freeCompClip:1;
    PixmapPtr		pRotatedPixmap;
    RegionPtr		pCompositeClip;
#endif    
    IcBits		and, xor;	/* reduced rop values */
    IcBits		bgand, bgxor;	/* for stipples */
    IcBits		fg, bg, pm;	/* expanded and filled */
    unsigned int	dashLength;	/* total of all dash elements */
    unsigned char    	oneRect;	/* clip list is single rectangle */
    unsigned char    	evenStipple;	/* stipple is even */
    unsigned char    	bpp;		/* current drawable bpp */
} IcGCPrivRec, *IcGCPrivPtr;

#define IcGetPixels(icpixels, pointer, _stride_, _bpp_, xoff, yoff) { \
    (pointer) = icpixels->data; \
    (_stride_) = icpixels->stride / sizeof(IcBits); \
    (_bpp_) = icpixels->bpp; \
    (xoff) = icpixels->x; /* XXX: fb.h had this ifdef'd to constant 0. Why? */ \
    (yoff) = icpixels->y; /* XXX: fb.h had this ifdef'd to constant 0. Why? */ \
}

#define IcGetStipPixels(icpixels, pointer, _stride_, _bpp_, xoff, yoff) { \
    (pointer) = (IcStip *) icpixels->data; \
    (_stride_) = icpixels->stride; \
    (_bpp_) = icpixels->bpp; \
    (xoff) = icpixels->x; \
    (yoff) = icpixels->y; \
}

/*
 * XFree86 empties the root BorderClip when the VT is inactive,
 * here's a macro which uses that to disable GetImage and GetSpans
 */

#define IcWindowEnabled(pWin) \
    REGION_NOTEMPTY((pWin)->drawable.pScreen, \
		    &WindowTable[(pWin)->drawable.pScreen->myNum]->borderClip)

#define IcDrawableEnabled(pDrawable) \
    ((pDrawable)->type == DRAWABLE_PIXMAP ? \
     TRUE : IcWindowEnabled((WindowPtr) pDrawable))

#ifdef IC_OLD_SCREEN
#define BitsPerPixel(d) (\
    ((1 << PixmapWidthPaddingInfo[d].padBytesLog2) * 8 / \
    (PixmapWidthPaddingInfo[d].padRoundUp+1)))
#endif

#define IcPowerOfTwo(w)	    (((w) & ((w) - 1)) == 0)
/*
 * Accelerated tiles are power of 2 width <= IC_UNIT
 */
#define IcEvenTile(w)	    ((w) <= IC_UNIT && IcPowerOfTwo(w))
/*
 * Accelerated stipples are power of 2 width and <= IC_UNIT/dstBpp
 * with dstBpp a power of 2 as well
 */
#define IcEvenStip(w,bpp)   ((w) * (bpp) <= IC_UNIT && IcPowerOfTwo(w) && IcPowerOfTwo(bpp))

/*
 * icblt.c
 */
void
IcBlt (IcBits   *src, 
       IcStride	srcStride,
       int	srcX,
       
       IcBits   *dst,
       IcStride dstStride,
       int	dstX,
       
       int	width, 
       int	height,
       
       int	alu,
       IcBits	pm,
       int	bpp,
       
       Bool	reverse,
       Bool	upsidedown);

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
	 Bool	    upsidedown);
    
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
	   int	    bpp);
    
/*
 * icbltone.c
 */
void
IcBltOne (IcStip   *src,
	  IcStride srcStride,
	  int	   srcX,
	  IcBits   *dst,
	  IcStride dstStride,
	  int	   dstX,
	  int	   dstBpp,

	  int	   width,
	  int	   height,

	  IcBits   fgand,
	  IcBits   icxor,
	  IcBits   bgand,
	  IcBits   bgxor);
 
#ifdef IC_24BIT
void
IcBltOne24 (IcStip    *src,
	  IcStride  srcStride,	    /* IcStip units per scanline */
	  int	    srcX,	    /* bit position of source */
	  IcBits    *dst,
	  IcStride  dstStride,	    /* IcBits units per scanline */
	  int	    dstX,	    /* bit position of dest */
	  int	    dstBpp,	    /* bits per destination unit */

	  int	    width,	    /* width in bits of destination */
	  int	    height,	    /* height in scanlines */

	  IcBits    fgand,	    /* rrop values */
	  IcBits    fgxor,
	  IcBits    bgand,
	  IcBits    bgxor);
#endif

/* XXX: Do we need this?
void
IcBltPlane (IcBits	    *src,
	    IcStride	    srcStride,
	    int		    srcX,
	    int		    srcBpp,

	    IcStip	    *dst,
	    IcStride	    dstStride,
	    int		    dstX,
	    
	    int		    width,
	    int		    height,
	    
	    IcStip	    fgand,
	    IcStip	    fgxor,
	    IcStip	    bgand,
	    IcStip	    bgxor,
	    Pixel	    planeMask);
*/

/*
 * icpict.c
 */

/* XXX: Name clash...
Bool
IcImageInit (ScreenPtr pScreen,
	     PictFormatPtr formats,
	     int nformats);
*/

/*
 * icsolid.c
 */

void
IcSolid (IcBits	    *dst,
	 IcStride   dstStride,
	 int	    dstX,
	 int	    bpp,

	 int	    width,
	 int	    height,

	 IcBits	    and,
	 IcBits	    xor);

#ifdef IC_24BIT
void
IcSolid24 (IcBits   *dst,
	   IcStride dstStride,
	   int	    dstX,

	   int	    width,
	   int	    height,

	   IcBits   and,
	   IcBits   xor);
#endif

/*
 * icstipple.c
 */

void
IcTransparentSpan (IcBits   *dst,
		   IcBits   stip,
		   IcBits   fgxor,
		   int	    n);

void
IcEvenStipple (IcBits   *dst,
	       IcStride dstStride,
	       int	dstX,
	       int	dstBpp,

	       int	width,
	       int	height,

	       IcStip   *stip,
	       IcStride	stipStride,
	       int	stipHeight,

	       IcBits   fgand,
	       IcBits   fgxor,
	       IcBits   bgand,
	       IcBits   bgxor,

	       int	xRot,
	       int	yRot);

void
IcOddStipple (IcBits	*dst,
	      IcStride	dstStride,
	      int	dstX,
	      int	dstBpp,

	      int	width,
	      int	height,

	      IcStip	*stip,
	      IcStride	stipStride,
	      int	stipWidth,
	      int	stipHeight,

	      IcBits	fgand,
	      IcBits	fgxor,
	      IcBits	bgand,
	      IcBits	bgxor,

	      int	xRot,
	      int	yRot);

void
IcStipple (IcBits   *dst,
	   IcStride dstStride,
	   int	    dstX,
	   int	    dstBpp,

	   int	    width,
	   int	    height,

	   IcStip   *stip,
	   IcStride stipStride,
	   int	    stipWidth,
	   int	    stipHeight,
	   Bool	    even,

	   IcBits   fgand,
	   IcBits   fgxor,
	   IcBits   bgand,
	   IcBits   bgxor,

	   int	    xRot,
	   int	    yRot);

/*
 * ictile.c
 */

/* XXX: I screwed this up somehow in the fb->Ic rename
void
IcEvenTile (IcBits	*dst,
	    IcStride	dstStride,
	    int		dstX,

	    int		width,
	    int		height,

	    IcBits	*tile,
	    int		tileHeight,

	    int		alu,
	    IcBits	pm,
	    int		xRot,
	    int		yRot);
*/

void
IcOddTile (IcBits	*dst,
	   IcStride	dstStride,
	   int		dstX,

	   int		width,
	   int		height,

	   IcBits	*tile,
	   IcStride	tileStride,
	   int		tileWidth,
	   int		tileHeight,

	   int		alu,
	   IcBits	pm,
	   int		bpp,
	   
	   int		xRot,
	   int		yRot);

void
IcTile (IcBits	    *dst,
	IcStride    dstStride,
	int	    dstX,

	int	    width,
	int	    height,

	IcBits	    *tile,
	IcStride    tileStride,
	int	    tileWidth,
	int	    tileHeight,
	
	int	    alu,
	IcBits	    pm,
	int	    bpp,
	
	int	    xRot,
	int	    yRot);

IcImage *
IcImageCreate (IcFormat		*format,
	       unsigned short	width,
	       unsigned short	height,
	       Mask		vmask,
	       XID		*vlist,
	       int		*error,
	       int		*error_value);

IcImage *
IcImageCreateForPixels (IcPixels	*pixels,
			IcFormat	*format,
			Mask		vmask,
			XID		*vlist,
			int		*error,
			int		*error_value);

/* icformat.c */
/* XXX: Is this datatype correct? */
typedef CARD32			IcFormatName;

/* XXX: Not sure if this is at all the API we want for IcFormat */
IcFormat *
IcFormatCreate (IcFormatName name);

void
IcFormatDestroy (IcFormat *format);

/* icpixels.c */

IcPixels *
IcPixelsCreate (unsigned int width, unsigned int height, unsigned int depth);

IcPixels *
IcPixelsCreateForData (IcBits *data, int width, int height, int depth, int bpp, int stride);

void
IcPixelsDestroy (IcPixels *pixels);

/* icimage.c */

void
IcComposite (CARD8      op,
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
	     CARD16     height);

/* ictrap.c */

void
IcTrapezoids (CARD8	 op,
	      IcImage	 *src,
	      IcImage	 *dst,
	      IcFormat	 *format,
	      INT16	 xSrc,
	      INT16	 ySrc,
	      int	 ntrap,
	      XTrapezoid *traps);

#endif /* _IC_H_ */
