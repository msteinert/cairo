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

#include "icint.h"
#include "icimage.h"

#include "misc.h"

/*
#include "scrnintstr.h"
#include "validate.h"
#include "windowstr.h"
#include "input.h"
#include "colormapst.h"
#include "cursorstr.h"
#include "dixstruct.h"
#include "gcstruct.h"
#include "picturestr.h"
*/

#include "os.h"
#include "resource.h"
#include "servermd.h"

int		PictureScreenPrivateIndex = -1;
int		PictureWindowPrivateIndex;
int		PictureGeneration;
RESTYPE		PictureType;
/* RESTYPE		PictFormatType; */
RESTYPE		GlyphSetType;
int		PictureCmapPolicy = PictureCmapPolicyDefault;

/* XXX: Do we need this?
Bool
PictureDestroyWindow (WindowPtr pWindow)
{
    ScreenPtr		pScreen = pWindow->drawable.pScreen;
    PicturePtr		pPicture;
    PictureScreenPtr    ps = GetPictureScreen(pScreen);
    Bool		ret;

    while ((pPicture = GetPictureWindow(pWindow)))
    {
	SetPictureWindow(pWindow, pPicture->pNext);
	if (pPicture->id)
	    FreeResource (pPicture->id, PictureType);
	FreePicture ((pointer) pPicture, pPicture->id);
    }
    pScreen->DestroyWindow = ps->DestroyWindow;
    ret = (*pScreen->DestroyWindow) (pWindow);
    ps->DestroyWindow = pScreen->DestroyWindow;
    pScreen->DestroyWindow = PictureDestroyWindow;
    return ret;
}

Bool
PictureCloseScreen (int index, ScreenPtr pScreen)
{
    PictureScreenPtr    ps = GetPictureScreen(pScreen);
    Bool                ret;
    int			n;

    pScreen->CloseScreen = ps->CloseScreen;
    ret = (*pScreen->CloseScreen) (index, pScreen);
    PictureResetFilters (pScreen);
    for (n = 0; n < ps->nformats; n++)
	if (ps->formats[n].type == PictTypeIndexed)
	    (*ps->CloseIndexed) (pScreen, &ps->formats[n]);
    SetPictureScreen(pScreen, 0);
    free (ps->formats);
    free (ps);
    return ret;
}
*/

/*
void
PictureStoreColors (ColormapPtr pColormap, int ndef, xColorItem *pdef)
{
    ScreenPtr		pScreen = pColormap->pScreen;
    PictureScreenPtr    ps = GetPictureScreen(pScreen);

    pScreen->StoreColors = ps->StoreColors;
    (*pScreen->StoreColors) (pColormap, ndef, pdef);
    ps->StoreColors = pScreen->StoreColors;
    pScreen->StoreColors = PictureStoreColors;

    if (pColormap->class == PseudoColor || pColormap->class == GrayScale)
    {
	PictFormatPtr	format = ps->formats;
	int		nformats = ps->nformats;

	while (nformats--)
	{
	    if (format->type == PictTypeIndexed &&
		format->index.pColormap == pColormap)
	    {
		(*ps->UpdateIndexed) (pScreen, format, ndef, pdef);
		break;
	    }
	    format++;
	}
    }
}
*/

#ifdef XXX_DO_WE_NEED_ANY_OF_THESE_FUNCTIONS
static int
visualDepth (ScreenPtr pScreen, VisualPtr pVisual)
{
    int		d, v;
    DepthPtr	pDepth;

    for (d = 0; d < pScreen->numDepths; d++)
    {
	pDepth = &pScreen->allowedDepths[d];
	for (v = 0; v < pDepth->numVids; v++)
	    if (pDepth->vids[v] == pVisual->vid)
		return pDepth->depth;
    }
    return 0;
}

typedef struct _formatInit {
    CARD32  format;
    CARD8   depth;
} FormatInitRec, *FormatInitPtr;

static int
addFormat (FormatInitRec    formats[256],
	   int		    nformat,
	   CARD32	    format,
	   CARD8	    depth)
{
    int	n;

    for (n = 0; n < nformat; n++)
	if (formats[n].format == format && formats[n].depth == depth)
	    return nformat;
    formats[nformat].format = format;
    formats[nformat].depth = depth;
    return ++nformat;
}

