/*
 * $XFree86: $
 *
 * Copyright © 2002 Carl D. Worth
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of Carl
 * D. Worth not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission.  Carl D. Worth makes no representations about the
 * suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * CARL D. WORTH DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL CARL D. WORTH BE LIABLE FOR ANY SPECIAL,
 * INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
 * IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdlib.h>
#include <math.h>

#include "xrint.h"

static Picture
_XrGStateGetPicture(XrGState *gstate);

static Picture
_XrGStateGetSrcPicture(XrGState *gstate);

XrGState *
_XrGStateCreate(Display *dpy)
{
    XrGState *gstate;

    gstate = malloc(sizeof(XrGState));

    if (gstate)
	_XrGStateInit(gstate, dpy);

    return gstate;
}

void
_XrGStateInit(XrGState *gstate, Display *dpy)
{
    gstate->dpy = dpy;

    gstate->operator = XR_GSTATE_OPERATOR_DEFAULT;

    gstate->tolerance = XR_GSTATE_TOLERANCE_DEFAULT;

    gstate->line_width = XR_GSTATE_LINE_WIDTH_DEFAULT;
    gstate->line_cap = XR_GSTATE_LINE_CAP_DEFAULT;
    gstate->line_join = XR_GSTATE_LINE_JOIN_DEFAULT;
    gstate->miter_limit = XR_GSTATE_MITER_LIMIT_DEFAULT;

    gstate->fill_rule = XR_GSTATE_FILL_RULE_DEFAULT;

    gstate->dashes = 0;
    gstate->ndashes = 0;
    gstate->dash_offset = 0.0;

    gstate->alphaFormat = XcFindStandardFormat(dpy, PictStandardA8);

    _XrFontInit(&gstate->font);

    gstate->parent_surface = NULL;
    gstate->surface = _XrSurfaceCreate(dpy);
    gstate->src = _XrSurfaceCreate(dpy);
    gstate->mask = NULL;

    gstate->alpha = 1.0;
    _XrColorInit(&gstate->color);
    _XrSurfaceSetSolidColor(gstate->src, &gstate->color);

    _XrTransformInitIdentity(&gstate->ctm);
    _XrTransformInitIdentity(&gstate->ctm_inverse);

    _XrPathInit(&gstate->path);

    gstate->has_current_pt = 0;

    _XrPenInitEmpty(&gstate->pen_regular);

    gstate->next = NULL;
}

XrStatus
_XrGStateInitCopy(XrGState *gstate, XrGState *other)
{
    XrStatus status;
    
    *gstate = *other;
    if (other->dashes) {
	gstate->dashes = malloc (other->ndashes * sizeof (double));
	if (gstate->dashes == NULL)
	    return XrStatusNoMemory;
	memcpy(gstate->dashes, other->dashes, other->ndashes * sizeof (double));
    }
    
    status = _XrFontInitCopy(&gstate->font, &other->font);
    if (status)
	goto CLEANUP_DASHES;

    gstate->parent_surface = NULL;
    _XrSurfaceReference(gstate->surface);
    _XrSurfaceReference(gstate->src);
    if (gstate->mask)
	_XrSurfaceReference(gstate->mask);
    
    status = _XrPathInitCopy(&gstate->path, &other->path);
    if (status)
	goto CLEANUP_FONT;

    status = _XrPenInitCopy(&gstate->pen_regular, &other->pen_regular);
    if (status)
	goto CLEANUP_PATH;

    return status;

  CLEANUP_PATH:
    _XrPathDeinit(&gstate->path);
  CLEANUP_FONT:
    _XrFontDeinit(&gstate->font);
  CLEANUP_DASHES:
    free (gstate->dashes);
    gstate->dashes = NULL;

    return status;
}

void
_XrGStateDeinit(XrGState *gstate)
{
    if (gstate->parent_surface)
	_XrGStateEndGroup(gstate);

    _XrFontDeinit(&gstate->font);

    _XrSurfaceDereferenceDestroy(gstate->surface);
    _XrSurfaceDereferenceDestroy(gstate->src);
    if (gstate->mask)
	_XrSurfaceDereferenceDestroy(gstate->mask);

    _XrColorDeinit(&gstate->color);

    _XrTransformDeinit(&gstate->ctm);
    _XrTransformDeinit(&gstate->ctm_inverse);

    _XrPathDeinit(&gstate->path);

    _XrPenDeinit(&gstate->pen_regular);

    if (gstate->dashes) {
	free (gstate->dashes);
	gstate->dashes = NULL;
    }
}

void
_XrGStateDestroy(XrGState *gstate)
{
    _XrGStateDeinit(gstate);
    free(gstate);
}

XrGState*
_XrGStateClone(XrGState *gstate)
{
    XrStatus status;
    XrGState *clone;

    clone = malloc(sizeof(XrGState));
    if (clone) {
	status = _XrGStateInitCopy(clone, gstate);
	if (status) {
	    free(clone);
	    return NULL;
	}
    }

    return clone;
}

/* Push rendering off to an off-screen group. */
XrStatus
_XrGStateBeginGroup(XrGState *gstate)
{
    Pixmap pix;
    XrColor clear;
    unsigned int width, height;

    gstate->parent_surface = gstate->surface;

    width = _XrSurfaceGetWidth(gstate->surface);
    height = _XrSurfaceGetHeight(gstate->surface);

    pix = XCreatePixmap(gstate->dpy,
			_XrSurfaceGetDrawable(gstate->surface),
			width, height,
			_XrSurfaceGetDepth(gstate->surface));
    if (pix == 0)
	return XrStatusNoMemory;

    gstate->surface = _XrSurfaceCreate(gstate->dpy);
    if (gstate->surface == NULL)
	return XrStatusNoMemory;

    _XrSurfaceSetDrawableWH(gstate->surface, pix, width, height);

    _XrColorInit(&clear);
    _XrColorSetAlpha(&clear, 0);

    XcFillRectangle(gstate->dpy,
		    XrOperatorSrc,
		    _XrSurfaceGetXcSurface(gstate->surface),
		    &clear.xc_color,
		    0, 0,
		    _XrSurfaceGetWidth(gstate->surface),
		    _XrSurfaceGetHeight(gstate->surface));

    return XrStatusSuccess;
}

