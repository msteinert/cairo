/*
 * Copyright © 2002 University of Southern California
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

#include <cairo-features.h>

#include <pixman.h>
#include <stdio.h>

typedef struct cairo cairo_t;
typedef struct cairo_surface cairo_surface_t;
typedef struct cairo_matrix cairo_matrix_t;

#ifdef __cplusplus
extern "C" {
#endif

/* From slim_export.h and slim_import.h */
#if defined(WIN32) || defined(__CYGWIN__)
# if defined(_CAIROINT_H_)
#  define __external_linkage	__declspec(dllexport)
# else
#  define __external_linkage	__declspec(dllimport)
# endif
#else
# define __external_linkage
#endif

/* Functions for manipulating state objects */
extern cairo_t * __external_linkage
cairo_create (void);

extern void __external_linkage
cairo_reference (cairo_t *cr);

extern void __external_linkage
cairo_destroy (cairo_t *cr);

extern void __external_linkage
cairo_save (cairo_t *cr);

extern void __external_linkage
cairo_restore (cairo_t *cr);

extern void __external_linkage
cairo_copy (cairo_t *dest, cairo_t *src);

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
    CAIRO_FORMAT_ARGB32,
    CAIRO_FORMAT_RGB24,
    CAIRO_FORMAT_A8,
    CAIRO_FORMAT_A1
} cairo_format_t;

extern void __external_linkage
cairo_set_target_image (cairo_t	*cr,
			char		*data,
			cairo_format_t	format,
			int		width,
			int		height,
			int		stride);

extern void __external_linkage
cairo_set_target_ps (cairo_t	*cr,
		     FILE	*file,
		     double	width_inches,
		     double	height_inches,
		     double	x_pixels_per_inch,
		     double	y_pixels_per_inch);

#ifdef CAIRO_HAS_XLIB_SURFACE

#include <X11/extensions/Xrender.h>

/* XXX: This shold be renamed to cairo_set_target_xlib to match the
 * other backends */
extern void __external_linkage
cairo_set_target_drawable (cairo_t	*cr,
			   Display	*dpy,
			   Drawable	drawable);
#endif /* CAIRO_HAS_XLIB_SURFACE */

