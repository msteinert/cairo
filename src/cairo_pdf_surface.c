/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2004 Red Hat, Inc
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
 *	Kristian Høgsberg <krh@redhat.com>
 */

#include "cairoint.h"

#include <time.h>

/* Issues:
 *
 * - Why doesn't pages inherit /alpha%d GS dictionaries from the Pages
 *   object?
 *
 * - Why isn't the pattern passed to composite traps instead of
 *   pattern->source?  If composite traps needs an image or a surface it
 *   can call create_pattern().
 *
 * - We embed an image in the stream each time it's composited.  We
 *   could add generation counters to surfaces and remember the stream
 *   ID for a particular generation for a particular surface.
 *
 * - Use compression for images.
 *
 * - Multi stop gradients.  What are the exponential interpolation
 *   functions, could they be used for gradients?
 *
 * - Clipping: must be able to reset clipping
 *
 * - Images of other formats than 8 bit RGBA.
 *
 * - Backend specific meta data.
 *
 * - Surface patterns.
 *
 * - Alpha channels in gradients.
 *
 * - Should/does cairo support drawing into a scratch surface and then
 *   using that as a fill pattern?  For this backend, that would involve
 *   using a tiling pattern (4.6.2).  How do you create such a scratch
 *   surface?  cairo_surface_create_similar() ?
 *
 * - What if you create a similiar surface and does show_page and then
 *   does show_surface on another surface?
 *
 * - Output TM so page scales to the right size - PDF default user
 *   space has 1 unit = 1 / 72 inch.
 *
 * - Add test case for RGBA images.
 *
 * - Add test case for RGBA gradients.
 *
 * - Pattern extend isn't honoured by image backend.
 *
 * - Coordinate space for create_similar() args?
 *
 * - Investigate /Matrix entry in content stream dicts for pages
 *   instead of outputting the cm operator in every page.
 */

typedef struct cairo_pdf_object cairo_pdf_object_t;
typedef struct cairo_pdf_resource cairo_pdf_resource_t;
typedef struct cairo_pdf_stream cairo_pdf_stream_t;
typedef struct cairo_pdf_document cairo_pdf_document_t;
typedef struct cairo_pdf_surface cairo_pdf_surface_t;

struct cairo_pdf_object {
    long offset;
};

struct cairo_pdf_resource {
    unsigned int id;
};

struct cairo_pdf_stream {
    unsigned int id;
    unsigned int length_id;
    long start_offset;
};

struct cairo_pdf_document {
    FILE *file;
    unsigned long refcount;

    double width_inches;
    double height_inches;
    double x_ppi;
    double y_ppi;

    unsigned int next_available_id;
    unsigned int pages_id;

    cairo_pdf_stream_t *current_stream;

    cairo_array_t objects;
    cairo_array_t pages;
};

struct cairo_pdf_surface {
    cairo_surface_t base;

    double width_inches;
    double height_inches;

    /* HACK: Non-null if this surface was created for a pattern. */
    cairo_pattern_t *pattern;

    cairo_pdf_document_t *document;
    cairo_pdf_stream_t *current_stream;

    cairo_array_t patterns;
    cairo_array_t xobjects;
    cairo_array_t streams;
    cairo_array_t alphas;
};


static cairo_pdf_document_t *
_cairo_pdf_document_create (FILE	*file,
			    double	width_inches,
			    double	height_inches,
			    double	x_pixels_per_inch,
			    double	y_pixels_per_inch);

static void
_cairo_pdf_document_destroy (cairo_pdf_document_t *document);

static void
_cairo_pdf_document_reference (cairo_pdf_document_t *document);

static cairo_status_t
_cairo_pdf_document_add_page (cairo_pdf_document_t *document,
			      cairo_pdf_surface_t *surface);

static void
_cairo_pdf_surface_clear (cairo_pdf_surface_t *surface);

static cairo_pdf_stream_t *
_cairo_pdf_document_open_stream (cairo_pdf_document_t	*document,
				 const char		*extra_entries);
static cairo_surface_t *
_cairo_pdf_surface_create_for_document (cairo_pdf_document_t	*document,
					double			width_inches,
					double			height_inches);
static void
_cairo_pdf_surface_add_stream (cairo_pdf_surface_t	*surface,
			       cairo_pdf_stream_t	*stream);
static void
_cairo_pdf_surface_ensure_stream (cairo_pdf_surface_t	*surface);

static const cairo_surface_backend_t cairo_pdf_surface_backend;

static unsigned int
_cairo_pdf_document_new_object (cairo_pdf_document_t *document)
{
    cairo_pdf_object_t object;

    object.offset = ftell (document->file);
    /* FIXME: check return value */
    _cairo_array_append (&document->objects, &object, 1);

    return document->next_available_id++;
}

static void
_cairo_pdf_surface_add_stream (cairo_pdf_surface_t	*surface,
			       cairo_pdf_stream_t	*stream)
{
    _cairo_array_append (&surface->streams, &stream, 1);
    surface->current_stream = stream;
}

