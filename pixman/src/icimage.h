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

/* XXX: This whole file should be moved up into incint.h (and cleaned
   up considerably as well) */

#ifndef _ICIMAGE_H_
#define _ICIMAGE_H_

#include <X11/Xdefs.h>

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

/* #include "glyphstr.h" */
/* #include "scrnintstr.h" */

/* XXX: Hmmm... what's needed from here?
#include "resource.h"
*/

#include <X11/Xutil.h>

#define IcIntMult(a,b,t) ( (t) = (a) * (b) + 0x80, ( ( ( (t)>>8 ) + (t) )>>8 ) )
#define IcIntDiv(a,b)	 (((uint16_t) (a) * 255) / (b))

#define IcGet8(v,i)   ((uint16_t) (uint8_t) ((v) >> i))

/*
 * There are two ways of handling alpha -- either as a single unified value or
 * a separate value for each component, hence each macro must have two
 * versions.  The unified alpha version has a 'U' at the end of the name,
 * the component version has a 'C'.  Similarly, functions which deal with
 * this difference will have two versions using the same convention.
 */

#define IcOverU(x,y,i,a,t) ((t) = IcIntMult(IcGet8(y,i),(a),(t)) + IcGet8(x,i),\
			   (uint32_t) ((uint8_t) ((t) | (0 - ((t) >> 8)))) << (i))

#define IcOverC(x,y,i,a,t) ((t) = IcIntMult(IcGet8(y,i),IcGet8(a,i),(t)) + IcGet8(x,i),\
			    (uint32_t) ((uint8_t) ((t) | (0 - ((t) >> 8)))) << (i))

#define IcInU(x,i,a,t) ((uint32_t) IcIntMult(IcGet8(x,i),(a),(t)) << (i))

#define IcInC(x,i,a,t) ((uint32_t) IcIntMult(IcGet8(x,i),IcGet8(a,i),(t)) << (i))

#define IcGen(x,y,i,ax,ay,t,u,v) ((t) = (IcIntMult(IcGet8(y,i),ay,(u)) + \
					 IcIntMult(IcGet8(x,i),ax,(v))),\
				  (uint32_t) ((uint8_t) ((t) | \
						     (0 - ((t) >> 8)))) << (i))

#define IcAdd(x,y,i,t)	((t) = IcGet8(x,i) + IcGet8(y,i), \
			 (uint32_t) ((uint8_t) ((t) | (0 - ((t) >> 8)))) << (i))

/*
typedef struct _IndexFormat {
    VisualPtr	    pVisual; 
    ColormapPtr	    pColormap;
    int		    nvalues;
    xIndexValue	    *pValues;
    void	    *devPrivate;
} IndexFormatRec;
*/

/*
typedef struct _IcFormat {
    uint32_t	    id;
    uint32_t	    format;
    unsigned char   type;
    unsigned char   depth;
    DirectFormatRec direct;
    IndexFormatRec  index;
} IcFormatRec;
*/

struct _IcImage {
    IcPixels	    *pixels;
    IcFormat	    *image_format;
    /* XXX: Should switch from int to an IcFormatName enum */
    int		    format_name;
    int		    refcnt;
    
    unsigned int    repeat : 1;
    unsigned int    graphicsExposures : 1;
    unsigned int    subWindowMode : 1;
    unsigned int    polyEdge : 1;
    unsigned int    polyMode : 1;
    /* XXX: Do we need this field */
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

    PixRegion	    *pCompositeClip;
    
    IcTransform     *transform;

    IcFilter	    filter;
    IcFixed16_16    *filter_params;
    int		    filter_nparams;

    int		    owns_pixels;
};

#endif /* _ICIMAGE_H_ */

#ifndef _IC_MIPICT_H_
#define _IC_MIPICT_H_

#define IC_MAX_INDEXED	256 /* XXX depth must be <= 8 */

#if IC_MAX_INDEXED <= 256
typedef uint8_t IcIndexType;
#endif

/* XXX: We're not supporting indexed operations, right?
typedef struct _IcIndexed {
    Bool	color;
    uint32_t	rgba[IC_MAX_INDEXED];
    IcIndexType	ent[32768];
} IcIndexedRec, *IcIndexedPtr;
*/

#define IcCvtR8G8B8to15(s) ((((s) >> 3) & 0x001f) | \
			     (((s) >> 6) & 0x03e0) | \
			     (((s) >> 9) & 0x7c00))
#define IcIndexToEnt15(icf,rgb15) ((icf)->ent[rgb15])
#define IcIndexToEnt24(icf,rgb24) IcIndexToEnt15(icf,IcCvtR8G8B8to15(rgb24))

#define IcIndexToEntY24(icf,rgb24) ((icf)->ent[CvtR8G8B8toY15(rgb24)])

