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

#include "cairoint.h"

#define CAIRO_TOLERANCE_MINIMUM	0.0002 /* We're limited by 16 bits of sub-pixel precision */

cairo_t *
cairo_create (void)
{
    cairo_t *cr;

    cr = malloc (sizeof (cairo_t));
    if (cr == NULL)
	return NULL;

    cr->status = CAIRO_STATUS_SUCCESS;
    cr->ref_count = 1;

    cr->gstate = _cairo_gstate_create ();
    if (cr->gstate == NULL)
	cr->status = CAIRO_STATUS_NO_MEMORY;

    return cr;
}

void
cairo_reference (cairo_t *cr)
{
    if (cr->status)
	return;

    cr->ref_count++;
}

void
cairo_destroy (cairo_t *cr)
{
    cr->ref_count--;
    if (cr->ref_count)
	return;

    while (cr->gstate) {
	cairo_gstate_t *tmp = cr->gstate;
	cr->gstate = tmp->next;

	_cairo_gstate_destroy (tmp);
    }

    free (cr);
}

void
cairo_save (cairo_t *cr)
{
    cairo_gstate_t *top;

    if (cr->status)
	return;

    if (cr->gstate) {
	top = _cairo_gstate_clone (cr->gstate);
    } else {
	top = _cairo_gstate_create ();
    }

    if (top == NULL) {
	cr->status = CAIRO_STATUS_NO_MEMORY;
	return;
    }

    top->next = cr->gstate;
    cr->gstate = top;
}
slim_hidden_def(cairo_save);

void
cairo_restore (cairo_t *cr)
{
    cairo_gstate_t *top;

    if (cr->status)
	return;

    top = cr->gstate;
    cr->gstate = top->next;

    _cairo_gstate_destroy (top);

    if (cr->gstate == NULL)
	cr->status = CAIRO_STATUS_INVALID_RESTORE;
}
slim_hidden_def(cairo_restore);

void
cairo_copy (cairo_t *dest, cairo_t *src)
{
    if (dest->status)
	return;

    if (src->status) {
	dest->status = src->status;
	return;
    }

    dest->status = _cairo_gstate_copy (dest->gstate, src->gstate);
}

/* XXX: I want to rethink this API
void
cairo_push_group (cairo_t *cr)
{
    if (cr->status)
	return;

    cr->status = cairoPush (cr);
    if (cr->status)
	return;

    cr->status = _cairo_gstate_begin_group (cr->gstate);
}

void
cairo_pop_group (cairo_t *cr)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_end_group (cr->gstate);
    if (cr->status)
	return;

    cr->status = cairoPop (cr);
}
*/

void
cairo_set_target_surface (cairo_t *cr, cairo_surface_t *surface)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_set_target_surface (cr->gstate, surface);
}
slim_hidden_def(cairo_set_target_surface);

void
cairo_set_target_image (cairo_t		*cr,
			char		*data,
			cairo_format_t	format,
			int		width,
			int		height,
			int		stride)
{
    cairo_surface_t *surface;

    if (cr->status)
	return;

    surface = cairo_surface_create_for_image (data,
					      format,
					      width, height, stride);
    if (surface == NULL) {
	cr->status = CAIRO_STATUS_NO_MEMORY;
	return;
    }

    cairo_set_target_surface (cr, surface);

    cairo_surface_destroy (surface);
}

void
cairo_set_operator (cairo_t *cr, cairo_operator_t op)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_set_operator (cr->gstate, op);
}

void
cairo_set_rgb_color (cairo_t *cr, double red, double green, double blue)
{
    if (cr->status)
	return;

    _cairo_restrict_value (&red, 0.0, 1.0);
    _cairo_restrict_value (&green, 0.0, 1.0);
    _cairo_restrict_value (&blue, 0.0, 1.0);

    cr->status = _cairo_gstate_set_rgb_color (cr->gstate, red, green, blue);
}

void
cairo_set_pattern (cairo_t *cr, cairo_pattern_t *pattern)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_set_pattern (cr->gstate, pattern);
}

cairo_pattern_t *
cairo_current_pattern (cairo_t *cr)
{
    return _cairo_gstate_current_pattern (cr->gstate);
}

void
cairo_set_tolerance (cairo_t *cr, double tolerance)
{
    if (cr->status)
	return;

    _cairo_restrict_value (&tolerance, CAIRO_TOLERANCE_MINIMUM, tolerance);

    cr->status = _cairo_gstate_set_tolerance (cr->gstate, tolerance);
}

void
cairo_set_alpha (cairo_t *cr, double alpha)
{
    if (cr->status)
	return;

    _cairo_restrict_value (&alpha, 0.0, 1.0);

    cr->status = _cairo_gstate_set_alpha (cr->gstate, alpha);
}