#define Mask(n)	((n) == 32 ? 0xffffffff : ((1 << (n))-1))

IcFormat *
IcCreateDefaultFormats (ScreenPtr pScreen, int *nformatp)
{
    int		    nformats, f;
    PictFormatPtr   pFormats;
    FormatInitRec   formats[1024];
    CARD32	    format;
    CARD8	    depth;
    VisualPtr	    pVisual;
    int		    v;
    int		    bpp;
    int		    type;
    int		    r, g, b;
    int		    d;
    DepthPtr	    pDepth;

    nformats = 0;
    /* formats required by protocol */
    formats[nformats].format = PICT_a1;
    formats[nformats].depth = 1;
    nformats++;
    formats[nformats].format = PICT_a8;
    formats[nformats].depth = 8;
    nformats++;
    formats[nformats].format = PICT_a4;
    formats[nformats].depth = 4;
    nformats++;
    formats[nformats].format = PICT_a8r8g8b8;
    formats[nformats].depth = 32;
    nformats++;
    formats[nformats].format = PICT_x8r8g8b8;
    formats[nformats].depth = 32;
    nformats++;

    pFormats = (PictFormatPtr) xalloc (nformats * sizeof (PictFormatRec));
    if (!pFormats)
	return 0;
    memset (pFormats, '\0', nformats * sizeof (PictFormatRec));
    for (f = 0; f < nformats; f++)
    {
        pFormats[f].id = FakeClientID (0);
	pFormats[f].depth = formats[f].depth;
	format = formats[f].format;
	pFormats[f].format = format;
	switch (PICT_FORMAT_TYPE(format)) {
	case PICT_TYPE_ARGB:
	    pFormats[f].type = PictTypeDirect;
	    
	    pFormats[f].direct.alphaMask = Mask(PICT_FORMAT_A(format));
	    if (pFormats[f].direct.alphaMask)
		pFormats[f].direct.alpha = (PICT_FORMAT_R(format) +
					    PICT_FORMAT_G(format) +
					    PICT_FORMAT_B(format));
	    
	    pFormats[f].direct.redMask = Mask(PICT_FORMAT_R(format));
	    pFormats[f].direct.red = (PICT_FORMAT_G(format) + 
				      PICT_FORMAT_B(format));
	    
	    pFormats[f].direct.greenMask = Mask(PICT_FORMAT_G(format));
	    pFormats[f].direct.green = PICT_FORMAT_B(format);
	    
	    pFormats[f].direct.blueMask = Mask(PICT_FORMAT_B(format));
	    pFormats[f].direct.blue = 0;
	    break;

	case PICT_TYPE_ABGR:
	    pFormats[f].type = PictTypeDirect;
	    
	    pFormats[f].direct.alphaMask = Mask(PICT_FORMAT_A(format));
	    if (pFormats[f].direct.alphaMask)
		pFormats[f].direct.alpha = (PICT_FORMAT_B(format) +
					    PICT_FORMAT_G(format) +
					    PICT_FORMAT_R(format));
	    
	    pFormats[f].direct.blueMask = Mask(PICT_FORMAT_B(format));
	    pFormats[f].direct.blue = (PICT_FORMAT_G(format) + 
				       PICT_FORMAT_R(format));
	    
	    pFormats[f].direct.greenMask = Mask(PICT_FORMAT_G(format));
	    pFormats[f].direct.green = PICT_FORMAT_R(format);
	    
	    pFormats[f].direct.redMask = Mask(PICT_FORMAT_R(format));
	    pFormats[f].direct.red = 0;
	    break;

	case PICT_TYPE_A:
	    pFormats[f].type = PictTypeDirect;

	    pFormats[f].direct.alpha = 0;
	    pFormats[f].direct.alphaMask = Mask(PICT_FORMAT_A(format));

	    /* remaining fields already set to zero */
	    break;
	    
/* XXX: Supporting indexed formats will take a bit of thought...
	case PICT_TYPE_COLOR:
	case PICT_TYPE_GRAY:
	    pFormats[f].type = PictTypeIndexed;
	    pFormats[f].index.pVisual = &pScreen->visuals[PICT_FORMAT_VIS(format)];
	    break;
*/
	}
    }
    *nformatp = nformats;
    return pFormats;
}

