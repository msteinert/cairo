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

/*
 * These definitions are solely for use by the implementation of Xr
 * and constitute no kind of standard.  If you need any of these
 * functions, please drop me a note.  Either the library needs new
 * functionality, or there's a way to do what you need using the
 * existing published interfaces. cworth@isi.edu
 */

#ifndef _XRINT_H_
#define _XRINT_H_

#include <math.h>
#include <X11/Xlibint.h>
#include <X11/Xft/Xft.h>

#include "Xr.h"

#ifndef __GCC__
#define __attribute__(x)
#endif

/* Sure wish C had a real enum type so that this would be distinct
   from XrStatus. Oh well, without that, I'll use this bogus 1000
   offset */
typedef enum _XrIntStatus {
    XrIntStatusDegenerate = 1000
} XrIntStatus;

typedef enum _XrPathOp {
    XrPathOpMoveTo = 0,
    XrPathOpLineTo = 1,
    XrPathOpCurveTo = 2,
    XrPathOpClosePath = 3
} __attribute__ ((packed)) XrPathOp; /* Don't want 32 bits if we can avoid it. */

typedef enum _XrPathDirection {
    XrPathDirectionForward,
    XrPathDirectionReverse
} XrPathDirection;

typedef enum _XrSubPathDone {
    XrSubPathDoneCap,
    XrSubPathDoneJoin
} XrSubPathDone;

typedef struct _XrPathCallbacks {
    XrStatus (*AddEdge)(void *closure, XPointFixed *p1, XPointFixed *p2);
    XrStatus (*AddSpline)(void *closure, XPointFixed *a, XPointFixed *b, XPointFixed *c, XPointFixed *d);
    XrStatus (*DoneSubPath) (void *closure, XrSubPathDone done);
    XrStatus (*DonePath) (void *closure);
} XrPathCallbacks;

#define XR_PATH_BUF_SZ 64

typedef struct _XrPathOpBuf {
    int num_ops;
    XrPathOp op[XR_PATH_BUF_SZ];

    struct _XrPathOpBuf *next, *prev;
} XrPathOpBuf;

typedef struct _XrPathArgBuf {
    int num_pts;
    XPointFixed pt[XR_PATH_BUF_SZ];

    struct _XrPathArgBuf *next, *prev;
} XrPathArgBuf;

typedef struct _XrPath {
    XrPathOpBuf *op_head;
    XrPathOpBuf *op_tail;

    XrPathArgBuf *arg_head;
    XrPathArgBuf *arg_tail;
} XrPath;

typedef struct _XrEdge {
    XLineFixed edge;
    Bool clockWise;

    XFixed current_x;
    struct _XrEdge *next, *prev;
} XrEdge;

typedef struct _XrPolygon {
    int num_edges;
    int edges_size;
    XrEdge *edges;

    XPointFixed first_pt;
    int first_pt_defined;
    XPointFixed last_pt;
    int last_pt_defined;

    int closed;
} XrPolygon;

typedef struct _XrSlopeFixed
{
    XFixed dx;
    XFixed dy;
} XrSlopeFixed;

typedef struct _XrSpline {
    XPointFixed a, b, c, d;

    XrSlopeFixed initial_slope;
    XrSlopeFixed final_slope;

    int num_pts;
    int pts_size;
    XPointFixed *pts;
} XrSpline;

typedef enum _XrPenVertexFlag {
    XrPenVertexFlagNone		= 0,
    XrPenVertexFlagForward	= 1,
    XrPenVertexFlagReverse	= 2
} XrPenVertexFlag;

typedef struct _XrPenFlaggedPoint {
    XPointFixed pt;
    XrPenVertexFlag flag;
} XrPenFlaggedPoint;

typedef struct _XrPenVertex {
    XPointFixed pt;
    XrPenVertexFlag flag;

    double theta;
    XrSlopeFixed slope_ccw;
    XrSlopeFixed slope_cw;
} XrPenVertex;

typedef struct _XrPen {
    double radius;
    double tolerance;

    int num_vertices;
    XrPenVertex *vertex;
} XrPen;

typedef struct _XrSurface {
    Display *dpy;

    Drawable drawable;

    unsigned int depth;

    unsigned long xc_sa_mask;
    XcSurfaceAttributes xc_sa;
    XcFormat *xc_format;

    XcSurface *xc_surface;

    unsigned int ref_count;
} XrSurface;

