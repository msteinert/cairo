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

struct _cairo_gl_context {
    cairo_reference_count_t ref_count;
    cairo_status_t status;

    cairo_mutex_t mutex; /* needed? */
    GLuint dummy_tex;
    GLint max_framebuffer_size;
    GLint max_texture_size;

    cairo_gl_surface_t *current_target;

    void (*make_current)(void *ctx, cairo_gl_surface_t *surface);
    void (*swap_buffers)(void *ctx, cairo_gl_surface_t *surface);
    void (*destroy) (void *ctx);
};

cairo_private cairo_gl_context_t *
_cairo_gl_context_create_in_error (cairo_status_t status);

cairo_private cairo_status_t
_cairo_gl_context_init (cairo_gl_context_t *ctx);

cairo_private void
_cairo_gl_surface_init (cairo_gl_context_t *ctx,
			cairo_gl_surface_t *surface,
			cairo_content_t content,
			int width, int height);

#endif /* CAIRO_GL_PRIVATE_H */