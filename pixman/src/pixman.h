#ifndef _PIXMAN_H_
#define _PIXMAN_H_


/* pixman.h - a merge of pixregion.h and ic.h */


/* from pixregion.h */


/***********************************************************

Copyright 1987, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.


Copyright 1987 by Digital Equipment Corporation, Maynard, Massachusetts.

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its 
documentation for any purpose and without fee is hereby granted, 
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in 
supporting documentation, and that the name of Digital not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.  

DIGITAL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

******************************************************************/
/* $Id: pixman.h,v 1.6 2003-12-10 00:08:16 dajobe Exp $ */

/* libic.h */

/*
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


#if defined (__SVR4) && defined (__sun)
# include <sys/int_types.h>
#else
# if defined (__OpenBSD__)
#  include <inttypes.h>
# else 
#  include <stdint.h>
# endif
#endif


#if defined(_PIXREGIONINT_H_) || defined(_ICINT_H_)
#include <slim_export.h>
#else
#include <slim_import.h>
#endif

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

/* pixregion.h */

typedef struct _PixRegion PixRegion;

typedef struct _PixRegionBox {
    short x1, y1, x2, y2;
} PixRegionBox;

typedef enum {
    PixRegionStatusFailure,
    PixRegionStatusSuccess
} PixRegionStatus;

/* creation/destruction */

extern PixRegion * __external_linkage
PixRegionCreate (void);

extern PixRegion * __external_linkage
PixRegionCreateSimple (PixRegionBox *extents);

extern void __external_linkage
PixRegionDestroy (PixRegion *region);

/* manipulation */

extern void __external_linkage
PixRegionTranslate (PixRegion *region, int x, int y);

extern PixRegionStatus __external_linkage
PixRegionCopy (PixRegion *dest, PixRegion *source);

extern PixRegionStatus __external_linkage
PixRegionIntersect (PixRegion *newReg, PixRegion *reg1, PixRegion *reg2);

extern PixRegionStatus __external_linkage
PixRegionUnion (PixRegion *newReg, PixRegion *reg1, PixRegion *reg2);

extern PixRegionStatus __external_linkage
PixRegionUnionRect(PixRegion *dest, PixRegion *source,
		   int x, int y, unsigned int width, unsigned int height);

extern PixRegionStatus __external_linkage
PixRegionSubtract (PixRegion *regD, PixRegion *regM, PixRegion *regS);

extern PixRegionStatus __external_linkage
PixRegionInverse (PixRegion *newReg, PixRegion *reg1, PixRegionBox *invRect);

/* XXX: Need to fix this so it doesn't depend on an X data structure
extern PixRegion * __external_linkage
RectsToPixRegion (int nrects, xRectanglePtr prect, int ctype);
*/

/* querying */

/* XXX: These should proably be combined: PixRegionGetRects? */
extern int __external_linkage
PixRegionNumRects (PixRegion *region);

extern PixRegionBox * __external_linkage
PixRegionRects (PixRegion *region);

/* XXX: Change to an enum */
#define rgnOUT 0
#define rgnIN  1
#define rgnPART 2

extern int __external_linkage
PixRegionPointInRegion (PixRegion *region, int x, int y, PixRegionBox *box);

extern int __external_linkage
PixRegionRectIn (PixRegion *PixRegion, PixRegionBox *prect);

extern int __external_linkage
PixRegionNotEmpty (PixRegion *region);

extern PixRegionBox * __external_linkage
PixRegionExtents (PixRegion *region);

/* mucking around */

/* WARNING: calling PixRegionAppend may leave dest as an invalid
   region. Follow-up with PixRegionValidate to fix it up. */
extern PixRegionStatus __external_linkage
PixRegionAppend (PixRegion *dest, PixRegion *region);

extern PixRegionStatus __external_linkage
PixRegionValidate (PixRegion *badreg, int *pOverlap);

/* Unclassified functionality
 * XXX: Do all of these need to be exported?
 */

extern void __external_linkage
PixRegionReset (PixRegion *region, PixRegionBox *pBox);

extern void __external_linkage
PixRegionEmpty (PixRegion *region);


/* ic.h */


/* icformat.c */
typedef enum _IcOperator {
    IcOperatorClear,
    IcOperatorSrc,
    IcOperatorDst,
    IcOperatorOver,
    IcOperatorOverReverse,
    IcOperatorIn,
    IcOperatorInReverse,
    IcOperatorOut,
    IcOperatorOutReverse,
    IcOperatorAtop,
    IcOperatorAtopReverse,
    IcOperatorXor,
    IcOperatorAdd,
    IcOperatorSaturate,
} IcOperator;


typedef enum _IcFormatName {
    IcFormatNameARGB32,
    IcFormatNameRGB24,
    IcFormatNameA8,
    IcFormatNameA1
} IcFormatName;

typedef struct _IcFormat IcFormat;

extern IcFormat * __external_linkage
IcFormatCreate (IcFormatName name);

extern IcFormat * __external_linkage
IcFormatCreateMasks (int bpp,
		     int alpha_mask,
		     int red_mask,
		     int green_mask,
		     int blue_mask);

extern void __external_linkage
IcFormatDestroy (IcFormat *format);

/* icimage.c */

typedef struct _IcImage	IcImage;

