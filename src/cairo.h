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

#ifndef _CAIRO_H_
#define _CAIRO_H_

#include <Xc.h>

typedef struct cairo cairo_t;
typedef struct cairo_surface_t cairo_surface_t;
typedef struct cairo_matrix_t cairo_matrix_t;

#ifdef __cplusplus
extern "C" {
#endif

/* Functions for manipulating state objects */
cairo_t *
cairo_create (void);

void
cairo_destroy (cairo_t *cr);

void
cairo_save (cairo_t *cr);

void
cairo_restore (cairo_t *cr);

/* XXX: I want to rethink this API
void
cairo_push_group (cairo_t *cr);

void
cairo_pop_group (cairo_t *cr);
*/

/* Modify state */
void
cairo_set_target_surface (cairo_t *cr, cairo_surface_t *surface);

typedef enum cairo_format_t {
    CAIRO_FORMAT_ARGB32 = PictStandardARGB32,
    CAIRO_FORMAT_RGB24 = PictStandardRGB24,
    CAIRO_FORMAT_A8 = PictStandardA8,
    CAIRO_FORMAT_A1 = PictStandardA1
} cairo_format_t;

void
cairo_set_target_drawable (cairo_t	*cr,
		     Display	*dpy,
		     Drawable	drawable);

void
cairo_set_target_image (cairo_t	*cr,
		  char		*data,
		  cairo_format_t	format,
		  int		width,
		  int		height,
		  int		stride);

typedef enum cairo_operator_t { 
    CAIRO_OPERATOR_CLEAR = PictOpClear,
    CAIRO_OPERATOR_SRC = PictOpSrc,
    CAIRO_OPERATOR_DST = PictOpDst,
    CAIRO_OPERATOR_OVER = PictOpOver,
    CAIRO_OPERATOR_OVER_REVERSE = PictOpOverReverse,
    CAIRO_OPERATOR_IN = PictOpIn,
    CAIRO_OPERATOR_IN_REVERSE = PictOpInReverse,
    CAIRO_OPERATOR_OUT = PictOpOut,
    CAIRO_OPERATOR_OUT_REVERSE = PictOpOutReverse,
    CAIRO_OPERATOR_ATOP = PictOpAtop,
    CAIRO_OPERATOR_ATOP_REVERSE = PictOpAtopReverse,
    CAIRO_OPERATOR_XOR = PictOpXor,
    CAIRO_OPERATOR_ADD = PictOpAdd,
    CAIRO_OPERATOR_SATURATE = PictOpSaturate,

    CAIRO_OPERATOR_DISJOINT_CLEAR = PictOpDisjointClear,
    CAIRO_OPERATOR_DISJOINT_SRC = PictOpDisjointSrc,
    CAIRO_OPERATOR_DISJOINT_DST = PictOpDisjointDst,
    CAIRO_OPERATOR_DISJOINT_OVER = PictOpDisjointOver,
    CAIRO_OPERATOR_DISJOINT_OVER_REVERSE = PictOpDisjointOverReverse,
    CAIRO_OPERATOR_DISJOINT_IN = PictOpDisjointIn,
    CAIRO_OPERATOR_DISJOINT_IN_REVERSE = PictOpDisjointInReverse,
    CAIRO_OPERATOR_DISJOINT_OUT = PictOpDisjointOut,
    CAIRO_OPERATOR_DISJOINT_OUT_REVERSE = PictOpDisjointOutReverse,
    CAIRO_OPERATOR_DISJOINT_ATOP = PictOpDisjointAtop,
    CAIRO_OPERATOR_DISJOINT_ATOP_REVERSE = PictOpDisjointAtopReverse,
    CAIRO_OPERATOR_DISJOINT_XOR = PictOpDisjointXor,

    CAIRO_OPERATOR_CONJOINT_CLEAR = PictOpConjointClear,
    CAIRO_OPERATOR_CONJOINT_SRC = PictOpConjointSrc,
    CAIRO_OPERATOR_CONJOINT_DST = PictOpConjointDst,
    CAIRO_OPERATOR_CONJOINT_OVER = PictOpConjointOver,
    CAIRO_OPERATOR_CONJOINT_OVER_REVERSE = PictOpConjointOverReverse,
    CAIRO_OPERATOR_CONJOINT_IN = PictOpConjointIn,
    CAIRO_OPERATOR_CONJOINT_IN_REVERSE = PictOpConjointInReverse,
    CAIRO_OPERATOR_CONJOINT_OUT = PictOpConjointOut,
    CAIRO_OPERATOR_CONJOINT_OUT_REVERSE = PictOpConjointOutReverse,
    CAIRO_OPERATOR_CONJOINT_ATOP = PictOpConjointAtop,
    CAIRO_OPERATOR_CONJOINT_ATOP_REVERSE = PictOpConjointAtopReverse,
    CAIRO_OPERATOR_CONJOINT_XOR = PictOpConjointXor
} cairo_operator_t;

void
cairo_set_operator (cairo_t *cr, cairo_operator_t op);

/* XXX: Probably want to bite the bullet and expose a cairo_color_t object */

void
cairo_set_rgb_color (cairo_t *cr, double red, double green, double blue);

void
cairo_set_pattern (cairo_t *cr, cairo_surface_t *pattern);

void
cairo_set_tolerance (cairo_t *cr, double tolerance);

void
cairo_set_alpha (cairo_t *cr, double alpha);

typedef enum cairo_fill_rule_t {
    CAIRO_FILL_RULE_WINDING,
    CAIRO_FILL_RULE_EVEN_ODD
} cairo_fill_rule_t;

void
cairo_set_fill_rule (cairo_t *cr, cairo_fill_rule_t fill_rule);

void
cairo_set_line_width (cairo_t *cr, double width);

typedef enum cairo_line_cap_t {
    CAIRO_LINE_CAP_BUTT,
    CAIRO_LINE_CAP_ROUND,
    CAIRO_LINE_CAP_SQUARE
} cairo_line_cap_t;

void
cairo_set_line_cap (cairo_t *cr, cairo_line_cap_t line_cap);

typedef enum cairo_line_join_t {
    CAIRO_LINE_JOIN_MITER,
    CAIRO_LINE_JOIN_ROUND,
    CAIRO_LINE_JOIN_BEVEL
} cairo_line_join_t;

void
cairo_set_line_join (cairo_t *cr, cairo_line_join_t line_join);

void
cairo_set_dash (cairo_t *cr, double *dashes, int ndash, double offset);

void
cairo_set_miter_limit (cairo_t *cr, double limit);

void
cairo_translate (cairo_t *cr, double tx, double ty);

void
cairo_scale (cairo_t *cr, double sx, double sy);

void
cairo_rotate (cairo_t *cr, double angle);

void
cairo_concat_matrix (cairo_t *cr,
	       cairo_matrix_t *matrix);

void
cairo_set_matrix (cairo_t *cr,
	    cairo_matrix_t *matrix);

void
cairo_default_matrix (cairo_t *cr);

/* XXX: There's been a proposal to add cairo_default_matrix_exact */

void
cairo_identity_matrix (cairo_t *cr);

void
cairo_transform_point (cairo_t *cr, double *x, double *y);

void
cairo_transform_distance (cairo_t *cr, double *dx, double *dy);

void
cairo_inverse_transform_point (cairo_t *cr, double *x, double *y);

void
cairo_inverse_transform_distance (cairo_t *cr, double *dx, double *dy);

/* Path creation functions */
void
cairo_new_path (cairo_t *cr);

void
cairo_move_to (cairo_t *cr, double x, double y);

void
cairo_line_to (cairo_t *cr, double x, double y);

void
cairo_curve_to (cairo_t *cr,
	  double x1, double y1,
	  double x2, double y2,
	  double x3, double y3);

void
cairo_rel_move_to (cairo_t *cr, double dx, double dy);

void
cairo_rel_line_to (cairo_t *cr, double dx, double dy);

void
cairo_rel_curve_to (cairo_t *cr,
	     double dx1, double dy1,
	     double dx2, double dy2,
	     double dx3, double dy3);

void
cairo_rectangle (cairo_t *cr,
	     double x, double y,
	     double width, double height);

void
cairo_close_path (cairo_t *cr);

/* Painting functions */
void
cairo_stroke (cairo_t *cr);

void
cairo_fill (cairo_t *cr);

/* Clipping */
void
cairo_clip (cairo_t *cr);

/* Font/Text functions */

/* XXX: The font support should probably expose a cairo_font_t object with
   several functions, (cairo_font_transform, etc.) in a parallel manner as
   cairo_matrix_t and (eventually) cairo_color_t */
void
cairo_select_font (cairo_t *cr, const char *key);

void
cairo_scale_font (cairo_t *cr, double scale);

/* XXX: Probably want to use a cairo_matrix_t here, (to fix as part of the
   big text support rewrite) */
void
cairo_transform_font (cairo_t *cr,
		double a, double b,
		double c, double d);

void
cairo_text_extents (cairo_t *cr,
	      const unsigned char *utf8,
	      double *x, double *y,
	      double *width, double *height,
	      double *dx, double *dy);

void
cairo_show_text (cairo_t *cr, const unsigned char *utf8);

/* Image functions */

void
cairo_show_surface (cairo_t		*cr,
	       cairo_surface_t	*surface,
	       int		width,
	       int		height);

/* Query functions */

cairo_operator_t
cairo_get_operator (cairo_t *cr);

void
cairo_get_rgb_color (cairo_t *cr, double *red, double *green, double *blue);

/* XXX: Do we want cairo_get_pattern as well? */

double
cairo_get_tolerance (cairo_t *cr);

double
cairo_get_alpha (cairo_t *cr);

void
cairo_get_current_point (cairo_t *cr, double *x, double *y);

cairo_fill_rule_t
cairo_get_fill_rule (cairo_t *cr);

double
cairo_get_line_width (cairo_t *cr);

cairo_line_cap_t
cairo_get_line_cap (cairo_t *cr);

cairo_line_join_t
cairo_get_line_join (cairo_t *cr);

double
cairo_get_miter_limit (cairo_t *cr);

/* XXX: How to do cairo_get_dash??? Do we want to switch to a cairo_dash object? */

void
cairo_get_matrix (cairo_t *cr,
	    double *a, double *b,
	    double *c, double *d,
	    double *tx, double *ty);

cairo_surface_t *
cairo_get_target_surface (cairo_t *cr);

/* Error status queries */

typedef enum cairo_status_t {
    CAIRO_STATUS_SUCCESS = 0,
    CAIRO_STATUS_NO_MEMORY,
    CAIRO_STATUS_INVALID_RESTORE,
    CAIRO_STATUS_INVALID_POP_GROUP,
    CAIRO_STATUS_NO_CURRENT_POINT,
    CAIRO_STATUS_INVALID_MATRIX
} cairo_status_t;

cairo_status_t
cairo_get_status (cairo_t *cr);

const char *
cairo_get_status_string (cairo_t *cr);

/* Surface mainpulation */

/* XXX: This is a mess from the user's POV. Should the Visual or the
   cairo_format_t control what render format is used? Maybe I can have
   cairo_surface_create_for_window with a visual, and
   cairo_surface_create_for_pixmap with a cairo_format_t. Would that work?
*/
cairo_surface_t *
cairo_surface_create_for_drawable (Display	*dpy,
			    Drawable	drawable,
			    Visual	*visual,
			    cairo_format_t	format,
			    Colormap	colormap);

cairo_surface_t *
cairo_surface_create_for_image (char		*data,
			 cairo_format_t	format,
			 int		width,
			 int		height,
			 int		stride);

cairo_surface_t *
cairo_surface_create_similar (cairo_surface_t	*other,
		       cairo_format_t		format,
		       int		width,
		       int		height);

/* XXX: One problem with having RGB and A here in one function is that
   it introduces the question of pre-multiplied vs. non-pre-multiplied
   alpha. Do I want to export a cairo_color_t structure instead? So far, no
   other public functions need it. */
cairo_surface_t *
cairo_surface_create_similar_solid (cairo_surface_t	*other,
			    cairo_format_t	format,
			    int		width,
			    int		height,
			    double	red,
			    double	green,
			    double	blue,
			    double	alpha);

void
cairo_surface_destroy (cairo_surface_t *surface);

/* XXX: Should this take an X/Y offset as well? (Probably) */
cairo_status_t
cairo_surface_put_image (cairo_surface_t	*surface,
		   char		*data,
		   int		width,
		   int		height,
		   int		stride);

/* XXX: The Xc version of this function isn't quite working yet
cairo_status_t
cairo_surface_set_clip_region (cairo_surface_t *surface, Region region);
*/

/* XXX: Note: The current Render/Ic implementations don't do the right
   thing with repeat when the surface has a non-identity matrix. */
cairo_status_t
cairo_surface_set_repeat (cairo_surface_t *surface, int repeat);

cairo_status_t
cairo_surface_set_matrix (cairo_surface_t *surface, cairo_matrix_t *matrix);

cairo_status_t
cairo_surface_get_matrix (cairo_surface_t *surface, cairo_matrix_t *matrix);

typedef enum cairo_filter_t {
    CAIRO_FILTER_FAST = XcFilterFast,
    CAIRO_FILTER_GOOD = XcFilterGood,
    CAIRO_FILTER_BEST = XcFilterBest,
    CAIRO_FILTER_NEAREST = XcFilterNearest,
    CAIRO_FILTER_BILINEAR = XcFilterBilinear
} cairo_filter_t;

cairo_status_t
cairo_surface_set_filter (cairo_surface_t *surface, cairo_filter_t filter);

/* Matrix functions */

cairo_matrix_t *
cairo_matrix_create (void);

void
cairo_matrix_destroy (cairo_matrix_t *matrix);

cairo_status_t
cairo_matrix_copy (cairo_matrix_t *matrix, const cairo_matrix_t *other);

cairo_status_t
cairo_matrix_set_identity (cairo_matrix_t *matrix);

cairo_status_t
cairo_matrix_set_affine (cairo_matrix_t *cr,
		   double a, double b,
		   double c, double d,
		   double tx, double ty);

cairo_status_t
cairo_matrix_get_affine (cairo_matrix_t *matrix,
		   double *a, double *b,
 		   double *c, double *d,
 		   double *tx, double *ty);

cairo_status_t
cairo_matrix_translate (cairo_matrix_t *matrix, double tx, double ty);

cairo_status_t
cairo_matrix_scale (cairo_matrix_t *matrix, double sx, double sy);

cairo_status_t
cairo_matrix_rotate (cairo_matrix_t *matrix, double radians);

cairo_status_t
cairo_matrix_invert (cairo_matrix_t *matrix);

cairo_status_t
cairo_matrix_multiply (cairo_matrix_t *result, const cairo_matrix_t *a, const cairo_matrix_t *b);

cairo_status_t
cairo_matrix_transform_distance (cairo_matrix_t *matrix, double *dx, double *dy);

cairo_status_t
cairo_matrix_transform_point (cairo_matrix_t *matrix, double *x, double *y);

#ifdef __cplusplus
}
#endif

#endif

