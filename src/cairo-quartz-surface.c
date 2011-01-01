/* -*- Mode: c; c-basic-offset: 4; indent-tabs-mode: t; tab-width: 8; -*- */
/* cairo - a vector graphics library with display and print output
 *
 * Copyright � 2006, 2007 Mozilla Corporation
 *
 * This library is free software; you can redistribute it and/or
 * modify it either under the terms of the GNU Lesser General Public
 * License version 2.1 as published by the Free Software Foundation
 * (the "LGPL") or, at your option, under the terms of the Mozilla
 * Public License Version 1.1 (the "MPL"). If you do not alter this
 * notice, a recipient may use your version of this file under either
 * the MPL or the LGPL.
 *
 * You should have received a copy of the LGPL along with this library
 * in the file COPYING-LGPL-2.1; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA 02110-1335, USA
 * You should have received a copy of the MPL along with this library
 * in the file COPYING-MPL-1.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY
 * OF ANY KIND, either express or implied. See the LGPL or the MPL for
 * the specific language governing rights and limitations.
 *
 * The Original Code is the cairo graphics library.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 *
 * Contributor(s):
 *	Vladimir Vukicevic <vladimir@mozilla.com>
 */

#define _GNU_SOURCE /* required for RTLD_DEFAULT */
#include "cairoint.h"

#include "cairo-quartz-private.h"

#include "cairo-error-private.h"
#include "cairo-surface-clipper-private.h"

#include <dlfcn.h>

#ifndef RTLD_DEFAULT
#define RTLD_DEFAULT ((void *) 0)
#endif

#include <limits.h>

#undef QUARTZ_DEBUG

#ifdef QUARTZ_DEBUG
#define ND(_x)	fprintf _x
#else
#define ND(_x)	do {} while(0)
#endif

#define IS_EMPTY(s) ((s)->extents.width == 0 || (s)->extents.height == 0)

/**
 * SECTION:cairo-quartz
 * @Title: Quartz Surfaces
 * @Short_Description: Rendering to Quartz surfaces
 * @See_Also: #cairo_surface_t
 *
 * The Quartz surface is used to render cairo graphics targeting the
 * Apple OS X Quartz rendering system.
 */

/**
 * CAIRO_HAS_QUARTZ_SURFACE:
 *
 * Defined if the Quartz surface backend is available.
 * This macro can be used to conditionally compile backend-specific code.
 */

#if __MAC_OS_X_VERSION_MIN_REQUIRED < 1050
/* This method is private, but it exists.  Its params are are exposed
 * as args to the NS* method, but not as CG.
 */
enum PrivateCGCompositeMode {
    kPrivateCGCompositeClear		= 0,
    kPrivateCGCompositeCopy		= 1,
    kPrivateCGCompositeSourceOver	= 2,
    kPrivateCGCompositeSourceIn		= 3,
    kPrivateCGCompositeSourceOut	= 4,
    kPrivateCGCompositeSourceAtop	= 5,
    kPrivateCGCompositeDestinationOver	= 6,
    kPrivateCGCompositeDestinationIn	= 7,
    kPrivateCGCompositeDestinationOut	= 8,
    kPrivateCGCompositeDestinationAtop	= 9,
    kPrivateCGCompositeXOR		= 10,
    kPrivateCGCompositePlusDarker	= 11, // (max (0, (1-d) + (1-s)))
    kPrivateCGCompositePlusLighter	= 12, // (min (1, s + d))
};
typedef enum PrivateCGCompositeMode PrivateCGCompositeMode;
CG_EXTERN void CGContextSetCompositeOperation (CGContextRef, PrivateCGCompositeMode);
#endif
CG_EXTERN void CGContextSetCTM (CGContextRef, CGAffineTransform);

/* Some of these are present in earlier versions of the OS than where
 * they are public; other are not public at all
 */
/* public since 10.5 */
static void (*CGContextDrawTiledImagePtr) (CGContextRef, CGRect, CGImageRef) = NULL;

/* public since 10.6 */
static CGPathRef (*CGContextCopyPathPtr) (CGContextRef) = NULL;
static void (*CGContextSetAllowsFontSmoothingPtr) (CGContextRef, bool) = NULL;

/* not yet public */
static unsigned int (*CGContextGetTypePtr) (CGContextRef) = NULL;
static bool (*CGContextGetAllowsFontSmoothingPtr) (CGContextRef) = NULL;

static cairo_bool_t _cairo_quartz_symbol_lookup_done = FALSE;

/*
 * Utility functions
 */

#ifdef QUARTZ_DEBUG
static void quartz_surface_to_png (cairo_quartz_surface_t *nq, char *dest);
static void quartz_image_to_png (CGImageRef, char *dest);
#endif

static cairo_quartz_surface_t *
_cairo_quartz_surface_create_internal (CGContextRef cgContext,
				       cairo_content_t content,
				       unsigned int width,
				       unsigned int height);

static cairo_bool_t
_cairo_surface_is_quartz (const cairo_surface_t *surface);

/* Load all extra symbols */
static void quartz_ensure_symbols (void)
{
    if (likely (_cairo_quartz_symbol_lookup_done))
	return;

    CGContextDrawTiledImagePtr = dlsym (RTLD_DEFAULT, "CGContextDrawTiledImage");
    CGContextGetTypePtr = dlsym (RTLD_DEFAULT, "CGContextGetType");
    CGContextCopyPathPtr = dlsym (RTLD_DEFAULT, "CGContextCopyPath");
    CGContextGetAllowsFontSmoothingPtr = dlsym (RTLD_DEFAULT, "CGContextGetAllowsFontSmoothing");
    CGContextSetAllowsFontSmoothingPtr = dlsym (RTLD_DEFAULT, "CGContextSetAllowsFontSmoothing");

    _cairo_quartz_symbol_lookup_done = TRUE;
}

CGImageRef
_cairo_quartz_create_cgimage (cairo_format_t format,
			      unsigned int width,
			      unsigned int height,
			      unsigned int stride,
			      void *data,
			      cairo_bool_t interpolate,
			      CGColorSpaceRef colorSpaceOverride,
			      CGDataProviderReleaseDataCallback releaseCallback,
			      void *releaseInfo)
{
    CGImageRef image = NULL;
    CGDataProviderRef dataProvider = NULL;
    CGColorSpaceRef colorSpace = colorSpaceOverride;
    CGBitmapInfo bitinfo = kCGBitmapByteOrder32Host;
    int bitsPerComponent, bitsPerPixel;

    switch (format) {
	case CAIRO_FORMAT_ARGB32:
	    if (colorSpace == NULL)
		colorSpace = CGColorSpaceCreateDeviceRGB ();
	    bitinfo |= kCGImageAlphaPremultipliedFirst;
	    bitsPerComponent = 8;
	    bitsPerPixel = 32;
	    break;

	case CAIRO_FORMAT_RGB24:
	    if (colorSpace == NULL)
		colorSpace = CGColorSpaceCreateDeviceRGB ();
	    bitinfo |= kCGImageAlphaNoneSkipFirst;
	    bitsPerComponent = 8;
	    bitsPerPixel = 32;
	    break;

	case CAIRO_FORMAT_A8:
	    bitsPerComponent = 8;
	    bitsPerPixel = 8;
	    break;

	case CAIRO_FORMAT_A1:
#ifdef WORDS_BIGENDIAN
	    bitsPerComponent = 1;
	    bitsPerPixel = 1;
	    break;
#endif

        case CAIRO_FORMAT_RGB16_565:
        case CAIRO_FORMAT_INVALID:
	default:
	    return NULL;
    }

    dataProvider = CGDataProviderCreateWithData (releaseInfo,
						 data,
						 height * stride,
						 releaseCallback);

    if (unlikely (!dataProvider)) {
	// manually release
	if (releaseCallback)
	    releaseCallback (releaseInfo, data, height * stride);
	goto FINISH;
    }

    if (format == CAIRO_FORMAT_A8 || format == CAIRO_FORMAT_A1) {
	cairo_quartz_float_t decode[] = {1.0, 0.0};
	image = CGImageMaskCreate (width, height,
				   bitsPerComponent,
				   bitsPerPixel,
				   stride,
				   dataProvider,
				   decode,
				   interpolate);
    } else
	image = CGImageCreate (width, height,
			       bitsPerComponent,
			       bitsPerPixel,
			       stride,
			       colorSpace,
			       bitinfo,
			       dataProvider,
			       NULL,
			       interpolate,
			       kCGRenderingIntentDefault);

FINISH:

    CGDataProviderRelease (dataProvider);

    if (colorSpace != colorSpaceOverride)
	CGColorSpaceRelease (colorSpace);

    return image;
}

static inline cairo_bool_t
_cairo_quartz_is_cgcontext_bitmap_context (CGContextRef cgc)
{
    if (unlikely (cgc == NULL))
	return FALSE;

    if (likely (CGContextGetTypePtr)) {
	/* 4 is the type value of a bitmap context */
	return CGContextGetTypePtr (cgc) == 4;
    }

    /* This will cause a (harmless) warning to be printed if called on a non-bitmap context */
    return CGBitmapContextGetBitsPerPixel (cgc) != 0;
}

/* CoreGraphics limitation with flipped CTM surfaces: height must be less than signed 16-bit max */

#define CG_MAX_HEIGHT   SHRT_MAX
#define CG_MAX_WIDTH    USHRT_MAX

/* is the desired size of the surface within bounds? */
cairo_bool_t
_cairo_quartz_verify_surface_size (int width, int height)
{
    /* hmmm, allow width, height == 0 ? */
    if (width < 0 || height < 0)
	return FALSE;

    if (width > CG_MAX_WIDTH || height > CG_MAX_HEIGHT)
	return FALSE;

    return TRUE;
}

/*
 * Cairo path -> Quartz path conversion helpers
 */