void
cairo_set_fill_rule (cairo_t *cr, cairo_fill_rule_t fill_rule)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_set_fill_rule (cr->gstate, fill_rule);
}

void
cairo_set_line_width (cairo_t *cr, double width)
{
    if (cr->status)
	return;

    _cairo_restrict_value (&width, 0.0, width);

    cr->status = _cairo_gstate_set_line_width (cr->gstate, width);
}

void
cairo_set_line_cap (cairo_t *cr, cairo_line_cap_t line_cap)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_set_line_cap (cr->gstate, line_cap);
}

void
cairo_set_line_join (cairo_t *cr, cairo_line_join_t line_join)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_set_line_join (cr->gstate, line_join);
}

void
cairo_set_dash (cairo_t *cr, double *dashes, int ndash, double offset)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_set_dash (cr->gstate, dashes, ndash, offset);
}

void
cairo_set_miter_limit (cairo_t *cr, double limit)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_set_miter_limit (cr->gstate, limit);
}

void
cairo_translate (cairo_t *cr, double tx, double ty)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_translate (cr->gstate, tx, ty);
}

void
cairo_scale (cairo_t *cr, double sx, double sy)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_scale (cr->gstate, sx, sy);
}

void
cairo_rotate (cairo_t *cr, double angle)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_rotate (cr->gstate, angle);
}

void
cairo_concat_matrix (cairo_t *cr,
	       cairo_matrix_t *matrix)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_concat_matrix (cr->gstate, matrix);
}

void
cairo_set_matrix (cairo_t *cr,
		  cairo_matrix_t *matrix)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_set_matrix (cr->gstate, matrix);
}

void
cairo_default_matrix (cairo_t *cr)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_default_matrix (cr->gstate);
}

void
cairo_identity_matrix (cairo_t *cr)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_identity_matrix (cr->gstate);
}

void
cairo_transform_point (cairo_t *cr, double *x, double *y)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_transform_point (cr->gstate, x, y);
}

void
cairo_transform_distance (cairo_t *cr, double *dx, double *dy)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_transform_distance (cr->gstate, dx, dy);
}

void
cairo_inverse_transform_point (cairo_t *cr, double *x, double *y)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_inverse_transform_point (cr->gstate, x, y);
}

void
cairo_inverse_transform_distance (cairo_t *cr, double *dx, double *dy)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_inverse_transform_distance (cr->gstate, dx, dy);
}

void
cairo_new_path (cairo_t *cr)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_new_path (cr->gstate);
}

void
cairo_move_to (cairo_t *cr, double x, double y)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_move_to (cr->gstate, x, y);
}
slim_hidden_def(cairo_move_to);

void
cairo_line_to (cairo_t *cr, double x, double y)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_line_to (cr->gstate, x, y);
}

void
cairo_curve_to (cairo_t *cr,
		double x1, double y1,
		double x2, double y2,
		double x3, double y3)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_curve_to (cr->gstate,
					 x1, y1,
					 x2, y2,
					 x3, y3);
}

void
cairo_arc (cairo_t *cr,
	   double xc, double yc,
	   double radius,
	   double angle1, double angle2)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_arc (cr->gstate,
				    xc, yc,
				    radius,
				    angle1, angle2);
}

void
cairo_arc_negative (cairo_t *cr,
		    double xc, double yc,
		    double radius,
		    double angle1, double angle2)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_arc_negative (cr->gstate,
					     xc, yc,
					     radius,
					     angle1, angle2);
}

/* XXX: NYI
void
cairo_arc_to (cairo_t *cr,
	      double x1, double y1,
	      double x2, double y2,
	      double radius)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_arc_to (cr->gstate,
				       x1, y1,
				       x2, y2,
				       radius);
}
*/

void
cairo_rel_move_to (cairo_t *cr, double dx, double dy)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_rel_move_to (cr->gstate, dx, dy);
}

void
cairo_rel_line_to (cairo_t *cr, double dx, double dy)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_rel_line_to (cr->gstate, dx, dy);
}
slim_hidden_def(cairo_rel_line_to);

void
cairo_rel_curve_to (cairo_t *cr,
		    double dx1, double dy1,
		    double dx2, double dy2,
		    double dx3, double dy3)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_rel_curve_to (cr->gstate,
					     dx1, dy1,
					     dx2, dy2,
					     dx3, dy3);
}

void
cairo_rectangle (cairo_t *cr,
		 double x, double y,
		 double width, double height)
{
    if (cr->status)
	return;

    cairo_move_to (cr, x, y);
    cairo_rel_line_to (cr, width, 0);
    cairo_rel_line_to (cr, 0, height);
    cairo_rel_line_to (cr, -width, 0);
    cairo_close_path (cr);
}

