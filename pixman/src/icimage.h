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

#ifndef _ICIMAGE_H_
#define _ICIMAGE_H_

#include "ic.h"

/* XXX: This is a hack since I don't want to include the server's
   miscstruct with the BoxRec that conflict's with Xlib's. */
#ifndef MISCSTRUCT_H
#define MISCSTRUCT_H 1

#include "misc.h"
#include "X11/Xprotostr.h"

typedef xPoint DDXPointRec;
typedef union _DevUnion {
    pointer		ptr;
    long		val;
    unsigned long	uval;
    pointer		(*fptr)(
#if NeedFunctionPrototypes
                        void
#endif
                        );
} DevUnion;
#endif

#include "glyphstr.h"
/* #include "scrnintstr.h" */
#include "resource.h"

#include "Xutil.h"

#define IcIntMult(a,b,t) ( (t) = (a) * (b) + 0x80, ( ( ( (t)>>8 ) + (t) )>>8 ) )
#define IcIntDiv(a,b)	 (((CARD16) (a) * 255) / (b))

#define IcGet8(v,i)   ((CARD16) (CARD8) ((v) >> i))

/*
 * There are two ways of handling alpha -- either as a single unified value or
 * a separate value for each component, hence each macro must have two
 * versions.  The unified alpha version has a 'U' at the end of the name,
 * the component version has a 'C'.  Similarly, functions which deal with
 * this difference will have two versions using the same convention.
 */

#define IcOverU(x,y,i,a,t) ((t) = IcIntMult(IcGet8(y,i),(a),(t)) + IcGet8(x,i),\
			   (CARD32) ((CARD8) ((t) | (0 - ((t) >> 8)))) << (i))

#define IcOverC(x,y,i,a,t) ((t) = IcIntMult(IcGet8(y,i),IcGet8(a,i),(t)) + IcGet8(x,i),\
			    (CARD32) ((CARD8) ((t) | (0 - ((t) >> 8)))) << (i))

#define IcInU(x,i,a,t) ((CARD32) IcIntMult(IcGet8(x,i),(a),(t)) << (i))

#define IcInC(x,i,a,t) ((CARD32) IcIntMult(IcGet8(x,i),IcGet8(a,i),(t)) << (i))

#define IcGen(x,y,i,ax,ay,t,u,v) ((t) = (IcIntMult(IcGet8(y,i),ay,(u)) + \
					 IcIntMult(IcGet8(x,i),ax,(v))),\
				  (CARD32) ((CARD8) ((t) | \
						     (0 - ((t) >> 8)))) << (i))

#define IcAdd(x,y,i,t)	((t) = IcGet8(x,i) + IcGet8(y,i), \
			 (CARD32) ((CARD8) ((t) | (0 - ((t) >> 8)))) << (i))

/* XXX: I'm not sure about several things here:

   Do we want data to be IcBits * or unsigned char *?

   I don't think I'm currently initializing depth or bpp anywhere...
*/
typedef struct _DirectFormat {
    CARD16	    red, redMask;
    CARD16	    green, greenMask;
    CARD16	    blue, blueMask;
    CARD16	    alpha, alphaMask;
} DirectFormatRec;

typedef struct _IndexFormat {
    VisualPtr	    pVisual;
    ColormapPtr	    pColormap;
    int		    nvalues;
    xIndexValue	    *pValues;
    void	    *devPrivate;
} IndexFormatRec;

typedef struct _IcFormat {
    CARD32	    id;
    CARD32	    format;	    /* except bpp */
    unsigned char   type;
    unsigned char   depth;
    DirectFormatRec direct;
    IndexFormatRec  index;
} IcFormatRec;

typedef struct _IcTransform {
    xFixed	    matrix[3][3];
} IcTransform, *IcTransformPtr;

struct _IcImage {
    IcPixels	    *pixels;
    IcFormat	    *image_format;
    CARD32	    format;
    int		    refcnt;
    
    unsigned int    repeat : 1;
    unsigned int    graphicsExposures : 1;
    unsigned int    subWindowMode : 1;
    unsigned int    polyEdge : 1;
    unsigned int    polyMode : 1;
    unsigned int    freeCompClip : 1;
    unsigned int    clientClipType : 2;
    unsigned int    componentAlpha : 1;
    unsigned int    unused : 23;

