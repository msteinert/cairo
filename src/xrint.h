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
#include "Xr.h"

#ifndef __GCC__
#define __attribute__(x)
#endif

typedef enum _XrError {
    XrErrorSuccess = 0,
    XrErrorNoMemory
} XrError;

typedef enum _XrPathOp {
    XrPathOpMoveTo = 0,
    XrPathOpLineTo = 1,
    XrPathOpCurveTo = 2,
    XrPathOpRelMoveTo = 3,
    XrPathOpRelLineTo = 4,
    XrPathOpRelCurveTo = 5,
    XrPathOpClosePath = 6
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
    XrError (*AddEdge)(void *closure, XPointFixed *p1, XPointFixed *p2);
    XrError (*AddSpline)(void *closure, XPointFixed *a, XPointFixed *b, XPointFixed *c, XPointFixed *d);
    XrError (*DoneSubPath) (void *closure, XrSubPathDone done);
    XrError (*DonePath) (void *closure);
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
    /* Externally initialized */
    XLineFixed edge;
    Bool clockWise;

    /* Internal use by XrTrapsTessellateEdges */
    XFixed current_x;
    XFixed next_x;
    struct _XrEdge *next, *prev;
} XrEdge;

typedef struct _XrPolygon {
    int num_edges;
    int edges_size;
    XrEdge *edges;
} XrPolygon;

typedef struct _XrSpline {
    XPointFixed a, b, c, d;
    XPointFixed *pt;
    int num_pts;
} XrSpline;

typedef enum _XrPenVertexStart {
    XrPenVertexNone,
    XrPenVertexForward,
    XrPenVertexRevers
} XrPenVertexStart;

typedef struct _XrPenVertex {
    XPointFixed pt;
    double theta;
    XPointFixed slope_ccw;
    XPointFixed slope_cw;
    XrPenVertexStart start;
} XrPenVertex;

typedef struct _XrPen {
    int num_vertices;
    XrPenVertex *vertex;
} XrPen;

typedef struct _XrSurface {
    Display *dpy;

    Drawable drawable;

    unsigned int depth;

    unsigned long sa_mask;
    XcSurfaceAttributes sa;
    XcFormat *xcformat;

    XcSurface *xcsurface;
    XcSurface *alpha;
} XrSurface;

typedef struct _XrColor {
    double red;
    double green;
    double blue;
    double alpha;

    XcColor xccolor;
} XrColor;

typedef struct _XrTransform {
    double m[3][2];
} XrTransform;

typedef struct _XrTraps {
    int num_xtraps;
    int xtraps_size;
    XTrapezoid *xtraps;
} XrTraps;

#define XR_GSTATE_OPERATOR_DEFAULT	XrOperatorOver
#define XR_GSTATE_TOLERANCE_DEFAULT	0.1
#define XR_GSTATE_WINDING_DEFAULT	1
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

    /* fill style */
    int winding;

    XrOperator operator;
    
    XcFormat *solidFormat;
    XcFormat *alphaFormat;

    XrColor color;
    XrSurface src;
    XrSurface surface;

    XrTransform ctm;
    XrTransform ctm_inverse;

    XrPath path;

    XrPen pen_regular;

    struct _XrGState *next;
} XrGState;

