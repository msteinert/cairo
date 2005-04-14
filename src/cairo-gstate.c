/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2002 University of Southern California
 * Copyright © 2005 Red Hat, Inc.
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
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
 * The Initial Developer of the Original Code is University of Southern
 * California.
 *
 * Contributor(s):
 *	Carl D. Worth <cworth@cworth.org>
 */

#include <stdlib.h>

#include "cairoint.h"

#include "cairo-gstate-private.h"

static cairo_status_t
_cairo_gstate_clip_and_composite_trapezoids (cairo_gstate_t *gstate,
					     cairo_pattern_t *src,
					     cairo_operator_t operator,
					     cairo_surface_t *dst,
					     cairo_traps_t *traps);

static cairo_status_t
_cairo_gstate_ensure_font (cairo_gstate_t *gstate);

static cairo_status_t
_cairo_gstate_ensure_font_face (cairo_gstate_t *gstate);

static void
_cairo_gstate_unset_font (cairo_gstate_t *gstate);

cairo_gstate_t *
_cairo_gstate_create ()
{
    cairo_status_t status;
    cairo_gstate_t *gstate;

    gstate = malloc (sizeof (cairo_gstate_t));

    if (gstate)
    {
	status = _cairo_gstate_init (gstate);
	if (status) {
	    free (gstate);
	    return NULL;		
	}
    }

    return gstate;
}

cairo_status_t
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

    gstate->scaled_font = NULL;
    gstate->font_face = NULL;

    cairo_matrix_init_scale (&gstate->font_matrix,
			     CAIRO_GSTATE_DEFAULT_FONT_SIZE, 
			     CAIRO_GSTATE_DEFAULT_FONT_SIZE);
    
    gstate->surface = NULL;

    gstate->clip.region = NULL;
    gstate->clip.surface = NULL;

    gstate->source = _cairo_pattern_create_solid (CAIRO_COLOR_BLACK);
    if (!gstate->source)
	return CAIRO_STATUS_NO_MEMORY;    

    _cairo_gstate_identity_matrix (gstate);

    _cairo_path_fixed_init (&gstate->path);

    _cairo_pen_init_empty (&gstate->pen_regular);

    gstate->next = NULL;

    return CAIRO_STATUS_SUCCESS;
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

    if (other->clip.region)
    {	
	gstate->clip.region = pixman_region_create ();
	pixman_region_copy (gstate->clip.region, other->clip.region);
    }

    if (gstate->font_face)
	cairo_font_face_reference (gstate->font_face);

    if (gstate->scaled_font)
	cairo_scaled_font_reference (gstate->scaled_font);
    
    cairo_surface_reference (gstate->surface);
    cairo_surface_reference (gstate->clip.surface);

    cairo_pattern_reference (gstate->source);
    
    status = _cairo_path_fixed_init_copy (&gstate->path, &other->path);
    if (status)
	goto CLEANUP_FONT;

    status = _cairo_pen_init_copy (&gstate->pen_regular, &other->pen_regular);
    if (status)
	goto CLEANUP_PATH;

    return status;

  CLEANUP_PATH:
    _cairo_path_fixed_fini (&gstate->path);

  CLEANUP_FONT:
    cairo_scaled_font_destroy (gstate->scaled_font);
    gstate->scaled_font = NULL;
    
    free (gstate->dash);
    gstate->dash = NULL;

    return CAIRO_STATUS_NO_MEMORY;
}

