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

/*
 * Type1 and Type3 PS fonts can hold only 256 glyphs.
 *
 * XXX Work around this by placing each set of 256 glyphs in a separate
 * font. No separate data structure is kept for this; the font name is
 * generated from all but the low 8 bits of the output glyph id.
 */

typedef struct cairo_ps_glyph {
    cairo_hash_entry_t	    base;	    /* font glyph index */
    unsigned int	    output_glyph;   /* PS sub-font glyph index */
} cairo_ps_glyph_t;

typedef struct cairo_ps_font {
    cairo_hash_entry_t	    base;
    cairo_scaled_font_t	    *scaled_font;
    unsigned int	    output_font;
    cairo_hash_table_t	    *glyphs;
    unsigned int	    max_glyph;
} cairo_ps_font_t;

typedef struct cairo_ps_surface {
    cairo_surface_t base;

    /* Here final_stream corresponds to the stream/file passed to
     * cairo_ps_surface_create surface is built. Meanwhile stream is a
     * temporary stream in which the file output is built, (so that
     * the header can be built and inserted into the target stream
     * before the contents of the temporary stream are copied). */
    cairo_output_stream_t *final_stream;

    FILE *tmpfile;
    cairo_output_stream_t *stream;

    double width;
    double height;
    double x_dpi;
    double y_dpi;

    cairo_bool_t need_start_page; 
    int num_pages;

    cairo_paginated_mode_t paginated_mode;

    cairo_hash_table_t *fonts;
    unsigned int max_font;
    
} cairo_ps_surface_t;

#define PS_SURFACE_DPI_DEFAULT 300.0

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
				 "%f %f M ",
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
	ps_operator = "L";
    else
	ps_operator = "M";
    
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
				 "%f %f %f %f %f %f C ",
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
    
    if (info->has_current_point)
        _cairo_output_stream_printf (info->output_stream,
				     "P\n");
    info->has_current_point = FALSE;

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_ps_surface_emit_header (cairo_ps_surface_t *surface)
{
    time_t now;

    now = time (NULL);

    _cairo_output_stream_printf (surface->final_stream,
				 "%%!PS-Adobe-3.0\n"
				 "%%%%Creator: cairo (http://cairographics.org)\n"
				 "%%%%CreationDate: %s"
				 "%%%%Pages: %d\n"
				 "%%%%BoundingBox: %f %f %f %f\n",
				 ctime (&now),
				 surface->num_pages,
				 0.0, 0.0, 
				 surface->width,
				 surface->height);

    _cairo_output_stream_printf (surface->final_stream,
				 "%%%%DocumentData: Clean7Bit\n"
				 "%%%%LanguageLevel: 2\n"
				 "%%%%Orientation: Portrait\n"
				 "%%%%EndComments\n");

    _cairo_output_stream_printf (surface->final_stream,
				 "%%%%BeginProlog\n"
				 "/C{curveto}bind def\n"
				 "/F{fill}bind def\n"
				 "/G{setgray}bind def\n"
				 "/L{lineto}bind def\n"
				 "/M{moveto}bind def\n"
				 "/P{closepath}bind def\n"
				 "/R{setrgbcolor}bind def\n"
				 "/S{show}bind def\n"
				 "%%%%EndProlog\n");
}

static cairo_bool_t
_cairo_ps_glyph_equal (const void *key_a, const void *key_b)
{
    const cairo_ps_glyph_t   *ps_glyph_a = key_a;
    const cairo_ps_glyph_t   *ps_glyph_b = key_b;

    return ps_glyph_a->base.hash == ps_glyph_b->base.hash;
}

static void
_cairo_ps_glyph_key_init (cairo_ps_glyph_t  *ps_glyph,
			  unsigned long	    index)
{
    ps_glyph->base.hash = index;
}

