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

#include <stdlib.h>
#include <math.h>

#include "cairoint.h"

static cairo_status_t
_cairo_gstate_ensure_source (cairo_gstate_t *gstate);

static cairo_status_t
_cairo_gstate_clip_and_composite_trapezoids (cairo_gstate_t *gstate,
					     cairo_surface_t *src,
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

    _cairo_font_init (&gstate->font);

    gstate->surface = NULL;
    gstate->source = NULL;
    gstate->source_offset.x = 0.0;
    gstate->source_offset.y = 0.0;
    gstate->source_is_solid = 1;

    gstate->clip.surface = NULL;

    gstate->alpha = 1.0;
    _cairo_color_init (&gstate->color);

    /* 3780 PPM (~96DPI) is a good enough assumption until we get a surface */
    gstate->ppm = 3780;
    _cairo_gstate_default_matrix (gstate);

    _cairo_path_init (&gstate->path);

    _cairo_pen_init_empty (&gstate->pen_regular);

    gstate->next = NULL;
}

cairo_status_t
_cairo_gstate_init_copy (cairo_gstate_t *gstate, cairo_gstate_t *other)
{
    cairo_status_t status;
    
    *gstate = *other;
    if (other->dash) {
	gstate->dash = malloc (other->num_dashes * sizeof (double));
	if (gstate->dash == NULL)
	    return CAIRO_STATUS_NO_MEMORY;
	memcpy (gstate->dash, other->dash, other->num_dashes * sizeof (double));
    }
    
    status = _cairo_font_init_copy (&gstate->font, &other->font);
    if (status)
	goto CLEANUP_DASHES;

    cairo_surface_reference (gstate->surface);
    cairo_surface_reference (gstate->source);
    cairo_surface_reference (gstate->clip.surface);
    
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
    _cairo_font_fini (&gstate->font);
  CLEANUP_DASHES:
    free (gstate->dash);
    gstate->dash = NULL;

    return status;
}

