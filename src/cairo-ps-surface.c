/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2003 University of Southern California
 * Copyright © 2005 Red Hat, Inc
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
 *	Kristian Høgsberg <krh@redhat.com>
 *	Keith Packard <keithp@keithp.com>
 */

#include "cairoint.h"
#include "cairo-ps.h"
#include "cairo-font-subset-private.h"
#include "cairo-paginated-surface-private.h"
#include "cairo-meta-surface-private.h"
#include "cairo-ft-private.h"

#include <time.h>
#include <zlib.h>

/* TODO:
 *
 * - Add document structure convention comments where appropriate.
 *
 * - Create a set of procs to use... specifically a trapezoid proc.
 */

static const cairo_surface_backend_t cairo_ps_surface_backend;

static void
_cairo_ps_set_paginated_mode (cairo_surface_t *target,
			      cairo_paginated_mode_t mode);

typedef struct cairo_ps_surface {
    cairo_surface_t base;

    /* PS-specific fields */
    cairo_output_stream_t *stream;

    double width;
    double height;
    double x_dpi;
    double y_dpi;

    cairo_bool_t need_start_page; 
    int num_pages;

    cairo_paginated_mode_t paginated_mode;

#if DONE_ADDING_FONTS_SUPPORT_BACK_AFTER_SWITCHING_TO_PAGINATED
    cairo_array_t fonts;
#endif
} cairo_ps_surface_t;

#define PS_SURFACE_DPI_DEFAULT 300.0

#if DONE_ADDING_FONTS_SUPPORT_BACK_AFTER_SWITCHING_TO_PAGINATED
static cairo_int_status_t
_cairo_ps_surface_write_font_subsets (cairo_ps_surface_t *surface);
#endif

static void
_cairo_ps_surface_emit_header (cairo_ps_surface_t *surface)
{
    time_t now;

    now = time (NULL);

    _cairo_output_stream_printf (surface->stream,
				 "%%!PS-Adobe-3.0\n"
				 "%%%%Creator: cairo (http://cairographics.org)\n"
				 "%%%%CreationDate: %s"
				 "%%%%Pages: (atend)\n"
				 "%%%%BoundingBox: %f %f %f %f\n",
				 ctime (&now),
				 0.0, 0.0, 
				 surface->width,
				 surface->height);

    _cairo_output_stream_printf (surface->stream,
				 "%%%%DocumentData: Clean7Bit\n"
				 "%%%%LanguageLevel: 2\n"
				 "%%%%Orientation: Portrait\n"
				 "%%%%EndComments\n");
}

static void
_cairo_ps_surface_emit_footer (cairo_ps_surface_t *surface)
{
    _cairo_output_stream_printf (surface->stream,
				 "%%%%Trailer\n"
				 "%%%%Pages: %d\n"
				 "%%%%EOF\n",
				 surface->num_pages);
}

static cairo_surface_t *
_cairo_ps_surface_create_for_stream_internal (cairo_output_stream_t *stream,
					      double		     width,
					      double		     height)
{
    cairo_ps_surface_t *surface;

    surface = malloc (sizeof (cairo_ps_surface_t));
    if (surface == NULL) {
	_cairo_error (CAIRO_STATUS_NO_MEMORY);
	return (cairo_surface_t*) &_cairo_surface_nil;
    }

    _cairo_surface_init (&surface->base, &cairo_ps_surface_backend);

    surface->stream = stream;

    surface->width  = width;
    surface->height = height;
    surface->x_dpi = PS_SURFACE_DPI_DEFAULT;
    surface->y_dpi = PS_SURFACE_DPI_DEFAULT;
    surface->paginated_mode = CAIRO_PAGINATED_MODE_ANALYZE;
#if DONE_ADDING_DEVICE_SCALE_SUPPORT_AFTER_SWITCHING_TO_PAGINATED
    surface->base.device_x_scale = surface->x_dpi / 72.0;
    surface->base.device_y_scale = surface->y_dpi / 72.0;
#endif

    surface->need_start_page = TRUE;
    surface->num_pages = 0;

#if DONE_ADDING_FONTS_SUPPORT_BACK_AFTER_SWITCHING_TO_PAGINATED
    _cairo_array_init (&surface->fonts, sizeof (cairo_font_subset_t *));
#endif

    _cairo_ps_surface_emit_header (surface);

    return _cairo_paginated_surface_create (&surface->base,
					    CAIRO_CONTENT_COLOR_ALPHA,
					    width, height,
					    _cairo_ps_set_paginated_mode);
}

/**
 * cairo_ps_surface_create:
 * @filename: a filename for the PS output (must be writable)
 * @width_in_points: width of the surface, in points (1 point == 1/72.0 inch)
 * @height_in_points: height of the surface, in points (1 point == 1/72.0 inch)
 * 
 * Creates a PostScript surface of the specified size in points to be
 * written to @filename.
 *
 * Return value: a pointer to the newly created surface. The caller
 * owns the surface and should call cairo_surface_destroy when done
 * with it.
 *
 * This function always returns a valid pointer, but it will return a
 * pointer to a "nil" surface if an error such as out of memory
 * occurs. You can use cairo_surface_status() to check for this.
 **/
cairo_surface_t *
cairo_ps_surface_create (const char		*filename,
			 double			 width_in_points,
			 double			 height_in_points)
{
    cairo_status_t status;
    cairo_output_stream_t *stream;

    stream = _cairo_output_stream_create_for_file (filename);
    status = _cairo_output_stream_get_status (stream);
    if (status) {
	_cairo_error (status);
	return (cairo_surface_t*) &_cairo_surface_nil;
    }

    return _cairo_ps_surface_create_for_stream_internal (stream,
							 width_in_points,
							 height_in_points);
}

/**
 * cairo_ps_surface_create_for_stream:
 * @write: a #cairo_write_func_t to accept the output data
 * @closure: the closure argument for @write
 * @width_in_points: width of the surface, in points (1 point == 1/72.0 inch)
 * @height_in_points: height of the surface, in points (1 point == 1/72.0 inch)
 * 
 * Creates a PostScript surface of the specified size in points to be
 * written incrementally to the stream represented by @write and
 * @closure.
 *
 * Return value: a pointer to the newly created surface. The caller
 * owns the surface and should call cairo_surface_destroy when done
 * with it.
 *
 * This function always returns a valid pointer, but it will return a
 * pointer to a "nil" surface if an error such as out of memory
 * occurs. You can use cairo_surface_status() to check for this.
 */