static cairo_ps_glyph_t *
_cairo_ps_glyph_create (cairo_ps_font_t *ps_font,
			unsigned long index)
{
    cairo_ps_glyph_t	*ps_glyph = malloc (sizeof (cairo_ps_glyph_t));

    if (!ps_glyph)
	return NULL;
    _cairo_ps_glyph_key_init (ps_glyph, index);
    ps_glyph->output_glyph = ps_font->max_glyph++;
    return ps_glyph;
}

static void
_cairo_ps_glyph_destroy (cairo_ps_glyph_t *ps_glyph)
{
    free (ps_glyph);
}

static cairo_status_t
_cairo_ps_glyph_find (cairo_ps_font_t	    *font,
		      cairo_scaled_font_t   *scaled_font,
		      unsigned long	    index,
		      cairo_ps_glyph_t	    **result)
{
    cairo_ps_glyph_t	key;
    cairo_ps_glyph_t	*ps_glyph;
    cairo_status_t	status;

    _cairo_ps_glyph_key_init (&key, index);
    if (!_cairo_hash_table_lookup (font->glyphs, 
				   &key.base, 
				   (cairo_hash_entry_t **) &ps_glyph)) {
	ps_glyph = _cairo_ps_glyph_create (font, index);
	if (!ps_glyph)
	    return CAIRO_STATUS_NO_MEMORY;
	status = _cairo_hash_table_insert (font->glyphs, &ps_glyph->base);
	if (status)
	    return status;
    }
    *result = ps_glyph;
    return CAIRO_STATUS_SUCCESS;
}

static cairo_bool_t
_cairo_ps_font_equal (const void *key_a, const void *key_b)
{
    const cairo_ps_font_t   *ps_font_a = key_a;
    const cairo_ps_font_t   *ps_font_b = key_b;

    return ps_font_a->scaled_font == ps_font_b->scaled_font;
}

static void
_cairo_ps_font_key_init (cairo_ps_font_t	*ps_font,
			 cairo_scaled_font_t	*scaled_font)
{
    ps_font->base.hash = (unsigned long) scaled_font;
    ps_font->scaled_font = scaled_font;
}

static cairo_ps_font_t *
_cairo_ps_font_create (cairo_ps_surface_t   *surface,
		       cairo_scaled_font_t  *scaled_font)
{
    cairo_ps_font_t *ps_font = malloc (sizeof (cairo_ps_font_t));
    if (!ps_font)
	return NULL;
    _cairo_ps_font_key_init (ps_font, scaled_font);
    ps_font->glyphs = _cairo_hash_table_create (_cairo_ps_glyph_equal);
    if (!ps_font->glyphs) {
	free (ps_font);
	return NULL;
    }
    ps_font->max_glyph = 0;
    ps_font->output_font = surface->max_font++;
    cairo_scaled_font_reference (ps_font->scaled_font);
    return ps_font;
}

static void
_cairo_ps_font_destroy_glyph (void *entry, void *closure)
{
    cairo_ps_glyph_t	*ps_glyph = entry;
    cairo_ps_font_t	*ps_font = closure;
    
    _cairo_hash_table_remove (ps_font->glyphs, &ps_glyph->base);
    _cairo_ps_glyph_destroy (ps_glyph);
}

static void
_cairo_ps_font_destroy (cairo_ps_font_t *ps_font)
{
    _cairo_hash_table_foreach (ps_font->glyphs,
			       _cairo_ps_font_destroy_glyph,
			       ps_font);
    _cairo_hash_table_destroy (ps_font->glyphs);
    cairo_scaled_font_destroy (ps_font->scaled_font);
    free (ps_font);
}

static void
_cairo_ps_surface_destroy_font (cairo_ps_surface_t *surface,
				cairo_ps_font_t *ps_font)
{
    _cairo_hash_table_remove (surface->fonts, &ps_font->base);
    _cairo_ps_font_destroy (ps_font);
}