Bool
PictureInitIndexedFormats (ScreenPtr pScreen)
{
    PictureScreenPtr    ps = GetPictureScreenIfSet(pScreen);
    PictFormatPtr	format;
    int			nformat;

    if (!ps)
	return FALSE;
    format = ps->formats;
    nformat = ps->nformats;
    while (nformat--)
    {
	if (format->type == PictTypeIndexed && !format->index.pColormap)
	{
	    if (format->index.pVisual->vid == pScreen->rootVisual)
		format->index.pColormap = (ColormapPtr) LookupIDByType(pScreen->defColormap,
								       RT_COLORMAP);
	    else
	    {
		if (CreateColormap (FakeClientID (0), pScreen,
				    format->index.pVisual,
				    &format->index.pColormap, AllocNone,
				    0) != Success)
		{
		    return FALSE;
		}
	    }
	    if (!(*ps->InitIndexed) (pScreen, format))
		return FALSE;
	}
	format++;
    }
    return TRUE;
}

Bool
PictureFinishInit (void)
{
    int	    s;

    for (s = 0; s < screenInfo.numScreens; s++)
    {
	if (!PictureInitIndexedFormats (screenInfo.screens[s]))
	    return FALSE;
	(void) AnimCurInit (screenInfo.screens[s]);
  p  }

    return TRUE;
}

Bool
PictureSetSubpixelOrder (ScreenPtr pScreen, int subpixel)
{
    PictureScreenPtr    ps = GetPictureScreenIfSet(pScreen);

    if (!ps)
	return FALSE;
    ps->subpixel = subpixel;
    return TRUE;
    
}

int
PictureGetSubpixelOrder (ScreenPtr pScreen)
{
    PictureScreenPtr    ps = GetPictureScreenIfSet(pScreen);

    if (!ps)
	return SubPixelUnknown;
    return ps->subpixel;
}
    
PictFormatPtr
PictureMatchVisual (ScreenPtr pScreen, int depth, VisualPtr pVisual)
{
    PictureScreenPtr    ps = GetPictureScreenIfSet(pScreen);
    PictFormatPtr	format;
    int			nformat;
    int			type;

    if (!ps)
	return 0;
    format = ps->formats;
    nformat = ps->nformats;
    switch (pVisual->class) {
    case StaticGray:
    case GrayScale:
    case StaticColor:
    case PseudoColor:
	type = PictTypeIndexed;
	break;
    case TrueColor:
	type = PictTypeDirect;
	break;
    case DirectColor:
    default:
	return 0;
    }
    while (nformat--)
    {
	if (format->depth == depth && format->type == type)
	{
	    if (type == PictTypeIndexed)
	    {
		if (format->index.pVisual == pVisual)
		    return format;
	    }
	    else
	    {
		if (format->direct.redMask << format->direct.red == 
		    pVisual->redMask &&
		    format->direct.greenMask << format->direct.green == 
		    pVisual->greenMask &&
		    format->direct.blueMask << format->direct.blue == 
		    pVisual->blueMask)
		{
		    return format;
		}
	    }
	}
	format++;
    }
    return 0;
}

PictFormatPtr
PictureMatchFormat (ScreenPtr pScreen, int depth, CARD32 f)
{
    PictureScreenPtr    ps = GetPictureScreenIfSet(pScreen);
    PictFormatPtr	format;
    int			nformat;

    if (!ps)
	return 0;
    format = ps->formats;
    nformat = ps->nformats;
    while (nformat--)
    {
	if (format->depth == depth && format->format == (f & 0xffffff))
	    return format;
	format++;
    }
    return 0;
}

int
PictureParseCmapPolicy (const char *name)
{
    if ( strcmp (name, "default" ) == 0)
	return PictureCmapPolicyDefault;
    else if ( strcmp (name, "mono" ) == 0)
	return PictureCmapPolicyMono;
    else if ( strcmp (name, "gray" ) == 0)
	return PictureCmapPolicyGray;
    else if ( strcmp (name, "color" ) == 0)
	return PictureCmapPolicyColor;
    else if ( strcmp (name, "all" ) == 0)
	return PictureCmapPolicyAll;
    else
	return PictureCmapPolicyInvalid;
}

