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
 * Author: David Reveman <c99drn@cs.umu.se>
 */

#include "cairoint.h"

void
_cairo_pattern_init (cairo_pattern_t *pattern)
{
    pattern->ref_count = 1;

    pattern->extend = CAIRO_EXTEND_DEFAULT;
    pattern->filter = CAIRO_FILTER_DEFAULT;

    _cairo_color_init (&pattern->color);

    _cairo_matrix_init (&pattern->matrix);

    pattern->stops = NULL;
    pattern->n_stops = 0;

    pattern->type = CAIRO_PATTERN_SOLID;

    pattern->source = NULL;
    pattern->source_offset.x = 0.0;
    pattern->source_offset.y = 0.0;
}

cairo_status_t
_cairo_pattern_init_copy (cairo_pattern_t *pattern, cairo_pattern_t *other)
{
    *pattern = *other;

    pattern->ref_count = 1;

    if (pattern->n_stops) {
	pattern->stops =
	    malloc (sizeof (cairo_color_stop_t) * pattern->n_stops);
	if (pattern->stops == NULL)
	    return CAIRO_STATUS_NO_MEMORY;
	memcpy (pattern->stops, other->stops,
		sizeof (cairo_color_stop_t) * other->n_stops);
    }

    if (pattern->source)
	cairo_surface_reference (other->source);

    if (pattern->type == CAIRO_PATTERN_SURFACE)
	cairo_surface_reference (other->u.surface.surface);
    
    return CAIRO_STATUS_SUCCESS;
}

void
_cairo_pattern_fini (cairo_pattern_t *pattern)
{
    if (pattern->n_stops)
	free (pattern->stops);
    
    if (pattern->type == CAIRO_PATTERN_SURFACE) {
	/* show_surface require us to restore surface matrix, repeat
	   attribute, filter type */
	if (pattern->source) {
	    cairo_surface_set_matrix (pattern->source,
				      &pattern->u.surface.save_matrix);
	    cairo_surface_set_repeat (pattern->source,
				      pattern->u.surface.save_repeat);
	    cairo_surface_set_filter (pattern->source,
				      pattern->u.surface.save_filter);
	}
	cairo_surface_destroy (pattern->u.surface.surface);
    }
    
    if (pattern->source)
	cairo_surface_destroy (pattern->source);
}

void
_cairo_pattern_init_solid (cairo_pattern_t *pattern,
			   double red, double green, double blue)
{
    _cairo_pattern_init (pattern);

    pattern->type = CAIRO_PATTERN_SOLID;
    _cairo_color_set_rgb (&pattern->color, red, green, blue);
}

cairo_pattern_t *
_cairo_pattern_create_solid (double red, double green, double blue)
{
    cairo_pattern_t *pattern;

    pattern = malloc (sizeof (cairo_pattern_t));
    if (pattern == NULL)
	return NULL;

    _cairo_pattern_init_solid (pattern, red, green, blue);

    return pattern;
}

cairo_pattern_t *
cairo_pattern_create_for_surface (cairo_surface_t *surface)
{
    cairo_pattern_t *pattern;

    pattern = malloc (sizeof (cairo_pattern_t));
    if (pattern == NULL)
	return NULL;

    _cairo_pattern_init (pattern);
    
    pattern->type = CAIRO_PATTERN_SURFACE;
    pattern->u.surface.surface = surface;
    cairo_surface_reference (surface);

    return pattern;
}

cairo_pattern_t *
cairo_pattern_create_linear (double x0, double y0, double x1, double y1)
{
    cairo_pattern_t *pattern;

    pattern = malloc (sizeof (cairo_pattern_t));
    if (pattern == NULL)
	return NULL;

    _cairo_pattern_init (pattern);

    pattern->type = CAIRO_PATTERN_LINEAR;
    pattern->u.linear.point0.x = x0;
    pattern->u.linear.point0.y = y0;
    pattern->u.linear.point1.x = x1;
    pattern->u.linear.point1.y = y1;

    return pattern;
}

cairo_pattern_t *
cairo_pattern_create_radial (double cx0, double cy0, double radius0,
			     double cx1, double cy1, double radius1)
{
    cairo_pattern_t *pattern;
    
    pattern = malloc (sizeof (cairo_pattern_t));
    if (pattern == NULL)
	return NULL;

    _cairo_pattern_init (pattern);
    
    pattern->type = CAIRO_PATTERN_RADIAL;
    pattern->u.radial.center0.x = cx0;
    pattern->u.radial.center0.y = cy0;
    pattern->u.radial.radius0.dx = radius0;
    pattern->u.radial.radius0.dy = radius0;
    pattern->u.radial.center1.x = cx1;
    pattern->u.radial.center1.y = cy1;
    pattern->u.radial.radius1.dx = radius1;
    pattern->u.radial.radius1.dy = radius1;

    return pattern;
}

