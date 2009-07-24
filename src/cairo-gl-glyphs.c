/* Cairo - a vector graphics library with display and print output
 *
 * Copyright © 2009 Chris Wilson
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
 * The Initial Developer of the Original Code is Chris Wilson.
 */

#include "cairoint.h"

#include "cairo-gl-private.h"
#include "cairo-freelist-private.h"

#define GLYPH_CACHE_WIDTH 1024
#define GLYPH_CACHE_HEIGHT 1024
#define GLYPH_CACHE_MIN_SIZE 4
#define GLYPH_CACHE_MAX_SIZE 128

typedef struct _cairo_gl_glyph_private {
    rtree_node_t node;
    void **owner;
    struct { float x, y; } p1, p2;
} cairo_gl_glyph_private_t;

static void
_rtree_node_evict (rtree_t *rtree, rtree_node_t *node)
{
    rtree->evict (node);
    node->state = RTREE_NODE_AVAILABLE;
}

static rtree_node_t *
_rtree_node_create (rtree_t		 *rtree,
		    rtree_node_t	 *parent,
		    int			  x,
		    int			  y,
		    int			  width,
		    int			  height)
{
    rtree_node_t *node;

    /* XXX chunked freelist */
    node = _cairo_freelist_alloc (&rtree->node_freelist);
    if (node == NULL) {
	_cairo_error_throw (CAIRO_STATUS_NO_MEMORY);
	return NULL;
    }

    memset (node->children, 0, sizeof (node->children));
    node->parent = parent;
    node->state  = RTREE_NODE_AVAILABLE;
    node->locked = FALSE;
    node->x	 = x;
    node->y	 = y;
    node->width  = width;
    node->height = height;

    return node;
}

static void
_rtree_node_destroy (rtree_t *rtree, rtree_node_t *node)
{
    int i;

    if (node == NULL)
	return;

    if (node->state == RTREE_NODE_OCCUPIED) {
	_rtree_node_evict (rtree, node);
    } else {
	for (i = 0; i < 4 && node->children[i] != NULL; i++)
	    _rtree_node_destroy (rtree, node->children[i]);
    }

    _cairo_freelist_free (&rtree->node_freelist, node);
}

static cairo_int_status_t
_rtree_insert (rtree_t	     *rtree,
	       rtree_node_t  *node,
	       int	      width,
	       int	      height,
	       rtree_node_t **out)
{
    cairo_status_t status;
    int i;

    switch (node->state) {
    case RTREE_NODE_DIVIDED:
	for (i = 0; i < 4 && node->children[i] != NULL; i++) {
	    if (node->children[i]->width  >= width &&
		node->children[i]->height >= height)
	    {
		status = _rtree_insert (rtree, node->children[i],
					width, height,
					out);
		if (status != CAIRO_INT_STATUS_UNSUPPORTED)
		    return status;
	    }
	}

    default:
    case RTREE_NODE_OCCUPIED:
	return CAIRO_INT_STATUS_UNSUPPORTED;

    case RTREE_NODE_AVAILABLE:
	if (node->width  - width  > GLYPH_CACHE_MIN_SIZE ||
	    node->height - height > GLYPH_CACHE_MIN_SIZE)
	{
	    int w, h;

	    w = node->width  - width;
	    h = node->height - height;

	    i = 0;
	    node->children[i] = _rtree_node_create (rtree, node,
						    node->x, node->y,
						    width, height);
	    if (unlikely (node->children[i] == NULL))
		return _cairo_error (CAIRO_STATUS_NO_MEMORY);
	    i++;

	    if (w > GLYPH_CACHE_MIN_SIZE) {
		node->children[i] = _rtree_node_create (rtree, node,
							node->x + width,
							node->y,
							w, height);
		if (unlikely (node->children[i] == NULL))
		    return _cairo_error (CAIRO_STATUS_NO_MEMORY);
		i++;
	    }

	    if (h > GLYPH_CACHE_MIN_SIZE) {
		node->children[i] = _rtree_node_create (rtree, node,
							node->x,
							node->y + height,
							width, h);
		if (unlikely (node->children[i] == NULL))
		    return _cairo_error (CAIRO_STATUS_NO_MEMORY);
		i++;

		if (w > GLYPH_CACHE_MIN_SIZE) {
		    node->children[i] = _rtree_node_create (rtree, node,
							    node->x + width,
							    node->y + height,
							    w, h);
		    if (unlikely (node->children[i] == NULL))
			return _cairo_error (CAIRO_STATUS_NO_MEMORY);
		    i++;
		}
	    }

	    node->state = RTREE_NODE_DIVIDED;
	    node = node->children[0];
	}

	node->state = RTREE_NODE_OCCUPIED;
	*out = node;
	return CAIRO_STATUS_SUCCESS;
    }
}

