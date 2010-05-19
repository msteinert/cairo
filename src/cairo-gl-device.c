/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2009 Eric Anholt
 * Copyright © 2009 Chris Wilson
 * Copyright © 2005,2010 Red Hat, Inc
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
 * Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA 02110-1335, USA
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
 *	Benjamin Otte <otte@gnome.org>
 *	Carl Worth <cworth@cworth.org>
 *	Chris Wilson <chris@chris-wilson.co.uk>
 *	Eric Anholt <eric@anholt.net>
 */

#include "cairoint.h"

#include "cairo-error-private.h"
#include "cairo-gl-private.h"

static void
_gl_lock (void *device)
{
    cairo_gl_context_t *ctx = (cairo_gl_context_t *) device;

    ctx->acquire (ctx);
}

static void
_gl_unlock (void *device)
{
    cairo_gl_context_t *ctx = (cairo_gl_context_t *) device;

    ctx->release (ctx);
    ctx->current_target = NULL;
}

static void
_gl_finish (void *device)
{
    cairo_gl_context_t *ctx = device;

    _gl_lock (device);

    _cairo_gl_context_fini_shaders (ctx);

    _gl_unlock (device);
}

static void
_gl_destroy (void *device)
{
    cairo_gl_context_t *ctx = device;
    cairo_scaled_font_t *scaled_font, *next_scaled_font;
    int n;

    ctx->acquire (ctx);

    cairo_list_foreach_entry_safe (scaled_font,
				   next_scaled_font,
				   cairo_scaled_font_t,
				   &ctx->fonts,
				   link)
    {
	_cairo_scaled_font_revoke_ownership (scaled_font);
    }

    for (n = 0; n < ARRAY_LENGTH (ctx->glyph_cache); n++)
	_cairo_gl_glyph_cache_fini (ctx, &ctx->glyph_cache[n]);

    ctx->destroy (ctx);

    free (ctx);
}

static const cairo_device_backend_t _cairo_gl_device_backend = {
    CAIRO_DEVICE_TYPE_GL,

    _gl_lock,
    _gl_unlock,

    NULL, /* flush */
    _gl_finish,
    _gl_destroy,
};

cairo_status_t
_cairo_gl_context_init (cairo_gl_context_t *ctx)
{
    cairo_status_t status;
    int n;

    _cairo_device_init (&ctx->base, &_cairo_gl_device_backend);

    memset (ctx->glyph_cache, 0, sizeof (ctx->glyph_cache));
    cairo_list_init (&ctx->fonts);

    if (glewInit () != GLEW_OK)
	return _cairo_error (CAIRO_STATUS_INVALID_FORMAT); /* XXX */

    if (! GLEW_EXT_framebuffer_object ||
	! GLEW_ARB_texture_env_combine ||
	! GLEW_EXT_bgra)
    {
	fprintf (stderr,
		 "Required GL extensions not available:\n");
	if (! GLEW_EXT_framebuffer_object)
	    fprintf (stderr, "    GL_EXT_framebuffer_object\n");
	if (! GLEW_ARB_texture_env_combine)
	    fprintf (stderr, "    GL_ARB_texture_env_combine\n");
        if (! GLEW_ARB_vertex_buffer_object)
	    fprintf (stderr, "    GL_ARB_vertex_buffer_object\n");

	/* EXT_bgra is used in two places:
	 * - draw_image to upload common pixman formats without hand-swizzling.
	 * - get_image to download common pixman formats without hand-swizzling.
	 */
	if (! GLEW_EXT_bgra)
	    fprintf (stderr, "    GL_EXT_bgra\n");

	return _cairo_error (CAIRO_STATUS_INVALID_FORMAT); /* XXX */
    }

    if (! GLEW_ARB_texture_non_power_of_two &&
	! GLEW_ARB_texture_rectangle ) {
	fprintf (stderr,
		 "Required GL extensions not available:\n");
	fprintf (stderr, "    GL_ARB_texture_non_power_of_two, GL_ARB_texture_rectangle\n");
    }

    if (!GLEW_ARB_texture_non_power_of_two)
	ctx->tex_target = GL_TEXTURE_RECTANGLE_EXT;
    else
	ctx->tex_target = GL_TEXTURE_2D;

    status = _cairo_gl_context_init_shaders (ctx);
    if (unlikely (status))
	return status;

    /* Set up the dummy texture for tex_env_combine with constant color. */
    glGenTextures (1, &ctx->dummy_tex);
    glBindTexture (ctx->tex_target, ctx->dummy_tex);
    glTexImage2D (ctx->tex_target, 0, GL_RGBA, 1, 1, 0,
		  GL_RGBA, GL_UNSIGNED_BYTE, NULL);

    /* PBO for any sort of texture upload */
    glGenBuffersARB (1, &ctx->texture_load_pbo);
    glGenBuffersARB (1, &ctx->vbo);

    ctx->max_framebuffer_size = 0;
    glGetIntegerv (GL_MAX_RENDERBUFFER_SIZE, &ctx->max_framebuffer_size);
    ctx->max_texture_size = 0;
    glGetIntegerv (GL_MAX_TEXTURE_SIZE, &ctx->max_texture_size);

    for (n = 0; n < ARRAY_LENGTH (ctx->glyph_cache); n++)
	_cairo_gl_glyph_cache_init (&ctx->glyph_cache[n]);

    return CAIRO_STATUS_SUCCESS;
}

void
_cairo_gl_context_set_destination (cairo_gl_context_t *ctx,
                           cairo_gl_surface_t *surface)
{
    if (ctx->current_target != surface) {
	ctx->current_target = surface;

	if (_cairo_gl_surface_is_texture (surface)) {
	    glBindFramebufferEXT (GL_FRAMEBUFFER_EXT, surface->fb);
	    glDrawBuffer (GL_COLOR_ATTACHMENT0_EXT);
	    glReadBuffer (GL_COLOR_ATTACHMENT0_EXT);
	} else {
	    ctx->make_current (ctx, surface);
	    glBindFramebufferEXT (GL_FRAMEBUFFER_EXT, 0);
	    glDrawBuffer (GL_BACK_LEFT);
	    glReadBuffer (GL_BACK_LEFT);
	}
    }

    glViewport (0, 0, surface->width, surface->height);

    glMatrixMode (GL_PROJECTION);
    glLoadIdentity ();
    if (_cairo_gl_surface_is_texture (surface))
	glOrtho (0, surface->width, 0, surface->height, -1.0, 1.0);
    else
	glOrtho (0, surface->width, surface->height, 0, -1.0, 1.0);

    glMatrixMode (GL_MODELVIEW);
    glLoadIdentity ();
}