/* XXX: NYI
void
cairo_stroke_path (cairo_t *cr)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_stroke_path (cr->gstate);
}
*/

void
cairo_close_path (cairo_t *cr)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_close_path (cr->gstate);
}
slim_hidden_def(cairo_close_path);

void
cairo_stroke (cairo_t *cr)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_stroke (cr->gstate);
}

void
cairo_fill (cairo_t *cr)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_fill (cr->gstate);
}

void
cairo_copy_page (cairo_t *cr)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_copy_page (cr->gstate);
}

void
cairo_show_page (cairo_t *cr)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_show_page (cr->gstate);
}

int
cairo_in_stroke (cairo_t *cr, double x, double y)
{
    int inside;

    if (cr->status)
	return 0;

    cr->status = _cairo_gstate_in_stroke (cr->gstate, x, y, &inside);

    if (cr->status)
	return 0;

    return inside;
}

int
cairo_in_fill (cairo_t *cr, double x, double y)
{
    int inside;

    if (cr->status)
	return 0;

    cr->status = _cairo_gstate_in_fill (cr->gstate, x, y, &inside);

    if (cr->status)
	return 0;

    return inside;
}

void
cairo_stroke_extents (cairo_t *cr,
                      double *x1, double *y1, double *x2, double *y2)
{
    if (cr->status)
	return;
    
    cr->status = _cairo_gstate_stroke_extents (cr->gstate, x1, y1, x2, y2);
}

void
cairo_fill_extents (cairo_t *cr,
                    double *x1, double *y1, double *x2, double *y2)
{
    if (cr->status)
	return;
    
    cr->status = _cairo_gstate_fill_extents (cr->gstate, x1, y1, x2, y2);
}

void
cairo_init_clip (cairo_t *cr)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_init_clip (cr->gstate);
}

void
cairo_clip (cairo_t *cr)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_clip (cr->gstate);
}

void
cairo_select_font (cairo_t              *cr, 
		   const char           *family, 
		   cairo_font_slant_t   slant, 
		   cairo_font_weight_t  weight)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_select_font (cr->gstate, family, slant, weight);
}

cairo_font_t *
cairo_current_font (cairo_t *cr)
{
    cairo_font_t *ret;  

    if (cr->status)
	return NULL;

    cr->status = _cairo_gstate_current_font (cr->gstate, &ret);  
    return ret;
}

void
cairo_current_font_extents (cairo_t *ct, 
			    cairo_font_extents_t *extents)
{
    if (ct->status)
	return;

    ct->status = _cairo_gstate_current_font_extents (ct->gstate, extents);
}


void
cairo_set_font (cairo_t *cr, cairo_font_t *font)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_set_font (cr->gstate, font);  
}

void
cairo_scale_font (cairo_t *cr, double scale)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_scale_font (cr->gstate, scale);
}

void
cairo_transform_font (cairo_t *cr, cairo_matrix_t *matrix)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_transform_font (cr->gstate, matrix);
}

void
cairo_text_extents (cairo_t                *cr,
		    const unsigned char    *utf8,
		    cairo_text_extents_t   *extents)
{
    if (cr->status)
	return;
    
    cr->status = _cairo_gstate_text_extents (cr->gstate, utf8, extents);
}

void
cairo_glyph_extents (cairo_t                *cr,
		     cairo_glyph_t          *glyphs, 
		     int                    num_glyphs,
		     cairo_text_extents_t   *extents)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_glyph_extents (cr->gstate, glyphs, num_glyphs,
					      extents);
}

void
cairo_show_text (cairo_t *cr, const unsigned char *utf8)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_show_text (cr->gstate, utf8);
}

void
cairo_show_glyphs (cairo_t *cr, cairo_glyph_t *glyphs, int num_glyphs)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_show_glyphs (cr->gstate, glyphs, num_glyphs);
}

void
cairo_text_path  (cairo_t *cr, const unsigned char *utf8)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_text_path (cr->gstate, utf8);
}

void
cairo_glyph_path (cairo_t *cr, cairo_glyph_t *glyphs, int num_glyphs)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_glyph_path (cr->gstate, glyphs, num_glyphs);  
}

void
cairo_show_surface (cairo_t		*cr,
		    cairo_surface_t	*surface,
		    int			width,
		    int			height)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_show_surface (cr->gstate,
					     surface, width, height);
}

cairo_operator_t
cairo_current_operator (cairo_t *cr)
{
    return _cairo_gstate_current_operator (cr->gstate);
}
DEPRECATE (cairo_get_operator, cairo_current_operator);

