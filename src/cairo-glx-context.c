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

typedef struct _cairo_glx_context {
    cairo_gl_context_t base;

    Display *display;
    GLXContext context;
} cairo_glx_context_t;

typedef struct _cairo_glx_surface {
    cairo_gl_surface_t base;

    Window win;
} cairo_glx_surface_t;

static void
_glx_make_current (void *abstract_ctx,
	           cairo_gl_surface_t *abstract_surface)
{
    cairo_glx_context_t *ctx = abstract_ctx;
    cairo_glx_surface_t *surface = (cairo_glx_surface_t *) abstract_surface;

    /* Set the window as the target of our context. */
    glXMakeCurrent (ctx->display, surface->win, ctx->context);
}

static void
_glx_swap_buffers (void *abstract_ctx,
		   cairo_gl_surface_t *abstract_surface)
{
    cairo_glx_context_t *ctx = abstract_ctx;
    cairo_glx_surface_t *surface = (cairo_glx_surface_t *) abstract_surface;

    glXSwapBuffers (ctx->display, surface->win);
}

static void
_glx_destroy (void *abstract_ctx)
{
}

cairo_gl_context_t *
cairo_glx_context_create (Display *dpy, GLXContext gl_ctx)
{
    cairo_glx_context_t *ctx;
    cairo_status_t status;

    /* Make our GL context active.  While we'll be setting the destination
     * drawable with each rendering operation, in order to set the context
     * we have to choose a drawable.  The root window happens to be convenient
     * for this.
     */
    if (! glXMakeCurrent (dpy, RootWindow (dpy, DefaultScreen (dpy)), gl_ctx))
	return _cairo_gl_context_create_in_error (CAIRO_STATUS_NO_MEMORY);

    ctx = calloc (1, sizeof (cairo_glx_context_t));
    if (ctx == NULL)
	return _cairo_gl_context_create_in_error (CAIRO_STATUS_NO_MEMORY);

    ctx->display = dpy;
    ctx->context = gl_ctx;

    ctx->base.make_current = _glx_make_current;
    ctx->base.swap_buffers = _glx_swap_buffers;
    ctx->base.destroy = _glx_destroy;

    status = _cairo_gl_context_init (&ctx->base);
    if (status) {
	free (ctx);
	return _cairo_gl_context_create_in_error (status);
    }

    return &ctx->base;
}

cairo_surface_t *
cairo_gl_surface_create_for_window (cairo_gl_context_t   *ctx,
				    Window                win,
				    int                   width,
				    int                   height)
{
    cairo_glx_surface_t *surface;

    if (ctx->status)
	return _cairo_surface_create_in_error (ctx->status);

    surface = calloc (1, sizeof (cairo_glx_surface_t));
    if (unlikely (surface == NULL))
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    _cairo_gl_surface_init (ctx, &surface->base,
			    CAIRO_CONTENT_COLOR_ALPHA, width, height);
    surface->win = win;

    return &surface->base.base;
}
