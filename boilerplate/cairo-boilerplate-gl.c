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

#include "cairo-boilerplate-private.h"

#include <cairo-gl.h>

typedef struct _gl_target_closure {
    Display *dpy;
    int screen;

    GLXContext gl_ctx;
    cairo_gl_context_t *ctx;
    cairo_surface_t *surface;
} gl_target_closure_t;

static void
_cairo_boilerplate_gl_cleanup (void *closure)
{
    gl_target_closure_t *gltc = closure;

    cairo_gl_context_destroy (gltc->ctx);
    glXDestroyContext (gltc->dpy, gltc->gl_ctx);
    XCloseDisplay (gltc->dpy);
    free (gltc);
}

static cairo_surface_t *
_cairo_boilerplate_gl_create_surface (const char		 *name,
				      cairo_content_t		  content,
				      double			  width,
				      double			  height,
				      double			  max_width,
				      double			  max_height,
				      cairo_boilerplate_mode_t	  mode,
				      int			  id,
				      void			**closure)
{
    int rgba_attribs[] = { GLX_RGBA,
			   GLX_RED_SIZE, 1,
			   GLX_GREEN_SIZE, 1,
			   GLX_BLUE_SIZE, 1,
			   GLX_ALPHA_SIZE, 1,
			   GLX_DOUBLEBUFFER,
			   None };
    int rgb_attribs[] = { GLX_RGBA,
			  GLX_RED_SIZE, 1,
			  GLX_GREEN_SIZE, 1,
			  GLX_BLUE_SIZE, 1,
			  GLX_DOUBLEBUFFER,
			  None };
    XVisualInfo *visinfo;
    GLXContext gl_ctx;
    gl_target_closure_t *gltc;
    cairo_surface_t *surface;
    Display *dpy;

    gltc = malloc (sizeof (gl_target_closure_t));
    *closure = gltc;

    if (width == 0)
	width = 1;
    if (height == 0)
	height = 1;

    dpy = XOpenDisplay (NULL);
    gltc->dpy = dpy;
    if (!gltc->dpy) {
	fprintf (stderr, "Failed to open display: %s\n", XDisplayName(0));
	free (gltc);
	return NULL;
    }

    if (mode == CAIRO_BOILERPLATE_MODE_TEST)
	XSynchronize (gltc->dpy, 1);

    if (content == CAIRO_CONTENT_COLOR)
	visinfo = glXChooseVisual (dpy, DefaultScreen (dpy), rgb_attribs);
    else
	visinfo = glXChooseVisual (dpy, DefaultScreen (dpy), rgba_attribs);

    if (visinfo == NULL) {
	fprintf (stderr, "Failed to create RGB, double-buffered visual\n");
	XCloseDisplay (dpy);
	free (gltc);
	return NULL;
    }

    gl_ctx = glXCreateContext (dpy, visinfo, NULL, True);
    XFree (visinfo);

    gltc->gl_ctx = gl_ctx;
    gltc->ctx = cairo_glx_context_create (dpy, gl_ctx);

    gltc->surface = surface = cairo_gl_surface_create (gltc->ctx, content,
					               ceil (width),
						       ceil (height));
    if (cairo_surface_status (surface))
	_cairo_boilerplate_gl_cleanup (gltc);

    return surface;
}

static void
_cairo_boilerplate_gl_synchronize (void *closure)
{
    gl_target_closure_t *gltc = closure;

    cairo_gl_surface_glfinish (gltc->surface);
}

static const cairo_boilerplate_target_t targets[] = {
    {
	"gl", "gl", NULL, NULL,
	CAIRO_SURFACE_TYPE_GL, CAIRO_CONTENT_COLOR_ALPHA, 1,
	"cairo_gl_surface_create",
	_cairo_boilerplate_gl_create_surface,
	NULL, NULL,
	_cairo_boilerplate_get_image_surface,
	cairo_surface_write_to_png,
	_cairo_boilerplate_gl_cleanup,
	_cairo_boilerplate_gl_synchronize
    },
    {
	"gl", "gl", NULL, NULL,
	CAIRO_SURFACE_TYPE_GL, CAIRO_CONTENT_COLOR, 1,
	"cairo_gl_surface_create",
	_cairo_boilerplate_gl_create_surface,
	NULL, NULL,
	_cairo_boilerplate_get_image_surface,
	cairo_surface_write_to_png,
	_cairo_boilerplate_gl_cleanup,
	_cairo_boilerplate_gl_synchronize
    },
};
CAIRO_BOILERPLATE (gl, targets)