typedef struct _XrColor {
    double red;
    double green;
    double blue;
    double alpha;

    XcColor xc_color;
} XrColor;

typedef struct _XrTransform {
    double m[3][2];
} XrTransform;

typedef struct _XrTraps {
    int num_xtraps;
    int xtraps_size;
    XTrapezoid *xtraps;
} XrTraps;

/* XXX: What should this really be? */
#define XR_FONT_KEY_DEFAULT		"mono"

typedef struct _XrFont {
    unsigned char *key;

    double scale;
    int has_transform;
    XrTransform transform;

    Display *dpy;
    XftFont *xft_font;
} XrFont;


#define XR_GSTATE_OPERATOR_DEFAULT	XrOperatorOver
#define XR_GSTATE_TOLERANCE_DEFAULT	0.1
#define XR_GSTATE_FILL_RULE_DEFAULT	XrFillRuleWinding
#define XR_GSTATE_LINE_WIDTH_DEFAULT	2.0
#define XR_GSTATE_LINE_CAP_DEFAULT	XrLineCapButt
#define XR_GSTATE_LINE_JOIN_DEFAULT	XrLineJoinMiter
#define XR_GSTATE_MITER_LIMIT_DEFAULT	10.0

typedef struct _XrGState {
    Display *dpy;

    double tolerance;

    /* stroke style */
    double line_width;
    XrLineCap line_cap;
    XrLineJoin line_join;
    double *dashes;
    int ndashes;
    double dash_offset;
    double miter_limit;

    XrFillRule fill_rule;

    XrOperator operator;
    
    XcFormat *solidFormat;
    XcFormat *alphaFormat;

    XrFont font;

    XrColor color;
    XrSurface src;
    XrSurface surface;

    XrTransform ctm;
    XrTransform ctm_inverse;

    XrPath path;

    XPointDouble last_move_pt;
    XPointDouble current_pt;

    XrPen pen_regular;

    struct _XrGState *next;
} XrGState;

struct _XrState {
    Display *dpy;
    XrGState *stack;
    XrStatus status;
};

typedef struct _XrStrokeFace {
    XPointFixed ccw;
    XPointFixed pt;
    XPointFixed cw;
    XPointDouble vector;
} XrStrokeFace;

typedef struct _XrStroker {
    XrGState *gstate;
    XrTraps *traps;

    int have_prev;
    int have_first;
    int is_first;
    XrStrokeFace prev;
    XrStrokeFace first;
    int dash_index;
    int dash_on;
    double dash_remain;
} XrStroker;

typedef struct _XrFiller {
    XrGState *gstate;
    XrTraps *traps;

    XrPolygon polygon;
} XrFiller;

/* xrstate.c */

XrState *
_XrStateCreate(Display *dpy);

XrStatus
_XrStateInit(XrState *state, Display *dpy);

void
_XrStateDeinit(XrState *xrs);

void
_XrStateDestroy(XrState *state);

XrStatus
_XrStatePush(XrState *xrs);

XrStatus
_XrStatePop(XrState *xrs);

/* xrgstate.c */
XrGState *
_XrGStateCreate(Display *dpy);

void
_XrGStateInit(XrGState *gstate, Display *dpy);

XrStatus
_XrGStateInitCopy(XrGState *gstate, XrGState *other);

void
_XrGStateDeinit(XrGState *gstate);

void
_XrGStateDestroy(XrGState *gstate);

XrGState *
_XrGStateClone(XrGState *gstate);

XrStatus
_XrGStateSetDrawable(XrGState *gstate, Drawable drawable);

XrStatus
_XrGStateSetVisual(XrGState *gstate, Visual *visual);

XrStatus
_XrGStateSetFormat(XrGState *gstate, XrFormat format);

XrStatus
_XrGStateSetOperator(XrGState *gstate, XrOperator operator);

XrStatus
_XrGStateSetRGBColor(XrGState *gstate, double red, double green, double blue);

XrStatus
_XrGStateSetTolerance(XrGState *gstate, double tolerance);

XrStatus
_XrGStateSetAlpha(XrGState *gstate, double alpha);

XrStatus
_XrGStateSetFillRule(XrGState *gstate, XrFillRule fill_rule);

XrStatus
_XrGStateSetLineWidth(XrGState *gstate, double width);

XrStatus
_XrGStateSetLineCap(XrGState *gstate, XrLineCap line_cap);

XrStatus
_XrGStateSetLineJoin(XrGState *gstate, XrLineJoin line_join);