cairo_surface_t *
cairo_ps_surface_create_for_stream (cairo_write_func_t	write_func,
				    void	       *closure,
				    double		width_in_points,
				    double		height_in_points)
{
    cairo_status_t status;
    cairo_output_stream_t *stream;

    stream = _cairo_output_stream_create (write_func, NULL, closure);
    status = _cairo_output_stream_get_status (stream);
    if (status) {
	_cairo_error (status);
	return (cairo_surface_t*) &_cairo_surface_nil;
    }

    return _cairo_ps_surface_create_for_stream_internal (stream,
							 width_in_points,
							 height_in_points);
}

static cairo_bool_t
_cairo_surface_is_ps (cairo_surface_t *surface)
{
    return surface->backend == &cairo_ps_surface_backend;
}

/**
 * cairo_ps_surface_set_dpi:
 * @surface: a postscript cairo_surface_t
 * @x_dpi: horizontal dpi
 * @y_dpi: vertical dpi
 * 
 * Set the horizontal and vertical resolution for image fallbacks.
 * When the ps backend needs to fall back to image overlays, it will
 * use this resolution. These DPI values are not used for any other
 * purpose, (in particular, they do not have any bearing on the size
 * passed to cairo_ps_surface_create() nor on the CTM).
 **/
void
cairo_ps_surface_set_dpi (cairo_surface_t *surface,
			  double	   x_dpi,
			  double	   y_dpi)
{
    cairo_surface_t *target;
    cairo_ps_surface_t *ps_surface;

    if (! _cairo_surface_is_paginated (surface)) {
	_cairo_error (CAIRO_STATUS_SURFACE_TYPE_MISMATCH);
	return;
    }

    target = _cairo_paginated_surface_get_target (surface);

    if (! _cairo_surface_is_ps (surface)) {
	_cairo_error (CAIRO_STATUS_SURFACE_TYPE_MISMATCH);
	return;
    }

    ps_surface = (cairo_ps_surface_t *) target;

    ps_surface->x_dpi = x_dpi;    
    ps_surface->y_dpi = y_dpi;

#if DONE_ADDING_DEVICE_SCALE_SUPPORT_AFTER_SWITCHING_TO_PAGINATED
    ps_surface->base.device_x_scale = ps_surface->x_dpi / 72.0;
    ps_surface->base.device_y_scale = ps_surface->y_dpi / 72.0;
#endif
}

