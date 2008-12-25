/*
 * Copyright © 2004 Keith Packard
 * Copyright © 2008 Red Hat, Inc.
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
 * The Initial Developer of the Original Code is Keith Packard
 *
 * Contributor(s):
 *      Keith Packard <keithp@keithp.com>
 *      Behdad Esfahbod <behdad@behdad.org>
 */

#define _ISOC99_SOURCE /* for round() */
#include <math.h>
#include "cairoint.h"

#include <ctype.h>

/*
 * This file implements a user-font rendering the decendant of the Hershey
 * font coded by Keith Packard for use in the Twin window system.
 * The actual font data is in cairo-font-face-twin-data.c
 *
 * Ported to cairo user font and extended by Behdad Esfahbod.
 */



/*
 * Face properties
 */

/* We synthesize multiple faces from the twin data.  Here is the parameters. */

/* CSS weight */
typedef enum {
  TWIN_WEIGHT_ULTRALIGHT = 200,
  TWIN_WEIGHT_LIGHT = 300,
  TWIN_WEIGHT_NORMAL = 400,
  TWIN_WEIGHT_MEDIUM = 500,
  TWIN_WEIGHT_SEMIBOLD = 600,
  TWIN_WEIGHT_BOLD = 700,
  TWIN_WEIGHT_ULTRABOLD = 800,
  TWIN_WEIGHT_HEAVY = 900
} twin_face_wight;

/* CSS stretch */
typedef enum {
  TWIN_STRETCH_ULTRA_CONDENSED,
  TWIN_STRETCH_EXTRA_CONDENSED,
  TWIN_STRETCH_CONDENSED,
  TWIN_STRETCH_SEMI_CONDENSED,
  TWIN_STRETCH_NORMAL,
  TWIN_STRETCH_SEMI_EXPANDED,
  TWIN_STRETCH_EXPANDED,
  TWIN_STRETCH_EXTRA_EXPANDED,
  TWIN_STRETCH_ULTRA_EXPANDED
} twin_face_stretch;


typedef struct _twin_face_properties {
    cairo_font_slant_t slant;
    twin_face_wight    weight;
    twin_face_stretch  stretch;

    /* lets have some fun */
    cairo_bool_t monospace;
    cairo_bool_t smallcaps;
} twin_face_properties_t;

cairo_user_data_key_t twin_face_properties_key;

#define TOLOWER(c) \
   (((c) >= 'A' && (c) <= 'Z') ? (c) - 'A' + 'a' : (c))

static cairo_bool_t
field_matches (const char *s1,
               const char *s2,
               int len)
{
  int c1, c2;

  while (len && *s1 && *s2)
    {
      c1 = TOLOWER (*s1);
      c2 = TOLOWER (*s2);
      if (c1 != c2) {
        if (c1 == '-') {
          s1++;
          continue;
        }
        return FALSE;
      }
      s1++; s2++;
      len--;
    }

  return len == 0 && *s1 == '\0';
}


static void
parse_field (twin_face_properties_t *props,
	     const char *s,
	     int len)
{
#define MATCH(s1, var, value) \
	if (field_matches (s1, s, len)) var = value


         MATCH ("oblique",    props->slant, CAIRO_FONT_SLANT_OBLIQUE);
    else MATCH ("italic",     props->slant, CAIRO_FONT_SLANT_ITALIC);


    else MATCH ("ultra-light", props->weight, TWIN_WEIGHT_ULTRALIGHT);
    else MATCH ("light",       props->weight, TWIN_WEIGHT_LIGHT);
    else MATCH ("medium",      props->weight, TWIN_WEIGHT_NORMAL);
    else MATCH ("semi-bold",   props->weight, TWIN_WEIGHT_SEMIBOLD);
    else MATCH ("bold",        props->weight, TWIN_WEIGHT_BOLD);
    else MATCH ("ultra-bold",  props->weight, TWIN_WEIGHT_ULTRABOLD);
    else MATCH ("heavy",       props->weight, TWIN_WEIGHT_HEAVY);

    else MATCH ("ultra-condensed", props->stretch, TWIN_STRETCH_ULTRA_CONDENSED);
    else MATCH ("extra-condensed", props->stretch, TWIN_STRETCH_EXTRA_CONDENSED);
    else MATCH ("condensed",       props->stretch, TWIN_STRETCH_CONDENSED);
    else MATCH ("semi-condensed",  props->stretch, TWIN_STRETCH_SEMI_CONDENSED);
    else MATCH ("semi-expanded",   props->stretch, TWIN_STRETCH_SEMI_EXPANDED);
    else MATCH ("expanded",        props->stretch, TWIN_STRETCH_EXPANDED);
    else MATCH ("extra-expanded",  props->stretch, TWIN_STRETCH_EXTRA_EXPANDED);
    else MATCH ("ultra-expanded",  props->stretch, TWIN_STRETCH_ULTRA_EXPANDED);

    else MATCH ("small-caps", props->smallcaps, TRUE);

    else MATCH ("mono",       props->monospace, TRUE);
    else MATCH ("monospace",  props->monospace, TRUE);
}