XrStatus
_XrGStateSetDash(XrGState *gstate, double *dashes, int ndash, double offset);

XrStatus
_XrGStateSetMiterLimit(XrGState *gstate, double limit);

XrStatus
_XrGStateTranslate(XrGState *gstate, double tx, double ty);

XrStatus
_XrGStateScale(XrGState *gstate, double sx, double sy);

XrStatus
_XrGStateRotate(XrGState *gstate, double angle);

XrStatus
_XrGStateConcatMatrix(XrGState *gstate,
		      double a, double b,
		      double c, double d,
		      double tx, double ty);

XrStatus
_XrGStateNewPath(XrGState *gstate);

XrStatus
_XrGStateMoveTo(XrGState *gstate, double x, double y);

XrStatus
_XrGStateLineTo(XrGState *gstate, double x, double y);

XrStatus
_XrGStateCurveTo(XrGState *gstate,
		 double x1, double y1,
		 double x2, double y2,
		 double x3, double y3);

XrStatus
_XrGStateRelMoveTo(XrGState *gstate, double dx, double dy);

XrStatus
_XrGStateRelLineTo(XrGState *gstate, double dx, double dy);

XrStatus
_XrGStateRelCurveTo(XrGState *gstate,
		    double dx1, double dy1,
		    double dx2, double dy2,
		    double dx3, double dy3);

XrStatus
_XrGStateClosePath(XrGState *gstate);

XrStatus
_XrGStateStroke(XrGState *gstate);

XrStatus
_XrGStateFill(XrGState *fill);

XrStatus
_XrGStateSelectFont(XrGState *gstate, const char *key);

XrStatus
_XrGStateScaleFont(XrGState *gstate, double scale);

XrStatus
_XrGStateTransformFont(XrGState *gstate,
		       double a, double b,
		       double c, double d);

XrStatus
_XrGStateTextExtents(XrGState *gstate,
		     const unsigned char *utf8,
		     double *x, double *y,
		     double *width, double *height,
		     double *dx, double *dy);

XrStatus
_XrGStateShowText(XrGState *gstate, const unsigned char *utf8);

/* xrcolor.c */
void
_XrColorInit(XrColor *color);

void
_XrColorDeinit(XrColor *color);

void
_XrColorSetRGB(XrColor *color, double red, double green, double blue);

void
_XrColorSetAlpha(XrColor *color, double alpha);

/* xrfont.c */

void
_XrFontInit(XrFont *font, XrGState *gstate);

XrStatus
_XrFontInitCopy(XrFont *font, XrFont *other);

void
_XrFontDeinit(XrFont *font);

XrStatus
_XrFontSelect(XrFont *font, const char *key);

XrStatus
_XrFontScale(XrFont *font, double scale);

XrStatus
_XrFontTransform(XrFont *font,
		 double a, double b,
		 double c, double d);

XrStatus
_XrFontResolveXftFont(XrFont *font, XrGState *gstate, XftFont **xft_font);

/* xrpath.c */
void
_XrPathInit(XrPath *path);

XrStatus
_XrPathInitCopy(XrPath *path, XrPath *other);

void
_XrPathDeinit(XrPath *path);

XrStatus
_XrPathMoveTo(XrPath *path, double x, double y);

XrStatus
_XrPathLineTo(XrPath *path, double x, double y);

XrStatus
_XrPathCurveTo(XrPath *path,
	       double x1, double y1,
	       double x2, double y2,
	       double x3, double y3);

XrStatus
_XrPathClosePath(XrPath *path);

XrStatus
_XrPathInterpret(XrPath *path, XrPathDirection dir, XrPathCallbacks *cb, void *closure);

/* xrsurface.c */
void
_XrSurfaceInit(XrSurface *surface, Display *dpy);

void
_XrSurfaceReference(XrSurface *surface);

void
_XrSurfaceDereference(XrSurface *surface);

void
_XrSurfaceDeinit(XrSurface *surface);

void
_XrSurfaceSetSolidColor(XrSurface *surface, XrColor *color, XcFormat *xc_format);

void
_XrSurfaceSetDrawable(XrSurface *surface, Drawable drawable);

void
_XrSurfaceSetVisual(XrSurface *surface, Visual *visual);

void
_XrSurfaceSetFormat(XrSurface *surface, XrFormat format);

Picture
_XrSurfaceGetPicture(XrSurface *surface);

/* xrpen.c */
XrStatus
_XrPenInit(XrPen *pen, double radius, XrGState *gstate);