typedef enum cairo_operator { 
    CAIRO_OPERATOR_CLEAR,
    CAIRO_OPERATOR_SRC,
    CAIRO_OPERATOR_DST,
    CAIRO_OPERATOR_OVER,
    CAIRO_OPERATOR_OVER_REVERSE,
    CAIRO_OPERATOR_IN,
    CAIRO_OPERATOR_IN_REVERSE,
    CAIRO_OPERATOR_OUT,
    CAIRO_OPERATOR_OUT_REVERSE,
    CAIRO_OPERATOR_ATOP,
    CAIRO_OPERATOR_ATOP_REVERSE,
    CAIRO_OPERATOR_XOR,
    CAIRO_OPERATOR_ADD,
    CAIRO_OPERATOR_SATURATE,
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
cairo_arc (cairo_t *cr,
	   double xc, double yc,
	   double radius,
	   double angle1, double angle2);

extern void __external_linkage
cairo_arc_negative (cairo_t *cr,
		    double xc, double yc,
		    double radius,
		    double angle1, double angle2);

/* XXX: NYI
extern void __external_linkage
cairo_arc_to (cairo_t *cr,
	      double x1, double y1,
	      double x2, double y2,
	      double radius);
*/

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

extern void __external_linkage
cairo_copy_page (cairo_t *cr);

extern void __external_linkage
cairo_show_page (cairo_t *cr);

/* Insideness testing */
extern int __external_linkage
cairo_in_stroke (cairo_t *cr, double x, double y);

extern int __external_linkage
cairo_in_fill (cairo_t *cr, double x, double y);

/* Clipping */
extern void __external_linkage
cairo_clip (cairo_t *cr);

/* Font/Text functions */

typedef struct cairo_font cairo_font_t;

typedef struct {
  unsigned long        index;
  double               x;
  double               y;
} cairo_glyph_t;

typedef struct {
    double left_side_bearing;
    double right_side_bearing;
    double ascent;
    double descent;
    double x_advance;
    double y_advance;
} cairo_text_extents_t;

typedef struct {
    double ascent;
    double descent;
    double height;
    double max_x_advance;
    double max_y_advance;
} cairo_font_extents_t;

typedef enum cairo_font_weight {
  CAIRO_FONT_WEIGHT_NORMAL,
  CAIRO_FONT_WEIGHT_BOLD
} cairo_font_weight_t;
  
typedef enum cairo_font_slant {
  CAIRO_FONT_SLANT_NORMAL,
  CAIRO_FONT_SLANT_ITALIC,
  CAIRO_FONT_SLANT_OBLIQUE
} cairo_font_slant_t;
  

/* This interface is for dealing with text as text, not caring about the
   font object inside the the cairo_t. */

extern void __external_linkage
cairo_select_font (cairo_t              *ct, 
		   const char           *family, 
		   cairo_font_slant_t   slant, 
		   cairo_font_weight_t  weight);

extern void __external_linkage
cairo_scale_font (cairo_t *cr, double scale);

extern void __external_linkage
cairo_transform_font (cairo_t *cr, cairo_matrix_t *matrix);

extern void __external_linkage
cairo_show_text (cairo_t *ct, const unsigned char *utf8);

extern void __external_linkage
cairo_show_glyphs (cairo_t *ct, cairo_glyph_t *glyphs, int num_glyphs);

extern cairo_font_t * __external_linkage
cairo_current_font (cairo_t *ct);

extern void __external_linkage
cairo_current_font_extents (cairo_t *ct, 
			    cairo_font_extents_t *extents);

extern void __external_linkage
cairo_set_font (cairo_t *ct, cairo_font_t *font);


/* XXX: NYI

extern void __external_linkage
cairo_text_extents (cairo_t                *ct,
		    const unsigned char    *utf8,
		    cairo_text_extents_t   *extents);

extern void __external_linkage
cairo_glyph_extents (cairo_t               *ct,
		     cairo_glyph_t         *glyphs, 
		     int                   num_glyphs,
		     cairo_text_extents_t  *extents);

extern void __external_linkage
cairo_text_path  (cairo_t *ct, const unsigned char *utf8);

extern void __external_linkage
cairo_glyph_path (cairo_t *ct, cairo_glyph_t *glyphs, int num_glyphs);

*/


/* Portable interface to general font features. */
  
extern void __external_linkage
cairo_font_reference (cairo_font_t *font);

extern void __external_linkage
cairo_font_destroy (cairo_font_t *font);

extern void __external_linkage
cairo_font_set_transform (cairo_font_t *font, 
			  cairo_matrix_t *matrix);

extern void __external_linkage
cairo_font_current_transform (cairo_font_t *font, 
			      cairo_matrix_t *matrix);


/* Fontconfig/Freetype platform-specific font interface */

#include <fontconfig/fontconfig.h>
#include <ft2build.h>
#include FT_FREETYPE_H

extern cairo_font_t * __external_linkage
cairo_ft_font_create (FT_Library ft_library, FcPattern *pattern);

extern cairo_font_t * __external_linkage
cairo_ft_font_create_for_ft_face (FT_Face face);

extern void __external_linkage
cairo_ft_font_destroy (cairo_font_t *ft_font);

extern FT_Face __external_linkage
cairo_ft_font_face (cairo_font_t *ft_font);

extern FcPattern * __external_linkage
cairo_ft_font_pattern (cairo_font_t  *ft_font);



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

/* XXX: Need to decide the memory mangement semantics of this
   function. Should it reference the surface again? */
extern cairo_surface_t * __external_linkage
cairo_current_target_surface (cairo_t *cr);

/* Error status queries */

typedef enum cairo_status {
    CAIRO_STATUS_SUCCESS = 0,
    CAIRO_STATUS_NO_MEMORY,
    CAIRO_STATUS_INVALID_RESTORE,
    CAIRO_STATUS_INVALID_POP_GROUP,
    CAIRO_STATUS_NO_CURRENT_POINT,
    CAIRO_STATUS_INVALID_MATRIX,
    CAIRO_STATUS_NO_TARGET_SURFACE,
    CAIRO_STATUS_NULL_POINTER
} cairo_status_t;

extern cairo_status_t __external_linkage
cairo_status (cairo_t *cr);

extern const char * __external_linkage
cairo_status_string (cairo_t *cr);

/* Surface manipulation */
/* XXX: We may want to rename this function in light of the new
   virtualized surface backends... */
extern cairo_surface_t * __external_linkage
cairo_surface_create_for_image (char           *data,
				cairo_format_t  format,
				int             width,
				int             height,
				int             stride);

/* XXX: I want to remove this function, (replace with
   cairo_set_target_scratch or similar). */
extern cairo_surface_t * __external_linkage
cairo_surface_create_similar (cairo_surface_t	*other,
			      cairo_format_t	format,
			      int		width,
			      int		height);

extern void __external_linkage
cairo_surface_reference (cairo_surface_t *surface);

extern void __external_linkage
cairo_surface_destroy (cairo_surface_t *surface);

/* XXX: NYI
extern cairo_status_t __external_linkage
cairo_surface_clip_restore (cairo_surface_t *surface);

extern cairo_status_t __external_linkage
cairo_surface_clip_begin (cairo_surface_t *surface);

extern cairo_status_t __external_linkage
cairo_surface_clip_rectangle (cairo_surface_t *surface,
			      int x, int y,
			      int width, int height);
*/

/* XXX: Note: The current Render/Ic implementations don't do the right
   thing with repeat when the surface has a non-identity matrix. */
/* XXX: Rework this as a cairo function with an enum: cairo_set_pattern_extend */
extern cairo_status_t __external_linkage
cairo_surface_set_repeat (cairo_surface_t *surface, int repeat);

/* XXX: Rework this as a cairo function: cairo_set_pattern_transform */
extern cairo_status_t __external_linkage
cairo_surface_set_matrix (cairo_surface_t *surface, cairo_matrix_t *matrix);

/* XXX: Rework this as a cairo function: cairo_current_pattern_transform */
extern cairo_status_t __external_linkage
cairo_surface_get_matrix (cairo_surface_t *surface, cairo_matrix_t *matrix);

typedef enum cairo_filter {
    CAIRO_FILTER_FAST,
    CAIRO_FILTER_GOOD,
    CAIRO_FILTER_BEST,
    CAIRO_FILTER_NEAREST,
    CAIRO_FILTER_BILINEAR,
} cairo_filter_t;

/* XXX: Rework this as a cairo function: cairo_set_pattern_filter */
extern cairo_status_t __external_linkage
cairo_surface_set_filter (cairo_surface_t *surface, cairo_filter_t filter);

/* Image-surface functions */

extern cairo_surface_t * __external_linkage
cairo_image_surface_create (cairo_format_t	format,
			    int			width,
			    int			height);

extern cairo_surface_t * __external_linkage
cairo_image_surface_create_for_data (char			*data,
				     cairo_format_t		format,
				     int			width,
				     int			height,
				     int			stride);

/* PS-surface functions */

extern cairo_surface_t * __external_linkage
cairo_ps_surface_create (FILE	*file,
			 double	width_inches,
			 double height_inches,
			 double	x_pixels_per_inch,
			 double	y_pixels_per_inch);

#ifdef CAIRO_HAS_XLIB_SURFACE

/* XXX: This is a mess from the user's POV. Should the Visual or the
   cairo_format_t control what render format is used? Maybe I can have
   cairo_surface_create_for_window with a visual, and
   cairo_surface_create_for_pixmap with a cairo_format_t. Would that work?
*/
extern cairo_surface_t * __external_linkage
cairo_xlib_surface_create (Display		*dpy,
			   Drawable		drawable,
			   Visual		*visual,
			   cairo_format_t	format,
			   Colormap		colormap);

#endif /* CAIRO_HAS_XLIB_SURFACE */

/* Matrix functions */

/* XXX: Rename all of these to cairo_transform_t */

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
#ifndef _CAIROINT_H_
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
#endif

#ifdef __cplusplus
}
#endif

#undef __external_linkage

#endif