void
cairo_pattern_reference (cairo_pattern_t *pattern)
{
    if (pattern == NULL)
	return;

    pattern->ref_count++;
}

void
cairo_pattern_destroy (cairo_pattern_t *pattern)
{
    if (pattern == NULL)
	return;

    pattern->ref_count--;
    if (pattern->ref_count)
	return;

    _cairo_pattern_fini (pattern);
    free (pattern);
}

static int
_cairo_pattern_stop_compare (const void *elem1, const void *elem2)
{
    return
        (((cairo_color_stop_t *) elem1)->offset ==
	 ((cairo_color_stop_t *) elem2)->offset) ?
        /* equal offsets, sort on id */
        ((((cairo_color_stop_t *) elem1)->id <
	  ((cairo_color_stop_t *) elem2)->id) ? -1 : 1) :
        /* sort on offset */
        ((((cairo_color_stop_t *) elem1)->offset <
	  ((cairo_color_stop_t *) elem2)->offset) ? -1 : 1);
}

cairo_status_t
cairo_pattern_add_color_stop (cairo_pattern_t *pattern,
			      double offset,
			      double red, double green, double blue,
			      double alpha)
{
    cairo_color_stop_t *stop;

    _cairo_restrict_value (&offset, 0.0, 1.0);
    _cairo_restrict_value (&red, 0.0, 1.0);
    _cairo_restrict_value (&green, 0.0, 1.0);
    _cairo_restrict_value (&blue, 0.0, 1.0);

    pattern->n_stops++;
    pattern->stops = realloc (pattern->stops,
			      sizeof (cairo_color_stop_t) * pattern->n_stops);
    if (pattern->stops == NULL) {
	pattern->n_stops = 0;
    
	return CAIRO_STATUS_NO_MEMORY;
    }

    stop = &pattern->stops[pattern->n_stops - 1];

    stop->offset = offset;
    stop->id = pattern->n_stops;
    _cairo_color_init (&stop->color);
    _cairo_color_set_rgb (&stop->color, red, green, blue);
    _cairo_color_set_alpha (&stop->color, alpha);
    stop->color_char[0] = stop->color.red_short / 256;
    stop->color_char[1] = stop->color.green_short / 256;
    stop->color_char[2] = stop->color.blue_short / 256;
    stop->color_char[3] = stop->color.alpha_short / 256;

    /* sort stops in ascending order */
    qsort (pattern->stops, pattern->n_stops, sizeof (cairo_color_stop_t),
	   _cairo_pattern_stop_compare);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
cairo_pattern_set_matrix (cairo_pattern_t *pattern, cairo_matrix_t *matrix)
{
    cairo_matrix_copy (&pattern->matrix, matrix);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
cairo_pattern_get_matrix (cairo_pattern_t *pattern, cairo_matrix_t *matrix)
{
    cairo_matrix_copy (matrix, &pattern->matrix);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
cairo_pattern_set_filter (cairo_pattern_t *pattern, cairo_filter_t filter)
{
    pattern->filter = filter;

    return CAIRO_STATUS_SUCCESS;
}

cairo_filter_t
cairo_pattern_get_filter (cairo_pattern_t *pattern)
{
    return pattern->filter;
}

cairo_status_t
cairo_pattern_set_extend (cairo_pattern_t *pattern, cairo_extend_t extend)
{
    pattern->extend = extend;

    return CAIRO_STATUS_SUCCESS;
}

cairo_extend_t
cairo_pattern_get_extend (cairo_pattern_t *pattern)
{
    return pattern->extend;
}

cairo_status_t
_cairo_pattern_get_rgb (cairo_pattern_t *pattern,
			double *red, double *green, double *blue)
{
    _cairo_color_get_rgb (&pattern->color, red, green, blue);

    return CAIRO_STATUS_SUCCESS;
}

void
_cairo_pattern_set_alpha (cairo_pattern_t *pattern, double alpha)
{
    int i;

    _cairo_color_set_alpha (&pattern->color, alpha);

    for (i = 0; i < pattern->n_stops; i++) {
	cairo_color_stop_t *stop = &pattern->stops[i];
    
	_cairo_color_set_alpha (&stop->color, stop->color.alpha * alpha);

	stop->color_char[0] = stop->color.red_short / 256;
	stop->color_char[1] = stop->color.green_short / 256;
	stop->color_char[2] = stop->color.blue_short / 256;
	stop->color_char[3] = stop->color.alpha_short / 256;
    }
}

void
_cairo_pattern_add_source_offset (cairo_pattern_t *pattern,
				  double x, double y)
{
    pattern->source_offset.x += x;
    pattern->source_offset.y += y;
}

void
_cairo_pattern_transform (cairo_pattern_t *pattern,
			  cairo_matrix_t *ctm,
			  cairo_matrix_t *ctm_inverse)
{
    cairo_matrix_t matrix;

    switch (pattern->type) {
    case CAIRO_PATTERN_SURFACE:
	/* hmm, maybe we should instead multiply with the inverse of the
	   pattern matrix here? */
	cairo_matrix_multiply (&pattern->matrix, ctm_inverse,
			       &pattern->matrix);
	break;
    case CAIRO_PATTERN_LINEAR:
	cairo_matrix_multiply (&matrix, &pattern->matrix, ctm);
	cairo_matrix_transform_point (&matrix,
				      &pattern->u.linear.point0.x,
				      &pattern->u.linear.point0.y);
	cairo_matrix_transform_point (&matrix,
				      &pattern->u.linear.point1.x,
				      &pattern->u.linear.point1.y);
	break;
    case CAIRO_PATTERN_RADIAL:
	cairo_matrix_multiply (&matrix, &pattern->matrix, ctm);
	cairo_matrix_transform_point (&matrix,
				      &pattern->u.radial.center0.x,
				      &pattern->u.radial.center0.y);
	cairo_matrix_transform_distance (&matrix,
					 &pattern->u.radial.radius0.dx,
					 &pattern->u.radial.radius0.dy);
	cairo_matrix_transform_point (&matrix,
				      &pattern->u.radial.center1.x,
				      &pattern->u.radial.center1.y);
	cairo_matrix_transform_distance (&matrix,
					 &pattern->u.radial.radius1.dx,
					 &pattern->u.radial.radius1.dy);
	break;
    case CAIRO_PATTERN_SOLID:
	break;
    }
}

void
_cairo_pattern_prepare_surface (cairo_pattern_t *pattern)
{
    cairo_matrix_t device_to_source;
    cairo_matrix_t user_to_source;
    
    /* should the surface matrix interface be remove from the API?
       for now we multiple the surface matrix with the pattern matrix */
    cairo_surface_get_matrix (pattern->u.surface.surface, &user_to_source);
    cairo_matrix_multiply (&device_to_source, &pattern->matrix,
			   &user_to_source);
    cairo_surface_set_matrix (pattern->source, &device_to_source);

    /* storing original surface matrix in pattern */
    pattern->u.surface.save_matrix = user_to_source;

    /* storing original surface repeat mode in pattern */
    pattern->u.surface.save_repeat = pattern->source->repeat;

    /* what do we do with extend types pad and reflect? */
    if (pattern->extend == CAIRO_EXTEND_REPEAT
	|| pattern->source->repeat == 1)
	cairo_surface_set_repeat (pattern->source, 1);
    else
	cairo_surface_set_repeat (pattern->source, 0);
    
    /* storing original surface filter in pattern */
    pattern->u.surface.save_filter =
        cairo_surface_get_filter (pattern->source);
    
    cairo_surface_set_filter (pattern->source, pattern->filter);
}

typedef void (*cairo_shader_function_t) (unsigned char *color0,
					 unsigned char *color1,
					 double factor,
					 unsigned char *result_color);

#define INTERPOLATE_COLOR_NEAREST(c1, c2, factor) \
  ((unsigned char) ((factor < 0.5)? c1: c2))

static void
_cairo_pattern_shader_nearest (unsigned char *color0,
			       unsigned char *color1,
			       double factor,
			       unsigned char *result_color)
{
    result_color[0] = INTERPOLATE_COLOR_NEAREST (color0[0], color1[0], factor);
    result_color[1] = INTERPOLATE_COLOR_NEAREST (color0[1], color1[1], factor);
    result_color[2] = INTERPOLATE_COLOR_NEAREST (color0[2], color1[2], factor);
    result_color[3] = INTERPOLATE_COLOR_NEAREST (color0[3], color1[3], factor);
}

#undef INTERPOLATE_COLOR_NEAREST

#define INTERPOLATE_COLOR_LINEAR(c1, c2, factor) \
  ((unsigned char) ((c2 * factor) + (c1 * (1.0 - factor))))

static void
_cairo_pattern_shader_linear (unsigned char *color0,
			      unsigned char *color1,
			      double factor,
			      unsigned char *result_color)
{
    result_color[0] = INTERPOLATE_COLOR_LINEAR (color0[0], color1[0], factor);
    result_color[1] = INTERPOLATE_COLOR_LINEAR (color0[1], color1[1], factor);
    result_color[2] = INTERPOLATE_COLOR_LINEAR (color0[2], color1[2], factor);
    result_color[3] = INTERPOLATE_COLOR_LINEAR (color0[3], color1[3], factor);
}

static void
_cairo_pattern_shader_gaussian (unsigned char *color0,
				unsigned char *color1,
				double factor,
				unsigned char *result_color)
{
    factor = (exp (factor * factor) - 1.0) / (M_E - 1.0);
    
    result_color[0] = INTERPOLATE_COLOR_LINEAR (color0[0], color1[0], factor);
    result_color[1] = INTERPOLATE_COLOR_LINEAR (color0[1], color1[1], factor);
    result_color[2] = INTERPOLATE_COLOR_LINEAR (color0[2], color1[2], factor);
    result_color[3] = INTERPOLATE_COLOR_LINEAR (color0[3], color1[3], factor);
}

#undef INTERPOLATE_COLOR_LINEAR

void
_cairo_pattern_calc_color_at_pixel (cairo_pattern_t *pattern,
				    double factor,
				    int *pixel)
{
    int p, colorstop;
    double factorscale;
    unsigned char result_color[4];
    cairo_shader_function_t shader_function;

    switch (pattern->filter) {
    case CAIRO_FILTER_FAST:
    case CAIRO_FILTER_NEAREST:
	shader_function = _cairo_pattern_shader_nearest;
	break;
    case CAIRO_FILTER_GAUSSIAN:
	shader_function = _cairo_pattern_shader_gaussian;
	break;
    case CAIRO_FILTER_GOOD:
    case CAIRO_FILTER_BEST:
    case CAIRO_FILTER_BILINEAR:
	shader_function = _cairo_pattern_shader_linear;
	break;
    }
    
    if (factor > 1.0 || factor < 0.0) {
	switch (pattern->extend) {
	case CAIRO_EXTEND_REPEAT:
	    factor -= floor (factor);
	    break;
	case CAIRO_EXTEND_REFLECT:
	    if (factor >= 0.0) {
		if (((int) factor) % 2)
		    factor = 1.0 - (factor - floor (factor));
		else
		    factor -= floor (factor);
	    } else {
		if (((int) factor) % 2)
		    factor -= floor (factor);
		else
		    factor = 1.0 - (factor - floor (factor));
	    }
	    break;
	case CAIRO_EXTEND_NONE:
	    break;
	}
    }
    
    if (factor < pattern->stops[0].offset)
	factor = pattern->stops[0].offset;

    if (factor > pattern->stops[pattern->n_stops - 1].offset)
	factor = pattern->stops[pattern->n_stops - 1].offset;

    for (colorstop = 0; colorstop < pattern->n_stops - 1; colorstop++) {
	if (factor <= pattern->stops[colorstop + 1].offset) {
	    factorscale = fabs (pattern->stops[colorstop].offset -
				pattern->stops[colorstop + 1].offset);
    
	    /* abrubt change, difference between two offsets == 0.0 */
	    if (factorscale == 0)
		break;
    
	    factor -= pattern->stops[colorstop].offset;
    
	    /* take offset as new 0 of coordinate system */
	    factor /= factorscale;

	    shader_function (pattern->stops[colorstop].color_char,
			     pattern->stops[colorstop + 1].color_char,
			     factor, result_color);

	    p = ((result_color[3] << 24) |
		 (result_color[0] << 16) |
		 (result_color[1] << 8) | (result_color[2] << 0));
	    *pixel = p;
	    break;
	}
    }
}

static void
_cairo_image_data_set_linear (cairo_pattern_t *pattern,
			      double offset_x,
			      double offset_y,
			      char *data,
			      int width,
			      int height)
{
    int x, y;
    cairo_point_double_t point0, point1, angle;
    double a, length, start, end;
    double factor;

    point0.x = pattern->u.linear.point0.x - offset_x;
    point0.y = pattern->u.linear.point0.y - offset_y;
    point1.x = pattern->u.linear.point1.x - offset_x;
    point1.y = pattern->u.linear.point1.y - offset_y;

    length = sqrt ((point1.x - point0.x) * (point1.x - point0.x) +
		   (point1.y - point0.y) * (point1.y - point0.y));
    length = (length) ? 1.0 / length : INT_MAX;

    a = -atan2 (point1.y - point0.y, point1.x - point0.x);
    angle.x = cos (a);
    angle.y = -sin (a);

    start = angle.x * point0.x;
    start += angle.y * point0.y;

    end = angle.x * point1.x;
    end += angle.y * point1.y;

    for (y = 0; y < height; y++) {
	for (x = 0; x < width; x++) {
    
	    factor = angle.x * (double) x;
	    factor += angle.y * (double) y;

	    factor = factor - start;
	    factor *= length;

	    _cairo_pattern_calc_color_at_pixel (pattern, factor, (int *)
						&data[y * width * 4 + x * 4]);
	}
    }
}

/* TODO: Inner circle is currently ignored. */
static void
_cairo_image_data_set_radial (cairo_pattern_t *pattern,
			      double offset_x,
			      double offset_y,
			      char *data,
			      int width,
			      int height)
{
    int x, y;
    cairo_point_double_t center1, pos;
    cairo_distance_double_t length;
    double factor;
    double min_length;

    center1.x = pattern->u.radial.center1.x - offset_x;
    center1.y = pattern->u.radial.center1.y - offset_y;

    min_length =
        fabs ((pattern->u.radial.radius1.dx < pattern->u.radial.radius1.dy) ?
	      pattern->u.radial.radius1.dx : pattern->u.radial.radius1.dy);
    
    /* ugly */
    if (min_length == 0.0)
	min_length = 0.000001;
    
    length.dx = min_length / pattern->u.radial.radius1.dx;
    length.dy = min_length / pattern->u.radial.radius1.dy;

    min_length = 1.0 / min_length;

    for (y = 0; y < height; y++) {
	for (x = 0; x < width; x++) {
	    pos.x = x - center1.x;
	    pos.y = y - center1.y;

	    pos.x *= length.dx;
	    pos.y *= length.dy;

	    factor = sqrt (pos.x * pos.x + pos.y * pos.y) * min_length;

	    _cairo_pattern_calc_color_at_pixel (pattern, factor, (int *)
						&data[y * width * 4 + x * 4]);
	}
    }
}

cairo_image_surface_t *
_cairo_pattern_get_image (cairo_pattern_t *pattern, cairo_box_t *box)
{
    cairo_surface_t *surface;

    switch (pattern->type) {
    case CAIRO_PATTERN_LINEAR:
    case CAIRO_PATTERN_RADIAL: {
	char *data;
	int width = ceil (_cairo_fixed_to_double (box->p2.x)) -
	    floor (_cairo_fixed_to_double (box->p1.x));
	int height = ceil (_cairo_fixed_to_double (box->p2.y)) -
	    floor (_cairo_fixed_to_double (box->p1.y));
	
	data = malloc (width * height * 4);
	if (!data)
	    return NULL;

	_cairo_pattern_add_source_offset (pattern,
					  floor (_cairo_fixed_to_double (box->p1.x)),
					  floor (_cairo_fixed_to_double (box->p1.y)));
    
	if (pattern->type == CAIRO_PATTERN_RADIAL)
	    _cairo_image_data_set_radial (pattern,
					  pattern->source_offset.x,
					  pattern->source_offset.y,
					  data, width, height);
	else
	    _cairo_image_data_set_linear (pattern,
					  pattern->source_offset.x,
					  pattern->source_offset.y,
					  data, width, height);

	surface = cairo_image_surface_create_for_data (data,
						       CAIRO_FORMAT_ARGB32,
						       width, height,
						       width * 4);
    
	if (surface)
	    _cairo_image_surface_assume_ownership_of_data (
		(cairo_image_surface_t *) surface);
    }
	break;
    case CAIRO_PATTERN_SOLID:
	surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 1, 1);
	if (surface) {
	    _cairo_surface_fill_rectangle (surface,
					   CAIRO_OPERATOR_SRC,
					   &pattern->color, 0, 0, 1, 1);
	    cairo_surface_set_repeat (surface, 1);
	}
	break;
    case CAIRO_PATTERN_SURFACE: {
	cairo_image_surface_t *image;

	image = _cairo_surface_get_image (pattern->u.surface.surface);
	if (image)
	    surface = &image->base;
	else
	    surface = NULL;
    
    }
	break;
    }
    
    return (cairo_image_surface_t *) surface;
}