static void
props_parse (twin_face_properties_t *props,
	     const char *s)
{
    const char *start, *end;

    for (start = end = s; *end; end++) {
	if (isalpha (*end) || *end == '-')
	    continue;

	if (start < end)
		parse_field (props, start, end - start);
	start = end + 1;
    }
    if (start < end)
	    parse_field (props, start, end - start);
}

static cairo_status_t
twin_set_face_properties_from_toy (cairo_font_face_t *twin_face,
				   cairo_toy_font_face_t *toy_face)
{
    cairo_status_t status;
    twin_face_properties_t *props;

    props = malloc (sizeof (twin_face_properties_t));
    if (unlikely (props == NULL))
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    props->stretch  = TWIN_STRETCH_NORMAL;
    props->monospace = FALSE;
    props->smallcaps = FALSE;

    /* fill in props */
    props->slant = toy_face->slant;
    props->weight = toy_face->weight == CAIRO_FONT_WEIGHT_NORMAL ?
		    TWIN_WEIGHT_NORMAL : TWIN_WEIGHT_BOLD;
    props_parse (props, toy_face->family);

    status = cairo_font_face_set_user_data (twin_face,
					    &twin_face_properties_key,
					    props, free);
    if (status)
	goto FREE_PROPS;

    return CAIRO_STATUS_SUCCESS;

FREE_PROPS:
    free (props);
    return status;
}


#define twin_glyph_left(g)      ((g)[0])
#define twin_glyph_right(g)     ((g)[1])
#define twin_glyph_ascent(g)    ((g)[2])
#define twin_glyph_descent(g)   ((g)[3])

#define twin_glyph_n_snap_x(g)  ((g)[4])
#define twin_glyph_n_snap_y(g)  ((g)[5])
#define twin_glyph_snap_x(g)    (&g[6])
#define twin_glyph_snap_y(g)    (twin_glyph_snap_x(g) + twin_glyph_n_snap_x(g))
#define twin_glyph_draw(g)      (twin_glyph_snap_y(g) + twin_glyph_n_snap_y(g))

#define F(g)		((g) / 72.)