void
_cairo_gstate_fini (cairo_gstate_t *gstate)
{
    _cairo_font_fini (&gstate->font);

    cairo_surface_destroy (gstate->surface);
    gstate->surface = NULL;

    cairo_surface_destroy (gstate->source);
    gstate->source = NULL;
    gstate->source_is_solid = 1;

    cairo_surface_destroy (gstate->clip.surface);
    gstate->clip.surface = NULL;

    _cairo_color_fini (&gstate->color);

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

cairo_gstate_t *
_cairo_gstate_copy (cairo_gstate_t *other)
{
    cairo_status_t status;
    cairo_gstate_t *gstate;

    gstate = malloc (sizeof (cairo_gstate_t));
    if (gstate) {
	status = _cairo_gstate_init_copy (gstate, other);
	if (status) {
	    free (gstate);
	    return NULL;
	}
    }
    gstate->next = NULL;

    return gstate;
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

    _cairo_surface_fill_rectangle (gstate->surface,
                                   CAIRO_OPERATOR_SRC,
				   &clear,
				   0, 0,
			           _cairo_surface_get_width (gstate->surface),
				   _cairo_surface_get_height (gstate->surface));

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

    cairo_surface_destroy (gstate->surface);

    gstate->surface = surface;
    cairo_surface_reference (gstate->surface);

    scale = surface->ppm / gstate->ppm;
    _cairo_gstate_scale (gstate, scale, scale);
    gstate->ppm = surface->ppm;

    return CAIRO_STATUS_SUCCESS;
}

/* XXX: Need to decide the memory mangement semantics of this
   function. Should it reference the surface again? */
cairo_surface_t *
_cairo_gstate_current_target_surface (cairo_gstate_t *gstate)
{
    if (gstate == NULL)
	return NULL;

    return gstate->surface;
}

cairo_path_t *
_cairo_gstate_current_path (cairo_gstate_t *gstate)
{
    cairo_path_t *path;

    if (gstate == NULL)
	return NULL;

    path = _cairo_path_copy (&gstate->path);
    if (path == NULL)
	return NULL;

    _cairo_path_set_ctm_inverse (path, &gstate->ctm_inverse);

    return path;
}

cairo_status_t
_cairo_gstate_set_pattern (cairo_gstate_t *gstate, cairo_surface_t *pattern)
{
    cairo_surface_destroy (gstate->source);

    gstate->source = pattern;
    gstate->source_is_solid = 0;

    cairo_surface_reference (gstate->source);

    _cairo_gstate_current_point (gstate,
				 &gstate->source_offset.x,
				 &gstate->source_offset.y);

    return CAIRO_STATUS_SUCCESS;
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
    _cairo_color_set_rgb (&gstate->color, red, green, blue);

    cairo_surface_destroy (gstate->source);

    gstate->source = NULL;
    gstate->source_offset.x = 0;
    gstate->source_offset.y = 0;
    gstate->source_is_solid = 1;

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_current_rgb_color (cairo_gstate_t *gstate, double *red, double *green, double *blue)
{
    _cairo_color_get_rgb (&gstate->color, red, green, blue);

    return CAIRO_STATUS_SUCCESS;
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

/* XXX: Need to fix this so it does the right thing after set_pattern. */
cairo_status_t
_cairo_gstate_set_alpha (cairo_gstate_t *gstate, double alpha)
{
    gstate->alpha = alpha;

    _cairo_color_set_alpha (&gstate->color, alpha);

    cairo_surface_destroy (gstate->source);

    gstate->source = NULL;
    gstate->source_offset.x = 0;
    gstate->source_offset.y = 0;

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

cairo_status_t
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
#define CAIRO_GSTATE_DEFAULT_PPM 3780.0

    int scale = gstate->ppm / CAIRO_GSTATE_DEFAULT_PPM + 0.5;
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
    cairo_status_t status;

    cairo_matrix_transform_point (&gstate->ctm, &x, &y);

    status = cairo_path_move_to (&gstate->path, x, y);
    if (status)
	return status;

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_line_to (cairo_gstate_t *gstate, double x, double y)
{
    cairo_status_t status;

    cairo_matrix_transform_point (&gstate->ctm, &x, &y);

    status = cairo_path_line_to (&gstate->path, x, y);
    if (status)
	return status;

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_curve_to (cairo_gstate_t *gstate,
			double x1, double y1,
			double x2, double y2,
			double x3, double y3)
{
    cairo_status_t status;

    cairo_matrix_transform_point (&gstate->ctm, &x1, &y1);
    cairo_matrix_transform_point (&gstate->ctm, &x2, &y2);
    cairo_matrix_transform_point (&gstate->ctm, &x3, &y3);

    status = cairo_path_curve_to (&gstate->path,
				  x1, y1,
				  x2, y2,
				  x3, y3);
    if (status)
	return status;

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_rel_move_to (cairo_gstate_t *gstate, double dx, double dy)
{
    cairo_matrix_transform_distance (&gstate->ctm, &dx, &dy);

    return cairo_path_rel_move_to (&gstate->path, dx, dy);
}

cairo_status_t
_cairo_gstate_rel_line_to (cairo_gstate_t *gstate, double dx, double dy)
{
    cairo_matrix_transform_distance (&gstate->ctm, &dx, &dy);

    return cairo_path_rel_line_to (&gstate->path, dx, dy);
}

cairo_status_t
_cairo_gstate_rel_curve_to (cairo_gstate_t *gstate,
			    double dx1, double dy1,
			    double dx2, double dy2,
			    double dx3, double dy3)
{
    cairo_matrix_transform_distance (&gstate->ctm, &dx1, &dy1);
    cairo_matrix_transform_distance (&gstate->ctm, &dx2, &dy2);
    cairo_matrix_transform_distance (&gstate->ctm, &dx3, &dy3);
    
    return cairo_path_rel_curve_to (&gstate->path,
				    dx1, dy1,
				    dx2, dy2,
				    dx3, dy3);
}

/* XXX: NYI 
cairo_status_t
_cairo_gstate_stroke_path (cairo_gstate_t *gstate)
{
    cairo_status_t status;

    return CAIRO_STATUS_SUCCESS;
}
*/

cairo_status_t
_cairo_gstate_close_path (cairo_gstate_t *gstate)
{
    return cairo_path_close_path (&gstate->path);
}

cairo_status_t
_cairo_gstate_set_path (cairo_gstate_t *gstate, cairo_path_t *path)
{
    cairo_status_t status;

    _cairo_path_fini (&gstate->path);

    status = _cairo_path_init_copy (&gstate->path, path);
    if (status)
	return status;

    _cairo_path_transform (&gstate->path, &gstate->ctm);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_current_point (cairo_gstate_t *gstate, double *x_ret, double *y_ret)
{
    cairo_status_t status;

    status = cairo_path_current_point (&gstate->path, x_ret, y_ret);
    if (status == CAIRO_STATUS_NO_CURRENT_POINT) {
	*x_ret = 0.0;
	*y_ret = 0.0;
	return CAIRO_STATUS_SUCCESS;
    }

    cairo_matrix_transform_point (&gstate->ctm_inverse, x_ret, y_ret);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_current_point_device (cairo_gstate_t *gstate, double *x_ret, double *y_ret)
{
    cairo_status_t status;

    status = cairo_path_current_point (&gstate->path, x_ret, y_ret);
    if (status == CAIRO_STATUS_NO_CURRENT_POINT) {
	*x_ret = 0.0;
	*y_ret = 0.0;
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_gstate_ensure_source (cairo_gstate_t *gstate)
{
    if (gstate->source)
	return CAIRO_STATUS_SUCCESS;

    if (gstate->surface == NULL)
	return CAIRO_STATUS_NO_TARGET_SURFACE;

    gstate->source = cairo_surface_create_similar_solid (gstate->surface,
							 CAIRO_FORMAT_ARGB32,
							 1, 1,
							 gstate->color.red,
							 gstate->color.green,
							 gstate->color.blue,
							 gstate->color.alpha);
    if (gstate->source == NULL)
	return CAIRO_STATUS_NO_MEMORY;

    cairo_surface_set_repeat (gstate->source, 1);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_stroke (cairo_gstate_t *gstate)
{
    cairo_status_t status;
    cairo_traps_t traps;
    cairo_matrix_t user_to_source, device_to_source;

    status = _cairo_gstate_ensure_source (gstate);
    if (status)
	return status;

    _cairo_pen_init (&gstate->pen_regular, gstate->line_width / 2.0, gstate);

    _cairo_traps_init (&traps);

    status = _cairo_path_stroke_to_traps (&gstate->path, gstate, &traps);
    if (status) {
	_cairo_traps_fini (&traps);
	return status;
    }

    if (! gstate->source_is_solid) {
	cairo_surface_get_matrix (gstate->source, &user_to_source);
	cairo_matrix_multiply (&device_to_source, &gstate->ctm_inverse, &user_to_source);
	cairo_surface_set_matrix (gstate->source, &device_to_source);
    }

    _cairo_gstate_clip_and_composite_trapezoids (gstate,
						 gstate->source,
						 gstate->operator,
						 gstate->surface,
						 &traps);
    
    /* restore the matrix originally in the source surface */
    if (! gstate->source_is_solid)
	cairo_surface_set_matrix (gstate->source, &user_to_source);

    _cairo_traps_fini (&traps);

    _cairo_gstate_new_path (gstate);

    return CAIRO_STATUS_SUCCESS;
}

/* Warning: This call modifies the coordinates of traps */
static cairo_status_t
_cairo_gstate_clip_and_composite_trapezoids (cairo_gstate_t *gstate,
					     cairo_surface_t *src,
					     cairo_operator_t operator,
					     cairo_surface_t *dst,
					     cairo_traps_t *traps)
{
    if (traps->num_traps == 0)
	return CAIRO_STATUS_SUCCESS;

    if (gstate->clip.surface) {
	cairo_fixed_t xoff, yoff;
	cairo_trapezoid_t *t;
	int i;

	cairo_surface_t *intermediate, *white;

	white = cairo_surface_create_similar_solid (gstate->surface, CAIRO_FORMAT_A8,
						    1, 1,
						    1.0, 1.0, 1.0, 1.0);
	cairo_surface_set_repeat (white, 1);

	intermediate = cairo_surface_create_similar_solid (gstate->clip.surface,
							   CAIRO_FORMAT_A8,
							   gstate->clip.width,
							   gstate->clip.height,
							   0.0, 0.0, 0.0, 0.0);

	/* Ugh. The cairo_composite/(Render) interface doesn't allow
           an offset for the trapezoids. Need to manually shift all
           the coordinates to align with the offset origin of the clip
           surface. */
	xoff = XDoubleToFixed (gstate->clip.x);
	yoff = XDoubleToFixed (gstate->clip.y);
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

	_cairo_surface_composite_trapezoids (CAIRO_OPERATOR_ADD,
					     white, intermediate,
					     0, 0,
					     traps->traps,
					     traps->num_traps);
	_cairo_surface_composite (CAIRO_OPERATOR_IN,
				  gstate->clip.surface,
				  NULL,
				  intermediate,
				  0, 0, 0, 0, 0, 0,
				  gstate->clip.width, gstate->clip.height);
	_cairo_surface_composite (operator,
				  src, intermediate, dst,
				  0, 0,
				  0, 0,
				  gstate->clip.x,
				  gstate->clip.y,
				  gstate->clip.width,
				  gstate->clip.height);
	cairo_surface_destroy (intermediate);
	cairo_surface_destroy (white);

    } else {
	int xoff, yoff;

	if (traps->traps[0].left.p1.y < traps->traps[0].left.p2.y) {
	    xoff = _cairo_fixed_to_double (traps->traps[0].left.p1.x);
	    yoff = _cairo_fixed_to_double (traps->traps[0].left.p1.y);
	} else {
	    xoff = _cairo_fixed_to_double (traps->traps[0].left.p2.x);
	    yoff = _cairo_fixed_to_double (traps->traps[0].left.p2.y);
	}

	_cairo_surface_composite_trapezoids (gstate->operator,
					     src, dst,
					     xoff - gstate->source_offset.x,
					     yoff - gstate->source_offset.y,
					     traps->traps,
					     traps->num_traps);
    }

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_fill (cairo_gstate_t *gstate)
{
    cairo_status_t status;
    cairo_traps_t traps;
    cairo_matrix_t user_to_source, device_to_source;

    status = _cairo_gstate_ensure_source (gstate);
    if (status)
	return status;

    _cairo_traps_init (&traps);

    status = _cairo_path_fill_to_traps (&gstate->path, gstate, &traps);
    if (status) {
	_cairo_traps_fini (&traps);
	return status;
    }

    if (! gstate->source_is_solid) {
	cairo_surface_get_matrix (gstate->source, &user_to_source);
	cairo_matrix_multiply (&device_to_source, &gstate->ctm_inverse, &user_to_source);
	cairo_surface_set_matrix (gstate->source, &device_to_source);
    }

    _cairo_gstate_clip_and_composite_trapezoids (gstate,
						 gstate->source,
						 gstate->operator,
						 gstate->surface,
						 &traps);

    /* restore the matrix originally in the source surface */
    if (! gstate->source_is_solid)
	cairo_surface_set_matrix (gstate->source, &user_to_source);

    _cairo_traps_fini (&traps);

    _cairo_gstate_new_path (gstate);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_clip (cairo_gstate_t *gstate)
{
    cairo_status_t status;
    cairo_surface_t *alpha_one;
    cairo_traps_t traps;

    if (gstate->clip.surface == NULL) {
	double x1, y1, x2, y2;
	_cairo_path_bounds (&gstate->path,
			    &x1, &y1, &x2, &y2);
	gstate->clip.x = floor (x1);
	gstate->clip.y = floor (y1);
	gstate->clip.width = ceil (x2 - gstate->clip.x);
	gstate->clip.height = ceil (y2 - gstate->clip.y);
	gstate->clip.surface = cairo_surface_create_similar_solid (gstate->surface,
								   CAIRO_FORMAT_A8,
								   gstate->clip.width,
								   gstate->clip.height,
								   1.0, 1.0, 1.0, 1.0);
    }

    alpha_one = cairo_surface_create_similar_solid (gstate->surface, CAIRO_FORMAT_A8,
						    1, 1,
						    0.0, 0.0, 0.0, 1.0);
    cairo_surface_set_repeat (alpha_one, 1);

    _cairo_traps_init (&traps);
    status = _cairo_path_fill_to_traps (&gstate->path, gstate, &traps);
    if (status) {
	_cairo_traps_fini (&traps);
	return status;
    }

    _cairo_gstate_clip_and_composite_trapezoids (gstate,
						 alpha_one,
						 CAIRO_OPERATOR_IN,
						 gstate->clip.surface,
						 &traps);

    _cairo_traps_fini (&traps);

    cairo_surface_destroy (alpha_one);

    return status;
}

cairo_status_t
_cairo_gstate_select_font (cairo_gstate_t *gstate, const char *key)
{
    return _cairo_font_select (&gstate->font, key);
}

cairo_status_t
_cairo_gstate_scale_font (cairo_gstate_t *gstate, double scale)
{
    return _cairo_font_scale (&gstate->font, scale);
}

cairo_status_t
_cairo_gstate_transform_font (cairo_gstate_t *gstate,
			      double a, double b,
			      double c, double d)
{
    return _cairo_font_transform (&gstate->font,
				  a, b, c, d);
}

cairo_status_t
_cairo_gstate_text_extents (cairo_gstate_t *gstate,
			    const unsigned char *utf8,
			    double *x, double *y,
			    double *width, double *height,
			    double *dx, double *dy)
{
    XftFont *xft_font;
    XGlyphInfo extents;

    if (gstate->surface->dpy == 0)
	return CAIRO_STATUS_SUCCESS;

    _cairo_font_resolve_xft_font (&gstate->font, gstate, &xft_font);

    /* XXX: Need to make a generic (non-Xft) backend for text. */
    XftTextExtentsUtf8 (gstate->surface->dpy,
			xft_font,
			utf8,
			strlen ((char *) utf8),
			&extents);

    /* XXX: What are the semantics of XftTextExtents? Specifically,
       what does it do with x/y? I think we actually need to use the
       gstate's current point in here somewhere. */
    *x = extents.x;
    *y = extents.y;
    *width = extents.width;
    *height = extents.height;
    *dx = extents.xOff;
    *dy = extents.yOff;

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_show_text (cairo_gstate_t *gstate, const unsigned char *utf8)
{
    cairo_status_t status;
    XftFont *xft_font;
    double x, y;
    cairo_matrix_t user_to_source, device_to_source;

    if (gstate->surface->dpy == 0)
	return CAIRO_STATUS_SUCCESS;

    _cairo_gstate_current_point_device (gstate, &x, &y);

    _cairo_font_resolve_xft_font (&gstate->font, gstate, &xft_font);

    status = _cairo_gstate_ensure_source (gstate);
    if (status)
	return status;

    /* XXX: This same source matrix manipulation code shows up in
       about 3 or 4 places. We should move that into a shared function
       or two. */
    if (! gstate->source_is_solid) {
	cairo_surface_get_matrix (gstate->source, &user_to_source);
	cairo_matrix_multiply (&device_to_source, &gstate->ctm_inverse, &user_to_source);
	cairo_surface_set_matrix (gstate->source, &device_to_source);
    }

    /* XXX: Need to make a generic (non-Xft) backend for text. */
    XftTextRenderUtf8 (gstate->surface->dpy,
		       gstate->operator,
		       gstate->source->picture,
		       xft_font,
		       gstate->surface->picture,
		       0, 0,
		       x, y,
		       utf8,
		       strlen ((char *) utf8));

    /* restore the matrix originally in the source surface */
    if (! gstate->source_is_solid)
	cairo_surface_set_matrix (gstate->source, &user_to_source);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_show_surface (cairo_gstate_t	*gstate,
			    cairo_surface_t	*surface,
			    int			width,
			    int			height)
{
    cairo_surface_t *mask;
    cairo_matrix_t user_to_image, image_to_user;
    cairo_matrix_t image_to_device, device_to_image;
    double device_x, device_y;
    double device_width, device_height;

    if (gstate->alpha != 1.0) {
	mask = cairo_surface_create_similar_solid (gstate->surface,
						   CAIRO_FORMAT_A8,
						   1, 1,
						   1.0, 1.0, 1.0,
						   gstate->alpha);
	if (mask == NULL)
	    return CAIRO_STATUS_NO_MEMORY;

	cairo_surface_set_repeat (mask, 1);
    } else {
	mask = NULL;
    }

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
    
    /* XXX: The rendered size is sometimes 1 or 2 pixels short from
       what I expect. Need to fix this. */
    _cairo_surface_composite (gstate->operator,
			      surface, mask, gstate->surface,
			      device_x, device_y,
			      0, 0,
			      device_x, device_y,
			      device_width,
			      device_height);

    if (mask)
	cairo_surface_destroy (mask);

    /* restore the matrix originally in the surface */
    cairo_surface_set_matrix (surface, &user_to_image);

    return CAIRO_STATUS_SUCCESS;
}
