/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2003 University of Southern California
 * Copyright © 2005 Red Hat, Inc
 * Copyright © 2006 Keith Packard
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
 *	Kristian Høgsberg <krh@redhat.com>
 *	Keith Packard <keithp@keithp.com>
 */

/*
 * Type1 and Type3 PS fonts can hold only 256 glyphs.
 *
 * XXX Work around this by placing each set of 256 glyphs in a separate
 * font. No separate data structure is kept for this; the font name is
 * generated from all but the low 8 bits of the output glyph id.
 */

#ifndef CAIRO_PS_FONT_PRIVATE_H
#define CAIRO_PS_FONT_PRIVATE_H

#include "cairoint.h"

typedef struct cairo_ps_glyph {
    cairo_hash_entry_t	    base;	    /* font glyph index */
    unsigned int	    output_glyph;   /* PS sub-font glyph index */
} cairo_ps_glyph_t;

typedef struct cairo_ps_font {
    cairo_hash_entry_t	    base;
    cairo_scaled_font_t	    *scaled_font;
    unsigned int	    output_font;
    cairo_hash_table_t	    *glyphs;
    unsigned int	    max_glyph;
} cairo_ps_font_t;

typedef struct _cairo_ps_font_glyph_select {
    cairo_ps_glyph_t	**glyphs;
    int			subfont;
    int			numglyph;
} cairo_ps_font_glyph_select_t;

cairo_private cairo_ps_font_t *
_cairo_ps_font_create (cairo_scaled_font_t	*scaled_font,
		       unsigned int		 id);

cairo_private void
_cairo_ps_font_destroy (cairo_ps_font_t *ps_font);

cairo_private void
_cairo_ps_font_key_init (cairo_ps_font_t	*ps_font,
			 cairo_scaled_font_t	*scaled_font);

cairo_private void
_cairo_ps_font_select_glyphs (void *entry, void *closure);

cairo_private void
_cairo_ps_font_destroy_glyph (cairo_ps_font_t	*ps_font,
			      cairo_ps_glyph_t	*ps_glyph);

cairo_private cairo_bool_t
_cairo_ps_font_equal (const void *key_a, const void *key_b);

cairo_private cairo_status_t
_cairo_ps_font_find_glyph (cairo_ps_font_t	 *font,
			   cairo_scaled_font_t   *scaled_font,
			   unsigned long	  index,
			   cairo_ps_glyph_t	**result);

#endif /* CAIRO_PS_FONT_PRIVATE_H */
