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

static cairo_glyph_cache_t *
_cairo_glyph_cache_create (void);

static void
_cairo_glyph_cache_destroy (cairo_glyph_cache_t *glyph_cache);

static void
_cairo_glyph_cache_reference (cairo_glyph_cache_t *glyph_cache);

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
    font->glyph_cache = _cairo_glyph_cache_create ();
    if (font->glyph_cache == NULL)
	return CAIRO_STATUS_NO_MEMORY;
    
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

    if (newfont->glyph_cache)
	_cairo_glyph_cache_destroy (newfont->glyph_cache);
    
    newfont->glyph_cache = font->glyph_cache;
    _cairo_glyph_cache_reference (font->glyph_cache);
    
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
_cairo_font_text_bbox (cairo_font_t             *font,
                       cairo_surface_t          *surface,
                       double                   x,
                       double                   y,
                       const unsigned char      *utf8,
		       cairo_box_t		*bbox)
{
    return font->backend->text_bbox (font, surface, x, y, utf8, bbox);
}

cairo_status_t
_cairo_font_glyph_bbox (cairo_font_t            *font,
			cairo_surface_t         *surface,
			cairo_glyph_t           *glyphs,
			int                     num_glyphs,
			cairo_box_t		*bbox)
{
    return font->backend->glyph_bbox (font, surface, glyphs, num_glyphs, bbox);
}

cairo_status_t
_cairo_font_show_text (cairo_font_t		*font,
		       cairo_operator_t		operator,
		       cairo_surface_t		*source,
		       cairo_surface_t		*surface,
		       int                      source_x,
		       int                      source_y,
		       double			x,
		       double			y,
		       const unsigned char	*utf8)
{
    return font->backend->show_text(font, operator, source, 
				    surface, source_x, source_y, x, y, utf8);
}

cairo_status_t
_cairo_font_show_glyphs (cairo_font_t           *font,
                         cairo_operator_t       operator,
                         cairo_surface_t        *source,
                         cairo_surface_t        *surface,
			 int                    source_x,
			 int                    source_y,
                         cairo_glyph_t          *glyphs,
                         int                    num_glyphs)
{
    return font->backend->show_glyphs(font, operator, source, 
				      surface, source_x, source_y,
				      glyphs, num_glyphs);
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

static void
_cairo_glyph_cache_pop_last (cairo_glyph_cache_t *glyph_cache)
{
    if (glyph_cache->last) {
	cairo_glyph_surface_node_t *remove = glyph_cache->last;
	
	cairo_surface_destroy (remove->s.surface);
	glyph_cache->last = remove->prev;
	if (glyph_cache->last)
	    glyph_cache->last->next = NULL;

	free (remove);
	glyph_cache->n_nodes--;
    }
}

static cairo_glyph_cache_t *
_cairo_glyph_cache_create (void)
{
    cairo_glyph_cache_t *glyph_cache;
	
    glyph_cache = malloc (sizeof (cairo_glyph_cache_t));
    if (glyph_cache == NULL)
	return NULL;
    
    glyph_cache->n_nodes = 0;
    glyph_cache->first = NULL;
    glyph_cache->last = NULL;
    glyph_cache->cache_size = CAIRO_FONT_CACHE_SIZE_DEFAULT;
    glyph_cache->ref_count = 1;

    return glyph_cache;
}

static void
_cairo_glyph_cache_reference (cairo_glyph_cache_t *glyph_cache)
{
    if (glyph_cache == NULL)
	return;

    glyph_cache->ref_count++;
}

static void
_cairo_glyph_cache_destroy (cairo_glyph_cache_t *glyph_cache)
{
    if (glyph_cache == NULL)
	return;

    glyph_cache->ref_count--;
    if (glyph_cache->ref_count)
	return;

    while (glyph_cache->last)
	_cairo_glyph_cache_pop_last (glyph_cache);

    free (glyph_cache);
}

static void
_cairo_glyph_surface_init (cairo_font_t *font,
			   cairo_surface_t *surface,
			   const cairo_glyph_t *glyph,
			   cairo_glyph_surface_t *glyph_surface)
{
    cairo_surface_t *image;
    
    glyph_surface->surface = NULL;
    glyph_surface->index = glyph->index;
    glyph_surface->matrix[0][0] = font->matrix.m[0][0];
    glyph_surface->matrix[0][1] = font->matrix.m[0][1];
    glyph_surface->matrix[1][0] = font->matrix.m[1][0];
    glyph_surface->matrix[1][1] = font->matrix.m[1][1];

    image = font->backend->create_glyph (font, glyph, &glyph_surface->size);
    if (image == NULL)
	return;
    
    if (surface->backend != image->backend) {
	cairo_status_t status;
	
	glyph_surface->surface =
	    _cairo_surface_create_similar_scratch (surface,
						   CAIRO_FORMAT_A8, 0,
						   glyph_surface->size.width,
						   glyph_surface->size.height);
	if (glyph_surface->surface == NULL) {
	    glyph_surface->surface = image;
	    return;
	}
	    
	status = _cairo_surface_set_image (glyph_surface->surface,
					   (cairo_image_surface_t *) image);
	if (status) {
	    cairo_surface_destroy (glyph_surface->surface);
	    glyph_surface->surface = NULL;
	}
	cairo_surface_destroy (image);
    } else
	glyph_surface->surface = image;
}

cairo_surface_t *
_cairo_font_lookup_glyph (cairo_font_t *font,
			  cairo_surface_t *surface,
			  const cairo_glyph_t *glyph,
			  cairo_glyph_size_t *return_size)
{
    cairo_glyph_surface_t glyph_surface;
    cairo_glyph_cache_t *cache = font->glyph_cache;
    cairo_glyph_surface_node_t *node;
	
    for (node = cache->first; node != NULL; node = node->next) {
	cairo_glyph_surface_t *s = &node->s;

	if ((s->surface == NULL || s->surface->backend == surface->backend) &&
	    s->index == glyph->index &&
	    s->matrix[0][0] == font->matrix.m[0][0] &&
	    s->matrix[0][1] == font->matrix.m[0][1] &&
	    s->matrix[1][0] == font->matrix.m[1][0] &&
	    s->matrix[1][1] == font->matrix.m[1][1]) {

	    /* move node first in cache */
	    if (node->prev) {
		if (node->next == NULL) {    
		    cache->last = node->prev;
		    node->prev->next = NULL;
		} else {
		    node->prev->next = node->next;
		    node->next->prev = node->prev;
		}

		node->prev = NULL;
		node->next = cache->first;
		cache->first = node;
		if (node->next)
		    node->next->prev = node;
		else
		    cache->last = node;
	    }
	    
	    cairo_surface_reference (s->surface);
	    *return_size = s->size;
	    
	    return s->surface;
	}
    }
    
    _cairo_glyph_surface_init (font, surface, glyph, &glyph_surface);

    *return_size = glyph_surface.size;
    
    if (cache->cache_size > 0) {
	if (cache->n_nodes == cache->cache_size)
	    _cairo_glyph_cache_pop_last (cache);

	node = malloc (sizeof (cairo_glyph_surface_node_t));
	if (node) {
	    cairo_surface_reference (glyph_surface.surface);
	    
	    /* insert node first in cache */
	    node->s = glyph_surface;
	    node->prev = NULL;
	    node->next = cache->first;
	    cache->first = node;
	    if (node->next)
		node->next->prev = node;
	    else
		cache->last = node;

	    cache->n_nodes++;
	}
    }
    
    return glyph_surface.surface;
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

    _cairo_glyph_cache_destroy (font->glyph_cache);

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
