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

XrGState *
_XrGStateCreate()
{
    XrGState *gstate;

    gstate = malloc(sizeof(XrGState));

    if (gstate)
	_XrGStateInit(gstate);

    return gstate;
}

void
_XrGStateInit(XrGState *gstate)
{
    gstate->operator = XR_GSTATE_OPERATOR_DEFAULT;

    gstate->tolerance = XR_GSTATE_TOLERANCE_DEFAULT;

    gstate->line_width = XR_GSTATE_LINE_WIDTH_DEFAULT;
    gstate->line_cap = XR_GSTATE_LINE_CAP_DEFAULT;
    gstate->line_join = XR_GSTATE_LINE_JOIN_DEFAULT;
    gstate->miter_limit = XR_GSTATE_MITER_LIMIT_DEFAULT;

    gstate->fill_rule = XR_GSTATE_FILL_RULE_DEFAULT;

    gstate->dash = NULL;
    gstate->num_dashes = 0;
    gstate->dash_offset = 0.0;

    _XrFontInit(&gstate->font);

    gstate->surface = NULL;
    gstate->solid = NULL;
    gstate->pattern = NULL;
    gstate->mask = NULL;

    gstate->alpha = 1.0;
    _XrColorInit(&gstate->color);

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
    if (other->dash) {
	gstate->dash = malloc (other->num_dashes * sizeof (double));
	if (gstate->dash == NULL)
	    return XrStatusNoMemory;
	memcpy(gstate->dash, other->dash, other->num_dashes * sizeof (double));
    }
    
    status = _XrFontInitCopy(&gstate->font, &other->font);
    if (status)
	goto CLEANUP_DASHES;

    _XrSurfaceReference(gstate->surface);
    _XrSurfaceReference(gstate->solid);
    _XrSurfaceReference(gstate->pattern);
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
    free (gstate->dash);
    gstate->dash = NULL;

    return status;
}

