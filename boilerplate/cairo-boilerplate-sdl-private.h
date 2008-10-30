/* Cairo - a vector graphics library with display and print output
 *
 * Copyright © 2008 Chris Wilson
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

#ifndef CAIRO_BOILERPLATE_SDL_PRIVATE_H
#define CAIRO_BOILERPLATE_SDL_PRIVATE_H

#include <cairo.h>

CAIRO_BEGIN_DECLS

extern cairo_surface_t *
_cairo_boilerplate_sdl_create_surface (const char			 *name,
				       cairo_content_t		  content,
				       int				  width,
				       int				  height,
				       int				  max_width,
				       int				  max_height,
				       cairo_boilerplate_mode_t	  mode,
				       int                           id,
				       void			**closure);

extern void
_cairo_boilerplate_sdl_cleanup (void* closure);

CAIRO_END_DECLS

#endif /* CAIRO_BOILERPLATE_SDL_PRIVATE_H */