static void
_cairo_pdf_surface_add_pattern (cairo_pdf_surface_t *surface, unsigned int id)
{
    cairo_pdf_resource_t resource;

    resource.id = id;
    _cairo_array_append (&surface->patterns, &resource, 1);
}

static void
_cairo_pdf_surface_add_xobject (cairo_pdf_surface_t *surface, unsigned int id)
{
    cairo_pdf_resource_t resource;
    int i, num_resources;

    num_resources = _cairo_array_num_elements (&surface->xobjects);
    for (i = 0; i < num_resources; i++) {
	_cairo_array_copy_element (&surface->xobjects, i, &resource);
	if (resource.id == id)
	    return;
    }

    resource.id = id;
    _cairo_array_append (&surface->xobjects, &resource, 1);
}

static unsigned int
_cairo_pdf_surface_add_alpha (cairo_pdf_surface_t *surface, double alpha)
{
    int num_alphas, i;
    double other;

    num_alphas = _cairo_array_num_elements (&surface->alphas);
    for (i = 0; i < num_alphas; i++) {
	_cairo_array_copy_element (&surface->alphas, i, &other);
	if (alpha == other)
	    return i;
    }

    _cairo_array_append (&surface->alphas, &alpha, 1);
    return _cairo_array_num_elements (&surface->alphas) - 1;
}

cairo_surface_t *
cairo_pdf_surface_create (FILE		*file,
			  double	width_inches,
			  double	height_inches,
			  double	x_pixels_per_inch,
			  double	y_pixels_per_inch)
{
    cairo_pdf_document_t *document;
    cairo_surface_t *surface;

    document = _cairo_pdf_document_create (file,
					   width_inches,
					   height_inches,
					   x_pixels_per_inch,
					   y_pixels_per_inch);
    if (document == NULL)
      return NULL;

    surface = _cairo_pdf_surface_create_for_document (document,
						      width_inches,
						      height_inches);

    _cairo_pdf_document_destroy (document);

    return surface;
}

static cairo_surface_t *
_cairo_pdf_surface_create_for_document (cairo_pdf_document_t	*document,
					double			width_inches,
					double			height_inches)
{
    cairo_pdf_surface_t *surface;

    surface = malloc (sizeof (cairo_pdf_surface_t));
    if (surface == NULL)
	return NULL;

    _cairo_surface_init (&surface->base, &cairo_pdf_surface_backend);

    surface->width_inches = width_inches;
    surface->height_inches = height_inches;

    surface->pattern = NULL;
    _cairo_pdf_document_reference (document);
    surface->document = document;
    _cairo_array_init (&surface->streams, sizeof (cairo_pdf_stream_t *));
    _cairo_array_init (&surface->patterns, sizeof (cairo_pdf_resource_t));
    _cairo_array_init (&surface->xobjects, sizeof (cairo_pdf_resource_t));
    _cairo_array_init (&surface->alphas, sizeof (double));

    return &surface->base;
}

static void
_cairo_pdf_surface_clear (cairo_pdf_surface_t *surface)
{
    int num_streams, i;
    cairo_pdf_stream_t *stream;

    num_streams = _cairo_array_num_elements (&surface->streams);
    for (i = 0; i < num_streams; i++) {
	_cairo_array_copy_element (&surface->streams, i, &stream);
	free (stream);
    }

    _cairo_array_truncate (&surface->streams, 0);
    _cairo_array_truncate (&surface->patterns, 0);
    _cairo_array_truncate (&surface->xobjects, 0);
    _cairo_array_truncate (&surface->alphas, 0);
}

static cairo_surface_t *
_cairo_pdf_surface_create_similar (void			*abstract_src,
				   cairo_format_t	format,
				   int			drawable,
				   int			width,
				   int			height)
{
    cairo_pdf_surface_t *template = abstract_src;

    return _cairo_pdf_surface_create_for_document (template->document,
						   width, height);
}

static cairo_pdf_stream_t *
_cairo_pdf_document_open_stream (cairo_pdf_document_t	*document,
				 const char		*extra_entries)
{
    FILE *file = document->file;
    cairo_pdf_stream_t *stream;

    stream = malloc (sizeof (cairo_pdf_stream_t));
    if (stream == NULL) {
	return NULL;
    }

    stream->id = _cairo_pdf_document_new_object (document);
    stream->length_id = _cairo_pdf_document_new_object (document);

    fprintf (file,
	     "%d 0 obj\r\n"
	     "<< /Length %d 0 R\r\n"
	     "%s"
	     ">>\r\n"
	     "stream\r\n",
	     stream->id,
	     stream->length_id,
	     extra_entries);

    stream->start_offset = ftell (file);

    document->current_stream = stream;

    return stream;
}

static void
_cairo_pdf_document_close_stream (cairo_pdf_document_t	*document)
{
    FILE *file = document->file;
    long length;
    cairo_pdf_stream_t *stream;
    cairo_pdf_object_t *object;

    stream = document->current_stream;
    if (stream == NULL)
	return;

    length = ftell(file) - stream->start_offset;
    fprintf (file, 
	     "\r\n"
	     "endstream\r\n"
	     "endobj\r\n");

    object = _cairo_array_index (&document->objects, stream->length_id - 1);
    object->offset = ftell(file);
    fprintf (file, 
	     "%d 0 obj\r\n"
	     "   %ld\r\n"
	     "endobj\r\n",
	     stream->length_id,
	     length);

    document->current_stream = NULL;
}

