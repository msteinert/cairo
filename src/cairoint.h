/*
 * Copyright © 2002 USC, Information Sciences Institute
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of the
 * University of Southern California not be used in advertising or
 * publicity pertaining to distribution of the software without
 * specific, written prior permission. The University of Southern
 * California makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 *
 * THE UNIVERSITY OF SOUTHERN CALIFORNIA DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL THE UNIVERSITY OF
 * SOUTHERN CALIFORNIA BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author: Carl D. Worth <cworth@isi.edu>
 */

/*
 * These definitions are solely for use by the implementation of Cairo
 * and constitute no kind of standard.  If you need any of these
 * functions, please drop me a note.  Either the library needs new
 * functionality, or there's a way to do what you need using the
 * existing published interfaces. cworth@isi.edu
 */

#ifndef _CAIROINT_H_
#define _CAIROINT_H_

#include <assert.h>

#include <math.h>
#include <X11/Xlibint.h>
#include <X11/Xft/Xft.h>

#include "cairo.h"

#include <slim_internal.h>

/* These macros allow us to deprecate a function by providing an alias
   for the old function name to the new function name. With this
   macro, code using the deprecated function will still compile and
   link, but will provide a useful warning message giving the old and
   new function names to the user at compilation time, (as long as the
   compiler reports calls to undeclared functions).

   If the macro is not supported by the compiler, the program will not
   link, and the user will still se a useful error message.  */