static cairo_status_t
twin_scaled_font_init (cairo_scaled_font_t  *scaled_font,
		       cairo_t              *cr,
		       cairo_font_extents_t *metrics)
{
  metrics->ascent  = F (54);
  metrics->descent = 1 - metrics->ascent;
  return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
twin_scaled_font_unicode_to_glyph (cairo_scaled_font_t *scaled_font,
				   unsigned long        unicode,
				   unsigned long       *glyph)
{
    /* We use an identity charmap.  Which means we could live
     * with no unicode_to_glyph method too.  But we define this
     * to map all unknown chars to a single unknown glyph to
     * reduce pressure on cache. */

    if (likely (unicode < ARRAY_LENGTH (_cairo_twin_charmap)))
	*glyph = unicode;
    else
	*glyph = 0;

    return CAIRO_STATUS_SUCCESS;
}

#define SNAPX(p)	_twin_snap (p, info.snap, info.snap_x, info.snapped_x, info.n_snap_x)
#define SNAPY(p)	_twin_snap (p, info.snap, info.snap_y, info.snapped_y, info.n_snap_y)

#define TWIN_GLYPH_MAX_SNAP_X 4
#define TWIN_GLYPH_MAX_SNAP_Y 7

#define SNAPXI(p)	(round ((p) * info->x_scale) * info->x_scale_inv)
#define SNAPYI(p)	(round ((p) * info->y_scale) * info->y_scale_inv)

static double
_twin_snap (int8_t v, cairo_bool_t do_snap, int8_t *snap, double *snapped, int n)
{
    int	s;

    if (!do_snap)
	return F(v);

    if (snap[0] == v)
	return snapped[0];

    for (s = 0; s < n - 1; s++)
    {
	if (snap[s+1] == v)
	    return snapped[s+1];

	if (snap[s] <= v && v <= snap[s+1])
	{
	    int before = snap[s];
	    int after = snap[s+1];
	    int dist = after - before;
	    double snap_before = snapped[s];
	    double snap_after = snapped[s+1];
	    double dist_before = v - before;
	    return snap_before + (snap_after - snap_before) * dist_before / dist;
	}
    }
    return F(v);
}

typedef struct {
    cairo_bool_t snap;

    double x_scale, x_scale_inv, x_off;
    double y_scale, y_scale_inv, y_off;

    int n_snap_x;
    int8_t snap_x[TWIN_GLYPH_MAX_SNAP_X];
    double snapped_x[TWIN_GLYPH_MAX_SNAP_X];
    int n_snap_y;
    int8_t snap_y[TWIN_GLYPH_MAX_SNAP_Y];
    double snapped_y[TWIN_GLYPH_MAX_SNAP_Y];
} twin_snap_info_t;

static void
_twin_compute_snap (cairo_t             *cr,
		    cairo_scaled_font_t *scaled_font,
		    twin_snap_info_t    *info,
		    const signed char   *b)
{
    int			s, n;
    const signed char	*snap;
    double x, y;

    info->snap = scaled_font->options.hint_style > CAIRO_HINT_STYLE_NONE;
    if (!info->snap)
	return;

    x = 1; y = 0;
    cairo_user_to_device_distance (cr, &x, &y);
    info->x_scale = sqrt (x*x + y*y);
    info->x_scale_inv = 1 / info->x_scale;

    x = 0; y = 1;
    cairo_user_to_device_distance (cr, &x, &y);
    info->y_scale = sqrt (x*x + y*y);
    info->y_scale_inv = 1 / info->y_scale;


    snap = twin_glyph_snap_x (b);
    n = twin_glyph_n_snap_x (b);
    info->n_snap_x = n;
    assert (n <= TWIN_GLYPH_MAX_SNAP_X);
    for (s = 0; s < n; s++) {
	info->snap_x[s] = snap[s];
	info->snapped_x[s] = SNAPXI (F (snap[s]));
    }

    snap = twin_glyph_snap_y (b);
    n = twin_glyph_n_snap_y (b);
    info->n_snap_y = n;
    assert (n <= TWIN_GLYPH_MAX_SNAP_Y);
    for (s = 0; s < n; s++) {
	info->snap_y[s] = snap[s];
	info->snapped_y[s] = SNAPYI (F (snap[s]));
    }
}

static void
_twin_compute_pen (cairo_t             *cr,
		   cairo_scaled_font_t *scaled_font,
		   double width,
		   double *penx, double *peny)
{
    double x, y;
    double scale, inv;
    cairo_bool_t hint;

    hint = scaled_font->options.hint_style > CAIRO_HINT_STYLE_SLIGHT;
    if (!hint) {
	*penx = *peny = width;
	return;
    }

    x = 1; y = 0;
    cairo_user_to_device_distance (cr, &x, &y);
    scale = sqrt (x*x + y*y);
    inv = 1 / scale;
    *penx = round (width * scale) * inv;
    if (*penx < inv)
	*penx = inv;

    x = 0; y = 1;
    cairo_user_to_device_distance (cr, &x, &y);
    scale = sqrt (x*x + y*y);
    inv = 1 / scale;
    *peny = round (width * scale) * inv;
    if (*peny < inv)
	*peny = inv;
}


static cairo_status_t
twin_scaled_font_render_glyph (cairo_scaled_font_t  *scaled_font,
			       unsigned long         glyph,
			       cairo_t              *cr,
			       cairo_text_extents_t *metrics)
{
    double x1, y1, x2, y2, x3, y3;
    twin_face_properties_t *props;
    twin_snap_info_t info;
    const int8_t *b;
    const int8_t *g;
    int8_t w;
    double gw;
    double weight, stretch;
    double penx, peny;

    cairo_set_tolerance (cr, 0.01);
    cairo_set_line_join (cr, CAIRO_LINE_JOIN_ROUND);
    cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND);

    /* Prepare face */

    props = cairo_font_face_get_user_data (cairo_scaled_font_get_font_face (scaled_font),
					   &twin_face_properties_key);

    /* weight */
    weight = props->weight * (5. / 64 / TWIN_WEIGHT_NORMAL);

    /* stretch */
    stretch = 1 + .05 * ((int) props->stretch - (int) TWIN_STRETCH_NORMAL);
    cairo_scale (cr, stretch, 1);

    /* lock pen matrix */
    _twin_compute_pen (cr, scaled_font, weight, &penx, &peny);
    cairo_save (cr);

    /* left margin + pen width, pen width */
    cairo_translate (cr, penx * 1.5, -peny * .5);

    /* slant */
    if (props->slant != CAIRO_FONT_SLANT_NORMAL) {
	cairo_matrix_t shear = { 1, 0, -.2, 1, 0, 0};
	cairo_transform (cr, &shear);
    }

    if (props->smallcaps && glyph >= 'a' && glyph <= 'z') {
	glyph += 'A' - 'a';
	cairo_scale (cr, 1, 28. / 42);
    }

    b = _cairo_twin_outlines +
	_cairo_twin_charmap[unlikely (glyph >= ARRAY_LENGTH (_cairo_twin_charmap)) ? 0 : glyph];
    g = twin_glyph_draw(b);
    w = twin_glyph_right(b);
    gw = F(w);

    /* monospace */
    if (props->monospace) {
	double monow = F(24);
	cairo_scale (cr, (monow+penx) / (gw+penx), 1);
	gw = monow;
    }

    _twin_compute_snap (cr, scaled_font, &info, b);

    /* advance width */
    metrics->x_advance = gw + penx * 3; /* pen width + margin */
    metrics->x_advance *= stretch;

    /* glyph shape */
    for (;;) {
	switch (*g++) {
	case 'M':
	    cairo_close_path (cr);
	    /* fall through */
	case 'm':
	    x1 = SNAPX(*g++);
	    y1 = SNAPY(*g++);
	    cairo_move_to (cr, x1, y1);
	    continue;
	case 'L':
	    cairo_close_path (cr);
	    /* fall through */
	case 'l':
	    x1 = SNAPX(*g++);
	    y1 = SNAPY(*g++);
	    cairo_line_to (cr, x1, y1);
	    continue;
	case 'C':
	    cairo_close_path (cr);
	    /* fall through */
	case 'c':
	    x1 = SNAPX(*g++);
	    y1 = SNAPY(*g++);
	    x2 = SNAPX(*g++);
	    y2 = SNAPY(*g++);
	    x3 = SNAPX(*g++);
	    y3 = SNAPY(*g++);
	    cairo_curve_to (cr, x1, y1, x2, y2, x3, y3);
	    continue;
	case 'E':
	    cairo_close_path (cr);
	    /* fall through */
	case 'e':
	    cairo_restore (cr);
	    cairo_scale (cr, penx, peny);
	    cairo_set_line_width (cr, 1);
	    cairo_stroke (cr);
	    break;
	case 'X':
	    /* filler */
	    continue;
	}
	break;
    }

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_font_face_twin_create_for_toy (cairo_toy_font_face_t   *toy_face,
				      cairo_font_face_t      **font_face)
{
    cairo_status_t status;
    cairo_font_face_t *twin_font_face;

    twin_font_face = cairo_user_font_face_create ();
    cairo_user_font_face_set_init_func             (twin_font_face, twin_scaled_font_init);
    cairo_user_font_face_set_render_glyph_func     (twin_font_face, twin_scaled_font_render_glyph);
    cairo_user_font_face_set_unicode_to_glyph_func (twin_font_face, twin_scaled_font_unicode_to_glyph);
    status = twin_set_face_properties_from_toy (twin_font_face, toy_face);
    if (status)
	return status;

    *font_face = twin_font_face;

    return CAIRO_STATUS_SUCCESS;
}