/* Complete the current offscreen group, composing its contents onto the parent surface. */
XrStatus
_XrGStateEndGroup(XrGState *gstate)
{
    Pixmap pix;
    XrColor mask_color;
    XrSurface mask;

    if (gstate->parent_surface == NULL)
	return XrStatusInvalidPopGroup;

    _XrSurfaceInit(&mask, gstate->dpy);
    _XrColorInit(&mask_color);
    _XrColorSetAlpha(&mask_color, gstate->alpha);

    _XrSurfaceSetSolidColor(&mask, &mask_color);

    /* XXX: This could be made much more efficient by using
       _XrSurfaceGetDamagedWidth/Height if XrSurface actually kept
       track of such informaton. */
    XcComposite(gstate->dpy, gstate->operator,
		_XrSurfaceGetXcSurface(gstate->surface),
		_XrSurfaceGetXcSurface(&mask),
		_XrSurfaceGetXcSurface(gstate->parent_surface),
		0, 0,
		0, 0,
		0, 0,
		_XrSurfaceGetWidth(gstate->surface),
		_XrSurfaceGetHeight(gstate->surface));

    _XrSurfaceDeinit(&mask);

    pix = _XrSurfaceGetDrawable(gstate->surface);
    XFreePixmap(gstate->dpy, pix);

    _XrSurfaceDestroy(gstate->surface);
    gstate->surface = gstate->parent_surface;
    gstate->parent_surface = NULL;

    return XrStatusSuccess;
}

XrStatus
_XrGStateSetDrawable(XrGState *gstate, Drawable drawable)
{
    _XrSurfaceSetDrawable(gstate->surface, drawable);

    return XrStatusSuccess;
}

XrStatus
_XrGStateSetVisual(XrGState *gstate, Visual *visual)
{
    _XrSurfaceSetVisual(gstate->surface, visual);

    return XrStatusSuccess;
}

XrStatus
_XrGStateSetFormat(XrGState *gstate, XrFormat format)
{
    _XrSurfaceSetFormat(gstate->surface, format);

    return XrStatusSuccess;
}