static cairo_status_t
_cairo_ps_font_find (cairo_ps_surface_t	    *surface,
		     cairo_scaled_font_t    *scaled_font,
		     cairo_ps_font_t	    **result)
{
    cairo_ps_font_t	key;
    cairo_ps_font_t	*ps_font;
    cairo_status_t	status;

    _cairo_ps_font_key_init (&key, scaled_font);
    if (!_cairo_hash_table_lookup (surface->fonts, &key.base,
				   (cairo_hash_entry_t **) &ps_font)) 
    {
	ps_font = _cairo_ps_font_create (surface, scaled_font);
	if (!ps_font)
	    return CAIRO_STATUS_NO_MEMORY;
	status = _cairo_hash_table_insert (surface->fonts,
					   &ps_font->base);
	if (status)
	    return status;
    }
    *result = ps_font;
    return CAIRO_STATUS_SUCCESS;
}

typedef struct _cairo_ps_font_glyph_select {
    cairo_ps_glyph_t	**glyphs;
    int			subfont;
    int			numglyph;
} cairo_ps_font_glyph_select_t;

static void
_cairo_ps_font_select_glyphs (void *entry, void *closure)
{
    cairo_ps_glyph_t		    *ps_glyph = entry;
    cairo_ps_font_glyph_select_t    *ps_glyph_select = closure;

    if (ps_glyph->output_glyph >> 8 == ps_glyph_select->subfont) {
	unsigned long	sub_glyph = ps_glyph->output_glyph & 0xff;
	ps_glyph_select->glyphs[sub_glyph] = ps_glyph;
	if (sub_glyph >= ps_glyph_select->numglyph)
	    ps_glyph_select->numglyph = sub_glyph + 1;
    }
}