Bool
PictureInit (ScreenPtr pScreen, PictFormatPtr formats, int nformats)
{
    PictureScreenPtr	ps;
    int			n;
    CARD32		type, a, r, g, b;
    
    if (PictureGeneration != serverGeneration)
    {
	PictureType = CreateNewResourceType (FreePicture);
	if (!PictureType)
	    return FALSE;
	PictFormatType = CreateNewResourceType (FreePictFormat);
	if (!PictFormatType)
	    return FALSE;
	GlyphSetType = CreateNewResourceType (FreeGlyphSet);
	if (!GlyphSetType)
	    return FALSE;
	PictureScreenPrivateIndex = AllocateScreenPrivateIndex();
	if (PictureScreenPrivateIndex < 0)
	    return FALSE;
	PictureWindowPrivateIndex = AllocateWindowPrivateIndex();
	PictureGeneration = serverGeneration;
#ifdef XResExtension
	RegisterResourceName (PictureType, "PICTURE");
	RegisterResourceName (PictFormatType, "PICTFORMAT");
	RegisterResourceName (GlyphSetType, "GLYPHSET");
#endif
    }
    if (!AllocateWindowPrivate (pScreen, PictureWindowPrivateIndex, 0))
	return FALSE;
    
    if (!formats)
    {
	formats = PictureCreateDefaultFormats (pScreen, &nformats);
	if (!formats)
	    return FALSE;
    }
    for (n = 0; n < nformats; n++)
    {
	if (!AddResource (formats[n].id, PictFormatType, (pointer) (formats+n)))
	{
	    free (formats);
	    return FALSE;
	}
	if (formats[n].type == PictTypeIndexed)
	{
	    if ((formats[n].index.pVisual->class | DynamicClass) == PseudoColor)
		type = PICT_TYPE_COLOR;
	    else
		type = PICT_TYPE_GRAY;
	    a = r = g = b = 0;
	}
	else
	{
	    if ((formats[n].direct.redMask|
		 formats[n].direct.blueMask|
		 formats[n].direct.greenMask) == 0)
		type = PICT_TYPE_A;
	    else if (formats[n].direct.red > formats[n].direct.blue)
		type = PICT_TYPE_ARGB;
	    else
		type = PICT_TYPE_ABGR;
	    a = Ones (formats[n].direct.alphaMask);
	    r = Ones (formats[n].direct.redMask);
	    g = Ones (formats[n].direct.greenMask);
	    b = Ones (formats[n].direct.blueMask);
	}
	formats[n].format = PICT_FORMAT(0,type,a,r,g,b);
    }
    ps = (PictureScreenPtr) xalloc (sizeof (PictureScreenRec));
    if (!ps)
    {
        free (formats);
	return FALSE;
    }
    SetPictureScreen(pScreen, ps);
    if (!GlyphInit (pScreen))
    {
	SetPictureScreen(pScreen, 0);
	free (formats);
	free (ps);
	return FALSE;
    }

    ps->totalPictureSize = sizeof (PictureRec);
    ps->PicturePrivateSizes = 0;
    ps->PicturePrivateLen = 0;
    
    ps->formats = formats;
    ps->fallback = formats;
    ps->nformats = nformats;
    
    ps->filters = 0;
    ps->nfilters = 0;
    ps->filterAliases = 0;
    ps->nfilterAliases = 0;

    ps->CloseScreen = pScreen->CloseScreen;
    ps->DestroyWindow = pScreen->DestroyWindow;
    ps->StoreColors = pScreen->StoreColors;
    pScreen->DestroyWindow = PictureDestroyWindow;
    pScreen->CloseScreen = PictureCloseScreen;
    pScreen->StoreColors = PictureStoreColors;

    if (!PictureSetDefaultFilters (pScreen))
    {
	PictureResetFilters (pScreen);
	SetPictureScreen(pScreen, 0);
	free (formats);
	free (ps);
	return FALSE;
    }

    return TRUE;
}
#endif /* DO_WE_NEED_THESE_FUNCTIONS */