extern IcImage * __external_linkage
IcImageCreate (IcFormat	*format,
	       int	width,
	       int	height);

/*
 * This single define controls the basic size of data manipulated
 * by this software; it must be log2(sizeof (IcBits) * 8)
 */

#ifndef IC_SHIFT
#  if defined(__alpha__) || defined(__alpha) || \
      defined(ia64) || defined(__ia64__) || \
      defined(__sparc64__) || \
      defined(__s390x__) || \
      defined(x86_64) || defined (__x86_64__)
#define IC_SHIFT 6
typedef uint64_t IcBits;
#  else
#define IC_SHIFT 5
typedef uint32_t IcBits;
#  endif
#endif

extern IcImage * __external_linkage
IcImageCreateForData (IcBits *data, IcFormat *format, int width, int height, int bpp, int stride);

extern void __external_linkage
IcImageDestroy (IcImage *image);

extern int __external_linkage
IcImageSetClipRegion (IcImage	*image,
		      PixRegion	*region);

typedef int IcFixed16_16;

typedef struct _IcPointFixed {
    IcFixed16_16  x, y;
} IcPointFixed;

typedef struct _IcLineFixed {
    IcPointFixed	p1, p2;
} IcLineFixed;

/* XXX: It's goofy that IcRectangle has integers while all the other
   datatypes have fixed-point values. (Though by design,
   IcFillRectangles is designed to fill only whole pixels) */
typedef struct _IcRectangle {
    short x, y;
    unsigned short width, height;
} IcRectangle;

typedef struct _IcTriangle {
    IcPointFixed	p1, p2, p3;
} IcTriangle;

typedef struct _IcTrapezoid {
    IcFixed16_16  top, bottom;
    IcLineFixed	left, right;
} IcTrapezoid;

typedef struct _IcVector {
    IcFixed16_16    vector[3];
} IcVector;

typedef struct _IcTransform {
    IcFixed16_16  matrix[3][3];
} IcTransform;

typedef enum {
    IcFilterFast,
    IcFilterGood,
    IcFilterBest,
    IcFilterNearest,
    IcFilterBilinear
} IcFilter;

extern int __external_linkage
IcImageSetTransform (IcImage		*image,
		     IcTransform	*transform);

extern void __external_linkage
IcImageSetRepeat (IcImage	*image,
		  int		repeat);

extern void __external_linkage
IcImageSetFilter (IcImage	*image,
		  IcFilter	filter);

extern int __external_linkage
IcImageGetWidth (IcImage	*image);

extern int __external_linkage
IcImageGetHeight (IcImage	*image);

extern int __external_linkage
IcImageGetStride (IcImage	*image);

extern int __external_linkage
IcImageGetDepth (IcImage	*image);

extern IcFormat * __external_linkage
IcImageGetFormat (IcImage	*image);

extern IcBits * __external_linkage
IcImageGetData (IcImage	*image);

/* iccolor.c */

/* XXX: Do we really need a struct here? Only IcRectangles uses this. */
typedef struct {
    unsigned short   red;
    unsigned short   green;
    unsigned short   blue;
    unsigned short   alpha;
} IcColor;

extern void __external_linkage
IcColorToPixel (const IcFormat	*format,
		const IcColor	*color,
		IcBits		*pixel);

extern void __external_linkage
IcPixelToColor (const IcFormat	*format,
		IcBits		pixel,
		IcColor		*color);

/* icrect.c */

extern void __external_linkage
IcFillRectangle (IcOperator	op,
		 IcImage	*dst,
		 const IcColor	*color,
		 int		x,
		 int		y,
		 unsigned int	width,
		 unsigned int	height);

extern void __external_linkage
IcFillRectangles (IcOperator		op,
		  IcImage		*dst,
		  const IcColor		*color,
		  const IcRectangle	*rects,
		  int			nRects);

/* ictrap.c */

/* XXX: Switch to enum for op */
extern void __external_linkage
IcCompositeTrapezoids (IcOperator	op,
		       IcImage		*src,
		       IcImage		*dst,
		       int		xSrc,
		       int		ySrc,
		       const IcTrapezoid *traps,
		       int		ntrap);

/* ictri.c */

extern void __external_linkage
IcCompositeTriangles (IcOperator	op,
		      IcImage		*src,
		      IcImage		*dst,
		      int		xSrc,
		      int		ySrc,
		      const IcTriangle	*tris,
		      int		ntris);

extern void __external_linkage
IcCompositeTriStrip (IcOperator		op,
		     IcImage		*src,
		     IcImage		*dst,
		     int		xSrc,
		     int		ySrc,
		     const IcPointFixed	*points,
		     int		npoints);


extern void __external_linkage
IcCompositeTriFan (IcOperator		op,
		   IcImage		*src,
		   IcImage		*dst,
		   int			xSrc,
		   int			ySrc,
		   const IcPointFixed	*points,
		   int			npoints);

/* ic.c */

extern void __external_linkage
IcComposite (IcOperator	op,
	     IcImage	*iSrc,
	     IcImage    *iMask,
	     IcImage    *iDst,
	     int      	xSrc,
	     int      	ySrc,
	     int      	xMask,
	     int      	yMask,
	     int      	xDst,
	     int      	yDst,
	     int	width,
	     int	height);



#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#undef __external_linkage

#endif /* _PIXMAN_H_ */