static cairo_status_t
_cairo_ps_surface_emit_glyph (cairo_ps_surface_t *surface,
			      cairo_ps_font_t *ps_font,
			      cairo_ps_glyph_t *ps_glyph)
{
    cairo_scaled_glyph_t    *scaled_glyph;
    cairo_status_t	    status;
    cairo_ps_surface_path_info_t info;
    
    _cairo_output_stream_printf (surface->final_stream,
				 "\t\t{ %% %d\n", ps_glyph->output_glyph);
    status = _cairo_scaled_glyph_lookup (ps_font->scaled_font,
					 ps_glyph->base.hash,
					 CAIRO_SCALED_GLYPH_INFO_METRICS|
					 CAIRO_SCALED_GLYPH_INFO_PATH,
					 &scaled_glyph);
    /*
     * If that fails, try again but ask for an image instead
     */
    if (status)
	status = _cairo_scaled_glyph_lookup (ps_font->scaled_font,
					     ps_glyph->base.hash,
					     CAIRO_SCALED_GLYPH_INFO_METRICS|
					     CAIRO_SCALED_GLYPH_INFO_SURFACE,
					     &scaled_glyph);
    if (status) {
	_cairo_output_stream_printf (surface->final_stream, "\t\t}\n");
	return status;
    }
    _cairo_output_stream_printf (surface->final_stream,
				 "%f %f %f %f 0 0 setcachedevice\n",
				 _cairo_fixed_to_double (scaled_glyph->bbox.p1.x),
				 -_cairo_fixed_to_double (scaled_glyph->bbox.p2.y),
				 _cairo_fixed_to_double (scaled_glyph->bbox.p2.x),
				 -_cairo_fixed_to_double (scaled_glyph->bbox.p1.y));
    
    info.output_stream = surface->final_stream;
    info.has_current_point = FALSE;

    status = _cairo_path_fixed_interpret (scaled_glyph->path,
					  CAIRO_DIRECTION_FORWARD,
					  _cairo_ps_surface_path_move_to,
					  _cairo_ps_surface_path_line_to,
					  _cairo_ps_surface_path_curve_to,
					  _cairo_ps_surface_path_close_path,
					  &info);
    
    _cairo_output_stream_printf (surface->final_stream,
				 "F\n");
    
    _cairo_output_stream_printf (surface->final_stream,
				 "\t\t}\n");
    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_ps_surface_emit_font (void *entry, void *closure)
{
    cairo_ps_font_t *ps_font = entry;
    cairo_ps_surface_t *surface = closure;
    cairo_ps_font_glyph_select_t glyph_select;
    cairo_ps_glyph_t *ps_glyphs[256], *ps_glyph;
    int glyph, numglyph;
    int subfont, nsubfont;

    _cairo_output_stream_printf (surface->final_stream,
				 "%% _cairo_ps_surface_emit_font\n");
    nsubfont = (ps_font->max_glyph >> 8) + 1;
    for (subfont = 0; subfont < nsubfont; subfont++) {
	_cairo_output_stream_printf (surface->final_stream,
				     "/CairoFont-%d-%d <<\n",
				     ps_font->output_font,
				     subfont);
	memset (ps_glyphs, '\0', sizeof (ps_glyphs));
        glyph_select.glyphs = ps_glyphs;
	glyph_select.numglyph = 0;
	glyph_select.subfont = subfont;
	_cairo_hash_table_foreach (ps_font->glyphs, 
				   _cairo_ps_font_select_glyphs,
				   &glyph_select);
	_cairo_output_stream_printf (surface->final_stream,
				     "\t/FontType\t3\n"
				     "\t/FontMatrix\t[1 0 0 1 0 0]\n"
				     "\t/Encoding\t[0]\n"
				     "\t/FontBBox\t[0 0 10 10]\n"
				     "\t/Glyphs [\n");
	numglyph = glyph_select.numglyph;
	for (glyph = 0; glyph < numglyph; glyph++) {
	    ps_glyph = ps_glyphs[glyph];
	    if (ps_glyph) {
		_cairo_ps_surface_emit_glyph (surface,
					      ps_font,
					      ps_glyph);
	    } else {
		_cairo_output_stream_printf (surface->final_stream,
					     "\t\t{ } %% %d\n", glyph);
	    }
	    _cairo_ps_font_destroy_glyph (ps_glyph, ps_font);
	}
	_cairo_output_stream_printf (surface->final_stream,
				     "\t]\n"
				     "\t/BuildChar {\n"
				     "\t\texch /Glyphs get\n"
				     "\t\texch get exec\n"
				     "\t}\n"
				     ">> definefont pop\n");
    }
    _cairo_ps_surface_destroy_font (surface, ps_font);
}


static void
_cairo_ps_surface_emit_fonts (cairo_ps_surface_t *surface)
{
    _cairo_hash_table_foreach (surface->fonts, 
			       _cairo_ps_surface_emit_font,
			       surface);
    _cairo_hash_table_destroy (surface->fonts);
    surface->fonts = NULL;
}

static void
_cairo_ps_surface_emit_body (cairo_ps_surface_t *surface)
{
    char    buf[4096];
    int	    n;
    
    rewind (surface->tmpfile);
    while ((n = fread (buf, 1, sizeof (buf), surface->tmpfile)) > 0)
	_cairo_output_stream_write (surface->final_stream, buf, n);
}

static void
_cairo_ps_surface_emit_footer (cairo_ps_surface_t *surface)
{
    _cairo_output_stream_printf (surface->final_stream,
				 "%%%%Trailer\n"
				 "%%%%EOF\n");
}

static cairo_surface_t *
_cairo_ps_surface_create_for_stream_internal (cairo_output_stream_t *stream,
					      double		     width,
					      double		     height)
{
    cairo_status_t status;
    cairo_ps_surface_t *surface;

    surface = malloc (sizeof (cairo_ps_surface_t));
    if (surface == NULL) {
	_cairo_error (CAIRO_STATUS_NO_MEMORY);
	return (cairo_surface_t*) &_cairo_surface_nil;
    }

    _cairo_surface_init (&surface->base, &cairo_ps_surface_backend);

    surface->final_stream = stream;

    surface->tmpfile = tmpfile ();
    surface->stream = _cairo_output_stream_create_for_file (surface->tmpfile);
    status = _cairo_output_stream_get_status (surface->stream);
    if (status) {
	fclose (surface->tmpfile);
	free (surface);
	_cairo_error (status);
	return (cairo_surface_t*) &_cairo_surface_nil;
    }

    surface->fonts = _cairo_hash_table_create (_cairo_ps_font_equal);
    if (!surface->fonts) {
	_cairo_output_stream_destroy (surface->stream);
	fclose (surface->tmpfile);
	free (surface);
	_cairo_error (CAIRO_STATUS_NO_MEMORY);
	return (cairo_surface_t*) &_cairo_surface_nil;
    }
    surface->max_font = 0;
    
    surface->width  = width;
    surface->height = height;
    surface->x_dpi = PS_SURFACE_DPI_DEFAULT;
    surface->y_dpi = PS_SURFACE_DPI_DEFAULT;
    surface->paginated_mode = CAIRO_PAGINATED_MODE_ANALYZE;

    surface->need_start_page = TRUE;
    surface->num_pages = 0;

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

    stream = _cairo_output_stream_create_for_filename (filename);
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

}

/* A word wrap stream can be used as a filter to do word wrapping on
 * top of an existing output stream. The word wrapping is quite
 * simple, using isspace to determine characters that separate
 * words. Any word that will cause the column count exceeed the given
 * max_column will have a '\n' character emitted before it.
 *
 * The stream is careful to maintain integrity for words that cross
 * the boundary from one call to write to the next.
 *
 * Note: This stream does not guarantee that the output will never
 * exceed max_column. In particular, if a single word is larger than
 * max_column it will not be broken up.
 */
typedef struct _word_wrap_stream {
    cairo_output_stream_t *output;
    int max_column;
    int column;
    cairo_bool_t last_write_was_space;
} word_wrap_stream_t;

static int
_count_word_up_to (const unsigned char *s, int length)
{
    int word = 0;

    while (length--) {
	if (! isspace (*s++))
	    word++;
	else
	    return word;
    }

    return word;
}

static cairo_status_t
_word_wrap_stream_write (void			*closure,
			 const unsigned char	*data,
			 unsigned int		 length)
{
    word_wrap_stream_t *stream = closure;
    cairo_bool_t newline;
    int word;

    while (length) {
	if (isspace (*data)) {
	    newline =  (*data == '\n' || *data == '\r');
	    if (! newline && stream->column >= stream->max_column) {
		_cairo_output_stream_printf (stream->output, "\n");
		stream->column = 0;
	    }
	    _cairo_output_stream_write (stream->output, data, 1);
	    data++;
	    length--;
	    if (newline)
		stream->column = 0;
	    else
		stream->column++;
	    stream->last_write_was_space = TRUE;
	} else {
	    word = _count_word_up_to (data, length);
	    /* Don't wrap if this word is a continuation of a word
	     * from a previous call to write. */
	    if (stream->column + word >= stream->max_column &&
		stream->last_write_was_space)
	    {
		_cairo_output_stream_printf (stream->output, "\n");
		stream->column = 0;
	    }
	    _cairo_output_stream_write (stream->output, data, word);
	    data += word;
	    length -= word;
	    stream->column += word;
	    stream->last_write_was_space = FALSE;
	}
    }

    return _cairo_output_stream_get_status (stream->output);
}

static cairo_output_stream_t *
_word_wrap_stream_create (cairo_output_stream_t *output, int max_column)
{
    word_wrap_stream_t *stream;

    stream = malloc (sizeof (word_wrap_stream_t));
    if (stream == NULL)
	return (cairo_output_stream_t *) &cairo_output_stream_nil;

    stream->output = output;
    stream->max_column = max_column;
    stream->column = 0;
    stream->last_write_was_space = FALSE;

    return _cairo_output_stream_create (_word_wrap_stream_write,
					NULL, stream);
}

static cairo_status_t
_cairo_ps_surface_finish (void *abstract_surface)
{
    cairo_status_t status;
    cairo_ps_surface_t *surface = abstract_surface;
    cairo_output_stream_t *final_stream, *word_wrap;

    /* Save final_stream to be restored later. */
    final_stream = surface->final_stream;

    word_wrap = _word_wrap_stream_create (final_stream, 79);
    surface->final_stream = word_wrap;
   
    _cairo_ps_surface_emit_header (surface);
    
    _cairo_ps_surface_emit_fonts (surface);

    _cairo_ps_surface_emit_body (surface);

    _cairo_ps_surface_emit_footer (surface);

    _cairo_output_stream_close (surface->stream);
    status = _cairo_output_stream_get_status (surface->stream);
    _cairo_output_stream_destroy (surface->stream);

    fclose (surface->tmpfile);

    /* Restore final stream before final cleanup. */
    _cairo_output_stream_destroy (word_wrap);
    surface->final_stream = final_stream;

    _cairo_output_stream_close (surface->final_stream);
    if (status == CAIRO_STATUS_SUCCESS)
	status = _cairo_output_stream_get_status (surface->final_stream);
    _cairo_output_stream_destroy (surface->final_stream);

    return status;
}

static void
_cairo_ps_surface_start_page (cairo_ps_surface_t *surface)
{
    _cairo_output_stream_printf (surface->stream,
				 "%%%%Page: %d %d\n",
				 surface->num_pages,
				 surface->num_pages);
    surface->num_pages++;

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

/* A string_array_stream wraps an existing output stream. It takes the
 * data provided to it and output one or more consecutive string
 * objects, each within the standard PostScript implementation limit
 * of 65k characters.
 *
 * The strings are each separated by a space character for easy
 * inclusion within an array object, (but the array delimiters are not
 * added by the string_array_stream).
 *
 * The string array stream is also careful to wrap the output within
 * STRING_ARRAY_MAX_COLUMN columns (+/- 1). The stream also adds
 * necessary escaping for special characters within a string,
 * (specifically '\', '(', and ')').
 */
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
				     "%f G\n",
				     pattern->color.red);
    else
	_cairo_output_stream_printf (surface->stream,
				     "%f %f %f R\n",
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

    _cairo_output_stream_printf (stream, "0 0 M\n");
    _cairo_output_stream_printf (stream, "%f 0 L\n", surface->width);
    _cairo_output_stream_printf (stream, "%f %f L\n",
				 surface->width, surface->height);
    _cairo_output_stream_printf (stream, "0 %f L\n", surface->height);
    _cairo_output_stream_printf (stream, "P F\n");
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
	ps_operator = "F";
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

static char
hex_digit (int i)
{
    i &= 0xf;
    if (i < 10) return '0' + i;
    return 'a' + (i - 10);
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
    int i;
    int cur_subfont = -1, subfont;
    cairo_ps_font_t *ps_font;
    cairo_ps_glyph_t *ps_glyph;

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
    status = _cairo_ps_font_find (surface, scaled_font, &ps_font);
    if (status) 
	goto fallback;

    if (num_glyphs)
	emit_pattern (surface, source);

    for (i = 0; i < num_glyphs; i++) {
	status = _cairo_ps_glyph_find (ps_font, scaled_font, 
				       glyphs[i].index, &ps_glyph);
	if (status) {
	    glyphs += i;
	    num_glyphs -= i;
	    goto fallback;
	}
	subfont = ps_glyph->output_glyph >> 8;
	if (subfont != cur_subfont) {
	    _cairo_output_stream_printf (surface->stream,
					 "/CairoFont-%d-%d 1 selectfont\n",
					 ps_font->output_font,
					 subfont);
	    cur_subfont = subfont;
	}
	_cairo_output_stream_printf (surface->stream,
				     "%f %f M <%c%c> S\n",
				     glyphs[i].x, glyphs[i].y,
				     hex_digit (ps_glyph->output_glyph >> 4),
				     hex_digit (ps_glyph->output_glyph));
    }
	
    return CAIRO_STATUS_SUCCESS;

fallback:
    
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
    NULL, /* old_show_glyphs */
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