void
IcImageInit (IcImage *image)
{
    XRectangle rect;

    image->refcnt = 1;
    image->repeat = 0;
    image->graphicsExposures = FALSE;
    image->subWindowMode = ClipByChildren;
    image->polyEdge = PolyEdgeSharp;
    image->polyMode = PolyModePrecise;
    image->freeCompClip = FALSE;
    image->clientClipType = CT_NONE;
    image->componentAlpha = FALSE;

    image->alphaMap = 0;
    image->alphaOrigin.x = 0;
    image->alphaOrigin.y = 0;

    image->clipOrigin.x = 0;
    image->clipOrigin.y = 0;
    image->clientClip = 0;

    image->dither = None;

    image->stateChanges = (1 << (CPLastBit+1)) - 1;
/* XXX: What to lodge here?
    image->serialNumber = GC_CHANGE_SERIAL_BIT;
*/

    image->pCompositeClip = XCreateRegion();
    rect.x = 0;
    rect.y = 0;
    rect.width = image->pixels->width;
    rect.height = image->pixels->height;
    XUnionRectWithRegion (&rect, image->pCompositeClip, image->pCompositeClip);

    image->transform = NULL;

/* XXX: Need to track down and include this function
    image->filter = PictureGetFilterId (FilterNearest, -1, TRUE);
*/
    image->filter_params = 0;
    image->filter_nparams = 0;


    image->owns_pixels = 0;
}

/* XXX: Do we need this?
PicturePtr
AllocatePicture (ScreenPtr  pScreen)
{
    PictureScreenPtr	ps = GetPictureScreen(pScreen);
    PicturePtr		pPicture;
    char		*ptr;
    DevUnion		*ppriv;
    unsigned int    	*sizes;
    unsigned int    	size;
    int			i;

    pPicture = (PicturePtr) xalloc (ps->totalPictureSize);
    if (!pPicture)
	return 0;
    ppriv = (DevUnion *)(pPicture + 1);
    pPicture->devPrivates = ppriv;
    sizes = ps->PicturePrivateSizes;
    ptr = (char *)(ppriv + ps->PicturePrivateLen);
    for (i = ps->PicturePrivateLen; --i >= 0; ppriv++, sizes++)
    {
	if ( (size = *sizes) )
	{
	    ppriv->ptr = (pointer)ptr;
	    ptr += size;
	}
	else
	    ppriv->ptr = (pointer)NULL;
    }
    return pPicture;
}
*/

IcImage *
IcImageCreate (IcFormat		*format,
	       unsigned short	width,
	       unsigned short	height,
	       Mask		vmask,
	       XID		*vlist,
	       int		*error,
	       int		*error_value)
{
    IcImage	*image;
    IcPixels	*pixels;

    pixels = IcPixelsCreate (width, height, format->depth);
    if (pixels == NULL) {
	*error = BadAlloc;
	*error_value = 0; /* XXX: What should this be? */
	return NULL;
    }
    
    image = IcImageCreateForPixels (pixels, format, vmask, vlist, error, error_value);

    image->owns_pixels = 1;

    return image;
}

IcImage *
IcImageCreateForPixels (IcPixels	*pixels,
			IcFormat	*format,
			Mask		vmask,
			XID		*vlist,
			int		*error,
			int		*error_value)
{
    IcImage		*image;

    image = malloc (sizeof (IcImage));
    if (!image)
    {
	*error = BadAlloc;
	*error_value = 0; /* XXX: What should this be? */
	return NULL;
    }

    image->pixels = pixels;
    image->image_format = format;
    image->format = format->format;
/* XXX: What's all this about?
    if (pDrawable->type == DRAWABLE_PIXMAP)
    {
	++((PixmapPtr)pDrawable)->refcnt;
	image->pNext = 0;
    }
    else
    {
	image->pNext = GetPictureWindow(((WindowPtr) pDrawable));
	SetPictureWindow(((WindowPtr) pDrawable), image);
    }
*/

    IcImageInit (image);
    
    if (vmask)
	;
	/* XXX: Need to finish porting this function
	*error = IcImageChange (image, vmask, vlist, 0, error_value);
	*/
    else
	*error = Success;
    if (*error != Success)
    {
	IcImageDestroy (image);
	image = 0;
    }
    return image;
}