void
_XrGStateDeinit(XrGState *gstate)
{
    _XrFontDeinit(&gstate->font);

    XrSurfaceDestroy(gstate->surface);
    XrSurfaceDestroy(gstate->solid);
    XrSurfaceDestroy(gstate->pattern);
    XrSurfaceDestroy(gstate->mask);

    _XrColorDeinit(&gstate->color);

    _XrTransformDeinit(&gstate->ctm);
    _XrTransformDeinit(&gstate->ctm_inverse);

    _XrPathDeinit(&gstate->path);

    _XrPenDeinit(&gstate->pen_regular);

    if (gstate->dash) {
	free (gstate->dash);
	gstate->dash = NULL;
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
/* XXX: Rethinking this API
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

    gstate->surface = XrSurfaceCreate(gstate->dpy);
    if (gstate->surface == NULL)
	return XrStatusNoMemory;

    _XrSurfaceSetDrawableWH(gstate->surface, pix, width, height);

    _XrColorInit(&clear);
    _XrColorSetAlpha(&clear, 0);

    XcFillRectangle(XrOperatorSrc,
		    _XrSurfaceGetXcSurface(gstate->surface),
		    &clear.xc_color,
		    0, 0,
		    _XrSurfaceGetWidth(gstate->surface),
		    _XrSurfaceGetHeight(gstate->surface));

    return XrStatusSuccess;
}
*/

/* Complete the current offscreen group, composing its contents onto the parent surface. */
/* XXX: Rethinking this API
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

    * XXX: This could be made much more efficient by using
       _XrSurfaceGetDamagedWidth/Height if XrSurface actually kept
       track of such informaton. *
    XcComposite(gstate->operator,
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

    XrSurfaceDestroy(gstate->surface);
    gstate->surface = gstate->parent_surface;
    gstate->parent_surface = NULL;

    return XrStatusSuccess;
}
*/

XrStatus
_XrGStateSetTargetSurface (XrGState *gstate, XrSurface *surface)
{
    XrSurfaceDestroy (gstate->surface);

    gstate->surface = surface;
    _XrSurfaceReference (surface);

    return XrStatusSuccess;
}

/* XXX: Need to decide the memory mangement semantics of this
   function. Should it reference the surface again? */
XrSurface *
_XrGStateGetTargetSurface (XrGState *gstate)
{
    if (gstate == NULL)
	return NULL;

    return gstate->surface;
}

XrStatus
_XrGStateSetTargetDrawable (XrGState	*gstate,
			    Display	*dpy,
			    Drawable	drawable,
			    Visual	*visual,
			    XrFormat	format)
{
    XrStatus status;
    XrSurface *surface;

    surface = XrSurfaceCreateForDrawable (dpy, drawable,
					  visual,
					  format,
					  DefaultColormap (dpy, DefaultScreen (dpy)));
    if (surface == NULL)
	return XrStatusNoMemory;

    status = _XrGStateSetTargetSurface (gstate, surface);

    XrSurfaceDestroy (surface);

    return status;
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

    XrSurfaceDestroy(gstate->solid);
    gstate->solid = XrSurfaceCreateNextToSolid (gstate->surface, XrFormatARGB32,
						1, 1,
						red, green, blue,
						gstate->alpha);
    XrSurfaceSetRepeat (gstate->solid, 1);

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

    XrSurfaceDestroy (gstate->solid);
    gstate->solid = XrSurfaceCreateNextToSolid (gstate->surface,
						XrFormatARGB32,
						1, 1,
						gstate->color.red,
						gstate->color.green,
						gstate->color.blue,
						gstate->color.alpha);
    XrSurfaceSetRepeat (gstate->solid, 1);

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
_XrGStateSetDash(XrGState *gstate, double *dash, int num_dashes, double offset)
{
    if (gstate->dash) {
	free (gstate->dash);
	gstate->dash = NULL;
    }
    
    gstate->num_dashes = num_dashes;
    if (gstate->num_dashes) {
	gstate->dash = malloc (gstate->num_dashes * sizeof (double));
	if (gstate->dash == NULL) {
	    gstate->num_dashes = 0;
	    return XrStatusNoMemory;
	}
    }

    memcpy (gstate->dash, dash, gstate->num_dashes * sizeof (double));
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

    _XrTransformInvert(&tmp);
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
    status = _XrTransformInvert (&gstate->ctm_inverse);
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
    XrPathCallbacks *cbs = gstate->dash ? &cb_dash : &cb;

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

    XcCompositeTrapezoids(gstate->operator,
			  gstate->pattern ? gstate->pattern->xc_surface : gstate->solid->xc_surface,
			  gstate->surface->xc_surface,
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

    XcCompositeTrapezoids(gstate->operator,
			  gstate->pattern ? gstate->pattern->xc_surface : gstate->solid->xc_surface,
			  gstate->surface->xc_surface,
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

    /* XXX: Need to abandon Xft and use Xc instead */
    /*      (until I do, this call will croak on IcImage XrSurfaces */
    XftTextExtentsUtf8(gstate->surface->dpy,
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

    /* XXX: Need to abandon Xft and use Xc instead */
    /*      (until I do, this call will croak on IcImage XrSurfaces */
    XftTextRenderUtf8(gstate->surface->dpy,
		      gstate->operator,
		      _XrSurfaceGetPicture (gstate->solid),
		      xft_font,
		      _XrSurfaceGetPicture (gstate->surface),
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
    XrSurface *image_surface, *mask;
    XrTransform user_to_image, image_to_user;
    XrTransform image_to_device, device_to_image;
    double dst_x, dst_y;
    double dst_width, dst_height;

    mask = XrSurfaceCreateNextToSolid (gstate->surface,
				       XrFormatA8,
				       1, 1,
				       1.0, 1.0, 1.0,
				       gstate->alpha);
    if (mask == NULL)
	return XrStatusNoMemory;
    XrSurfaceSetRepeat (mask, 1);

    image_surface = XrSurfaceCreateNextTo (gstate->surface, format, width, height);
    if (image_surface == NULL) {
	XrSurfaceDestroy (mask);
	return XrStatusNoMemory;
    }

    /* XXX: Need a way to transfer bits to an XcSurface
    XcPutImage (image_surface->xc_surface, data, width, height, stride);
    */

    _XrTransformInitMatrix(&user_to_image, a, b, c, d, tx, ty);
    _XrTransformMultiply(&gstate->ctm_inverse, &user_to_image, &device_to_image);
    _XrSurfaceSetTransform(image_surface, &device_to_image);

    image_to_user = user_to_image;
    _XrTransformInvert (&image_to_user);
    _XrTransformMultiply (&image_to_user, &gstate->ctm, &image_to_device);

    dst_x = 0;
    dst_y = 0;
    dst_width = width;
    dst_height = height;
    _XrTransformBoundingBox(&image_to_device,
			    &dst_x, &dst_y,
			    &dst_width, &dst_height);

    XcComposite(gstate->operator,
		image_surface->xc_surface,
		mask->xc_surface,
		gstate->surface->xc_surface,
		dst_x, dst_y,
		0, 0,
		dst_x, dst_y,
		dst_width + 2,
		dst_height + 2);

    XrSurfaceDestroy (image_surface);
    XrSurfaceDestroy (mask);

    return XrStatusSuccess;
}

XrStatus
_XrGStateShowSurface(XrGState	*gstate,
		     XrSurface	*surface,
		     int	x,
		     int	y,
		     int	width,
		     int	height)
{
    XrSurface *mask;

    mask = XrSurfaceCreateNextToSolid (gstate->surface,
				       XrFormatARGB32,
				       1, 1,
				       1.0, 1.0, 1.0,
				       gstate->alpha);
    if (mask == NULL)
	return XrStatusNoMemory;

    XrSurfaceSetRepeat (mask, 1);

    XcComposite (gstate->operator,
		 surface->xc_surface,
		 mask->xc_surface,
		 gstate->surface->xc_surface,
		 x, y,
		 0, 0,
		 x, y,
		 width,
		 height);

    XrSurfaceDestroy (mask);

    return XrStatusSuccess;
}
