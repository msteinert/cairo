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

#include <stdlib.h>
#include <math.h>

#include "cairoint.h"

static cairo_status_t
_cairo_gstate_clip_and_composite_trapezoids (cairo_gstate_t *gstate,
					     cairo_pattern_t *src,
					     cairo_operator_t operator,
					     cairo_surface_t *dst,
					     cairo_traps_t *traps);

cairo_gstate_t *
_cairo_gstate_create ()
{
    cairo_gstate_t *gstate;

    gstate = malloc (sizeof (cairo_gstate_t));

    if (gstate)
	_cairo_gstate_init (gstate);

    return gstate;
}

void
_cairo_gstate_init (cairo_gstate_t *gstate)
{
    gstate->operator = CAIRO_GSTATE_OPERATOR_DEFAULT;

    gstate->tolerance = CAIRO_GSTATE_TOLERANCE_DEFAULT;

    gstate->line_width = CAIRO_GSTATE_LINE_WIDTH_DEFAULT;
    gstate->line_cap = CAIRO_GSTATE_LINE_CAP_DEFAULT;
    gstate->line_join = CAIRO_GSTATE_LINE_JOIN_DEFAULT;
    gstate->miter_limit = CAIRO_GSTATE_MITER_LIMIT_DEFAULT;

    gstate->fill_rule = CAIRO_GSTATE_FILL_RULE_DEFAULT;

    gstate->dash = NULL;
    gstate->num_dashes = 0;
    gstate->dash_offset = 0.0;

    gstate->font = _cairo_font_create (CAIRO_FONT_FAMILY_DEFAULT,
				       CAIRO_FONT_SLANT_DEFAULT,
				       CAIRO_FONT_WEIGHT_DEFAULT);

    gstate->surface = NULL;

    gstate->clip.region = NULL;
    gstate->clip.surface = NULL;
    
    gstate->pattern = _cairo_pattern_create_solid (1.0, 1.0, 1.0);
    gstate->pattern_offset.x = 0.0;
    gstate->pattern_offset.y = 0.0;
    gstate->alpha = 1.0;

    gstate->pixels_per_inch = CAIRO_GSTATE_PIXELS_PER_INCH_DEFAULT;
    _cairo_gstate_default_matrix (gstate);

    _cairo_path_init (&gstate->path);

    _cairo_pen_init_empty (&gstate->pen_regular);

    gstate->next = NULL;
}

cairo_status_t
_cairo_gstate_init_copy (cairo_gstate_t *gstate, cairo_gstate_t *other)
{
    cairo_status_t status;
    cairo_gstate_t *next;
    
    /* Copy all members, but don't smash the next pointer */
    next = gstate->next;
    *gstate = *other;
    gstate->next = next;

    /* Now fix up pointer data that needs to be cloned/referenced */
    if (other->dash) {
	gstate->dash = malloc (other->num_dashes * sizeof (double));
	if (gstate->dash == NULL)
	    return CAIRO_STATUS_NO_MEMORY;
	memcpy (gstate->dash, other->dash, other->num_dashes * sizeof (double));
    }

    if (other->font) {
	gstate->font = _cairo_font_copy (other->font);
	if (!gstate->font) {
	    status = CAIRO_STATUS_NO_MEMORY;
	    goto CLEANUP_DASHES;
	}
    }

    if (other->clip.region)
    {	
	gstate->clip.region = pixman_region_create ();
	pixman_region_copy (gstate->clip.region, other->clip.region);
    }

    cairo_surface_reference (gstate->surface);
    cairo_surface_reference (gstate->clip.surface);

    cairo_pattern_reference (gstate->pattern);
    
    status = _cairo_path_init_copy (&gstate->path, &other->path);
    if (status)
	goto CLEANUP_FONT;

    status = _cairo_pen_init_copy (&gstate->pen_regular, &other->pen_regular);
    if (status)
	goto CLEANUP_PATH;

    return status;

  CLEANUP_PATH:
    _cairo_path_fini (&gstate->path);
  CLEANUP_FONT:
    cairo_font_destroy (gstate->font);
  CLEANUP_DASHES:
    free (gstate->dash);
    gstate->dash = NULL;

    return status;
}

void
_cairo_gstate_fini (cairo_gstate_t *gstate)
{
    cairo_font_destroy (gstate->font);

    if (gstate->surface)
	cairo_surface_destroy (gstate->surface);
    gstate->surface = NULL;

    if (gstate->clip.surface)
	cairo_surface_destroy (gstate->clip.surface);
    gstate->clip.surface = NULL;

    if (gstate->clip.region)
	pixman_region_destroy (gstate->clip.region);
    gstate->clip.region = NULL;

    cairo_pattern_destroy (gstate->pattern);

    _cairo_matrix_fini (&gstate->ctm);
    _cairo_matrix_fini (&gstate->ctm_inverse);

    _cairo_path_fini (&gstate->path);

    _cairo_pen_fini (&gstate->pen_regular);

    if (gstate->dash) {
	free (gstate->dash);
	gstate->dash = NULL;
    }
}

void
_cairo_gstate_destroy (cairo_gstate_t *gstate)
{
    _cairo_gstate_fini (gstate);
    free (gstate);
}

cairo_gstate_t*
_cairo_gstate_clone (cairo_gstate_t *gstate)
{
    cairo_status_t status;
    cairo_gstate_t *clone;

    clone = malloc (sizeof (cairo_gstate_t));
    if (clone) {
	status = _cairo_gstate_init_copy (clone, gstate);
	if (status) {
	    free (clone);
	    return NULL;
	}
    }
    clone->next = NULL;

    return clone;
}

cairo_status_t
_cairo_gstate_copy (cairo_gstate_t *dest, cairo_gstate_t *src)
{
    cairo_status_t status;
    cairo_gstate_t *next;

    /* Preserve next pointer over fini/init */
    next = dest->next;
    _cairo_gstate_fini (dest);
    status = _cairo_gstate_init_copy (dest, src);
    dest->next = next;

    return status;
}

/* Push rendering off to an off-screen group. */
/* XXX: Rethinking this API
cairo_status_t
_cairo_gstate_begin_group (cairo_gstate_t *gstate)
{
    Pixmap pix;
    cairo_color_t clear;
    unsigned int width, height;

    gstate->parent_surface = gstate->surface;

    width = _cairo_surface_get_width (gstate->surface);
    height = _cairo_surface_get_height (gstate->surface);

    pix = XCreatePixmap (gstate->dpy,
			 _cairo_surface_get_drawable (gstate->surface),
			 width, height,
			 _cairo_surface_get_depth (gstate->surface));
    if (pix == 0)
	return CAIRO_STATUS_NO_MEMORY;

    gstate->surface = cairo_surface_create (gstate->dpy);
    if (gstate->surface == NULL)
	return CAIRO_STATUS_NO_MEMORY;

    _cairo_surface_set_drawableWH (gstate->surface, pix, width, height);

    _cairo_color_init (&clear);
    _cairo_color_set_alpha (&clear, 0);

    status = _cairo_surface_fill_rectangle (gstate->surface,
                                   CAIRO_OPERATOR_SRC,
				   &clear,
				   0, 0,
			           _cairo_surface_get_width (gstate->surface),
				   _cairo_surface_get_height (gstate->surface));
    if (status)				 
        return status;

    return CAIRO_STATUS_SUCCESS;
}
*/

/* Complete the current offscreen group, composing its contents onto the parent surface. */
/* XXX: Rethinking this API
cairo_status_t
_cairo_gstate_end_group (cairo_gstate_t *gstate)
{
    Pixmap pix;
    cairo_color_t mask_color;
    cairo_surface_t mask;

    if (gstate->parent_surface == NULL)
	return CAIRO_STATUS_INVALID_POP_GROUP;

    _cairo_surface_init (&mask, gstate->dpy);
    _cairo_color_init (&mask_color);
    _cairo_color_set_alpha (&mask_color, gstate->alpha);

    _cairo_surface_set_solid_color (&mask, &mask_color);

    * XXX: This could be made much more efficient by using
       _cairo_surface_get_damaged_width/Height if cairo_surface_t actually kept
       track of such informaton. *
    _cairo_surface_composite (gstate->operator,
			      gstate->surface,
			      mask,
			      gstate->parent_surface,
			      0, 0,
			      0, 0,
			      0, 0,
			      _cairo_surface_get_width (gstate->surface),
			      _cairo_surface_get_height (gstate->surface));

    _cairo_surface_fini (&mask);

    pix = _cairo_surface_get_drawable (gstate->surface);
    XFreePixmap (gstate->dpy, pix);

    cairo_surface_destroy (gstate->surface);
    gstate->surface = gstate->parent_surface;
    gstate->parent_surface = NULL;

    return CAIRO_STATUS_SUCCESS;
}
*/