/* XXX: Need to decide what to do with this
#define NEXT_VAL(_type) (vlist ? (_type) *vlist++ : (_type) ulist++->val)

#define NEXT_PTR(_type) ((_type) ulist++->ptr)

int
IcImageChange (IcImage		*image,
	       Mask		vmask,
	       XID		*vlist,
	       DevUnion		*ulist,
	       int		*error_value)
{
    BITS32		index2;
    int			error = 0;
    BITS32		maskQ;
    
    maskQ = vmask;
    while (vmask && !error)
    {
	index2 = (BITS32) lowbit (vmask);
	vmask &= ~index2;
	image->stateChanges |= index2;
	switch (index2)
	{
	case CPRepeat:
	    {
		unsigned int	newr;
		newr = NEXT_VAL(unsigned int);
		if (newr <= xTrue)
		    image->repeat = newr;
		else
		{
		    *error_value = newr;
		    error = BadValue;
		}
	    }
	    break;
	case CPAlphaMap:
	    {
		IcImage *iAlpha;
		
		iAlpha = NEXT_PTR(IcImage *);
		if (iAlpha)
		    iAlpha->refcnt++;
		if (image->alphaMap)
		    IcImageDestroy ((pointer) image->alphaMap);
		image->alphaMap = iAlpha;
	    }
	    break;
	case CPAlphaXOrigin:
	    image->alphaOrigin.x = NEXT_VAL(INT16);
	    break;
	case CPAlphaYOrigin:
	    image->alphaOrigin.y = NEXT_VAL(INT16);
	    break;
	case CPClipXOrigin:
	    image->clipOrigin.x = NEXT_VAL(INT16);
	    break;
	case CPClipYOrigin:
	    image->clipOrigin.y = NEXT_VAL(INT16);
	    break;
	case CPClipMask:
	    {
		IcImage	    *mask;
		int	    clipType;

		mask = NEXT_PTR(IcImage *);
		if (mask) {
		    clipType = CT_PIXMAP;
		    mask->refcnt++;
		} else {
		    clipType = CT_NONE;
		}
		error = IcImageChangeClip (image, clipType,
					   (pointer)mask, 0);
		break;
	    }
	case CPGraphicsExposure:
	    {
		unsigned int	newe;
		newe = NEXT_VAL(unsigned int);
		if (newe <= xTrue)
		    image->graphicsExposures = newe;
		else
		{
		    *error_value = newe;
		    error = BadValue;
		}
	    }
	    break;
	case CPSubwindowMode:
	    {
		unsigned int	news;
		news = NEXT_VAL(unsigned int);
		if (news == ClipByChildren || news == IncludeInferiors)
		    image->subWindowMode = news;
		else
		{
		    *error_value = news;
		    error = BadValue;
		}
	    }
	    break;
	case CPPolyEdge:
	    {
		unsigned int	newe;
		newe = NEXT_VAL(unsigned int);
		if (newe == PolyEdgeSharp || newe == PolyEdgeSmooth)
		    image->polyEdge = newe;
		else
		{
		    *error_value = newe;
		    error = BadValue;
		}
	    }
	    break;
	case CPPolyMode:
	    {
		unsigned int	newm;
		newm = NEXT_VAL(unsigned int);
		if (newm == PolyModePrecise || newm == PolyModeImprecise)
		    image->polyMode = newm;
		else
		{
		    *error_value = newm;
		    error = BadValue;
		}
	    }
	    break;
	case CPDither:
	    image->dither = NEXT_VAL(Atom);
	    break;
	case CPComponentAlpha:
	    {
		unsigned int	newca;

		newca = NEXT_VAL (unsigned int);
		if (newca <= xTrue)
		    image->componentAlpha = newca;
		else
		{
		    *error_value = newca;
		    error = BadValue;
		}
	    }
	    break;
	default:
	    *error_value = maskQ;
	    error = BadValue;
	    break;
	}
    }
    return error;
}
*/

/* XXX: Do we need this?
int
SetPictureClipRects (PicturePtr	pPicture,
		     int	xOrigin,
		     int	yOrigin,
		     int	nRect,
		     xRectangle	*rects)
{
    ScreenPtr		pScreen = pPicture->pDrawable->pScreen;
    PictureScreenPtr	ps = GetPictureScreen(pScreen);
    RegionPtr		clientClip;
    int			result;

    clientClip = RECTS_TO_REGION(pScreen,
				 nRect, rects, CT_UNSORTED);
    if (!clientClip)
	return BadAlloc;
    result =(*ps->ChangePictureClip) (pPicture, CT_REGION, 
				      (pointer) clientClip, 0);
    if (result == Success)
    {
	pPicture->clipOrigin.x = xOrigin;
	pPicture->clipOrigin.y = yOrigin;
	pPicture->stateChanges |= CPClipXOrigin|CPClipYOrigin|CPClipMask;
	pPicture->serialNumber |= GC_CHANGE_SERIAL_BIT;
    }
    return result;
}
*/