XrStatus
_XrPenInitEmpty(XrPen *pen);

XrStatus
_XrPenInitCopy(XrPen *pen, XrPen *other);

void
_XrPenDeinit(XrPen *pen);

XrStatus
_XrPenAddPoints(XrPen *pen, XrPenFlaggedPoint *pt, int num_pts);

XrStatus
_XrPenAddPointsForSlopes(XrPen *pen, XPointFixed *a, XPointFixed *b, XPointFixed *c, XPointFixed *d);

XrStatus
_XrPenStrokeSpline(XrPen *pen, XrSpline *spline, double tolerance, XrTraps *traps);

/* xrpolygon.c */
void
_XrPolygonInit(XrPolygon *polygon);

void
_XrPolygonDeinit(XrPolygon *polygon);

XrStatus
_XrPolygonAddEdge(XrPolygon *polygon, XPointFixed *p1, XPointFixed *p2);

XrStatus
_XrPolygonAddPoint(XrPolygon *polygon, XPointFixed *pt);

XrStatus
_XrPolygonClose(XrPolygon *polygon);

/* xrspline.c */
XrIntStatus
_XrSplineInit(XrSpline *spline, XPointFixed *a,  XPointFixed *b,  XPointFixed *c,  XPointFixed *d);

XrStatus
_XrSplineDecompose(XrSpline *spline, double tolerance);

void
_XrSplineDeinit(XrSpline *spline);

/* xrstroker.c */
void
_XrStrokerInit(XrStroker *stroker, XrGState *gstate, XrTraps *traps);

void
_XrStrokerDeinit(XrStroker *stroker);

XrStatus
_XrStrokerAddEdge(void *closure, XPointFixed *p1, XPointFixed *p2);

XrStatus
_XrStrokerAddEdgeDashed(void *closure, XPointFixed *p1, XPointFixed *p2);

XrStatus
_XrStrokerAddSpline (void *closure, XPointFixed *a, XPointFixed *b, XPointFixed *c, XPointFixed *d);

XrStatus
_XrStrokerDoneSubPath (void *closure, XrSubPathDone done);

XrStatus
_XrStrokerDonePath (void *closure);

/* xrfiller.c */
void
_XrFillerInit(XrFiller *filler, XrGState *gstate, XrTraps *traps);

void
_XrFillerDeinit(XrFiller *filler);

XrStatus
_XrFillerAddEdge(void *closure, XPointFixed *p1, XPointFixed *p2);

XrStatus
_XrFillerAddSpline (void *closure, XPointFixed *a, XPointFixed *b, XPointFixed *c, XPointFixed *d);

XrStatus
_XrFillerDoneSubPath (void *closure, XrSubPathDone done);

XrStatus
_XrFillerDonePath (void *closure);
    
/* xrtransform.c */
void
_XrTransformInit(XrTransform *transform);

void
_XrTransformDeinit(XrTransform *transform);

void
_XrTransformInitMatrix(XrTransform *transform,
		       double a, double b,
		       double c, double d,
		       double tx, double ty);

void
_XrTransformInitTranslate(XrTransform *transform,
			  double tx, double ty);

void
_XrTransformInitScale(XrTransform *transform,
		      double sx, double sy);

void
_XrTransformInitRotate(XrTransform *transform,
		       double angle);

void
_XrTransformMultiplyIntoLeft(XrTransform *t1, const XrTransform *t2);

void
_XrTransformMultiplyIntoRight(const XrTransform *t1, XrTransform *t2);

void
_XrTransformMultiply(const XrTransform *t1, const XrTransform *t2, XrTransform *new);

void
_XrTransformDistance(XrTransform *transform, double *dx, double *dy);

void
_XrTransformPoint(XrTransform *transform, double *x, double *y);

void
_XrTransformComputeInverse(XrTransform *transform);

void
_XrTransformEigenValues(XrTransform *transform, double *lambda1, double *lambda2);

/* xrtraps.c */
void
_XrTrapsInit(XrTraps *traps);

void
_XrTrapsDeinit(XrTraps *traps);

XrStatus
_XrTrapsTessellateRectangle (XrTraps *traps, XPointFixed q[4]);

XrStatus
_XrTrapsTessellatePolygon (XrTraps *traps, XrPolygon *poly, XrFillRule fill_rule);

/* xrmisc.c */

void
_ComputeSlope(XPointFixed *a, XPointFixed *b, XrSlopeFixed *slope);

#endif
