/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2002 University of Southern California
 * Copyright © 2005 Red Hat, Inc.
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
 * The Initial Developer of the Original Code is University of Southern
 * California.
 *
 * Contributor(s):
 *	Carl D. Worth <cworth@cworth.org>
 */

#ifndef CAIRO_IMAGE_SURFACE_PRIVATE_H
#define CAIRO_IMAGE_SURFACE_PRIVATE_H

#include "cairo-surface-private.h"

#include <pixman.h>

CAIRO_BEGIN_DECLS

struct _cairo_image_surface {
    cairo_surface_t base;

    pixman_format_code_t pixman_format;
    cairo_format_t format;
    unsigned char *data;

    int width;
    int height;
    int stride;
    int depth;

    pixman_image_t *pixman_image;

    unsigned owns_data : 1;
    unsigned transparency : 2;
    unsigned color : 2;
};

extern const cairo_private cairo_surface_backend_t _cairo_image_surface_backend;

cairo_private void
_cairo_image_surface_init (cairo_image_surface_t *surface,
			   pixman_image_t	*pixman_image,
			   pixman_format_code_t	 pixman_format);

cairo_private_no_warn cairo_bool_t
_cairo_image_surface_get_extents (void			  *abstract_surface,
				  cairo_rectangle_int_t   *rectangle);

cairo_private void
_cairo_image_surface_get_font_options (void                  *abstract_surface,
				       cairo_font_options_t  *options);

cairo_private cairo_status_t
_cairo_image_surface_finish (void *abstract_surface);

CAIRO_END_DECLS

#endif /* CAIRO_IMAGE_SURFACE_PRIVATE_H */