XrStatus
_XrGStateSetOperator(XrGState *gstate, XrOperator operator)
{
    gstate->operator = operator;

    return XrStatusSuccess;
}

XrStatus
_XrGStateSetRGBColor(XrGState *gstate, double red, double green, double blue)
{
    _XrColorSetRGB(&gstate->color, red, green, blue);
    _XrSurfaceSetSolidColor(gstate->src, &gstate->color);

    return XrStatusSuccess;
}

XrStatus
_XrGStateSetTolerance(XrGState *gstate, double tolerance)
{
    gstate->tolerance = tolerance;

    return XrStatusSuccess;
}

XrStatus
_XrGStateSetAlpha(XrGState *gstate, double alpha)
{
    gstate->alpha = alpha;
    _XrColorSetAlpha(&gstate->color, alpha);
    _XrSurfaceSetSolidColor(gstate->src, &gstate->color);

    return XrStatusSuccess;
}

XrStatus
_XrGStateSetFillRule(XrGState *gstate, XrFillRule fill_rule)
{
    gstate->fill_rule = fill_rule;

    return XrStatusSuccess;
}

XrStatus
_XrGStateSetLineWidth(XrGState *gstate, double width)
{
    gstate->line_width = width;

    return XrStatusSuccess;
}

XrStatus
_XrGStateSetLineCap(XrGState *gstate, XrLineCap line_cap)
{
    gstate->line_cap = line_cap;

    return XrStatusSuccess;
}

XrStatus
_XrGStateSetLineJoin(XrGState *gstate, XrLineJoin line_join)
{
    gstate->line_join = line_join;

    return XrStatusSuccess;
}

XrStatus
_XrGStateSetDash(XrGState *gstate, double *dashes, int ndash, double offset)
{
    if (gstate->dashes) {
	free (gstate->dashes);
    }
    
    gstate->dashes = malloc (ndash * sizeof (double));
    if (!gstate->dashes) {
	gstate->ndashes = 0;
	return XrStatusNoMemory;
    }
    gstate->ndashes = ndash;
    memcpy (gstate->dashes, dashes, ndash * sizeof (double));
    gstate->dash_offset = offset;

    return XrStatusSuccess;
}

XrStatus
_XrGStateSetMiterLimit(XrGState *gstate, double limit)
{
    gstate->miter_limit = limit;

    return XrStatusSuccess;
}

XrStatus
_XrGStateTranslate(XrGState *gstate, double tx, double ty)
{
    XrTransform tmp;

    _XrTransformInitTranslate(&tmp, tx, ty);
    _XrTransformMultiplyIntoRight(&tmp, &gstate->ctm);

    _XrTransformInitTranslate(&tmp, -tx, -ty);
    _XrTransformMultiplyIntoLeft(&gstate->ctm_inverse, &tmp);

    return XrStatusSuccess;
}

XrStatus
_XrGStateScale(XrGState *gstate, double sx, double sy)
{
    XrTransform tmp;

    _XrTransformInitScale(&tmp, sx, sy);
    _XrTransformMultiplyIntoRight(&tmp, &gstate->ctm);

    _XrTransformInitScale(&tmp, 1/sx, 1/sy);
    _XrTransformMultiplyIntoLeft(&gstate->ctm_inverse, &tmp);

    return XrStatusSuccess;
}

XrStatus
_XrGStateRotate(XrGState *gstate, double angle)
{
    XrTransform tmp;

    _XrTransformInitRotate(&tmp, angle);
    _XrTransformMultiplyIntoRight(&tmp, &gstate->ctm);

    _XrTransformInitRotate(&tmp, -angle);
    _XrTransformMultiplyIntoLeft(&gstate->ctm_inverse, &tmp);

    return XrStatusSuccess;
}

XrStatus
_XrGStateConcatMatrix(XrGState *gstate,
		      double a, double b,
		      double c, double d,
		      double tx, double ty)
{
    XrTransform tmp;

    _XrTransformInitMatrix(&tmp, a, b, c, d, tx, ty);
    _XrTransformMultiplyIntoRight(&tmp, &gstate->ctm);

    _XrTransformComputeInverse(&tmp);
    _XrTransformMultiplyIntoLeft(&gstate->ctm_inverse, &tmp);

    return XrStatusSuccess;
}

