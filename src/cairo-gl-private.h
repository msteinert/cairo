/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2009 Eric Anholt
 * Copyright © 2009 Chris Wilson
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
 * The Initial Developer of the Original Code is Red Hat, Inc.
 *
 * Contributor(s):
 *	Carl Worth <cworth@cworth.org>
 *	Chris Wilson <chris@chris-wilson.co.uk>
 */

#ifndef CAIRO_GL_PRIVATE_H
#define CAIRO_GL_PRIVATE_H

#include "cairoint.h"
#include "cairo-freelist-private.h"

#include <GL/glew.h>

#include "cairo-gl.h"

#include <GL/gl.h>
#define GL_GLEXT_PROTOTYPES
#include <GL/glext.h>

typedef struct _cairo_gl_surface {
    cairo_surface_t base;

    cairo_gl_context_t *ctx;
    int width, height;

    GLuint tex; /* GL texture object containing our data. */
    GLuint depth_stencil_tex;
    GLuint fb; /* GL framebuffer object wrapping our data. */
} cairo_gl_surface_t;

enum {
    RTREE_NODE_AVAILABLE,
    RTREE_NODE_DIVIDED,
    RTREE_NODE_OCCUPIED,
};
typedef struct _rtree_node {
    struct _rtree_node *children[4], *parent;
    uint16_t state;
    uint16_t locked;
    uint16_t x, y;
    uint16_t width, height;
} rtree_node_t;

typedef struct _rtree {
    rtree_node_t root;
    void (*evict) (void *node);
    cairo_freelist_t node_freelist;
} rtree_t;

typedef struct cairo_gl_glyph_cache {
    rtree_t rtree;
    GLuint tex;
    unsigned int width, height;
} cairo_gl_glyph_cache_t;

struct _cairo_gl_context {
    cairo_reference_count_t ref_count;
    cairo_status_t status;

    cairo_mutex_t mutex; /* needed? */
    GLuint dummy_tex;
    GLint max_framebuffer_size;
    GLint max_texture_size;

    cairo_gl_surface_t *current_target;
    cairo_gl_glyph_cache_t glyph_cache[2];

    void (*make_current)(void *ctx, cairo_gl_surface_t *surface);
    void (*swap_buffers)(void *ctx, cairo_gl_surface_t *surface);
    void (*destroy) (void *ctx);
};

enum cairo_gl_composite_operand_type {
    OPERAND_CONSTANT,
    OPERAND_TEXTURE,
};

/* This union structure describes a potential source or mask operand to the
 * compositing equation.
 */
typedef struct cairo_gl_composite_operand {
    enum cairo_gl_composite_operand_type type;
    union {
	struct {
	    GLuint tex;
	    cairo_gl_surface_t *surface;
	    cairo_surface_attributes_t attributes;
	    cairo_bool_t has_alpha;
	} texture;
	struct {
	    GLfloat color[4];
	} constant;
    } operand;

    const cairo_pattern_t *pattern;
} cairo_gl_composite_operand_t;

typedef struct _cairo_gl_composite_setup {
    cairo_gl_composite_operand_t src;
    cairo_gl_composite_operand_t mask;
} cairo_gl_composite_setup_t;

extern const cairo_surface_backend_t _cairo_gl_surface_backend;

cairo_private cairo_gl_context_t *
_cairo_gl_context_create_in_error (cairo_status_t status);

cairo_private cairo_status_t
_cairo_gl_context_init (cairo_gl_context_t *ctx);

cairo_private void
_cairo_gl_surface_init (cairo_gl_context_t *ctx,
			cairo_gl_surface_t *surface,
			cairo_content_t content,
			int width, int height);

cairo_status_t
_cairo_gl_surface_draw_image (cairo_gl_surface_t *dst,
			      cairo_image_surface_t *src,
			      int src_x, int src_y,
			      int width, int height,
			      int dst_x, int dst_y);

cairo_int_status_t
_cairo_gl_operand_init (cairo_gl_composite_operand_t *operand,
			const cairo_pattern_t *pattern,
			cairo_gl_surface_t *dst,
			int src_x, int src_y,
			int dst_x, int dst_y,
			int width, int height);

cairo_gl_context_t *
_cairo_gl_context_acquire (cairo_gl_context_t *ctx);

void
_cairo_gl_context_release (cairo_gl_context_t *ctx);

void
_cairo_gl_set_destination (cairo_gl_surface_t *surface);

int
_cairo_gl_set_operator (cairo_gl_surface_t *dst, cairo_operator_t op);

void
_cairo_gl_set_src_operand (cairo_gl_context_t *ctx,
			   cairo_gl_composite_setup_t *setup);

void
_cairo_gl_operand_destroy (cairo_gl_composite_operand_t *operand);

cairo_status_t
_cairo_gl_get_image_format_and_type (pixman_format_code_t pixman_format,
				     GLenum *internal_format, GLenum *format,
				     GLenum *type, cairo_bool_t *has_alpha);

void
_cairo_gl_surface_scaled_glyph_fini (cairo_scaled_glyph_t *scaled_glyph,
				     cairo_scaled_font_t  *scaled_font);

cairo_int_status_t
_cairo_gl_surface_show_glyphs (void			*abstract_dst,
			       cairo_operator_t		 op,
			       const cairo_pattern_t	*source,
			       cairo_glyph_t		*glyphs,
			       int			 num_glyphs,
			       cairo_scaled_font_t	*scaled_font,
			       cairo_clip_t		*clip,
			       int			*remaining_glyphs);

void
_cairo_gl_glyph_cache_fini (cairo_gl_glyph_cache_t *cache);

#endif /* CAIRO_GL_PRIVATE_H */