static void
_cairo_pdf_surface_destroy (void *abstract_surface)
{
    cairo_pdf_surface_t *surface = abstract_surface;
    cairo_pdf_document_t *document = surface->document;

    if (surface->current_stream == document->current_stream)
	_cairo_pdf_document_close_stream (document);

    _cairo_pdf_document_destroy (document);

    free (surface);
}

/* XXX: We should re-work this interface to return both X/Y ppi values. */
static double
_cairo_pdf_surface_pixels_per_inch (void *abstract_surface)
{
    cairo_pdf_surface_t *surface = abstract_surface;
 
    return surface->document->y_ppi;
}

static void
_cairo_pdf_surface_ensure_stream (cairo_pdf_surface_t *surface)
{
    cairo_pdf_document_t *document = surface->document;
    cairo_pdf_stream_t *stream;
    FILE *file = document->file;
    char extra[200];

    if (document->current_stream == NULL ||
	document->current_stream != surface->current_stream) {
	_cairo_pdf_document_close_stream (document);
	snprintf (extra, sizeof extra,
		  "   /Type /XObject\r\n"
		  "   /Subtype /Form\r\n"
		  "   /BBox [ 0 0 %f %f ]\r\n",
		  surface->width_inches * document->x_ppi,
		  surface->height_inches * document->y_ppi);
	stream = _cairo_pdf_document_open_stream (document, extra);
	_cairo_pdf_surface_add_stream (surface, stream);

	/* If this is the first stream we open for this surface,
	 * output the cairo to PDF transformation matrix. */
	if (_cairo_array_num_elements (&surface->streams) == 1)
	    fprintf (file, "1 0 0 -1 0 %f cm\r\n",
		     document->height_inches * document->y_ppi);
    }
}

static cairo_image_surface_t *
_cairo_pdf_surface_get_image (void *abstract_surface)
{
    return NULL;
}

static cairo_status_t
_cairo_pdf_surface_set_image (void			*abstract_surface,
			      cairo_image_surface_t	*image)
{
    return CAIRO_INT_STATUS_UNSUPPORTED;
}