void
cairo_current_rgb_color (cairo_t *cr, double *red, double *green, double *blue)
{
    _cairo_gstate_current_rgb_color (cr->gstate, red, green, blue);
}
DEPRECATE (cairo_get_rgb_color, cairo_current_rgb_color);

double
cairo_current_alpha (cairo_t *cr)
{
    return _cairo_gstate_current_alpha (cr->gstate);
}
DEPRECATE (cairo_get_alpha, cairo_current_alpha);

double
cairo_current_tolerance (cairo_t *cr)
{
    return _cairo_gstate_current_tolerance (cr->gstate);
}
DEPRECATE (cairo_get_tolerance, cairo_current_tolerance);

void
cairo_current_point (cairo_t *cr, double *x, double *y)
{
    _cairo_gstate_current_point (cr->gstate, x, y);
}
DEPRECATE (cairo_get_current_point, cairo_current_point);

cairo_fill_rule_t
cairo_current_fill_rule (cairo_t *cr)
{
    return _cairo_gstate_current_fill_rule (cr->gstate);
}
DEPRECATE (cairo_get_fill_rule, cairo_current_fill_rule);

double
cairo_current_line_width (cairo_t *cr)
{
    return _cairo_gstate_current_line_width (cr->gstate);
}
DEPRECATE (cairo_get_line_width, cairo_current_line_width);

cairo_line_cap_t
cairo_current_line_cap (cairo_t *cr)
{
    return _cairo_gstate_current_line_cap (cr->gstate);
}
DEPRECATE (cairo_get_line_cap, cairo_current_line_cap);

cairo_line_join_t
cairo_current_line_join (cairo_t *cr)
{
    return _cairo_gstate_current_line_join (cr->gstate);
}
DEPRECATE (cairo_get_line_join, cairo_current_line_join);

double
cairo_current_miter_limit (cairo_t *cr)
{
    return _cairo_gstate_current_miter_limit (cr->gstate);
}
DEPRECATE (cairo_get_miter_limit, cairo_current_miter_limit);

void
cairo_current_matrix (cairo_t *cr, cairo_matrix_t *matrix)
{
    _cairo_gstate_current_matrix (cr->gstate, matrix);
}
DEPRECATE (cairo_get_matrix, cairo_current_matrix);

cairo_surface_t *
cairo_current_target_surface (cairo_t *cr)
{
    return _cairo_gstate_current_target_surface (cr->gstate);
}
DEPRECATE (cairo_get_target_surface, cairo_current_target_surface);

void
cairo_current_path (cairo_t			*cr,
		    cairo_move_to_func_t	*move_to,
		    cairo_line_to_func_t	*line_to,
		    cairo_curve_to_func_t	*curve_to,
		    cairo_close_path_func_t	*close_path,
		    void			*closure)
{
    if (cr->status)
	return;
	
    cr->status = _cairo_gstate_interpret_path (cr->gstate,
					       move_to,
					       line_to,
					       curve_to,
					       close_path,
					       closure);
}

void
cairo_current_path_flat (cairo_t			*cr,
			 cairo_move_to_func_t		*move_to,
			 cairo_line_to_func_t		*line_to,
			 cairo_close_path_func_t	*close_path,
			 void				*closure)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_interpret_path (cr->gstate,
					       move_to,
					       line_to,
					       NULL,
					       close_path,
					       closure);
}

cairo_status_t
cairo_status (cairo_t *cr)
{
    return cr->status;
}
DEPRECATE (cairo_get_status, cairo_status);

const char *
cairo_status_string (cairo_t *cr)
{
    switch (cr->status) {
    case CAIRO_STATUS_SUCCESS:
	return "success";
    case CAIRO_STATUS_NO_MEMORY:
	return "out of memory";
    case CAIRO_STATUS_INVALID_RESTORE:
	return "cairo_restore without matching cairo_save";
    case CAIRO_STATUS_INVALID_POP_GROUP:
	return "cairo_pop_group without matching cairo_push_group";
    case CAIRO_STATUS_NO_CURRENT_POINT:
	return "no current point defined";
    case CAIRO_STATUS_INVALID_MATRIX:
	return "invalid matrix (not invertible)";
    case CAIRO_STATUS_NO_TARGET_SURFACE:
	return "no target surface has been set";
    case CAIRO_STATUS_NULL_POINTER:
	return "NULL pointer";
    }

    return "<unknown error status>";
}
DEPRECATE (cairo_get_status_string, cairo_status_string);

void
_cairo_restrict_value (double *value, double min, double max)
{
    if (*value < min)
	*value = min;
    else if (*value > max)
	*value = max;
}
