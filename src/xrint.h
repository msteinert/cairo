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

/* XXX: This can go away once incremental spline tessellation is working */
typedef enum _XrPenStrokeDirection {
    XrPenStrokeDirectionForward,
    XrPenStrokeDirectionReverse
} XrPenStrokeDirection;

typedef struct _XrPenVertex {
    XPointFixed pt;

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

struct _XrSurface {
    Display *dpy;
    char *image_data;

    XcSurface *xc_surface;

    unsigned int ref_count;
};

typedef struct _XrColor {
    double red;
    double green;
    double blue;
    double alpha;

    XcColor xc_color;
} XrColor;

struct _XrMatrix {
    double m[3][2];
};

typedef struct _XrTraps {
    int num_xtraps;
    int xtraps_size;
    XTrapezoid *xtraps;
} XrTraps;

#define XR_FONT_KEY_DEFAULT		"serif"

typedef struct _XrFont {
    unsigned char *key;

    double scale;
    XrMatrix matrix;

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

/* Need a name distinct from the XrClip function */
typedef struct _XrClipRec {
    int x;
    int y;
    int width;
    int height;
    XrSurface *surface;
} XrClipRec;

typedef struct _XrGState {
    XrOperator operator;
    
    double tolerance;

    /* stroke style */
    double line_width;
    XrLineCap line_cap;
    XrLineJoin line_join;
    double miter_limit;

    XrFillRule fill_rule;

    double *dash;
    int num_dashes;
    double dash_offset;

    XrFont font;

    XrSurface *surface;
    XrSurface *solid;
    XrSurface *pattern;
    XPointDouble pattern_offset;

    XrClipRec clip;

    double alpha;
    XrColor color;

    XrMatrix ctm;
    XrMatrix ctm_inverse;

    XrPath path;

    XPointDouble last_move_pt;
    XPointDouble current_pt;
    int has_current_pt;

    XrPen pen_regular;

    struct _XrGState *next;
} XrGState;

struct _XrState {
    XrGState *stack;
    XrStatus status;
};

typedef struct _XrStrokeFace {
    XPointFixed ccw;
    XPointFixed pt;
    XPointFixed cw;
    XrSlopeFixed dev_vector;
    XPointDouble usr_vector;
} XrStrokeFace;

/* xrstate.c */

XrState *
_XrStateCreate(void);

XrStatus
_XrStateInit(XrState *state);

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
_XrGStateCreate(void);

void
_XrGStateInit(XrGState *gstate);

XrStatus
_XrGStateInitCopy(XrGState *gstate, XrGState *other);

void
_XrGStateDeinit(XrGState *gstate);

void
_XrGStateDestroy(XrGState *gstate);

XrGState *
_XrGStateClone(XrGState *gstate);

XrStatus
_XrGStateBeginGroup(XrGState *gstate);

XrStatus
_XrGStateEndGroup(XrGState *gstate);

XrStatus
_XrGStateSetDrawable(XrGState *gstate, Drawable drawable);

XrStatus
_XrGStateSetVisual(XrGState *gstate, Visual *visual);

XrStatus
_XrGStateSetFormat(XrGState *gstate, XrFormat format);

XrStatus
_XrGStateSetTargetSurface (XrGState *gstate, XrSurface *surface);

XrSurface *
_XrGStateGetTargetSurface (XrGState *gstate);

XrStatus
_XrGStateSetPattern (XrGState *gstate, XrSurface *pattern);

XrStatus
_XrGStateSetOperator(XrGState *gstate, XrOperator operator);

XrOperator
_XrGStateGetOperator(XrGState *gstate);

XrStatus
_XrGStateSetRGBColor(XrGState *gstate, double red, double green, double blue);

XrStatus
_XrGStateSetTolerance(XrGState *gstate, double tolerance);

double
_XrGStateGetTolerance(XrGState *gstate);

XrStatus
_XrGStateSetAlpha(XrGState *gstate, double alpha);

XrStatus
_XrGStateSetFillRule(XrGState *gstate, XrFillRule fill_rule);

XrStatus
_XrGStateSetLineWidth(XrGState *gstate, double width);

double
_XrGStateGetLineWidth(XrGState *gstate);

XrStatus
_XrGStateSetLineCap(XrGState *gstate, XrLineCap line_cap);

XrLineCap
_XrGStateGetLineCap(XrGState *gstate);

XrStatus
_XrGStateSetLineJoin(XrGState *gstate, XrLineJoin line_join);

XrLineJoin
_XrGStateGetLineJoin(XrGState *gstate);

XrStatus
_XrGStateSetDash(XrGState *gstate, double *dash, int num_dashes, double offset);

XrStatus
_XrGStateSetMiterLimit(XrGState *gstate, double limit);

double
_XrGStateGetMiterLimit(XrGState *gstate);

XrStatus
_XrGStateTranslate(XrGState *gstate, double tx, double ty);

XrStatus
_XrGStateScale(XrGState *gstate, double sx, double sy);

XrStatus
_XrGStateRotate(XrGState *gstate, double angle);

XrStatus
_XrGStateConcatMatrix(XrGState *gstate,
		      XrMatrix *matrix);

XrStatus
_XrGStateSetMatrix(XrGState *gstate,
		   XrMatrix *matrix);

XrStatus
_XrGStateIdentityMatrix(XrGState *xrs);

XrStatus
_XrGStateTransformPoint (XrGState *gstate, double *x, double *y);

XrStatus
_XrGStateTransformDistance (XrGState *gstate, double *dx, double *dy);

XrStatus
_XrGStateInverseTransformPoint (XrGState *gstate, double *x, double *y);

XrStatus
_XrGStateInverseTransformDistance (XrGState *gstate, double *dx, double *dy);

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
_XrGStateGetCurrentPoint(XrGState *gstate, double *x, double *y);

XrStatus
_XrGStateStroke(XrGState *gstate);

XrStatus
_XrGStateFill(XrGState *gstate);

XrStatus
_XrGStateClip(XrGState *gstate);

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

XrStatus
_XrGStateShowSurface(XrGState	*gstate,
		     XrSurface	*surface,
		     int	width,
		     int	height);

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
_XrFontInit(XrFont *font);

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
_XrPathInterpret(XrPath *path, XrPathDirection dir, const XrPathCallbacks *cb, void *closure);

XrStatus
_XrPathBounds(XrPath *path, double *x1, double *y1, double *x2, double *y2);

/* xrpathfill.c */

XrStatus
_XrPathFillToTraps(XrPath *path, XrGState *gstate, XrTraps *traps);

/* xrpathstroke.c */

XrStatus
_XrPathStrokeToTraps (XrPath *path, XrGState *gstate, XrTraps *traps);

/* xrsurface.c */

void
_XrSurfaceReference(XrSurface *surface);

XcSurface *
_XrSurfaceGetXcSurface(XrSurface *surface);

/* XXX: This function is going away, right? */
Picture
_XrSurfaceGetPicture(XrSurface *surface);

void
_XrSurfaceFillRectangle (XrSurface	*surface,
			 XrOperator	operator,
			 XrColor	*color,
			 int		x,
			 int		y,
			 int		width,
			 int		height);

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
_XrPenAddPoints(XrPen *pen, XPointFixed *pt, int num_pts);

XrStatus
_XrPenAddPointsForSlopes(XrPen *pen, XPointFixed *a, XPointFixed *b, XPointFixed *c, XPointFixed *d);

XrStatus
_XrPenFindActiveCWVertexIndex(XrPen *pen, XrSlopeFixed *slope, int *active);

XrStatus
_XrPenFindActiveCCWVertexIndex(XrPen *pen, XrSlopeFixed *slope, int *active);

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

/* xrmatrix.c */
void
_XrMatrixInit(XrMatrix *matrix);

void
_XrMatrixFini(XrMatrix *matrix);

XrStatus
_XrMatrixSetTranslate(XrMatrix *matrix,
		      double tx, double ty);

XrStatus
_XrMatrixSetScale(XrMatrix *matrix,
		  double sx, double sy);

XrStatus
_XrMatrixSetRotate(XrMatrix *matrix,
		   double angle);

XrStatus
_XrMatrixMultiplyIntoLeft(XrMatrix *t1, const XrMatrix *t2);

XrStatus
_XrMatrixMultiplyIntoRight(const XrMatrix *t1, XrMatrix *t2);


XrStatus
_XrMatrixTransformBoundingBox(XrMatrix *matrix,
			      double *x, double *y,
			      double *width, double *height);

XrStatus
_XrMatrixComputeDeterminant(XrMatrix *matrix, double *det);

XrStatus
_XrMatrixComputeEigenValues(XrMatrix *matrix, double *lambda1, double *lambda2);

/* xrtraps.c */
void
_XrTrapsInit(XrTraps *traps);

void
_XrTrapsDeinit(XrTraps *traps);

XrStatus
_XrTrapsTessellateTriangle (XrTraps *traps, XPointFixed t[3]);

XrStatus
_XrTrapsTessellateRectangle (XrTraps *traps, XPointFixed q[4]);

XrStatus
_XrTrapsTessellatePolygon (XrTraps *traps, XrPolygon *poly, XrFillRule fill_rule);

/* xrmisc.c */

void
_ComputeSlope(XPointFixed *a, XPointFixed *b, XrSlopeFixed *slope);

#endif
