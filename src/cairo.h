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

#include <X11/extensions/Xrender.h>
#include <fontconfig/fontconfig.h>
#include <ic.h>

#ifdef _CAIROINT_H_
#include <slim_export.h>
#else
#include <slim_import.h>
#endif

typedef struct cairo cairo_t;
typedef struct cairo_surface cairo_surface_t;
typedef struct cairo_matrix cairo_matrix_t;

#ifdef __cplusplus
extern "C" {
#endif

/* Functions for manipulating state objects */
extern cairo_t * __external_linkage
cairo_create (void);

extern void __external_linkage
cairo_destroy (cairo_t *cr);

extern void __external_linkage
cairo_save (cairo_t *cr);

extern void __external_linkage
cairo_restore (cairo_t *cr);

/* XXX: I want to rethink this API
extern void __external_linkage
cairo_push_group (cairo_t *cr);

extern void __external_linkage
cairo_pop_group (cairo_t *cr);
*/

/* Modify state */
extern void __external_linkage
cairo_set_target_surface (cairo_t *cr, cairo_surface_t *surface);

typedef enum cairo_format {
    CAIRO_FORMAT_ARGB32 = PictStandardARGB32,
    CAIRO_FORMAT_RGB24 = PictStandardRGB24,
    CAIRO_FORMAT_A8 = PictStandardA8,
    CAIRO_FORMAT_A1 = PictStandardA1
} cairo_format_t;

extern void __external_linkage
cairo_set_target_drawable (cairo_t	*cr,
			   Display	*dpy,
			   Drawable	drawable);

extern void __external_linkage
cairo_set_target_image (cairo_t	*cr,
			char		*data,
			cairo_format_t	format,
			int		width,
			int		height,
			int		stride);

typedef enum cairo_operator { 
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

extern void __external_linkage
cairo_set_operator (cairo_t *cr, cairo_operator_t op);

/* XXX: Probably want to bite the bullet and expose a cairo_color_t object */

/* XXX: I've been trying to come up with a sane way to specify:

   cairo_set_color (cairo_t *cr, cairo_color_t *color);

   Keith wants to be able to support super-luminescent colors,
   (premultiplied colors with R/G/B greater than alpha). The current
   API does not allow that. Adding a premulitplied RGBA cairo_color_t
   would do the trick.

   One problem though is that alpha is currently orthogonal to
   color. For example, show_surface uses gstate->alpha but ignores the
   color. So, it doesn't seem be right to have cairo_set_color modify
   the behavior of cairo_show_surface.
*/

extern void __external_linkage
cairo_set_rgb_color (cairo_t *cr, double red, double green, double blue);

extern void __external_linkage
cairo_set_alpha (cairo_t *cr, double alpha);

extern void __external_linkage
cairo_set_pattern (cairo_t *cr, cairo_surface_t *pattern);

extern void __external_linkage
cairo_set_tolerance (cairo_t *cr, double tolerance);

typedef enum cairo_fill_rule {
    CAIRO_FILL_RULE_WINDING,
    CAIRO_FILL_RULE_EVEN_ODD
} cairo_fill_rule_t;

extern void __external_linkage
cairo_set_fill_rule (cairo_t *cr, cairo_fill_rule_t fill_rule);

extern void __external_linkage
cairo_set_line_width (cairo_t *cr, double width);

typedef enum cairo_line_cap {
    CAIRO_LINE_CAP_BUTT,
    CAIRO_LINE_CAP_ROUND,
    CAIRO_LINE_CAP_SQUARE
} cairo_line_cap_t;

extern void __external_linkage
cairo_set_line_cap (cairo_t *cr, cairo_line_cap_t line_cap);

typedef enum cairo_line_join {
    CAIRO_LINE_JOIN_MITER,
    CAIRO_LINE_JOIN_ROUND,
    CAIRO_LINE_JOIN_BEVEL
} cairo_line_join_t;

extern void __external_linkage
cairo_set_line_join (cairo_t *cr, cairo_line_join_t line_join);

extern void __external_linkage
cairo_set_dash (cairo_t *cr, double *dashes, int ndash, double offset);

extern void __external_linkage
cairo_set_miter_limit (cairo_t *cr, double limit);

extern void __external_linkage
cairo_translate (cairo_t *cr, double tx, double ty);

extern void __external_linkage
cairo_scale (cairo_t *cr, double sx, double sy);

extern void __external_linkage
cairo_rotate (cairo_t *cr, double angle);

extern void __external_linkage
cairo_concat_matrix (cairo_t *cr,
	       cairo_matrix_t *matrix);

extern void __external_linkage
cairo_set_matrix (cairo_t *cr,
	    cairo_matrix_t *matrix);

extern void __external_linkage
cairo_default_matrix (cairo_t *cr);

/* XXX: There's been a proposal to add cairo_default_matrix_exact */

extern void __external_linkage
cairo_identity_matrix (cairo_t *cr);

extern void __external_linkage
cairo_transform_point (cairo_t *cr, double *x, double *y);

extern void __external_linkage
cairo_transform_distance (cairo_t *cr, double *dx, double *dy);

extern void __external_linkage
cairo_inverse_transform_point (cairo_t *cr, double *x, double *y);

extern void __external_linkage
cairo_inverse_transform_distance (cairo_t *cr, double *dx, double *dy);

/* Path creation functions */
extern void __external_linkage
cairo_new_path (cairo_t *cr);

extern void __external_linkage
cairo_move_to (cairo_t *cr, double x, double y);

extern void __external_linkage
cairo_line_to (cairo_t *cr, double x, double y);

extern void __external_linkage
cairo_curve_to (cairo_t *cr,
	  double x1, double y1,
	  double x2, double y2,
	  double x3, double y3);

extern void __external_linkage
cairo_rel_move_to (cairo_t *cr, double dx, double dy);

extern void __external_linkage
cairo_rel_line_to (cairo_t *cr, double dx, double dy);

extern void __external_linkage
cairo_rel_curve_to (cairo_t *cr,
		    double dx1, double dy1,
		    double dx2, double dy2,
		    double dx3, double dy3);

extern void __external_linkage
cairo_rectangle (cairo_t *cr,
		 double x, double y,
		 double width, double height);

/* XXX: This is the same name that PostScript uses, but to me the name
   suggests an actual drawing operation ala cairo_stroke --- especially
   since I want to add a cairo_path_t and with that it would be
   natural to have "cairo_stroke_path (cairo_t *, cairo_path_t *)"

   Maybe we could use something like "cairo_outline_path (cairo_t *)"? 
*/
/* XXX: NYI
extern void __external_linkage
cairo_stroke_path (cairo_t *cr);
*/

extern void __external_linkage
cairo_close_path (cairo_t *cr);

/* Painting functions */
extern void __external_linkage
cairo_stroke (cairo_t *cr);

extern void __external_linkage
cairo_fill (cairo_t *cr);

/* Clipping */
extern void __external_linkage
cairo_clip (cairo_t *cr);

/* Font/Text functions */

/* XXX: The font support should probably expose a cairo_font_t object with
   several functions, (cairo_font_transform, etc.) in a parallel manner as
   cairo_matrix_t and (eventually) cairo_color_t */
extern void __external_linkage
cairo_select_font (cairo_t *cr, const char *key);

extern void __external_linkage
cairo_scale_font (cairo_t *cr, double scale);

/* XXX: Probably want to use a cairo_matrix_t here, (to fix as part of the
   big text support rewrite) */
extern void __external_linkage
cairo_transform_font (cairo_t *cr,
		double a, double b,
		double c, double d);

extern void __external_linkage
cairo_text_extents (cairo_t *cr,
	      const unsigned char *utf8,
	      double *x, double *y,
	      double *width, double *height,
	      double *dx, double *dy);

extern void __external_linkage
cairo_show_text (cairo_t *cr, const unsigned char *utf8);

/* Image functions */

extern void __external_linkage
cairo_show_surface (cairo_t		*cr,
		    cairo_surface_t	*surface,
		    int		width,
		    int		height);

/* Query functions */

/* XXX: It would be nice if I could find a simpler way to make the
   definitions for the deprecated functions. Ideally, I would just
   have to put DEPRECATE (cairo_get_operator, cairo_current_operator)
   into one file and be done with it. For now, I've got a little more
   typing than that. */

extern cairo_operator_t __external_linkage
cairo_current_operator (cairo_t *cr);

extern void __external_linkage
cairo_current_rgb_color (cairo_t *cr, double *red, double *green, double *blue);

extern double __external_linkage
cairo_current_alpha (cairo_t *cr);

/* XXX: Do we want cairo_current_pattern as well? */

extern double __external_linkage
cairo_current_tolerance (cairo_t *cr);

extern void __external_linkage
cairo_current_point (cairo_t *cr, double *x, double *y);

extern cairo_fill_rule_t __external_linkage
cairo_current_fill_rule (cairo_t *cr);

extern double __external_linkage
cairo_current_line_width (cairo_t *cr);

extern cairo_line_cap_t __external_linkage
cairo_current_line_cap (cairo_t *cr);

extern cairo_line_join_t __external_linkage
cairo_current_line_join (cairo_t *cr);

extern double __external_linkage
cairo_current_miter_limit (cairo_t *cr);

/* XXX: How to do cairo_current_dash??? Do we want to switch to a cairo_dash object? */

extern void __external_linkage
cairo_current_matrix (cairo_t *cr, cairo_matrix_t *matrix);

extern cairo_surface_t * __external_linkage
cairo_current_target_surface (cairo_t *cr);

/* Error status queries */

typedef enum cairo_status {
    CAIRO_STATUS_SUCCESS = 0,
    CAIRO_STATUS_NO_MEMORY,
    CAIRO_STATUS_INVALID_RESTORE,
    CAIRO_STATUS_INVALID_POP_GROUP,
    CAIRO_STATUS_NO_CURRENT_POINT,
    CAIRO_STATUS_INVALID_MATRIX
} cairo_status_t;

extern cairo_status_t __external_linkage
cairo_status (cairo_t *cr);

extern const char * __external_linkage
cairo_status_string (cairo_t *cr);

/* Surface mainpulation */

/* XXX: This is a mess from the user's POV. Should the Visual or the
   cairo_format_t control what render format is used? Maybe I can have
   cairo_surface_create_for_window with a visual, and
   cairo_surface_create_for_pixmap with a cairo_format_t. Would that work?
*/
extern cairo_surface_t * __external_linkage
cairo_surface_create_for_drawable (Display		*dpy,
				   Drawable		drawable,
				   Visual		*visual,
				   cairo_format_t	format,
				   Colormap		colormap);

extern cairo_surface_t * __external_linkage
cairo_surface_create_for_image (char		*data,
				cairo_format_t	format,
				int		width,
				int		height,
				int		stride);

extern cairo_surface_t * __external_linkage
cairo_surface_create_similar (cairo_surface_t	*other,
			      cairo_format_t	format,
			      int		width,
			      int		height);

/* XXX: One problem with having RGB and A here in one function is that
   it introduces the question of pre-multiplied vs. non-pre-multiplied
   alpha. Do I want to export a cairo_color_t structure instead? So far, no
   other public functions need it. */
extern cairo_surface_t * __external_linkage
cairo_surface_create_similar_solid (cairo_surface_t	*other,
				    cairo_format_t	format,
				    int		width,
				    int		height,
				    double	red,
				    double	green,
				    double	blue,
				    double	alpha);

extern void __external_linkage
cairo_surface_destroy (cairo_surface_t *surface);

extern cairo_status_t __external_linkage
cairo_surface_put_image (cairo_surface_t	*surface,
			 char			*data,
			 int			width,
			 int			height,
			 int			stride);

/* XXX: NYI
extern cairo_status_t __external_linkage
cairo_surface_clip_restore (cairo_surface_t *surface);

extern cairo_status_t __external_linkage
cairo_surface_clip_rectangle (cairo_surface_t *surface,
			      int x, int y,
			      int width, int height);
*/

/* XXX: Note: The current Render/Ic implementations don't do the right
   thing with repeat when the surface has a non-identity matrix. */
extern cairo_status_t __external_linkage
cairo_surface_set_repeat (cairo_surface_t *surface, int repeat);

extern cairo_status_t __external_linkage
cairo_surface_set_matrix (cairo_surface_t *surface, cairo_matrix_t *matrix);

extern cairo_status_t __external_linkage
cairo_surface_get_matrix (cairo_surface_t *surface, cairo_matrix_t *matrix);

typedef enum cairo_filter {
    CAIRO_FILTER_FAST = IcFilterFast,
    CAIRO_FILTER_GOOD = IcFilterGood,
    CAIRO_FILTER_BEST = IcFilterBest,
    CAIRO_FILTER_NEAREST = IcFilterNearest,
    CAIRO_FILTER_BILINEAR = IcFilterBilinear
} cairo_filter_t;

extern cairo_status_t __external_linkage
cairo_surface_set_filter (cairo_surface_t *surface, cairo_filter_t filter);

/* Matrix functions */

extern cairo_matrix_t * __external_linkage
cairo_matrix_create (void);

extern void __external_linkage
cairo_matrix_destroy (cairo_matrix_t *matrix);

extern cairo_status_t __external_linkage
cairo_matrix_copy (cairo_matrix_t *matrix, const cairo_matrix_t *other);

extern cairo_status_t __external_linkage
cairo_matrix_set_identity (cairo_matrix_t *matrix);

extern cairo_status_t __external_linkage
cairo_matrix_set_affine (cairo_matrix_t *cr,
			 double a, double b,
			 double c, double d,
			 double tx, double ty);

extern cairo_status_t __external_linkage
cairo_matrix_get_affine (cairo_matrix_t *matrix,
			 double *a, double *b,
			 double *c, double *d,
			 double *tx, double *ty);

extern cairo_status_t __external_linkage
cairo_matrix_translate (cairo_matrix_t *matrix, double tx, double ty);

extern cairo_status_t __external_linkage
cairo_matrix_scale (cairo_matrix_t *matrix, double sx, double sy);

extern cairo_status_t __external_linkage
cairo_matrix_rotate (cairo_matrix_t *matrix, double radians);

extern cairo_status_t __external_linkage
cairo_matrix_invert (cairo_matrix_t *matrix);

extern cairo_status_t __external_linkage
cairo_matrix_multiply (cairo_matrix_t *result, const cairo_matrix_t *a, const cairo_matrix_t *b);

extern cairo_status_t __external_linkage
cairo_matrix_transform_distance (cairo_matrix_t *matrix, double *dx, double *dy);

extern cairo_status_t __external_linkage
cairo_matrix_transform_point (cairo_matrix_t *matrix, double *x, double *y);

/* Deprecated functions. We've made some effort to allow the
   deprecated functions to continue to work for now, (with useful
   warnings). But the deprecated functions will not appear in the next
   release. */
#define cairo_get_operator	cairo_get_operator_DEPRECATED_BY_cairo_current_operator
#define cairo_get_rgb_color	cairo_get_rgb_color_DEPRECATED_BY_cairo_current_rgb_color
#define cairo_get_alpha	 	cairo_get_alpha_DEPRECATED_BY_cairo_current_alpha
#define cairo_get_tolerance	cairo_get_tolerance_DEPRECATED_BY_cairo_current_tolerance
#define cairo_get_current_point	cairo_get_current_point_DEPRECATED_BY_cairo_current_point
#define cairo_get_fill_rule	cairo_get_fill_rule_DEPRECATED_BY_cairo_current_fill_rule
#define cairo_get_line_width	cairo_get_line_width_DEPRECATED_BY_cairo_current_line_width
#define cairo_get_line_cap	cairo_get_line_cap_DEPRECATED_BY_cairo_current_line_cap
#define cairo_get_line_join	cairo_get_line_join_DEPRECATED_BY_cairo_current_line_join
#define cairo_get_miter_limit	cairo_get_miter_limit_DEPRECATED_BY_cairo_current_miter_limit
#define cairo_get_matrix	cairo_get_matrix_DEPRECATED_BY_cairo_current_matrix
#define cairo_get_target_surface	cairo_get_target_surface_DEPRECATED_BY_cairo_current_target_surface
#define cairo_get_status	 	cairo_get_status_DEPRECATED_BY_cairo_status
#define cairo_get_status_string		cairo_get_status_string_DEPRECATED_BY_cairo_status_string

#ifdef __cplusplus
}
#endif

#undef __external_linkage

#endif
