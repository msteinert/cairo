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

cairo_font_t *
_cairo_font_create (const char           *family, 
		    cairo_font_slant_t   slant, 
		    cairo_font_weight_t  weight)
{
    const struct cairo_font_backend *backend = CAIRO_FONT_BACKEND_DEFAULT;

    /* XXX: The current freetype backend may return NULL, (for example
     * if no fonts are installed), but I would like to guarantee that
     * the toy API always returns at least *some* font, so I would
     * like to build in some sort fo font here, (even a really lame,
     * ugly one if necessary). */

    return backend->create (family, slant, weight);
}

cairo_status_t
_cairo_font_init (cairo_font_t *font, 
		  const struct cairo_font_backend *backend)
{
    cairo_matrix_set_identity (&font->matrix);
    font->refcount = 1;
    font->backend = backend;
    
    return CAIRO_STATUS_SUCCESS;
}

cairo_font_t *
_cairo_font_copy (cairo_font_t *font)
{
    cairo_font_t *newfont = NULL;
    char *tmp = NULL;

    if (font == NULL || font->backend->copy == NULL)
	return NULL;
    
    newfont = font->backend->copy (font);
    if (newfont == NULL) {
	free (tmp);
	return NULL;
    }

    newfont->refcount = 1;
    cairo_matrix_copy(&newfont->matrix, &font->matrix);
    newfont->backend = font->backend;
    return newfont;
}

cairo_status_t
_cairo_font_scale (cairo_font_t *font, double scale)
{
    return cairo_matrix_scale (&font->matrix, scale, scale);
}

cairo_status_t
_cairo_font_transform (cairo_font_t *font, cairo_matrix_t *matrix)
{
    return cairo_matrix_multiply (&font->matrix, matrix, &font->matrix);
}


cairo_status_t
_cairo_font_text_extents (cairo_font_t *font,
			  const unsigned char *utf8,
			  cairo_text_extents_t *extents)
{
    return font->backend->text_extents(font, utf8, extents);
}

cairo_status_t
_cairo_font_glyph_extents (cairo_font_t *font,
                           cairo_glyph_t *glyphs,
                           int num_glyphs,
			   cairo_text_extents_t *extents)
{
    return font->backend->glyph_extents(font, glyphs, num_glyphs, extents);
}


cairo_status_t
_cairo_font_show_text (cairo_font_t		*font,
		       cairo_operator_t		operator,
		       cairo_surface_t		*source,
		       cairo_surface_t		*surface,
		       double			x,
		       double			y,
		       const unsigned char	*utf8)
{
    return font->backend->show_text(font, operator, source, 
				    surface, x, y, utf8);
}

cairo_status_t
_cairo_font_show_glyphs (cairo_font_t           *font,
                         cairo_operator_t       operator,
                         cairo_surface_t        *source,
                         cairo_surface_t        *surface,
                         cairo_glyph_t          *glyphs,
                         int                    num_glyphs)
{
    return font->backend->show_glyphs(font, operator, source, 
				      surface, glyphs, num_glyphs);
}

cairo_status_t
_cairo_font_text_path (cairo_font_t             *font,
		       double			x,
		       double			y,
                       const unsigned char      *utf8,
                       cairo_path_t             *path)
{
    return font->backend->text_path(font, x, y, utf8, path);
}

cairo_status_t
_cairo_font_glyph_path (cairo_font_t            *font,
                        cairo_glyph_t           *glyphs, 
                        int                     num_glyphs,
                        cairo_path_t            *path)
{
    return font->backend->glyph_path(font, glyphs, num_glyphs, path);
}

cairo_status_t
_cairo_font_font_extents (cairo_font_t *font,
			  cairo_font_extents_t *extents)
{
    return font->backend->font_extents(font, extents);
}

/* public font interface follows */

void
cairo_font_reference (cairo_font_t *font)
{
    font->refcount++;
}

void
cairo_font_destroy (cairo_font_t *font)
{
    if (--(font->refcount) > 0)
	return;

    if (font->backend->destroy)
	font->backend->destroy (font);
}

void
cairo_font_set_transform (cairo_font_t *font, 
			  cairo_matrix_t *matrix)
{
    cairo_matrix_copy (&(font->matrix), matrix);
}

void
cairo_font_current_transform (cairo_font_t *font, 
			      cairo_matrix_t *matrix)
{
    cairo_matrix_copy (matrix, &(font->matrix));
}