static cairo_int_status_t
_rtree_add_evictable_nodes (rtree_t *rtree,
			    rtree_node_t *node,
			    int width,
			    int height,
			    cairo_array_t *evictable_nodes)
{
    cairo_int_status_t status;
    cairo_bool_t child_added = FALSE;
    int i;

    switch (node->state) {
    case RTREE_NODE_DIVIDED:
	for (i = 0; i < 4 && node->children[i] != NULL; i++) {
	    if (node->children[i]->width  >= width &&
		node->children[i]->height >= height)
	    {
		status = _rtree_add_evictable_nodes (rtree, node->children[i],
						     width, height,
						     evictable_nodes);
		if (_cairo_status_is_error (status))
		    return status;

		child_added |= status == CAIRO_STATUS_SUCCESS;
	    }
	}
	if (child_added)
	    return CAIRO_STATUS_SUCCESS;

	/* fall through */
    case RTREE_NODE_AVAILABLE:
    case RTREE_NODE_OCCUPIED:
	if (node->locked)
	    return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    return _cairo_array_append (evictable_nodes, &node);
}

static uint32_t
hars_petruska_f54_1_random (void)
{
#define rol(x,k) ((x << k) | (x >> (32-k)))
    static uint32_t x;
    return x = (x ^ rol (x, 5) ^ rol (x, 24)) + 0x37798849;
#undef rol
}

static cairo_int_status_t
_rtree_evict_random (rtree_t	*rtree,
		     rtree_node_t *root,
		     int	 width,
		     int	 height,
		     rtree_node_t **out)
{
    cairo_array_t evictable_nodes;
    cairo_status_t status;
    int i;

    _cairo_array_init (&evictable_nodes, sizeof (rtree_node_t *));

    status = _rtree_add_evictable_nodes (rtree, root,
					 width, height,
					 &evictable_nodes);
    if (status == CAIRO_STATUS_SUCCESS) {
	rtree_node_t *node;

	node = *(rtree_node_t **)
	    _cairo_array_index (&evictable_nodes,
				hars_petruska_f54_1_random () % evictable_nodes.num_elements);
	if (node->state == RTREE_NODE_OCCUPIED) {
	    _rtree_node_evict (rtree, node);
	} else {
	    for (i = 0; i < 4 && node->children[i] != NULL; i++) {
		_rtree_node_destroy (rtree, node->children[i]);
		node->children[i] = NULL;
	    }
	}

	node->state = RTREE_NODE_AVAILABLE;
	*out = node;
    }

    _cairo_array_fini (&evictable_nodes);

    return status;
}

static void *
_rtree_lock (rtree_t *rtree, rtree_node_t *node)
{
    void *ptr = node;

    while (node != NULL && ! node->locked) {
	node->locked = TRUE;
	node = node->parent;
    }

    return ptr;
}

static void
_rtree_unlock (rtree_t *rtree, rtree_node_t *node)
{
    int i;

    if (! node->locked)
	return;

    node->locked = FALSE;
    if (node->state == RTREE_NODE_DIVIDED) {
	for (i = 0; i < 4 && node->children[i] != NULL; i++)
	    _rtree_unlock (rtree, node->children[i]);
    }
}

static void
_rtree_init (rtree_t	    *rtree,
	     int	     width,
	     int	     height,
	     int             node_size,
	     void (*evict) (void *node))
{
    rtree->evict = evict;

    assert (node_size >= (int) sizeof (rtree_node_t));
    _cairo_freelist_init (&rtree->node_freelist, node_size);

    memset (&rtree->root, 0, sizeof (rtree->root));
    rtree->root.width = width;
    rtree->root.height = height;
}

static void
_rtree_fini (rtree_t *rtree)
{
    int i;

    if (rtree->root.state == RTREE_NODE_OCCUPIED) {
	_rtree_node_evict (rtree, &rtree->root);
    } else {
	for (i = 0; i < 4 && rtree->root.children[i] != NULL; i++)
	    _rtree_node_destroy (rtree, rtree->root.children[i]);
    }

    _cairo_freelist_fini (&rtree->node_freelist);
}

static void
_glyph_evict (void *node)
{
    cairo_gl_glyph_private_t *glyph_private = node;

    if (glyph_private->owner != NULL)
	*glyph_private->owner = NULL;
}

static cairo_status_t
_cairo_gl_glyph_cache_add_glyph (cairo_gl_glyph_cache_t *cache,
				 cairo_scaled_glyph_t  *scaled_glyph)
{
    cairo_image_surface_t *glyph_surface = scaled_glyph->surface;
    cairo_gl_glyph_private_t *glyph_private;
    rtree_node_t *node = NULL;
    cairo_status_t status;
    int width, height;
    GLenum internal_format, format, type;
    cairo_bool_t has_alpha;

    status = _cairo_gl_get_image_format_and_type (glyph_surface->pixman_format,
						  &internal_format,
						  &format,
						  &type,
						  &has_alpha);
    if (status != CAIRO_STATUS_SUCCESS)
	return status;

    width = glyph_surface->width;
    if (width < GLYPH_CACHE_MIN_SIZE)
	width = GLYPH_CACHE_MIN_SIZE;
    height = glyph_surface->height;
    if (height < GLYPH_CACHE_MIN_SIZE)
	height = GLYPH_CACHE_MIN_SIZE;

    /* search for an available slot */
    status = _rtree_insert (&cache->rtree, &cache->rtree.root,
			    width, height, &node);
    /* search for an unlocked slot */
    if (status == CAIRO_INT_STATUS_UNSUPPORTED) {
	status = _rtree_evict_random (&cache->rtree, &cache->rtree.root,
				      width, height, &node);
	if (status == CAIRO_STATUS_SUCCESS)
	    status = _rtree_insert (&cache->rtree, node, width, height, &node);
    }
    if (status)
	return status;

    glPixelStorei (GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei (GL_UNPACK_ROW_LENGTH,
		   glyph_surface->stride /
		   (PIXMAN_FORMAT_BPP (glyph_surface->pixman_format) / 8));
    glTexSubImage2D (GL_TEXTURE_2D, 0,
		     node->x, node->y,
		     glyph_surface->width, glyph_surface->height,
		     format, type,
		     glyph_surface->data);
    glPixelStorei (GL_UNPACK_ROW_LENGTH, 0);

    scaled_glyph->surface_private = node;

    glyph_private = (cairo_gl_glyph_private_t *) node;
    glyph_private->owner = &scaled_glyph->surface_private;

    /* compute tex coords */
    glyph_private->p1.x = node->x / (double) cache->width;
    glyph_private->p1.y = node->y / (double) cache->height;
    glyph_private->p2.x =
	(node->x + glyph_surface->width) / (double) cache->width;
    glyph_private->p2.y =
	(node->y + glyph_surface->height) / (double) cache->height;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_gl_glyph_private_t *
_cairo_gl_glyph_cache_lock (cairo_gl_glyph_cache_t *cache,
			    cairo_scaled_glyph_t *scaled_glyph)
{
    return _rtree_lock (&cache->rtree, scaled_glyph->surface_private);
}

static cairo_status_t
cairo_gl_glyph_cache_init (cairo_gl_glyph_cache_t *cache,
			   cairo_gl_context_t *ctx,
			   cairo_format_t format,
			   int width, int height)
{
    cairo_content_t content;
    GLenum internal_format;

    assert ((width & 3) == 0);
    assert ((height & 1) == 0);
    cache->width = width;
    cache->height = height;

    switch (format) {
    case CAIRO_FORMAT_A1:
    case CAIRO_FORMAT_RGB24:
	ASSERT_NOT_REACHED;
    case CAIRO_FORMAT_ARGB32:
	content = CAIRO_CONTENT_COLOR_ALPHA;
	internal_format = GL_RGBA;
	break;
    case CAIRO_FORMAT_A8:
	content = CAIRO_CONTENT_ALPHA;
	internal_format = GL_ALPHA;
	break;
    }

    glGenTextures (1, &cache->tex);
    glBindTexture (GL_TEXTURE_2D, cache->tex);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D (GL_TEXTURE_2D, 0, internal_format,
		  width, height, 0, internal_format, GL_FLOAT, NULL);

    _rtree_init (&cache->rtree,
		 width, height,
		 sizeof (cairo_gl_glyph_private_t),
		 _glyph_evict);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
cairo_gl_context_get_glyph_cache (cairo_gl_context_t *ctx,
				  cairo_format_t format,
				  cairo_gl_glyph_cache_t **out)
{
    cairo_gl_glyph_cache_t *cache;
    cairo_status_t status;

    switch (format) {
    case CAIRO_FORMAT_ARGB32:
    case CAIRO_FORMAT_RGB24:
	cache = &ctx->glyph_cache[0];
	format = CAIRO_FORMAT_ARGB32;
	break;
    case CAIRO_FORMAT_A8:
    case CAIRO_FORMAT_A1:
	cache = &ctx->glyph_cache[1];
	format = CAIRO_FORMAT_A8;
	break;
    }

    if (unlikely (cache->tex == 0)) {
	status = cairo_gl_glyph_cache_init (cache, ctx, format,
					    GLYPH_CACHE_WIDTH,
					    GLYPH_CACHE_HEIGHT);
	if (unlikely (status))
	    return status;
    }

    *out = cache;
    return CAIRO_STATUS_SUCCESS;
}

static cairo_bool_t
_cairo_gl_surface_owns_font (cairo_gl_surface_t *surface,
			     cairo_scaled_font_t *scaled_font)
{
    cairo_gl_context_t *font_private;

    font_private = scaled_font->surface_private;
    if ((scaled_font->surface_backend != NULL &&
	 scaled_font->surface_backend != &_cairo_gl_surface_backend) ||
	(font_private != NULL && font_private != surface->ctx))
    {
	return FALSE;
    }

    return TRUE;
}

void
_cairo_gl_surface_scaled_glyph_fini (cairo_scaled_glyph_t *scaled_glyph,
				     cairo_scaled_font_t  *scaled_font)
{
    cairo_gl_glyph_private_t *glyph_private;

    glyph_private = scaled_glyph->surface_private;
    if (glyph_private != NULL)
	glyph_private->owner = NULL;
}

typedef struct _cairo_gl_glyphs_setup
{
    unsigned int vbo_size; /* units of floats */
    unsigned int vb_offset; /* units of floats */
    unsigned int vertex_size; /* units of floats */
    unsigned int num_prims;
    float *vb;
    cairo_gl_composite_setup_t *composite;
} cairo_gl_glyphs_setup_t;

static void
_cairo_gl_flush_glyphs (cairo_gl_context_t *ctx,
			cairo_gl_glyphs_setup_t *setup)
{
    int i;

    if (setup->vb != NULL) {
	glUnmapBufferARB (GL_ARRAY_BUFFER_ARB);
	setup->vb = NULL;

	if (setup->num_prims != 0) {
	    glDrawArrays (GL_QUADS, 0, 4 * setup->num_prims);
	    setup->num_prims = 0;
	}
    }

    for (i = 0; i < ARRAY_LENGTH (ctx->glyph_cache); i++) {
	_rtree_unlock (&ctx->glyph_cache[i].rtree,
		       &ctx->glyph_cache[i].rtree.root);
    }
}

static void
_cairo_gl_glyphs_emit_vertex (cairo_gl_glyphs_setup_t *setup,
			      int x, int y, float glyph_x, float glyph_y)
{
    int i = 0;
    float *vb = &setup->vb[setup->vb_offset];
    cairo_surface_attributes_t *src_attributes;

    src_attributes = &setup->composite->src.operand.texture.attributes;

    vb[i++] = x;
    vb[i++] = y;

    vb[i++] = glyph_x;
    vb[i++] = glyph_y;

    if (setup->composite->src.type == OPERAND_TEXTURE) {
	double s = x;
	double t = y;
	cairo_matrix_transform_point (&src_attributes->matrix, &s, &t);
	vb[i++] = s;
	vb[i++] = t;
    }

    setup->vb_offset += setup->vertex_size;
}


static void
_cairo_gl_emit_glyph_rectangle (cairo_gl_context_t *ctx,
				cairo_gl_glyphs_setup_t *setup,
				int x1, int y1,
				int x2, int y2,
				cairo_gl_glyph_private_t *glyph)
{
    if (setup->vb != NULL &&
	setup->vb_offset + 4 * setup->vertex_size > setup->vbo_size) {
	_cairo_gl_flush_glyphs (ctx, setup);
    }

    if (setup->vb == NULL) {
	glBufferDataARB (GL_ARRAY_BUFFER_ARB,
			 setup->vbo_size * sizeof (GLfloat),
			 NULL, GL_STREAM_DRAW_ARB);
	setup->vb = glMapBufferARB (GL_ARRAY_BUFFER_ARB, GL_WRITE_ONLY_ARB);
	setup->vb_offset = 0;
    }

    _cairo_gl_glyphs_emit_vertex (setup, x1, y1, glyph->p1.x, glyph->p1.y);
    _cairo_gl_glyphs_emit_vertex (setup, x2, y1, glyph->p2.x, glyph->p1.y);
    _cairo_gl_glyphs_emit_vertex (setup, x2, y2, glyph->p2.x, glyph->p2.y);
    _cairo_gl_glyphs_emit_vertex (setup, x1, y2, glyph->p1.x, glyph->p2.y);
    setup->num_prims++;
}

cairo_int_status_t
_cairo_gl_surface_show_glyphs (void			*abstract_dst,
			       cairo_operator_t		 op,
			       const cairo_pattern_t	*source,
			       cairo_glyph_t		*glyphs,
			       int			 num_glyphs,
			       cairo_scaled_font_t	*scaled_font,
			       cairo_clip_t		*clip,
			       int			*remaining_glyphs)
{
    cairo_gl_surface_t *dst = abstract_dst;
    cairo_gl_context_t *ctx;
    cairo_rectangle_int_t extents;
    cairo_format_t last_format = (cairo_format_t) -1;
    cairo_gl_glyph_cache_t *cache = NULL;
    cairo_status_t status;
    int i = 0;
    cairo_region_t *clip_region = NULL;
    cairo_solid_pattern_t solid_pattern;
    cairo_gl_glyphs_setup_t setup;
    cairo_gl_composite_setup_t composite_setup;
    GLuint vbo = 0;

    /* Just let unbounded operators go through the fallback code
     * instead of trying to do the fixups here */
    if (! _cairo_operator_bounded_by_mask (op))
        return CAIRO_INT_STATUS_UNSUPPORTED;

    /* For CLEAR, cairo's rendering equation (quoting Owen's description in:
     * http://lists.cairographics.org/archives/cairo/2005-August/004992.html)
     * is:
     *     mask IN clip ? src OP dest : dest
     * or more simply:
     *     mask IN CLIP ? 0 : dest
     *
     * where the ternary operator A ? B : C is (A * B) + ((1 - A) * C).
     *
     * The model we use in _cairo_gl_set_operator() is Render's:
     *     src IN mask IN clip OP dest
     * which would boil down to:
     *     0 (bounded by the extents of the drawing).
     *
     * However, we can do a Render operation using an opaque source
     * and DEST_OUT to produce:
     *    1 IN mask IN clip DEST_OUT dest
     * which is
     *    mask IN clip ? 0 : dest
     */
    if (op == CAIRO_OPERATOR_CLEAR) {
	_cairo_pattern_init_solid (&solid_pattern, CAIRO_COLOR_WHITE,
				   CAIRO_CONTENT_COLOR);
	source = &solid_pattern.base;
	op = CAIRO_OPERATOR_DEST_OUT;
    }

    /* For SOURCE, cairo's rendering equation is:
     *     (mask IN clip) ? src OP dest : dest
     * or more simply:
     *     (mask IN clip) ? src : dest.
     *
     * If we just used the Render equation, we would get:
     *     (src IN mask IN clip) OP dest
     * or:
     *     (src IN mask IN clip) bounded by extents of the drawing.
     *
     * The trick is that for GL blending, we only get our 4 source values
     * into the blender, and since we need all 4 components of source, we
     * can't also get the mask IN clip into the blender.  But if we did
     * two passes we could make it work:
     *     dest = (mask IN clip) DEST_OUT dest
     *     dest = src IN mask IN clip ADD dest
     *
     * But for now, fall back :)
     */
    if (op == CAIRO_OPERATOR_SOURCE)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    /* XXX we don't need ownership of the font as we use a global
     * glyph cache -- but we do need scaled_glyph eviction notification. :-(
     */
    if (! _cairo_gl_surface_owns_font (dst, scaled_font))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    /* XXX If glyphs overlap, build tmp mask and composite.
     * Could we use the stencil instead but only write if alpha !=0 ?
     * TEXKILL? PIXELKILL?
     * Antialiasing issues - but using glyph images cause their own anyway.
     */

    status = _cairo_scaled_font_glyph_device_extents (scaled_font,
						      glyphs, num_glyphs,
						      &extents);
    if (unlikely (status))
	return status;

    status = _cairo_gl_operand_init (&composite_setup.src, source, dst,
				     extents.x, extents.y,
				     extents.x, extents.y,
				     extents.width, extents.height);
    if (unlikely (status))
	return status;

    ctx = _cairo_gl_context_acquire (dst->ctx);

    /* Set up the mask to source from the incoming vertex color. */
    glActiveTexture (GL_TEXTURE1);
    glEnable (GL_TEXTURE_2D);
    /* IN: dst.argb = src.argb * mask.aaaa */
    glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
    glTexEnvi (GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
    glTexEnvi (GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_MODULATE);

    glTexEnvi (GL_TEXTURE_ENV, GL_SRC0_RGB, GL_PREVIOUS);
    glTexEnvi (GL_TEXTURE_ENV, GL_SRC0_ALPHA, GL_PREVIOUS);
    glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
    glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);

    glTexEnvi (GL_TEXTURE_ENV, GL_SRC1_RGB, GL_TEXTURE1);
    glTexEnvi (GL_TEXTURE_ENV, GL_SRC1_ALPHA, GL_TEXTURE1);
    glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_ALPHA);
    glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND1_ALPHA, GL_SRC_ALPHA);

    _cairo_gl_set_destination (dst);

    status = _cairo_gl_set_operator (dst, op);
    if (status != CAIRO_STATUS_SUCCESS)
	goto CLEANUP_CONTEXT;

    _cairo_gl_set_src_operand (ctx, &composite_setup);

    _cairo_scaled_font_freeze_cache (scaled_font);
    if (! _cairo_gl_surface_owns_font (dst, scaled_font))
	goto CLEANUP_CONTEXT;

    if (scaled_font->surface_private == NULL) {
	/* XXX couple into list to remove on context destruction */
	scaled_font->surface_private = ctx;
	scaled_font->surface_backend = &_cairo_gl_surface_backend;
    }

    /* Create our VBO so that we can accumulate a bunch of glyph primitives
     * into one giant DrawArrays.
     */
    memset(&setup, 0, sizeof(setup));
    setup.composite = &composite_setup;
    setup.vertex_size = 4;
    if (composite_setup.src.type == OPERAND_TEXTURE)
	setup.vertex_size += 2;
    setup.vbo_size = num_glyphs * 4 * setup.vertex_size;
    if (setup.vbo_size > 4096)
	setup.vbo_size = 4096;

    glGenBuffersARB (1, &vbo);
    glBindBufferARB (GL_ARRAY_BUFFER_ARB, vbo);

    glVertexPointer (2, GL_FLOAT, setup.vertex_size * sizeof (GLfloat),
		     (void *)(uintptr_t)(0));
    glEnableClientState (GL_VERTEX_ARRAY);
    if (composite_setup.src.type == OPERAND_TEXTURE) {
	/* Note that we're packing texcoord 0 after texcoord 1, for
	 * convenience.
	 */
	glClientActiveTexture (GL_TEXTURE0);
	glTexCoordPointer (2, GL_FLOAT, setup.vertex_size * sizeof (GLfloat),
			   (void *)(uintptr_t)(4 * sizeof (GLfloat)));
	glEnableClientState (GL_TEXTURE_COORD_ARRAY);
    }
    glClientActiveTexture (GL_TEXTURE1);
    glTexCoordPointer (2, GL_FLOAT, setup.vertex_size * sizeof (GLfloat),
		       (void *)(uintptr_t)(2 * sizeof (GLfloat)));
    glEnableClientState (GL_TEXTURE_COORD_ARRAY);

    for (i = 0; i < num_glyphs; i++) {
	cairo_scaled_glyph_t *scaled_glyph;
	double x_offset, y_offset;
	double x1, x2, y1, y2;

	status = _cairo_scaled_glyph_lookup (scaled_font,
					     glyphs[i].index,
					     CAIRO_SCALED_GLYPH_INFO_SURFACE,
					     &scaled_glyph);
	if (unlikely (status))
	    goto FINISH;

	if (scaled_glyph->surface->width  == 0 ||
	    scaled_glyph->surface->height == 0)
	{
	    continue;
	}
	if (scaled_glyph->surface->width  > GLYPH_CACHE_MAX_SIZE ||
	    scaled_glyph->surface->height > GLYPH_CACHE_MAX_SIZE)
	{
	    status = CAIRO_INT_STATUS_UNSUPPORTED;
	    goto FINISH;
	}

	if (scaled_glyph->surface->format != last_format) {
	    /* Switching textures, so flush any queued prims. */
	    _cairo_gl_flush_glyphs (ctx, &setup);

	    glActiveTexture (GL_TEXTURE1);
	    status = cairo_gl_context_get_glyph_cache (ctx,
						       scaled_glyph->surface->format,
						       &cache);
	    if (unlikely (status))
		goto FINISH;

	    glBindTexture (GL_TEXTURE_2D, cache->tex);

	    last_format = scaled_glyph->surface->format;
	}

	if (scaled_glyph->surface_private == NULL) {
	    status = _cairo_gl_glyph_cache_add_glyph (cache, scaled_glyph);
	    if (unlikely (_cairo_status_is_error (status)))
		goto FINISH;

	    if (status == CAIRO_INT_STATUS_UNSUPPORTED) {
		/* Cache is full, so flush existing prims and try again. */
		_cairo_gl_flush_glyphs (ctx, &setup);
	    }

	    status = _cairo_gl_glyph_cache_add_glyph (cache, scaled_glyph);
	    if (unlikely (status))
		goto FINISH;
	}

	x_offset = scaled_glyph->surface->base.device_transform.x0;
	y_offset = scaled_glyph->surface->base.device_transform.y0;

	x1 = _cairo_lround (glyphs[i].x - x_offset);
	y1 = _cairo_lround (glyphs[i].y - y_offset);
	x2 = x1 + scaled_glyph->surface->width;
	y2 = y1 + scaled_glyph->surface->height;

	_cairo_gl_emit_glyph_rectangle (ctx, &setup,
					x1, y1, x2, y2,
					_cairo_gl_glyph_cache_lock (cache, scaled_glyph));
    }

    status = CAIRO_STATUS_SUCCESS;
  FINISH:
    _cairo_gl_flush_glyphs (ctx, &setup);
    _cairo_scaled_font_thaw_cache (scaled_font);

  CLEANUP_CONTEXT:
    glDisable (GL_BLEND);

    glDisableClientState (GL_VERTEX_ARRAY);

    glClientActiveTexture (GL_TEXTURE0);
    glDisableClientState (GL_TEXTURE_COORD_ARRAY);
    glActiveTexture (GL_TEXTURE0);
    glDisable (GL_TEXTURE_2D);

    glClientActiveTexture (GL_TEXTURE1);
    glDisableClientState (GL_TEXTURE_COORD_ARRAY);
    glActiveTexture (GL_TEXTURE1);
    glDisable (GL_TEXTURE_2D);

    if (vbo != 0) {
	glBindBufferARB (GL_ARRAY_BUFFER_ARB, 0);
	glDeleteBuffersARB (1, &vbo);
    }

    _cairo_gl_context_release (ctx);

    _cairo_gl_operand_destroy (&composite_setup.src);

    *remaining_glyphs = num_glyphs - i;
    return status;
}

void
_cairo_gl_glyph_cache_fini (cairo_gl_glyph_cache_t *cache)
{
    if (cache->tex == 0)
	return;

    glDeleteTextures (1, &cache->tex);

    _rtree_fini (&cache->rtree);
}
