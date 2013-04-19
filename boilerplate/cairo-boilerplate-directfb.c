/* Cairo - a vector graphics library with display and print output
 *
 * Copyright © 2013 EchoStar Corporation
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
 */

#include "cairo-boilerplate-private.h"
#include "cairo-directfb.h"

static cairo_surface_t *
_cairo_boilerplate_directfb_create_surface (const char		    *name,
					    cairo_content_t	     content,
					    double		     width,
					    double		     height,
					    double		     max_width,
					    double		     max_height,
					    cairo_boilerplate_mode_t mode,
					    void		   **closure)
{
    IDirectFB *dfb;
    DFBResult result;
    cairo_surface_t *surface;
    DFBSurfaceDescription  dsc;
    IDirectFBSurface *dfb_surface;
    result = DirectFBInit (NULL, NULL);
    if (DFB_OK != result) {
	return NULL;
    }
    result = DirectFBCreate (&dfb);
    if (DFB_OK != result) {
	return NULL;
    }
    dsc.flags = DSDESC_WIDTH | DSDESC_HEIGHT |
	DSDESC_CAPS | DSDESC_PIXELFORMAT;
    dsc.width = width;
    dsc.height = height;
    dsc.caps = DSCAPS_PREMULTIPLIED;
    dsc.pixelformat = DSPF_UNKNOWN;
    switch (content) {
    case CAIRO_CONTENT_COLOR:
	dsc.pixelformat = DSPF_RGB24;
	break;
    case CAIRO_CONTENT_ALPHA:
	dsc.pixelformat = DSPF_A8;
	break;
    case CAIRO_CONTENT_COLOR_ALPHA:
	dsc.pixelformat = DSPF_ARGB;
	break;
    }
    result = dfb->CreateSurface (dfb, &dsc, &dfb_surface);
    if (DFB_OK != result) {
	return NULL;
    }
    surface = cairo_directfb_surface_create (dfb, dfb_surface);
    (void) dfb_surface->Release (dfb_surface);
    (void) dfb->Release (dfb);
    return surface;
}

static const cairo_boilerplate_target_t targets[] = {
    {
	"directfb", "directfb", NULL, NULL,
	CAIRO_SURFACE_TYPE_DIRECTFB, CAIRO_CONTENT_COLOR_ALPHA, 1,
	"cairo_directfb_surface_create",
	_cairo_boilerplate_directfb_create_surface,
	cairo_surface_create_similar,
	NULL, NULL,
	_cairo_boilerplate_get_image_surface,
	cairo_surface_write_to_png,
	NULL, NULL, NULL, FALSE, FALSE, FALSE
    },
};
CAIRO_BOILERPLATE (directfb, targets);
