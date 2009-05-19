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

#include "cairoint.h"

#include "cairo-gl-private.h"

#include <i915_drm.h> /* XXX dummy surface for glewInit() */
#include <sys/ioctl.h>

typedef struct _cairo_egl_context {
    cairo_gl_context_t base;

    EGLDisplay display;
    EGLContext context;
} cairo_egl_context_t;

typedef struct _cairo_egl_surface {
    cairo_gl_surface_t base;

    EGLSurface egl;
} cairo_egl_surface_t;

static void
_egl_make_current (void *abstract_ctx,
	           cairo_gl_surface_t *abstract_surface)
{
    cairo_egl_context_t *ctx = abstract_ctx;
    cairo_egl_surface_t *surface = (cairo_egl_surface_t *) abstract_surface;

    eglMakeCurrent (ctx->display, surface->egl, surface->egl, ctx->context);
}

static void
_egl_swap_buffers (void *abstract_ctx,
		   cairo_gl_surface_t *abstract_surface)
{
    cairo_egl_context_t *ctx = abstract_ctx;
    cairo_egl_surface_t *surface = (cairo_egl_surface_t *) abstract_surface;

    eglSwapBuffers (ctx->display, surface->egl);
}

static void
_egl_destroy (void *abstract_ctx)
{
}

static cairo_bool_t
_egl_init (EGLDisplay display, EGLContext context)
{
    const EGLint config_attribs[] = {
	EGL_CONFIG_CAVEAT, EGL_NONE,
	EGL_NONE
    };
    const EGLint surface_attribs[] = {
	EGL_RENDER_BUFFER, EGL_BACK_BUFFER,
	EGL_NONE
    };
    EGLConfig config;
    EGLSurface dummy;
    struct drm_i915_gem_create create;
    struct drm_gem_flink flink;
    int fd;
    GLenum err;

    if (! eglChooseConfig (display, config_attribs, &config, 1, NULL)) {
	fprintf (stderr, "Unable to choose config\n");
	return FALSE;
    }

    /* XXX */
    fd = eglGetDisplayFD (display);

    create.size = 4096;
    if (ioctl (fd, DRM_IOCTL_I915_GEM_CREATE, &create) != 0) {
	fprintf (stderr, "gem create failed: %m\n");
	return FALSE;
    }
    flink.handle = create.handle;
    if (ioctl (fd, DRM_IOCTL_GEM_FLINK, &flink) != 0) {
	fprintf (stderr, "gem flink failed: %m\n");
	return FALSE;
    }

    dummy = eglCreateSurfaceForName (display, config, flink.name,
				     1, 1, 4, surface_attribs);
    if (dummy == NULL) {
	fprintf (stderr, "Failed to create dummy surface\n");
	return FALSE;
    }

    eglMakeCurrent (display, dummy, dummy, context);
}

cairo_gl_context_t *
cairo_egl_context_create (EGLDisplay display, EGLContext context)
{
    cairo_egl_context_t *ctx;
    cairo_status_t status;

    if (! _egl_init (display, context)) {
	return _cairo_gl_context_create_in_error (CAIRO_STATUS_NO_MEMORY);
    }

    ctx = calloc (1, sizeof (cairo_egl_context_t));
    if (ctx == NULL)
	return _cairo_gl_context_create_in_error (CAIRO_STATUS_NO_MEMORY);

    ctx->display = display;
    ctx->context = context;

    ctx->base.make_current = _egl_make_current;
    ctx->base.swap_buffers = _egl_swap_buffers;
    ctx->base.destroy = _egl_destroy;

    status = _cairo_gl_context_init (&ctx->base);
    if (status) {
	free (ctx);
	return _cairo_gl_context_create_in_error (status);
    }

    return &ctx->base;
}

cairo_surface_t *
cairo_gl_surface_create_for_eagle (cairo_gl_context_t   *ctx,
				   EGLSurface            egl,
				   int                   width,
				   int                   height)
{
    cairo_egl_surface_t *surface;

    if (ctx->status)
	return _cairo_surface_create_in_error (ctx->status);

    surface = calloc (1, sizeof (cairo_egl_surface_t));
    if (unlikely (surface == NULL))
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    _cairo_gl_surface_init (ctx, &surface->base,
			    CAIRO_CONTENT_COLOR_ALPHA, width, height);
    surface->egl = egl;

    return &surface->base.base;
}