static cairo_status_t
_cairo_pdf_surface_set_matrix (void		*abstract_surface,
			       cairo_matrix_t	*matrix)
{
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_pdf_surface_set_filter (void		*abstract_surface,
			       cairo_filter_t	filter)
{
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_pdf_surface_set_repeat (void		*abstract_surface,
			       int		repeat)
{
    return CAIRO_STATUS_SUCCESS;
}

static unsigned int
emit_image_data (cairo_pdf_document_t *document,
		 cairo_image_surface_t *image)
{
    FILE *file = document->file;
    cairo_pdf_stream_t *stream;
    char entries[200];
    int i, j;

    _cairo_pdf_document_close_stream (document);

    snprintf (entries, sizeof entries, 
	      "   /Type /XObject\r\n"
	      "   /Subtype /Image\r\n"
	      "   /Width %d\r\n"
	      "   /Height %d\r\n"
	      "   /ColorSpace /DeviceRGB\r\n"
	      "   /BitsPerComponent 8\r\n",
	      image->width, image->height);

    stream = _cairo_pdf_document_open_stream (document, entries);

    for (i = 0; i < image->height; i++) {
	for (j = 0; j < image->width; j++) {
	    fputc((unsigned) image->data[i * image->stride + j * 4 + 2], file);
	    fputc((unsigned) image->data[i * image->stride + j * 4 + 1], file);
	    fputc((unsigned) image->data[i * image->stride + j * 4 + 0], file);
	}
    }

    _cairo_pdf_document_close_stream (document);

    return stream->id;
}

static void
_cairo_pdf_surface_composite_image (cairo_pdf_surface_t *dst,
				    cairo_image_surface_t *image)
{
    cairo_pdf_document_t *document = dst->document;
    FILE *file = document->file;
    unsigned id;
    cairo_matrix_t i2u;

    id = emit_image_data (dst->document, image);
    _cairo_pdf_surface_add_xobject (dst, id);

    _cairo_pdf_surface_ensure_stream (dst);

    cairo_matrix_copy (&i2u, &image->base.matrix);
    cairo_matrix_invert (&i2u);
    cairo_matrix_translate (&i2u, 0, image->height);
    cairo_matrix_scale (&i2u, image->width, -image->height);

    fprintf (file,
	     "q %f %f %f %f %f %f cm /res%d Do Q\r\n",
	     i2u.m[0][0], i2u.m[0][1],
	     i2u.m[1][0], i2u.m[1][1],
	     i2u.m[2][0], i2u.m[2][1],
	     id);
}

/* The contents of the surface is already transformed into PDF units,
 * but when we composite the surface we may want to use a different
 * space.  The problem I see now is that the show_surface snippet
 * creates a surface 1x1, which in the snippet environment is the
 * entire surface.  When compositing the surface, cairo gives us the
 * 1x1 to 256x256 matrix.  This would be fine if cairo didn't actually
 * also transform the drawing to the surface.  Should the CTM be part
 * of the current target surface?
 */

static cairo_int_status_t
_cairo_pdf_surface_composite_pdf (cairo_pdf_surface_t *dst,
				  cairo_pdf_surface_t *src,
				  int width, int height)
{
    cairo_pdf_document_t *document = dst->document;
    FILE *file = document->file;
    cairo_matrix_t i2u;
    cairo_pdf_stream_t *stream;
    int num_streams, i;

    if (src->pattern != NULL)
	return CAIRO_STATUS_SUCCESS;

    _cairo_pdf_surface_ensure_stream (dst);

    cairo_matrix_copy (&i2u, &src->base.matrix);
    cairo_matrix_invert (&i2u);
    cairo_matrix_scale (&i2u, 
			1.0 / (src->width_inches * document->x_ppi),
			1.0 / (src->height_inches * document->y_ppi));

    fprintf (file,
	     "q %f %f %f %f %f %f cm",
	     i2u.m[0][0], i2u.m[0][1],
	     i2u.m[1][0], i2u.m[1][1],
	     i2u.m[2][0], i2u.m[2][1]);

    num_streams = _cairo_array_num_elements (&src->streams);
    for (i = 0; i < num_streams; i++) {
	_cairo_array_copy_element (&src->streams, i, &stream);
	fprintf (file,
		 " /res%d Do",
		 stream->id);

	_cairo_pdf_surface_add_xobject (dst, stream->id);

    }
	
    fprintf (file, " Q\r\n");

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_pdf_surface_composite (cairo_operator_t	operator,
			      cairo_surface_t	*generic_src,
			      cairo_surface_t	*generic_mask,
			      void		*abstract_dst,
			      int		src_x,
			      int		src_y,
			      int		mask_x,
			      int		mask_y,
			      int		dst_x,
			      int		dst_y,
			      unsigned int	width,
			      unsigned int	height)
{
    cairo_pdf_surface_t *dst = abstract_dst;
    cairo_pdf_surface_t *src;
    cairo_image_surface_t *image;

    if (generic_src->backend == &cairo_pdf_surface_backend) {
	src = (cairo_pdf_surface_t *) generic_src;
	_cairo_pdf_surface_composite_pdf (dst, src, width, height);
    }
    else {
	image = _cairo_surface_get_image (generic_src);
	_cairo_pdf_surface_composite_image (dst, image);
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_pdf_surface_fill_rectangles (void		*abstract_surface,
				    cairo_operator_t	operator,
				    const cairo_color_t	*color,
				    cairo_rectangle_t	*rects,
				    int			num_rects)
{
    cairo_pdf_surface_t *surface = abstract_surface;
    cairo_pdf_document_t *document = surface->document;
    FILE *file = document->file;
    int i;

    if (surface->pattern != NULL)
	return CAIRO_STATUS_SUCCESS;

    _cairo_pdf_surface_ensure_stream (surface);

    fprintf (file,
	     "%f %f %f rg\r\n",
	     color->red, color->green, color->blue);

    for (i = 0; i < num_rects; i++) {
	fprintf (file, 
		 "%d %d %d %d re f\r\n",
		 rects[i].x, rects[i].y,
		 rects[i].width, rects[i].height);
    }

    return CAIRO_STATUS_SUCCESS;
}

static void
emit_tiling_pattern (cairo_operator_t		operator,
		     cairo_pdf_surface_t	*dst,
		     cairo_pattern_t		*pattern)
{
    cairo_pdf_document_t *document = dst->document;
    FILE *file = document->file;
    cairo_pdf_stream_t *stream;
    cairo_image_surface_t *image;
    char entries[250];
    unsigned int id, alpha;
    cairo_matrix_t pm;

    if (pattern->u.surface.surface->backend == &cairo_pdf_surface_backend) {
	return;
    }
    
    image = _cairo_surface_get_image (pattern->u.surface.surface);

    _cairo_pdf_document_close_stream (document);

    id = emit_image_data (dst->document, image);

    /* BBox must be smaller than XStep by YStep or acroread wont
     * display the pattern. */

    cairo_matrix_set_identity (&pm);
    cairo_matrix_scale (&pm, image->width, image->height);
    cairo_matrix_copy (&pm, &pattern->matrix);
    cairo_matrix_invert (&pm);

    snprintf (entries, sizeof entries,
	      "   /BBox [ 0 0 256 256 ]\r\n"
	      "   /XStep 256\r\n"
	      "   /YStep 256\r\n"
	      "   /PatternType 1\r\n"
	      "   /TilingType 1\r\n"
	      "   /PaintType 1\r\n"
	      "   /Resources << /XObject << /res%d %d 0 R >> >>\r\n"
	      "   /Matrix [ %f %f %f %f %f %f ]\r\n",
	      id, id,
	      pm.m[0][0], pm.m[0][1],
	      pm.m[1][0], pm.m[1][1],
	      pm.m[2][0], pm.m[2][1]);

    stream = _cairo_pdf_document_open_stream (document, entries);

    _cairo_pdf_surface_add_pattern (dst, stream->id);

    _cairo_pdf_surface_ensure_stream (dst);
    alpha = _cairo_pdf_surface_add_alpha (dst, 1.0);
    fprintf (file,
	     "/Pattern cs /res%d scn /a%d gs\r\n",
	     stream->id, alpha);
}

static unsigned int
emit_pattern_stops (cairo_pdf_surface_t *surface, cairo_pattern_t *pattern)
{
    cairo_pdf_document_t *document = surface->document;
    FILE *file = document->file;
    unsigned int function_id;

    function_id = _cairo_pdf_document_new_object (document);
    fprintf (file,
	     "%d 0 obj\r\n"
	     "<< /FunctionType 0\r\n"
	     "   /Domain [ 0.0 1.0 ]\r\n"
	     "   /Size [ 2 ]\r\n"
	     "   /BitsPerSample 8\r\n"
	     "   /Range [ 0.0 1.0 0.0 1.0 0.0 1.0 ]\r\n"
	     "   /Length 6\r\n"
	     ">>\r\n"
	     "stream\r\n",
	     function_id);

    fputc (pattern->stops[0].color_char[0], file);
    fputc (pattern->stops[0].color_char[1], file);
    fputc (pattern->stops[0].color_char[2], file);
    fputc (pattern->stops[1].color_char[0], file);
    fputc (pattern->stops[1].color_char[1], file);
    fputc (pattern->stops[1].color_char[2], file);

    fprintf (file,
	     "\r\n"
	     "endstream\r\n"
	     "endobj\r\n");

    return function_id;
}

static void
emit_linear_pattern (cairo_pdf_surface_t *surface, cairo_pattern_t *pattern)
{
    cairo_pdf_document_t *document = surface->document;
    FILE *file = document->file;
    unsigned int function_id, pattern_id, alpha;
    double x0, y0, x1, y1;
    cairo_matrix_t p2u;

    _cairo_pdf_document_close_stream (document);

    function_id = emit_pattern_stops (surface, pattern);

    cairo_matrix_copy (&p2u, &pattern->matrix);
    cairo_matrix_invert (&p2u);

    x0 = pattern->u.linear.point0.x;
    y0 = pattern->u.linear.point0.y;
    cairo_matrix_transform_point (&p2u, &x0, &y0);
    x1 = pattern->u.linear.point1.x;
    y1 = pattern->u.linear.point1.y;
    cairo_matrix_transform_point (&p2u, &x1, &y1);

    pattern_id = _cairo_pdf_document_new_object (document);
    fprintf (file,
	     "%d 0 obj\r\n"
	     "<< /Type /Pattern\r\n"
	     "   /PatternType 2\r\n"
	     "   /Matrix [ 1 0 0 -1 0 %f ]\r\n"
	     "   /Shading\r\n"
	     "      << /ShadingType 2\r\n"
	     "         /ColorSpace /DeviceRGB\r\n"
	     "         /Coords [ %f %f %f %f ]\r\n"
	     "         /Function %d 0 R\r\n"
	     "         /Extend [ %s %s ]\r\n"
	     "      >>\r\n"
	     ">>\r\n"
	     "endobj\r\n",
	     pattern_id,
	     document->height_inches * document->y_ppi,
	     x0, y0, x1, y1,
	     function_id,
	     (1 || pattern->extend) ? "true" : "false",
	     (1 || pattern->extend) ? "true" : "false");
    
    _cairo_pdf_surface_add_pattern (surface, pattern_id);

    _cairo_pdf_surface_ensure_stream (surface);
    alpha = _cairo_pdf_surface_add_alpha (surface, 1.0);

    /* Use pattern */
    fprintf (file,
	     "/Pattern cs /res%d scn /a%d gs\r\n",
	     pattern_id, alpha);
}
	
static void
emit_radial_pattern (cairo_pdf_surface_t *surface, cairo_pattern_t *pattern)
{
    cairo_pdf_document_t *document = surface->document;
    FILE *file = document->file;
    unsigned int function_id, pattern_id, alpha;
    double x0, y0, x1, y1, r0, r1;
    cairo_matrix_t p2u;

    _cairo_pdf_document_close_stream (document);

    function_id = emit_pattern_stops (surface, pattern);

    cairo_matrix_copy (&p2u, &pattern->matrix);
    cairo_matrix_invert (&p2u);

    x0 = pattern->u.radial.center0.x;
    y0 = pattern->u.radial.center0.y;
    r0 = pattern->u.radial.radius0;
    cairo_matrix_transform_point (&p2u, &x0, &y0);
    x1 = pattern->u.radial.center1.x;
    y1 = pattern->u.radial.center1.y;
    r1 = pattern->u.radial.radius1;
    cairo_matrix_transform_point (&p2u, &x1, &y1);

    /* FIXME: This is surely crack, but how should you scale a radius
     * in a non-orthogonal coordinate system? */
    cairo_matrix_transform_distance (&p2u, &r0, &r1);

    pattern_id = _cairo_pdf_document_new_object (document);
    fprintf (file,
	     "%d 0 obj\r\n"
	     "<< /Type /Pattern\r\n"
	     "   /PatternType 2\r\n"
	     "   /Matrix [ 1 0 0 -1 0 %f ]\r\n"
	     "   /Shading\r\n"
	     "      << /ShadingType 3\r\n"
	     "         /ColorSpace /DeviceRGB\r\n"
	     "         /Coords [ %f %f %f %f %f %f ]\r\n"
	     "         /Function %d 0 R\r\n"
	     "         /Extend [ %s %s ]\r\n"
	     "      >>\r\n"
	     ">>\r\n"
	     "endobj\r\n",
	     pattern_id,
	     document->height_inches * document->y_ppi,
	     x0, y0, r0, x1, y1, r1,
	     function_id,
	     (1 || pattern->extend) ? "true" : "false",
	     (1 || pattern->extend) ? "true" : "false");
    
    _cairo_pdf_surface_add_pattern (surface, pattern_id);

    _cairo_pdf_surface_ensure_stream (surface);
    alpha = _cairo_pdf_surface_add_alpha (surface, 1.0);

    /* Use pattern */
    fprintf (file,
	     "/Pattern cs /res%d scn /a%d gs\r\n",
	     pattern_id, alpha);
}
	
static double
intersect (cairo_line_t *line, cairo_fixed_t y)
{
    return _cairo_fixed_to_double (line->p1.x) +
	_cairo_fixed_to_double (line->p2.x - line->p1.x) *
	_cairo_fixed_to_double (y - line->p1.y) /
	_cairo_fixed_to_double (line->p2.y - line->p1.y);
}

static cairo_int_status_t
_cairo_pdf_surface_composite_trapezoids (cairo_operator_t	operator,
					 cairo_surface_t	*generic_src,
					 void			*abstract_dst,
					 int			x_src,
					 int			y_src,
					 cairo_trapezoid_t	*traps,
					 int			num_traps)
{
    cairo_pdf_surface_t *surface = abstract_dst;
    cairo_pdf_surface_t *source = (cairo_pdf_surface_t *) generic_src;
    cairo_pdf_document_t *document = surface->document;
    cairo_pattern_t *pattern;
    FILE *file = document->file;
    int i;
    unsigned int alpha;

    /* FIXME: we really just want the original pattern here, not a
     * source surface. */
    pattern = source->pattern;

    if (source->base.backend != &cairo_pdf_surface_backend) {
	printf ("_cairo_pdf_surface_composite_trapezoids: not a pdf source\r");
	return CAIRO_STATUS_SUCCESS;
    }

    if (pattern == NULL) {
	printf ("_cairo_pdf_surface_composite_trapezoids: "
		"non-pattern pdf source\r");
	return CAIRO_STATUS_SUCCESS;
    }

    switch (pattern->type) {
    case CAIRO_PATTERN_SOLID:	
	alpha = _cairo_pdf_surface_add_alpha (surface, pattern->color.alpha);
	_cairo_pdf_surface_ensure_stream (surface);
	fprintf (file, 
		 "%f %f %f rg /a%d gs\r\n",
		 pattern->color.red,
		 pattern->color.green,
		 pattern->color.blue,
		 alpha);
	break;

    case CAIRO_PATTERN_SURFACE:
	emit_tiling_pattern (operator, surface, pattern);
	break;

    case CAIRO_PATTERN_LINEAR:
	emit_linear_pattern (surface, pattern);
	break;

    case CAIRO_PATTERN_RADIAL:
	emit_radial_pattern (surface, pattern );
	break;	    
    }

    /* After the above switch the current stream should belong to this
     * surface, so no need to _cairo_pdf_surface_ensure_stream() */
    assert (document->current_stream != NULL &&
	    document->current_stream == surface->current_stream);

    for (i = 0; i < num_traps; i++) {
	double left_x1, left_x2, right_x1, right_x2;

	left_x1  = intersect (&traps[i].left, traps[i].top);
	left_x2  = intersect (&traps[i].left, traps[i].bottom);
	right_x1 = intersect (&traps[i].right, traps[i].top);
	right_x2 = intersect (&traps[i].right, traps[i].bottom);

	fprintf (file, 
		 "%f %f m %f %f l %f %f l %f %f l h\r\n",
		 left_x1, _cairo_fixed_to_double (traps[i].top),
		 left_x2, _cairo_fixed_to_double (traps[i].bottom),
		 right_x2, _cairo_fixed_to_double (traps[i].bottom),
		 right_x1, _cairo_fixed_to_double (traps[i].top));
    }

    fprintf (file, 
	     "f\r\n");

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_pdf_surface_copy_page (void *abstract_surface)
{
    cairo_pdf_surface_t *surface = abstract_surface;
    cairo_pdf_document_t *document = surface->document;

    return _cairo_pdf_document_add_page (document, surface);
}

static cairo_int_status_t
_cairo_pdf_surface_show_page (void *abstract_surface)
{
    cairo_pdf_surface_t *surface = abstract_surface;
    cairo_pdf_document_t *document = surface->document;
    cairo_int_status_t status;

    status = _cairo_pdf_document_add_page (document, surface);
    if (status == CAIRO_STATUS_SUCCESS)
	_cairo_pdf_surface_clear (surface);

    return status;
}

static cairo_int_status_t
_cairo_pdf_surface_set_clip_region (void *abstract_surface,
				    pixman_region16_t *region)
{
    return CAIRO_INT_STATUS_UNSUPPORTED;
}

static cairo_int_status_t
_cairo_pdf_surface_create_pattern (void *abstract_surface,
				   cairo_pattern_t *pattern,
				   cairo_box_t *extents)
{
    cairo_pdf_surface_t *surface = abstract_surface;
    cairo_pdf_surface_t *source;

    source = (cairo_pdf_surface_t *) 
	_cairo_pdf_surface_create_for_document (surface->document, 0, 0);
    source->pattern = pattern;
    pattern->source = &source->base;

    return CAIRO_STATUS_SUCCESS;
}

static const cairo_surface_backend_t cairo_pdf_surface_backend = {
    _cairo_pdf_surface_create_similar,
    _cairo_pdf_surface_destroy,
    _cairo_pdf_surface_pixels_per_inch,
    _cairo_pdf_surface_get_image,
    _cairo_pdf_surface_set_image,
    _cairo_pdf_surface_set_matrix,
    _cairo_pdf_surface_set_filter,
    _cairo_pdf_surface_set_repeat,
    _cairo_pdf_surface_composite,
    _cairo_pdf_surface_fill_rectangles,
    _cairo_pdf_surface_composite_trapezoids,
    _cairo_pdf_surface_copy_page,
    _cairo_pdf_surface_show_page,
    _cairo_pdf_surface_set_clip_region,
    _cairo_pdf_surface_create_pattern,
    NULL, /* show_glyphs */
};

static cairo_pdf_document_t *
_cairo_pdf_document_create (FILE	*file,
			   double	width_inches,
			   double	height_inches,
			   double	x_pixels_per_inch,
			   double	y_pixels_per_inch)
{
    cairo_pdf_document_t *document;

    document = malloc (sizeof (cairo_pdf_document_t));
    if (document == NULL)
	return NULL;

    document->file = file;
    document->refcount = 1;
    document->width_inches = width_inches;
    document->height_inches = height_inches;
    document->x_ppi = x_pixels_per_inch;
    document->y_ppi = y_pixels_per_inch;

    _cairo_array_init (&document->objects, sizeof (cairo_pdf_object_t));
    _cairo_array_init (&document->pages, sizeof (unsigned int));
    document->next_available_id = 1;

    document->current_stream = NULL;

    document->pages_id = _cairo_pdf_document_new_object (document);

    /* Document header */
    fprintf (file, "%%PDF-1.4\r\n");

    return document;
}

static unsigned int
_cairo_pdf_document_write_info (cairo_pdf_document_t *document)
{
    FILE *file = document->file;
    unsigned int id;

    id = _cairo_pdf_document_new_object (document);
    fprintf (file,
	     "%d 0 obj\r\n"
	     "<< /Creator (cairographics.org)\r\n"
	     "   /Producer (cairographics.org)\r\n"
	     ">>\r\n"
	     "endobj\r\n",
	     id);

    return id;
}

static void
_cairo_pdf_document_write_pages (cairo_pdf_document_t *document)
{
    FILE *file = document->file;
    cairo_pdf_object_t *pages_object;
    unsigned int page_id;
    int num_pages, i;

    pages_object = _cairo_array_index (&document->objects,
				       document->pages_id - 1);
    pages_object->offset = ftell (file);

    fprintf (file,
	     "%d 0 obj\r\n"
	     "<< /Type /Pages\r\n"
	     "   /Kids [ ",
	     document->pages_id);
    
    num_pages = _cairo_array_num_elements (&document->pages);
    for (i = 0; i < num_pages; i++) {
	_cairo_array_copy_element (&document->pages, i, &page_id);
	fprintf (file, "%d 0 R ", page_id);
    }

    fprintf (file, "]\r\n"); 
    fprintf (file, "   /Count %d\r\n", num_pages);

    /* TODO: Figure out wich other defaults to be inherited by /Page
     * objects. */
    fprintf (file,
	     "   /MediaBox [ 0 0 %f %f ]\r\n"
	     ">>\r\n"
	     "endobj\r\n",
	     document->width_inches * document->x_ppi,
	     document->height_inches * document->y_ppi);
}

static unsigned int
_cairo_pdf_document_write_catalog (cairo_pdf_document_t *document)
{
    FILE *file = document->file;
    unsigned int id;

    id = _cairo_pdf_document_new_object (document);
    fprintf (file,
	     "%d 0 obj\r\n"
	     "<< /Type /Catalog\r\n"
	     "   /Pages %d 0 R\r\n" 
	     ">>\r\n"
	     "endobj\r\n",
	     id, document->pages_id);

    return id;
}

static long
_cairo_pdf_document_write_xref (cairo_pdf_document_t *document)
{
    FILE *file = document->file;
    cairo_pdf_object_t *object;
    int num_objects, i;
    long offset;

    num_objects = _cairo_array_num_elements (&document->objects);

    offset = ftell(file);
    fprintf (document->file,
	     "xref\r\n"
	     "%d %d\r\n",
	     0, num_objects + 1);

    fprintf (file, "0000000000 65535 f\r\n");
    for (i = 0; i < num_objects; i++) {
	object = _cairo_array_index (&document->objects, i);
	fprintf (file, "%010ld 00000 n\r\n", object->offset);
    }

    return offset;
}

static void
_cairo_pdf_document_reference (cairo_pdf_document_t *document)
{
    document->refcount++;
}

static void
_cairo_pdf_document_destroy (cairo_pdf_document_t *document)
{
    FILE *file = document->file;
    long offset;
    unsigned int info_id, catalog_id;

    document->refcount--;
    if (document->refcount > 0)
      return;

    _cairo_pdf_document_close_stream (document);
    _cairo_pdf_document_write_pages (document);
    info_id = _cairo_pdf_document_write_info (document);
    catalog_id = _cairo_pdf_document_write_catalog (document);
    offset = _cairo_pdf_document_write_xref (document);
    
    fprintf (file,
	     "trailer\r\n"
	     "<< /Size %d\r\n"
	     "   /Root %d 0 R\r\n"
	     "   /Info %d 0 R\r\n"
	     ">>\r\n",
	     document->next_available_id,
	     catalog_id,
	     info_id);

    fprintf (file,
	     "startxref\r\n"
	     "%ld\r\n"
	     "%%%%EOF\r\n",
	     offset);

    free (document);
}

static cairo_status_t
_cairo_pdf_document_add_page (cairo_pdf_document_t	*document,
			      cairo_pdf_surface_t	*surface)
{
    cairo_pdf_stream_t *stream;
    cairo_pdf_resource_t *res;
    FILE *file = document->file;
    unsigned int page_id;
    double alpha;
    int num_streams, num_alphas, num_resources, i;

    _cairo_pdf_document_close_stream (document);

    page_id = _cairo_pdf_document_new_object (document);
    fprintf (file,
	     "%d 0 obj\r\n"
	     "<< /Type /Page\r\n"
	     "   /Parent %d 0 R\r\n"
	     "   /Contents [",
	     page_id,
	     document->pages_id);

    num_streams = _cairo_array_num_elements (&surface->streams);
    for (i = 0; i < num_streams; i++) {
	_cairo_array_copy_element (&surface->streams, i, &stream);	
	fprintf (file,
		 " %d 0 R",
		 stream->id);
    }

    fprintf (file, 
	     " ]\r\n"
	     "   /Resources <<\r\n");

    
    num_alphas =  _cairo_array_num_elements (&surface->alphas);
    if (num_alphas > 0) {
	fprintf (file,
		 "      /ExtGState <<\r\n");

	for (i = 0; i < num_alphas; i++) {
	    _cairo_array_copy_element (&surface->alphas, i, &alpha);
	    fprintf (file,
		     "         /a%d << /ca %f >>\r\n",
		     i, alpha);
	}

	fprintf (file,
		 "      >>\r\n");
    }
    
    num_resources = _cairo_array_num_elements (&surface->patterns);
    if (num_resources > 0) {
	fprintf (file,
		 "      /Pattern <<");
	for (i = 0; i < num_resources; i++) {
	    res = _cairo_array_index (&surface->patterns, i);
	    fprintf (file,
		     " /res%d %d 0 R",
		     res->id, res->id);
	}

	fprintf (file,
		 " >>\r\n");
    }

    num_resources = _cairo_array_num_elements (&surface->xobjects);
    if (num_resources > 0) {
	fprintf (file,
		 "      /XObject <<");

	for (i = 0; i < num_resources; i++) {
	    res = _cairo_array_index (&surface->xobjects, i);
	    fprintf (file,
		     " /res%d %d 0 R",
		     res->id, res->id);
	}

	fprintf (file,
		 " >>\r\n");
    }

    fprintf (file,
	     "   >>\r\n"
	     ">>\r\n"
	     "endobj\r\n");

    _cairo_array_append (&document->pages, &page_id, 1);

    return CAIRO_STATUS_SUCCESS;
}

void
cairo_set_target_pdf (cairo_t	*cr,
		      FILE	*file,
		      double	width_inches,
		      double	height_inches,
		      double	x_pixels_per_inch,
		      double	y_pixels_per_inch)
{
    cairo_surface_t *surface;

    surface = cairo_pdf_surface_create (file,
					width_inches,
					height_inches,
					x_pixels_per_inch,
					y_pixels_per_inch);

    if (surface == NULL) {
	cr->status = CAIRO_STATUS_NO_MEMORY;
	return;
    }

    cairo_set_target_surface (cr, surface);

    /* cairo_set_target_surface takes a reference, so we must destroy ours */
    cairo_surface_destroy (surface);
}