void
_cairo_gstate_fini (cairo_gstate_t *gstate)
{
    if (gstate->font_face)
	cairo_font_face_destroy (gstate->font_face);

    if (gstate->scaled_font)
	cairo_scaled_font_destroy (gstate->scaled_font);

    if (gstate->surface)
	cairo_surface_destroy (gstate->surface);
    gstate->surface = NULL;

    if (gstate->clip.surface)
	cairo_surface_destroy (gstate->clip.surface);
    gstate->clip.surface = NULL;

    if (gstate->clip.region)
	pixman_region_destroy (gstate->clip.region);
    gstate->clip.region = NULL;

    cairo_pattern_destroy (gstate->source);

    _cairo_path_fixed_fini (&gstate->path);

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

    status = _cairo_surface_fill_rectangle (gstate->surface,
                                   CAIRO_OPERATOR_SRC,
				   &CAIRO_COLOR_TRANSPARENT,
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
    _cairo_gstate_unset_font (gstate);

    if (gstate->surface)
	cairo_surface_destroy (gstate->surface);

    gstate->surface = surface;

    /* Sometimes the user wants to return to having no target surface,
     * (just like after cairo_create). This can be useful for forcing
     * the old surface to be destroyed. */
    if (surface == NULL)
	return CAIRO_STATUS_SUCCESS;

    cairo_surface_reference (gstate->surface);

    _cairo_gstate_identity_matrix (gstate);

    return CAIRO_STATUS_SUCCESS;
}

/* XXX: Need to decide the memory mangement semantics of this
   function. Should it reference the surface again? */
cairo_surface_t *
_cairo_gstate_get_target_surface (cairo_gstate_t *gstate)
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
_cairo_gstate_set_source (cairo_gstate_t *gstate,
			  cairo_pattern_t *source)
{
    if (source == NULL)
	return CAIRO_STATUS_NULL_POINTER;

    cairo_pattern_reference (source);
    cairo_pattern_destroy (gstate->source);
    gstate->source = source;
    
    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_set_source_solid (cairo_gstate_t	    *gstate,
				const cairo_color_t *color)
{
    cairo_status_t status;
    cairo_pattern_t *source;

    source = _cairo_pattern_create_solid (color);
    if (!source)
	return CAIRO_STATUS_NO_MEMORY;

    status = _cairo_gstate_set_source (gstate, source);

    cairo_pattern_destroy (source);
    
    return CAIRO_STATUS_SUCCESS;
}

cairo_pattern_t *
_cairo_gstate_get_source (cairo_gstate_t *gstate)
{
    if (gstate == NULL)
	return NULL;

    return gstate->source;
}

cairo_status_t
_cairo_gstate_set_operator (cairo_gstate_t *gstate, cairo_operator_t operator)
{
    gstate->operator = operator;

    return CAIRO_STATUS_SUCCESS;
}

cairo_operator_t
_cairo_gstate_get_operator (cairo_gstate_t *gstate)
{
    return gstate->operator;
}

cairo_status_t
_cairo_gstate_get_rgb_color (cairo_gstate_t *gstate, double *red, double *green, double *blue)
{
    return _cairo_pattern_get_rgb (gstate->source, red, green, blue);
}

cairo_status_t
_cairo_gstate_set_tolerance (cairo_gstate_t *gstate, double tolerance)
{
    gstate->tolerance = tolerance;

    return CAIRO_STATUS_SUCCESS;
}

double
_cairo_gstate_get_tolerance (cairo_gstate_t *gstate)
{
    return gstate->tolerance;
}

cairo_status_t
_cairo_gstate_set_fill_rule (cairo_gstate_t *gstate, cairo_fill_rule_t fill_rule)
{
    gstate->fill_rule = fill_rule;

    return CAIRO_STATUS_SUCCESS;
}

cairo_fill_rule_t
_cairo_gstate_get_fill_rule (cairo_gstate_t *gstate)
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
_cairo_gstate_get_line_width (cairo_gstate_t *gstate)
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
_cairo_gstate_get_line_cap (cairo_gstate_t *gstate)
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
_cairo_gstate_get_line_join (cairo_gstate_t *gstate)
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
_cairo_gstate_get_miter_limit (cairo_gstate_t *gstate)
{
    return gstate->miter_limit;
}

void
_cairo_gstate_get_matrix (cairo_gstate_t *gstate, cairo_matrix_t *matrix)
{
    *matrix = gstate->ctm;
}

cairo_status_t
_cairo_gstate_translate (cairo_gstate_t *gstate, double tx, double ty)
{
    cairo_matrix_t tmp;

    _cairo_gstate_unset_font (gstate);
    
    cairo_matrix_init_translate (&tmp, tx, ty);
    cairo_matrix_multiply (&gstate->ctm, &tmp, &gstate->ctm);

    cairo_matrix_init_translate (&tmp, -tx, -ty);
    cairo_matrix_multiply (&gstate->ctm_inverse, &gstate->ctm_inverse, &tmp);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_scale (cairo_gstate_t *gstate, double sx, double sy)
{
    cairo_matrix_t tmp;

    if (sx == 0 || sy == 0)
	return CAIRO_STATUS_INVALID_MATRIX;

    _cairo_gstate_unset_font (gstate);
    
    cairo_matrix_init_scale (&tmp, sx, sy);
    cairo_matrix_multiply (&gstate->ctm, &tmp, &gstate->ctm);

    cairo_matrix_init_scale (&tmp, 1/sx, 1/sy);
    cairo_matrix_multiply (&gstate->ctm_inverse, &gstate->ctm_inverse, &tmp);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_rotate (cairo_gstate_t *gstate, double angle)
{
    cairo_matrix_t tmp;

    _cairo_gstate_unset_font (gstate);
    
    cairo_matrix_init_rotate (&tmp, angle);
    cairo_matrix_multiply (&gstate->ctm, &tmp, &gstate->ctm);

    cairo_matrix_init_rotate (&tmp, -angle);
    cairo_matrix_multiply (&gstate->ctm_inverse, &gstate->ctm_inverse, &tmp);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_transform (cairo_gstate_t *gstate, cairo_matrix_t *matrix)
{
    cairo_matrix_t tmp;

    _cairo_gstate_unset_font (gstate);
    
    tmp = *matrix;
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

    _cairo_gstate_unset_font (gstate);
    
    gstate->ctm = *matrix;

    gstate->ctm_inverse = *matrix;
    status = cairo_matrix_invert (&gstate->ctm_inverse);
    if (status)
	return status;

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_identity_matrix (cairo_gstate_t *gstate)
{
    _cairo_gstate_unset_font (gstate);
    
    cairo_matrix_init_identity (&gstate->ctm);
    cairo_matrix_init_identity (&gstate->ctm_inverse);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_user_to_device (cairo_gstate_t *gstate, double *x, double *y)
{
    cairo_matrix_transform_point (&gstate->ctm, x, y);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_user_to_device_distance (cairo_gstate_t *gstate,
				       double *dx, double *dy)
{
    cairo_matrix_transform_distance (&gstate->ctm, dx, dy);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_device_to_user (cairo_gstate_t *gstate, double *x, double *y)
{
    cairo_matrix_transform_point (&gstate->ctm_inverse, x, y);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_device_to_user_distance (cairo_gstate_t *gstate,
				       double *dx, double *dy)
{
    cairo_matrix_transform_distance (&gstate->ctm_inverse, dx, dy);

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_gstate_user_to_backend (cairo_gstate_t *gstate, double *x, double *y)
{
    cairo_matrix_transform_point (&gstate->ctm, x, y);
    if (gstate->surface) {
	*x += gstate->surface->device_x_offset;
	*y += gstate->surface->device_y_offset;
    }
}

static void
_cairo_gstate_backend_to_user (cairo_gstate_t *gstate, double *x, double *y)
{
    if (gstate->surface) {
	*x -= gstate->surface->device_x_offset;
	*y -= gstate->surface->device_y_offset;
    }
    cairo_matrix_transform_point (&gstate->ctm_inverse, x, y);
}

cairo_status_t
_cairo_gstate_new_path (cairo_gstate_t *gstate)
{
    _cairo_path_fixed_fini (&gstate->path);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_move_to (cairo_gstate_t *gstate, double x, double y)
{
    cairo_point_t point;

    _cairo_gstate_user_to_backend (gstate, &x, &y);

    point.x = _cairo_fixed_from_double (x);
    point.y = _cairo_fixed_from_double (y);

    return _cairo_path_fixed_move_to (&gstate->path, &point);
}

cairo_status_t
_cairo_gstate_line_to (cairo_gstate_t *gstate, double x, double y)
{
    cairo_point_t point;

    _cairo_gstate_user_to_backend (gstate, &x, &y);

    point.x = _cairo_fixed_from_double (x);
    point.y = _cairo_fixed_from_double (y);

    return _cairo_path_fixed_line_to (&gstate->path, &point);
}

cairo_status_t
_cairo_gstate_curve_to (cairo_gstate_t *gstate,
			double x0, double y0,
			double x1, double y1,
			double x2, double y2)
{
    cairo_point_t p0, p1, p2;

    _cairo_gstate_user_to_backend (gstate, &x0, &y0);
    _cairo_gstate_user_to_backend (gstate, &x1, &y1);
    _cairo_gstate_user_to_backend (gstate, &x2, &y2);

    p0.x = _cairo_fixed_from_double (x0);
    p0.y = _cairo_fixed_from_double (y0);

    p1.x = _cairo_fixed_from_double (x1);
    p1.y = _cairo_fixed_from_double (y1);

    p2.x = _cairo_fixed_from_double (x2);
    p2.y = _cairo_fixed_from_double (y2);

    return _cairo_path_fixed_curve_to (&gstate->path, &p0, &p1, &p2);
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
	double angle_mid = angle_min + (angle_max - angle_min) / 2.0;
	/* XXX: Something tells me this block could be condensed. */
	if (dir == CAIRO_DIRECTION_FORWARD) {
	    status = _cairo_gstate_arc_dir (gstate, xc, yc, radius,
					    angle_min, angle_mid, dir);
	    if (status)
		return status;
	    
	    status = _cairo_gstate_arc_dir (gstate, xc, yc, radius,
					    angle_mid, angle_max, dir);
	    if (status)
		return status;
	} else {
	    status = _cairo_gstate_arc_dir (gstate, xc, yc, radius,
					    angle_mid, angle_max, dir);
	    if (status)
		return status;

	    status = _cairo_gstate_arc_dir (gstate, xc, yc, radius,
					    angle_min, angle_mid, dir);
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

    return _cairo_path_fixed_rel_move_to (&gstate->path, &distance);
}

cairo_status_t
_cairo_gstate_rel_line_to (cairo_gstate_t *gstate, double dx, double dy)
{
    cairo_distance_t distance;

    cairo_matrix_transform_distance (&gstate->ctm, &dx, &dy);

    distance.dx = _cairo_fixed_from_double (dx);
    distance.dy = _cairo_fixed_from_double (dy);

    return _cairo_path_fixed_rel_line_to (&gstate->path, &distance);
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

    return _cairo_path_fixed_rel_curve_to (&gstate->path,
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
    return _cairo_path_fixed_close_path (&gstate->path);
}

cairo_status_t
_cairo_gstate_get_current_point (cairo_gstate_t *gstate, double *x_ret, double *y_ret)
{
    cairo_status_t status;
    cairo_point_t point;
    double x, y;

    status = _cairo_path_fixed_get_current_point (&gstate->path, &point);
    if (status == CAIRO_STATUS_NO_CURRENT_POINT) {
	x = 0.0;
	y = 0.0;
    } else {
	x = _cairo_fixed_to_double (point.x);
	y = _cairo_fixed_to_double (point.y);
	_cairo_gstate_backend_to_user (gstate, &x, &y);
    }

    if (x_ret)
	*x_ret = x;
    if (y_ret)
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
    cairo_path_fixed_t path;
    gpi_t gpi;

    /* Anything we want from gstate must be copied. We must not retain
       pointers into gstate. */
    _cairo_path_fixed_init_copy (&path, &gstate->path);

    gpi.ctm_inverse = gstate->ctm_inverse;
    if (gstate->surface)
	cairo_matrix_translate (&gpi.ctm_inverse,
				- gstate->surface->device_x_offset,
				- gstate->surface->device_y_offset);
				
    gpi.tolerance = gstate->tolerance;

    gpi.move_to = move_to;
    gpi.line_to = line_to;
    gpi.curve_to = curve_to;
    gpi.close_path = close_path;
    gpi.closure = closure;

    gpi.current_point.x = 0;
    gpi.current_point.y = 0;

    return _cairo_path_fixed_interpret (&path,
					CAIRO_DIRECTION_FORWARD,
					_gpi_move_to,
					_gpi_line_to,
					_gpi_curve_to,
					_gpi_close_path,
					&gpi);
}

static void
_cairo_gstate_pattern_transform (cairo_gstate_t  *gstate,
				 cairo_pattern_t *pattern)
{
    cairo_matrix_t tmp_matrix = gstate->ctm_inverse;
    
    if (gstate->surface)
	cairo_matrix_translate (&tmp_matrix,
				- gstate->surface->device_x_offset,
				- gstate->surface->device_y_offset);

    _cairo_pattern_transform (pattern, &tmp_matrix);
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

    status = _cairo_path_fixed_stroke_to_traps (&gstate->path, gstate, &traps);
    if (status) {
	_cairo_traps_fini (&traps);
	return status;
    }

    _cairo_gstate_clip_and_composite_trapezoids (gstate,
                                                 gstate->source,
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
			 cairo_bool_t	*inside_ret)
{
    cairo_status_t status = CAIRO_STATUS_SUCCESS;
    cairo_traps_t traps;

    _cairo_gstate_user_to_backend (gstate, &x, &y);

    _cairo_pen_init (&gstate->pen_regular, gstate->line_width / 2.0, gstate);

    _cairo_traps_init (&traps);

    status = _cairo_path_fixed_stroke_to_traps (&gstate->path, gstate, &traps);
    if (status)
	goto BAIL;

    *inside_ret = _cairo_traps_contain (&traps, x, y);

BAIL:
    _cairo_traps_fini (&traps);

    return status;
}

/* XXX We currently have a confusing mix of boxes and rectangles as
 * exemplified by this function.  A cairo_box_t is a rectangular area
 * represented by the coordinates of the upper left and lower right
 * corners, expressed in fixed point numbers.  A cairo_rectangle_t is
 * also a rectangular area, but represented by the upper left corner
 * and the width and the height, as integer numbers.
 *
 * This function converts a cairo_box_t to a cairo_rectangle_t by
 * increasing the area to the nearest integer coordinates.  We should
 * standardize on cairo_rectangle_t and cairo_rectangle_fixed_t, and
 * this function could be renamed to the more reasonable
 * _cairo_rectangle_fixed_round.
 */

static void
_cairo_box_round_to_rectangle (cairo_box_t *box, cairo_rectangle_t *rectangle)
{
    rectangle->x = _cairo_fixed_integer_floor (box->p1.x);
    rectangle->y = _cairo_fixed_integer_floor (box->p1.y);
    rectangle->width = _cairo_fixed_integer_ceil (box->p2.x) - rectangle->x;
    rectangle->height = _cairo_fixed_integer_ceil (box->p2.y) - rectangle->y;
}

static void
_cairo_rectangle_intersect (cairo_rectangle_t *dest, cairo_rectangle_t *src)
{
    int x1, y1, x2, y2;

    x1 = MAX (dest->x, src->x);
    y1 = MAX (dest->y, src->y);
    x2 = MIN (dest->x + dest->width, src->x + src->width);
    y2 = MIN (dest->y + dest->height, src->y + src->height);

    if (x1 >= x2 || y1 >= y2) {
	dest->x = 0;
	dest->y = 0;
	dest->width = 0;
	dest->height = 0;
    } else {
	dest->x = x1;
	dest->y = y1;
	dest->width = x2 - x1;
	dest->height = y2 - y1;
    }	
}

static int
_cairo_rectangle_empty (cairo_rectangle_t *rect)
{
    return rect->width == 0 || rect->height == 0;
}

/* Creates a region from a cairo_rectangle_t */
static cairo_status_t
_region_new_from_rect (cairo_rectangle_t  *rect,
		       pixman_region16_t **region)
{
    *region = pixman_region_create ();
    if (pixman_region_union_rect (*region, *region,
				  rect->x, rect->y,
				  rect->width, rect->height) != PIXMAN_REGION_STATUS_SUCCESS) {
	pixman_region_destroy (*region);
	return CAIRO_STATUS_NO_MEMORY;
    }

    return CAIRO_STATUS_SUCCESS;
}

/* Gets the bounding box of a region as a cairo_rectangle_t */
static void
_region_rect_extents (pixman_region16_t *region,
		      cairo_rectangle_t *rect)
{
    pixman_box16_t *region_extents = pixman_region_extents (region);

    rect->x = region_extents->x1;
    rect->y = region_extents->y1;
    rect->width = region_extents->x2 - region_extents->x1;
    rect->height = region_extents->y2 - region_extents->y1;
}

/* Given a region representing a set of trapezoids that will be
 * drawn, clip the region according to the gstate and compute
 * the overall extents.
 */
static cairo_status_t
_clip_and_compute_extents_region (cairo_gstate_t    *gstate,
				  pixman_region16_t *trap_region,
				  cairo_rectangle_t *extents)
{
    if (gstate->clip.region)
	pixman_region_intersect (trap_region,
				 gstate->clip.region,
				 trap_region);

    if (gstate->clip.surface) {
	pixman_region16_t *clip_rect;
	cairo_status_t status;
    
	status = _region_new_from_rect (&gstate->clip.rect,
					&clip_rect);
	if (!CAIRO_OK (status))
	    return status;
	
	if (pixman_region_intersect (trap_region,
				     clip_rect,
				     trap_region) != PIXMAN_REGION_STATUS_SUCCESS)
	    status = CAIRO_STATUS_NO_MEMORY;

	pixman_region_destroy (clip_rect);

	if (!CAIRO_OK (status))
	    return status;
    }

    _region_rect_extents (trap_region, extents);

    return CAIRO_STATUS_SUCCESS;
}

/* Given a a set of trapezoids to draw, find a bounding box (non-exact)
 * of the trapezoids clipped by the gstate
 */
static cairo_status_t
_clip_and_compute_extents_arbitrary (cairo_gstate_t    *gstate,
				     cairo_traps_t     *traps,
				     cairo_rectangle_t *extents)
{
    cairo_box_t trap_extents;
	
    _cairo_traps_extents (traps, &trap_extents);
    _cairo_box_round_to_rectangle (&trap_extents, extents);
    
    if (gstate->clip.region) {
	pixman_region16_t *intersection;
	cairo_status_t status;
	
	status = _region_new_from_rect (extents, &intersection);
	if (!CAIRO_OK (status))
	    return status;
	
	if (pixman_region_intersect (intersection,
				     gstate->clip.region,
				     intersection) == PIXMAN_REGION_STATUS_SUCCESS) 
	    _region_rect_extents (intersection, extents);
	else
	    status = CAIRO_STATUS_NO_MEMORY;

	pixman_region_destroy (intersection);

	if (!CAIRO_OK (status))
	    return status;
    }

    if (gstate->clip.surface)
	_cairo_rectangle_intersect (extents, &gstate->clip.rect);

    return CAIRO_STATUS_SUCCESS;
}

/* Composites a region representing a set of trapezoids.
 */
static cairo_status_t
_composite_trap_region (cairo_gstate_t    *gstate,
			cairo_pattern_t   *src,
			cairo_operator_t   operator,
			cairo_surface_t   *dst,
			pixman_region16_t *trap_region,
			cairo_rectangle_t *extents)
{
    cairo_status_t status, tmp_status;
    cairo_pattern_union_t pattern;
    cairo_pattern_union_t mask;
    int num_rects = pixman_region_num_rects (trap_region);

    if (num_rects == 0)
	return CAIRO_STATUS_SUCCESS;
    
    if (num_rects > 1) {
	status = _cairo_surface_set_clip_region (dst, trap_region);
	if (!CAIRO_OK (status))
	    return status;
    }
    
    _cairo_pattern_init_copy (&pattern.base, src);
    _cairo_gstate_pattern_transform (gstate, &pattern.base);

    if (gstate->clip.surface)
	_cairo_pattern_init_for_surface (&mask.surface, gstate->clip.surface);
	
    status = _cairo_surface_composite (gstate->operator,
				       &pattern.base,
				       gstate->clip.surface ? &mask.base : NULL,
				       dst,
				       extents->x, extents->y,
				       extents->x - (gstate->clip.surface ? gstate->clip.rect.x : 0),
				       extents->y - (gstate->clip.surface ? gstate->clip.rect.y : 0),
				       extents->x, extents->y,
				       extents->width, extents->height);

    if (gstate->clip.surface)
	_cairo_pattern_fini (&pattern.base);
    _cairo_pattern_fini (&mask.base);

    if (num_rects > 1) {
	tmp_status = _cairo_surface_set_clip_region (dst, gstate->clip.region);
	if (!CAIRO_OK (tmp_status))
	    status = tmp_status;
    }
				      
    return status;
}

static void
translate_traps (cairo_traps_t *traps, int x, int y)
{
    cairo_fixed_t xoff, yoff;
    cairo_trapezoid_t *t;
    int i;

    /* Ugh. The cairo_composite/(Render) interface doesn't allow
       an offset for the trapezoids. Need to manually shift all
       the coordinates to align with the offset origin of the
       intermediate surface. */

    xoff = _cairo_fixed_from_int (x);
    yoff = _cairo_fixed_from_int (y);

    for (i = 0, t = traps->traps; i < traps->num_traps; i++, t++) {
	t->top += yoff;
	t->bottom += yoff;
	t->left.p1.x += xoff;
	t->left.p1.y += yoff;
	t->left.p2.x += xoff;
	t->left.p2.y += yoff;
	t->right.p1.x += xoff;
	t->right.p1.y += yoff;
	t->right.p2.x += xoff;
	t->right.p2.y += yoff;
    }
}

/* Composites a set of trapezoids in the case where we need to create
 * an intermediate surface to handle gstate->clip.surface
 * 
 * Warning: This call modifies the coordinates of traps
 */
static cairo_status_t
_composite_traps_intermediate_surface (cairo_gstate_t    *gstate,
				       cairo_pattern_t   *src,
				       cairo_operator_t   operator,
				       cairo_surface_t   *dst,
				       cairo_traps_t     *traps,
				       cairo_rectangle_t *extents)
{
    cairo_surface_t *intermediate;
    cairo_pattern_union_t pattern;
    cairo_surface_pattern_t intermediate_pattern;
    cairo_status_t status;

    translate_traps (traps, -extents->x, -extents->y);

    intermediate = _cairo_surface_create_similar_solid (gstate->clip.surface,
							CAIRO_FORMAT_A8,
							extents->width,
							extents->height,
							CAIRO_COLOR_TRANSPARENT);
    if (intermediate == NULL)
	return CAIRO_STATUS_NO_MEMORY;
    
    _cairo_pattern_init_solid (&pattern.solid, CAIRO_COLOR_WHITE);
    
    status = _cairo_surface_composite_trapezoids (CAIRO_OPERATOR_ADD,
						  &pattern.base,
						  intermediate,
						  extents->x, extents->y,
						  0, 0,
						  extents->width,
						  extents->height,
						  traps->traps,
						  traps->num_traps);
    _cairo_pattern_fini (&pattern.base);
    
    if (!CAIRO_OK (status))
	goto out;

    _cairo_pattern_init_for_surface (&pattern.surface,
				     gstate->clip.surface);
    
    status = _cairo_surface_composite (CAIRO_OPERATOR_IN,
				       &pattern.base,
				       NULL,
				       intermediate,
				       extents->x - gstate->clip.rect.x,
				       extents->y - gstate->clip.rect.y, 
				       0, 0,
				       0, 0,
				       extents->width, extents->height);
    _cairo_pattern_fini (&pattern.base);
    
    if (!CAIRO_OK (status))
	goto out;
    
    _cairo_pattern_init_for_surface (&intermediate_pattern, intermediate);
    _cairo_pattern_init_copy (&pattern.base, src);
    _cairo_gstate_pattern_transform (gstate, &pattern.base);
    
    status = _cairo_surface_composite (operator,
				       &pattern.base,
				       &intermediate_pattern.base,
				       dst,
				       extents->x, extents->y,
				       0, 0,
				       extents->x, extents->y,
				       extents->width, extents->height);
    
    _cairo_pattern_fini (&pattern.base);
    _cairo_pattern_fini (&intermediate_pattern.base);
    
 out:
    cairo_surface_destroy (intermediate);
    
    return status;
}

/* Composites a region representing a set of trapezoids in the
 * case of a solid source (so we can use
 * _cairo_surface_fill_rectangles).
 */
static cairo_status_t
_composite_trap_region_solid (cairo_gstate_t        *gstate,
			      cairo_solid_pattern_t *src,
			      cairo_operator_t       operator,
			      cairo_surface_t       *dst,
			      pixman_region16_t     *region)
{
    int num_rects = pixman_region_num_rects (region);
    pixman_box16_t *boxes = pixman_region_rects (region);
    cairo_rectangle_t *rects;
    cairo_status_t status;
    int i;

    if (!num_rects)
	return CAIRO_STATUS_SUCCESS;
    
    rects = malloc (sizeof (pixman_rectangle_t) * num_rects);
    if (!rects)
	return CAIRO_STATUS_NO_MEMORY;

    for (i = 0; i < num_rects; i++) {
	rects[i].x = boxes[i].x1;
	rects[i].y = boxes[i].y1;
	rects[i].width = boxes[i].x2 - boxes[i].x1;
	rects[i].height = boxes[i].y2 - boxes[i].y1;
    }

    status =  _cairo_surface_fill_rectangles (dst, operator,
					      &src->color, rects, num_rects);
    
    free (rects);

    return status;
}

/* Composites a set of trapezoids in the general case where
   gstate->clip.surface == NULL
 */
static cairo_status_t
_composite_traps (cairo_gstate_t    *gstate,
		  cairo_pattern_t   *src,
		  cairo_operator_t   operator,
		  cairo_surface_t   *dst,
		  cairo_traps_t     *traps,
		  cairo_rectangle_t *extents)
{
    cairo_pattern_union_t pattern;
    cairo_status_t status;

    _cairo_pattern_init_copy (&pattern.base, src);
    _cairo_gstate_pattern_transform (gstate, &pattern.base);
    
    status = _cairo_surface_composite_trapezoids (gstate->operator,
						  &pattern.base, dst,
						  extents->x, extents->y,
						  extents->x, extents->y,
						  extents->width,
						  extents->height,
						  traps->traps,
						  traps->num_traps);

    _cairo_pattern_fini (&pattern.base);

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
    pixman_region16_t *trap_region;
    cairo_rectangle_t extents;
    
    if (traps->num_traps == 0)
	return CAIRO_STATUS_SUCCESS;

    if (gstate->surface == NULL)
	return CAIRO_STATUS_NO_TARGET_SURFACE;

    status = _cairo_traps_extract_region (traps, &trap_region);
    if (!CAIRO_OK (status))
	return status;

    if (trap_region)
	status = _clip_and_compute_extents_region (gstate, trap_region, &extents);
    else
	status = _clip_and_compute_extents_arbitrary (gstate, traps, &extents);
	
    if (!CAIRO_OK (status))
	goto out;
    
    if (_cairo_rectangle_empty (&extents))
	/* Nothing to do */
	goto out;

    if (gstate->clip.surface) {
	if (trap_region) {
	    /* If we are compositing a set of rectangles, we can set them as the
	     * clip region for the destination surface and use the clip surface
	     * as the mask. A clip region might not be supported, in which case
	     * we fall through to the next method
	     */
	    status = _composite_trap_region (gstate, src, operator, dst,
					     trap_region, &extents);
	    if (status != CAIRO_INT_STATUS_UNSUPPORTED)
		goto out;
	}
	
	/* Handle a clip surface by creating an intermediate surface. */
	status = _composite_traps_intermediate_surface (gstate, src, operator,
							dst, traps, &extents);
    } else {
        /* No clip surface */
	if (trap_region && src->type == CAIRO_PATTERN_SOLID) {
	    /* Solid rectangles are handled specially */
	    status = _composite_trap_region_solid (gstate, (cairo_solid_pattern_t *)src,
						   operator, dst, trap_region);
	} else if (trap_region && pixman_region_num_rects (trap_region) <= 1) {
	    /* For a simple rectangle, we can just use composite(), for more
	     * rectangles, we'd have to set a clip region. That might still
	     * be a win, but it's less obvious. (Depends on the backend)
	     */
	    status = _composite_trap_region (gstate, src, operator, dst,
					     trap_region, &extents);
	} else {
	    status = _composite_traps (gstate, src, operator,
				       dst, traps, &extents);
	}
    }

 out:
    if (trap_region)
	pixman_region_destroy (trap_region);
    
    return status;
}

cairo_status_t
_cairo_gstate_fill (cairo_gstate_t *gstate)
{
    cairo_status_t status;
    cairo_traps_t traps;

    _cairo_traps_init (&traps);

    status = _cairo_path_fixed_fill_to_traps (&gstate->path, gstate, &traps);
    if (status) {
	_cairo_traps_fini (&traps);
	return status;
    }

    _cairo_gstate_clip_and_composite_trapezoids (gstate,
                                                 gstate->source,
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
		       cairo_bool_t	*inside_ret)
{
    cairo_status_t status = CAIRO_STATUS_SUCCESS;
    cairo_traps_t traps;

    _cairo_gstate_user_to_backend (gstate, &x, &y);

    _cairo_traps_init (&traps);

    status = _cairo_path_fixed_fill_to_traps (&gstate->path, gstate, &traps);
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
  
    _cairo_pen_init (&gstate->pen_regular, gstate->line_width / 2.0, gstate);

    _cairo_traps_init (&traps);
  
    status = _cairo_path_fixed_stroke_to_traps (&gstate->path, gstate, &traps);
    if (status)
	goto BAIL;

    _cairo_traps_extents (&traps, &extents);

    *x1 = _cairo_fixed_to_double (extents.p1.x);
    *y1 = _cairo_fixed_to_double (extents.p1.y);
    *x2 = _cairo_fixed_to_double (extents.p2.x);
    *y2 = _cairo_fixed_to_double (extents.p2.y);

    _cairo_gstate_backend_to_user (gstate, x1, y1);
    _cairo_gstate_backend_to_user (gstate, x2, y2);
  
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
  
    status = _cairo_path_fixed_fill_to_traps (&gstate->path, gstate, &traps);
    if (status)
	goto BAIL;
  
    _cairo_traps_extents (&traps, &extents);

    *x1 = _cairo_fixed_to_double (extents.p1.x);
    *y1 = _cairo_fixed_to_double (extents.p1.y);
    *x2 = _cairo_fixed_to_double (extents.p2.x);
    *y2 = _cairo_fixed_to_double (extents.p2.y);

    _cairo_gstate_backend_to_user (gstate, x1, y1);
    _cairo_gstate_backend_to_user (gstate, x2, y2);
  
BAIL:
    _cairo_traps_fini (&traps);
  
    return status;
}

cairo_status_t
_cairo_gstate_reset_clip (cairo_gstate_t *gstate)
{
    /* destroy any existing clip-region artifacts */
    if (gstate->clip.surface)
	cairo_surface_destroy (gstate->clip.surface);
    gstate->clip.surface = NULL;

    if (gstate->clip.region)
	pixman_region_destroy (gstate->clip.region);
    gstate->clip.region = NULL;

    /* reset the surface's clip to the whole surface */
    if (gstate->surface)
	_cairo_surface_set_clip_region (gstate->surface, 
					gstate->clip.region);

    return CAIRO_STATUS_SUCCESS;
}

/* Reset surface clip region to the one in the gstate */
cairo_status_t
_cairo_gstate_restore_external_state (cairo_gstate_t *gstate)
{
    cairo_status_t status;

    status = CAIRO_STATUS_SUCCESS;
    
    if (gstate->surface) 
	status = _cairo_surface_set_clip_region (gstate->surface, 
						 gstate->clip.region);

    /* If not supported we're already using surface clipping */
    if (status == CAIRO_INT_STATUS_UNSUPPORTED)
	status = CAIRO_STATUS_SUCCESS;

    return status;
}

cairo_status_t
_cairo_gstate_clip (cairo_gstate_t *gstate)
{
    cairo_status_t status;
    cairo_pattern_union_t pattern;
    cairo_traps_t traps;
    cairo_box_t extents;
    pixman_region16_t *region;

    /* Fill the clip region as traps. */

    _cairo_traps_init (&traps);
    status = _cairo_path_fixed_fill_to_traps (&gstate->path, gstate, &traps);
    if (!CAIRO_OK (status)) {
	_cairo_traps_fini (&traps);
	return status;
    }

    /* Check to see if we can represent these traps as a PixRegion. */

    status = _cairo_traps_extract_region (&traps, &region);
    if (!CAIRO_OK (status)) {
	_cairo_traps_fini (&traps);
	return status;
    }
    
    if (region) {
	status = CAIRO_STATUS_SUCCESS;
	
	if (gstate->clip.region == NULL) {
	    gstate->clip.region = region;
	} else {
	    pixman_region16_t *intersection = pixman_region_create();

	    if (pixman_region_intersect (intersection, 
					 gstate->clip.region, region)
		== PIXMAN_REGION_STATUS_SUCCESS) {
		pixman_region_destroy (gstate->clip.region);
		gstate->clip.region = intersection;
	    } else {		
		status = CAIRO_STATUS_NO_MEMORY;
	    }
	    pixman_region_destroy (region);
	}
	    
	if (CAIRO_OK (status))
	    status = _cairo_surface_set_clip_region (gstate->surface, 
						     gstate->clip.region);
	
	if (status != CAIRO_INT_STATUS_UNSUPPORTED) {
	    _cairo_traps_fini (&traps);
	    return status;
	}

	/* Fall through as status == CAIRO_INT_STATUS_UNSUPPORTED
	   means that backend doesn't support clipping regions and
	   mask surface clipping should be used instead. */
    }

    /* Otherwise represent the clip as a mask surface. */

    if (gstate->clip.surface == NULL) {
	_cairo_traps_extents (&traps, &extents);
	_cairo_box_round_to_rectangle (&extents, &gstate->clip.rect);
	gstate->clip.surface =
	    _cairo_surface_create_similar_solid (gstate->surface,
						 CAIRO_FORMAT_A8,
						 gstate->clip.rect.width,
						 gstate->clip.rect.height,
						 CAIRO_COLOR_WHITE);
	if (gstate->clip.surface == NULL)
	    return CAIRO_STATUS_NO_MEMORY;
    }

    translate_traps (&traps, -gstate->clip.rect.x, -gstate->clip.rect.y);
    _cairo_pattern_init_solid (&pattern.solid, CAIRO_COLOR_WHITE);
    
    status = _cairo_surface_composite_trapezoids (CAIRO_OPERATOR_IN,
						  &pattern.base,
						  gstate->clip.surface,
						  0, 0,
						  0, 0,
						  gstate->clip.rect.width,
						  gstate->clip.rect.height,
						  traps.traps,
						  traps.num_traps);

    _cairo_pattern_fini (&pattern.base);
    
    _cairo_traps_fini (&traps);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_show_surface (cairo_gstate_t	*gstate,
			    cairo_surface_t	*surface,
			    int			width,
			    int			height)
{

    /* We are dealing with 6 coordinate spaces in this function. this makes
     * it ugly. 
     *
     * - "Image" space is the space of the surface we're reading pixels from.
     *   it is the surface argument to this function. The surface has a
     *   matrix attached to it which maps "user" space (see below) into
     *   image space.
     *
     * - "Device" space is the space of the surface we're ultimately writing
     *   pixels to. It is the current surface of the gstate argument to
     *   this function.
     * 
     * - "User" space is an arbitrary space defined by the user, defined 
     *   implicitly by the gstate's CTM. The CTM maps from user space to
     *   device space. The CTM inverse (which is also kept at all times)
     *   maps from device space to user space.
     *
     * - "Clip" space is the space of the surface being used to clip pixels
     *   during compositing. Space-wise, it is a bounding box (offset+size)
     *   within device space. This surface is usually smaller than the device
     *   surface (and possibly the image surface too) and logically occupies
     *   a bounding box around the "clip path", situated somewhere in device
     *   space. The clip path is already painted on the clip surface.
     *
     * - "Intermediate" space is the subset of the Clip space that the
     *   drawing will affect, and we allocate an intermediate surface
     *   of this size so that we can paint in it.
     *
     * - "Pattern" space is another arbitrary space defined in the pattern
     *   element of gstate. As pixels are read from image space, they are
     *   combined with pixels being read from pattern space and pixels
     *   already existing in device space. User coordinates are converted
     *   to pattern space, similarly, using a matrix attached to the pattern.
     *   (in fact, there is a 7th space in here, which is the space of the
     *   surface acting as a source for the pattern)
     *
     * To composite these spaces, we temporarily change the image surface 
     * so that it can be read and written in device coordinates; in a sense
     * this makes it "spatially compatible" with the clip and device spaces.
     *
     *
     * There is also some confusion about the interaction between a clip and
     * a pattern; it is assumed that in this "show surface" operation a pattern
     * is to be used as an auxiliary alpha mask. this might be wrong, but it's 
     * what we're doing now. 
     *
     * so, to follow the operations below, remember that in the compositing
     * model, each operation is always of the form ((src IN mask) OP dst).
     * that's the basic operation.
     *
     * so the compositing we are trying to do here, in general, involves 2
     * steps, going via a temporary surface:
     *
     *  - combining clip and pattern pixels together into a mask channel.
     *    this will be ((pattern IN clip) SRC temporary). it ignores the
     *    pixels already in the temporary, overwriting it with the
     *    pattern, clipped to the clip mask.
     *
     *  - combining temporary and "image" pixels with "device" pixels,
     *    with a user-provided porter/duff operator. this will be
     *    ((image IN temporary) OP device).
     *
     * if there is no clip, the degenerate case is just the second step
     * with pattern standing in for temporary.
     *
     */

    cairo_status_t status = CAIRO_STATUS_SUCCESS;
    cairo_matrix_t image_to_user, image_to_device, image_to_backend;
    double backend_x, backend_y;
    double backend_width, backend_height;
    cairo_surface_pattern_t pattern;
    cairo_box_t pattern_extents;
    cairo_rectangle_t extents;
    double origin_x, origin_y;
    int src_x, src_y;
        
    cairo_surface_get_matrix (surface, &image_to_user);
    cairo_matrix_invert (&image_to_user);
    cairo_matrix_multiply (&image_to_device, &image_to_user, &gstate->ctm);
    if (gstate->surface) {
	cairo_matrix_t device_to_backend;
	
	cairo_matrix_init_translate (&device_to_backend,
				     gstate->surface->device_x_offset,
				     gstate->surface->device_y_offset);
	cairo_matrix_multiply (&image_to_backend, &image_to_device, &device_to_backend);
    } else {
	image_to_backend = image_to_device;
    }

    _cairo_gstate_get_current_point (gstate, &backend_x, &backend_y);
    backend_width = width;
    backend_height = height;
    _cairo_matrix_transform_bounding_box (&image_to_backend,
					  &backend_x, &backend_y,
					  &backend_width, &backend_height);

    _cairo_pattern_init_for_surface (&pattern, surface);

    /* inherit surface attributes while surface attribute functions still
       exist */
    pattern.base.matrix = surface->matrix;
    pattern.base.filter = surface->filter;
    if (surface->repeat)
	pattern.base.extend = CAIRO_EXTEND_REPEAT;
    else
	pattern.base.extend = CAIRO_EXTEND_NONE;
    
    _cairo_gstate_pattern_transform (gstate, &pattern.base);

    pattern_extents.p1.x = _cairo_fixed_from_double (backend_x);
    pattern_extents.p1.y = _cairo_fixed_from_double (backend_y);
    pattern_extents.p2.x = _cairo_fixed_from_double (backend_x + backend_width);
    pattern_extents.p2.y = _cairo_fixed_from_double (backend_y + backend_height);
    _cairo_box_round_to_rectangle (&pattern_extents, &extents);

    /* XXX: This offset here isn't very clean, and it isn't even doing
     * a perfect job, (there are some rounding issues with
     * cairo_show_surface under the influence of cairo_scale). But it
     * does address the bug demonstrated in test/translate-show-surface.
     * And, this whole cairo_show_surface thing is going to be
     * disappearing soon anyway. */
    origin_x = origin_y = 0.0;
    _cairo_gstate_user_to_device (gstate, &origin_x, &origin_y);
    src_x = (int) (origin_x + 0.5);
    src_y = (int) (origin_y + 0.5);

    if (gstate->clip.surface)
    {
	_cairo_rectangle_intersect (&extents, &gstate->clip.rect);

	/* We only need to composite if the rectangle is not empty. */
	if (!_cairo_rectangle_empty (&extents)) {
	    cairo_surface_pattern_t clip_pattern;
 
 	    _cairo_pattern_init_for_surface (&clip_pattern,
					     gstate->clip.surface);
 
	    status = _cairo_surface_composite (gstate->operator,
					       &pattern.base, 
					       &clip_pattern.base,
					       gstate->surface,
					       src_x, src_y,
					       0, 0,
					       extents.x, extents.y,
					       extents.width, extents.height);

	    _cairo_pattern_fini (&clip_pattern.base);
	}
    }
    else
    {
	/* XXX: The rendered size is sometimes 1 or 2 pixels short
	 * from what I expect. Need to fix this.
	 * KRH: I'm guessing this was due to rounding error when
	 * passing double coordinates for integer arguments.  Using
	 * the extents rectangle should fix this, since it's properly
	 * rounded.  Is this still the case?
	 */
	status = _cairo_surface_composite (gstate->operator,
					   &pattern.base, 
					   NULL,
					   gstate->surface,
					   src_x, src_y,
					   0, 0,
					   extents.x, extents.y,
					   extents.width, extents.height);

    }

    _cairo_pattern_fini (&pattern.base);
    
    return status;
}

static void
_cairo_gstate_unset_font (cairo_gstate_t *gstate)
{
    if (gstate->scaled_font) {
	cairo_scaled_font_destroy (gstate->scaled_font);
	gstate->scaled_font = NULL;
    }
}

cairo_status_t
_cairo_gstate_select_font_face (cairo_gstate_t       *gstate, 
				const char           *family, 
				cairo_font_slant_t    slant, 
				cairo_font_weight_t   weight)
{
    cairo_font_face_t *font_face;

    font_face = _cairo_simple_font_face_create (family, slant, weight);
    if (!font_face)
	return CAIRO_STATUS_NO_MEMORY;

    _cairo_gstate_set_font_face (gstate, font_face);
    cairo_font_face_destroy (font_face);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_set_font_size (cairo_gstate_t *gstate, 
			     double          size)
{
    _cairo_gstate_unset_font (gstate);

    cairo_matrix_init_scale (&gstate->font_matrix, size, size);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_set_font_matrix (cairo_gstate_t *gstate, 
			       cairo_matrix_t *matrix)
{
    _cairo_gstate_unset_font (gstate);

    gstate->font_matrix = *matrix;

    return CAIRO_STATUS_SUCCESS;
}

cairo_matrix_t
_cairo_gstate_get_font_matrix (cairo_gstate_t *gstate)
{
    return gstate->font_matrix;
}

cairo_status_t
_cairo_gstate_get_font_face (cairo_gstate_t     *gstate,
			     cairo_font_face_t **font_face)
{
    cairo_status_t status;

    status = _cairo_gstate_ensure_font_face (gstate);
    if (status)
	return status;
    
    *font_face = gstate->font_face;

    return CAIRO_STATUS_SUCCESS;
}

/* 
 * Like everything else in this file, fonts involve Too Many Coordinate Spaces;
 * it is easy to get confused about what's going on.
 *
 * The user's view
 * ---------------
 *
 * Users ask for things in user space. When cairo starts, a user space unit
 * is about 1/96 inch, which is similar to (but importantly different from)
 * the normal "point" units most users think in terms of. When a user
 * selects a font, its scale is set to "one user unit". The user can then
 * independently scale the user coordinate system *or* the font matrix, in
 * order to adjust the rendered size of the font.
 *
 * Metrics are returned in user space, whether they are obtained from
 * the currently selected font in a  #cairo_t or from a #cairo_scaled_font_t
 * which is aa font specialized to a particular scale matrix, CTM, and target
 * surface. 
 *
 * The font's view
 * ---------------
 *
 * Fonts are designed and stored (in say .ttf files) in "font space", which
 * describes an "EM Square" (a design tile) and has some abstract number
 * such as 1000, 1024, or 2048 units per "EM". This is basically an
 * uninteresting space for us, but we need to remember that it exists.
 *
 * Font resources (from libraries or operating systems) render themselves
 * to a particular device. Since they do not want to make most programmers
 * worry about the font design space, the scaling API is simplified to
 * involve just telling the font the required pixel size of the EM square
 * (that is, in device space).
 *
 *
 * Cairo's gstate view
 * -------------------
 *
 * In addition to the CTM and CTM inverse, we keep a matrix in the gstate
 * called the "font matrix" which describes the user's most recent
 * font-scaling or font-transforming request. This is kept in terms of an
 * abstract scale factor, composed with the CTM and used to set the font's
 * pixel size. So if the user asks to "scale the font by 12", the matrix
 * is:
 *
 *   [ 12.0, 0.0, 0.0, 12.0, 0.0, 0.0 ]
 *
 * It is an affine matrix, like all cairo matrices, but its tx and ty
 * components are always set to zero; we don't permit "nudging" fonts
 * around.
 *
 * In order to perform any action on a font, we must build an object
 * called a cairo_font_scale_t; this contains the central 2x2 matrix 
 * resulting from "font matrix * CTM".
 *  
 * We pass this to the font when making requests of it, which causes it to
 * reply for a particular [user request, device] combination, under the CTM
 * (to accomodate the "zoom in" == "bigger fonts" issue above).
 *
 * The other terms in our communication with the font are therefore in
 * device space. When we ask it to perform text->glyph conversion, it will
 * produce a glyph string in device space. Glyph vectors we pass to it for
 * measuring or rendering should be in device space. The metrics which we
 * get back from the font will be in device space. The contents of the
 * global glyph image cache will be in device space.
 *
 *
 * Cairo's public view
 * -------------------
 *
 * Since the values entering and leaving via public API calls are in user
 * space, the gstate functions typically need to multiply argumens by the
 * CTM (for user-input glyph vectors), and return values by the CTM inverse
 * (for font responses such as metrics or glyph vectors).
 *
 */

static cairo_status_t
_cairo_gstate_ensure_font_face (cairo_gstate_t *gstate)
{
    if (!gstate->font_face) {
	gstate->font_face = _cairo_simple_font_face_create (CAIRO_FONT_FAMILY_DEFAULT,
							    CAIRO_FONT_SLANT_DEFAULT,
							    CAIRO_FONT_WEIGHT_DEFAULT);
	if (!gstate->font_face)
	    return CAIRO_STATUS_NO_MEMORY;
    }
    
    return CAIRO_STATUS_SUCCESS;
}
    
static cairo_status_t
_cairo_gstate_ensure_font (cairo_gstate_t *gstate)
{
    cairo_status_t status;
    
    if (gstate->scaled_font)
	return CAIRO_STATUS_SUCCESS;
    
    status = _cairo_gstate_ensure_font_face (gstate);
    if (status)
	return status;

    gstate->scaled_font = cairo_scaled_font_create (gstate->font_face,
						    &gstate->font_matrix,
						    &gstate->ctm);
    
    if (!gstate->scaled_font)
	return CAIRO_STATUS_NO_MEMORY;

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_get_font_extents (cairo_gstate_t *gstate, 
				cairo_font_extents_t *extents)
{
    cairo_status_t status = _cairo_gstate_ensure_font (gstate);
    if (status)
	return status;

    return cairo_scaled_font_extents (gstate->scaled_font, extents);
}

cairo_status_t
_cairo_gstate_text_to_glyphs (cairo_gstate_t *gstate, 
			      const char     *utf8,
			      cairo_glyph_t **glyphs,
			      int	     *num_glyphs)
{
    cairo_status_t status;

    cairo_point_t point; 
    double origin_x, origin_y;
    int i;

    status = _cairo_gstate_ensure_font (gstate);
    if (status)
	return status;
    
    status = _cairo_path_fixed_get_current_point (&gstate->path, &point);
    if (status == CAIRO_STATUS_NO_CURRENT_POINT) {
	origin_x = 0.0;
	origin_y = 0.0;
    } else {
	origin_x = _cairo_fixed_to_double (point.x);
	origin_y = _cairo_fixed_to_double (point.y);
	_cairo_gstate_backend_to_user (gstate,
				       &origin_x, &origin_y);
    }

    status = _cairo_scaled_font_text_to_glyphs (gstate->scaled_font, 
						utf8, glyphs, num_glyphs);

    if (status || !glyphs || !num_glyphs || !(*glyphs) || !(num_glyphs))
	return status;

    /* The font responded in glyph space, starting from (0,0).  Convert to
       user space by applying the font transform, then add any current point
       offset. */

    for (i = 0; i < *num_glyphs; ++i) {
	cairo_matrix_transform_point (&gstate->font_matrix, 
				      &((*glyphs)[i].x),
				      &((*glyphs)[i].y));
	(*glyphs)[i].x += origin_x;
	(*glyphs)[i].y += origin_y;
    }
    
    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_set_font_face (cairo_gstate_t    *gstate, 
			     cairo_font_face_t *font_face)
{
    if (font_face != gstate->font_face) {
	if (gstate->font_face)
	    cairo_font_face_destroy (gstate->font_face);
	gstate->font_face = font_face;
	if (gstate->font_face)
	    cairo_font_face_reference (gstate->font_face);
    }

    _cairo_gstate_unset_font (gstate);
    
    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_glyph_extents (cairo_gstate_t *gstate,
			     cairo_glyph_t *glyphs, 
			     int num_glyphs,
			     cairo_text_extents_t *extents)
{
    cairo_status_t status;

    status = _cairo_gstate_ensure_font (gstate);
    if (status)
	return status;

    cairo_scaled_font_glyph_extents (gstate->scaled_font,
				     glyphs, num_glyphs,
				     extents);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_show_glyphs (cairo_gstate_t *gstate, 
			   cairo_glyph_t *glyphs, 
			   int num_glyphs)
{
    cairo_status_t status;
    int i;
    cairo_glyph_t *transformed_glyphs = NULL;
    cairo_pattern_union_t pattern;
    cairo_box_t bbox;
    cairo_rectangle_t extents;

    status = _cairo_gstate_ensure_font (gstate);
    if (status)
	return status;
    
    transformed_glyphs = malloc (num_glyphs * sizeof(cairo_glyph_t));
    if (transformed_glyphs == NULL)
	return CAIRO_STATUS_NO_MEMORY;
    
    for (i = 0; i < num_glyphs; ++i)
    {
	transformed_glyphs[i] = glyphs[i];
	_cairo_gstate_user_to_backend (gstate,
				       &transformed_glyphs[i].x, 
				       &transformed_glyphs[i].y);
    }
    
    status = _cairo_scaled_font_glyph_bbox (gstate->scaled_font,
					    transformed_glyphs, num_glyphs, 
					    &bbox);
    _cairo_box_round_to_rectangle (&bbox, &extents);

    if (status)
	goto CLEANUP_GLYPHS;

    if (gstate->clip.surface)
    {
	cairo_surface_t *intermediate;
	cairo_surface_pattern_t intermediate_pattern;
	
	_cairo_rectangle_intersect (&extents, &gstate->clip.rect);

	/* Shortcut if empty */
	if (_cairo_rectangle_empty (&extents)) {
	    status = CAIRO_STATUS_SUCCESS;
	    goto BAIL1;
	}
	
	intermediate = _cairo_surface_create_similar_solid (gstate->clip.surface,
							    CAIRO_FORMAT_A8,
							    extents.width,
							    extents.height,
							    CAIRO_COLOR_TRANSPARENT);
	if (intermediate == NULL) {
	    status = CAIRO_STATUS_NO_MEMORY;
	    goto BAIL1;
	}

	/* move the glyphs again, from dev space to intermediate space */
	for (i = 0; i < num_glyphs; ++i)
	{
	    transformed_glyphs[i].x -= extents.x;
	    transformed_glyphs[i].y -= extents.y;
	}

	_cairo_pattern_init_solid (&pattern.solid, CAIRO_COLOR_WHITE);
    
	status = _cairo_scaled_font_show_glyphs (gstate->scaled_font, 
						 CAIRO_OPERATOR_ADD, 
						 &pattern.base, intermediate,
						 extents.x, extents.y,
						 0, 0,
						 extents.width, extents.height,
						 transformed_glyphs, num_glyphs);
	
	_cairo_pattern_fini (&pattern.base);

	if (status)
	    goto BAIL2;

	_cairo_pattern_init_for_surface (&pattern.surface,
					 gstate->clip.surface);
    
	status = _cairo_surface_composite (CAIRO_OPERATOR_IN,
					   &pattern.base,
					   NULL,
					   intermediate,
					   extents.x - gstate->clip.rect.x,
					   extents.y - gstate->clip.rect.y, 
					   0, 0,
					   0, 0,
					   extents.width, extents.height);

	_cairo_pattern_fini (&pattern.base);

	if (status)
	    goto BAIL2;

	_cairo_pattern_init_for_surface (&intermediate_pattern, intermediate);
	_cairo_pattern_init_copy (&pattern.base, gstate->source);
	_cairo_gstate_pattern_transform (gstate, &pattern.base);
    
	status = _cairo_surface_composite (gstate->operator,
					   &pattern.base,
					   &intermediate_pattern.base,
					   gstate->surface,
					   extents.x, extents.y, 
					   0, 0,
					   extents.x, extents.y,
					   extents.width, extents.height);
	_cairo_pattern_fini (&pattern.base);
	_cairo_pattern_fini (&intermediate_pattern.base);

    BAIL2:
	cairo_surface_destroy (intermediate);
    BAIL1:
	;
    }
    else
    {
	_cairo_pattern_init_copy (&pattern.base, gstate->source);
	_cairo_gstate_pattern_transform (gstate, &pattern.base);

	status = _cairo_scaled_font_show_glyphs (gstate->scaled_font, 
						 gstate->operator, &pattern.base,
						 gstate->surface,
						 extents.x, extents.y,
						 extents.x, extents.y,
						 extents.width, extents.height,
						 transformed_glyphs, num_glyphs);

	_cairo_pattern_fini (&pattern.base);
    }
    
 CLEANUP_GLYPHS:
    free (transformed_glyphs);
    
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

    transformed_glyphs = malloc (num_glyphs * sizeof(cairo_glyph_t));
    if (transformed_glyphs == NULL)
	return CAIRO_STATUS_NO_MEMORY;
    
    for (i = 0; i < num_glyphs; ++i)
    {
	transformed_glyphs[i] = glyphs[i];
	_cairo_gstate_user_to_backend (gstate,
				       &(transformed_glyphs[i].x), 
				       &(transformed_glyphs[i].y));
    }

    status = _cairo_scaled_font_glyph_path (gstate->scaled_font,
					    transformed_glyphs, num_glyphs,
					    &gstate->path);

    free (transformed_glyphs);
    return status;
}