#if __GNUC__ >= 2 && defined(__ELF__)
# define DEPRECATE(old, new)	ADD_ALIAS(old, new); ADD_ALIAS(old##_DEPRECATED_BY_##new , new)
# define ADD_ALIAS(old, new)			\
	extern __typeof (new) old		\
	__asm__ ("" #old)			\
	__attribute__((__alias__("" #new)))
#else
# define DEPRECATE(old, new)
#endif

#ifndef __GNUC__
#define __attribute__(x)
#endif

#ifdef WIN32
typedef __int64		cairo_fixed_32_32_t;
#else
#  if defined(__alpha__) || defined(__alpha) || \
      defined(ia64) || defined(__ia64__) || \
      defined(__sparc64__) || \
      defined(__s390x__) || \
      defined(x86_64) || defined (__x86_64__)
typedef long		cairo_fixed_32_32_t;
# else
#  if defined(__GNUC__) && \
    ((__GNUC__ > 2) || \
     ((__GNUC__ == 2) && defined(__GNUC_MINOR__) && (__GNUC_MINOR__ > 7)))
__extension__
#  endif
typedef long long int	cairo_fixed_32_32_t;
# endif
#endif

typedef cairo_fixed_32_32_t cairo_fixed_48_16_t;
typedef int32_t cairo_fixed_16_16_t;

/* The common 16.16 format gets a shorter name */
typedef cairo_fixed_16_16_t cairo_fixed_t;

#define cairo_double_to_fixed(f)    ((cairo_fixed_t) ((f) * 65536))
#define cairo_fixed_to_double(d)    (((double) (d)) / 65536)

typedef struct cairo_point {
    cairo_fixed_t x;
    cairo_fixed_t y;
} cairo_point_t;

typedef struct cairo_slope
{
    cairo_fixed_t dx;
    cairo_fixed_t dy;
} cairo_slope_t;

typedef struct cairo_point_double {
    double x;
    double y;
} cairo_point_double_t;

typedef struct cairo_line {
    cairo_point_t p1;
    cairo_point_t p2;
} cairo_line_t;

typedef struct cairo_trapezoid {
    cairo_fixed_t top, bottom;
    cairo_line_t left, right;
} cairo_trapezoid_t;

typedef struct cairo_rectangle_int {
    short x, y;
    unsigned short width, height;
} cairo_rectangle_t;

/* Sure wish C had a real enum type so that this would be distinct
   from cairo_status_t. Oh well, without that, I'll use this bogus 1000
   offset */
typedef enum cairo_int_status {
    cairo_int_status_degenerate = 1000
} cairo_int_status_t;

typedef enum cairo_path_op {
    cairo_path_op_move_to = 0,
    cairo_path_op_line_to = 1,
    cairo_path_op_curve_to = 2,
    cairo_path_op_close_path = 3
} __attribute__ ((packed)) cairo_path_op_t; /* Don't want 32 bits if we can avoid it. */

typedef enum cairo_path_direction {
    cairo_path_direction_forward,
    cairo_path_direction_reverse
} cairo_path_direction_t;

typedef enum cairo_sub_path_done {
    cairo_sub_path_done_cap,
    cairo_sub_path_done_join
} cairo_sub_path_done_t;

typedef struct cairo_path_callbacks {
    cairo_status_t (*add_edge) (void *closure, cairo_point_t *p1, cairo_point_t *p2);
    cairo_status_t (*add_spline) (void *closure, cairo_point_t *a, cairo_point_t *b, cairo_point_t *c, cairo_point_t *d);
    cairo_status_t (*done_sub_path) (void *closure, cairo_sub_path_done_t done);
    cairo_status_t (*done_path) (void *closure);
} cairo_path_callbacks_t;

#define CAIRO_PATH_BUF_SZ 64

typedef struct cairo_path_op_buf {
    int num_ops;
    cairo_path_op_t op[CAIRO_PATH_BUF_SZ];

    struct cairo_path_op_buf *next, *prev;
} cairo_path_op_buf_t;

typedef struct cairo_path_arg_buf {
    int num_pts;
    cairo_point_t pt[CAIRO_PATH_BUF_SZ];

    struct cairo_path_arg_buf *next, *prev;
} cairo_path_arg_buf_t;

typedef struct cairo_path {
    cairo_path_op_buf_t *op_head;
    cairo_path_op_buf_t *op_tail;

    cairo_path_arg_buf_t *arg_head;
    cairo_path_arg_buf_t *arg_tail;
} cairo_path_t;

typedef struct cairo_edge {
    cairo_line_t edge;
    int clockWise;

    cairo_fixed_16_16_t current_x;
} cairo_edge_t;

typedef struct cairo_polygon {
    int num_edges;
    int edges_size;
    cairo_edge_t *edges;

    cairo_point_t first_pt;
    int first_pt_defined;
    cairo_point_t last_pt;
    int last_pt_defined;

    int closed;
} cairo_polygon_t;

typedef struct cairo_spline {
    cairo_point_t a, b, c, d;

    cairo_slope_t initial_slope;
    cairo_slope_t final_slope;

    int num_pts;
    int pts_size;
    cairo_point_t *pts;
} cairo_spline_t;

/* XXX: This can go away once incremental spline tessellation is working */
typedef enum cairo_pen_stroke_direction {
    cairo_pen_stroke_direction_forward,
    cairo_pen_stroke_direction_reverse
} cairo_pen_stroke_direction_t;

typedef struct _cairo_pen_vertex {
    cairo_point_t pt;

    cairo_slope_t slope_ccw;
    cairo_slope_t slope_cw;
} cairo_pen_vertex_t;

typedef struct cairo_pen {
    double radius;
    double tolerance;

    int num_vertices;
    cairo_pen_vertex_t *vertex;
} cairo_pen_t;

typedef enum cairo_surface_type {
    CAIRO_SURFACE_TYPE_DRAWABLE,
    CAIRO_SURFACE_TYPE_ICIMAGE
} cairo_surface_type_t;

struct cairo_surface {
    char *image_data;

    double ppm;
    unsigned int ref_count;

    cairo_surface_type_t type;
    XTransform xtransform;

    /* For TYPE_DRAWABLE */
    Display *dpy;
    GC gc;
    Drawable drawable;
    Visual *visual;

    int render_major;
    int render_minor;

    Picture picture;

    /* For TYPE_ICIMAGE */
    IcImage *icimage;
};

/* XXX: Right now, the cairo_color structure puts unpremultiplied
   color in the doubles and premultiplied color in the shorts. Yes,
   this is crazy insane, (but at least we don't export this
   madness). I'm still working on a cleaner API, but in the meantime,
   at least this does prevent precision loss in color when changing
   alpha. */
typedef struct cairo_color {
    double red;
    double green;
    double blue;
    double alpha;

    unsigned short red_short;
    unsigned short green_short;
    unsigned short blue_short;
    unsigned short alpha_short;
} cairo_color_t;

struct cairo_matrix {
    double m[3][2];
};

typedef struct cairo_traps {
    cairo_trapezoid_t *traps;
    int num_traps;
    int traps_size;
} cairo_traps_t;

#define CAIRO_FONT_KEY_DEFAULT		"serif"

typedef struct cairo_font {
    unsigned char *key;

    double scale;
    cairo_matrix_t matrix;

    Display *dpy;
    XftFont *xft_font;
} cairo_font_t;

#define CAIRO_GSTATE_OPERATOR_DEFAULT	CAIRO_OPERATOR_OVER
#define CAIRO_GSTATE_TOLERANCE_DEFAULT	0.1
#define CAIRO_GSTATE_FILL_RULE_DEFAULT	CAIRO_FILL_RULE_WINDING
#define CAIRO_GSTATE_LINE_WIDTH_DEFAULT	2.0
#define CAIRO_GSTATE_LINE_CAP_DEFAULT	CAIRO_LINE_CAP_BUTT
#define CAIRO_GSTATE_LINE_JOIN_DEFAULT	CAIRO_LINE_JOIN_MITER
#define CAIRO_GSTATE_MITER_LIMIT_DEFAULT	10.0

/* Need a name distinct from the cairo_clip function */
typedef struct cairo_clip_rec {
    int x;
    int y;
    int width;
    int height;
    cairo_surface_t *surface;
} cairo_clip_rec_t;

typedef struct cairo_gstate {
    cairo_operator_t operator;
    
    double tolerance;

    /* stroke style */
    double line_width;
    cairo_line_cap_t line_cap;
    cairo_line_join_t line_join;
    double miter_limit;

    cairo_fill_rule_t fill_rule;

    double *dash;
    int num_dashes;
    double dash_offset;

    cairo_font_t font;

    cairo_surface_t *surface;

    cairo_surface_t *source;
    cairo_point_double_t source_offset;
    int source_is_solid;

    cairo_clip_rec_t clip;

    double alpha;
    cairo_color_t color;

    double ppm;
    cairo_matrix_t ctm;
    cairo_matrix_t ctm_inverse;

    cairo_path_t path;

    cairo_point_double_t last_move_pt;
    cairo_point_double_t current_pt;
    int has_current_pt;

    cairo_pen_t pen_regular;

    struct cairo_gstate *next;
} cairo_gstate_t;

struct cairo {
    cairo_gstate_t *gstate;
    cairo_status_t status;
};

typedef struct cairo_stroke_face {
    cairo_point_t ccw;
    cairo_point_t pt;
    cairo_point_t cw;
    cairo_slope_t dev_vector;
    cairo_point_double_t usr_vector;
} cairo_stroke_face_t;

/* cairo_gstate.c */
extern cairo_gstate_t * __internal_linkage
_cairo_gstate_create (void);

extern void __internal_linkage
_cairo_gstate_init (cairo_gstate_t *gstate);

extern cairo_status_t __internal_linkage
_cairo_gstate_init_copy (cairo_gstate_t *gstate, cairo_gstate_t *other);

extern void __internal_linkage
_cairo_gstate_fini (cairo_gstate_t *gstate);

extern void __internal_linkage
_cairo_gstate_destroy (cairo_gstate_t *gstate);

extern cairo_gstate_t * __internal_linkage
_cairo_gstate_clone (cairo_gstate_t *gstate);

extern cairo_status_t __internal_linkage
_cairo_gstate_begin_group (cairo_gstate_t *gstate);

extern cairo_status_t __internal_linkage
_cairo_gstate_end_group (cairo_gstate_t *gstate);

extern cairo_status_t __internal_linkage
_cairo_gstate_set_drawable (cairo_gstate_t *gstate, Drawable drawable);

extern cairo_status_t __internal_linkage
_cairo_gstate_set_visual (cairo_gstate_t *gstate, Visual *visual);

extern cairo_status_t __internal_linkage
_cairo_gstate_set_format (cairo_gstate_t *gstate, cairo_format_t format);

extern cairo_status_t __internal_linkage
_cairo_gstate_set_target_surface (cairo_gstate_t *gstate, cairo_surface_t *surface);

extern cairo_surface_t * __internal_linkage
_cairo_gstate_current_target_surface (cairo_gstate_t *gstate);

extern cairo_status_t __internal_linkage
_cairo_gstate_set_pattern (cairo_gstate_t *gstate, cairo_surface_t *pattern);

extern cairo_status_t __internal_linkage
_cairo_gstate_set_operator (cairo_gstate_t *gstate, cairo_operator_t operator);

extern cairo_operator_t __internal_linkage
_cairo_gstate_current_operator (cairo_gstate_t *gstate);

extern cairo_status_t __internal_linkage
_cairo_gstate_set_rgb_color (cairo_gstate_t *gstate, double red, double green, double blue);

extern cairo_status_t __internal_linkage
_cairo_gstate_current_rgb_color (cairo_gstate_t *gstate,
				 double *red,
				 double *green,
				 double *blue);

extern cairo_status_t __internal_linkage
_cairo_gstate_set_tolerance (cairo_gstate_t *gstate, double tolerance);

extern double __internal_linkage
_cairo_gstate_current_tolerance (cairo_gstate_t *gstate);

extern cairo_status_t __internal_linkage
_cairo_gstate_set_alpha (cairo_gstate_t *gstate, double alpha);

extern double __internal_linkage
_cairo_gstate_current_alpha (cairo_gstate_t *gstate);

extern cairo_status_t __internal_linkage
_cairo_gstate_set_fill_rule (cairo_gstate_t *gstate, cairo_fill_rule_t fill_rule);

extern cairo_fill_rule_t __internal_linkage
_cairo_gstate_current_fill_rule (cairo_gstate_t *gstate);

extern cairo_status_t __internal_linkage
_cairo_gstate_set_line_width (cairo_gstate_t *gstate, double width);

extern double __internal_linkage
_cairo_gstate_current_line_width (cairo_gstate_t *gstate);

extern cairo_status_t __internal_linkage
_cairo_gstate_set_line_cap (cairo_gstate_t *gstate, cairo_line_cap_t line_cap);

extern cairo_line_cap_t __internal_linkage
_cairo_gstate_current_line_cap (cairo_gstate_t *gstate);

extern cairo_status_t __internal_linkage
_cairo_gstate_set_line_join (cairo_gstate_t *gstate, cairo_line_join_t line_join);

extern cairo_line_join_t __internal_linkage
_cairo_gstate_current_line_join (cairo_gstate_t *gstate);

extern cairo_status_t __internal_linkage
_cairo_gstate_set_dash (cairo_gstate_t *gstate, double *dash, int num_dashes, double offset);

extern cairo_status_t __internal_linkage
_cairo_gstate_set_miter_limit (cairo_gstate_t *gstate, double limit);

extern double __internal_linkage
_cairo_gstate_current_miter_limit (cairo_gstate_t *gstate);

extern void __internal_linkage
_cairo_gstate_current_matrix (cairo_gstate_t *gstate, cairo_matrix_t *matrix);

extern cairo_status_t __internal_linkage
_cairo_gstate_translate (cairo_gstate_t *gstate, double tx, double ty);

extern cairo_status_t __internal_linkage
_cairo_gstate_scale (cairo_gstate_t *gstate, double sx, double sy);

extern cairo_status_t __internal_linkage
_cairo_gstate_rotate (cairo_gstate_t *gstate, double angle);

extern cairo_status_t __internal_linkage
_cairo_gstate_concat_matrix (cairo_gstate_t *gstate,
			     cairo_matrix_t *matrix);

extern cairo_status_t __internal_linkage
_cairo_gstate_set_matrix (cairo_gstate_t *gstate,
			  cairo_matrix_t *matrix);

extern cairo_status_t __internal_linkage
_cairo_gstate_default_matrix (cairo_gstate_t *gstate);

extern cairo_status_t __internal_linkage
_cairo_gstate_identity_matrix (cairo_gstate_t *gstate);

extern cairo_status_t __internal_linkage
_cairo_gstate_transform_point (cairo_gstate_t *gstate, double *x, double *y);

extern cairo_status_t __internal_linkage
_cairo_gstate_transform_distance (cairo_gstate_t *gstate, double *dx, double *dy);

extern cairo_status_t __internal_linkage
_cairo_gstate_inverse_transform_point (cairo_gstate_t *gstate, double *x, double *y);

extern cairo_status_t __internal_linkage
_cairo_gstate_inverse_transform_distance (cairo_gstate_t *gstate, double *dx, double *dy);

extern cairo_status_t __internal_linkage
_cairo_gstate_new_path (cairo_gstate_t *gstate);

extern cairo_status_t __internal_linkage
_cairo_gstate_move_to (cairo_gstate_t *gstate, double x, double y);

extern cairo_status_t __internal_linkage
_cairo_gstate_line_to (cairo_gstate_t *gstate, double x, double y);

extern cairo_status_t __internal_linkage
_cairo_gstate_curve_to (cairo_gstate_t *gstate,
			double x1, double y1,
			double x2, double y2,
			double x3, double y3);

extern cairo_status_t __internal_linkage
_cairo_gstate_rel_move_to (cairo_gstate_t *gstate, double dx, double dy);

extern cairo_status_t __internal_linkage
_cairo_gstate_rel_line_to (cairo_gstate_t *gstate, double dx, double dy);

extern cairo_status_t __internal_linkage
_cairo_gstate_rel_curve_to (cairo_gstate_t *gstate,
			    double dx1, double dy1,
			    double dx2, double dy2,
			    double dx3, double dy3);

/* XXX: NYI
extern cairo_status_t __internal_linkage
_cairo_gstate_stroke_path (cairo_gstate_t *gstate);
*/

extern cairo_status_t __internal_linkage
_cairo_gstate_close_path (cairo_gstate_t *gstate);

extern cairo_status_t __internal_linkage
_cairo_gstate_current_point (cairo_gstate_t *gstate, double *x, double *y);

extern cairo_status_t __internal_linkage
_cairo_gstate_stroke (cairo_gstate_t *gstate);

extern cairo_status_t __internal_linkage
_cairo_gstate_fill (cairo_gstate_t *gstate);

extern cairo_status_t __internal_linkage
_cairo_gstate_clip (cairo_gstate_t *gstate);

extern cairo_status_t __internal_linkage
_cairo_gstate_select_font (cairo_gstate_t *gstate, const char *key);

extern cairo_status_t __internal_linkage
_cairo_gstate_scale_font (cairo_gstate_t *gstate, double scale);

extern cairo_status_t __internal_linkage
_cairo_gstate_transform_font (cairo_gstate_t *gstate,
			      double a, double b,
			      double c, double d);

extern cairo_status_t __internal_linkage
_cairo_gstate_text_extents (cairo_gstate_t *gstate,
			    const unsigned char *utf8,
			    double *x, double *y,
			    double *width, double *height,
			    double *dx, double *dy);

extern cairo_status_t __internal_linkage
_cairo_gstate_show_text (cairo_gstate_t *gstate, const unsigned char *utf8);

extern cairo_status_t __internal_linkage
_cairo_gstate_show_surface (cairo_gstate_t	*gstate,
			    cairo_surface_t	*surface,
			    int			width,
			    int			height);

/* cairo_color.c */
extern void __internal_linkage
_cairo_color_init (cairo_color_t *color);

extern void __internal_linkage
_cairo_color_fini (cairo_color_t *color);

extern void __internal_linkage
_cairo_color_set_rgb (cairo_color_t *color, double red, double green, double blue);

extern void __internal_linkage
_cairo_color_get_rgb (cairo_color_t *color, double *red, double *green, double *blue);

extern void __internal_linkage
_cairo_color_set_alpha (cairo_color_t *color, double alpha);

/* cairo_font.c */
extern void __internal_linkage
_cairo_font_init (cairo_font_t *font);

extern cairo_status_t __internal_linkage
_cairo_font_init_copy (cairo_font_t *font, cairo_font_t *other);

extern void __internal_linkage
_cairo_font_fini (cairo_font_t *font);

extern cairo_status_t __internal_linkage
_cairo_font_select (cairo_font_t *font, const char *key);

extern cairo_status_t __internal_linkage
_cairo_font_scale (cairo_font_t *font, double scale);

extern cairo_status_t __internal_linkage
_cairo_font_transform (cairo_font_t *font,
		       double a, double b,
		       double c, double d);

extern cairo_status_t __internal_linkage
_cairo_font_resolve_xft_font (cairo_font_t *font, cairo_gstate_t *gstate, XftFont **xft_font);

/* cairo_path.c */
extern void __internal_linkage
_cairo_path_init (cairo_path_t *path);

extern cairo_status_t __internal_linkage
_cairo_path_init_copy (cairo_path_t *path, cairo_path_t *other);

extern void __internal_linkage
_cairo_path_fini (cairo_path_t *path);

extern cairo_status_t __internal_linkage
_cairo_path_move_to (cairo_path_t *path, double x, double y);

extern cairo_status_t __internal_linkage
_cairo_path_line_to (cairo_path_t *path, double x, double y);

extern cairo_status_t __internal_linkage
_cairo_path_curve_to (cairo_path_t *path,
		      double x1, double y1,
		      double x2, double y2,
		      double x3, double y3);

extern cairo_status_t __internal_linkage
_cairo_path_close_path (cairo_path_t *path);

extern cairo_status_t __internal_linkage
_cairo_path_interpret (cairo_path_t *path,
		       cairo_path_direction_t dir,
		       const cairo_path_callbacks_t *cb,
		       void *closure);

extern cairo_status_t __internal_linkage
_cairo_path_bounds (cairo_path_t *path, double *x1, double *y1, double *x2, double *y2);

/* cairo_path_fill.c */
extern cairo_status_t __internal_linkage
_cairo_path_fill_to_traps (cairo_path_t *path, cairo_gstate_t *gstate, cairo_traps_t *traps);

/* cairo_path_stroke.c */
extern cairo_status_t __internal_linkage
_cairo_path_stroke_to_traps (cairo_path_t *path, cairo_gstate_t *gstate, cairo_traps_t *traps);

/* cairo_surface.c */
extern void __internal_linkage
_cairo_surface_reference (cairo_surface_t *surface);

extern void __internal_linkage
_cairo_surface_fill_rectangle (cairo_surface_t	*surface,
			       cairo_operator_t	operator,
			       cairo_color_t	*color,
			       int		x,
			       int		y,
			       int		width,
			       int		height);

extern void __internal_linkage
_cairo_surface_composite (cairo_operator_t	operator,
			  cairo_surface_t	*src,
			  cairo_surface_t	*mask,
			  cairo_surface_t	*dst,
			  int			src_x,
			  int			src_y,
			  int			mask_x,
			  int			mask_y,
			  int			dst_x,
			  int			dst_y,
			  unsigned int		width,
			  unsigned int		height);

extern void __internal_linkage
_cairo_surface_fill_rectangles (cairo_surface_t		*surface,
				cairo_operator_t	operator,
				const cairo_color_t	*color,
				cairo_rectangle_t	*rects,
				int			num_rects);

extern void __internal_linkage
_cairo_surface_composite_trapezoids (cairo_operator_t		operator,
				     cairo_surface_t		*src,
				     cairo_surface_t		*dst,
				     int			xSrc,
				     int			ySrc,
				     const cairo_trapezoid_t	*traps,
				     int			ntraps);

extern void __internal_linkage
_cairo_surface_pull_image (cairo_surface_t *surface);

extern void __internal_linkage
_cairo_surface_push_image (cairo_surface_t *surface);

/* cairo_pen.c */
extern cairo_status_t __internal_linkage
_cairo_pen_init (cairo_pen_t *pen, double radius, cairo_gstate_t *gstate);

extern cairo_status_t __internal_linkage
_cairo_pen_init_empty (cairo_pen_t *pen);

extern cairo_status_t __internal_linkage
_cairo_pen_init_copy (cairo_pen_t *pen, cairo_pen_t *other);

extern void __internal_linkage
_cairo_pen_fini (cairo_pen_t *pen);

extern cairo_status_t __internal_linkage
_cairo_pen_add_points (cairo_pen_t *pen, cairo_point_t *pt, int num_pts);

extern cairo_status_t __internal_linkage
_cairo_pen_add_points_for_slopes (cairo_pen_t *pen,
				  cairo_point_t *a,
				  cairo_point_t *b,
				  cairo_point_t *c,
				  cairo_point_t *d);

extern cairo_status_t __internal_linkage
_cairo_pen_find_active_cw_vertex_index (cairo_pen_t *pen,
					cairo_slope_t *slope,
					int *active);

extern cairo_status_t __internal_linkage
_cairo_pen_find_active_ccw_vertex_index (cairo_pen_t *pen,
					 cairo_slope_t *slope,
					 int *active);

extern cairo_status_t __internal_linkage
_cairo_pen_stroke_spline (cairo_pen_t *pen,
			  cairo_spline_t *spline,
			  double tolerance,
			  cairo_traps_t *traps);

/* cairo_polygon.c */
extern void __internal_linkage
_cairo_polygon_init (cairo_polygon_t *polygon);

extern void __internal_linkage
_cairo_polygon_fini (cairo_polygon_t *polygon);

extern cairo_status_t __internal_linkage
_cairo_polygon_add_edge (cairo_polygon_t *polygon, cairo_point_t *p1, cairo_point_t *p2);

extern cairo_status_t __internal_linkage
_cairo_polygon_add_point (cairo_polygon_t *polygon, cairo_point_t *pt);

extern cairo_status_t __internal_linkage
_cairo_polygon_close (cairo_polygon_t *polygon);

/* cairo_spline.c */
extern cairo_int_status_t __internal_linkage
_cairo_spline_init (cairo_spline_t *spline,
		    cairo_point_t *a,
		    cairo_point_t *b,
		    cairo_point_t *c,
		    cairo_point_t *d);

extern cairo_status_t __internal_linkage
_cairo_spline_decompose (cairo_spline_t *spline, double tolerance);

extern void __internal_linkage
_cairo_spline_fini (cairo_spline_t *spline);

/* cairo_matrix.c */
extern void __internal_linkage
_cairo_matrix_init (cairo_matrix_t *matrix);

extern void __internal_linkage
_cairo_matrix_fini (cairo_matrix_t *matrix);

extern cairo_status_t __internal_linkage
_cairo_matrix_set_translate (cairo_matrix_t *matrix,
			     double tx, double ty);

extern cairo_status_t __internal_linkage
_cairo_matrix_set_scale (cairo_matrix_t *matrix,
			 double sx, double sy);

extern cairo_status_t __internal_linkage
_cairo_matrix_set_rotate (cairo_matrix_t *matrix,
			  double angle);

extern cairo_status_t __internal_linkage
_cairo_matrix_transform_bounding_box (cairo_matrix_t *matrix,
				      double *x, double *y,
				      double *width, double *height);

extern cairo_status_t __internal_linkage
_cairo_matrix_compute_determinant (cairo_matrix_t *matrix, double *det);

extern cairo_status_t __internal_linkage
_cairo_matrix_compute_eigen_values (cairo_matrix_t *matrix, double *lambda1, double *lambda2);

/* cairo_traps.c */
extern void __internal_linkage
_cairo_traps_init (cairo_traps_t *traps);

extern void __internal_linkage
_cairo_traps_fini (cairo_traps_t *traps);

extern cairo_status_t __internal_linkage
_cairo_traps_tessellate_triangle (cairo_traps_t *traps, cairo_point_t t[3]);

extern cairo_status_t __internal_linkage
_cairo_traps_tessellate_rectangle (cairo_traps_t *traps, cairo_point_t q[4]);

extern cairo_status_t __internal_linkage
_cairo_traps_tessellate_polygon (cairo_traps_t *traps,
				 cairo_polygon_t *poly,
				 cairo_fill_rule_t fill_rule);

/* cairo_slope.c */
extern void __internal_linkage
_cairo_slope_init (cairo_slope_t *slope, cairo_point_t *a, cairo_point_t *b);

extern int __internal_linkage
_cairo_slope_clockwise (cairo_slope_t *a, cairo_slope_t *b);

extern int __internal_linkage
_cairo_slope_counter_clockwise (cairo_slope_t *a, cairo_slope_t *b);

/* Avoid unnecessary PLT entries.  */

slim_hidden_proto(cairo_close_path)
slim_hidden_proto(cairo_matrix_copy)
slim_hidden_proto(cairo_matrix_invert)
slim_hidden_proto(cairo_matrix_multiply)
slim_hidden_proto(cairo_matrix_scale)
slim_hidden_proto(cairo_matrix_set_affine)
slim_hidden_proto(cairo_matrix_set_identity)
slim_hidden_proto(cairo_matrix_transform_distance)
slim_hidden_proto(cairo_matrix_transform_point)
slim_hidden_proto(cairo_move_to)
slim_hidden_proto(cairo_rel_line_to)
slim_hidden_proto(cairo_restore)
slim_hidden_proto(cairo_save)
slim_hidden_proto(cairo_set_target_surface)
slim_hidden_proto(cairo_surface_create_for_drawable)
slim_hidden_proto(cairo_surface_create_for_image)
slim_hidden_proto(cairo_surface_create_similar_solid)
slim_hidden_proto(cairo_surface_destroy)
slim_hidden_proto(cairo_surface_get_matrix)
slim_hidden_proto(cairo_surface_set_matrix)
slim_hidden_proto(cairo_surface_set_repeat)

#endif
