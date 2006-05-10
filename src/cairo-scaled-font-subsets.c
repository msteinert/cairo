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

#include "cairoint.h"
#include "cairo-ps-font-private.h"

static cairo_bool_t
_cairo_ps_glyph_equal (const void *key_a, const void *key_b)
{
    const cairo_ps_glyph_t   *ps_glyph_a = key_a;
    const cairo_ps_glyph_t   *ps_glyph_b = key_b;

    return ps_glyph_a->base.hash == ps_glyph_b->base.hash;
}

static void
_cairo_ps_glyph_key_init (cairo_ps_glyph_t  *ps_glyph,
			  unsigned long	    index)
{
    ps_glyph->base.hash = index;
}

static cairo_ps_glyph_t *
_cairo_ps_glyph_create (cairo_ps_font_t *ps_font,
			unsigned long index)
{
    cairo_ps_glyph_t	*ps_glyph = malloc (sizeof (cairo_ps_glyph_t));

    if (!ps_glyph)
	return NULL;
    _cairo_ps_glyph_key_init (ps_glyph, index);
    ps_glyph->output_glyph = ps_font->max_glyph++;
    return ps_glyph;
}

static void
_cairo_ps_glyph_destroy (cairo_ps_glyph_t *ps_glyph)
{
    free (ps_glyph);
}

cairo_status_t
_cairo_ps_font_find_glyph (cairo_ps_font_t	 *font,
			   cairo_scaled_font_t	 *scaled_font,
			   unsigned long	  index,
			   cairo_ps_glyph_t	**result)
{
    cairo_ps_glyph_t	key;
    cairo_ps_glyph_t	*ps_glyph;
    cairo_status_t	status;

    _cairo_ps_glyph_key_init (&key, index);
    if (! _cairo_hash_table_lookup (font->glyphs,
				    &key.base,
				    (cairo_hash_entry_t **) &ps_glyph)) {
	ps_glyph = _cairo_ps_glyph_create (font, index);
	if (!ps_glyph)
	    return CAIRO_STATUS_NO_MEMORY;
	status = _cairo_hash_table_insert (font->glyphs, &ps_glyph->base);
	if (status)
	    return status;
    }
    *result = ps_glyph;
    return CAIRO_STATUS_SUCCESS;
}

cairo_bool_t
_cairo_ps_font_equal (const void *key_a, const void *key_b)
{
    const cairo_ps_font_t   *ps_font_a = key_a;
    const cairo_ps_font_t   *ps_font_b = key_b;

    return ps_font_a->scaled_font == ps_font_b->scaled_font;
}

void
_cairo_ps_font_key_init (cairo_ps_font_t	*ps_font,
			 cairo_scaled_font_t	*scaled_font)
{
    ps_font->base.hash = (unsigned long) scaled_font;
    ps_font->scaled_font = scaled_font;
}

cairo_ps_font_t *
_cairo_ps_font_create (cairo_scaled_font_t	*scaled_font,
		       unsigned int		 id)
{
    cairo_ps_font_t *ps_font = malloc (sizeof (cairo_ps_font_t));
    if (!ps_font)
	return NULL;
    _cairo_ps_font_key_init (ps_font, scaled_font);
    ps_font->glyphs = _cairo_hash_table_create (_cairo_ps_glyph_equal);
    if (!ps_font->glyphs) {
	free (ps_font);
	return NULL;
    }
    ps_font->max_glyph = 0;
    ps_font->output_font = id;
    cairo_scaled_font_reference (ps_font->scaled_font);
    return ps_font;
}

void
_cairo_ps_font_destroy_glyph (cairo_ps_font_t	*ps_font,
			      cairo_ps_glyph_t	*ps_glyph)
{
    _cairo_hash_table_remove (ps_font->glyphs, &ps_glyph->base);
    _cairo_ps_glyph_destroy (ps_glyph);
}

static void
_cairo_ps_font_destroy_glyph_callback (void *entry, void *closure)
{
    cairo_ps_glyph_t	*ps_glyph = entry;
    cairo_ps_font_t	*ps_font = closure;

    _cairo_ps_font_destroy_glyph (ps_font, ps_glyph);
}

void
_cairo_ps_font_destroy (cairo_ps_font_t *ps_font)
{
    _cairo_hash_table_foreach (ps_font->glyphs,
			       _cairo_ps_font_destroy_glyph_callback,
			       ps_font);
    _cairo_hash_table_destroy (ps_font->glyphs);
    cairo_scaled_font_destroy (ps_font->scaled_font);
    free (ps_font);
}

void
_cairo_ps_font_select_glyphs (void *entry, void *closure)
{
    cairo_ps_glyph_t		    *ps_glyph = entry;
    cairo_ps_font_glyph_select_t    *ps_glyph_select = closure;

    if (ps_glyph->output_glyph >> 8 == ps_glyph_select->subfont) {
	unsigned long	sub_glyph = ps_glyph->output_glyph & 0xff;
	ps_glyph_select->glyphs[sub_glyph] = ps_glyph;
	if (sub_glyph >= ps_glyph_select->numglyph)
	    ps_glyph_select->numglyph = sub_glyph + 1;
    }
}