cairo_status_t
_cairo_gstate_set_target_surface (cairo_gstate_t *gstate, cairo_surface_t *surface)
{
    double scale;

    if (gstate->surface)
	cairo_surface_destroy (gstate->surface);

    gstate->surface = surface;

    /* Sometimes the user wants to return to having no target surface,
     * (just like after cairo_create). This can be useful for forcing
     * the old surface to be destroyed. */
    if (surface == NULL)
	return CAIRO_STATUS_SUCCESS;

    cairo_surface_reference (gstate->surface);

    scale = _cairo_surface_pixels_per_inch (surface) / gstate->pixels_per_inch;
    _cairo_gstate_scale (gstate, scale, scale);
    gstate->pixels_per_inch = _cairo_surface_pixels_per_inch (surface);

    return CAIRO_STATUS_SUCCESS;
}

/* XXX: Need to decide the memory mangement semantics of this
   function. Should it reference the surface again? */
cairo_surface_t *
_cairo_gstate_current_target_surface (cairo_gstate_t *gstate)
{
    if (gstate == NULL)
	return NULL;

/* XXX: Do we want this?
    if (gstate->surface)
	_cairo_surface_reference (gstate->surface);
*/

    return gstate->surface;
}

cairo_status_t
_cairo_gstate_set_pattern (cairo_gstate_t *gstate, cairo_pattern_t *pattern)
{
    if (pattern == NULL)
	return CAIRO_STATUS_NULL_POINTER;

    if (gstate->pattern)
	cairo_pattern_destroy (gstate->pattern);
    
    gstate->pattern = pattern;
    cairo_pattern_reference (pattern);
    
    return CAIRO_STATUS_SUCCESS;
}

cairo_pattern_t *
_cairo_gstate_current_pattern (cairo_gstate_t *gstate)
{
    if (gstate == NULL)
	return NULL;

/* XXX: Do we want this?    
   cairo_pattern_reference (gstate->pattern);
*/

    return gstate->pattern;
}

cairo_status_t
_cairo_gstate_set_operator (cairo_gstate_t *gstate, cairo_operator_t operator)
{
    gstate->operator = operator;

    return CAIRO_STATUS_SUCCESS;
}

cairo_operator_t
_cairo_gstate_current_operator (cairo_gstate_t *gstate)
{
    return gstate->operator;
}

cairo_status_t
_cairo_gstate_set_rgb_color (cairo_gstate_t *gstate, double red, double green, double blue)
{
    cairo_pattern_destroy (gstate->pattern);
    
    gstate->pattern = _cairo_pattern_create_solid (red, green, blue);
    gstate->pattern_offset.x = 0.0;
    gstate->pattern_offset.y = 0.0;
    
    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_current_rgb_color (cairo_gstate_t *gstate, double *red, double *green, double *blue)
{
    return _cairo_pattern_get_rgb (gstate->pattern, red, green, blue);
}

cairo_status_t
_cairo_gstate_set_tolerance (cairo_gstate_t *gstate, double tolerance)
{
    gstate->tolerance = tolerance;

    return CAIRO_STATUS_SUCCESS;
}

double
_cairo_gstate_current_tolerance (cairo_gstate_t *gstate)
{
    return gstate->tolerance;
}

cairo_status_t
_cairo_gstate_set_alpha (cairo_gstate_t *gstate, double alpha)
{
    gstate->alpha = alpha;

    return CAIRO_STATUS_SUCCESS;
}

double
_cairo_gstate_current_alpha (cairo_gstate_t *gstate)
{
    return gstate->alpha;
}

cairo_status_t
_cairo_gstate_set_fill_rule (cairo_gstate_t *gstate, cairo_fill_rule_t fill_rule)
{
    gstate->fill_rule = fill_rule;

    return CAIRO_STATUS_SUCCESS;
}

cairo_fill_rule_t
_cairo_gstate_current_fill_rule (cairo_gstate_t *gstate)
{
    return gstate->fill_rule;
}

cairo_status_t
_cairo_gstate_set_line_width (cairo_gstate_t *gstate, double width)
{
    gstate->line_width = width;

    return CAIRO_STATUS_SUCCESS;
}

double
_cairo_gstate_current_line_width (cairo_gstate_t *gstate)
{
    return gstate->line_width;
}

cairo_status_t
_cairo_gstate_set_line_cap (cairo_gstate_t *gstate, cairo_line_cap_t line_cap)
{
    gstate->line_cap = line_cap;

    return CAIRO_STATUS_SUCCESS;
}

cairo_line_cap_t
_cairo_gstate_current_line_cap (cairo_gstate_t *gstate)
{
    return gstate->line_cap;
}

cairo_status_t
_cairo_gstate_set_line_join (cairo_gstate_t *gstate, cairo_line_join_t line_join)
{
    gstate->line_join = line_join;

    return CAIRO_STATUS_SUCCESS;
}

cairo_line_join_t
_cairo_gstate_current_line_join (cairo_gstate_t *gstate)
{
    return gstate->line_join;
}

cairo_status_t
_cairo_gstate_set_dash (cairo_gstate_t *gstate, double *dash, int num_dashes, double offset)
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
	    return CAIRO_STATUS_NO_MEMORY;
	}
    }

    memcpy (gstate->dash, dash, gstate->num_dashes * sizeof (double));
    gstate->dash_offset = offset;

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_set_miter_limit (cairo_gstate_t *gstate, double limit)
{
    gstate->miter_limit = limit;

    return CAIRO_STATUS_SUCCESS;
}

double
_cairo_gstate_current_miter_limit (cairo_gstate_t *gstate)
{
    return gstate->miter_limit;
}

void
_cairo_gstate_current_matrix (cairo_gstate_t *gstate, cairo_matrix_t *matrix)
{
    cairo_matrix_copy (matrix, &gstate->ctm);
}

