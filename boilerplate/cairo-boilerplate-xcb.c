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

#include <cairo-xcb-xrender.h>

#include <xcb/xcb_renderutil.h>

static const cairo_user_data_key_t xcb_closure_key;

typedef struct _xcb_target_closure {
    xcb_connection_t *c;
    xcb_pixmap_t pixmap;
} xcb_target_closure_t;

static void
_cairo_boilerplate_xcb_cleanup (void *closure)
{
    xcb_target_closure_t *xtc = closure;

    xcb_free_pixmap (xtc->c, xtc->pixmap);
    xcb_disconnect (xtc->c);
    free (xtc);
}

static void
_cairo_boilerplate_xcb_synchronize (void *closure)
{
    xcb_target_closure_t *xtc = closure;
    free (xcb_get_image_reply (xtc->c,
		xcb_get_image (xtc->c, XCB_IMAGE_FORMAT_Z_PIXMAP,
		    xtc->pixmap, 0, 0, 1, 1, /* AllPlanes */ ~0UL),
		0));
}

static cairo_surface_t *
_cairo_boilerplate_xcb_create_surface (const char		 *name,
				       cairo_content_t		  content,
				       double			  width,
				       double			  height,
				       double			  max_width,
				       double			  max_height,
				       cairo_boilerplate_mode_t	  mode,
				       int                        id,
				       void			**closure)
{
    xcb_screen_t *root;
    xcb_target_closure_t *xtc;
    xcb_connection_t *c;
    xcb_render_pictforminfo_t *render_format;
    xcb_pict_standard_t format;
    xcb_void_cookie_t cookie;
    cairo_surface_t *surface;
    cairo_status_t status;

    *closure = xtc = xmalloc (sizeof (xcb_target_closure_t));

    if (width == 0)
	width = 1;
    if (height == 0)
	height = 1;

    xtc->c = c = xcb_connect(NULL,NULL);
    if (xcb_connection_has_error(c)) {
	fprintf (stderr, "Failed to connect to X server through XCB\n");
	return NULL;
    }

    root = xcb_setup_roots_iterator(xcb_get_setup(c)).data;

    xtc->pixmap = xcb_generate_id (c);
    switch (content) {
    case CAIRO_CONTENT_COLOR:
	cookie = xcb_create_pixmap_checked (c, 24,
					    xtc->pixmap, root->root,
					    width, height);
	format = XCB_PICT_STANDARD_RGB_24;
	break;

    case CAIRO_CONTENT_COLOR_ALPHA:
	cookie = xcb_create_pixmap_checked (c, 32,
					    xtc->pixmap, root->root,
					    width, height);
	format = XCB_PICT_STANDARD_ARGB_32;
	break;

    case CAIRO_CONTENT_ALPHA:  /* would be XCB_PICT_STANDARD_A_8 */
    default:
	fprintf (stderr, "Invalid content for XCB test: %d\n", content);
	return NULL;
    }

    /* slow, but sure */
    if (xcb_request_check (c, cookie) != NULL) {
	xcb_disconnect (c);
	free (xtc);
	return NULL;
    }

    render_format = xcb_render_util_find_standard_format (xcb_render_util_query_formats (c), format);
    if (render_format->id == 0)
	return NULL;

    surface = cairo_xcb_surface_create_with_xrender_format (c, xtc->pixmap, root,
							    render_format,
							    width, height);

    status = cairo_surface_set_user_data (surface, &xcb_closure_key, xtc, NULL);
    if (status == CAIRO_STATUS_SUCCESS)
	return surface;

    cairo_surface_destroy (surface);
    surface = cairo_boilerplate_surface_create_in_error (status);

    _cairo_boilerplate_xcb_cleanup (xtc);

    return surface;
}

static cairo_status_t
_cairo_boilerplate_xcb_finish_surface (cairo_surface_t		*surface)
{
    xcb_target_closure_t *xtc = cairo_surface_get_user_data (surface,
							     &xcb_closure_key);
    xcb_generic_event_t *ev;

    cairo_surface_flush (surface);

    if (cairo_surface_status (surface))
	return cairo_surface_status (surface);

    while ((ev = xcb_poll_for_event (xtc->c)) != NULL) {
	if (ev->response_type == 0 /* trust me! */) {
	    xcb_generic_error_t *error = (xcb_generic_error_t *) ev;

	    fprintf (stderr, "Detected error during xcb run: %d major=%d, minor=%d\n",
		     error->error_code, error->major_code, error->minor_code);

	    return CAIRO_STATUS_WRITE_ERROR;
	}
    }

    if (xcb_connection_has_error (xtc->c))
	return CAIRO_STATUS_WRITE_ERROR;

    return CAIRO_STATUS_SUCCESS;
}

static const cairo_boilerplate_target_t targets[] = {
    /* Acceleration architectures may make the results differ by a
     * bit, so we set the error tolerance to 1. */
    {
	"xcb", "xlib", NULL, NULL,
	CAIRO_SURFACE_TYPE_XCB, CAIRO_CONTENT_COLOR_ALPHA, 1,
	"cairo_xcb_surface_create_with_xrender_format",
	_cairo_boilerplate_xcb_create_surface,
	NULL,
	_cairo_boilerplate_xcb_finish_surface,
	_cairo_boilerplate_get_image_surface,
	cairo_surface_write_to_png,
	_cairo_boilerplate_xcb_cleanup,
	_cairo_boilerplate_xcb_synchronize
    },
    {
	"xcb", "xlib", NULL, NULL,
	CAIRO_SURFACE_TYPE_XCB, CAIRO_CONTENT_COLOR, 1,
	"cairo_xcb_surface_create_with_xrender_format",
	_cairo_boilerplate_xcb_create_surface,
	NULL,
	_cairo_boilerplate_xcb_finish_surface,
	_cairo_boilerplate_get_image_surface,
	cairo_surface_write_to_png,
	_cairo_boilerplate_xcb_cleanup,
	_cairo_boilerplate_xcb_synchronize
    },
};
CAIRO_BOILERPLATE (xcb, targets)