int
IcImageSetTransform (IcImage		*image,
		     IcTransform	*transform)
{
    static const IcTransform	identity = { {
	{ xFixed1, 0x00000, 0x00000 },
	{ 0x00000, xFixed1, 0x00000 },
	{ 0x00000, 0x00000, xFixed1 },
    } };

    if (transform && memcmp (transform, &identity, sizeof (IcTransform)) == 0)
	transform = 0;
    
    if (transform)
    {
	if (!image->transform)
	{
	    image->transform = malloc (sizeof (IcTransform));
	    if (!image->transform)
		return BadAlloc;
	}
	*image->transform = *transform;
    }
    else
    {
	if (image->transform)
	{
	    free (image->transform);
	    image->transform = 0;
	}
    }
    return Success;
}

/* XXX: Do we need this? 
static void
ValidateOnePicture (IcImage *image)
{
    if (image->serialNumber != image->pDrawable->serialNumber)
    {
	PictureScreenPtr    ps = GetPictureScreen(image->pDrawable->pScreen);

	(*ps->ValidatePicture) (image, image->stateChanges);
	image->stateChanges = 0;
	image->serialNumber = image->pDrawable->serialNumber;
    }
}

void
ValidatePicture(PicturePtr pPicture)
{
    ValidateOnePicture (pPicture);
    if (pPicture->alphaMap)
	ValidateOnePicture (pPicture->alphaMap);
}

int
FreePicture (pointer	value,
	     XID	pid)
{
    PicturePtr	pPicture = (PicturePtr) value;

    if (--pPicture->refcnt == 0)
    {
	ScreenPtr	    pScreen = pPicture->pDrawable->pScreen;
	PictureScreenPtr    ps = GetPictureScreen(pScreen);
	
	if (pPicture->alphaMap)
	    FreePicture ((pointer) pPicture->alphaMap, (XID) 0);
	(*ps->DestroyPicture) (pPicture);
	(*ps->DestroyPictureClip) (pPicture);
	if (pPicture->transform)
	    free (pPicture->transform);
	if (pPicture->pDrawable->type == DRAWABLE_WINDOW)
	{
	    WindowPtr	pWindow = (WindowPtr) pPicture->pDrawable;
	    PicturePtr	*pPrev;

	    for (pPrev = (PicturePtr *) &((pWindow)->devPrivates[PictureWindowPrivateIndex].ptr);
		 *pPrev;
		 pPrev = &(*pPrev)->pNext)
	    {
		if (*pPrev == pPicture)
		{
		    *pPrev = pPicture->pNext;
		    break;
		}
	    }
	}
	else if (pPicture->pDrawable->type == DRAWABLE_PIXMAP)
	{
	    (*pScreen->DestroyPixmap) ((PixmapPtr)pPicture->pDrawable);
	}
	free (pPicture);
    }
    return Success;
}

int
FreePictFormat (pointer	pPictFormat,
		XID     pid)
{
    return Success;
}

void
CompositePicture (CARD8		op,
		  PicturePtr	pSrc,
		  PicturePtr	pMask,
		  PicturePtr	pDst,
		  INT16		xSrc,
		  INT16		ySrc,
		  INT16		xMask,
		  INT16		yMask,
		  INT16		xDst,
		  INT16		yDst,
		  CARD16	width,
		  CARD16	height)
{
    PictureScreenPtr	ps = GetPictureScreen(pDst->pDrawable->pScreen);
    
    ValidatePicture (pSrc);
    if (pMask)
	ValidatePicture (pMask);
    ValidatePicture (pDst);
    (*ps->Composite) (op,
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
		       height);
}

void
CompositeGlyphs (CARD8		op,
		 PicturePtr	pSrc,
		 PicturePtr	pDst,
		 PictFormatPtr	maskFormat,
		 INT16		xSrc,
		 INT16		ySrc,
		 int		nlist,
		 GlyphListPtr	lists,
		 GlyphPtr	*glyphs)
{
    PictureScreenPtr	ps = GetPictureScreen(pDst->pDrawable->pScreen);
    
    ValidatePicture (pSrc);
    ValidatePicture (pDst);
    (*ps->Glyphs) (op, pSrc, pDst, maskFormat, xSrc, ySrc, nlist, lists, glyphs);
}

void
CompositeRects (CARD8		op,
		PicturePtr	pDst,
		xRenderColor	*color,
		int		nRect,
		xRectangle      *rects)
{
    PictureScreenPtr	ps = GetPictureScreen(pDst->pDrawable->pScreen);
    
    ValidatePicture (pDst);
    (*ps->CompositeRects) (op, pDst, color, nRect, rects);
}

void
CompositeTrapezoids (CARD8	    op,
		     PicturePtr	    pSrc,
		     PicturePtr	    pDst,
		     PictFormatPtr  maskFormat,
		     INT16	    xSrc,
		     INT16	    ySrc,
		     int	    ntrap,
		     xTrapezoid	    *traps)
{
    PictureScreenPtr	ps = GetPictureScreen(pDst->pDrawable->pScreen);
    
    ValidatePicture (pSrc);
    ValidatePicture (pDst);
    (*ps->Trapezoids) (op, pSrc, pDst, maskFormat, xSrc, ySrc, ntrap, traps);
}

void
CompositeTriangles (CARD8	    op,
		    PicturePtr	    pSrc,
		    PicturePtr	    pDst,
		    PictFormatPtr   maskFormat,
		    INT16	    xSrc,
		    INT16	    ySrc,
		    int		    ntriangles,
		    xTriangle	    *triangles)
{
    PictureScreenPtr	ps = GetPictureScreen(pDst->pDrawable->pScreen);
    
    ValidatePicture (pSrc);
    ValidatePicture (pDst);
    (*ps->Triangles) (op, pSrc, pDst, maskFormat, xSrc, ySrc, ntriangles, triangles);
}

void
CompositeTriStrip (CARD8	    op,
		   PicturePtr	    pSrc,
		   PicturePtr	    pDst,
		   PictFormatPtr    maskFormat,
		   INT16	    xSrc,
		   INT16	    ySrc,
		   int		    npoints,
		   xPointFixed	    *points)
{
    PictureScreenPtr	ps = GetPictureScreen(pDst->pDrawable->pScreen);
    
    ValidatePicture (pSrc);
    ValidatePicture (pDst);
    (*ps->TriStrip) (op, pSrc, pDst, maskFormat, xSrc, ySrc, npoints, points);
}

void
CompositeTriFan (CARD8		op,
		 PicturePtr	pSrc,
		 PicturePtr	pDst,
		 PictFormatPtr	maskFormat,
		 INT16		xSrc,
		 INT16		ySrc,
		 int		npoints,
		 xPointFixed	*points)
{
    PictureScreenPtr	ps = GetPictureScreen(pDst->pDrawable->pScreen);
    
    ValidatePicture (pSrc);
    ValidatePicture (pDst);
    (*ps->TriFan) (op, pSrc, pDst, maskFormat, xSrc, ySrc, npoints, points);
}
*/