struct _XrState {
    Display *dpy;
    XrGState *stack;
    XrError error;
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

#define CURRENT_GSTATE(xrs) (xrs->stack)

XrState *
XrStateCreate(Display *dpy);

XrError
XrStateInit(XrState *state, Display *dpy);

void
XrStateDeinit(XrState *xrs);

void
XrStateDestroy(XrState *state);

XrError
XrStatePush(XrState *xrs);

void
XrStatePop(XrState *xrs);

/* xrgstate.c */
XrGState *
XrGStateCreate(Display *dpy);

void
XrGStateInit(XrGState *gstate, Display *dpy);

XrError
XrGStateInitCopy(XrGState *gstate, XrGState *other);

void
XrGStateDeinit(XrGState *gstate);

void
XrGStateDestroy(XrGState *gstate);

XrGState *
XrGStateClone(XrGState *gstate);

void
XrGStateSetDrawable(XrGState *gstate, Drawable drawable);

void
XrGStateSetVisual(XrGState *gstate, Visual *visual);

void
XrGStateSetFormat(XrGState *gstate, XrFormat format);

void
XrGStateSetOperator(XrGState *gstate, XrOperator operator);

void
XrGStateSetRGBColor(XrGState *gstate, double red, double green, double blue);

void
XrGStateSetAlpha(XrGState *gstate, double alpha);

void
XrGStateSetLineWidth(XrGState *gstate, double width);

void
XrGStateSetLineCap(XrGState *gstate, XrLineCap line_cap);

void
XrGStateSetLineJoin(XrGState *gstate, XrLineJoin line_join);

XrError
XrGStateSetDash(XrGState *gstate, double *dashes, int ndash, double offset);

void
XrGStateSetMiterLimit(XrGState *gstate, double limit);

void
XrGStateTranslate(XrGState *gstate, double tx, double ty);

void
XrGStateScale(XrGState *gstate, double sx, double sy);

void
XrGStateRotate(XrGState *gstate, double angle);

void
XrGStateNewPath(XrGState *gstate);

XrError
XrGStateAddPathOp(XrGState *gstate, XrPathOp op, XPointDouble *pt, int num_pts);

XrError
XrGStateAddUnaryPathOp(XrGState *gstate, XrPathOp op, double x, double y);

XrError
XrGStateClosePath(XrGState *gstate);

XrError
XrGStateStroke(XrGState *gstate);

XrError
XrGStateFill(XrGState *fill);

/* xrcolor.c */
void
XrColorInit(XrColor *color);

void
XrColorDeinit(XrColor *color);

void
XrColorSetRGB(XrColor *color, double red, double green, double blue);

void
XrColorSetAlpha(XrColor *color, double alpha);

/* xrpath.c */
void
XrPathInit(XrPath *path);

XrError
XrPathInitCopy(XrPath *path, XrPath *other);

void
XrPathDeinit(XrPath *path);

XrError
XrPathAdd(XrPath *path, XrPathOp op, XPointFixed *pts, int num_pts);

XrError
XrPathInterpret(XrPath *path, XrPathDirection dir, XrPathCallbacks *cb, void *closure);

/* xrsurface.c */
void
XrSurfaceInit(XrSurface *surface, Display *dpy);

void
XrSurfaceDeinit(XrSurface *surface);

void
XrSurfaceSetSolidColor(XrSurface *surface, XrColor *color, XcFormat *xcformat);

void
XrSurfaceSetDrawable(XrSurface *surface, Drawable drawable);

void
XrSurfaceSetVisual(XrSurface *surface, Visual *visual);

void
XrSurfaceSetFormat(XrSurface *surface, XrFormat format);

/* xrpen.c */
XrError
XrPenInit(XrPen *pen, double radius);

XrError
XrPenInitCopy(XrPen *pen, XrPen *other);

void
XrPenDeinit(XrPen *pen);

void
XrPenAddPointsForSlopes(XrPen *pen, XPointFixed *a, XPointFixed *b, XPointFixed *c, XPointFixed *d);

XrError
XrPenStrokePoints(XrPen *pen, XPointFixed *pt, int num_pts, XrPolygon *polygon);

/* xrpolygon.c */
void
XrPolygonInit(XrPolygon *polygon);

void
XrPolygonDeinit(XrPolygon *polygon);

XrError
XrPolygonAddEdge(XrPolygon *polygon, XPointFixed *p1, XPointFixed *p2);

/* xrspline.c */
void
XrSplineInit(XrSpline *spline, XPointFixed *a,  XPointFixed *b,  XPointFixed *c,  XPointFixed *d);

XrError
XrSplineDecompose(XrSpline *spline, double tolerance);

void
XrSplineDeinit(XrSpline *spline);

/* xrstroker.c */
void
XrStrokerInit(XrStroker *stroker, XrGState *gstate, XrTraps *traps);

void
XrStrokerDeinit(XrStroker *stroker);

XrError
XrStrokerAddEdge(void *closure, XPointFixed *p1, XPointFixed *p2);

XrError
XrStrokerAddEdgeDashed(void *closure, XPointFixed *p1, XPointFixed *p2);

XrError
XrStrokerAddSpline (void *closure, XPointFixed *a, XPointFixed *b, XPointFixed *c, XPointFixed *d);

XrError
XrStrokerDoneSubPath (void *closure, XrSubPathDone done);

XrError
XrStrokerDonePath (void *closure);

/* xrfiller.c */
void
XrFillerInit(XrFiller *filler, XrGState *gstate, XrTraps *traps);

void
XrFillerDeinit(XrFiller *filler);

XrError
XrFillerAddEdge(void *closure, XPointFixed *p1, XPointFixed *p2);

XrError
XrFillerAddSpline (void *closure, XPointFixed *a, XPointFixed *b, XPointFixed *c, XPointFixed *d);

XrError
XrFillerDoneSubPath (void *closure, XrSubPathDone done);

XrError
XrFillerDonePath (void *closure);
    
/* xrtransform.c */
void
XrTransformInit(XrTransform *transform);

void
XrTransformDeinit(XrTransform *transform);

void
XrTransformInitMatrix(XrTransform *transform,
		      double a, double b,
		      double c, double d,
		      double tx, double ty);

void
XrTransformInitTranslate(XrTransform *transform,
			 double tx, double ty);

void
XrTransformInitScale(XrTransform *transform,
		     double sx, double sy);

void
XrTransformInitRotate(XrTransform *transform,
		      double angle);

void
XrTransformMultiplyIntoLeft(XrTransform *t1, const XrTransform *t2);

void
XrTransformMultiplyIntoRight(const XrTransform *t1, XrTransform *t2);

void
XrTransformMultiply(const XrTransform *t1, const XrTransform *t2, XrTransform *new);

void
XrTransformPointScaleOnly(XrTransform *transform, XPointDouble *pt);

void
XrTransformPointWithoutTranslate(XrTransform *transform, XPointDouble *pt);

void
XrTransformPoint(XrTransform *transform, XPointDouble *pt);

/* xrtraps.c */
void
XrTrapsInit(XrTraps *traps);

void
XrTrapsDeinit(XrTraps *traps);

XrError
XrTrapsTessellateRectangle (XrTraps *traps, XPointFixed q[4]);

XrError
XrTrapsTessellatePolygon (XrTraps *traps, XrPolygon *poly, int winding);

#endif