cairo_status_t
_cairo_gstate_translate (cairo_gstate_t *gstate, double tx, double ty)
{
    cairo_matrix_t tmp;

    _cairo_matrix_set_translate (&tmp, tx, ty);
    cairo_matrix_multiply (&gstate->ctm, &tmp, &gstate->ctm);

    _cairo_matrix_set_translate (&tmp, -tx, -ty);
    cairo_matrix_multiply (&gstate->ctm_inverse, &gstate->ctm_inverse, &tmp);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_scale (cairo_gstate_t *gstate, double sx, double sy)
{
    cairo_matrix_t tmp;

    if (sx == 0 || sy == 0)
	return CAIRO_STATUS_INVALID_MATRIX;

    _cairo_matrix_set_scale (&tmp, sx, sy);
    cairo_matrix_multiply (&gstate->ctm, &tmp, &gstate->ctm);

    _cairo_matrix_set_scale (&tmp, 1/sx, 1/sy);
    cairo_matrix_multiply (&gstate->ctm_inverse, &gstate->ctm_inverse, &tmp);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_rotate (cairo_gstate_t *gstate, double angle)
{
    cairo_matrix_t tmp;

    _cairo_matrix_set_rotate (&tmp, angle);
    cairo_matrix_multiply (&gstate->ctm, &tmp, &gstate->ctm);

    _cairo_matrix_set_rotate (&tmp, -angle);
    cairo_matrix_multiply (&gstate->ctm_inverse, &gstate->ctm_inverse, &tmp);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_concat_matrix (cairo_gstate_t *gstate,
		      cairo_matrix_t *matrix)
{
    cairo_matrix_t tmp;

    cairo_matrix_copy (&tmp, matrix);
    cairo_matrix_multiply (&gstate->ctm, &tmp, &gstate->ctm);

    cairo_matrix_invert (&tmp);
    cairo_matrix_multiply (&gstate->ctm_inverse, &gstate->ctm_inverse, &tmp);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_set_matrix (cairo_gstate_t *gstate,
		   cairo_matrix_t *matrix)
{
    cairo_status_t status;

    cairo_matrix_copy (&gstate->ctm, matrix);

    cairo_matrix_copy (&gstate->ctm_inverse, matrix);
    status = cairo_matrix_invert (&gstate->ctm_inverse);
    if (status)
	return status;

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_default_matrix (cairo_gstate_t *gstate)
{
    int scale = gstate->pixels_per_inch / CAIRO_GSTATE_PIXELS_PER_INCH_DEFAULT + 0.5;
    if (scale == 0)
	scale = 1;

    cairo_matrix_set_identity (&gstate->ctm);
    cairo_matrix_scale (&gstate->ctm, scale, scale);
    cairo_matrix_copy (&gstate->ctm_inverse, &gstate->ctm);
    cairo_matrix_invert (&gstate->ctm_inverse);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_identity_matrix (cairo_gstate_t *gstate)
{
    cairo_matrix_set_identity (&gstate->ctm);
    cairo_matrix_set_identity (&gstate->ctm_inverse);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_transform_point (cairo_gstate_t *gstate, double *x, double *y)
{
    cairo_matrix_transform_point (&gstate->ctm, x, y);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_transform_distance (cairo_gstate_t *gstate, double *dx, double *dy)
{
    cairo_matrix_transform_distance (&gstate->ctm, dx, dy);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_inverse_transform_point (cairo_gstate_t *gstate, double *x, double *y)
{
    cairo_matrix_transform_point (&gstate->ctm_inverse, x, y);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_inverse_transform_distance (cairo_gstate_t *gstate, double *dx, double *dy)
{
    cairo_matrix_transform_distance (&gstate->ctm_inverse, dx, dy);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_new_path (cairo_gstate_t *gstate)
{
    _cairo_path_fini (&gstate->path);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_move_to (cairo_gstate_t *gstate, double x, double y)
{
    cairo_point_t point;

    cairo_matrix_transform_point (&gstate->ctm, &x, &y);

    point.x = _cairo_fixed_from_double (x);
    point.y = _cairo_fixed_from_double (y);

    return _cairo_path_move_to (&gstate->path, &point);
}

cairo_status_t
_cairo_gstate_line_to (cairo_gstate_t *gstate, double x, double y)
{
    cairo_point_t point;

    cairo_matrix_transform_point (&gstate->ctm, &x, &y);

    point.x = _cairo_fixed_from_double (x);
    point.y = _cairo_fixed_from_double (y);

    return _cairo_path_line_to (&gstate->path, &point);
}

cairo_status_t
_cairo_gstate_curve_to (cairo_gstate_t *gstate,
			double x0, double y0,
			double x1, double y1,
			double x2, double y2)
{
    cairo_point_t p0, p1, p2;

    cairo_matrix_transform_point (&gstate->ctm, &x0, &y0);
    cairo_matrix_transform_point (&gstate->ctm, &x1, &y1);
    cairo_matrix_transform_point (&gstate->ctm, &x2, &y2);

    p0.x = _cairo_fixed_from_double (x0);
    p0.y = _cairo_fixed_from_double (y0);

    p1.x = _cairo_fixed_from_double (x1);
    p1.y = _cairo_fixed_from_double (y1);

    p2.x = _cairo_fixed_from_double (x2);
    p2.y = _cairo_fixed_from_double (y2);

    return _cairo_path_curve_to (&gstate->path, &p0, &p1, &p2);
}

/* Spline deviation from the circle in radius would be given by:

	error = sqrt (x**2 + y**2) - 1

   A simpler error function to work with is:

	e = x**2 + y**2 - 1

   From "Good approximation of circles by curvature-continuous Bezier
   curves", Tor Dokken and Morten Daehlen, Computer Aided Geometric
   Design 8 (1990) 22-41, we learn:

	abs (max(e)) = 4/27 * sin**6(angle/4) / cos**2(angle/4)

   and
	abs (error) =~ 1/2 * e

   Of course, this error value applies only for the particular spline
   approximation that is used in _cairo_gstate_arc_segment.
*/
static double
_arc_error_normalized (double angle)
{
    return 2.0/27.0 * pow (sin (angle / 4), 6) / pow (cos (angle / 4), 2);
}

static double
_arc_max_angle_for_tolerance_normalized (double tolerance)
{
    double angle, error;
    int i;

    /* Use table lookup to reduce search time in most cases. */
    struct {
	double angle;
	double error;
    } table[] = {
	{ M_PI / 1.0,   0.0185185185185185036127 },
	{ M_PI / 2.0,   0.000272567143730179811158 },
	{ M_PI / 3.0,   2.38647043651461047433e-05 },
	{ M_PI / 4.0,   4.2455377443222443279e-06 },
	{ M_PI / 5.0,   1.11281001494389081528e-06 },
	{ M_PI / 6.0,   3.72662000942734705475e-07 },
	{ M_PI / 7.0,   1.47783685574284411325e-07 },
	{ M_PI / 8.0,   6.63240432022601149057e-08 },
	{ M_PI / 9.0,   3.2715520137536980553e-08 },
	{ M_PI / 10.0,  1.73863223499021216974e-08 },
	{ M_PI / 11.0,  9.81410988043554039085e-09 },
    };
    int table_size = (sizeof (table) / sizeof (table[0]));

    for (i = 0; i < table_size; i++)
	if (table[i].error < tolerance)
	    return table[i].angle;

    ++i;
    do {
	angle = M_PI / i++;
	error = _arc_error_normalized (angle);
    } while (error > tolerance);

    return angle;
}

static int
_cairo_gstate_arc_segments_needed (cairo_gstate_t *gstate,
				   double angle,
				   double radius)
{
    double l1, l2, lmax;
    double max_angle;

    _cairo_matrix_compute_eigen_values (&gstate->ctm, &l1, &l2);

    l1 = fabs (l1);
    l2 = fabs (l2);
    if (l1 > l2)
	lmax = l1;
    else
	lmax = l2;

    max_angle = _arc_max_angle_for_tolerance_normalized (gstate->tolerance / (radius * lmax));

    return (int) ceil (angle / max_angle);
}

/* We want to draw a single spline approximating a circular arc radius
   R from angle A to angle B. Since we want a symmetric spline that
   matches the endpoints of the arc in position and slope, we know
   that the spline control points must be:

	(R * cos(A), R * sin(A))
	(R * cos(A) - h * sin(A), R * sin(A) + h * cos (A))
	(R * cos(B) + h * sin(B), R * sin(B) - h * cos (B))
	(R * cos(B), R * sin(B))

   for some value of h.

   "Approximation of circular arcs by cubic poynomials", Michael
   Goldapp, Computer Aided Geometric Design 8 (1991) 227-238, provides
   various values of h along with error analysis for each.

   From that paper, a very practical value of h is:

	h = 4/3 * tan(angle/4)

   This value does not give the spline with minimal error, but it does
   provide a very good approximation, (6th-order convergence), and the
   error expression is quite simple, (see the comment for
   _arc_error_normalized).
*/
static cairo_status_t
_cairo_gstate_arc_segment (cairo_gstate_t *gstate,
			   double xc, double yc,
			   double radius,
			   double angle_A, double angle_B)
{
    cairo_status_t status;
    double r_sin_A, r_cos_A;
    double r_sin_B, r_cos_B;
    double h;

    r_sin_A = radius * sin (angle_A);
    r_cos_A = radius * cos (angle_A);
    r_sin_B = radius * sin (angle_B);
    r_cos_B = radius * cos (angle_B);

    h = 4.0/3.0 * tan ((angle_B - angle_A) / 4.0);

    status = _cairo_gstate_curve_to (gstate,
				     xc + r_cos_A - h * r_sin_A, yc + r_sin_A + h * r_cos_A,
				     xc + r_cos_B + h * r_sin_B, yc + r_sin_B - h * r_cos_B,
				     xc + r_cos_B, yc + r_sin_B);
    if (status)
	return status;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_gstate_arc_dir (cairo_gstate_t *gstate,
		       double xc, double yc,
		       double radius,
		       double angle_min,
		       double angle_max,
		       cairo_direction_t dir)
{
    cairo_status_t status;

    while (angle_max - angle_min > 4 * M_PI)
	angle_max -= 2 * M_PI;

    /* Recurse if drawing arc larger than pi */
    if (angle_max - angle_min > M_PI) {
	/* XXX: Something tells me this block could be condensed. */
	if (dir == CAIRO_DIRECTION_FORWARD) {
	    status = _cairo_gstate_arc_dir (gstate, xc, yc, radius,
					    angle_min, angle_min + M_PI, dir);
	    if (status)
		return status;
	    
	    status = _cairo_gstate_arc_dir (gstate, xc, yc, radius,
					    angle_min + M_PI, angle_max, dir);
	    if (status)
		return status;
	} else {
	    status = _cairo_gstate_arc_dir (gstate, xc, yc, radius,
					    angle_min + M_PI, angle_max, dir);
	    if (status)
		return status;

	    status = _cairo_gstate_arc_dir (gstate, xc, yc, radius,
					    angle_min, angle_min + M_PI, dir);
	    if (status)
		return status;
	}
    } else {
	int i, segments;
	double angle, angle_step;

	segments = _cairo_gstate_arc_segments_needed (gstate,
						      angle_max - angle_min,
						      radius);
	angle_step = (angle_max - angle_min) / (double) segments;

	if (dir == CAIRO_DIRECTION_FORWARD) {
	    angle = angle_min;
	} else {
	    angle = angle_max;
	    angle_step = - angle_step;
	}

	for (i = 0; i < segments; i++, angle += angle_step) {
	    _cairo_gstate_arc_segment (gstate,
				       xc, yc,
				       radius,
				       angle,
				       angle + angle_step);
	}
	
    }

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_arc (cairo_gstate_t *gstate,
		   double xc, double yc,
		   double radius,
		   double angle1, double angle2)
{
    cairo_status_t status;

    if (radius <= 0.0)
	return CAIRO_STATUS_SUCCESS;

    while (angle2 < angle1)
	angle2 += 2 * M_PI;

    status = _cairo_gstate_line_to (gstate,
				    xc + radius * cos (angle1),
				    yc + radius * sin (angle1));
    if (status)
	return status;

    status = _cairo_gstate_arc_dir (gstate, xc, yc, radius,
				    angle1, angle2, CAIRO_DIRECTION_FORWARD);
    if (status)
	return status;

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_arc_negative (cairo_gstate_t *gstate,
			    double xc, double yc,
			    double radius,
			    double angle1, double angle2)
{
    cairo_status_t status;

    if (radius <= 0.0)
	return CAIRO_STATUS_SUCCESS;

    while (angle2 > angle1)
	angle2 -= 2 * M_PI;

    status = _cairo_gstate_line_to (gstate,
				    xc + radius * cos (angle1),
				    yc + radius * sin (angle1));
    if (status)
	return status;

    status = _cairo_gstate_arc_dir (gstate, xc, yc, radius,
				    angle2, angle1, CAIRO_DIRECTION_REVERSE);
    if (status)
	return status;

    return CAIRO_STATUS_SUCCESS;
}

/* XXX: NYI
cairo_status_t
_cairo_gstate_arc_to (cairo_gstate_t *gstate,
		      double x1, double y1,
		      double x2, double y2,
		      double radius)
{

}
*/

cairo_status_t
_cairo_gstate_rel_move_to (cairo_gstate_t *gstate, double dx, double dy)
{
    cairo_distance_t distance;

    cairo_matrix_transform_distance (&gstate->ctm, &dx, &dy);

    distance.dx = _cairo_fixed_from_double (dx);
    distance.dy = _cairo_fixed_from_double (dy);

    return _cairo_path_rel_move_to (&gstate->path, &distance);
}

cairo_status_t
_cairo_gstate_rel_line_to (cairo_gstate_t *gstate, double dx, double dy)
{
    cairo_distance_t distance;

    cairo_matrix_transform_distance (&gstate->ctm, &dx, &dy);

    distance.dx = _cairo_fixed_from_double (dx);
    distance.dy = _cairo_fixed_from_double (dy);

    return _cairo_path_rel_line_to (&gstate->path, &distance);
}

cairo_status_t
_cairo_gstate_rel_curve_to (cairo_gstate_t *gstate,
			    double dx0, double dy0,
			    double dx1, double dy1,
			    double dx2, double dy2)
{
    cairo_distance_t distance[3];

    cairo_matrix_transform_distance (&gstate->ctm, &dx0, &dy0);
    cairo_matrix_transform_distance (&gstate->ctm, &dx1, &dy1);
    cairo_matrix_transform_distance (&gstate->ctm, &dx2, &dy2);

    distance[0].dx = _cairo_fixed_from_double (dx0);
    distance[0].dy = _cairo_fixed_from_double (dy0);

    distance[1].dx = _cairo_fixed_from_double (dx1);
    distance[1].dy = _cairo_fixed_from_double (dy1);

    distance[2].dx = _cairo_fixed_from_double (dx2);
    distance[2].dy = _cairo_fixed_from_double (dy2);

    return _cairo_path_rel_curve_to (&gstate->path,
				     &distance[0],
				     &distance[1],
				     &distance[2]);
}

/* XXX: NYI 
cairo_status_t
_cairo_gstate_stroke_path (cairo_gstate_t *gstate)
{
    cairo_status_t status;

    _cairo_pen_init (&gstate);
    return CAIRO_STATUS_SUCCESS;
}
*/

cairo_status_t
_cairo_gstate_close_path (cairo_gstate_t *gstate)
{
    return _cairo_path_close_path (&gstate->path);
}

cairo_status_t
_cairo_gstate_current_point (cairo_gstate_t *gstate, double *x_ret, double *y_ret)
{
    cairo_status_t status;
    cairo_point_t point;
    double x, y;

    status = _cairo_path_current_point (&gstate->path, &point);
    if (status == CAIRO_STATUS_NO_CURRENT_POINT) {
	x = 0.0;
	y = 0.0;
    } else {
	x = _cairo_fixed_to_double (point.x);
	y = _cairo_fixed_to_double (point.y);
	cairo_matrix_transform_point (&gstate->ctm_inverse, &x, &y);
    }

    *x_ret = x;
    *y_ret = y;

    return CAIRO_STATUS_SUCCESS;
}

typedef struct gstate_path_interpreter {
    cairo_matrix_t		ctm_inverse;
    double			tolerance;
    cairo_point_t		current_point;

    cairo_move_to_func_t	*move_to;
    cairo_line_to_func_t	*line_to;
    cairo_curve_to_func_t	*curve_to;
    cairo_close_path_func_t	*close_path;

    void			*closure;
} gpi_t;

static cairo_status_t
_gpi_move_to (void *closure, cairo_point_t *point)
{
    gpi_t *gpi = closure;
    double x, y;

    x = _cairo_fixed_to_double (point->x);
    y = _cairo_fixed_to_double (point->y);

    cairo_matrix_transform_point (&gpi->ctm_inverse, &x, &y);

    gpi->move_to (gpi->closure, x, y);
    gpi->current_point = *point;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_gpi_line_to (void *closure, cairo_point_t *point)
{
    gpi_t *gpi = closure;
    double x, y;

    x = _cairo_fixed_to_double (point->x);
    y = _cairo_fixed_to_double (point->y);

    cairo_matrix_transform_point (&gpi->ctm_inverse, &x, &y);

    gpi->line_to (gpi->closure, x, y);
    gpi->current_point = *point;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_gpi_curve_to (void *closure,
	       cairo_point_t *p1,
	       cairo_point_t *p2,
	       cairo_point_t *p3)
{
    gpi_t *gpi = closure;
    cairo_status_t status;
    cairo_spline_t spline;
    double x1, y1, x2, y2, x3, y3;

    if (gpi->curve_to) {
	x1 = _cairo_fixed_to_double (p1->x);
	y1 = _cairo_fixed_to_double (p1->y);
	cairo_matrix_transform_point (&gpi->ctm_inverse, &x1, &y1);

	x2 = _cairo_fixed_to_double (p2->x);
	y2 = _cairo_fixed_to_double (p2->y);
	cairo_matrix_transform_point (&gpi->ctm_inverse, &x2, &y2);

	x3 = _cairo_fixed_to_double (p3->x);
	y3 = _cairo_fixed_to_double (p3->y);
	cairo_matrix_transform_point (&gpi->ctm_inverse, &x3, &y3);

	gpi->curve_to (gpi->closure, x1, y1, x2, y2, x3, y3);
    } else {
	cairo_point_t *p0 = &gpi->current_point;
	int i;
	double x, y;

	status = _cairo_spline_init (&spline, p0, p1, p2, p3);
	if (status == CAIRO_INT_STATUS_DEGENERATE)
	    return CAIRO_STATUS_SUCCESS;

	status = _cairo_spline_decompose (&spline, gpi->tolerance);
	if (status)
	    return status;

	for (i=1; i < spline.num_points; i++) {
	    x = _cairo_fixed_to_double (spline.points[i].x);
	    y = _cairo_fixed_to_double (spline.points[i].y);

	    cairo_matrix_transform_point (&gpi->ctm_inverse, &x, &y);

	    gpi->line_to (gpi->closure, x, y);
	}
    }

    gpi->current_point = *p3;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_gpi_close_path (void *closure)
{
    gpi_t *gpi = closure;

    gpi->close_path (gpi->closure);

    gpi->current_point.x = 0;
    gpi->current_point.y = 0;

    return CAIRO_STATUS_SUCCESS;
}

/* It's OK for curve_path to be NULL. In that case, all curves in the
   path will be decomposed into one or more calls to the line_to
   function, (according to the current tolerance). */
cairo_status_t
_cairo_gstate_interpret_path (cairo_gstate_t		*gstate,
			      cairo_move_to_func_t	*move_to,
			      cairo_line_to_func_t	*line_to,
			      cairo_curve_to_func_t	*curve_to,
			      cairo_close_path_func_t	*close_path,
			      void			*closure)
{
    cairo_path_t path;
    gpi_t gpi;

    /* Anything we want from gstate must be copied. We must not retain
       pointers into gstate. */
    _cairo_path_init_copy (&path, &gstate->path);

    cairo_matrix_copy (&gpi.ctm_inverse, &gstate->ctm_inverse);
    gpi.tolerance = gstate->tolerance;

    gpi.move_to = move_to;
    gpi.line_to = line_to;
    gpi.curve_to = curve_to;
    gpi.close_path = close_path;
    gpi.closure = closure;

    gpi.current_point.x = 0;
    gpi.current_point.y = 0;

    return _cairo_path_interpret (&path,
				  CAIRO_DIRECTION_FORWARD,
				  _gpi_move_to,
				  _gpi_line_to,
				  _gpi_curve_to,
				  _gpi_close_path,
				  &gpi);
}

/* This function modifies the pattern and the state of the pattern surface it
   may contain. The pattern surface will be restored to its orignal state
   when the pattern is destroyed. The appropriate way is to pass a copy of
   the original pattern to this function just before the pattern should be
   used and destroy the copy when done. */
static cairo_status_t
_cairo_gstate_create_pattern (cairo_gstate_t *gstate,
			      cairo_pattern_t *pattern,
			      cairo_box_t *extents)
{
    cairo_int_status_t status;
  
    if (gstate->surface == NULL) {
	_cairo_pattern_fini (pattern);
	return CAIRO_STATUS_NO_TARGET_SURFACE;
    }

    if (pattern->type == CAIRO_PATTERN_LINEAR ||
	pattern->type == CAIRO_PATTERN_RADIAL) {
	if (pattern->n_stops < 2) {
	    pattern->type = CAIRO_PATTERN_SOLID;
      
	    if (pattern->n_stops)
		pattern->color = pattern->stops->color;
	}
    }
  
    _cairo_pattern_set_alpha (pattern, gstate->alpha);
    _cairo_pattern_transform (pattern, &gstate->ctm_inverse);

    _cairo_pattern_set_source_offset (pattern,
				      gstate->pattern_offset.x,
				      gstate->pattern_offset.y);

    status = _cairo_surface_create_pattern (gstate->surface, pattern, extents);
    if (status) {
	_cairo_pattern_fini (pattern);
	return status;
    }

    if (pattern->type == CAIRO_PATTERN_SURFACE)
	_cairo_pattern_prepare_surface (pattern);
    
    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_stroke (cairo_gstate_t *gstate)
{
    cairo_status_t status;
    cairo_traps_t traps;

    if (gstate->line_width <= 0.0)
	return CAIRO_STATUS_SUCCESS;

    _cairo_pen_init (&gstate->pen_regular, gstate->line_width / 2.0, gstate);

    _cairo_traps_init (&traps);

    status = _cairo_path_stroke_to_traps (&gstate->path, gstate, &traps);
    if (status) {
	_cairo_traps_fini (&traps);
	return status;
    }

    _cairo_gstate_clip_and_composite_trapezoids (gstate,
                                                 gstate->pattern,
                                                 gstate->operator,
                                                 gstate->surface,
                                                 &traps);

    _cairo_traps_fini (&traps);

    _cairo_gstate_new_path (gstate);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_in_stroke (cairo_gstate_t	*gstate,
			 double		x,
			 double		y,
			 int		*inside_ret)
{
    cairo_status_t status = CAIRO_STATUS_SUCCESS;
    cairo_traps_t traps;

    cairo_matrix_transform_point (&gstate->ctm, &x, &y);

    _cairo_pen_init (&gstate->pen_regular, gstate->line_width / 2.0, gstate);

    _cairo_traps_init (&traps);

    status = _cairo_path_stroke_to_traps (&gstate->path, gstate, &traps);
    if (status)
	goto BAIL;

    *inside_ret = _cairo_traps_contain (&traps, x, y);

BAIL:
    _cairo_traps_fini (&traps);

    return status;
}

/* Warning: This call modifies the coordinates of traps */
static cairo_status_t
_cairo_gstate_clip_and_composite_trapezoids (cairo_gstate_t *gstate,
					     cairo_pattern_t *src,
					     cairo_operator_t operator,
					     cairo_surface_t *dst,
					     cairo_traps_t *traps)
{
    cairo_status_t status;
    cairo_pattern_t pattern;
    cairo_box_t extents;

    if (traps->num_traps == 0)
	return CAIRO_STATUS_SUCCESS;

    if (gstate->clip.surface) {
	cairo_fixed_t xoff, yoff;
	cairo_trapezoid_t *t;
	int i;
	cairo_surface_t *intermediate;
	cairo_color_t empty_color;

	_cairo_color_init (&empty_color);
	_cairo_color_set_alpha (&empty_color, 0.);
	intermediate = _cairo_surface_create_similar_solid (gstate->clip.surface,
							    CAIRO_FORMAT_A8,
							    gstate->clip.width,
							    gstate->clip.height,
                                &empty_color);    
	if (intermediate == NULL) {
	    status = CAIRO_STATUS_NO_MEMORY;
	    goto BAIL0;
	}

	/* Ugh. The cairo_composite/(Render) interface doesn't allow
           an offset for the trapezoids. Need to manually shift all
           the coordinates to align with the offset origin of the clip
           surface. */
	xoff = _cairo_fixed_from_double (gstate->clip.x);
	yoff = _cairo_fixed_from_double (gstate->clip.y);
	for (i=0, t=traps->traps; i < traps->num_traps; i++, t++) {
	    t->top -= yoff;
	    t->bottom -= yoff;
	    t->left.p1.x -= xoff;
	    t->left.p1.y -= yoff;
	    t->left.p2.x -= xoff;
	    t->left.p2.y -= yoff;
	    t->right.p1.x -= xoff;
	    t->right.p1.y -= yoff;
	    t->right.p2.x -= xoff;
	    t->right.p2.y -= yoff;
	}

	_cairo_pattern_init_solid (&pattern, 1.0, 1.0, 1.0);
	_cairo_pattern_set_alpha (&pattern, 1.0);

	_cairo_traps_extents (traps, &extents);
	status = _cairo_gstate_create_pattern (gstate, &pattern, &extents);
	if (status)
	    goto BAIL1;

	status = _cairo_surface_composite_trapezoids (CAIRO_OPERATOR_ADD,
						      pattern.source, intermediate,
						      0, 0,
						      traps->traps,
						      traps->num_traps);
	if (status)
	    goto BAIL2;

	status = _cairo_surface_composite (CAIRO_OPERATOR_IN,
					   gstate->clip.surface,
					   NULL,
					   intermediate,
					   0, 0, 0, 0, 0, 0,
					   gstate->clip.width, gstate->clip.height);
	if (status)
	    goto BAIL2;
    
	_cairo_pattern_fini (&pattern);
    
	_cairo_pattern_init_copy (&pattern, src);
    
	extents.p1.x = _cairo_fixed_from_int (gstate->clip.x);
	extents.p1.y = _cairo_fixed_from_int (gstate->clip.y);
	extents.p2.x =
	    _cairo_fixed_from_int (gstate->clip.x + gstate->clip.width);
	extents.p2.y =
	    _cairo_fixed_from_int (gstate->clip.y + gstate->clip.height);
	status = _cairo_gstate_create_pattern (gstate, &pattern, &extents);
	if (status)
	    goto BAIL2;

	status = _cairo_surface_composite (operator,
					   pattern.source, intermediate, dst,
					   0, 0,
					   0, 0,
					   gstate->clip.x,
					   gstate->clip.y,
					   gstate->clip.width,
					   gstate->clip.height);
	
    BAIL2:
	cairo_surface_destroy (intermediate);
    BAIL1:
	_cairo_pattern_fini (&pattern);
    BAIL0:

	if (status)
	    return status;
	
    } else {
	int xoff, yoff;

	if (traps->traps[0].left.p1.y < traps->traps[0].left.p2.y) {
	    xoff = _cairo_fixed_to_double (traps->traps[0].left.p1.x);
	    yoff = _cairo_fixed_to_double (traps->traps[0].left.p1.y);
	} else {
	    xoff = _cairo_fixed_to_double (traps->traps[0].left.p2.x);
	    yoff = _cairo_fixed_to_double (traps->traps[0].left.p2.y);
	}

	_cairo_pattern_init_copy (&pattern, src);
	
	_cairo_traps_extents (traps, &extents);
	status = _cairo_gstate_create_pattern (gstate, &pattern, &extents);
	if (status)
	    return status;

	status = _cairo_surface_composite_trapezoids (gstate->operator,
						      pattern.source, dst,
						      xoff - pattern.source_offset.x,
						      yoff - pattern.source_offset.y,
						      traps->traps,
						      traps->num_traps);

	_cairo_pattern_fini (&pattern);
    
	if (status)
	    return status;
    }
    
    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_fill (cairo_gstate_t *gstate)
{
    cairo_status_t status;
    cairo_traps_t traps;

    _cairo_traps_init (&traps);

    status = _cairo_path_fill_to_traps (&gstate->path, gstate, &traps);
    if (status) {
	_cairo_traps_fini (&traps);
	return status;
    }

    _cairo_gstate_clip_and_composite_trapezoids (gstate,
                                                 gstate->pattern,
                                                 gstate->operator,
                                                 gstate->surface,
                                                 &traps);

    _cairo_traps_fini (&traps);

    _cairo_gstate_new_path (gstate);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_in_fill (cairo_gstate_t	*gstate,
		       double		x,
		       double		y,
		       int		*inside_ret)
{
    cairo_status_t status = CAIRO_STATUS_SUCCESS;
    cairo_traps_t traps;

    cairo_matrix_transform_point (&gstate->ctm, &x, &y);

    _cairo_traps_init (&traps);

    status = _cairo_path_fill_to_traps (&gstate->path, gstate, &traps);
    if (status)
	goto BAIL;

    *inside_ret = _cairo_traps_contain (&traps, x, y);
    
BAIL:
    _cairo_traps_fini (&traps);

    return status;
}

cairo_status_t
_cairo_gstate_copy_page (cairo_gstate_t *gstate)
{
    if (gstate->surface == NULL)
	return CAIRO_STATUS_NO_TARGET_SURFACE;

    return _cairo_surface_copy_page (gstate->surface);
}

cairo_status_t
_cairo_gstate_show_page (cairo_gstate_t *gstate)
{
    if (gstate->surface == NULL)
	return CAIRO_STATUS_NO_TARGET_SURFACE;

    return _cairo_surface_show_page (gstate->surface);
}

cairo_status_t
_cairo_gstate_stroke_extents (cairo_gstate_t *gstate,
                              double *x1, double *y1,
			      double *x2, double *y2)
{
    cairo_status_t status;
    cairo_traps_t traps;
    cairo_box_t extents;
  
    _cairo_traps_init (&traps);
  
    status = _cairo_path_stroke_to_traps (&gstate->path, gstate, &traps);
    if (status)
	goto BAIL;

    _cairo_traps_extents (&traps, &extents);

    *x1 = _cairo_fixed_to_double (extents.p1.x);
    *y1 = _cairo_fixed_to_double (extents.p1.y);
    *x2 = _cairo_fixed_to_double (extents.p2.x);
    *y2 = _cairo_fixed_to_double (extents.p2.y);

    cairo_matrix_transform_point (&gstate->ctm_inverse, x1, y1);
    cairo_matrix_transform_point (&gstate->ctm_inverse, x2, y2);
  
BAIL:
    _cairo_traps_fini (&traps);
  
    return status;
}

cairo_status_t
_cairo_gstate_fill_extents (cairo_gstate_t *gstate,
                            double *x1, double *y1,
			    double *x2, double *y2)
{
    cairo_status_t status;
    cairo_traps_t traps;
    cairo_box_t extents;
  
    _cairo_traps_init (&traps);
  
    status = _cairo_path_fill_to_traps (&gstate->path, gstate, &traps);
    if (status)
	goto BAIL;
  
    _cairo_traps_extents (&traps, &extents);

    *x1 = _cairo_fixed_to_double (extents.p1.x);
    *y1 = _cairo_fixed_to_double (extents.p1.y);
    *x2 = _cairo_fixed_to_double (extents.p2.x);
    *y2 = _cairo_fixed_to_double (extents.p2.y);

    cairo_matrix_transform_point (&gstate->ctm_inverse, x1, y1);
    cairo_matrix_transform_point (&gstate->ctm_inverse, x2, y2);
  
BAIL:
    _cairo_traps_fini (&traps);
  
    return status;
}

cairo_status_t
_cairo_gstate_init_clip (cairo_gstate_t *gstate)
{
    /* destroy any existing clip-region artifacts */
    if (gstate->clip.surface)
	cairo_surface_destroy (gstate->clip.surface);
    gstate->clip.surface = NULL;

    if (gstate->clip.region)
	pixman_region_destroy (gstate->clip.region);
    gstate->clip.region = NULL;

    /* reset the surface's clip to the whole surface */
    _cairo_surface_set_clip_region (gstate->surface, 
				    gstate->clip.region);

    return CAIRO_STATUS_SUCCESS;
}

static int
extract_transformed_rectangle(cairo_matrix_t *mat,
			      cairo_traps_t *tr,
			      pixman_box16_t *box)
{
#define CAIRO_FIXED_IS_INTEGER(x) (((x) & 0xFFFF) == 0)
#define CAIRO_FIXED_INTEGER_PART(x) ((x) >> 16)

    double a, b, c, d, tx, ty;
    cairo_status_t st;

    st = cairo_matrix_get_affine (mat, &a, &b, &c, &d, &tx, &ty);    
    if (!(st == CAIRO_STATUS_SUCCESS && b == 0. && c == 0.))
	return 0;

    if (tr->num_traps == 1 
	&& tr->traps[0].left.p1.x == tr->traps[0].left.p2.x
	&& tr->traps[0].right.p1.x == tr->traps[0].right.p2.x
	&& tr->traps[0].left.p1.y == tr->traps[0].right.p1.y
	&& tr->traps[0].left.p2.y == tr->traps[0].right.p2.y
	&& CAIRO_FIXED_IS_INTEGER(tr->traps[0].left.p1.x)
	&& CAIRO_FIXED_IS_INTEGER(tr->traps[0].left.p1.y)
	&& CAIRO_FIXED_IS_INTEGER(tr->traps[0].left.p2.x)
	&& CAIRO_FIXED_IS_INTEGER(tr->traps[0].left.p2.y)
	&& CAIRO_FIXED_IS_INTEGER(tr->traps[0].right.p1.x)
	&& CAIRO_FIXED_IS_INTEGER(tr->traps[0].right.p1.y)
	&& CAIRO_FIXED_IS_INTEGER(tr->traps[0].right.p2.x)
	&& CAIRO_FIXED_IS_INTEGER(tr->traps[0].right.p2.y)) {

	box->x1 = (short) CAIRO_FIXED_INTEGER_PART(tr->traps[0].left.p1.x);
	box->x2 = (short) CAIRO_FIXED_INTEGER_PART(tr->traps[0].right.p1.x);
	box->y1 = (short) CAIRO_FIXED_INTEGER_PART(tr->traps[0].left.p1.y);
	box->y2 = (short) CAIRO_FIXED_INTEGER_PART(tr->traps[0].left.p2.y);
	return 1;
    }
    return 0;

#undef CAIRO_FIXED_IS_INTEGER
#undef CAIRO_FIXED_INTEGER_PART
}

cairo_status_t
_cairo_gstate_clip (cairo_gstate_t *gstate)
{
    cairo_status_t status;
    cairo_pattern_t pattern;
    cairo_traps_t traps;
    cairo_color_t white_color;
    pixman_box16_t box;

    /* Fill the clip region as traps. */

    _cairo_traps_init (&traps);
    status = _cairo_path_fill_to_traps (&gstate->path, gstate, &traps);
    if (status) {
	_cairo_traps_fini (&traps);
	return status;
    }

    /* Check to see if we can represent these traps as a PixRegion. */

    if (extract_transformed_rectangle (&gstate->ctm, &traps, &box)) {

	pixman_region16_t *rect = NULL;
	pixman_region16_t *intersection = NULL;

	status = CAIRO_STATUS_SUCCESS;
	rect = pixman_region_create_simple (&box);
	
	if (rect == NULL) {
	    status = CAIRO_STATUS_NO_MEMORY;

	} else {
	    
	    if (gstate->clip.region == NULL) {
		gstate->clip.region = rect;		
	    } else {
		intersection = pixman_region_create();
		if (pixman_region_intersect (intersection, 
					     gstate->clip.region, rect)
		    == PIXMAN_REGION_STATUS_SUCCESS) {
		    pixman_region_destroy (gstate->clip.region);
		    gstate->clip.region = intersection;
		} else {		
		    status = CAIRO_STATUS_NO_MEMORY;
		}
		pixman_region_destroy (rect);
	    }
	    
	    if (!status)
		status = _cairo_surface_set_clip_region (gstate->surface, 
							 gstate->clip.region);
	}
	
	if (status != CAIRO_INT_STATUS_UNSUPPORTED) {
	    _cairo_traps_fini (&traps);
	    return status;
	}
    }

    /* Otherwise represent the clip as a mask surface. */

    _cairo_color_init (&white_color);

    if (gstate->clip.surface == NULL) {
	double x1, y1, x2, y2;
      _cairo_path_bounds (&gstate->path,
                          &x1, &y1, &x2, &y2);
      gstate->clip.x = floor (x1);
      gstate->clip.y = floor (y1);
      gstate->clip.width = ceil (x2 - gstate->clip.x);
      gstate->clip.height = ceil (y2 - gstate->clip.y);
      gstate->clip.surface =
        _cairo_surface_create_similar_solid (gstate->surface,
                                             CAIRO_FORMAT_A8,
                                             gstate->clip.width,
                                             gstate->clip.height,
                                             &white_color);
      if (gstate->clip.surface == NULL)
	    return CAIRO_STATUS_NO_MEMORY;
    }

    _cairo_pattern_init_solid (&pattern, 1.0, 1.0, 1.0);
    _cairo_pattern_set_alpha (&pattern, 1.0);
    
    _cairo_gstate_clip_and_composite_trapezoids (gstate,
						 &pattern,
						 CAIRO_OPERATOR_IN,
						 gstate->clip.surface,
						 &traps);
    
    _cairo_pattern_fini (&pattern);
    
    _cairo_traps_fini (&traps);

    return status;
}

cairo_status_t
_cairo_gstate_show_surface (cairo_gstate_t	*gstate,
			    cairo_surface_t	*surface,
			    int			width,
			    int			height)
{
    cairo_status_t status;
    cairo_matrix_t user_to_image, image_to_user;
    cairo_matrix_t image_to_device, device_to_image;
    double device_x, device_y;
    double device_width, device_height;
    cairo_pattern_t pattern;
    cairo_box_t extents;

    cairo_surface_get_matrix (surface, &user_to_image);
    cairo_matrix_multiply (&device_to_image, &gstate->ctm_inverse, &user_to_image);
    cairo_surface_set_matrix (surface, &device_to_image);

    image_to_user = user_to_image;
    cairo_matrix_invert (&image_to_user);
    cairo_matrix_multiply (&image_to_device, &image_to_user, &gstate->ctm);

    _cairo_gstate_current_point (gstate, &device_x, &device_y);
    device_width = width;
    device_height = height;
    _cairo_matrix_transform_bounding_box (&image_to_device,
					  &device_x, &device_y,
					  &device_width, &device_height);

    _cairo_pattern_init (&pattern);

    if ((gstate->pattern->type != CAIRO_PATTERN_SOLID) ||
        (gstate->alpha != 1.0)) {
	/* I'm allowing any type of pattern for the mask right now.
	   Maybe this is bad. Will allow for some cool effects though. */
	_cairo_pattern_init_copy (&pattern, gstate->pattern);
	extents.p1.x = _cairo_fixed_from_double (device_x);
	extents.p1.y = _cairo_fixed_from_double (device_y);
	extents.p2.x = _cairo_fixed_from_double (device_x + device_width);
	extents.p2.y = _cairo_fixed_from_double (device_y + device_height);
	status = _cairo_gstate_create_pattern (gstate, &pattern, &extents);
	if (status)
	    return status;
    }
    
    /* XXX: The rendered size is sometimes 1 or 2 pixels short from
       what I expect. Need to fix this. */
    status = _cairo_surface_composite (gstate->operator,
				       surface, pattern.source, gstate->surface,
				       device_x, device_y,
				       0, 0,
				       device_x, device_y,
				       device_width,
				       device_height);

    _cairo_pattern_fini (&pattern);

    /* restore the matrix originally in the surface */
    cairo_surface_set_matrix (surface, &user_to_image);
    
    if (status)
	return status;    

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_select_font (cairo_gstate_t       *gstate, 
			   const char           *family, 
			   cairo_font_slant_t   slant, 
			   cairo_font_weight_t  weight)
{
    if (gstate->font != NULL)
	cairo_font_destroy (gstate->font);

    gstate->font = _cairo_font_create (family, slant, weight);
  
    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_scale_font (cairo_gstate_t *gstate, 
			  double scale)
{
    return _cairo_font_scale (gstate->font, scale);
}

cairo_status_t
_cairo_gstate_transform_font (cairo_gstate_t *gstate, 
			      cairo_matrix_t *matrix)
{
    return _cairo_font_transform (gstate->font, matrix);    
}

cairo_status_t
_cairo_gstate_current_font (cairo_gstate_t *gstate, 
			    cairo_font_t **font)
{
    *font = gstate->font;

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_current_font_extents (cairo_gstate_t *gstate, 
				    cairo_font_extents_t *extents)
{
    cairo_int_status_t status;
    cairo_matrix_t saved_font_matrix;
    
    cairo_matrix_copy (&saved_font_matrix, &gstate->font->matrix);
    cairo_matrix_multiply (&gstate->font->matrix, &gstate->ctm, &gstate->font->matrix);
    
    status = _cairo_font_font_extents (gstate->font, extents);

    cairo_matrix_copy (&gstate->font->matrix, &saved_font_matrix);    

    return status;
}


cairo_status_t
_cairo_gstate_set_font (cairo_gstate_t *gstate, 
			cairo_font_t *font)
{
    if (gstate->font != NULL)    
	cairo_font_destroy (gstate->font);
    gstate->font = font;
    cairo_font_reference (gstate->font);
    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_text_extents (cairo_gstate_t *gstate,
			    const unsigned char *utf8,
			    cairo_text_extents_t *extents)
{
    cairo_matrix_t saved_font_matrix;
    cairo_status_t status;
    double scale_x, scale_y;

    cairo_matrix_copy (&saved_font_matrix, &gstate->font->matrix);
    _cairo_matrix_compute_scale_factors (&gstate->ctm, &scale_x, &scale_y);
    cairo_matrix_scale (&gstate->font->matrix, scale_x, scale_y);

    status = _cairo_font_text_extents (gstate->font,
				       utf8, extents);

    cairo_matrix_copy (&gstate->font->matrix, &saved_font_matrix);

    extents->x_bearing /= scale_x;
    extents->y_bearing /= scale_y;
    extents->width  /= scale_x;
    extents->height /= scale_y;
    extents->x_advance /= scale_x;
    extents->y_advance /= scale_y;

    return status;
}

cairo_status_t
_cairo_gstate_glyph_extents (cairo_gstate_t *gstate,
			     cairo_glyph_t *glyphs, 
			     int num_glyphs,
			     cairo_text_extents_t *extents)
{
    cairo_status_t status;
    cairo_matrix_t saved_font_matrix;
    double scale_x, scale_y;

    cairo_matrix_copy (&saved_font_matrix, &gstate->font->matrix);
    _cairo_matrix_compute_scale_factors (&gstate->ctm, &scale_x, &scale_y);
    cairo_matrix_scale (&gstate->font->matrix, scale_x, scale_y);

    status = _cairo_font_glyph_extents (gstate->font,
					glyphs, num_glyphs,
					extents);

    cairo_matrix_copy (&gstate->font->matrix, &saved_font_matrix);

    extents->x_bearing /= scale_x;
    extents->y_bearing /= scale_y;
    extents->width  /= scale_x;
    extents->height /= scale_y;
    extents->x_advance /= scale_x;
    extents->y_advance /= scale_y;

    return status;
}

cairo_status_t
_cairo_gstate_show_text (cairo_gstate_t *gstate, 
			 const unsigned char *utf8)
{
    cairo_status_t status;
    cairo_point_t point;
    double x, y;
    cairo_matrix_t saved_font_matrix;
    cairo_pattern_t pattern;
    cairo_text_extents_t text_extents;
    cairo_box_t extents;

    status = _cairo_path_current_point (&gstate->path, &point);
    if (status == CAIRO_STATUS_NO_CURRENT_POINT) {
	x = 0;
	y = 0;
	cairo_matrix_transform_point (&gstate->ctm, &x, &y);
    } else {
	x = _cairo_fixed_to_double (point.x);
	y = _cairo_fixed_to_double (point.y);
    }
    
    cairo_matrix_copy (&saved_font_matrix, &gstate->font->matrix);
    cairo_matrix_multiply (&gstate->font->matrix, &gstate->ctm, &gstate->font->matrix);

    _cairo_pattern_init_copy (&pattern, gstate->pattern);
    
    status = _cairo_gstate_text_extents (gstate, utf8, &text_extents);
    if (status)
	return status;
    
    extents.p1.x = _cairo_fixed_from_double (x);
    extents.p1.y = _cairo_fixed_from_double (y);
    extents.p2.x = _cairo_fixed_from_double (x + text_extents.width);
    extents.p2.y = _cairo_fixed_from_double (y + text_extents.height);
    status = _cairo_gstate_create_pattern (gstate, &pattern, &extents);
    if (status)
	return status;
    
    status = _cairo_font_show_text (gstate->font,
				    gstate->operator, pattern.source,
				    gstate->surface, x, y, utf8);
    
    cairo_matrix_copy (&gstate->font->matrix, &saved_font_matrix);

    _cairo_pattern_fini (&pattern);

    return status;
}

cairo_status_t
_cairo_gstate_show_glyphs (cairo_gstate_t *gstate, 
			   cairo_glyph_t *glyphs, 
			   int num_glyphs)
{
    cairo_status_t status;
    cairo_matrix_t saved_font_matrix;
    int i;
    cairo_glyph_t *transformed_glyphs = NULL;
    cairo_pattern_t pattern;
    cairo_text_extents_t text_extents;
    cairo_box_t extents;

    transformed_glyphs = malloc (num_glyphs * sizeof(cairo_glyph_t));
    if (transformed_glyphs == NULL)
	return CAIRO_STATUS_NO_MEMORY;
    
    for (i = 0; i < num_glyphs; ++i)
    {
	transformed_glyphs[i] = glyphs[i];
	cairo_matrix_transform_point (&gstate->ctm, 
				      &(transformed_glyphs[i].x), 
				      &(transformed_glyphs[i].y));
    }
    
    cairo_matrix_copy (&saved_font_matrix, &gstate->font->matrix);
    cairo_matrix_multiply (&gstate->font->matrix, &gstate->ctm, &gstate->font->matrix);

    _cairo_pattern_init_copy (&pattern, gstate->pattern);
    _cairo_gstate_glyph_extents (gstate, transformed_glyphs, num_glyphs,
                                 &text_extents);
    if (status)
	return status;

    extents.p1.x = _cairo_fixed_from_double (transformed_glyphs[0].x);
    extents.p1.y = _cairo_fixed_from_double (transformed_glyphs[0].y);
    extents.p2.x = _cairo_fixed_from_double (transformed_glyphs[0].x +
					     text_extents.width);
    extents.p2.y = _cairo_fixed_from_double (transformed_glyphs[0].y +
					     text_extents.height);
    status = _cairo_gstate_create_pattern (gstate, &pattern, &extents);
    if (status)
	return status;

    status = _cairo_font_show_glyphs (gstate->font, 
				      gstate->operator, pattern.source,
				      gstate->surface,
				      transformed_glyphs, num_glyphs);
    
    cairo_matrix_copy (&gstate->font->matrix, &saved_font_matrix);

    _cairo_pattern_fini (&pattern);

    free (transformed_glyphs);
    
    return status;
}


cairo_status_t
_cairo_gstate_text_path (cairo_gstate_t *gstate, 
			 const unsigned char *utf8)
{
    cairo_status_t status;
    cairo_matrix_t saved_font_matrix;
    cairo_point_t point;
    double x, y;

    status = _cairo_path_current_point (&gstate->path, &point);
    if (status == CAIRO_STATUS_NO_CURRENT_POINT) {
	x = 0;
	y = 0;
	cairo_matrix_transform_point (&gstate->ctm, &x, &y);
    } else {
	x = _cairo_fixed_to_double (point.x);
	y = _cairo_fixed_to_double (point.y);
    }

    cairo_matrix_copy (&saved_font_matrix, &gstate->font->matrix);
    cairo_matrix_multiply (&gstate->font->matrix, &gstate->ctm, &gstate->font->matrix);

    status = _cairo_font_text_path (gstate->font, 
				    x, y,
				    utf8,
				    &gstate->path);

    cairo_matrix_copy (&gstate->font->matrix, &saved_font_matrix);

    return status;
}


cairo_status_t
_cairo_gstate_glyph_path (cairo_gstate_t *gstate,
			  cairo_glyph_t *glyphs, 
			  int num_glyphs)
{
    cairo_status_t status;
    int i;
    cairo_glyph_t *transformed_glyphs = NULL;
    cairo_matrix_t saved_font_matrix;

    transformed_glyphs = malloc (num_glyphs * sizeof(cairo_glyph_t));
    if (transformed_glyphs == NULL)
	return CAIRO_STATUS_NO_MEMORY;
    
    for (i = 0; i < num_glyphs; ++i)
    {
	transformed_glyphs[i] = glyphs[i];
	cairo_matrix_transform_point (&gstate->ctm, 
				      &(transformed_glyphs[i].x), 
				      &(transformed_glyphs[i].y));
    }

    cairo_matrix_copy (&saved_font_matrix, &gstate->font->matrix);
    cairo_matrix_multiply (&gstate->font->matrix, &gstate->ctm, &gstate->font->matrix);

    status = _cairo_font_glyph_path (gstate->font, 
				     transformed_glyphs, num_glyphs,
				     &gstate->path);

    cairo_matrix_copy (&gstate->font->matrix, &saved_font_matrix);

    free (transformed_glyphs);
    return status;
}
