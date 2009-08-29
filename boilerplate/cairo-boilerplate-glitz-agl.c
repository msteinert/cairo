/* -*- Mode: c; c-basic-offset: 4; indent-tabs-mode: t; tab-width: 8; -*- */
/*
 * Copyright © 2004,2006 Red Hat, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of
 * Red Hat, Inc. not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission. Red Hat, Inc. makes no representations about the
 * suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * RED HAT, INC. DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL RED HAT, INC. BE LIABLE FOR ANY SPECIAL,
 * INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
 * IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author: Carl D. Worth <cworth@cworth.org>
 */

#include "cairo-boilerplate-private.h"

#if CAIRO_CAN_TEST_GLITZ_AGL_SURFACE
#include <cairo-glitz.h>
#include <glitz-agl.h>

static const cairo_user_data_key_t glitz_closure_key;

typedef struct _glitz_agl_target_closure {
    glitz_target_closure_base_t base;
} glitz_agl_target_closure_t;

static glitz_surface_t *
_cairo_boilerplate_glitz_agl_create_surface_internal (glitz_format_name_t		 formatname,
						      int				 width,
						      int				 height,
						      glitz_agl_target_closure_t	*closure)
{
    glitz_drawable_format_t *dformat;
    glitz_drawable_format_t templ;
    glitz_drawable_t *gdraw;
    glitz_format_t *format;
    glitz_surface_t *sr = NULL;
    unsigned long mask;

    memset(&templ, 0, sizeof(templ));
    templ.color.red_size = 8;
    templ.color.green_size = 8;
    templ.color.blue_size = 8;
    templ.color.alpha_size = 8;
    templ.color.fourcc = GLITZ_FOURCC_RGB;
    templ.samples = 1;

    mask = GLITZ_FORMAT_SAMPLES_MASK | GLITZ_FORMAT_FOURCC_MASK |
	GLITZ_FORMAT_RED_SIZE_MASK | GLITZ_FORMAT_GREEN_SIZE_MASK |
	GLITZ_FORMAT_BLUE_SIZE_MASK;
    if (formatname == GLITZ_STANDARD_ARGB32)
	mask |= GLITZ_FORMAT_ALPHA_SIZE_MASK;

    dformat = glitz_agl_find_pbuffer_format (mask, &templ, 0);
    if (!dformat) {
	fprintf (stderr, "Glitz failed to find pbuffer format for template.");
	goto FAIL;
    }

    gdraw = glitz_agl_create_pbuffer_drawable (dformat, width, height);
    if (!gdraw) {
	fprintf (stderr, "Glitz failed to create pbuffer drawable.");
	goto FAIL;
    }

    format = glitz_find_standard_format (gdraw, formatname);
    if (!format) {
	fprintf (stderr, "Glitz failed to find standard format for drawable.");
	goto DESTROY_DRAWABLE;
    }

    sr = glitz_surface_create (gdraw, format, width, height, 0, NULL);
    if (!sr) {
	fprintf (stderr, "Glitz failed to create a surface.");
	goto DESTROY_DRAWABLE;
    }

    glitz_surface_attach (sr, gdraw, GLITZ_DRAWABLE_BUFFER_FRONT_COLOR);

 DESTROY_DRAWABLE:
    glitz_drawable_destroy (gdraw);
    return sr;

 FAIL:
    return NULL;
}

static cairo_surface_t *
_cairo_boilerplate_glitz_agl_create_surface (const char			 *name,
					     cairo_content_t 		  content,
					     double			  width,
					     double			  height,
					     double			  max_width,
					     double			  max_height,
					     cairo_boilerplate_mode_t	  mode,
					     int                          id,
					     void			**closure)
{
    glitz_surface_t *glitz_surface;
    cairo_surface_t *surface = NULL;
    glitz_agl_target_closure_t *aglc;
    int status;

    glitz_agl_init ();

    *closure = aglc = xmalloc (sizeof (glitz_agl_target_closure_t));

    switch (content) {
    case CAIRO_CONTENT_COLOR:
	glitz_surface = _cairo_boilerplate_glitz_agl_create_surface_internal (GLITZ_STANDARD_RGB24, width, height, NULL);
	break;
    case CAIRO_CONTENT_COLOR_ALPHA:
	glitz_surface = _cairo_boilerplate_glitz_agl_create_surface_internal (GLITZ_STANDARD_ARGB32, width, height, NULL);
	break;
    case CAIRO_CONTENT_ALPHA:
    default:
	fprintf (stderr, "Invalid content for glitz-agl test: %d\n", content);
	goto FAIL;
    }

    if (!glitz_surface)
	goto FAIL;

    surface = cairo_glitz_surface_create (glitz_surface);
    glitz_surface_destroy (glitz_surface);

    if (cairo_surface_status (surface))
	goto FAIL;

    aglc->base.width = width;
    aglc->base.height = height;
    aglc->base.content = content;
    status = cairo_surface_set_user_data (surface,
					  &glitz_closure_key, aglc, NULL);
    if (status == CAIRO_STATUS_SUCCESS)
	return surface;

    cairo_surface_destroy (surface);
    surface = cairo_boilerplate_surface_create_in_error (status);

 FAIL:
    glitz_agl_fini ();
    return surface;
}

static void
_cairo_boilerplate_glitz_agl_cleanup (void *closure)
{
    free (closure);
    glitz_agl_fini ();
}

#endif

static const cairo_boilerplate_target_t targets[] = {
#if CAIRO_CAN_TEST_GLITZ_AGL_SURFACE
    {
	"glitz-agl", "glitz", NULL, NULL,
	CAIRO_SURFACE_TYPE_GLITZ, CAIRO_CONTENT_COLOR_ALPHA, 0,
	"cairo_glitz_surface_create",
	_cairo_boilerplate_glitz_agl_create_surface,
	NULL, NULL,
	_cairo_boilerplate_get_image_surface,
	cairo_surface_write_to_png,
	_cairo_boilerplate_glitz_agl_cleanup
    },
    {
	"glitz-agl", "glitz", NULL, NULL,
	CAIRO_SURFACE_TYPE_GLITZ, CAIRO_CONTENT_COLOR, 0,
	"cairo_glitz_surface_create",
	_cairo_boilerplate_glitz_agl_create_surface,
	NULL, NULL,
	_cairo_boilerplate_get_image_surface,
	cairo_surface_write_to_png,
	_cairo_boilerplate_glitz_agl_cleanup
    },
#endif
};
CAIRO_BOILERPLATE (glitz_agl, targets)