typedef xFixed_32_32	xFixed_48_16;

#define MAX_FIXED_48_16	    ((xFixed_48_16) 0x7fffffff)
#define MIN_FIXED_48_16	    (-((xFixed_48_16) 1 << 31))

/* XXX: Still need to port this
Bool
IcTransformPoint (IcTransform	*transform,
		  PictVectorPtr	vector)
{
    PictVector	    result;
    int		    i, j;
    xFixed_32_32    partial;
    xFixed_48_16    v;

    for (j = 0; j < 3; j++)
    {
	v = 0;
	for (i = 0; i < 3; i++)
	{
	    partial = ((xFixed_48_16) transform->matrix[j][i] * 
		       (xFixed_48_16) vector->vector[i]);
	    v += partial >> 16;
	}
	if (v > MAX_FIXED_48_16 || v < MIN_FIXED_48_16)
	    return FALSE;
	result.vector[j] = (xFixed) v;
    }
    if (!result.vector[2])
	return FALSE;
    for (j = 0; j < 2; j++)
    {
	partial = (xFixed_48_16) result.vector[j] << 16;
	v = partial / result.vector[2];
	if (v > MAX_FIXED_48_16 || v < MIN_FIXED_48_16)
	    return FALSE;
	vector->vector[j] = (xFixed) v;
    }
    vector->vector[2] = xFixed1;
    return TRUE;
}

*/