XrStatus
_XrGStateSetMatrix(XrGState *gstate,
		   double a, double b,
		   double c, double d,
		   double tx, double ty)
{
    XrStatus status;

    _XrTransformInitMatrix(&gstate->ctm, a, b, c, d, tx, ty);

    gstate->ctm_inverse = gstate->ctm;
    status = _XrTransformComputeInverse(&gstate->ctm_inverse);
    if (status)
	return status;

    return XrStatusSuccess;
}

XrStatus
_XrGStateIdentityMatrix(XrGState *gstate)
{
    _XrTransformInitIdentity(&gstate->ctm);
    _XrTransformInitIdentity(&gstate->ctm_inverse);

    return XrStatusSuccess;
}

static void
_XrGStateSetCurrentPt(XrGState *gstate, double x, double y)
{
    gstate->current_pt.x = x;
    gstate->current_pt.y = y;

    gstate->has_current_pt = 1;
}

XrStatus
_XrGStateNewPath(XrGState *gstate)
{
    _XrPathDeinit(&gstate->path);
    gstate->has_current_pt = 0;

    return XrStatusSuccess;
}

XrStatus
_XrGStateMoveTo(XrGState *gstate, double x, double y)
{
    XrStatus status;

    _XrTransformPoint(&gstate->ctm, &x, &y);

    status = _XrPathMoveTo(&gstate->path, x, y);

    _XrGStateSetCurrentPt(gstate, x, y);

    gstate->last_move_pt = gstate->current_pt;

    return status;
}

XrStatus
_XrGStateLineTo(XrGState *gstate, double x, double y)
{
    XrStatus status;

    _XrTransformPoint(&gstate->ctm, &x, &y);

    status = _XrPathLineTo(&gstate->path, x, y);

    _XrGStateSetCurrentPt(gstate, x, y);

    return status;
}

XrStatus
_XrGStateCurveTo(XrGState *gstate,
		 double x1, double y1,
		 double x2, double y2,
		 double x3, double y3)
{
    XrStatus status;

    _XrTransformPoint(&gstate->ctm, &x1, &y1);
    _XrTransformPoint(&gstate->ctm, &x2, &y2);
    _XrTransformPoint(&gstate->ctm, &x3, &y3);

    status = _XrPathCurveTo(&gstate->path,
			    x1, y1,
			    x2, y2,
			    x3, y3);

    _XrGStateSetCurrentPt(gstate, x3, y3);

    return status;
}

XrStatus
_XrGStateRelMoveTo(XrGState *gstate, double dx, double dy)
{
    XrStatus status;
    double x, y;

    _XrTransformDistance(&gstate->ctm, &dx, &dy);

    x = gstate->current_pt.x + dx;
    y = gstate->current_pt.y + dy;

    status = _XrPathMoveTo(&gstate->path, x, y);

    _XrGStateSetCurrentPt(gstate, x, y);

    gstate->last_move_pt = gstate->current_pt;

    return status;
}

XrStatus
_XrGStateRelLineTo(XrGState *gstate, double dx, double dy)
{
    XrStatus status;
    double x, y;

    _XrTransformDistance(&gstate->ctm, &dx, &dy);

    x = gstate->current_pt.x + dx;
    y = gstate->current_pt.y + dy;

    status = _XrPathLineTo(&gstate->path, x, y);

    _XrGStateSetCurrentPt(gstate, x, y);

    return status;
}

XrStatus
_XrGStateRelCurveTo(XrGState *gstate,
		    double dx1, double dy1,
		    double dx2, double dy2,
		    double dx3, double dy3)
{
    XrStatus status;

    _XrTransformDistance(&gstate->ctm, &dx1, &dy1);
    _XrTransformDistance(&gstate->ctm, &dx2, &dy2);
    _XrTransformDistance(&gstate->ctm, &dx3, &dy3);

    status = _XrPathCurveTo(&gstate->path,
			    gstate->current_pt.x + dx1, gstate->current_pt.y + dy1,
			    gstate->current_pt.x + dx2, gstate->current_pt.y + dy2,
			    gstate->current_pt.x + dx3, gstate->current_pt.y + dy3);

    _XrGStateSetCurrentPt(gstate,
			  gstate->current_pt.x + dx3,
			  gstate->current_pt.y + dy3);

    return status;
}