    struct _IcImage *alphaMap;
    DDXPointRec	    alphaOrigin;

    DDXPointRec	    clipOrigin;
    pointer	    clientClip;

    Atom	    dither;

    unsigned long   stateChanges;
    unsigned long   serialNumber;

    Region	    pCompositeClip;
    
    IcTransform   *transform;

    int		    filter;
    xFixed	    *filter_params;
    int		    filter_nparams;

    int		    owns_pixels;
};

#endif /* _ICIMAGE_H_ */

#ifndef _IC_MIPICT_H_
#define _IC_MIPICT_H_

#define IC_MAX_INDEXED	256 /* XXX depth must be <= 8 */

#if IC_MAX_INDEXED <= 256
typedef CARD8 IcIndexType;
#endif

typedef struct _IcIndexed {
    Bool	color;
    CARD32	rgba[IC_MAX_INDEXED];
    IcIndexType	ent[32768];
} IcIndexedRec, *IcIndexedPtr;

#define IcCvtR8G8B8to15(s) ((((s) >> 3) & 0x001f) | \
			     (((s) >> 6) & 0x03e0) | \
			     (((s) >> 9) & 0x7c00))
#define IcIndexToEnt15(icf,rgb15) ((icf)->ent[rgb15])
#define IcIndexToEnt24(icf,rgb24) IcIndexToEnt15(icf,IcCvtR8G8B8to15(rgb24))

#define IcIndexToEntY24(icf,rgb24) ((icf)->ent[CvtR8G8B8toY15(rgb24)])

int
IcCreatePicture (PicturePtr pPicture);

void
IcImageDestroy (IcImage *image);

void
IcImageInit (IcImage *image);

int
IcImageChange (IcImage		*image,
	       Mask		vmask,
	       XID		*vlist,
	       DevUnion		*ulist,
	       int		*error_value);

int
IcImageChangeClip (IcImage	*image,
		   int		type,
		   pointer	value,
		   int		n);

void
IcImageDestroyClip (IcImage *image);

void
IcValidatePicture (PicturePtr pPicture,
		   Mask       mask);


/* XXX: What should this be?
Bool
IcClipPicture (RegionPtr    pRegion,
	       PicturePtr   pPicture,
	       INT16	    xReg,
	       INT16	    yReg,
	       INT16	    xPict,
	       INT16	    yPict);
*/

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
			  CARD16	height);

Bool
IcPictureInit (ScreenPtr pScreen, PictFormatPtr formats, int nformats);

void
IcGlyphs (CARD8		op,
	  PicturePtr	pSrc,
	  PicturePtr	pDst,
	  PictFormatPtr	maskFormat,
	  INT16		xSrc,
	  INT16		ySrc,
	  int		nlist,
	  GlyphListPtr	list,
	  GlyphPtr	*glyphs);

void
IcRenderColorToPixel (PictFormatPtr pPict,
		      xRenderColor  *color,
		      CARD32	    *pixel);

void
IcRenderPixelToColor (PictFormatPtr pPict,
		      CARD32	    pixel,
		      xRenderColor  *color);

void
IcCompositeRects (CARD8		op,
		  PicturePtr	pDst,
		  xRenderColor  *color,
		  int		nRect,
		  xRectangle    *rects);

/* XXX: Need to fix this...
void
IcTrapezoidBounds (int ntrap, xTrapezoid *traps, BoxPtr box);
*/

/* XXX: Need to fix this...
void
IcPointFixedBounds (int npoint, xPointFixed *points, BoxPtr bounds);
*/
    
/* XXX: Need to fix this...
void
IcTriangleBounds (int ntri, xTriangle *tris, BoxPtr bounds);
*/

void
IcRasterizeTriangle (PicturePtr	pMask,
		     xTriangle	*tri,
		     int	x_off,
		     int	y_off);

void
IcTriangles (CARD8	    op,
	     PicturePtr	    pSrc,
	     PicturePtr	    pDst,
	     PictFormatPtr  maskFormat,
	     INT16	    xSrc,
	     INT16	    ySrc,
	     int	    ntri,
	     xTriangle	    *tris);

void
IcTriStrip (CARD8	    op,
	    PicturePtr	    pSrc,
	    PicturePtr	    pDst,
	    PictFormatPtr   maskFormat,
	    INT16	    xSrc,
	    INT16	    ySrc,
	    int		    npoint,
	    xPointFixed	    *points);