/* cairo path -> execute in context */
static cairo_status_t
_cairo_path_to_quartz_context_move_to (void *closure,
				       const cairo_point_t *point)
{
    //ND ((stderr, "moveto: %f %f\n", _cairo_fixed_to_double (point->x), _cairo_fixed_to_double (point->y)));
    double x = _cairo_fixed_to_double (point->x);
    double y = _cairo_fixed_to_double (point->y);

    CGContextMoveToPoint (closure, x, y);
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_path_to_quartz_context_line_to (void *closure,
				       const cairo_point_t *point)
{
    //ND ((stderr, "lineto: %f %f\n",  _cairo_fixed_to_double (point->x), _cairo_fixed_to_double (point->y)));
    double x = _cairo_fixed_to_double (point->x);
    double y = _cairo_fixed_to_double (point->y);

    CGContextAddLineToPoint (closure, x, y);
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_path_to_quartz_context_curve_to (void *closure,
					const cairo_point_t *p0,
					const cairo_point_t *p1,
					const cairo_point_t *p2)
{
    //ND ((stderr, "curveto: %f,%f %f,%f %f,%f\n",
    //		   _cairo_fixed_to_double (p0->x), _cairo_fixed_to_double (p0->y),
    //		   _cairo_fixed_to_double (p1->x), _cairo_fixed_to_double (p1->y),
    //		   _cairo_fixed_to_double (p2->x), _cairo_fixed_to_double (p2->y)));
    double x0 = _cairo_fixed_to_double (p0->x);
    double y0 = _cairo_fixed_to_double (p0->y);
    double x1 = _cairo_fixed_to_double (p1->x);
    double y1 = _cairo_fixed_to_double (p1->y);
    double x2 = _cairo_fixed_to_double (p2->x);
    double y2 = _cairo_fixed_to_double (p2->y);

    CGContextAddCurveToPoint (closure, x0, y0, x1, y1, x2, y2);
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_path_to_quartz_context_close_path (void *closure)
{
    //ND ((stderr, "closepath\n"));
    CGContextClosePath (closure);
    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_quartz_cairo_path_to_quartz_context (cairo_path_fixed_t *path,
					    CGContextRef closure)
{
    cairo_status_t status;

    CGContextBeginPath (closure);
    status = _cairo_path_fixed_interpret (path,
					  _cairo_path_to_quartz_context_move_to,
					  _cairo_path_to_quartz_context_line_to,
					  _cairo_path_to_quartz_context_curve_to,
					  _cairo_path_to_quartz_context_close_path,
					  closure);

    assert (status == CAIRO_STATUS_SUCCESS);
}

/*
 * Misc helpers/callbacks
 */

#if __MAC_OS_X_VERSION_MIN_REQUIRED < 1050
static PrivateCGCompositeMode
_cairo_quartz_cairo_operator_to_quartz_composite (cairo_operator_t op)
{
    switch (op) {
	case CAIRO_OPERATOR_CLEAR:
	    return kPrivateCGCompositeClear;
	case CAIRO_OPERATOR_SOURCE:
	    return kPrivateCGCompositeCopy;
	case CAIRO_OPERATOR_OVER:
	    return kPrivateCGCompositeSourceOver;
	case CAIRO_OPERATOR_IN:
	    return kPrivateCGCompositeSourceIn;
	case CAIRO_OPERATOR_OUT:
	    return kPrivateCGCompositeSourceOut;
	case CAIRO_OPERATOR_ATOP:
	    return kPrivateCGCompositeSourceAtop;
	case CAIRO_OPERATOR_DEST_OVER:
	    return kPrivateCGCompositeDestinationOver;
	case CAIRO_OPERATOR_DEST_IN:
	    return kPrivateCGCompositeDestinationIn;
	case CAIRO_OPERATOR_DEST_OUT:
	    return kPrivateCGCompositeDestinationOut;
	case CAIRO_OPERATOR_DEST_ATOP:
	    return kPrivateCGCompositeDestinationAtop;
	case CAIRO_OPERATOR_XOR:
	    return kPrivateCGCompositeXOR;
	case CAIRO_OPERATOR_ADD:
	    return kPrivateCGCompositePlusLighter;

	case CAIRO_OPERATOR_DEST:
	case CAIRO_OPERATOR_SATURATE:
	case CAIRO_OPERATOR_MULTIPLY:
	case CAIRO_OPERATOR_SCREEN:
	case CAIRO_OPERATOR_OVERLAY:
	case CAIRO_OPERATOR_DARKEN:
	case CAIRO_OPERATOR_LIGHTEN:
	case CAIRO_OPERATOR_COLOR_DODGE:
	case CAIRO_OPERATOR_COLOR_BURN:
	case CAIRO_OPERATOR_HARD_LIGHT:
	case CAIRO_OPERATOR_SOFT_LIGHT:
	case CAIRO_OPERATOR_DIFFERENCE:
	case CAIRO_OPERATOR_EXCLUSION:
	case CAIRO_OPERATOR_HSL_HUE:
	case CAIRO_OPERATOR_HSL_SATURATION:
	case CAIRO_OPERATOR_HSL_COLOR:
	case CAIRO_OPERATOR_HSL_LUMINOSITY:
        default:
	    ASSERT_NOT_REACHED;
    }
}
#endif

static CGBlendMode
_cairo_quartz_cairo_operator_to_quartz_blend (cairo_operator_t op)
{
    switch (op) {
	case CAIRO_OPERATOR_MULTIPLY:
	    return kCGBlendModeMultiply;
	case CAIRO_OPERATOR_SCREEN:
	    return kCGBlendModeScreen;
	case CAIRO_OPERATOR_OVERLAY:
	    return kCGBlendModeOverlay;
	case CAIRO_OPERATOR_DARKEN:
	    return kCGBlendModeDarken;
	case CAIRO_OPERATOR_LIGHTEN:
	    return kCGBlendModeLighten;
	case CAIRO_OPERATOR_COLOR_DODGE:
	    return kCGBlendModeColorDodge;
	case CAIRO_OPERATOR_COLOR_BURN:
	    return kCGBlendModeColorBurn;
	case CAIRO_OPERATOR_HARD_LIGHT:
	    return kCGBlendModeHardLight;
	case CAIRO_OPERATOR_SOFT_LIGHT:
	    return kCGBlendModeSoftLight;
	case CAIRO_OPERATOR_DIFFERENCE:
	    return kCGBlendModeDifference;
	case CAIRO_OPERATOR_EXCLUSION:
	    return kCGBlendModeExclusion;
	case CAIRO_OPERATOR_HSL_HUE:
	    return kCGBlendModeHue;
	case CAIRO_OPERATOR_HSL_SATURATION:
	    return kCGBlendModeSaturation;
	case CAIRO_OPERATOR_HSL_COLOR:
	    return kCGBlendModeColor;
	case CAIRO_OPERATOR_HSL_LUMINOSITY:
	    return kCGBlendModeLuminosity;

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 1050
	case CAIRO_OPERATOR_CLEAR:
	    return kCGBlendModeClear;
	case CAIRO_OPERATOR_SOURCE:
	    return kCGBlendModeCopy;
	case CAIRO_OPERATOR_OVER:
	    return kCGBlendModeNormal;
	case CAIRO_OPERATOR_IN:
	    return kCGBlendModeSourceIn;
	case CAIRO_OPERATOR_OUT:
	    return kCGBlendModeSourceOut;
	case CAIRO_OPERATOR_ATOP:
	    return kCGBlendModeSourceAtop;
	case CAIRO_OPERATOR_DEST_OVER:
	    return kCGBlendModeDestinationOver;
	case CAIRO_OPERATOR_DEST_IN:
	    return kCGBlendModeDestinationIn;
	case CAIRO_OPERATOR_DEST_OUT:
	    return kCGBlendModeDestinationOut;
	case CAIRO_OPERATOR_DEST_ATOP:
	    return kCGBlendModeDestinationAtop;
	case CAIRO_OPERATOR_XOR:
	    return kCGBlendModeXOR;
	case CAIRO_OPERATOR_ADD:
	    return kCGBlendModePlusLighter;
#else
	case CAIRO_OPERATOR_CLEAR:
	case CAIRO_OPERATOR_SOURCE:
	case CAIRO_OPERATOR_OVER:
	case CAIRO_OPERATOR_IN:
	case CAIRO_OPERATOR_OUT:
	case CAIRO_OPERATOR_ATOP:
	case CAIRO_OPERATOR_DEST_OVER:
	case CAIRO_OPERATOR_DEST_IN:
	case CAIRO_OPERATOR_DEST_OUT:
	case CAIRO_OPERATOR_DEST_ATOP:
	case CAIRO_OPERATOR_XOR:
	case CAIRO_OPERATOR_ADD:
#endif

	case CAIRO_OPERATOR_DEST:
	case CAIRO_OPERATOR_SATURATE:
        default:
	    ASSERT_NOT_REACHED;
    }
}

static cairo_int_status_t
_cairo_cgcontext_set_cairo_operator (CGContextRef context, cairo_operator_t op)
{
    CGBlendMode blendmode;

    if (op == CAIRO_OPERATOR_DEST)
	return CAIRO_INT_STATUS_NOTHING_TO_DO;

    /* Quartz doesn't support SATURATE at all. COLOR_DODGE and
     * COLOR_BURN in Quartz follow the ISO32000 definition, but cairo
     * uses the definition from the Adobe Supplement.
     */
    if (op == CAIRO_OPERATOR_SATURATE ||
	op == CAIRO_OPERATOR_COLOR_DODGE ||
	op == CAIRO_OPERATOR_COLOR_BURN)
    {
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

#if __MAC_OS_X_VERSION_MIN_REQUIRED < 1050
    if (op <= CAIRO_OPERATOR_ADD) {
	PrivateCGCompositeMode compmode;

	compmode = _cairo_quartz_cairo_operator_to_quartz_composite (op);
	CGContextSetCompositeOperation (context, compmode);
	return CAIRO_STATUS_SUCCESS;
    }
#endif

    blendmode = _cairo_quartz_cairo_operator_to_quartz_blend (op);
    CGContextSetBlendMode (context, blendmode);
    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_quartz_surface_set_cairo_operator (cairo_quartz_surface_t *surface, cairo_operator_t op)
{
    ND((stderr, "%p _cairo_quartz_surface_set_cairo_operator %d\n", surface, op));

    if (surface->base.content == CAIRO_CONTENT_ALPHA) {
	if (op == CAIRO_OPERATOR_OUT ||
	    op == CAIRO_OPERATOR_XOR)
	    return CAIRO_INT_STATUS_UNSUPPORTED;
	else if (op == CAIRO_OPERATOR_SATURATE)
	    op = CAIRO_OPERATOR_ADD;
	else if (op == CAIRO_OPERATOR_IN)
	    op = CAIRO_OPERATOR_DEST_ATOP;
	else if (op == CAIRO_OPERATOR_DEST_ATOP)
	    op = CAIRO_OPERATOR_IN;
	else if (op == CAIRO_OPERATOR_ATOP)
	    return CAIRO_INT_STATUS_NOTHING_TO_DO; /* op = CAIRO_OPERATOR_DEST_OVER */
	else if (op == CAIRO_OPERATOR_DEST_OVER)
	    op = CAIRO_OPERATOR_ATOP;
    }

    return _cairo_cgcontext_set_cairo_operator (surface->cgContext, op);
}

static inline CGLineCap
_cairo_quartz_cairo_line_cap_to_quartz (cairo_line_cap_t ccap)
{
    switch (ccap) {
    default:
	ASSERT_NOT_REACHED;

    case CAIRO_LINE_CAP_BUTT:
	return kCGLineCapButt;

    case CAIRO_LINE_CAP_ROUND:
	return kCGLineCapRound;

    case CAIRO_LINE_CAP_SQUARE:
	return kCGLineCapSquare;
    }
}

static inline CGLineJoin
_cairo_quartz_cairo_line_join_to_quartz (cairo_line_join_t cjoin)
{
    switch (cjoin) {
    default:
	ASSERT_NOT_REACHED;

    case CAIRO_LINE_JOIN_MITER:
	return kCGLineJoinMiter;

    case CAIRO_LINE_JOIN_ROUND:
	return kCGLineJoinRound;

    case CAIRO_LINE_JOIN_BEVEL:
	return kCGLineJoinBevel;
    }
}

static inline CGInterpolationQuality
_cairo_quartz_filter_to_quartz (cairo_filter_t filter)
{
    switch (filter) {
    case CAIRO_FILTER_NEAREST:
	return kCGInterpolationNone;

    case CAIRO_FILTER_FAST:
	return kCGInterpolationLow;

    case CAIRO_FILTER_BEST:
    case CAIRO_FILTER_GOOD:
    case CAIRO_FILTER_BILINEAR:
    case CAIRO_FILTER_GAUSSIAN:
	return kCGInterpolationDefault;

    default:
	ASSERT_NOT_REACHED;
	return kCGInterpolationDefault;
    }
}

static inline void
_cairo_quartz_cairo_matrix_to_quartz (const cairo_matrix_t *src,
				      CGAffineTransform *dst)
{
    dst->a = src->xx;
    dst->b = src->yx;
    dst->c = src->xy;
    dst->d = src->yy;
    dst->tx = src->x0;
    dst->ty = src->y0;
}

typedef struct {
    bool isClipping;
    CGGlyph *cg_glyphs;
    CGSize *cg_advances;
    size_t nglyphs;
    CGAffineTransform textTransform;
    CGFontRef font;
    CGPoint origin;
} unbounded_show_glyphs_t;

typedef struct {
    CGPathRef cgPath;
    cairo_fill_rule_t fill_rule;
} unbounded_stroke_fill_t;

typedef struct {
    CGImageRef mask;
    CGAffineTransform maskTransform;
} unbounded_mask_t;

typedef enum {
    UNBOUNDED_STROKE_FILL,
    UNBOUNDED_SHOW_GLYPHS,
    UNBOUNDED_MASK
} unbounded_op_t;

typedef struct {
    unbounded_op_t op;
    union {
	unbounded_stroke_fill_t stroke_fill;
	unbounded_show_glyphs_t show_glyphs;
	unbounded_mask_t mask;
    } u;
} unbounded_op_data_t;

static void
_cairo_quartz_fixup_unbounded_operation (cairo_quartz_surface_t *surface,
					 unbounded_op_data_t *op,
					 cairo_antialias_t antialias)
{
    CGRect clipBox, clipBoxRound;
    CGContextRef cgc;
    CGImageRef maskImage;

    clipBox = CGContextGetClipBoundingBox (surface->cgContext);
    clipBoxRound = CGRectIntegral (clipBox);

    cgc = CGBitmapContextCreate (NULL,
				 clipBoxRound.size.width,
				 clipBoxRound.size.height,
				 8,
				 (((size_t) clipBoxRound.size.width) + 15) & (~15),
				 NULL,
				 kCGImageAlphaOnly);

    if (!cgc)
	return;

    _cairo_cgcontext_set_cairo_operator (cgc, CAIRO_OPERATOR_SOURCE);

    /* We want to mask out whatever we just rendered, so we fill the
     * surface opaque, and then we'll render transparent.
     */
    CGContextSetAlpha (cgc, 1.0f);
    CGContextFillRect (cgc, CGRectMake (0, 0, clipBoxRound.size.width, clipBoxRound.size.height));

    _cairo_cgcontext_set_cairo_operator (cgc, CAIRO_OPERATOR_CLEAR);
    CGContextSetShouldAntialias (cgc, (antialias != CAIRO_ANTIALIAS_NONE));

    CGContextTranslateCTM (cgc, -clipBoxRound.origin.x, -clipBoxRound.origin.y);

    /* We need to either render the path that was given to us, or the glyph op */
    if (op->op == UNBOUNDED_STROKE_FILL) {
	CGContextBeginPath (cgc);
	CGContextAddPath (cgc, op->u.stroke_fill.cgPath);

	if (op->u.stroke_fill.fill_rule == CAIRO_FILL_RULE_WINDING)
	    CGContextFillPath (cgc);
	else
	    CGContextEOFillPath (cgc);
    } else if (op->op == UNBOUNDED_SHOW_GLYPHS) {
	CGContextSetFont (cgc, op->u.show_glyphs.font);
	CGContextSetFontSize (cgc, 1.0);
	CGContextSetTextMatrix (cgc, CGAffineTransformIdentity);
	CGContextTranslateCTM (cgc, op->u.show_glyphs.origin.x, op->u.show_glyphs.origin.y);
	CGContextConcatCTM (cgc, op->u.show_glyphs.textTransform);

	if (op->u.show_glyphs.isClipping) {
	    /* Note that the comment in show_glyphs about kCGTextClip
	     * and the text transform still applies here; however, the
	     * cg_advances we have were already transformed, so we
	     * don't have to do anything. */
	    CGContextSetTextDrawingMode (cgc, kCGTextClip);
	    CGContextSaveGState (cgc);
	}

	CGContextShowGlyphsWithAdvances (cgc,
					 op->u.show_glyphs.cg_glyphs,
					 op->u.show_glyphs.cg_advances,
					 op->u.show_glyphs.nglyphs);

	if (op->u.show_glyphs.isClipping) {
	    CGContextClearRect (cgc, clipBoxRound);
	    CGContextRestoreGState (cgc);
	}
    } else if (op->op == UNBOUNDED_MASK) {
	CGAffineTransform ctm = CGContextGetCTM (cgc);
	CGContextSaveGState (cgc);
	CGContextConcatCTM (cgc, op->u.mask.maskTransform);
	CGContextClipToMask (cgc,
			     CGRectMake (0.0,
					 0.0,
					 CGImageGetWidth (op->u.mask.mask),
					 CGImageGetHeight (op->u.mask.mask)),
			     op->u.mask.mask);
	CGContextSetCTM (cgc, ctm);
	CGContextClearRect (cgc, clipBoxRound);
	CGContextRestoreGState (cgc);
    }

    /* Also mask out the portion of the clipbox that we rounded out, if any */
    if (!CGRectEqualToRect (clipBox, clipBoxRound)) {
	CGContextBeginPath (cgc);
	CGContextAddRect (cgc, clipBoxRound);
	CGContextAddRect (cgc, clipBox);
	CGContextEOFillPath (cgc);
    }

    maskImage = CGBitmapContextCreateImage (cgc);
    CGContextRelease (cgc);

    if (!maskImage)
	return;

    /* Then render with the mask */
    CGContextSaveGState (surface->cgContext);

    _cairo_quartz_surface_set_cairo_operator (surface, CAIRO_OPERATOR_SOURCE);
    CGContextClipToMask (surface->cgContext, clipBoxRound, maskImage);
    CGImageRelease (maskImage);

    /* Finally, clear out the entire clipping region through our mask */
    CGContextClearRect (surface->cgContext, clipBoxRound);

    CGContextRestoreGState (surface->cgContext);
}

/*
 * Source -> Quartz setup and finish functions
 */

static void
ComputeGradientValue (void *info,
                      const cairo_quartz_float_t *in,
                      cairo_quartz_float_t *out)
{
    double fdist = *in;
    const cairo_gradient_pattern_t *grad = (cairo_gradient_pattern_t*) info;
    unsigned int i;

    /* Put fdist back in the 0.0..1.0 range if we're doing
     * REPEAT/REFLECT
     */
    if (grad->base.extend == CAIRO_EXTEND_REPEAT) {
	fdist = fdist - floor (fdist);
    } else if (grad->base.extend == CAIRO_EXTEND_REFLECT) {
	fdist = fmod (fabs (fdist), 2.0);
	if (fdist > 1.0)
	    fdist = 2.0 - fdist;
    }

    for (i = 0; i < grad->n_stops; i++)
	if (grad->stops[i].offset > fdist)
	    break;

    if (i == 0 || i == grad->n_stops) {
	if (i == grad->n_stops)
	    --i;
	out[0] = grad->stops[i].color.red;
	out[1] = grad->stops[i].color.green;
	out[2] = grad->stops[i].color.blue;
	out[3] = grad->stops[i].color.alpha;
    } else {
	cairo_quartz_float_t ax = grad->stops[i-1].offset;
	cairo_quartz_float_t bx = grad->stops[i].offset - ax;
	cairo_quartz_float_t bp = (fdist - ax)/bx;
	cairo_quartz_float_t ap = 1.0 - bp;

	out[0] =
	    grad->stops[i-1].color.red * ap +
	    grad->stops[i].color.red * bp;
	out[1] =
	    grad->stops[i-1].color.green * ap +
	    grad->stops[i].color.green * bp;
	out[2] =
	    grad->stops[i-1].color.blue * ap +
	    grad->stops[i].color.blue * bp;
	out[3] =
	    grad->stops[i-1].color.alpha * ap +
	    grad->stops[i].color.alpha * bp;
    }
}

static const cairo_quartz_float_t gradient_output_value_ranges[8] = {
    0.f, 1.f, 0.f, 1.f, 0.f, 1.f, 0.f, 1.f
};
static const CGFunctionCallbacks gradient_callbacks = {
    0, ComputeGradientValue, (CGFunctionReleaseInfoCallback) cairo_pattern_destroy
};

/* Quartz computes a small number of samples of the gradient color
 * function. On MacOS X 10.5 it apparently computes only 1024
 * samples. */
#define MAX_GRADIENT_RANGE 1024

static CGFunctionRef
_cairo_quartz_create_gradient_function (const cairo_gradient_pattern_t *gradient,
					const cairo_rectangle_int_t *extents,
					cairo_circle_double_t       *start,
					cairo_circle_double_t       *end)
{
    cairo_pattern_t *pat;
    cairo_quartz_float_t input_value_range[2];

    if (gradient->base.extend != CAIRO_EXTEND_NONE) {
	double bounds_x1, bounds_x2, bounds_y1, bounds_y2;
	double t[2], tolerance;

	tolerance = fabs (_cairo_matrix_compute_determinant (&gradient->base.matrix));
	tolerance /= _cairo_matrix_transformed_circle_major_axis (&gradient->base.matrix, 1);

	bounds_x1 = extents->x;
	bounds_y1 = extents->y;
	bounds_x2 = extents->x + extents->width;
	bounds_y2 = extents->y + extents->height;
	_cairo_matrix_transform_bounding_box (&gradient->base.matrix,
					      &bounds_x1, &bounds_y1,
					      &bounds_x2, &bounds_y2,
					      NULL);

	_cairo_gradient_pattern_box_to_parameter (gradient,
						  bounds_x1, bounds_y1,
						  bounds_x2, bounds_y2,
						  tolerance,
						  t);

	if (gradient->base.extend == CAIRO_EXTEND_PAD) {
	    t[0] = MAX (t[0], -0.5);
	    t[1] = MIN (t[1],  1.5);
	} else if (t[1] - t[0] > MAX_GRADIENT_RANGE)
	    return NULL;

	/* set the input range for the function -- the function knows how
	   to map values outside of 0.0 .. 1.0 to the correct color */
	input_value_range[0] = t[0];
	input_value_range[1] = t[1];
    } else {
	input_value_range[0] = 0;
	input_value_range[1] = 1;
    }

    _cairo_gradient_pattern_interpolate (gradient, input_value_range[0], start);
    _cairo_gradient_pattern_interpolate (gradient, input_value_range[1], end);

    if (_cairo_pattern_create_copy (&pat, &gradient->base))
	return NULL;

    return CGFunctionCreate (pat,
			     1,
			     input_value_range,
			     4,
			     gradient_output_value_ranges,
			     &gradient_callbacks);
}

/* Obtain a CGImageRef from a #cairo_surface_t * */

typedef struct {
    cairo_surface_t *surface;
    cairo_image_surface_t *image_out;
    void *image_extra;
} quartz_source_image_t;

static void
DataProviderReleaseCallback (void *info, const void *data, size_t size)
{
    quartz_source_image_t *source_img = info;
    _cairo_surface_release_source_image (source_img->surface, source_img->image_out, source_img->image_extra);
    free (source_img);
}

static cairo_status_t
_cairo_surface_to_cgimage (cairo_surface_t *source,
			   CGImageRef *image_out)
{
    cairo_status_t status;
    quartz_source_image_t *source_img;

    if (source->backend && source->backend->type == CAIRO_SURFACE_TYPE_QUARTZ_IMAGE) {
	cairo_quartz_image_surface_t *surface = (cairo_quartz_image_surface_t *) source;
	*image_out = CGImageRetain (surface->image);
	return CAIRO_STATUS_SUCCESS;
    }

    if (_cairo_surface_is_quartz (source)) {
	cairo_quartz_surface_t *surface = (cairo_quartz_surface_t *) source;
	if (IS_EMPTY (surface)) {
	    *image_out = NULL;
	    return CAIRO_STATUS_SUCCESS;
	}

	if (_cairo_quartz_is_cgcontext_bitmap_context (surface->cgContext)) {
	    *image_out = CGBitmapContextCreateImage (surface->cgContext);
	    if (*image_out)
		return CAIRO_STATUS_SUCCESS;
	}
    }

    source_img = malloc (sizeof (quartz_source_image_t));
    if (unlikely (source_img == NULL))
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    source_img->surface = source;

    status = _cairo_surface_acquire_source_image (source_img->surface, &source_img->image_out, &source_img->image_extra);
    if (unlikely (status)) {
	free (source_img);
	return status;
    }

    if (source_img->image_out->width == 0 || source_img->image_out->height == 0) {
	*image_out = NULL;
	DataProviderReleaseCallback (source_img,
				     source_img->image_out->data,
				     source_img->image_out->height * source_img->image_out->stride);
    } else {
	*image_out = _cairo_quartz_create_cgimage (source_img->image_out->format,
						   source_img->image_out->width,
						   source_img->image_out->height,
						   source_img->image_out->stride,
						   source_img->image_out->data,
						   TRUE,
						   NULL,
						   DataProviderReleaseCallback,
						   source_img);

	/* TODO: differentiate memory error and unsupported surface type */
	if (unlikely (*image_out == NULL))
	    status = CAIRO_INT_STATUS_UNSUPPORTED;
    }

    return status;
}

/* Generic #cairo_pattern_t -> CGPattern function */

typedef struct {
    CGImageRef image;
    CGRect imageBounds;
    cairo_bool_t do_reflect;
} SurfacePatternDrawInfo;

static void
SurfacePatternDrawFunc (void *ainfo, CGContextRef context)
{
    SurfacePatternDrawInfo *info = (SurfacePatternDrawInfo*) ainfo;

    CGContextTranslateCTM (context, 0, info->imageBounds.size.height);
    CGContextScaleCTM (context, 1, -1);

    CGContextDrawImage (context, info->imageBounds, info->image);
    if (info->do_reflect) {
	/* draw 3 more copies of the image, flipped.
	 * DrawImage draws the image according to the current Y-direction into the rectangle given
	 * (imageBounds); at the time of the first DrawImage above, the origin is at the bottom left
	 * of the base image position, and the Y axis is extending upwards.
	 */

	/* Make the y axis extend downwards, and draw a flipped image below */
	CGContextScaleCTM (context, 1, -1);
	CGContextDrawImage (context, info->imageBounds, info->image);

	/* Shift over to the right, and flip vertically (translation is 2x,
	 * since we'll be flipping and thus rendering the rectangle "backwards"
	 */
	CGContextTranslateCTM (context, 2 * info->imageBounds.size.width, 0);
	CGContextScaleCTM (context, -1, 1);
	CGContextDrawImage (context, info->imageBounds, info->image);

	/* Then unflip the Y-axis again, and draw the image above the point. */
	CGContextScaleCTM (context, 1, -1);
	CGContextDrawImage (context, info->imageBounds, info->image);
    }
}

static void
SurfacePatternReleaseInfoFunc (void *ainfo)
{
    SurfacePatternDrawInfo *info = (SurfacePatternDrawInfo*) ainfo;

    CGImageRelease (info->image);
    free (info);
}

static cairo_int_status_t
_cairo_quartz_cairo_repeating_surface_pattern_to_quartz (cairo_quartz_surface_t *dest,
							 const cairo_pattern_t *apattern,
							 CGPatternRef *cgpat)
{
    cairo_surface_pattern_t *spattern;
    cairo_surface_t *pat_surf;
    cairo_rectangle_int_t extents;

    CGImageRef image;
    CGRect pbounds;
    CGAffineTransform ptransform, stransform;
    CGPatternCallbacks cb = { 0,
			      SurfacePatternDrawFunc,
			      SurfacePatternReleaseInfoFunc };
    SurfacePatternDrawInfo *info;
    cairo_quartz_float_t rw, rh;
    cairo_status_t status;
    cairo_bool_t is_bounded;

    cairo_matrix_t m;

    /* SURFACE is the only type we'll handle here */
    assert (apattern->type == CAIRO_PATTERN_TYPE_SURFACE);

    spattern = (cairo_surface_pattern_t *) apattern;
    pat_surf = spattern->surface;

    is_bounded = _cairo_surface_get_extents (pat_surf, &extents);
    assert (is_bounded);

    status = _cairo_surface_to_cgimage (pat_surf, &image);
    if (unlikely (status))
	return status;
    if (unlikely (image == NULL))
	return CAIRO_INT_STATUS_NOTHING_TO_DO;

    info = malloc (sizeof (SurfacePatternDrawInfo));
    if (unlikely (!info))
	return CAIRO_STATUS_NO_MEMORY;

    /* XXX -- if we're printing, we may need to call CGImageCreateCopy to make sure
     * that the data will stick around for this image when the printer gets to it.
     * Otherwise, the underlying data store may disappear from under us!
     *
     * _cairo_surface_to_cgimage will copy when it converts non-Quartz surfaces,
     * since the Quartz surfaces have a higher chance of sticking around.  If the
     * source is a quartz image surface, then it's set up to retain a ref to the
     * image surface that it's backed by.
     */
    info->image = image;
    info->imageBounds = CGRectMake (0, 0, extents.width, extents.height);
    info->do_reflect = FALSE;

    pbounds.origin.x = 0;
    pbounds.origin.y = 0;

    if (spattern->base.extend == CAIRO_EXTEND_REFLECT) {
	pbounds.size.width = 2.0 * extents.width;
	pbounds.size.height = 2.0 * extents.height;
	info->do_reflect = TRUE;
    } else {
	pbounds.size.width = extents.width;
	pbounds.size.height = extents.height;
    }
    rw = pbounds.size.width;
    rh = pbounds.size.height;

    m = spattern->base.matrix;
    cairo_matrix_invert (&m);
    _cairo_quartz_cairo_matrix_to_quartz (&m, &stransform);

    /* The pattern matrix is relative to the bottom left, again; the
     * incoming cairo pattern matrix is relative to the upper left.
     * So we take the pattern matrix and the original context matrix,
     * which gives us the correct base translation/y flip.
     */
    ptransform = CGAffineTransformConcat (stransform, dest->cgContextBaseCTM);

#ifdef QUARTZ_DEBUG
    ND ((stderr, "  pbounds: %f %f %f %f\n", pbounds.origin.x, pbounds.origin.y, pbounds.size.width, pbounds.size.height));
    ND ((stderr, "  pattern xform: t: %f %f xx: %f xy: %f yx: %f yy: %f\n", ptransform.tx, ptransform.ty, ptransform.a, ptransform.b, ptransform.c, ptransform.d));
    CGAffineTransform xform = CGContextGetCTM (dest->cgContext);
    ND ((stderr, "  context xform: t: %f %f xx: %f xy: %f yx: %f yy: %f\n", xform.tx, xform.ty, xform.a, xform.b, xform.c, xform.d));
#endif

    *cgpat = CGPatternCreate (info,
			      pbounds,
			      ptransform,
			      rw, rh,
			      kCGPatternTilingConstantSpacing, /* kCGPatternTilingNoDistortion, */
			      TRUE,
			      &cb);

    return CAIRO_STATUS_SUCCESS;
}

/* State used during a drawing operation. */
typedef struct {
    cairo_quartz_action_t action;

    /* Used with DO_SHADING, DO_IMAGE and DO_TILED_IMAGE */
    CGAffineTransform transform;

    /* Used with DO_IMAGE and DO_TILED_IMAGE */
    CGImageRef image;
    CGRect imageRect;

    /* Used with DO_SHADING */
    CGShadingRef shading;

} cairo_quartz_drawing_state_t;

/*
Quartz does not support repeating radients. We handle repeating gradients
by manually extending the gradient and repeating color stops. We need to
minimize the number of repetitions since Quartz seems to sample our color
function across the entire range, even if part of that range is not needed
for the visible area of the gradient, and it samples with some fixed resolution,
so if the gradient range is too large it samples with very low resolution and
the gradient is very coarse. _cairo_quartz_create_gradient_function computes
the number of repetitions needed based on the extents.
*/
static cairo_int_status_t
_cairo_quartz_setup_gradient_source (cairo_quartz_drawing_state_t *state,
				     const cairo_gradient_pattern_t *gradient,
				     const cairo_rectangle_int_t *extents)
{
    cairo_matrix_t mat;
    cairo_circle_double_t start, end;
    CGFunctionRef gradFunc;
    CGColorSpaceRef rgb;
    bool extend = gradient->base.extend != CAIRO_EXTEND_NONE;

    assert (gradient->n_stops > 0);

    mat = gradient->base.matrix;
    cairo_matrix_invert (&mat);
    _cairo_quartz_cairo_matrix_to_quartz (&mat, &state->transform);

    gradFunc = _cairo_quartz_create_gradient_function (gradient,
						       extents,
						       &start,
						       &end);

    if (unlikely (gradFunc == NULL))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    rgb = CGColorSpaceCreateDeviceRGB ();

    if (gradient->base.type == CAIRO_PATTERN_TYPE_LINEAR) {
	state->shading = CGShadingCreateAxial (rgb,
					       CGPointMake (start.center.x,
							    start.center.y),
					       CGPointMake (end.center.x,
							    end.center.y),
					       gradFunc,
					       extend, extend);
    } else {
	state->shading = CGShadingCreateRadial (rgb,
						CGPointMake (start.center.x,
							     start.center.y),
						MAX (start.radius, 0),
						CGPointMake (end.center.x,
							     end.center.y),
						MAX (end.radius, 0),
						gradFunc,
						extend, extend);
    }

    CGColorSpaceRelease (rgb);
    CGFunctionRelease (gradFunc);

    state->action = DO_SHADING;
    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_quartz_setup_source (cairo_quartz_drawing_state_t *state,
			    cairo_quartz_surface_t *surface,
			    cairo_operator_t op,
			    const cairo_pattern_t *source)
{
    cairo_status_t status;

    state->image = NULL;
    state->shading = NULL;

    /* Save before we change the pattern, colorspace, etc. so that
     * we can restore and make sure that quartz releases our
     * pattern (which may be stack allocated)
     */

    CGContextSaveGState (surface->cgContext);

    status = _cairo_quartz_surface_set_cairo_operator (surface, op);
    if (unlikely (status))
	return status;

    CGContextSetInterpolationQuality (surface->cgContext, _cairo_quartz_filter_to_quartz (source->filter));

    if (source->type == CAIRO_PATTERN_TYPE_SOLID) {
	cairo_solid_pattern_t *solid = (cairo_solid_pattern_t *) source;

	CGContextSetRGBStrokeColor (surface->cgContext,
				    solid->color.red,
				    solid->color.green,
				    solid->color.blue,
				    solid->color.alpha);
	CGContextSetRGBFillColor (surface->cgContext,
				  solid->color.red,
				  solid->color.green,
				  solid->color.blue,
				  solid->color.alpha);

	state->action = DO_DIRECT;
	return CAIRO_STATUS_SUCCESS;
    }

    if (source->type == CAIRO_PATTERN_TYPE_LINEAR ||
	source->type == CAIRO_PATTERN_TYPE_RADIAL)
    {
	const cairo_gradient_pattern_t *gpat = (const cairo_gradient_pattern_t *)source;
	cairo_rectangle_int_t extents;

	extents = surface->virtual_extents;
	extents.x -= surface->base.device_transform.x0;
	extents.y -= surface->base.device_transform.y0;
	_cairo_rectangle_union (&extents, &surface->extents);

	return _cairo_quartz_setup_gradient_source (state, gpat, &extents);
    }

    if (source->type == CAIRO_PATTERN_TYPE_SURFACE &&
	(source->extend == CAIRO_EXTEND_NONE || (CGContextDrawTiledImagePtr && source->extend == CAIRO_EXTEND_REPEAT)))
    {
	const cairo_surface_pattern_t *spat = (const cairo_surface_pattern_t *) source;
	cairo_surface_t *pat_surf = spat->surface;
	CGImageRef img;
	cairo_matrix_t m = spat->base.matrix;
	cairo_rectangle_int_t extents;
	CGAffineTransform xform;
	CGRect srcRect;
	cairo_fixed_t fw, fh;
	cairo_bool_t is_bounded;

	status = _cairo_surface_to_cgimage (pat_surf, &img);
	if (unlikely (status))
	    return status;
	if (unlikely (img == NULL))
	    return CAIRO_INT_STATUS_NOTHING_TO_DO;

	CGContextSetRGBFillColor (surface->cgContext, 0, 0, 0, 1);

	state->image = img;

	cairo_matrix_invert (&m);
	_cairo_quartz_cairo_matrix_to_quartz (&m, &state->transform);

	is_bounded = _cairo_surface_get_extents (pat_surf, &extents);
	assert (is_bounded);

	if (source->extend == CAIRO_EXTEND_NONE) {
	    state->imageRect = CGRectMake (0, 0, extents.width, extents.height);
	    state->action = DO_IMAGE;
	    return CAIRO_STATUS_SUCCESS;
	}

	/* Quartz seems to tile images at pixel-aligned regions only -- this
	 * leads to seams if the image doesn't end up scaling to fill the
	 * space exactly.  The CGPattern tiling approach doesn't have this
	 * problem.  Check if we're going to fill up the space (within some
	 * epsilon), and if not, fall back to the CGPattern type.
	 */

	xform = CGAffineTransformConcat (CGContextGetCTM (surface->cgContext),
					 state->transform);

	srcRect = CGRectMake (0, 0, extents.width, extents.height);
	srcRect = CGRectApplyAffineTransform (srcRect, xform);

	fw = _cairo_fixed_from_double (srcRect.size.width);
	fh = _cairo_fixed_from_double (srcRect.size.height);

	if ((fw & CAIRO_FIXED_FRAC_MASK) <= CAIRO_FIXED_EPSILON &&
	    (fh & CAIRO_FIXED_FRAC_MASK) <= CAIRO_FIXED_EPSILON)
	{
	    /* We're good to use DrawTiledImage, but ensure that
	     * the math works out */

	    srcRect.size.width = round (srcRect.size.width);
	    srcRect.size.height = round (srcRect.size.height);

	    xform = CGAffineTransformInvert (xform);

	    srcRect = CGRectApplyAffineTransform (srcRect, xform);

	    state->imageRect = srcRect;
	    state->action = DO_TILED_IMAGE;
	    return CAIRO_STATUS_SUCCESS;
	}

	/* Fall through to generic SURFACE case */
    }

    if (source->type == CAIRO_PATTERN_TYPE_SURFACE) {
	cairo_quartz_float_t patternAlpha = 1.0f;
	CGColorSpaceRef patternSpace;
	CGPatternRef pattern = NULL;
	cairo_int_status_t status;

	status = _cairo_quartz_cairo_repeating_surface_pattern_to_quartz (surface, source, &pattern);
	if (unlikely (status))
	    return status;

	patternSpace = CGColorSpaceCreatePattern (NULL);
	CGContextSetFillColorSpace (surface->cgContext, patternSpace);
	CGContextSetFillPattern (surface->cgContext, pattern, &patternAlpha);
	CGContextSetStrokeColorSpace (surface->cgContext, patternSpace);
	CGContextSetStrokePattern (surface->cgContext, pattern, &patternAlpha);
	CGColorSpaceRelease (patternSpace);

	/* Quartz likes to munge the pattern phase (as yet unexplained
	 * why); force it to 0,0 as we've already baked in the correct
	 * pattern translation into the pattern matrix
	 */
	CGContextSetPatternPhase (surface->cgContext, CGSizeMake (0, 0));

	CGPatternRelease (pattern);

	state->action = DO_DIRECT;
	return CAIRO_STATUS_SUCCESS;
    }

    return CAIRO_INT_STATUS_UNSUPPORTED;
}

static void
_cairo_quartz_teardown_source (cairo_quartz_drawing_state_t *state,
			       cairo_quartz_surface_t *surface)
			       
{
    CGContextRestoreGState (surface->cgContext);

    if (state->image)
	CGImageRelease (state->image);

    if (state->shading)
	CGShadingRelease (state->shading);
}

static cairo_int_status_t
_cairo_quartz_setup_source_safe (cairo_quartz_drawing_state_t *state,
				 cairo_quartz_surface_t *surface,
				 cairo_operator_t op,
				 const cairo_pattern_t *source)
{
    cairo_int_status_t status;

    status = _cairo_quartz_setup_source (state, surface, op, source);
    if (unlikely (status))
	_cairo_quartz_teardown_source (state, surface);

    return status;
}

static void
_cairo_quartz_draw_source (cairo_quartz_surface_t *surface, cairo_operator_t op, cairo_quartz_drawing_state_t *state)
{
    CGContextConcatCTM (surface->cgContext, state->transform);

    if (state->action == DO_SHADING) {
	CGContextDrawShading (surface->cgContext, state->shading);
	return;
    }

    CGContextTranslateCTM (surface->cgContext, 0, state->imageRect.size.height);
    CGContextScaleCTM (surface->cgContext, 1, -1);

    if (state->action == DO_IMAGE) {
	CGContextDrawImage (surface->cgContext, state->imageRect, state->image);
	if (!_cairo_operator_bounded_by_source (op)) {
	    CGContextBeginPath (surface->cgContext);
	    CGContextAddRect (surface->cgContext, state->imageRect);
	    CGContextAddRect (surface->cgContext, CGContextGetClipBoundingBox (surface->cgContext));
	    CGContextSetRGBFillColor (surface->cgContext, 0, 0, 0, 0);
	    CGContextEOFillPath (surface->cgContext);
	}
    } else
	CGContextDrawTiledImagePtr (surface->cgContext, state->imageRect, state->image);
}


/*
 * get source/dest image implementation
 */

/* Read the image from the surface's front buffer */
static cairo_int_status_t
_cairo_quartz_get_image (cairo_quartz_surface_t *surface,
			 cairo_image_surface_t **image_out)
{
    unsigned char *imageData;
    cairo_image_surface_t *isurf;

    if (IS_EMPTY (surface)) {
	*image_out = (cairo_image_surface_t*) cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 0, 0);
	return CAIRO_STATUS_SUCCESS;
    }

    if (surface->imageSurfaceEquiv) {
	*image_out = (cairo_image_surface_t*) cairo_surface_reference (surface->imageSurfaceEquiv);
	return CAIRO_STATUS_SUCCESS;
    }

    if (_cairo_quartz_is_cgcontext_bitmap_context (surface->cgContext)) {
	unsigned int stride;
	unsigned int bitinfo;
	unsigned int bpc, bpp;
	CGColorSpaceRef colorspace;
	unsigned int color_comps;

	imageData = (unsigned char *) CGBitmapContextGetData (surface->cgContext);

	bitinfo = CGBitmapContextGetBitmapInfo (surface->cgContext);
	stride = CGBitmapContextGetBytesPerRow (surface->cgContext);
	bpp = CGBitmapContextGetBitsPerPixel (surface->cgContext);
	bpc = CGBitmapContextGetBitsPerComponent (surface->cgContext);

	// let's hope they don't add YUV under us
	colorspace = CGBitmapContextGetColorSpace (surface->cgContext);
	color_comps = CGColorSpaceGetNumberOfComponents (colorspace);

	// XXX TODO: We can handle all of these by converting to
	// pixman masks, including non-native-endian masks
	if (bpc != 8)
	    return CAIRO_INT_STATUS_UNSUPPORTED;

	if (bpp != 32 && bpp != 8)
	    return CAIRO_INT_STATUS_UNSUPPORTED;

	if (color_comps != 3 && color_comps != 1)
	    return CAIRO_INT_STATUS_UNSUPPORTED;

	if (bpp == 32 && color_comps == 3 &&
	    (bitinfo & kCGBitmapAlphaInfoMask) == kCGImageAlphaPremultipliedFirst &&
	    (bitinfo & kCGBitmapByteOrderMask) == kCGBitmapByteOrder32Host)
	{
	    isurf = (cairo_image_surface_t *)
		cairo_image_surface_create_for_data (imageData,
						     CAIRO_FORMAT_ARGB32,
						     surface->extents.width,
						     surface->extents.height,
						     stride);
	} else if (bpp == 32 && color_comps == 3 &&
		   (bitinfo & kCGBitmapAlphaInfoMask) == kCGImageAlphaNoneSkipFirst &&
		   (bitinfo & kCGBitmapByteOrderMask) == kCGBitmapByteOrder32Host)
	{
	    isurf = (cairo_image_surface_t *)
		cairo_image_surface_create_for_data (imageData,
						     CAIRO_FORMAT_RGB24,
						     surface->extents.width,
						     surface->extents.height,
						     stride);
	} else if (bpp == 8 && color_comps == 1)
	{
	    isurf = (cairo_image_surface_t *)
		cairo_image_surface_create_for_data (imageData,
						     CAIRO_FORMAT_A8,
						     surface->extents.width,
						     surface->extents.height,
						     stride);
	} else {
	    return CAIRO_INT_STATUS_UNSUPPORTED;
	}
    } else {
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    *image_out = isurf;
    return CAIRO_STATUS_SUCCESS;
}

/*
 * Cairo surface backend implementations
 */

static cairo_status_t
_cairo_quartz_surface_finish (void *abstract_surface)
{
    cairo_quartz_surface_t *surface = (cairo_quartz_surface_t *) abstract_surface;

    ND ((stderr, "_cairo_quartz_surface_finish[%p] cgc: %p\n", surface, surface->cgContext));

    if (IS_EMPTY (surface))
	return CAIRO_STATUS_SUCCESS;

    /* Restore our saved gstate that we use to reset clipping */
    CGContextRestoreGState (surface->cgContext);
    _cairo_surface_clipper_reset (&surface->clipper);

    CGContextRelease (surface->cgContext);

    surface->cgContext = NULL;

    if (surface->imageSurfaceEquiv) {
	cairo_surface_destroy (surface->imageSurfaceEquiv);
	surface->imageSurfaceEquiv = NULL;
    }

    if (surface->imageData) {
	free (surface->imageData);
	surface->imageData = NULL;
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_quartz_surface_acquire_source_image (void *abstract_surface,
					     cairo_image_surface_t **image_out,
					     void **image_extra)
{
    cairo_int_status_t status;
    cairo_quartz_surface_t *surface = (cairo_quartz_surface_t *) abstract_surface;

    //ND ((stderr, "%p _cairo_quartz_surface_acquire_source_image\n", surface));

    status = _cairo_quartz_get_image (surface, image_out);
    if (unlikely (status))
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    *image_extra = NULL;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_surface_t *
_cairo_quartz_surface_snapshot (void *abstract_surface)
{
    cairo_int_status_t status;
    cairo_quartz_surface_t *surface = abstract_surface;
    cairo_image_surface_t *image;

    if (surface->imageSurfaceEquiv)
	return NULL;

    status = _cairo_quartz_get_image (surface, &image);
    if (unlikely (status))
        return _cairo_surface_create_in_error (CAIRO_STATUS_NO_MEMORY);

    return &image->base;
}

static void
_cairo_quartz_surface_release_source_image (void *abstract_surface,
					    cairo_image_surface_t *image,
					    void *image_extra)
{
    cairo_surface_destroy (&image->base);
}


static cairo_status_t
_cairo_quartz_surface_acquire_dest_image (void *abstract_surface,
					  cairo_rectangle_int_t *interest_rect,
					  cairo_image_surface_t **image_out,
					  cairo_rectangle_int_t *image_rect,
					  void **image_extra)
{
    cairo_quartz_surface_t *surface = (cairo_quartz_surface_t *) abstract_surface;
    cairo_int_status_t status;

    ND ((stderr, "%p _cairo_quartz_surface_acquire_dest_image\n", surface));

    status = _cairo_quartz_get_image (surface, image_out);
    if (unlikely (status))
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    *image_rect = surface->extents;
    *image_extra = NULL;

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_quartz_surface_release_dest_image (void *abstract_surface,
					  cairo_rectangle_int_t *interest_rect,
					  cairo_image_surface_t *image,
					  cairo_rectangle_int_t *image_rect,
					  void *image_extra)
{
    //cairo_quartz_surface_t *surface = (cairo_quartz_surface_t *) abstract_surface;

    //ND ((stderr, "%p _cairo_quartz_surface_release_dest_image\n", surface));

    cairo_surface_destroy (&image->base);
}

static cairo_surface_t *
_cairo_quartz_surface_create_similar (void *abstract_surface,
				      cairo_content_t content,
				      int width,
				      int height)
{
    cairo_quartz_surface_t *surface, *similar_quartz;
    cairo_surface_t *similar;
    cairo_format_t format;

    if (content == CAIRO_CONTENT_COLOR_ALPHA)
	format = CAIRO_FORMAT_ARGB32;
    else if (content == CAIRO_CONTENT_COLOR)
	format = CAIRO_FORMAT_RGB24;
    else if (content == CAIRO_CONTENT_ALPHA)
	format = CAIRO_FORMAT_A8;
    else
	return NULL;

    // verify width and height of surface
    if (!_cairo_quartz_verify_surface_size (width, height)) {
	return _cairo_surface_create_in_error (_cairo_error
					       (CAIRO_STATUS_INVALID_SIZE));
    }

    similar = cairo_quartz_surface_create (format, width, height);
    if (unlikely (similar->status))
	return similar;

    surface = (cairo_quartz_surface_t *) abstract_surface;
    similar_quartz = (cairo_quartz_surface_t *) similar;
    similar_quartz->virtual_extents = surface->virtual_extents;

    return similar;
}

static cairo_status_t
_cairo_quartz_surface_clone_similar (void *abstract_surface,
				     cairo_surface_t *src,
				     int              src_x,
				     int              src_y,
				     int              width,
				     int              height,
				     int             *clone_offset_x,
				     int             *clone_offset_y,
				     cairo_surface_t **clone_out)
{
    cairo_quartz_surface_t *new_surface = NULL;
    cairo_format_t new_format;
    CGImageRef quartz_image = NULL;
    cairo_status_t status;

    *clone_out = NULL;

    // verify width and height of surface
    if (!_cairo_quartz_verify_surface_size (width, height))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if (width == 0 || height == 0) {
	*clone_out = &_cairo_quartz_surface_create_internal (NULL, CAIRO_CONTENT_COLOR_ALPHA,
							     width, height)->base;
	*clone_offset_x = 0;
	*clone_offset_y = 0;
	return CAIRO_STATUS_SUCCESS;
    }

    if (_cairo_surface_is_quartz (src)) {
	cairo_quartz_surface_t *qsurf = (cairo_quartz_surface_t *) src;

	if (IS_EMPTY (qsurf)) {
	    *clone_out = &_cairo_quartz_surface_create_internal (NULL,
								 CAIRO_CONTENT_COLOR_ALPHA,
								 qsurf->extents.width,
								 qsurf->extents.height)->base;
	    *clone_offset_x = 0;
	    *clone_offset_y = 0;
	    return CAIRO_STATUS_SUCCESS;
	}
    }

    status = _cairo_surface_to_cgimage (src, &quartz_image);
    if (unlikely (status))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    new_format = CAIRO_FORMAT_ARGB32;  /* assumed */
    if (_cairo_surface_is_image (src))
	new_format = ((cairo_image_surface_t *) src)->format;

    new_surface = (cairo_quartz_surface_t *)
	cairo_quartz_surface_create (new_format, width, height);

    if (quartz_image == NULL)
	goto FINISH;

    if (!new_surface || new_surface->base.status) {
	CGImageRelease (quartz_image);
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    CGContextSaveGState (new_surface->cgContext);

    _cairo_quartz_surface_set_cairo_operator (new_surface, CAIRO_OPERATOR_SOURCE);

    CGContextTranslateCTM (new_surface->cgContext, -src_x, -src_y);
    CGContextDrawImage (new_surface->cgContext,
			CGRectMake (0, 0, CGImageGetWidth (quartz_image), CGImageGetHeight (quartz_image)),
			quartz_image);

    CGContextRestoreGState (new_surface->cgContext);

    CGImageRelease (quartz_image);

FINISH:
    *clone_offset_x = src_x;
    *clone_offset_y = src_y;
    *clone_out = &new_surface->base;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_bool_t
_cairo_quartz_surface_get_extents (void *abstract_surface,
				   cairo_rectangle_int_t *extents)
{
    cairo_quartz_surface_t *surface = (cairo_quartz_surface_t *) abstract_surface;

    *extents = surface->extents;
    return TRUE;
}

static cairo_int_status_t
_cairo_quartz_surface_paint_cg (cairo_quartz_surface_t *surface,
				cairo_operator_t op,
				const cairo_pattern_t *source,
				cairo_clip_t *clip)
{
    cairo_quartz_drawing_state_t state;
    cairo_int_status_t rv = CAIRO_STATUS_SUCCESS;

    ND ((stderr, "%p _cairo_quartz_surface_paint op %d source->type %d\n", surface, op, source->type));

    if (IS_EMPTY (surface))
	return CAIRO_INT_STATUS_NOTHING_TO_DO;

    rv = _cairo_surface_clipper_set_clip (&surface->clipper, clip);
    if (unlikely (rv))
	return rv;

    rv = _cairo_quartz_setup_source_safe (&state, surface, op, source);
    if (unlikely (rv))
	return rv;

    if (state.action == DO_DIRECT) {
	CGContextFillRect (surface->cgContext, CGRectMake (surface->extents.x,
							   surface->extents.y,
							   surface->extents.width,
							   surface->extents.height));
    } else {
	_cairo_quartz_draw_source (surface, op, &state);
    }

    _cairo_quartz_teardown_source (&state, surface);

    ND ((stderr, "-- paint\n"));
    return rv;
}

static cairo_int_status_t
_cairo_quartz_surface_paint (void *abstract_surface,
			     cairo_operator_t op,
			     const cairo_pattern_t *source,
			     cairo_clip_t *clip)
{
    cairo_quartz_surface_t *surface = (cairo_quartz_surface_t *) abstract_surface;
    cairo_int_status_t rv;
    cairo_image_surface_t *image;

    rv = _cairo_quartz_surface_paint_cg (surface,
					 op,
					 source,
					 clip);

    if (likely (rv != CAIRO_INT_STATUS_UNSUPPORTED))
	return rv;

    rv = _cairo_quartz_get_image (surface, &image);
    if (likely (rv == CAIRO_STATUS_SUCCESS)) {
	rv = _cairo_surface_paint (&image->base, op, source, clip);
	cairo_surface_destroy (&image->base);
    }

    return rv;
}

static cairo_int_status_t
_cairo_quartz_surface_fill_cg (cairo_quartz_surface_t *surface,
			       cairo_operator_t op,
			       const cairo_pattern_t *source,
			       cairo_path_fixed_t *path,
			       cairo_fill_rule_t fill_rule,
			       double tolerance,
			       cairo_antialias_t antialias,
			       cairo_clip_t *clip)
{
    cairo_quartz_drawing_state_t state;
    cairo_int_status_t rv = CAIRO_STATUS_SUCCESS;
    CGPathRef path_for_unbounded = NULL;

    ND ((stderr, "%p _cairo_quartz_surface_fill op %d source->type %d\n", surface, op, source->type));

    if (IS_EMPTY (surface))
	return CAIRO_INT_STATUS_NOTHING_TO_DO;

    rv = _cairo_surface_clipper_set_clip (&surface->clipper, clip);
    if (unlikely (rv))
	return rv;

    rv = _cairo_quartz_setup_source_safe (&state, surface, op, source);
    if (unlikely (rv))
	return rv;

    CGContextSetShouldAntialias (surface->cgContext, (antialias != CAIRO_ANTIALIAS_NONE));

    _cairo_quartz_cairo_path_to_quartz_context (path, surface->cgContext);

    if (!_cairo_operator_bounded_by_mask (op) && CGContextCopyPathPtr)
	path_for_unbounded = CGContextCopyPathPtr (surface->cgContext);

    if (state.action == DO_DIRECT) {
	if (fill_rule == CAIRO_FILL_RULE_WINDING)
	    CGContextFillPath (surface->cgContext);
	else
	    CGContextEOFillPath (surface->cgContext);
    } else {
	if (fill_rule == CAIRO_FILL_RULE_WINDING)
	    CGContextClip (surface->cgContext);
	else
	    CGContextEOClip (surface->cgContext);

	_cairo_quartz_draw_source (surface, op, &state);
    }

    _cairo_quartz_teardown_source (&state, surface);

    if (path_for_unbounded) {
	unbounded_op_data_t ub;
	ub.op = UNBOUNDED_STROKE_FILL;
	ub.u.stroke_fill.cgPath = path_for_unbounded;
	ub.u.stroke_fill.fill_rule = fill_rule;

	_cairo_quartz_fixup_unbounded_operation (surface, &ub, antialias);
	CGPathRelease (path_for_unbounded);
    }

    ND ((stderr, "-- fill\n"));
    return rv;
}

static cairo_int_status_t
_cairo_quartz_surface_fill (void *abstract_surface,
			    cairo_operator_t op,
			    const cairo_pattern_t *source,
			    cairo_path_fixed_t *path,
			    cairo_fill_rule_t fill_rule,
			    double tolerance,
			    cairo_antialias_t antialias,
			    cairo_clip_t *clip)
{
    cairo_quartz_surface_t *surface = (cairo_quartz_surface_t *) abstract_surface;
    cairo_int_status_t rv;
    cairo_image_surface_t *image;

    rv = _cairo_quartz_surface_fill_cg (surface,
					op,
					source,
					path,
					fill_rule,
					tolerance,
					antialias,
					clip);

    if (likely (rv != CAIRO_INT_STATUS_UNSUPPORTED))
	return rv;

    rv = _cairo_quartz_get_image (surface, &image);
    if (likely (rv == CAIRO_STATUS_SUCCESS)) {
	rv = _cairo_surface_fill (&image->base, op, source,
				  path, fill_rule, tolerance, antialias,
				  clip);
	cairo_surface_destroy (&image->base);
    }

    return rv;
}

static cairo_int_status_t
_cairo_quartz_surface_stroke_cg (cairo_quartz_surface_t *surface,
				 cairo_operator_t op,
				 const cairo_pattern_t *source,
				 cairo_path_fixed_t *path,
				 const cairo_stroke_style_t *style,
				 const cairo_matrix_t *ctm,
				 const cairo_matrix_t *ctm_inverse,
				 double tolerance,
				 cairo_antialias_t antialias,
				 cairo_clip_t *clip)
{
    cairo_quartz_drawing_state_t state;
    cairo_int_status_t rv = CAIRO_STATUS_SUCCESS;
    CGAffineTransform origCTM, strokeTransform;
    CGPathRef path_for_unbounded = NULL;

    ND ((stderr, "%p _cairo_quartz_surface_stroke op %d source->type %d\n", surface, op, source->type));

    if (IS_EMPTY (surface))
	return CAIRO_INT_STATUS_NOTHING_TO_DO;

    rv = _cairo_surface_clipper_set_clip (&surface->clipper, clip);
    if (unlikely (rv))
	return rv;

    // Turning antialiasing off used to cause misrendering with
    // single-pixel lines (e.g. 20,10.5 -> 21,10.5 end up being rendered as 2 pixels).
    // That's been since fixed in at least 10.5, and in the latest 10.4 dot releases.
    CGContextSetShouldAntialias (surface->cgContext, (antialias != CAIRO_ANTIALIAS_NONE));
    CGContextSetLineWidth (surface->cgContext, style->line_width);
    CGContextSetLineCap (surface->cgContext, _cairo_quartz_cairo_line_cap_to_quartz (style->line_cap));
    CGContextSetLineJoin (surface->cgContext, _cairo_quartz_cairo_line_join_to_quartz (style->line_join));
    CGContextSetMiterLimit (surface->cgContext, style->miter_limit);

    origCTM = CGContextGetCTM (surface->cgContext);

    if (style->dash && style->num_dashes) {
	cairo_quartz_float_t sdash[CAIRO_STACK_ARRAY_LENGTH (cairo_quartz_float_t)];
	cairo_quartz_float_t *fdash = sdash;
	unsigned int max_dashes = style->num_dashes;
	unsigned int k;

	if (style->num_dashes%2)
	    max_dashes *= 2;
	if (max_dashes > ARRAY_LENGTH (sdash))
	    fdash = _cairo_malloc_ab (max_dashes, sizeof (cairo_quartz_float_t));
	if (unlikely (fdash == NULL))
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);

	for (k = 0; k < max_dashes; k++)
	    fdash[k] = (cairo_quartz_float_t) style->dash[k % style->num_dashes];

	CGContextSetLineDash (surface->cgContext, style->dash_offset, fdash, max_dashes);
	if (fdash != sdash)
	    free (fdash);
    } else
	CGContextSetLineDash (surface->cgContext, 0, NULL, 0);

    rv = _cairo_quartz_setup_source_safe (&state, surface, op, source);
    if (unlikely (rv))
	return rv;

    _cairo_quartz_cairo_path_to_quartz_context (path, surface->cgContext);

    if (!_cairo_operator_bounded_by_mask (op) && CGContextCopyPathPtr)
	path_for_unbounded = CGContextCopyPathPtr (surface->cgContext);

    _cairo_quartz_cairo_matrix_to_quartz (ctm, &strokeTransform);
    CGContextConcatCTM (surface->cgContext, strokeTransform);

    if (state.action == DO_DIRECT) {
	CGContextStrokePath (surface->cgContext);
    } else {
	CGContextReplacePathWithStrokedPath (surface->cgContext);
	CGContextClip (surface->cgContext);

	CGContextSetCTM (surface->cgContext, origCTM);
	_cairo_quartz_draw_source (surface, op, &state);
    }

    _cairo_quartz_teardown_source (&state, surface);

    if (path_for_unbounded) {
	unbounded_op_data_t ub;
	ub.op = UNBOUNDED_STROKE_FILL;
	ub.u.stroke_fill.fill_rule = CAIRO_FILL_RULE_WINDING;

	CGContextBeginPath (surface->cgContext);
	CGContextAddPath (surface->cgContext, path_for_unbounded);
	CGPathRelease (path_for_unbounded);

	CGContextSaveGState (surface->cgContext);
	CGContextConcatCTM (surface->cgContext, strokeTransform);
	CGContextReplacePathWithStrokedPath (surface->cgContext);
	CGContextRestoreGState (surface->cgContext);

	ub.u.stroke_fill.cgPath = CGContextCopyPathPtr (surface->cgContext);

	_cairo_quartz_fixup_unbounded_operation (surface, &ub, antialias);
	CGPathRelease (ub.u.stroke_fill.cgPath);
    }

    ND ((stderr, "-- stroke\n"));
    return rv;
}

static cairo_int_status_t
_cairo_quartz_surface_stroke (void *abstract_surface,
			      cairo_operator_t op,
			      const cairo_pattern_t *source,
			      cairo_path_fixed_t *path,
			      const cairo_stroke_style_t *style,
			      const cairo_matrix_t *ctm,
			      const cairo_matrix_t *ctm_inverse,
			      double tolerance,
			      cairo_antialias_t antialias,
			      cairo_clip_t *clip)
{
    cairo_quartz_surface_t *surface = (cairo_quartz_surface_t *) abstract_surface;
    cairo_int_status_t rv;
    cairo_image_surface_t *image;

    rv = _cairo_quartz_surface_stroke_cg (surface, op, source,
					  path, style, ctm, ctm_inverse,
					  tolerance, antialias,
					  clip);

    if (likely (rv != CAIRO_INT_STATUS_UNSUPPORTED))
	return rv;

    rv = _cairo_quartz_get_image (surface, &image);
    if (likely (rv == CAIRO_STATUS_SUCCESS)) {
	rv = _cairo_surface_stroke (&image->base, op, source,
				    path, style, ctm, ctm_inverse,
				    tolerance, antialias,
				    clip);
	cairo_surface_destroy (&image->base);
    }

    return rv;
}

#if CAIRO_HAS_QUARTZ_FONT
static cairo_int_status_t
_cairo_quartz_surface_show_glyphs_cg (cairo_quartz_surface_t *surface,
				      cairo_operator_t op,
				      const cairo_pattern_t *source,
				      cairo_glyph_t *glyphs,
				      int num_glyphs,
				      cairo_scaled_font_t *scaled_font,
				      cairo_clip_t *clip,
				      int *remaining_glyphs)
{
    CGAffineTransform textTransform, ctm, invTextTransform;
    CGGlyph glyphs_static[CAIRO_STACK_ARRAY_LENGTH (CGSize)];
    CGSize cg_advances_static[CAIRO_STACK_ARRAY_LENGTH (CGSize)];
    CGGlyph *cg_glyphs = &glyphs_static[0];
    CGSize *cg_advances = &cg_advances_static[0];
    COMPILE_TIME_ASSERT (sizeof (CGGlyph) <= sizeof (CGSize));

    cairo_quartz_drawing_state_t state;
    cairo_int_status_t rv = CAIRO_STATUS_SUCCESS;
    cairo_quartz_float_t xprev, yprev;
    int i;
    CGFontRef cgfref = NULL;

    cairo_bool_t isClipping = FALSE;
    cairo_bool_t didForceFontSmoothing = FALSE;

    if (IS_EMPTY (surface))
	return CAIRO_INT_STATUS_NOTHING_TO_DO;

    if (num_glyphs <= 0)
	return CAIRO_INT_STATUS_NOTHING_TO_DO;

    if (cairo_scaled_font_get_type (scaled_font) != CAIRO_FONT_TYPE_QUARTZ)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    rv = _cairo_surface_clipper_set_clip (&surface->clipper, clip);
    if (unlikely (rv))
	return rv;

    rv = _cairo_quartz_setup_source_safe (&state, surface, op, source);
    if (unlikely (rv))
	return rv;

    if (state.action == DO_DIRECT) {
	CGContextSetTextDrawingMode (surface->cgContext, kCGTextFill);
    } else {
	CGContextSetTextDrawingMode (surface->cgContext, kCGTextClip);
	isClipping = TRUE;
    }

    /* this doesn't addref */
    cgfref = _cairo_quartz_scaled_font_get_cg_font_ref (scaled_font);
    CGContextSetFont (surface->cgContext, cgfref);
    CGContextSetFontSize (surface->cgContext, 1.0);

    switch (scaled_font->options.antialias) {
	case CAIRO_ANTIALIAS_SUBPIXEL:
	    CGContextSetShouldAntialias (surface->cgContext, TRUE);
	    CGContextSetShouldSmoothFonts (surface->cgContext, TRUE);
	    if (CGContextSetAllowsFontSmoothingPtr &&
		!CGContextGetAllowsFontSmoothingPtr (surface->cgContext))
	    {
		didForceFontSmoothing = TRUE;
		CGContextSetAllowsFontSmoothingPtr (surface->cgContext, TRUE);
	    }
	    break;
	case CAIRO_ANTIALIAS_NONE:
	    CGContextSetShouldAntialias (surface->cgContext, FALSE);
	    break;
	case CAIRO_ANTIALIAS_GRAY:
	    CGContextSetShouldAntialias (surface->cgContext, TRUE);
	    CGContextSetShouldSmoothFonts (surface->cgContext, FALSE);
	    break;
	case CAIRO_ANTIALIAS_DEFAULT:
	    /* Don't do anything */
	    break;
    }

    if (num_glyphs > ARRAY_LENGTH (glyphs_static)) {
	cg_glyphs = (CGGlyph*) _cairo_malloc_ab (num_glyphs, sizeof (CGGlyph) + sizeof (CGSize));
	if (unlikely (cg_glyphs == NULL)) {
	    rv = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	    goto BAIL;
	}

	cg_advances = (CGSize*) (cg_glyphs + num_glyphs);
    }

    textTransform = CGAffineTransformMake (scaled_font->scale.xx,
					   scaled_font->scale.yx,
					   -scaled_font->scale.xy,
					   -scaled_font->scale.yy,
					   0, 0);
    _cairo_quartz_cairo_matrix_to_quartz (&scaled_font->scale_inverse, &invTextTransform);

    CGContextSetTextMatrix (surface->cgContext, CGAffineTransformIdentity);

    /* Convert our glyph positions to glyph advances.  We need n-1 advances,
     * since the advance at index 0 is applied after glyph 0. */
    xprev = glyphs[0].x;
    yprev = glyphs[0].y;

    cg_glyphs[0] = glyphs[0].index;

    for (i = 1; i < num_glyphs; i++) {
	cairo_quartz_float_t xf = glyphs[i].x;
	cairo_quartz_float_t yf = glyphs[i].y;
	cg_glyphs[i] = glyphs[i].index;
	cg_advances[i - 1] = CGSizeApplyAffineTransform (CGSizeMake (xf - xprev, yf - yprev), invTextTransform);
	xprev = xf;
	yprev = yf;
    }

    /* Translate to the first glyph's position before drawing */
    ctm = CGContextGetCTM (surface->cgContext);
    CGContextTranslateCTM (surface->cgContext, glyphs[0].x, glyphs[0].y);
    CGContextConcatCTM (surface->cgContext, textTransform);

    CGContextShowGlyphsWithAdvances (surface->cgContext,
				     cg_glyphs,
				     cg_advances,
				     num_glyphs);

    CGContextSetCTM (surface->cgContext, ctm);

    if (state.action != DO_DIRECT)
	_cairo_quartz_draw_source (surface, op, &state);

BAIL:
    _cairo_quartz_teardown_source (&state, surface);

    if (didForceFontSmoothing)
	CGContextSetAllowsFontSmoothingPtr (surface->cgContext, FALSE);

    if (rv == CAIRO_STATUS_SUCCESS &&
	cgfref &&
	!_cairo_operator_bounded_by_mask (op))
    {
	unbounded_op_data_t ub;
	ub.op = UNBOUNDED_SHOW_GLYPHS;

	ub.u.show_glyphs.isClipping = isClipping;
	ub.u.show_glyphs.cg_glyphs = cg_glyphs;
	ub.u.show_glyphs.cg_advances = cg_advances;
	ub.u.show_glyphs.nglyphs = num_glyphs;
	ub.u.show_glyphs.textTransform = textTransform;
	ub.u.show_glyphs.font = cgfref;
	ub.u.show_glyphs.origin = CGPointMake (glyphs[0].x, glyphs[0].y);

	_cairo_quartz_fixup_unbounded_operation (surface, &ub, scaled_font->options.antialias);
    }

    if (cg_glyphs != glyphs_static)
	free (cg_glyphs);

    return rv;
}
#endif /* CAIRO_HAS_QUARTZ_FONT */

static cairo_int_status_t
_cairo_quartz_surface_show_glyphs (void *abstract_surface,
				   cairo_operator_t op,
				   const cairo_pattern_t *source,
				   cairo_glyph_t *glyphs,
				   int num_glyphs,
				   cairo_scaled_font_t *scaled_font,
				   cairo_clip_t *clip,
				   int *remaining_glyphs)
{
    cairo_quartz_surface_t *surface = (cairo_quartz_surface_t *) abstract_surface;
    cairo_int_status_t rv = CAIRO_INT_STATUS_UNSUPPORTED;
    cairo_image_surface_t *image;

#if CAIRO_HAS_QUARTZ_FONT
    rv = _cairo_quartz_surface_show_glyphs_cg (surface, op, source,
					       glyphs, num_glyphs,
					       scaled_font, clip, remaining_glyphs);

    if (likely (rv != CAIRO_INT_STATUS_UNSUPPORTED))
	return rv;

#endif

    rv = _cairo_quartz_get_image (surface, &image);
    if (likely (rv == CAIRO_STATUS_SUCCESS)) {
	rv = _cairo_surface_show_text_glyphs (&image->base, op, source,
					      NULL, 0,
					      glyphs, num_glyphs,
					      NULL, 0, 0,
					      scaled_font, clip);
	cairo_surface_destroy (&image->base);
    }

    return rv;
}

static cairo_int_status_t
_cairo_quartz_surface_mask_with_surface (cairo_quartz_surface_t *surface,
                                         cairo_operator_t op,
                                         const cairo_pattern_t *source,
                                         const cairo_surface_pattern_t *mask,
					 cairo_clip_t *clip)
{
    CGRect rect;
    CGImageRef img;
    cairo_surface_t *pat_surf = mask->surface;
    cairo_status_t status = CAIRO_STATUS_SUCCESS;
    CGAffineTransform ctm, mask_matrix;

    status = _cairo_surface_to_cgimage (pat_surf, &img);
    if (unlikely (status))
	return status;
    if (unlikely (img == NULL)) {
	if (!_cairo_operator_bounded_by_mask (op))
	    CGContextClearRect (surface->cgContext, CGContextGetClipBoundingBox (surface->cgContext));
	return CAIRO_STATUS_SUCCESS;
    }

    rect = CGRectMake (0.0f, 0.0f, CGImageGetWidth (img) , CGImageGetHeight (img));

    CGContextSaveGState (surface->cgContext);

    /* ClipToMask is essentially drawing an image, so we need to flip the CTM
     * to get the image to appear oriented the right way */
    ctm = CGContextGetCTM (surface->cgContext);

    _cairo_quartz_cairo_matrix_to_quartz (&mask->base.matrix, &mask_matrix);
    mask_matrix = CGAffineTransformInvert (mask_matrix);
    mask_matrix = CGAffineTransformTranslate (mask_matrix, 0.0, CGImageGetHeight (img));
    mask_matrix = CGAffineTransformScale (mask_matrix, 1.0, -1.0);

    CGContextConcatCTM (surface->cgContext, mask_matrix);
    CGContextClipToMask (surface->cgContext, rect, img);

    CGContextSetCTM (surface->cgContext, ctm);

    status = _cairo_quartz_surface_paint_cg (surface, op, source, clip);

    CGContextRestoreGState (surface->cgContext);

    if (!_cairo_operator_bounded_by_mask (op)) {
	unbounded_op_data_t ub;
	ub.op = UNBOUNDED_MASK;
	ub.u.mask.mask = img;
	ub.u.mask.maskTransform = mask_matrix;
	_cairo_quartz_fixup_unbounded_operation (surface, &ub, CAIRO_ANTIALIAS_NONE);
    }

    CGImageRelease (img);

    return status;
}

/* This is somewhat less than ideal, but it gets the job done;
 * it would be better to avoid calling back into cairo.  This
 * creates a temporary surface to use as the mask.
 */
static cairo_int_status_t
_cairo_quartz_surface_mask_with_generic (cairo_quartz_surface_t *surface,
					 cairo_operator_t op,
					 const cairo_pattern_t *source,
					 const cairo_pattern_t *mask,
					 cairo_clip_t *clip)
{
    cairo_surface_t *gradient_surf;
    cairo_surface_pattern_t surface_pattern;
    cairo_int_status_t status;

    /* Render the gradient to a surface */
    gradient_surf = _cairo_quartz_surface_create_similar (surface,
							  CAIRO_CONTENT_ALPHA,
							  surface->extents.width,
							  surface->extents.height);
    status = gradient_surf->status;
    if (unlikely (status))
	goto BAIL;

    status = _cairo_quartz_surface_paint (gradient_surf, CAIRO_OPERATOR_SOURCE, mask, NULL);
    if (unlikely (status))
	goto BAIL;

    _cairo_pattern_init_for_surface (&surface_pattern, gradient_surf);
    status = _cairo_quartz_surface_mask_with_surface (surface, op, source, &surface_pattern, clip);
    _cairo_pattern_fini (&surface_pattern.base);

 BAIL:
    cairo_surface_destroy (gradient_surf);

    return status;
}

static cairo_int_status_t
_cairo_quartz_surface_mask_cg (cairo_quartz_surface_t *surface,
			       cairo_operator_t op,
			       const cairo_pattern_t *source,
			       const cairo_pattern_t *mask,
			       cairo_clip_t *clip)
{
    cairo_int_status_t rv = CAIRO_STATUS_SUCCESS;

    ND ((stderr, "%p _cairo_quartz_surface_mask op %d source->type %d mask->type %d\n", surface, op, source->type, mask->type));

    if (IS_EMPTY (surface))
	return CAIRO_INT_STATUS_NOTHING_TO_DO;

    rv = _cairo_surface_clipper_set_clip (&surface->clipper, clip);
    if (unlikely (rv))
	return rv;

    if (mask->type == CAIRO_PATTERN_TYPE_SOLID) {
	/* This is easy; we just need to paint with the alpha. */
	cairo_solid_pattern_t *solid_mask = (cairo_solid_pattern_t *) mask;

	CGContextSetAlpha (surface->cgContext, solid_mask->color.alpha);
	rv = _cairo_quartz_surface_paint_cg (surface, op, source, clip);
	CGContextSetAlpha (surface->cgContext, 1.0);

	return rv;
    }

    /* For these, we can skip creating a temporary surface, since we already have one */
    if (mask->type == CAIRO_PATTERN_TYPE_SURFACE && mask->extend == CAIRO_EXTEND_NONE) {
	const cairo_surface_pattern_t *mask_spat =  (const cairo_surface_pattern_t *) mask;

	if (mask_spat->surface->content & CAIRO_CONTENT_ALPHA)
	    return _cairo_quartz_surface_mask_with_surface (surface, op, source, mask_spat, clip);
    }

    return _cairo_quartz_surface_mask_with_generic (surface, op, source, mask, clip);
}

static cairo_int_status_t
_cairo_quartz_surface_mask (void *abstract_surface,
			    cairo_operator_t op,
			    const cairo_pattern_t *source,
			    const cairo_pattern_t *mask,
			    cairo_clip_t *clip)
{
    cairo_quartz_surface_t *surface = (cairo_quartz_surface_t *) abstract_surface;
    cairo_int_status_t rv;
    cairo_image_surface_t *image;

    rv = _cairo_quartz_surface_mask_cg (surface,
					op,
					source,
					mask,
					clip);

    if (likely (rv != CAIRO_INT_STATUS_UNSUPPORTED))
	return rv;

    rv = _cairo_quartz_get_image (surface, &image);
    if (likely (rv == CAIRO_STATUS_SUCCESS)) {
	rv = _cairo_surface_mask (&image->base, op, source, mask, clip);
	cairo_surface_destroy (&image->base);
    }

    return rv;
}

static cairo_status_t
_cairo_quartz_surface_clipper_intersect_clip_path (cairo_surface_clipper_t *clipper,
						   cairo_path_fixed_t *path,
						   cairo_fill_rule_t fill_rule,
						   double tolerance,
						   cairo_antialias_t antialias)
{
    cairo_quartz_surface_t *surface =
	cairo_container_of (clipper, cairo_quartz_surface_t, clipper);

    ND ((stderr, "%p _cairo_quartz_surface_intersect_clip_path path: %p\n", surface, path));

    if (IS_EMPTY (surface))
	return CAIRO_STATUS_SUCCESS;

    if (path == NULL) {
	/* If we're being asked to reset the clip, we can only do it
	 * by restoring the gstate to our previous saved one, and
	 * saving it again.
	 *
	 * Note that this assumes that ALL quartz surface creation
	 * functions will do a SaveGState first; we do this in create_internal.
	 */
	CGContextRestoreGState (surface->cgContext);
	CGContextSaveGState (surface->cgContext);
    } else {
	CGContextSetShouldAntialias (surface->cgContext, (antialias != CAIRO_ANTIALIAS_NONE));

	_cairo_quartz_cairo_path_to_quartz_context (path, surface->cgContext);

	if (fill_rule == CAIRO_FILL_RULE_WINDING)
	    CGContextClip (surface->cgContext);
	else
	    CGContextEOClip (surface->cgContext);
    }

    ND ((stderr, "-- intersect_clip_path\n"));

    return CAIRO_STATUS_SUCCESS;
}

// XXXtodo implement show_page; need to figure out how to handle begin/end

static const struct _cairo_surface_backend cairo_quartz_surface_backend = {
    CAIRO_SURFACE_TYPE_QUARTZ,
    _cairo_quartz_surface_create_similar,
    _cairo_quartz_surface_finish,
    _cairo_quartz_surface_acquire_source_image,
    _cairo_quartz_surface_release_source_image,
    _cairo_quartz_surface_acquire_dest_image,
    _cairo_quartz_surface_release_dest_image,
    _cairo_quartz_surface_clone_similar,
    NULL, /* composite */
    NULL, /* fill_rectangles */
    NULL, /* composite_trapezoids */
    NULL, /* create_span_renderer */
    NULL, /* check_span_renderer */
    NULL, /* copy_page */
    NULL, /* show_page */
    _cairo_quartz_surface_get_extents,
    NULL, /* old_show_glyphs */
    NULL, /* get_font_options */
    NULL, /* flush */
    NULL, /* mark_dirty_rectangle */
    NULL, /* scaled_font_fini */
    NULL, /* scaled_glyph_fini */

    _cairo_quartz_surface_paint,
    _cairo_quartz_surface_mask,
    _cairo_quartz_surface_stroke,
    _cairo_quartz_surface_fill,
    _cairo_quartz_surface_show_glyphs,

    _cairo_quartz_surface_snapshot,
    NULL, /* is_similar */
    NULL  /* fill_stroke */
};

cairo_quartz_surface_t *
_cairo_quartz_surface_create_internal (CGContextRef cgContext,
				       cairo_content_t content,
				       unsigned int width,
				       unsigned int height)
{
    cairo_quartz_surface_t *surface;

    quartz_ensure_symbols ();

    /* Init the base surface */
    surface = malloc (sizeof (cairo_quartz_surface_t));
    if (unlikely (surface == NULL))
	return (cairo_quartz_surface_t*) _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    memset (surface, 0, sizeof (cairo_quartz_surface_t));

    _cairo_surface_init (&surface->base,
			 &cairo_quartz_surface_backend,
			 NULL, /* device */
			 content);

    _cairo_surface_clipper_init (&surface->clipper,
				 _cairo_quartz_surface_clipper_intersect_clip_path);

    /* Save our extents */
    surface->extents.x = surface->extents.y = 0;
    surface->extents.width = width;
    surface->extents.height = height;
    surface->virtual_extents = surface->extents;

    if (IS_EMPTY (surface)) {
	surface->cgContext = NULL;
	surface->cgContextBaseCTM = CGAffineTransformIdentity;
	surface->imageData = NULL;
	return surface;
    }

    /* Save so we can always get back to a known-good CGContext -- this is
     * required for proper behaviour of intersect_clip_path(NULL)
     */
    CGContextSaveGState (cgContext);

    surface->cgContext = cgContext;
    surface->cgContextBaseCTM = CGContextGetCTM (cgContext);

    surface->imageData = NULL;
    surface->imageSurfaceEquiv = NULL;

    return surface;
}

/**
 * cairo_quartz_surface_create_for_cg_context
 * @cgContext: the existing CGContext for which to create the surface
 * @width: width of the surface, in pixels
 * @height: height of the surface, in pixels
 *
 * Creates a Quartz surface that wraps the given CGContext.  The
 * CGContext is assumed to be in the standard Cairo coordinate space
 * (that is, with the origin at the upper left and the Y axis
 * increasing downward).  If the CGContext is in the Quartz coordinate
 * space (with the origin at the bottom left), then it should be
 * flipped before this function is called.  The flip can be accomplished
 * using a translate and a scale; for example:
 *
 * <informalexample><programlisting>
 * CGContextTranslateCTM (cgContext, 0.0, height);
 * CGContextScaleCTM (cgContext, 1.0, -1.0);
 * </programlisting></informalexample>
 *
 * All Cairo operations are implemented in terms of Quartz operations,
 * as long as Quartz-compatible elements are used (such as Quartz fonts).
 *
 * Return value: the newly created Cairo surface.
 *
 * Since: 1.4
 **/

cairo_surface_t *
cairo_quartz_surface_create_for_cg_context (CGContextRef cgContext,
					    unsigned int width,
					    unsigned int height)
{
    cairo_quartz_surface_t *surf;

    surf = _cairo_quartz_surface_create_internal (cgContext, CAIRO_CONTENT_COLOR_ALPHA,
						  width, height);
    if (likely (!surf->base.status))
	CGContextRetain (cgContext);

    return &surf->base;
}

/**
 * cairo_quartz_surface_create
 * @format: format of pixels in the surface to create
 * @width: width of the surface, in pixels
 * @height: height of the surface, in pixels
 *
 * Creates a Quartz surface backed by a CGBitmap.  The surface is
 * created using the Device RGB (or Device Gray, for A8) color space.
 * All Cairo operations, including those that require software
 * rendering, will succeed on this surface.
 *
 * Return value: the newly created surface.
 *
 * Since: 1.4
 **/
cairo_surface_t *
cairo_quartz_surface_create (cairo_format_t format,
			     unsigned int width,
			     unsigned int height)
{
    cairo_quartz_surface_t *surf;
    CGContextRef cgc;
    CGColorSpaceRef cgColorspace;
    CGBitmapInfo bitinfo;
    void *imageData;
    int stride;
    int bitsPerComponent;

    // verify width and height of surface
    if (!_cairo_quartz_verify_surface_size (width, height))
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_SIZE));

    if (width == 0 || height == 0) {
	return &_cairo_quartz_surface_create_internal (NULL, _cairo_content_from_format (format),
						       width, height)->base;
    }

    if (format == CAIRO_FORMAT_ARGB32 ||
	format == CAIRO_FORMAT_RGB24)
    {
	cgColorspace = CGColorSpaceCreateDeviceRGB ();
	bitinfo = kCGBitmapByteOrder32Host;
	if (format == CAIRO_FORMAT_ARGB32)
	    bitinfo |= kCGImageAlphaPremultipliedFirst;
	else
	    bitinfo |= kCGImageAlphaNoneSkipFirst;
	bitsPerComponent = 8;
	stride = width * 4;
    } else if (format == CAIRO_FORMAT_A8) {
	cgColorspace = NULL;
	stride = width;
	bitinfo = kCGImageAlphaOnly;
	bitsPerComponent = 8;
    } else if (format == CAIRO_FORMAT_A1) {
	/* I don't think we can usefully support this, as defined by
	 * cairo_format_t -- these are 1-bit pixels stored in 32-bit
	 * quantities.
	 */
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_FORMAT));
    } else {
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_FORMAT));
    }

    /* The Apple docs say that for best performance, the stride and the data
     * pointer should be 16-byte aligned.  malloc already aligns to 16-bytes,
     * so we don't have to anything special on allocation.
     */
    stride = (stride + 15) & ~15;

    imageData = _cairo_malloc_ab (height, stride);
    if (unlikely (!imageData)) {
	CGColorSpaceRelease (cgColorspace);
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));
    }

    /* zero the memory to match the image surface behaviour */
    memset (imageData, 0, height * stride);

    cgc = CGBitmapContextCreate (imageData,
				 width,
				 height,
				 bitsPerComponent,
				 stride,
				 cgColorspace,
				 bitinfo);
    CGColorSpaceRelease (cgColorspace);

    if (!cgc) {
	free (imageData);
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));
    }

    /* flip the Y axis */
    CGContextTranslateCTM (cgc, 0.0, height);
    CGContextScaleCTM (cgc, 1.0, -1.0);

    surf = _cairo_quartz_surface_create_internal (cgc, _cairo_content_from_format (format),
						  width, height);
    if (surf->base.status) {
	CGContextRelease (cgc);
	free (imageData);
	// create_internal will have set an error
	return &surf->base;
    }

    surf->imageData = imageData;
    surf->imageSurfaceEquiv = cairo_image_surface_create_for_data (imageData, format, width, height, stride);

    return &surf->base;
}

/**
 * cairo_quartz_surface_get_cg_context
 * @surface: the Cairo Quartz surface
 *
 * Returns the CGContextRef that the given Quartz surface is backed
 * by.
 *
 * A call to cairo_surface_flush() is required before using the
 * CGContextRef to ensure that all pending drawing operations are
 * finished and to restore any temporary modification cairo has made
 * to its state. A call to cairo_surface_mark_dirty() is required
 * after the state or the content of the CGContextRef has been
 * modified.
 *
 * Return value: the CGContextRef for the given surface.
 *
 * Since: 1.4
 **/
CGContextRef
cairo_quartz_surface_get_cg_context (cairo_surface_t *surface)
{
    if (surface && _cairo_surface_is_quartz (surface)) {
	cairo_quartz_surface_t *quartz = (cairo_quartz_surface_t *) surface;
	return quartz->cgContext;
    } else
	return NULL;
}

static cairo_bool_t
_cairo_surface_is_quartz (const cairo_surface_t *surface)
{
    return surface->backend == &cairo_quartz_surface_backend;
}

/* Debug stuff */

#ifdef QUARTZ_DEBUG

#include <Movies.h>

void ExportCGImageToPNGFile (CGImageRef inImageRef, char* dest)
{
    Handle  dataRef = NULL;
    OSType  dataRefType;
    CFStringRef inPath = CFStringCreateWithCString (NULL, dest, kCFStringEncodingASCII);

    GraphicsExportComponent grex = 0;
    unsigned long sizeWritten;

    ComponentResult result;

    // create the data reference
    result = QTNewDataReferenceFromFullPathCFString (inPath, kQTNativeDefaultPathStyle,
						     0, &dataRef, &dataRefType);

    if (NULL != dataRef && noErr == result) {
	// get the PNG exporter
	result = OpenADefaultComponent (GraphicsExporterComponentType, kQTFileTypePNG,
					&grex);

	if (grex) {
	    // tell the exporter where to find its source image
	    result = GraphicsExportSetInputCGImage (grex, inImageRef);

	    if (noErr == result) {
		// tell the exporter where to save the exporter image
		result = GraphicsExportSetOutputDataReference (grex, dataRef,
							       dataRefType);

		if (noErr == result) {
		    // write the PNG file
		    result = GraphicsExportDoExport (grex, &sizeWritten);
		}
	    }

	    // remember to close the component
	    CloseComponent (grex);
	}

	// remember to dispose of the data reference handle
	DisposeHandle (dataRef);
    }
}

void
quartz_image_to_png (CGImageRef imgref, char *dest)
{
    static int sctr = 0;
    char sptr[] = "/Users/vladimir/Desktop/barXXXXX.png";

    if (dest == NULL) {
	fprintf (stderr, "** Writing %p to bar%d\n", imgref, sctr);
	sprintf (sptr, "/Users/vladimir/Desktop/bar%d.png", sctr);
	sctr++;
	dest = sptr;
    }

    ExportCGImageToPNGFile (imgref, dest);
}

void
quartz_surface_to_png (cairo_quartz_surface_t *nq, char *dest)
{
    static int sctr = 0;
    char sptr[] = "/Users/vladimir/Desktop/fooXXXXX.png";

    if (nq->base.type != CAIRO_SURFACE_TYPE_QUARTZ) {
	fprintf (stderr, "** quartz_surface_to_png: surface %p isn't quartz!\n", nq);
	return;
    }

    if (dest == NULL) {
	fprintf (stderr, "** Writing %p to foo%d\n", nq, sctr);
	sprintf (sptr, "/Users/vladimir/Desktop/foo%d.png", sctr);
	sctr++;
	dest = sptr;
    }

    CGImageRef imgref = CGBitmapContextCreateImage (nq->cgContext);
    if (imgref == NULL) {
	fprintf (stderr, "quartz surface at %p is not a bitmap context!\n", nq);
	return;
    }

    ExportCGImageToPNGFile (imgref, dest);

    CGImageRelease (imgref);
}

#endif /* QUARTZ_DEBUG */