XrStatus
_XrGStateClosePath(XrGState *gstate)
{
    XrStatus status;

    status = _XrPathClosePath(&gstate->path);

    _XrGStateSetCurrentPt(gstate,
			  gstate->last_move_pt.x, 
			  gstate->last_move_pt.y);

    return status;
}

XrStatus
_XrGStateStroke(XrGState *gstate)
{
    XrStatus status;

    static XrPathCallbacks cb = {
	_XrStrokerAddEdge,
	_XrStrokerAddSpline,
	_XrStrokerDoneSubPath,
	_XrStrokerDonePath
    };

    static XrPathCallbacks cb_dash = {
	_XrStrokerAddEdgeDashed,
	_XrStrokerAddSpline,
	_XrStrokerDoneSubPath,
	_XrStrokerDonePath
    };
    XrPathCallbacks *cbs = gstate->dashes ? &cb_dash : &cb;

    XrStroker stroker;
    XrTraps traps;

    _XrPenInit(&gstate->pen_regular, gstate->line_width / 2.0, gstate);

    _XrTrapsInit(&traps);
    _XrStrokerInit(&stroker, gstate, &traps);

    status = _XrPathInterpret(&gstate->path, XrPathDirectionForward, cbs, &stroker);
    if (status) {
	_XrStrokerDeinit(&stroker);
	_XrTrapsDeinit(&traps);
	return status;
    }

    XcCompositeTrapezoids(gstate->dpy, gstate->operator,
			  _XrSurfaceGetXcSurface(gstate->src),
			  _XrSurfaceGetXcSurface(gstate->surface),
			  gstate->alphaFormat,
			  0, 0,
			  traps.xtraps,
			  traps.num_xtraps);

    _XrStrokerDeinit(&stroker);
    _XrTrapsDeinit(&traps);

    _XrGStateNewPath(gstate);

    return XrStatusSuccess;
}

XrStatus
_XrGStateFill(XrGState *gstate)
{
    XrStatus status;
    static XrPathCallbacks cb = {
	_XrFillerAddEdge,
	_XrFillerAddSpline,
	_XrFillerDoneSubPath,
	_XrFillerDonePath
    };

    XrFiller filler;
    XrTraps traps;

    _XrTrapsInit(&traps);
    _XrFillerInit(&filler, gstate, &traps);

    status = _XrPathInterpret(&gstate->path, XrPathDirectionForward, &cb, &filler);
    if (status) {
	_XrFillerDeinit(&filler);
	_XrTrapsDeinit(&traps);
	return status;
    }

    XcCompositeTrapezoids(gstate->dpy, gstate->operator,
			  _XrSurfaceGetXcSurface(gstate->src),
			  _XrSurfaceGetXcSurface(gstate->surface),
			  gstate->alphaFormat,
			  0, 0,
			  traps.xtraps,
			  traps.num_xtraps);

    _XrFillerDeinit(&filler);
    _XrTrapsDeinit(&traps);

    _XrGStateNewPath(gstate);

    return XrStatusSuccess;
}

XrStatus
_XrGStateSelectFont(XrGState *gstate, const char *key)
{
    return _XrFontSelect(&gstate->font, key);
}

XrStatus
_XrGStateScaleFont(XrGState *gstate, double scale)
{
    return _XrFontScale(&gstate->font, scale);
}

XrStatus
_XrGStateTransformFont(XrGState *gstate,
		       double a, double b,
		       double c, double d)
{
    return _XrFontTransform(&gstate->font,
			    a, b, c, d);
}

