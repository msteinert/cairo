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

static void
_XrGStateSetCurrentPt(XrGState *gstate, double x, double y);

static XrStatus
_XrGStateClipAndCompositeTrapezoids(XrGState *gstate,
				    XrSurface *src,
				    XrOperator operator,
				    XrSurface *dst,
				    XrTraps *traps);

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
    gstate->pattern_offset.x = 0.0;
    gstate->pattern_offset.y = 0.0;

    gstate->clip.surface = NULL;

    gstate->alpha = 1.0;
    _XrColorInit(&gstate->color);

    /* 3780 PPM (~96DPI) is a good enough assumption until we get a surface */
    gstate->ppm = 3780;
    _XrGStateDefaultMatrix (gstate);

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
    _XrSurfaceReference(gstate->clip.surface);
    
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
    gstate->surface = NULL;

    XrSurfaceDestroy(gstate->solid);
    gstate->solid = NULL;

    XrSurfaceDestroy(gstate->pattern);
    gstate->pattern = NULL;

    XrSurfaceDestroy(gstate->clip.surface);
    gstate->clip.surface = NULL;

    _XrColorDeinit(&gstate->color);

    _XrMatrixFini(&gstate->ctm);
    _XrMatrixFini(&gstate->ctm_inverse);

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
    double scale;

    XrSurfaceDestroy (gstate->surface);

    gstate->surface = surface;
    _XrSurfaceReference (gstate->surface);

    scale = surface->ppm / gstate->ppm;
    _XrGStateScale (gstate, scale, scale);
    gstate->ppm = surface->ppm;

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
_XrGStateSetPattern (XrGState *gstate, XrSurface *pattern)
{
    XrSurfaceDestroy (gstate->pattern);

    gstate->pattern = pattern;
    _XrSurfaceReference (gstate->pattern);

    gstate->pattern_offset.x = 0;
    gstate->pattern_offset.y = 0;
    XrMatrixTransformPoint (&gstate->ctm,
			    &gstate->pattern_offset.x,
			    &gstate->pattern_offset.y);

    return XrStatusSuccess;
}

XrStatus
_XrGStateSetOperator(XrGState *gstate, XrOperator operator)
{
    gstate->operator = operator;

    return XrStatusSuccess;
}

XrOperator
_XrGStateGetOperator(XrGState *gstate)
{
    return gstate->operator;
}

