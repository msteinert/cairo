/* -*- Mode: c; c-basic-offset: 4; indent-tabs-mode: t; tab-width: 8; -*- */
/*
 * Copyright © Chris Wilson
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of
 * Chris Wilson not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission. Chris Wilson makes no representations about the
 * suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * CHRIS WILSON DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL CHRIS WILSON BE LIABLE FOR ANY SPECIAL,
 * INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
 * IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author: Chris Wilson <chris@chris-wilson.co.uk>
 */

#include "cairo-boilerplate.h"
#include "cairo-boilerplate-script-private.h"

#include "cairo-script.h"

cairo_user_data_key_t script_closure_key;

typedef struct _script_target_closure {
    char		*filename;
    int			 width;
    int			 height;
} script_target_closure_t;

cairo_surface_t *
_cairo_boilerplate_script_create_surface (const char		 *name,
					  cairo_content_t	  content,
					  int			  width,
					  int			  height,
					  int			  max_width,
					  int			  max_height,
					  cairo_boilerplate_mode_t	  mode,
					  int                        id,
					  void			**closure)
{
    script_target_closure_t *ptc;
    cairo_surface_t *surface;
    cairo_status_t status;

    *closure = ptc = xmalloc (sizeof (script_target_closure_t));

    ptc->width = width;
    ptc->height = height;

    xasprintf (&ptc->filename, "%s.out.cs", name);
    xunlink (ptc->filename);

    surface = cairo_script_surface_create (ptc->filename, width, height);

    status = cairo_surface_set_user_data (surface,
					  &script_closure_key, ptc, NULL);
    if (status == CAIRO_STATUS_SUCCESS)
	return surface;

    cairo_surface_destroy (surface);
    surface = cairo_boilerplate_surface_create_in_error (status);

    free (ptc->filename);
    free (ptc);
    return surface;
}

cairo_status_t
_cairo_boilerplate_script_finish_surface (cairo_surface_t		*surface)
{
    cairo_surface_finish (surface);
    return cairo_surface_status (surface);
}

cairo_status_t
_cairo_boilerplate_script_surface_write_to_png (cairo_surface_t *surface,
						const char *filename)
{
    return CAIRO_STATUS_WRITE_ERROR;
}

static cairo_surface_t *
_cairo_boilerplate_script_convert_to_image (cairo_surface_t *surface,
					    int page)
{
    script_target_closure_t *ptc = cairo_surface_get_user_data (surface,
								&script_closure_key);
    return cairo_boilerplate_convert_to_image (ptc->filename, page);
}

cairo_surface_t *
_cairo_boilerplate_script_get_image_surface (cairo_surface_t *surface,
					     int page,
					     int width,
					     int height)
{
    cairo_surface_t *image;

    image = _cairo_boilerplate_script_convert_to_image (surface, page);
    cairo_surface_set_device_offset (image,
				     cairo_image_surface_get_width (image) - width,
				     cairo_image_surface_get_height (image) - height);
    surface = _cairo_boilerplate_get_image_surface (image, 0, width, height);
    cairo_surface_destroy (image);

    return surface;
}

void
_cairo_boilerplate_script_cleanup (void *closure)
{
    script_target_closure_t *ptc = closure;
    free (ptc->filename);
    free (ptc);
}