XrStatus
_XrGStateTextExtents(XrGState *gstate,
		     const unsigned char *utf8,
		     double *x, double *y,
		     double *width, double *height,
		     double *dx, double *dy)
{
    XftFont *xft_font;
    XGlyphInfo extents;

    _XrFontResolveXftFont(&gstate->font, gstate, &xft_font);

    XftTextExtentsUtf8(gstate->dpy,
		       xft_font,
		       utf8,
		       strlen((char *) utf8),
		       &extents);

    /* XXX: What are the semantics of XftTextExtents? Specifically,
       what does it do with x/y? I think we actually need to use the
       gstate's current point in here somewhere. */
    *x = extents.x;
    *y = extents.y;
    *width = extents.width;
    *height = extents.height;
    *dx = extents.xOff;
    *dy = extents.yOff;

    return XrStatusSuccess;
}

XrStatus
_XrGStateShowText(XrGState *gstate, const unsigned char *utf8)
{
    XftFont *xft_font;

    if (gstate->has_current_pt == 0)
	return XrStatusNoCurrentPoint;

    _XrFontResolveXftFont(&gstate->font, gstate, &xft_font);

    XftTextRenderUtf8(gstate->dpy,
		      gstate->operator,
		      _XrGStateGetSrcPicture(gstate),
		      xft_font,
		      _XrGStateGetPicture(gstate),
		      0, 0,
		      gstate->current_pt.x,
		      gstate->current_pt.y,
		      utf8,
		      strlen((char *) utf8));

    return XrStatusSuccess;
}

XrStatus
_XrGStateShowImage(XrGState	*gstate,
		   char		*data,
		   XrFormat	format,
		   unsigned int	width,
		   unsigned int	height,
		   unsigned int	stride)
{
    return _XrGStateShowImageTransform(gstate,
				       data, format, width, height, stride,
				       width, 0,
				       0,     height,
				       0,     0);
}

XrStatus
_XrGStateShowImageTransform(XrGState		*gstate,
			    char		*data,
			    XrFormat		format,
			    unsigned int	width,
			    unsigned int	height,
			    unsigned int	stride,
			    double a, double b,
			    double c, double d,
			    double tx, double ty)
{
    XrStatus status;
    XrColor mask_color;
    XrSurface image_surface, mask;
    XrTransform user_to_image, image_to_user;
    XrTransform image_to_device, device_to_image;
    double dst_x, dst_y;
    double dst_width, dst_height;

    _XrSurfaceInit(&mask, gstate->dpy);
    _XrColorInit(&mask_color);
    _XrColorSetAlpha(&mask_color, gstate->alpha);

    _XrSurfaceSetSolidColor(&mask, &mask_color);

    _XrSurfaceInit(&image_surface, gstate->dpy);

    _XrSurfaceSetFormat(&image_surface, format);

    status = _XrSurfaceSetImage(&image_surface,	data,width, height, stride);
    if (status)
	return status;

    _XrTransformInitMatrix(&user_to_image, a, b, c, d, tx, ty);
    _XrTransformMultiply(&gstate->ctm_inverse, &user_to_image, &device_to_image);
    _XrSurfaceSetTransform(&image_surface, &device_to_image);

    image_to_user = user_to_image;
    _XrTransformComputeInverse(&image_to_user);
    _XrTransformMultiply(&image_to_user, &gstate->ctm, &image_to_device);

    dst_x = 0;
    dst_y = 0;
    dst_width = width;
    dst_height = height;
    _XrTransformBoundingBox(&image_to_device,
			    &dst_x, &dst_y,
			    &dst_width, &dst_height);

    XcComposite(gstate->dpy, gstate->operator,
		_XrSurfaceGetXcSurface(&image_surface),
		_XrSurfaceGetXcSurface(&mask),
		_XrSurfaceGetXcSurface(gstate->surface),
		dst_x, dst_y,
		0, 0,
		dst_x, dst_y,
		dst_width + 2,
		dst_height + 2);

    _XrSurfaceDeinit(&image_surface);
    _XrSurfaceDeinit(&mask);

    return XrStatusSuccess;
}

static Picture
_XrGStateGetPicture(XrGState *gstate)
{
    return _XrSurfaceGetPicture(gstate->surface);
}

static Picture
_XrGStateGetSrcPicture(XrGState *gstate)
{
    return _XrSurfaceGetPicture(gstate->src);
}