XrStatus
_XrGStateSetRGBColor(XrGState *gstate, double red, double green, double blue)
{
    _XrColorSetRGB(&gstate->color, red, green, blue);

    XrSurfaceDestroy(gstate->pattern);
    gstate->pattern = NULL;

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

double
_XrGStateGetTolerance(XrGState *gstate)
{
    return gstate->tolerance;
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

double
_XrGStateGetLineWidth(XrGState *gstate)
{
    return gstate->line_width;
}

XrStatus
_XrGStateSetLineCap(XrGState *gstate, XrLineCap line_cap)
{
    gstate->line_cap = line_cap;

    return XrStatusSuccess;
}

XrLineCap
_XrGStateGetLineCap(XrGState *gstate)
{
    return gstate->line_cap;
}

XrStatus
_XrGStateSetLineJoin(XrGState *gstate, XrLineJoin line_join)
{
    gstate->line_join = line_join;

    return XrStatusSuccess;
}

XrLineJoin
_XrGStateGetLineJoin(XrGState *gstate)
{
    return gstate->line_join;
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

double
_XrGStateGetMiterLimit(XrGState *gstate)
{
    return gstate->miter_limit;
}

XrStatus
_XrGStateTranslate(XrGState *gstate, double tx, double ty)
{
    XrMatrix tmp;

    _XrMatrixSetTranslate(&tmp, tx, ty);
    _XrMatrixMultiplyIntoRight(&tmp, &gstate->ctm);

    _XrMatrixSetTranslate(&tmp, -tx, -ty);
    _XrMatrixMultiplyIntoLeft(&gstate->ctm_inverse, &tmp);

    return XrStatusSuccess;
}

XrStatus
_XrGStateScale(XrGState *gstate, double sx, double sy)
{
    XrMatrix tmp;

    _XrMatrixSetScale(&tmp, sx, sy);
    _XrMatrixMultiplyIntoRight(&tmp, &gstate->ctm);

    _XrMatrixSetScale(&tmp, 1/sx, 1/sy);
    _XrMatrixMultiplyIntoLeft(&gstate->ctm_inverse, &tmp);

    return XrStatusSuccess;
}

XrStatus
_XrGStateRotate(XrGState *gstate, double angle)
{
    XrMatrix tmp;

    _XrMatrixSetRotate(&tmp, angle);
    _XrMatrixMultiplyIntoRight(&tmp, &gstate->ctm);

    _XrMatrixSetRotate(&tmp, -angle);
    _XrMatrixMultiplyIntoLeft(&gstate->ctm_inverse, &tmp);

    return XrStatusSuccess;
}

XrStatus
_XrGStateConcatMatrix(XrGState *gstate,
		      XrMatrix *matrix)
{
    XrMatrix tmp;

    XrMatrixCopy(&tmp, matrix);
    _XrMatrixMultiplyIntoRight(&tmp, &gstate->ctm);

    XrMatrixInvert(&tmp);
    _XrMatrixMultiplyIntoLeft(&gstate->ctm_inverse, &tmp);

    return XrStatusSuccess;
}

XrStatus
_XrGStateSetMatrix(XrGState *gstate,
		   XrMatrix *matrix)
{
    XrStatus status;

    XrMatrixCopy(&gstate->ctm, matrix);

    XrMatrixCopy(&gstate->ctm_inverse, matrix);
    status = XrMatrixInvert (&gstate->ctm_inverse);
    if (status)
	return status;

    return XrStatusSuccess;
}

XrStatus
_XrGStateDefaultMatrix(XrGState *gstate)
{
#define XR_GSTATE_DEFAULT_PPM 3780.0

    int scale = gstate->ppm / XR_GSTATE_DEFAULT_PPM + 0.5;
    if (scale == 0)
	scale = 1;

    XrMatrixSetIdentity (&gstate->ctm);
    XrMatrixScale (&gstate->ctm, scale, scale);
    XrMatrixCopy (&gstate->ctm_inverse, &gstate->ctm);
    XrMatrixInvert (&gstate->ctm_inverse);

    return XrStatusSuccess;
}

XrStatus
_XrGStateIdentityMatrix(XrGState *gstate)
{
    XrMatrixSetIdentity(&gstate->ctm);
    XrMatrixSetIdentity(&gstate->ctm_inverse);

    return XrStatusSuccess;
}

XrStatus
_XrGStateTransformPoint (XrGState *gstate, double *x, double *y)
{
    XrMatrixTransformPoint (&gstate->ctm, x, y);

    return XrStatusSuccess;
}

XrStatus
_XrGStateTransformDistance (XrGState *gstate, double *dx, double *dy)
{
    XrMatrixTransformDistance (&gstate->ctm, dx, dy);

    return XrStatusSuccess;
}

XrStatus
_XrGStateInverseTransformPoint (XrGState *gstate, double *x, double *y)
{
    XrMatrixTransformPoint (&gstate->ctm_inverse, x, y);

    return XrStatusSuccess;
}

XrStatus
_XrGStateInverseTransformDistance (XrGState *gstate, double *dx, double *dy)
{
    XrMatrixTransformDistance (&gstate->ctm_inverse, dx, dy);

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

    XrMatrixTransformPoint(&gstate->ctm, &x, &y);

    status = _XrPathMoveTo(&gstate->path, x, y);

    _XrGStateSetCurrentPt(gstate, x, y);

    gstate->last_move_pt = gstate->current_pt;

    return status;
}

XrStatus
_XrGStateLineTo(XrGState *gstate, double x, double y)
{
    XrStatus status;

    XrMatrixTransformPoint(&gstate->ctm, &x, &y);

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

    XrMatrixTransformPoint(&gstate->ctm, &x1, &y1);
    XrMatrixTransformPoint(&gstate->ctm, &x2, &y2);
    XrMatrixTransformPoint(&gstate->ctm, &x3, &y3);

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

    XrMatrixTransformDistance(&gstate->ctm, &dx, &dy);

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

    XrMatrixTransformDistance(&gstate->ctm, &dx, &dy);

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

    XrMatrixTransformDistance(&gstate->ctm, &dx1, &dy1);
    XrMatrixTransformDistance(&gstate->ctm, &dx2, &dy2);
    XrMatrixTransformDistance(&gstate->ctm, &dx3, &dy3);

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
_XrGStateGetCurrentPoint(XrGState *gstate, double *x, double *y)
{
    *x = gstate->current_pt.x;
    *y = gstate->current_pt.y;

    XrMatrixTransformPoint(&gstate->ctm_inverse, x, y);

    return XrStatusSuccess;
}

XrStatus
_XrGStateStroke(XrGState *gstate)
{
    XrStatus status;

    XrTraps traps;

    _XrPenInit(&gstate->pen_regular, gstate->line_width / 2.0, gstate);

    _XrTrapsInit(&traps);

    status = _XrPathStrokeToTraps(&gstate->path, gstate, &traps);
    if (status) {
	_XrTrapsDeinit(&traps);
	return status;
    }

    _XrGStateClipAndCompositeTrapezoids(gstate,
					gstate->pattern ? gstate->pattern : gstate->solid,
					gstate->operator,
					gstate->surface,
					&traps);

    _XrTrapsDeinit(&traps);

    _XrGStateNewPath(gstate);

    return XrStatusSuccess;
}

static XrStatus
_XrGStateClipAndCompositeTrapezoids(XrGState *gstate,
				    XrSurface *src,
				    XrOperator operator,
				    XrSurface *dst,
				    XrTraps *traps)
{
    if (traps->num_xtraps == 0)
	return XrStatusSuccess;

    if (gstate->clip.surface) {
	XrSurface *intermediate, *white;

	white = XrSurfaceCreateNextToSolid(gstate->surface, XrFormatA8,
					   1, 1,
					   1.0, 1.0, 1.0, 1.0);
	XrSurfaceSetRepeat(white, 1);

	intermediate = XrSurfaceCreateNextToSolid(gstate->clip.surface,
						  XrFormatA8,
						  gstate->clip.width, gstate->clip.height,
						  0.0, 0.0, 0.0, 0.0);
	XcCompositeTrapezoids(XrOperatorAdd,
			      white->xc_surface,
			      intermediate->xc_surface,
			      0, 0,
			      traps->xtraps,
			      traps->num_xtraps);
	XcComposite(XrOperatorIn,
		    gstate->clip.surface->xc_surface,
		    NULL,
		    intermediate->xc_surface,
		    0, 0, 0, 0, 0, 0,
		    gstate->clip.width, gstate->clip.height);
	XcComposite(operator,
		    src->xc_surface,
		    intermediate->xc_surface,
		    dst->xc_surface,
		    0, 0,
		    0, 0,
		    0, 0,
		    gstate->clip.width,
		    gstate->clip.height);
	XrSurfaceDestroy(intermediate);
	XrSurfaceDestroy (white);

    } else {
	double xoff, yoff;

	if (traps->xtraps[0].left.p1.y < traps->xtraps[0].left.p2.y) {
	    xoff = traps->xtraps[0].left.p1.x;
	    yoff = traps->xtraps[0].left.p1.y;
	} else {
	    xoff = traps->xtraps[0].left.p2.x;
	    yoff = traps->xtraps[0].left.p2.y;
	}

	XcCompositeTrapezoids(gstate->operator,
			      src->xc_surface,
			      dst->xc_surface,
			      XFixedToDouble(xoff) - gstate->pattern_offset.x,
			      XFixedToDouble(yoff) - gstate->pattern_offset.y,
			      traps->xtraps,
			      traps->num_xtraps);
    }

    return XrStatusSuccess;
}

XrStatus
_XrGStateFill(XrGState *gstate)
{
    XrStatus status;
    XrTraps traps;

    _XrTrapsInit(&traps);

    status = _XrPathFillToTraps(&gstate->path, gstate, &traps);
    if (status) {
	_XrTrapsDeinit(&traps);
	return status;
    }

    _XrGStateClipAndCompositeTrapezoids(gstate,
					gstate->pattern ? gstate->pattern : gstate->solid,
					gstate->operator,
					gstate->surface,
					&traps);
    _XrTrapsDeinit(&traps);

    _XrGStateNewPath(gstate);

    return XrStatusSuccess;
}

XrStatus
_XrGStateClip(XrGState *gstate)
{
    XrStatus status;
    XrSurface *alpha_one;
    XrTraps traps;

    if (gstate->clip.surface == NULL) {
	double x1, y1, x2, y2;
	_XrPathBounds(&gstate->path,
		      &x1, &y1, &x2, &y2);
	gstate->clip.x = floor(x1);
	gstate->clip.y = floor(y1);
	gstate->clip.width = ceil(x2 - gstate->clip.x);
	gstate->clip.height = ceil(y2 - gstate->clip.y);
	gstate->clip.surface = XrSurfaceCreateNextToSolid(gstate->surface,
							  XrFormatA8,
							  gstate->clip.width,
							  gstate->clip.height,
							  1.0, 1.0, 1.0, 1.0);
    }

    alpha_one = XrSurfaceCreateNextToSolid(gstate->surface, XrFormatA8,
					   1, 1,
					   0.0, 0.0, 0.0, 1.0);
    XrSurfaceSetRepeat(alpha_one, 1);

    _XrTrapsInit(&traps);
    status = _XrPathFillToTraps(&gstate->path, gstate, &traps);
    if (status) {
	_XrTrapsDeinit(&traps);
	return status;
    }

    _XrGStateClipAndCompositeTrapezoids(gstate,
					alpha_one,
					XrOperatorIn,
					gstate->clip.surface,
					&traps);

    _XrTrapsDeinit(&traps);

    XrSurfaceDestroy (alpha_one);

    return status;
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
    /*      (also, this means text clipping isn't working. Basically text is broken.) */
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
_XrGStateShowSurface(XrGState	*gstate,
		     XrSurface	*surface,
		     int	width,
		     int	height)
{
    XrSurface *mask;
    XrMatrix user_to_image, image_to_user;
    XrMatrix image_to_device, device_to_image;
    double device_x, device_y;
    double device_width, device_height;

    mask = XrSurfaceCreateNextToSolid (gstate->surface,
				       XrFormatA8,
				       1, 1,
				       1.0, 1.0, 1.0,
				       gstate->alpha);
    if (mask == NULL)
	return XrStatusNoMemory;

    XrSurfaceSetRepeat (mask, 1);

    XrSurfaceGetMatrix (surface, &user_to_image);
    XrMatrixMultiply (&device_to_image, &gstate->ctm_inverse, &user_to_image);
    XrSurfaceSetMatrix (surface, &device_to_image);

    image_to_user = user_to_image;
    XrMatrixInvert (&image_to_user);
    XrMatrixMultiply (&image_to_device, &image_to_user, &gstate->ctm);

    device_x = 0;
    device_y = 0;
    device_width = width;
    device_height = height;
    _XrMatrixTransformBoundingBox(&image_to_device,
				  &device_x, &device_y,
				  &device_width, &device_height);
    
    /* XXX: The +2 here looks bogus to me */
    XcComposite(gstate->operator,
		surface->xc_surface,
		mask->xc_surface,
		gstate->surface->xc_surface,
		device_x, device_y,
		0, 0,
		device_x, device_y,
		device_width + 2,
		device_height + 2);

    XrSurfaceDestroy (mask);

    /* restore the matrix originally in the surface */
    XrSurfaceSetMatrix (surface, &user_to_image);

    return XrStatusSuccess;
}
