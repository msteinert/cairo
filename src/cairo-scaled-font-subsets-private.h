/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2006 Red Hat, Inc
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

#ifndef CAIRO_SCALED_FONT_SUBSETS_PRIVATE_H
#define CAIRO_SCALED_FONT_SUBSETS_PRIVATE_H

#include "cairoint.h"

typedef struct _cairo_scaled_font_subsets cairo_scaled_font_subsets_t;

typedef struct _cairo_scaled_font_subset {
    cairo_scaled_font_t *scaled_font;
    unsigned int font_id;
    unsigned int subset_id;

    /* Index of glyphs array is subset_glyph_index.
     * Value of glyphs array is scaled_font_glyph_index.
     */
    unsigned long *glyphs;
    int num_glyphs;
} cairo_scaled_font_subset_t;

cairo_private cairo_scaled_font_subsets_t *
_cairo_scaled_font_subsets_create (int max_glyphs_per_subset);

cairo_private void
_cairo_scaled_font_subsets_destroy (cairo_scaled_font_subsets_t *font_subsets);

cairo_private cairo_status_t
_cairo_scaled_font_subsets_map_glyph (cairo_scaled_font_subsets_t	*font_subsets,
				      cairo_scaled_font_t		*scaled_font,
				      unsigned long			 scaled_font_glyph_index,
				      unsigned int			*font_id,
				      unsigned int			*subset_id,
				      unsigned int			*subset_glyph_index);

typedef cairo_status_t
(*cairo_scaled_font_subset_callback_func_t) (cairo_scaled_font_subset_t	*font_subset,
					     void			*closure);

cairo_private cairo_status_t
_cairo_scaled_font_subsets_foreach (cairo_scaled_font_subsets_t			*font_subsets,
				    cairo_scaled_font_subset_callback_func_t	 font_subset_callback,
				    void					*closure);

#endif /* CAIRO_SCALED_FONT_SUBSETS_PRIVATE_H */