void
IcTriFan (CARD8		op,
	  PicturePtr	pSrc,
	  PicturePtr	pDst,
	  PictFormatPtr maskFormat,
	  INT16		xSrc,
	  INT16		ySrc,
	  int		npoint,
	  xPointFixed	*points);

IcImage *
IcCreateAlphaPicture (IcImage	*dst,
		      IcFormat	*format,
		      CARD16	width,
		      CARD16	height);

/* XXX: Do we need these?
Bool
IcInitIndexed (ScreenPtr	pScreen,
	       PictFormatPtr	pFormat);

void
IcCloseIndexed (ScreenPtr	pScreen,
		PictFormatPtr	pFormat);

void
IcUpdateIndexed (ScreenPtr	pScreen,
		 PictFormatPtr	pFormat,
		 int		ndef,
		 xColorItem	*pdef);
*/

typedef void	(*CompositeFunc) (CARD8      op,
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

typedef struct _IcCompositeOperand IcCompositeOperand;

typedef CARD32 (*IcCompositeFetch)(IcCompositeOperand *op);
typedef void (*IcCompositeStore) (IcCompositeOperand *op, CARD32 value);

typedef void (*IcCompositeStep) (IcCompositeOperand *op);
typedef void (*IcCompositeSet) (IcCompositeOperand *op, int x, int y);

struct _IcCompositeOperand {
    union {
	struct {
	    IcBits		*top_line;
	    int			left_offset;
	    
	    int			start_offset;
	    IcBits		*line;
	    CARD32		offset;
	    IcStride		stride;
	    int			bpp;
	} drawable;
	struct {
	    int			alpha_dx;
	    int			alpha_dy;
	} external;
	struct {
	    int			top_y;
	    int			left_x;
	    int			start_x;
	    int			x;
	    int			y;
	    IcTransformPtr	transform;
	    int			filter;
	} transform;
    } u;
    IcCompositeFetch	fetch;
    IcCompositeFetch	fetcha;
    IcCompositeStore	store;
    IcCompositeStep	over;
    IcCompositeStep	down;
    IcCompositeSet	set;
    IcIndexedPtr	indexed;
    Region		clip;
};

typedef void (*IcCombineFunc) (IcCompositeOperand	*src,
			       IcCompositeOperand	*msk,
			       IcCompositeOperand	*dst);

/*
 * indexed by op
 */
extern IcCombineFunc	icCombineFunc[];

typedef struct _IcAccessMap {
    CARD32		format;
    IcCompositeFetch	fetch;
    IcCompositeFetch	fetcha;
    IcCompositeStore	store;
} IcAccessMap;

/*
 * search on format
 */
extern IcAccessMap  icAccessMap[];

/* iccompose.c */

typedef struct _IcCompSrc {
    CARD32	value;
    CARD32	alpha;
} IcCompSrc;

/*
 * All compositing operators *
 */

CARD32
IcCombineMaskU (IcCompositeOperand   *src,
		IcCompositeOperand   *msk);

IcCompSrc
IcCombineMaskC (IcCompositeOperand   *src,
		IcCompositeOperand   *msk);

CARD32
IcCombineMaskValueC (IcCompositeOperand   *src,
		     IcCompositeOperand   *msk);

CARD32
IcCombineMaskAlphaU (IcCompositeOperand   *src,
		     IcCompositeOperand   *msk);

CARD32
IcCombineMaskAlphaC (IcCompositeOperand   *src,
		     IcCompositeOperand   *msk);


#if 0
CARD32
IcCombineMask (IcCompositeOperand   *src,
	       IcCompositeOperand   *msk);
#endif

void
IcCombineClear (IcCompositeOperand   *src,
		IcCompositeOperand   *msk,
		IcCompositeOperand   *dst);

void
IcCombineSrcU (IcCompositeOperand    *src,
	       IcCompositeOperand    *msk,
	       IcCompositeOperand    *dst);

void
IcCombineSrcC (IcCompositeOperand    *src,
	       IcCompositeOperand    *msk,
	       IcCompositeOperand    *dst);

void
IcCombineDst (IcCompositeOperand    *src,
	      IcCompositeOperand    *msk,
	      IcCompositeOperand    *dst);

void
IcCombineOverU (IcCompositeOperand   *src,
		IcCompositeOperand   *msk,
		IcCompositeOperand   *dst);

void
IcCombineOverC (IcCompositeOperand   *src,
		IcCompositeOperand   *msk,
		IcCompositeOperand   *dst);

void
IcCombineOverReverseU (IcCompositeOperand    *src,
		       IcCompositeOperand    *msk,
		       IcCompositeOperand    *dst);

void
IcCombineOverReverseC (IcCompositeOperand    *src,
		       IcCompositeOperand    *msk,
		       IcCompositeOperand    *dst);

void
IcCombineInU (IcCompositeOperand	    *src,
	      IcCompositeOperand	    *msk,
	      IcCompositeOperand	    *dst);

void
IcCombineInC (IcCompositeOperand	    *src,
	      IcCompositeOperand	    *msk,
	      IcCompositeOperand	    *dst);

void
IcCombineInReverseU (IcCompositeOperand  *src,
		     IcCompositeOperand  *msk,
		     IcCompositeOperand  *dst);

void
IcCombineInReverseC (IcCompositeOperand  *src,
		     IcCompositeOperand  *msk,
		     IcCompositeOperand  *dst);

void
IcCombineOutU (IcCompositeOperand    *src,
	       IcCompositeOperand    *msk,
	       IcCompositeOperand    *dst);

void
IcCombineOutC (IcCompositeOperand    *src,
	       IcCompositeOperand    *msk,
	       IcCompositeOperand    *dst);

void
IcCombineOutReverseU (IcCompositeOperand *src,
		      IcCompositeOperand *msk,
		      IcCompositeOperand *dst);

void
IcCombineOutReverseC (IcCompositeOperand *src,
		      IcCompositeOperand *msk,
		      IcCompositeOperand *dst);

void
IcCombineAtopU (IcCompositeOperand   *src,
		IcCompositeOperand   *msk,
		IcCompositeOperand   *dst);


void
IcCombineAtopC (IcCompositeOperand   *src,
		IcCompositeOperand   *msk,
		IcCompositeOperand   *dst);

void
IcCombineAtopReverseU (IcCompositeOperand    *src,
		       IcCompositeOperand    *msk,
		       IcCompositeOperand    *dst);

void
IcCombineAtopReverseC (IcCompositeOperand    *src,
		       IcCompositeOperand    *msk,
		       IcCompositeOperand    *dst);

void
IcCombineXorU (IcCompositeOperand    *src,
	       IcCompositeOperand    *msk,
	       IcCompositeOperand    *dst);

void
IcCombineXorC (IcCompositeOperand    *src,
	       IcCompositeOperand    *msk,
	       IcCompositeOperand    *dst);


void
IcCombineAddU (IcCompositeOperand    *src,
	       IcCompositeOperand    *msk,
	       IcCompositeOperand    *dst);

void
IcCombineAddC (IcCompositeOperand    *src,
	       IcCompositeOperand    *msk,
	       IcCompositeOperand    *dst);

void
IcCombineSaturateU (IcCompositeOperand   *src,
		    IcCompositeOperand   *msk,
		    IcCompositeOperand   *dst);

void
IcCombineSaturateC (IcCompositeOperand   *src,
		    IcCompositeOperand   *msk,
		    IcCompositeOperand   *dst);

CARD8
IcCombineDisjointOutPart (CARD8 a, CARD8 b);

CARD8
IcCombineDisjointInPart (CARD8 a, CARD8 b);

void
IcCombineDisjointGeneralU (IcCompositeOperand   *src,
			   IcCompositeOperand   *msk,
			   IcCompositeOperand   *dst,
			   CARD8		combine);

void
IcCombineDisjointGeneralC (IcCompositeOperand   *src,
			   IcCompositeOperand   *msk,
			   IcCompositeOperand   *dst,
			   CARD8		combine);

void
IcCombineDisjointOverU (IcCompositeOperand   *src,
			IcCompositeOperand   *msk,
			IcCompositeOperand   *dst);

void
IcCombineDisjointOverC (IcCompositeOperand   *src,
			IcCompositeOperand   *msk,
			IcCompositeOperand   *dst);

void
IcCombineDisjointOverReverseU (IcCompositeOperand    *src,
			       IcCompositeOperand    *msk,
			       IcCompositeOperand    *dst);

void
IcCombineDisjointOverReverseC (IcCompositeOperand    *src,
			       IcCompositeOperand    *msk,
			       IcCompositeOperand    *dst);

void
IcCombineDisjointInU (IcCompositeOperand	    *src,
		      IcCompositeOperand	    *msk,
		      IcCompositeOperand	    *dst);

void
IcCombineDisjointInC (IcCompositeOperand	    *src,
		      IcCompositeOperand	    *msk,
		      IcCompositeOperand	    *dst);

void
IcCombineDisjointInReverseU (IcCompositeOperand  *src,
                             IcCompositeOperand  *msk,
                             IcCompositeOperand  *dst);

void
IcCombineDisjointInReverseC (IcCompositeOperand  *src,
                             IcCompositeOperand  *msk,
                             IcCompositeOperand  *dst);

void
IcCombineDisjointOutU (IcCompositeOperand    *src,
                       IcCompositeOperand    *msk,
                       IcCompositeOperand    *dst);

void
IcCombineDisjointOutC (IcCompositeOperand    *src,
                       IcCompositeOperand    *msk,
                       IcCompositeOperand    *dst);
void
IcCombineDisjointOutReverseU (IcCompositeOperand *src,
                              IcCompositeOperand *msk,
                              IcCompositeOperand *dst);

void
IcCombineDisjointOutReverseC (IcCompositeOperand *src,
                              IcCompositeOperand *msk,
                              IcCompositeOperand *dst);

void
IcCombineDisjointAtopU (IcCompositeOperand   *src,
                        IcCompositeOperand   *msk,
                        IcCompositeOperand   *dst);

void
IcCombineDisjointAtopC (IcCompositeOperand   *src,
                        IcCompositeOperand   *msk,
                        IcCompositeOperand   *dst);

void
IcCombineDisjointAtopReverseU (IcCompositeOperand    *src,
                               IcCompositeOperand    *msk,
                               IcCompositeOperand    *dst);

void
IcCombineDisjointAtopReverseC (IcCompositeOperand    *src,
                               IcCompositeOperand    *msk,
                               IcCompositeOperand    *dst);

void
IcCombineDisjointXorU (IcCompositeOperand    *src,
                       IcCompositeOperand    *msk,
                       IcCompositeOperand    *dst);

void
IcCombineDisjointXorC (IcCompositeOperand    *src,
                       IcCompositeOperand    *msk,
                       IcCompositeOperand    *dst);

CARD8
IcCombineConjointOutPart (CARD8 a, CARD8 b);

CARD8
IcCombineConjointInPart (CARD8 a, CARD8 b);


void
IcCombineConjointGeneralU (IcCompositeOperand   *src,
                           IcCompositeOperand   *msk,
                           IcCompositeOperand   *dst,
                           CARD8                combine);

void
IcCombineConjointGeneralC (IcCompositeOperand   *src,
                           IcCompositeOperand   *msk,
                           IcCompositeOperand   *dst,
                           CARD8                combine);

void
IcCombineConjointOverU (IcCompositeOperand   *src,
                        IcCompositeOperand   *msk,
                        IcCompositeOperand   *dst);

void
IcCombineConjointOverC (IcCompositeOperand   *src,
                        IcCompositeOperand   *msk,
                        IcCompositeOperand   *dst);
void
IcCombineConjointOverReverseU (IcCompositeOperand    *src,
                               IcCompositeOperand    *msk,
                               IcCompositeOperand    *dst);

void
IcCombineConjointOverReverseC (IcCompositeOperand    *src,
                               IcCompositeOperand    *msk,
                               IcCompositeOperand    *dst);

void
IcCombineConjointInU (IcCompositeOperand            *src,
                      IcCompositeOperand            *msk,
                      IcCompositeOperand            *dst);

void
IcCombineConjointInC (IcCompositeOperand            *src,
                      IcCompositeOperand            *msk,
                      IcCompositeOperand            *dst);

void
IcCombineConjointInReverseU (IcCompositeOperand  *src,
                             IcCompositeOperand  *msk,
                             IcCompositeOperand  *dst);


void
IcCombineConjointInReverseC (IcCompositeOperand  *src,
                             IcCompositeOperand  *msk,
                             IcCompositeOperand  *dst);

void
IcCombineConjointOutU (IcCompositeOperand    *src,
                       IcCompositeOperand    *msk,
                       IcCompositeOperand    *dst);

void
IcCombineConjointOutC (IcCompositeOperand    *src,
                       IcCompositeOperand    *msk,
                       IcCompositeOperand    *dst);

void
IcCombineConjointOutReverseU (IcCompositeOperand *src,
                              IcCompositeOperand *msk,
                              IcCompositeOperand *dst);

void
IcCombineConjointOutReverseC (IcCompositeOperand *src,
                              IcCompositeOperand *msk,
                              IcCompositeOperand *dst);

void
IcCombineConjointAtopU (IcCompositeOperand   *src,
                        IcCompositeOperand   *msk,
                        IcCompositeOperand   *dst);

void
IcCombineConjointAtopC (IcCompositeOperand   *src,
                        IcCompositeOperand   *msk,
                        IcCompositeOperand   *dst);

void
IcCombineConjointAtopReverseU (IcCompositeOperand    *src,
                               IcCompositeOperand    *msk,
                               IcCompositeOperand    *dst);
void
IcCombineConjointAtopReverseC (IcCompositeOperand    *src,
                               IcCompositeOperand    *msk,
                               IcCompositeOperand    *dst);

void
IcCombineConjointXorU (IcCompositeOperand    *src,
                       IcCompositeOperand    *msk,
                       IcCompositeOperand    *dst);

void
IcCombineConjointXorC (IcCompositeOperand    *src,
                       IcCompositeOperand    *msk,
                       IcCompositeOperand    *dst);

/*
 * All fetch functions
 */

CARD32
IcFetch_a8r8g8b8 (IcCompositeOperand *op);

CARD32
IcFetch_x8r8g8b8 (IcCompositeOperand *op);

CARD32
IcFetch_a8b8g8r8 (IcCompositeOperand *op);

CARD32
IcFetch_x8b8g8r8 (IcCompositeOperand *op);

CARD32
IcFetch_r8g8b8 (IcCompositeOperand *op);

CARD32
IcFetch_b8g8r8 (IcCompositeOperand *op);

CARD32
IcFetch_r5g6b5 (IcCompositeOperand *op);

CARD32
IcFetch_b5g6r5 (IcCompositeOperand *op);

CARD32
IcFetch_a1r5g5b5 (IcCompositeOperand *op);

CARD32
IcFetch_x1r5g5b5 (IcCompositeOperand *op);

CARD32
IcFetch_a1b5g5r5 (IcCompositeOperand *op);

CARD32
IcFetch_x1b5g5r5 (IcCompositeOperand *op);

CARD32
IcFetch_a4r4g4b4 (IcCompositeOperand *op);

CARD32
IcFetch_x4r4g4b4 (IcCompositeOperand *op);

CARD32
IcFetch_a4b4g4r4 (IcCompositeOperand *op);

CARD32
IcFetch_x4b4g4r4 (IcCompositeOperand *op);

CARD32
IcFetch_a8 (IcCompositeOperand *op);

CARD32
IcFetcha_a8 (IcCompositeOperand *op);

CARD32
IcFetch_r3g3b2 (IcCompositeOperand *op);

CARD32
IcFetch_b2g3r3 (IcCompositeOperand *op);

CARD32
IcFetch_a2r2g2b2 (IcCompositeOperand *op);

CARD32
IcFetch_a2b2g2r2 (IcCompositeOperand *op);

CARD32
IcFetch_c8 (IcCompositeOperand *op);

CARD32
IcFetch_a4 (IcCompositeOperand *op);

CARD32
IcFetcha_a4 (IcCompositeOperand *op);

CARD32
IcFetch_r1g2b1 (IcCompositeOperand *op);

CARD32
IcFetch_b1g2r1 (IcCompositeOperand *op);

CARD32
IcFetch_a1r1g1b1 (IcCompositeOperand *op);

CARD32
IcFetch_a1b1g1r1 (IcCompositeOperand *op);

CARD32
IcFetch_c4 (IcCompositeOperand *op);

CARD32
IcFetch_a1 (IcCompositeOperand *op);

CARD32
IcFetcha_a1 (IcCompositeOperand *op);

CARD32
IcFetch_g1 (IcCompositeOperand *op);

void
IcStore_a8r8g8b8 (IcCompositeOperand *op, CARD32 value);

void
IcStore_x8r8g8b8 (IcCompositeOperand *op, CARD32 value);

void
IcStore_a8b8g8r8 (IcCompositeOperand *op, CARD32 value);

void
IcStore_x8b8g8r8 (IcCompositeOperand *op, CARD32 value);

void
IcStore_r8g8b8 (IcCompositeOperand *op, CARD32 value);

void
IcStore_b8g8r8 (IcCompositeOperand *op, CARD32 value);

void
IcStore_r5g6b5 (IcCompositeOperand *op, CARD32 value);

void
IcStore_b5g6r5 (IcCompositeOperand *op, CARD32 value);

void
IcStore_a1r5g5b5 (IcCompositeOperand *op, CARD32 value);

void
IcStore_x1r5g5b5 (IcCompositeOperand *op, CARD32 value);

void
IcStore_a1b5g5r5 (IcCompositeOperand *op, CARD32 value);

void
IcStore_x1b5g5r5 (IcCompositeOperand *op, CARD32 value);

void
IcStore_a4r4g4b4 (IcCompositeOperand *op, CARD32 value);

void
IcStore_x4r4g4b4 (IcCompositeOperand *op, CARD32 value);

void
IcStore_a4b4g4r4 (IcCompositeOperand *op, CARD32 value);

void
IcStore_x4b4g4r4 (IcCompositeOperand *op, CARD32 value);

void
IcStore_a8 (IcCompositeOperand *op, CARD32 value);

void
IcStore_r3g3b2 (IcCompositeOperand *op, CARD32 value);

void
IcStore_b2g3r3 (IcCompositeOperand *op, CARD32 value);

void
IcStore_a2r2g2b2 (IcCompositeOperand *op, CARD32 value);

void
IcStore_c8 (IcCompositeOperand *op, CARD32 value);

void
IcStore_g8 (IcCompositeOperand *op, CARD32 value);


void
IcStore_a4 (IcCompositeOperand *op, CARD32 value);

void
IcStore_r1g2b1 (IcCompositeOperand *op, CARD32 value);

void
IcStore_b1g2r1 (IcCompositeOperand *op, CARD32 value);

void
IcStore_a1r1g1b1 (IcCompositeOperand *op, CARD32 value);

void
IcStore_a1b1g1r1 (IcCompositeOperand *op, CARD32 value);

void
IcStore_c4 (IcCompositeOperand *op, CARD32 value);

void
IcStore_g4 (IcCompositeOperand *op, CARD32 value);

void
IcStore_a1 (IcCompositeOperand *op, CARD32 value);

void
IcStore_g1 (IcCompositeOperand *op, CARD32 value);

CARD32
IcFetch_external (IcCompositeOperand *op);

CARD32
IcFetch_transform (IcCompositeOperand *op);

CARD32
IcFetcha_transform (IcCompositeOperand *op);

CARD32
IcFetcha_external (IcCompositeOperand *op);

void
IcStore_external (IcCompositeOperand *op, CARD32 value);

Bool
IcBuildOneCompositeOperand (PicturePtr		pPict,
			    IcCompositeOperand	*op,
			    INT16		x,
			    INT16		y);
Bool
IcBuildCompositeOperand (IcImage	    *image,
			 IcCompositeOperand op[4],
			 INT16		    x,
			 INT16		    y,
			 Bool		    transform,
			 Bool		    alpha);
void
IcCompositeGeneral (CARD8	op,
		    IcImage	*iSrc,
		    IcImage	*iMask,
		    IcImage	*iDst,
		    INT16	xSrc,
		    INT16	ySrc,
		    INT16	xMask,
		    INT16	yMask,
		    INT16	xDst,
		    INT16	yDst,
		    CARD16	width,
		    CARD16	height);


/* icimage.c */
CARD32
IcOver (CARD32 x, CARD32 y);

CARD32
IcOver24 (CARD32 x, CARD32 y);

CARD32
IcIn (CARD32 x, CARD8 y);

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
			       CARD16     height);

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
			       CARD16     height);

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
				   CARD16     height);

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
			       CARD16     height);

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
				   CARD16     height);

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
			  CARD16     height);

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
			 CARD16     height);

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
			  CARD16     height);

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
			  CARD16     height);

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
			     CARD16     height);

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
			     CARD16     height);

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
			     CARD16     height);

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
			    CARD16     height);

/* over in ic.c */

int
IcImageSetTransform (IcImage		*image,
		     IcTransform	*transform);

#endif /* _IC_MIPICT_H_ */