/* XXX */
static cairo_status_t
_cairo_ps_surface_finish (void *abstract_surface)
{
    cairo_ps_surface_t *surface = abstract_surface;

#if DONE_ADDING_FONTS_SUPPORT_BACK_AFTER_SWITCHING_TO_PAGINATED
    _cairo_ps_surface_write_font_subsets (surface);
#endif

    _cairo_ps_surface_emit_footer (surface);

#if DONE_ADDING_FONTS_SUPPORT_BACK_AFTER_SWITCHING_TO_PAGINATED
    for (i = 0; i < surface->fonts.num_elements; i++) {
	_cairo_array_copy_element (&surface->fonts, i, &subset);
	_cairo_font_subset_destroy (subset);
    }	
    _cairo_array_fini (&surface->fonts);
#endif

    _cairo_output_stream_destroy (surface->stream);

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_ps_surface_start_page (cairo_ps_surface_t *surface)
{
    _cairo_output_stream_printf (surface->stream,
				 "%%%%Page: %d\n",
				 ++surface->num_pages);


    _cairo_output_stream_printf (surface->stream,
				 "gsave %f %f translate %f %f scale \n",
				 0.0, surface->height,
				 1.0/surface->base.device_x_scale,
				 -1.0/surface->base.device_y_scale);

    surface->need_start_page = FALSE;
}

static void
_cairo_ps_surface_end_page (cairo_ps_surface_t *surface)
{
    _cairo_output_stream_printf (surface->stream,
				 "grestore\n");

    surface->need_start_page = TRUE;
}

static cairo_int_status_t
_cairo_ps_surface_copy_page (void *abstract_surface)
{
    cairo_ps_surface_t *surface = abstract_surface;

    _cairo_ps_surface_end_page (surface);

    _cairo_output_stream_printf (surface->stream, "copypage\n");

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_ps_surface_show_page (void *abstract_surface)
{
    cairo_ps_surface_t *surface = abstract_surface;

    _cairo_ps_surface_end_page (surface);

    _cairo_output_stream_printf (surface->stream, "showpage\n");

    surface->need_start_page = TRUE;

    return CAIRO_STATUS_SUCCESS;
}

#if DONE_ADDING_FONTS_SUPPORT_BACK_AFTER_SWITCHING_TO_PAGINATED
static cairo_font_subset_t *
_cairo_ps_surface_get_font (cairo_ps_surface_t  *surface,
			    cairo_scaled_font_t *scaled_font)
{
    cairo_status_t status;
    cairo_unscaled_font_t *unscaled_font;
    cairo_font_subset_t *subset;
    unsigned int num_fonts, i;

    /* XXX: Need to fix this to work with a general cairo_scaled_font_t. */
    if (! _cairo_scaled_font_is_ft (scaled_font))
	return NULL;

    /* XXX Why is this an ft specific function? */
    unscaled_font = _cairo_ft_scaled_font_get_unscaled_font (scaled_font);

    num_fonts = _cairo_array_num_elements (&surface->fonts);
    for (i = 0; i < num_fonts; i++) {
	_cairo_array_copy_element (&surface->fonts, i, &subset);
	if (subset->unscaled_font == unscaled_font)
	    return subset;
    }

    subset = _cairo_font_subset_create (unscaled_font);
    if (subset == NULL)
	return NULL;

    subset->font_id = surface->fonts.num_elements;

    status = _cairo_array_append (&surface->fonts, &subset);
    if (status) {
	_cairo_font_subset_destroy (subset);
	return NULL;
    }

    return subset;
}

static cairo_int_status_t
_cairo_ps_surface_write_type42_dict (cairo_ps_surface_t  *surface,
				     cairo_font_subset_t *subset)
{
    const char *data;
    unsigned long data_size;
    cairo_status_t status;
    int i;

    status = CAIRO_STATUS_SUCCESS;

    /* FIXME: Figure out document structure convention for fonts */

    _cairo_output_stream_printf (surface->stream,
				 "11 dict begin\n"
				 "/FontType 42 def\n"
				 "/FontName /f%d def\n"
				 "/PaintType 0 def\n"
				 "/FontMatrix [ 1 0 0 1 0 0 ] def\n"
				 "/FontBBox [ 0 0 0 0 ] def\n"
				 "/Encoding 256 array def\n"
				 "0 1 255 { Encoding exch /.notdef put } for\n",
				 subset->font_id);

    /* FIXME: Figure out how subset->x_max etc maps to the /FontBBox */

    for (i = 1; i < subset->num_glyphs; i++)
	_cairo_output_stream_printf (surface->stream,
				     "Encoding %d /g%d put\n", i, i);

    _cairo_output_stream_printf (surface->stream,
				 "/CharStrings %d dict dup begin\n"
				 "/.notdef 0 def\n",
				 subset->num_glyphs);

    for (i = 1; i < subset->num_glyphs; i++)
	_cairo_output_stream_printf (surface->stream,
				     "/g%d %d def\n", i, i);

    _cairo_output_stream_printf (surface->stream,
				 "end readonly def\n");

    status = _cairo_font_subset_generate (subset, &data, &data_size);

    /* FIXME: We need to break up fonts bigger than 64k so we don't
     * exceed string size limitation.  At glyph boundaries.  Stupid
     * postscript. */
    _cairo_output_stream_printf (surface->stream,
				 "/sfnts [<");

    _cairo_output_stream_write_hex_string (surface->stream, data, data_size);

    _cairo_output_stream_printf (surface->stream,
				 ">] def\n"
				 "FontName currentdict end definefont pop\n");

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_ps_surface_write_font_subsets (cairo_ps_surface_t *surface)
{
    cairo_font_subset_t *subset;
    int i;

    for (i = 0; i < surface->fonts.num_elements; i++) {
	_cairo_array_copy_element (&surface->fonts, i, &subset);
	_cairo_ps_surface_write_type42_dict (surface, subset);
    }

    return CAIRO_STATUS_SUCCESS;
}
#endif

static cairo_bool_t
color_is_gray (cairo_color_t *color)
{
    const double epsilon = 0.00001;

    return (fabs (color->red - color->green) < epsilon &&
	    fabs (color->red - color->blue) < epsilon);
}

static cairo_bool_t
color_is_opaque (const cairo_color_t *color)
{
    return color->alpha >= 0.999;
}

static cairo_bool_t
format_is_opaque (cairo_format_t format)
{
    switch (format) {
    case CAIRO_FORMAT_ARGB32:
	return FALSE;
    case CAIRO_FORMAT_RGB24:
	return TRUE;
    case CAIRO_FORMAT_A8:
	return FALSE;
    case CAIRO_FORMAT_A1:
	return TRUE;
    }
    return FALSE;
}

static cairo_bool_t
surface_is_opaque (const cairo_surface_t *surface)
{ 
    if (_cairo_surface_is_image (surface)) {
	const cairo_image_surface_t	*image_surface = (cairo_image_surface_t *) surface;

	return format_is_opaque (image_surface->format);
    }
    return TRUE;
}

static cairo_bool_t
gradient_is_opaque (const cairo_gradient_pattern_t *gradient)
{
    return FALSE;    /* XXX no gradient support */
#if 0
    int i;
    
    for (i = 0; i < gradient->n_stops; i++)
	if (!color_is_opaque (&gradient->stops[i].color))
	    return FALSE;
    return TRUE;
#endif
}

static cairo_bool_t
pattern_is_opaque (const cairo_pattern_t *abstract_pattern)
{
    const cairo_pattern_union_t *pattern;

    pattern = (cairo_pattern_union_t *) abstract_pattern;
    switch (pattern->base.type) {
    case CAIRO_PATTERN_TYPE_SOLID:
	return color_is_opaque (&pattern->solid.color);
    case CAIRO_PATTERN_TYPE_SURFACE:
	return surface_is_opaque (pattern->surface.surface);
    case CAIRO_PATTERN_TYPE_LINEAR:
    case CAIRO_PATTERN_TYPE_RADIAL:
	return gradient_is_opaque (&pattern->gradient.base);
    }	

    ASSERT_NOT_REACHED;
    return FALSE;
}

static cairo_bool_t
operator_always_opaque (cairo_operator_t op)
{
    switch (op) {
    case CAIRO_OPERATOR_CLEAR:
	return FALSE;

    case CAIRO_OPERATOR_SOURCE:
	return FALSE;
	
    case CAIRO_OPERATOR_OVER:
    case CAIRO_OPERATOR_IN:
    case CAIRO_OPERATOR_OUT:
    case CAIRO_OPERATOR_ATOP:
	return FALSE;

    case CAIRO_OPERATOR_DEST:
	return TRUE;
	
    case CAIRO_OPERATOR_DEST_OVER:
    case CAIRO_OPERATOR_DEST_IN:
    case CAIRO_OPERATOR_DEST_OUT:
    case CAIRO_OPERATOR_DEST_ATOP:
	return FALSE;

    case CAIRO_OPERATOR_XOR:
    case CAIRO_OPERATOR_ADD:
    case CAIRO_OPERATOR_SATURATE:
	return FALSE;
    }
    return FALSE;
}

static cairo_bool_t
operator_always_translucent (cairo_operator_t op)
{
    switch (op) {
    case CAIRO_OPERATOR_CLEAR:
	return TRUE;

    case CAIRO_OPERATOR_SOURCE:
	return FALSE;
	
    case CAIRO_OPERATOR_OVER:
    case CAIRO_OPERATOR_IN:
    case CAIRO_OPERATOR_OUT:
    case CAIRO_OPERATOR_ATOP:
	return FALSE;

    case CAIRO_OPERATOR_DEST:
	return FALSE;
	
    case CAIRO_OPERATOR_DEST_OVER:
    case CAIRO_OPERATOR_DEST_IN:
    case CAIRO_OPERATOR_DEST_OUT:
    case CAIRO_OPERATOR_DEST_ATOP:
	return FALSE;

    case CAIRO_OPERATOR_XOR:
    case CAIRO_OPERATOR_ADD:
    case CAIRO_OPERATOR_SATURATE:
	return TRUE;
    }
    return TRUE;
}

static cairo_bool_t
surface_pattern_supported (const cairo_surface_pattern_t *pattern)
{
    if (pattern->surface->backend->acquire_source_image != NULL)
	return TRUE;
    return FALSE;
}

static cairo_bool_t
pattern_supported (const cairo_pattern_t *pattern)
{
    if (pattern->type == CAIRO_PATTERN_TYPE_SOLID)
	return TRUE;
    if (pattern->type == CAIRO_PATTERN_TYPE_SURFACE)
	return surface_pattern_supported ((const cairo_surface_pattern_t *) pattern);
	
    return FALSE;
}

static cairo_int_status_t
operation_supported (cairo_ps_surface_t *surface,
		      cairo_operator_t op,
		      const cairo_pattern_t *pattern)
{
    /* As a special-case, (see all drawing operations below), we
     * optimize away any erasing where nothing has been drawn yet. */
    if (surface->need_start_page && op == CAIRO_OPERATOR_CLEAR)
	return TRUE;

    if (! pattern_supported (pattern))
	return FALSE;

    if (operator_always_opaque (op))
	return TRUE;
    if (operator_always_translucent (op))
	return FALSE;

    return pattern_is_opaque (pattern);
}

static cairo_int_status_t
_analyze_operation (cairo_ps_surface_t *surface,
		    cairo_operator_t op,
		    const cairo_pattern_t *pattern)
{
    if (operation_supported (surface, op, pattern))
	return CAIRO_STATUS_SUCCESS;
    else
	return CAIRO_INT_STATUS_UNSUPPORTED;
}

/* The "standard" implementation limit for PostScript string sizes is
 * 65535 characters (see PostScript Language Reference, Appendix
 * B). We go one short of that because we sometimes need two
 * characters in a string to represent a single ASCII85 byte, (for the
 * escape sequences "\\", "\(", and "\)") and we must not split these
 * across two strings. So we'd be in trouble if we went right to the
 * limit and one of these escape sequences just happened to land at
 * the end.
 */
#define STRING_ARRAY_MAX_STRING_SIZE (65535-1)
#define STRING_ARRAY_MAX_COLUMN	     72

typedef struct _string_array_stream {
    cairo_output_stream_t *output;
    int column;
    int string_size;
} string_array_stream_t;

static cairo_status_t
_string_array_stream_write (void		*closure,
			    const unsigned char	*data,
			    unsigned int	 length)
{
    string_array_stream_t *stream = closure;
    unsigned char c;
    const unsigned char backslash = '\\';

    if (length == 0)
	return CAIRO_STATUS_SUCCESS;

    while (length--) {
	if (stream->string_size == 0) {
	    _cairo_output_stream_printf (stream->output, "(");
	    stream->column++;
	}

	c = *data++;
	switch (c) {
	case '\\':
	case '(':
	case ')':
	    _cairo_output_stream_write (stream->output, &backslash, 1);
	    stream->column++;
	    stream->string_size++;
	    break;
	}
	_cairo_output_stream_write (stream->output, &c, 1);
	stream->column++;
	stream->string_size++;

	if (stream->string_size >= STRING_ARRAY_MAX_STRING_SIZE) {
	    _cairo_output_stream_printf (stream->output, ")\n");
	    stream->string_size = 0;
	    stream->column = 0;
	}
	if (stream->column >= STRING_ARRAY_MAX_COLUMN) {
	    _cairo_output_stream_printf (stream->output, "\n ");
	    stream->string_size += 2;
	    stream->column = 1;
	}
    }

    return _cairo_output_stream_get_status (stream->output);
}

static cairo_status_t
_string_array_stream_close (void *closure)
{
    cairo_status_t status;
    string_array_stream_t *stream = closure;

    _cairo_output_stream_printf (stream->output, ")\n");

    status = _cairo_output_stream_get_status (stream->output);

    free (stream);

    return status;
}

static cairo_output_stream_t *
_string_array_stream_create (cairo_output_stream_t *output)
{
    string_array_stream_t *stream;

    stream = malloc (sizeof (string_array_stream_t));
    if (stream == NULL)
	return (cairo_output_stream_t *) &cairo_output_stream_nil;

    stream->output = output;
    stream->column = 0;
    stream->string_size = 0;

    return _cairo_output_stream_create (_string_array_stream_write,
					_string_array_stream_close,
					stream);
}

/* PS Output - this section handles output of the parts of the meta
 * surface we can render natively in PS. */

static cairo_status_t
emit_image (cairo_ps_surface_t    *surface,
	    cairo_image_surface_t *image,
	    cairo_matrix_t	  *matrix,
	    char		  *name)
{
    cairo_status_t status;
    unsigned char *rgb, *compressed;
    unsigned long rgb_size, compressed_size;
    cairo_surface_t *opaque;
    cairo_image_surface_t *opaque_image;
    cairo_pattern_union_t pattern;
    cairo_matrix_t d2i;
    int x, y, i;
    cairo_output_stream_t *base85_stream, *string_array_stream;

    /* PostScript can not represent the alpha channel, so we blend the
       current image over a white RGB surface to eliminate it. */

    if (image->base.status)
	return image->base.status;

    if (image->format != CAIRO_FORMAT_RGB24) {
	opaque = cairo_image_surface_create (CAIRO_FORMAT_RGB24,
					     image->width,
					     image->height);
	if (opaque->status) {
	    status = CAIRO_STATUS_NO_MEMORY;
	    goto bail0;
	}
    
	_cairo_pattern_init_for_surface (&pattern.surface, &image->base);
    
	_cairo_surface_fill_rectangle (opaque,
				       CAIRO_OPERATOR_SOURCE,
				       CAIRO_COLOR_WHITE,
				       0, 0, image->width, image->height);

	_cairo_surface_composite (CAIRO_OPERATOR_OVER,
				  &pattern.base,
				  NULL,
				  opaque,
				  0, 0,
				  0, 0,
				  0, 0,
				  image->width,
				  image->height);
    
	_cairo_pattern_fini (&pattern.base);
	opaque_image = (cairo_image_surface_t *) opaque;
    } else {
	opaque = &image->base;
	opaque_image = image;
    }

    rgb_size = 3 * opaque_image->width * opaque_image->height;
    rgb = malloc (rgb_size);
    if (rgb == NULL) {
	status = CAIRO_STATUS_NO_MEMORY;
	goto bail1;
    }

    i = 0;
    for (y = 0; y < opaque_image->height; y++) {
	pixman_bits_t *pixel = (pixman_bits_t *) (opaque_image->data + y * opaque_image->stride);
	for (x = 0; x < opaque_image->width; x++, pixel++) {
	    rgb[i++] = (*pixel & 0x00ff0000) >> 16;
	    rgb[i++] = (*pixel & 0x0000ff00) >>  8;
	    rgb[i++] = (*pixel & 0x000000ff) >>  0;
	}
    }

    /* XXX: Should fix cairo-lzw to provide a stream-based interface
     * instead. */
    compressed_size = rgb_size;
    compressed = _cairo_lzw_compress (rgb, &compressed_size);
    if (compressed == NULL) {
	status = CAIRO_STATUS_NO_MEMORY;
	goto bail2;
    }

    /* First emit the image data as a base85-encoded string which will
     * be used as the data source for the image operator later. */
    _cairo_output_stream_printf (surface->stream,
				 "/%sData [\n", name);

    string_array_stream = _string_array_stream_create (surface->stream);
    base85_stream = _cairo_base85_stream_create (string_array_stream);

    _cairo_output_stream_write (base85_stream, compressed, compressed_size);

    _cairo_output_stream_destroy (base85_stream);
    _cairo_output_stream_destroy (string_array_stream);

    _cairo_output_stream_printf (surface->stream,
				 "] def\n");
    _cairo_output_stream_printf (surface->stream,
				 "/%sDataIndex 0 def\n", name);

    /* matrix transforms from user space to image space.  We need to
     * transform from device space to image space to compensate for
     * postscripts coordinate system. */
    cairo_matrix_init (&d2i, 1, 0, 0, 1, 0, 0);
    cairo_matrix_multiply (&d2i, &d2i, matrix);

    _cairo_output_stream_printf (surface->stream,
				 "/%s {\n"
				 "    /DeviceRGB setcolorspace\n"
				 "    <<\n"
				 "	/ImageType 1\n"
				 "	/Width %d\n"
				 "	/Height %d\n"
				 "	/BitsPerComponent 8\n"
				 "	/Decode [ 0 1 0 1 0 1 ]\n"
				 "	/DataSource {\n"
				 "	    %sData %sDataIndex get\n"
				 "	    /%sDataIndex %sDataIndex 1 add def\n"
				 "	    %sDataIndex %sData length 1 sub gt { /%sDataIndex 0 def } if\n"
				 "	} /ASCII85Decode filter /LZWDecode filter\n"
				 "	/ImageMatrix [ %f %f %f %f %f %f ]\n"
				 "    >>\n"
				 "    image\n"
				 "} def\n",
				 name,
				 opaque_image->width,
				 opaque_image->height,
				 name, name, name, name, name, name, name,
				 d2i.xx, d2i.yx,
				 d2i.xy, d2i.yy,
				 d2i.x0, d2i.y0);

    status = CAIRO_STATUS_SUCCESS;

    free (compressed);
 bail2:
    free (rgb);
 bail1:
    if (opaque_image != image)
	cairo_surface_destroy (opaque);
 bail0:
    return status;
}

static void
emit_solid_pattern (cairo_ps_surface_t *surface,
		    cairo_solid_pattern_t *pattern)
{
    if (color_is_gray (&pattern->color))
	_cairo_output_stream_printf (surface->stream,
				     "%f setgray\n",
				     pattern->color.red);
    else
	_cairo_output_stream_printf (surface->stream,
				     "%f %f %f setrgbcolor\n",
				     pattern->color.red,
				     pattern->color.green,
				     pattern->color.blue);
}

static void
emit_surface_pattern (cairo_ps_surface_t *surface,
		      cairo_surface_pattern_t *pattern)
{
    cairo_rectangle_t		extents;

    if (_cairo_surface_is_meta (pattern->surface)) {
	_cairo_output_stream_printf (surface->stream, "/MyPattern {\n");
	_cairo_meta_surface_replay (pattern->surface, &surface->base);
	extents.width = surface->width;
	extents.height = surface->height;
	_cairo_output_stream_printf (surface->stream, "} bind def\n");
    } else {
	cairo_image_surface_t	*image;
	void			*image_extra;
	cairo_status_t		status;

	status = _cairo_surface_acquire_source_image (pattern->surface,
						      &image,
						      &image_extra);
	_cairo_surface_get_extents (&image->base, &extents);
	assert (status == CAIRO_STATUS_SUCCESS);
	emit_image (surface, image, &pattern->base.matrix, "MyPattern");
	_cairo_surface_release_source_image (pattern->surface, image,
					     image_extra);
    }
    _cairo_output_stream_printf (surface->stream,
				 "<< /PatternType 1\n"
				 "   /PaintType 1\n"
				 "   /TilingType 1\n");
    _cairo_output_stream_printf (surface->stream,
				 "   /BBox [0 0 %d %d]\n",
				 extents.width, extents.height);
    _cairo_output_stream_printf (surface->stream,
				 "   /XStep %d /YStep %d\n",
				 extents.width, extents.height);
    _cairo_output_stream_printf (surface->stream,
				 "   /PaintProc { MyPattern } bind\n"
				 ">> matrix makepattern setpattern\n");
}

static void
emit_linear_pattern (cairo_ps_surface_t *surface,
		     cairo_linear_pattern_t *pattern)
{
    /* XXX: NYI */
}

static void
emit_radial_pattern (cairo_ps_surface_t *surface,
		     cairo_radial_pattern_t *pattern)
{
    /* XXX: NYI */
}

static void
emit_pattern (cairo_ps_surface_t *surface, cairo_pattern_t *pattern)
{
    /* FIXME: We should keep track of what pattern is currently set in
     * the postscript file and only emit code if we're setting a
     * different pattern. */

    switch (pattern->type) {
    case CAIRO_PATTERN_TYPE_SOLID:	
	emit_solid_pattern (surface, (cairo_solid_pattern_t *) pattern);
	break;

    case CAIRO_PATTERN_TYPE_SURFACE:
	emit_surface_pattern (surface, (cairo_surface_pattern_t *) pattern);
	break;

    case CAIRO_PATTERN_TYPE_LINEAR:
	emit_linear_pattern (surface, (cairo_linear_pattern_t *) pattern);
	break;

    case CAIRO_PATTERN_TYPE_RADIAL:
	emit_radial_pattern (surface, (cairo_radial_pattern_t *) pattern);
	break;	    
    }
}

typedef struct
{
    cairo_output_stream_t *output_stream;
    cairo_bool_t has_current_point;
} cairo_ps_surface_path_info_t;

static cairo_status_t
_cairo_ps_surface_path_move_to (void *closure, cairo_point_t *point)
{
    cairo_ps_surface_path_info_t *info = closure;

    _cairo_output_stream_printf (info->output_stream,
				 "%f %f moveto ",
				 _cairo_fixed_to_double (point->x),
				 _cairo_fixed_to_double (point->y));
    info->has_current_point = TRUE;
    
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_ps_surface_path_line_to (void *closure, cairo_point_t *point)
{
    cairo_ps_surface_path_info_t *info = closure;
    const char *ps_operator;

    if (info->has_current_point)
	ps_operator = "lineto";
    else
	ps_operator = "moveto";
    
    _cairo_output_stream_printf (info->output_stream,
				 "%f %f %s ",
				 _cairo_fixed_to_double (point->x),
				 _cairo_fixed_to_double (point->y),
				 ps_operator);
    info->has_current_point = TRUE;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_ps_surface_path_curve_to (void          *closure,
			  cairo_point_t *b,
			  cairo_point_t *c,
			  cairo_point_t *d)
{
    cairo_ps_surface_path_info_t *info = closure;

    _cairo_output_stream_printf (info->output_stream,
				 "%f %f %f %f %f %f curveto ",
				 _cairo_fixed_to_double (b->x),
				 _cairo_fixed_to_double (b->y),
				 _cairo_fixed_to_double (c->x),
				 _cairo_fixed_to_double (c->y),
				 _cairo_fixed_to_double (d->x),
				 _cairo_fixed_to_double (d->y));
    
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_ps_surface_path_close_path (void *closure)
{
    cairo_ps_surface_path_info_t *info = closure;
    
    _cairo_output_stream_printf (info->output_stream,
				 "closepath\n");
    info->has_current_point = FALSE;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_ps_surface_intersect_clip_path (void		   *abstract_surface,
				cairo_path_fixed_t *path,
				cairo_fill_rule_t   fill_rule,
				double		    tolerance,
				cairo_antialias_t   antialias)
{
    cairo_ps_surface_t *surface = abstract_surface;
    cairo_output_stream_t *stream = surface->stream;
    cairo_status_t status;
    cairo_ps_surface_path_info_t info;
    const char *ps_operator;

    if (surface->paginated_mode == CAIRO_PAGINATED_MODE_ANALYZE)
	return CAIRO_STATUS_SUCCESS;

    if (surface->need_start_page)
	_cairo_ps_surface_start_page (surface);

    _cairo_output_stream_printf (stream,
				 "%% _cairo_ps_surface_intersect_clip_path\n");

    if (path == NULL) {
	_cairo_output_stream_printf (stream, "initclip\n");
	return CAIRO_STATUS_SUCCESS;
    }

    info.output_stream = stream;
    info.has_current_point = FALSE;

    status = _cairo_path_fixed_interpret (path,
					  CAIRO_DIRECTION_FORWARD,
					  _cairo_ps_surface_path_move_to,
					  _cairo_ps_surface_path_line_to,
					  _cairo_ps_surface_path_curve_to,
					  _cairo_ps_surface_path_close_path,
					  &info);

    switch (fill_rule) {
    case CAIRO_FILL_RULE_WINDING:
	ps_operator = "clip";
	break;
    case CAIRO_FILL_RULE_EVEN_ODD:
	ps_operator = "eoclip";
	break;
    default:
	ASSERT_NOT_REACHED;
    }

    _cairo_output_stream_printf (stream,
				 "%s newpath\n",
				 ps_operator);

    return status;
}

static cairo_int_status_t
_cairo_ps_surface_get_extents (void		  *abstract_surface,
			       cairo_rectangle_t *rectangle)
{
    cairo_ps_surface_t *surface = abstract_surface;

    rectangle->x = 0;
    rectangle->y = 0;

    /* XXX: The conversion to integers here is pretty bogus, (not to
     * mention the aribitray limitation of width to a short(!). We
     * may need to come up with a better interface for get_extents.
     */
    rectangle->width  = (int) ceil (surface->width);
    rectangle->height = (int) ceil (surface->height);

    return CAIRO_STATUS_SUCCESS;
}

#if DONE_ADDING_FONTS_SUPPORT_BACK_AFTER_SWITCHING_TO_PAGINATED
static cairo_int_status_t
_cairo_ps_surface_old_show_glyphs (cairo_scaled_font_t	*scaled_font,
			    cairo_operator_t	 op,
			    cairo_pattern_t	*pattern,
			    void		*abstract_surface,
			    int			 source_x,
			    int			 source_y,
			    int			 dest_x,
			    int			 dest_y,
			    unsigned int	 width,
			    unsigned int	 height,
			    const cairo_glyph_t	*glyphs,
			    int			 num_glyphs)
{
    cairo_ps_surface_t *surface = abstract_surface;
    cairo_output_stream_t *stream = surface->stream;
    cairo_font_subset_t *subset;
    int i, subset_index;

    if (surface->fallback)
	return CAIRO_STATUS_SUCCESS;

    if (surface->need_start_page) {
	/* Optimize away erasing of nothing. */
	if (op == CAIRO_OPERATOR_CLEAR)
	    return CAIRO_STATUS_SUCCESS;
	_cairo_ps_surface_start_page (surface);
    }

    /* XXX: Need to fix this to work with a general cairo_scaled_font_t. */
    if (! _cairo_scaled_font_is_ft (scaled_font))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if (surface->fallback)
	return CAIRO_STATUS_SUCCESS;

    if (pattern_operation_needs_fallback (op, pattern))
	return _cairo_ps_surface_add_fallback_area (surface, dest_x, dest_y, width, height);

    _cairo_output_stream_printf (stream,
				 "%% _cairo_ps_surface_old_show_glyphs\n");

    emit_pattern (surface, pattern);

    /* FIXME: Need to optimize this so we only do this sequence if the
     * font isn't already set. */

    subset = _cairo_ps_surface_get_font (surface, scaled_font);
    _cairo_output_stream_printf (stream,
				 "/f%d findfont\n"
				 "[ %f %f %f %f 0 0 ] makefont\n"
				 "setfont\n",
				 subset->font_id,
				 scaled_font->scale.xx,
				 scaled_font->scale.yx,
				 scaled_font->scale.xy,
				 -scaled_font->scale.yy);

    /* FIXME: Need to optimize per glyph code.  Should detect when
     * glyphs share the same baseline and when the spacing corresponds
     * to the glyph widths. */

    for (i = 0; i < num_glyphs; i++) {
	subset_index = _cairo_font_subset_use_glyph (subset, glyphs[i].index);
	_cairo_output_stream_printf (stream,
				     "%f %f moveto (\\%o) show\n",
				     glyphs[i].x,
				     glyphs[i].y,
				     subset_index);
	
    }

    return CAIRO_STATUS_SUCCESS;
}
#endif

static cairo_int_status_t
_cairo_ps_surface_paint (void			*abstract_surface,
			 cairo_operator_t	 op,
			 cairo_pattern_t	*source)
{
    cairo_ps_surface_t *surface = abstract_surface;
    cairo_output_stream_t *stream = surface->stream;
    cairo_ps_surface_path_info_t info;

    if (surface->paginated_mode == CAIRO_PAGINATED_MODE_ANALYZE)
	return _analyze_operation (surface, op, source);

    /* XXX: It would be nice to be able to assert this condition
     * here. But, we actually allow one 'cheat' that is used when
     * painting the final image-based fallbacks. The final fallbacks
     * do have alpha which we support by blending with white. This is
     * possible only because there is nothing between the fallback
     * images and the paper, nor is anything painted above. */
    /*
    assert (pattern_operation_supported (op, source));
    */
    
    if (surface->need_start_page) {
	/* Optimize away erasing of nothing. */
	if (op == CAIRO_OPERATOR_CLEAR)
	    return CAIRO_STATUS_SUCCESS;
	_cairo_ps_surface_start_page (surface);
    }

    _cairo_output_stream_printf (stream,
				 "%% _cairo_ps_surface_paint\n");

    emit_pattern (surface, source);

    info.output_stream = stream;
    info.has_current_point = FALSE;

    _cairo_output_stream_printf (stream, "0 0 moveto\n");
    _cairo_output_stream_printf (stream, "%f 0 lineto\n", surface->width);
    _cairo_output_stream_printf (stream, "%f %f lineto\n",
				 surface->width, surface->height);
    _cairo_output_stream_printf (stream, "0 %f lineto\n", surface->height);
    _cairo_output_stream_printf (stream, "closepath fill\n");
    return CAIRO_STATUS_SUCCESS;
}

static int
_cairo_ps_line_cap (cairo_line_cap_t cap)
{
    switch (cap) {
    case CAIRO_LINE_CAP_BUTT:
	return 0;
    case CAIRO_LINE_CAP_ROUND:
	return 1;
    case CAIRO_LINE_CAP_SQUARE:
	return 2;
    default:
	ASSERT_NOT_REACHED;
	return 0;
    }
}

static int
_cairo_ps_line_join (cairo_line_join_t join)
{
    switch (join) {
    case CAIRO_LINE_JOIN_MITER:
	return 0;
    case CAIRO_LINE_JOIN_ROUND:
	return 1;
    case CAIRO_LINE_JOIN_BEVEL:
	return 2;
    default:
	ASSERT_NOT_REACHED;
	return 0;
    }
}

static cairo_int_status_t
_cairo_ps_surface_stroke (void			*abstract_surface,
			  cairo_operator_t	 op,
			  cairo_pattern_t	*source,
			  cairo_path_fixed_t	*path,
			  cairo_stroke_style_t	*style,
			  cairo_matrix_t	*ctm,
			  cairo_matrix_t	*ctm_inverse,
			  double		 tolerance,
			  cairo_antialias_t	 antialias)
{
    cairo_ps_surface_t *surface = abstract_surface;
    cairo_output_stream_t *stream = surface->stream;
    cairo_int_status_t status;
    cairo_ps_surface_path_info_t info;

    if (surface->paginated_mode == CAIRO_PAGINATED_MODE_ANALYZE)
	return _analyze_operation (surface, op, source);

    assert (operation_supported (surface, op, source));
    
    if (surface->need_start_page) {
	/* Optimize away erasing of nothing. */
	if (op == CAIRO_OPERATOR_CLEAR)
	    return CAIRO_STATUS_SUCCESS;
	_cairo_ps_surface_start_page (surface);
    }

    _cairo_output_stream_printf (stream,
				 "%% _cairo_ps_surface_stroke\n");

    emit_pattern (surface, source);

    
    info.output_stream = stream;
    info.has_current_point = FALSE;

    _cairo_output_stream_printf (stream,
				 "gsave\n");
    status = _cairo_path_fixed_interpret (path,
					  CAIRO_DIRECTION_FORWARD,
					  _cairo_ps_surface_path_move_to,
					  _cairo_ps_surface_path_line_to,
					  _cairo_ps_surface_path_curve_to,
					  _cairo_ps_surface_path_close_path,
					  &info);

    /*
     * Switch to user space to set line parameters
     */
    _cairo_output_stream_printf (stream,
				 "[%f %f %f %f 0 0] concat\n",
				 ctm->xx, ctm->yx, ctm->xy, ctm->yy);
    /* line width */
    _cairo_output_stream_printf (stream, "%f setlinewidth\n",
				 style->line_width);
    /* line cap */
    _cairo_output_stream_printf (stream, "%d setlinecap\n",
				 _cairo_ps_line_cap (style->line_cap));
    /* line join */
    _cairo_output_stream_printf (stream, "%d setlinejoin\n",
				 _cairo_ps_line_join (style->line_join));
    /* dashes */
    if (style->num_dashes) {
	int d;
	_cairo_output_stream_printf (stream, "[");
	for (d = 0; d < style->num_dashes; d++)
	    _cairo_output_stream_printf (stream, " %f", style->dash[d]);
	_cairo_output_stream_printf (stream, "] %f setdash\n",
				     style->dash_offset);
    }
    /* miter limit */
    _cairo_output_stream_printf (stream, "%f setmiterlimit\n",
				 style->miter_limit);
    _cairo_output_stream_printf (stream,
				 "stroke\n");
    _cairo_output_stream_printf (stream,
				 "grestore\n");
    return status;
}

static cairo_int_status_t
_cairo_ps_surface_fill (void		*abstract_surface,
		 cairo_operator_t	 op,
		 cairo_pattern_t	*source,
		 cairo_path_fixed_t	*path,
		 cairo_fill_rule_t	 fill_rule,
		 double			 tolerance,
		 cairo_antialias_t	 antialias)
{
    cairo_ps_surface_t *surface = abstract_surface;
    cairo_output_stream_t *stream = surface->stream;
    cairo_int_status_t status;
    cairo_ps_surface_path_info_t info;
    const char *ps_operator;

    if (surface->paginated_mode == CAIRO_PAGINATED_MODE_ANALYZE)
	return _analyze_operation (surface, op, source);

    assert (operation_supported (surface, op, source));
    
    if (surface->need_start_page) {
	/* Optimize away erasing of nothing. */
	if (op == CAIRO_OPERATOR_CLEAR)
	    return CAIRO_STATUS_SUCCESS;
	_cairo_ps_surface_start_page (surface);
    }

    _cairo_output_stream_printf (stream,
				 "%% _cairo_ps_surface_fill\n");

    emit_pattern (surface, source);

    info.output_stream = stream;
    info.has_current_point = FALSE;

    status = _cairo_path_fixed_interpret (path,
					  CAIRO_DIRECTION_FORWARD,
					  _cairo_ps_surface_path_move_to,
					  _cairo_ps_surface_path_line_to,
					  _cairo_ps_surface_path_curve_to,
					  _cairo_ps_surface_path_close_path,
					  &info);

    switch (fill_rule) {
    case CAIRO_FILL_RULE_WINDING:
	ps_operator = "fill";
	break;
    case CAIRO_FILL_RULE_EVEN_ODD:
	ps_operator = "eofill";
	break;
    default:
	ASSERT_NOT_REACHED;
    }

    _cairo_output_stream_printf (stream,
				 "%s\n", ps_operator);

    return status;
}

static cairo_int_status_t
_cairo_ps_surface_show_glyphs (void		     *abstract_surface,
			       cairo_operator_t	      op,
			       cairo_pattern_t	     *source,
			       const cairo_glyph_t   *glyphs,
			       int		      num_glyphs,
			       cairo_scaled_font_t   *scaled_font)
{
    cairo_ps_surface_t *surface = abstract_surface;
    cairo_output_stream_t *stream = surface->stream;
    cairo_int_status_t status;
    cairo_path_fixed_t *path;

    if (surface->paginated_mode == CAIRO_PAGINATED_MODE_ANALYZE)
	return _analyze_operation (surface, op, source);

    assert (operation_supported (surface, op, source));

    if (surface->need_start_page) {
	/* Optimize away erasing of nothing. */
	if (op == CAIRO_OPERATOR_CLEAR)
	    return CAIRO_STATUS_SUCCESS;
	_cairo_ps_surface_start_page (surface);
    }

    _cairo_output_stream_printf (stream,
				 "%% _cairo_ps_surface_show_glyphs\n");

    path = _cairo_path_fixed_create ();
    _cairo_scaled_font_glyph_path (scaled_font, glyphs, num_glyphs, path);
    status = _cairo_ps_surface_fill (abstract_surface, op, source,
				     path, CAIRO_FILL_RULE_WINDING,
				     0.1, scaled_font->options.antialias);
    _cairo_path_fixed_destroy (path);

    return CAIRO_STATUS_SUCCESS;
}

static const cairo_surface_backend_t cairo_ps_surface_backend = {
    CAIRO_SURFACE_TYPE_PS,
    NULL, /* create_similar */
    _cairo_ps_surface_finish,
    NULL, /* acquire_source_image */
    NULL, /* release_source_image */
    NULL, /* acquire_dest_image */
    NULL, /* release_dest_image */
    NULL, /* clone_similar */
    NULL, /* composite */
    NULL, /* fill_rectangles */
    NULL, /* composite_trapezoids */
    _cairo_ps_surface_copy_page,
    _cairo_ps_surface_show_page,
    NULL, /* set_clip_region */
    _cairo_ps_surface_intersect_clip_path,
    _cairo_ps_surface_get_extents,
#if DONE_ADDING_FONTS_SUPPORT_BACK_AFTER_SWITCHING_TO_PAGINATED
    _cairo_ps_surface_old_show_glyphs,
#else
    NULL, /* old_show_glyphs */
#endif
    NULL, /* get_font_options */
    NULL, /* flush */
    NULL, /* mark_dirty_rectangle */
    NULL, /* scaled_font_fini */
    NULL, /* scaled_glyph_fini */

    /* Here are the drawing functions */
    
    _cairo_ps_surface_paint, /* paint */
    NULL, /* mask */
    _cairo_ps_surface_stroke,
    _cairo_ps_surface_fill,
    _cairo_ps_surface_show_glyphs,
    NULL, /* snapshot */
};

static void
_cairo_ps_set_paginated_mode (cairo_surface_t *target,
			      cairo_paginated_mode_t paginated_mode)
{
    cairo_ps_surface_t *surface = (cairo_ps_surface_t *) target;

    surface->paginated_mode = paginated_mode;
}