/*
extern int __internal_linkage
IcCreatePicture (PicturePtr pPicture);
*/

extern void __internal_linkage
IcImageInit (IcImage *image);

extern int __internal_linkage
IcImageChange (IcImage		*image,
	       Mask		vmask,
	       XID		*vlist,
	       DevUnion		*ulist,
	       int		*error_value);

extern void __internal_linkage
IcImageDestroyClip (IcImage *image);

/*
extern void __internal_linkage
IcValidatePicture (PicturePtr pPicture,
		   Mask       mask);
*/


/* XXX: What should this be?
extern Bool __internal_linkage
IcClipPicture (PixRegion    *region,
	       IcImage	    *image,
	       int16_t	    xReg,
	       int16_t	    yReg,
	       int16_t	    xPict,
	       int16_t	    yPict);
*/

extern Bool __internal_linkage
IcComputeCompositeRegion (PixRegion	*region,
			  IcImage	*iSrc,
			  IcImage	*iMask,
			  IcImage	*iDst,
			  int16_t		xSrc,
			  int16_t		ySrc,
			  int16_t		xMask,
			  int16_t		yMask,
			  int16_t		xDst,
			  int16_t		yDst,
			  uint16_t	width,
			  uint16_t	height);

/*
extern Bool __internal_linkage
IcPictureInit (ScreenPtr pScreen, PictFormatPtr formats, int nformats);
*/

/*
extern void __internal_linkage
IcGlyphs (uint8_t		op,
	  PicturePtr	pSrc,
	  PicturePtr	pDst,
	  PictFormatPtr	maskFormat,
	  int16_t		xSrc,
	  int16_t		ySrc,
	  int		nlist,
	  GlyphListPtr	list,
	  GlyphPtr	*glyphs);
*/

/*
extern void __internal_linkage
IcCompositeRects (uint8_t		op,
		  PicturePtr	pDst,
		  xRenderColor  *color,
		  int		nRect,
		  xRectangle    *rects);
*/

extern IcImage * __internal_linkage
IcCreateAlphaPicture (IcImage	*dst,
		      IcFormat	*format,
		      uint16_t	width,
		      uint16_t	height);

typedef void	(*CompositeFunc) (uint8_t      op,
				  IcImage    *iSrc,
				  IcImage    *iMask,
				  IcImage    *iDst,
				  int16_t      xSrc,
				  int16_t      ySrc,
				  int16_t      xMask,
				  int16_t      yMask,
				  int16_t      xDst,
				  int16_t      yDst,
				  uint16_t     width,
				  uint16_t     height);

typedef struct _IcCompositeOperand IcCompositeOperand;

typedef uint32_t (*IcCompositeFetch)(IcCompositeOperand *op);
typedef void (*IcCompositeStore) (IcCompositeOperand *op, uint32_t value);

typedef void (*IcCompositeStep) (IcCompositeOperand *op);
typedef void (*IcCompositeSet) (IcCompositeOperand *op, int x, int y);

struct _IcCompositeOperand {
    union {
	struct {
	    IcBits		*top_line;
	    int			left_offset;
	    
	    int			start_offset;
	    IcBits		*line;
	    uint32_t		offset;
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
	    IcTransform		*transform;
	    IcFilter		filter;
	} transform;
    } u;
    IcCompositeFetch	fetch;
    IcCompositeFetch	fetcha;
    IcCompositeStore	store;
    IcCompositeStep	over;
    IcCompositeStep	down;
    IcCompositeSet	set;
/* XXX: We're not supporting indexed operations, right?
    IcIndexedPtr	indexed;
*/
    PixRegion		*clip;
};

typedef void (*IcCombineFunc) (IcCompositeOperand	*src,
			       IcCompositeOperand	*msk,
			       IcCompositeOperand	*dst);

typedef struct _IcAccessMap {
    uint32_t		format;
    IcCompositeFetch	fetch;
    IcCompositeFetch	fetcha;
    IcCompositeStore	store;
} IcAccessMap;

/* iccompose.c */

typedef struct _IcCompSrc {
    uint32_t	value;
    uint32_t	alpha;
} IcCompSrc;

extern Bool __internal_linkage
IcBuildCompositeOperand (IcImage	    *image,
			 IcCompositeOperand op[4],
			 int16_t		    x,
			 int16_t		    y,
			 Bool		    transform,
			 Bool		    alpha);

extern void __internal_linkage
IcCompositeGeneral (uint8_t	op,
		    IcImage	*iSrc,
		    IcImage	*iMask,
		    IcImage	*iDst,
		    int16_t	xSrc,
		    int16_t	ySrc,
		    int16_t	xMask,
		    int16_t	yMask,
		    int16_t	xDst,
		    int16_t	yDst,
		    uint16_t	width,
		    uint16_t	height);

#endif /* _IC_MIPICT_H_ */
