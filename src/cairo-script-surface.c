/* -*- Mode: c; tab-width: 8; c-basic-offset: 4; indent-tabs-mode: t; -*- */
/* cairo - a vector graphics library with display and print output
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
 *
 * Contributor(s):
 *      Chris Wilson <chris@chris-wilson.co.uk>
 */

/* The script surface is one that records all operations performed on
 * it in the form of a procedural script, similar in fashion to
 * PostScript but using Cairo's imaging model. In essence, this is
 * equivalent to the meta-surface, but as there is no impedance mismatch
 * between Cairo and CairoScript, we can generate output immediately
 * without having to copy and hold the data in memory.
 */

#include "cairoint.h"

#include "cairo-script.h"

#include "cairo-analysis-surface-private.h"
#include "cairo-ft-private.h"
#include "cairo-meta-surface-private.h"
#include "cairo-output-stream-private.h"

#define _cairo_output_stream_puts(S, STR) \
    _cairo_output_stream_write ((S), (STR), strlen (STR))

#define static cairo_warn static

typedef struct _cairo_script_vmcontext cairo_script_vmcontext_t;
typedef struct _cairo_script_surface cairo_script_surface_t;
typedef struct _cairo_script_implicit_context cairo_script_implicit_context_t;
typedef struct _cairo_script_surface_font_private cairo_script_surface_font_private_t;

struct _cairo_script_vmcontext {
    int ref;

    cairo_output_stream_t *stream;
    cairo_script_mode_t mode;

    struct _bitmap {
	unsigned long min;
	unsigned long count;
	unsigned int map[64];
	struct _bitmap *next;
    } surface_id, font_id;

    cairo_script_surface_t *current_target;

    cairo_script_surface_font_private_t *fonts;
};

struct _cairo_script_surface_font_private {
    cairo_script_vmcontext_t *ctx;
    cairo_bool_t has_sfnt;
    unsigned long id;
    unsigned long subset_glyph_index;
    cairo_script_surface_font_private_t *prev, *next;
    cairo_scaled_font_t *parent;
};

struct _cairo_script_implicit_context {
    cairo_operator_t current_operator;
    cairo_fill_rule_t current_fill_rule;
    double current_tolerance;
    cairo_antialias_t current_antialias;
    cairo_stroke_style_t current_style;
    cairo_pattern_t *current_source;
    cairo_matrix_t current_ctm;
    cairo_matrix_t current_font_matrix;
    cairo_font_options_t current_font_options;
    cairo_scaled_font_t *current_scaled_font;
    cairo_path_fixed_t current_path;
};

struct _cairo_script_surface {
    cairo_surface_t base;

    cairo_script_vmcontext_t *ctx;

    unsigned long id;

    double width, height;

    /* implicit flattened context */
    cairo_script_implicit_context_t cr;
};

static const cairo_surface_backend_t _cairo_script_surface_backend;

static cairo_script_surface_t *
_cairo_script_surface_create_internal (cairo_script_vmcontext_t *ctx,
				       double width,
				       double height);

static void
_cairo_script_surface_scaled_font_fini (cairo_scaled_font_t *scaled_font);

static void
_cairo_script_implicit_context_init (cairo_script_implicit_context_t *cr);

static void
_bitmap_release_id (struct _bitmap *b, unsigned long token)
{
    struct _bitmap **prev = NULL;

    do {
	if (token < b->min + sizeof (b->map) * CHAR_BIT) {
	    unsigned int bit, elem;

	    token -= b->min;
	    elem = token / (sizeof (b->map[0]) * CHAR_BIT);
	    bit  = token % (sizeof (b->map[0]) * CHAR_BIT);
	    b->map[elem] &= ~(1 << bit);
	    if (! --b->count && prev) {
		*prev = b->next;
		free (b);
	    }
	    return;
	}
	prev = &b->next;
	b = b->next;
    } while (b != NULL);
}

static cairo_status_t
_bitmap_next_id (struct _bitmap *b,
		 unsigned long *id)
{
    struct _bitmap *bb, **prev = NULL;
    unsigned long min = 0;

    do {
	if (b->min != min)
	    break;

	if (b->count < sizeof (b->map) * CHAR_BIT) {
	    unsigned int n, m, bit;
	    for (n = 0; n < ARRAY_LENGTH (b->map); n++) {
		if (b->map[n] == (unsigned int) -1)
		    continue;

		for (m=0, bit=1; m<sizeof (b->map[0])*CHAR_BIT; m++, bit<<=1) {
		    if ((b->map[n] & bit) == 0) {
			b->map[n] |= bit;
			b->count++;
			*id = n * sizeof (b->map[0])*CHAR_BIT + m + b->min;
			return CAIRO_STATUS_SUCCESS;
		    }
		}
	    }
	}
	min += sizeof (b->map) * CHAR_BIT;

	prev = &b->next;
	b = b->next;
    } while (b != NULL);

    bb = malloc (sizeof (struct _bitmap));
    if (unlikely (bb == NULL))
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    *prev = bb;
    bb->next = b;
    bb->min = min;
    bb->count = 1;
    bb->map[0] = 0x1;
    memset (bb->map + 1, 0, sizeof (bb->map) - sizeof (bb->map[0]));
    *id = min;

    return CAIRO_STATUS_SUCCESS;
}

static const char *
_direction_to_string (cairo_bool_t backward)
{
    static const char *names[] = {
	"FORWARD",
	"BACKWARD"
    };
    assert (backward < ARRAY_LENGTH (names));
    return names[backward];
}

static const char *
_operator_to_string (cairo_operator_t op)
{
    static const char *names[] = {
	"CLEAR",	/* CAIRO_OPERATOR_CLEAR */

	"SOURCE",	/* CAIRO_OPERATOR_SOURCE */
	"OVER",		/* CAIRO_OPERATOR_OVER */
	"IN",		/* CAIRO_OPERATOR_IN */
	"OUT",		/* CAIRO_OPERATOR_OUT */
	"ATOP",		/* CAIRO_OPERATOR_ATOP */

	"DEST",		/* CAIRO_OPERATOR_DEST */
	"DEST_OVER",	/* CAIRO_OPERATOR_DEST_OVER */
	"DEST_IN",	/* CAIRO_OPERATOR_DEST_IN */
	"DEST_OUT",	/* CAIRO_OPERATOR_DEST_OUT */
	"DEST_ATOP",	/* CAIRO_OPERATOR_DEST_ATOP */

	"XOR",		/* CAIRO_OPERATOR_XOR */
	"ADD",		/* CAIRO_OPERATOR_ADD */
	"SATURATE"	/* CAIRO_OPERATOR_SATURATE */
    };
    assert (op < ARRAY_LENGTH (names));
    return names[op];
}

static const char *
_extend_to_string (cairo_extend_t extend)
{
    static const char *names[] = {
	"EXTEND_NONE",		/* CAIRO_EXTEND_NONE */
	"EXTEND_REPEAT",	/* CAIRO_EXTEND_REPEAT */
	"EXTEND_REFLECT",	/* CAIRO_EXTEND_REFLECT */
	"EXTEND_PAD"		/* CAIRO_EXTEND_PAD */
    };
    assert (extend < ARRAY_LENGTH (names));
    return names[extend];
}

static const char *
_filter_to_string (cairo_filter_t filter)
{
    static const char *names[] = {
	"FILTER_FAST",		/* CAIRO_FILTER_FAST */
	"FILTER_GOOD",		/* CAIRO_FILTER_GOOD */
	"FILTER_BEST",		/* CAIRO_FILTER_BEST */
	"FILTER_NEAREST",	/* CAIRO_FILTER_NEAREST */
	"FILTER_BILINEAR",	/* CAIRO_FILTER_BILINEAR */
	"FILTER_GAUSSIAN",	/* CAIRO_FILTER_GAUSSIAN */
    };
    assert (filter < ARRAY_LENGTH (names));
    return names[filter];
}

static const char *
_fill_rule_to_string (cairo_fill_rule_t rule)
{
    static const char *names[] = {
	"WINDING",	/* CAIRO_FILL_RULE_WINDING */
	"EVEN_ODD"	/* CAIRO_FILL_RILE_EVEN_ODD */
    };
    assert (rule < ARRAY_LENGTH (names));
    return names[rule];
}

static const char *
_antialias_to_string (cairo_antialias_t antialias)
{
    static const char *names[] = {
	"ANTIALIAS_DEFAULT",	/* CAIRO_ANTIALIAS_DEFAULT */
	"ANTIALIAS_NONE",	/* CAIRO_ANTIALIAS_NONE */
	"ANTIALIAS_GRAY",	/* CAIRO_ANTIALIAS_GRAY */
	"ANTIALIAS_SUBPIXEL"	/* CAIRO_ANTIALIAS_SUBPIXEL */
    };
    assert (antialias < ARRAY_LENGTH (names));
    return names[antialias];
}

static const char *
_line_cap_to_string (cairo_line_cap_t line_cap)
{
    static const char *names[] = {
	"LINE_CAP_BUTT",	/* CAIRO_LINE_CAP_BUTT */
	"LINE_CAP_ROUND",	/* CAIRO_LINE_CAP_ROUND */
	"LINE_CAP_SQUARE"	/* CAIRO_LINE_CAP_SQUARE */
    };
    assert (line_cap < ARRAY_LENGTH (names));
    return names[line_cap];
}

static const char *
_line_join_to_string (cairo_line_join_t line_join)
{
    static const char *names[] = {
	"LINE_JOIN_MITER",	/* CAIRO_LINE_JOIN_MITER */
	"LINE_JOIN_ROUND",	/* CAIRO_LINE_JOIN_ROUND */
	"LINE_JOIN_BEVEL",	/* CAIRO_LINE_JOIN_BEVEL */
    };
    assert (line_join < ARRAY_LENGTH (names));
    return names[line_join];
}

static cairo_bool_t
_cairo_script_surface_owns_context (cairo_script_surface_t *surface)
{
    return surface->ctx->current_target == surface;
}

static cairo_status_t
_emit_context (cairo_script_surface_t *surface)
{
    if (_cairo_script_surface_owns_context (surface))
	return CAIRO_STATUS_SUCCESS;

    if (surface->ctx->current_target != NULL)
	_cairo_output_stream_puts (surface->ctx->stream, "pop\n");

    surface->ctx->current_target = surface;

    if (surface->id == (unsigned long) -1) {
	cairo_status_t status;

	status = _bitmap_next_id (&surface->ctx->surface_id,
				  &surface->id);
	if (unlikely (status))
	    return status;

	_cairo_output_stream_printf (surface->ctx->stream,
				     "dict\n"
				     "  /width %f set\n"
				     "  /height %f set\n",
				     surface->width,
				     surface->height);
	if (surface->base.x_fallback_resolution !=
	    CAIRO_SURFACE_FALLBACK_RESOLUTION_DEFAULT ||
	    surface->base.y_fallback_resolution !=
	    CAIRO_SURFACE_FALLBACK_RESOLUTION_DEFAULT)
	{
	    _cairo_output_stream_printf (surface->ctx->stream,
					 "  /fallback-resolution [%f %f] set\n",
					 surface->base.x_fallback_resolution,
					 surface->base.y_fallback_resolution);
	}
	if (surface->base.device_transform.x0 != 0. ||
	    surface->base.device_transform.y0 != 0.)
	{
	    /* XXX device offset is encoded into the pattern matrices etc. */
	    _cairo_output_stream_printf (surface->ctx->stream,
					 "  %%/device-offset [%f %f] set\n",
					 surface->base.device_transform.x0,
					 surface->base.device_transform.y0);
	}
	_cairo_output_stream_printf (surface->ctx->stream,
				     "  surface dup /s%lu exch def\n"
				     "context dup /c%lu exch def\n",
				     surface->id,
				     surface->id);
    } else {
	_cairo_output_stream_printf (surface->ctx->stream,
				     "c%lu\n",
				     surface->id);
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_emit_operator (cairo_script_surface_t *surface,
		cairo_operator_t op)
{
    assert (_cairo_script_surface_owns_context (surface));

    if (surface->cr.current_operator == op)
	return CAIRO_STATUS_SUCCESS;

    surface->cr.current_operator = op;

    _cairo_output_stream_printf (surface->ctx->stream,
				 "//%s set-operator\n",
				 _operator_to_string (op));
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_emit_fill_rule (cairo_script_surface_t *surface,
		 cairo_fill_rule_t fill_rule)
{
    assert (_cairo_script_surface_owns_context (surface));

    if (surface->cr.current_fill_rule == fill_rule)
	return CAIRO_STATUS_SUCCESS;

    surface->cr.current_fill_rule = fill_rule;

    _cairo_output_stream_printf (surface->ctx->stream,
				 "//%s set-fill-rule\n",
				 _fill_rule_to_string (fill_rule));
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_emit_tolerance (cairo_script_surface_t *surface,
		 double tolerance,
		 cairo_bool_t force)
{
    assert (_cairo_script_surface_owns_context (surface));

    if (! force && surface->cr.current_tolerance == tolerance)
	return CAIRO_STATUS_SUCCESS;

    surface->cr.current_tolerance = tolerance;

    _cairo_output_stream_printf (surface->ctx->stream,
				 "%f set-tolerance\n",
				 tolerance);
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_emit_antialias (cairo_script_surface_t *surface,
		 cairo_antialias_t antialias)
{
    assert (_cairo_script_surface_owns_context (surface));

    if (surface->cr.current_antialias == antialias)
	return CAIRO_STATUS_SUCCESS;

    surface->cr.current_antialias = antialias;

    _cairo_output_stream_printf (surface->ctx->stream,
				 "//%s set-antialias\n",
				 _antialias_to_string (antialias));

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_emit_line_width (cairo_script_surface_t *surface,
		 double line_width,
		 cairo_bool_t force)
{
    assert (_cairo_script_surface_owns_context (surface));

    if (! force && surface->cr.current_style.line_width == line_width)
	return CAIRO_STATUS_SUCCESS;

    surface->cr.current_style.line_width = line_width;

    _cairo_output_stream_printf (surface->ctx->stream,
				 "%f set-line-width\n",
				 line_width);
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_emit_line_cap (cairo_script_surface_t *surface,
		cairo_line_cap_t line_cap)
{
    assert (_cairo_script_surface_owns_context (surface));

    if (surface->cr.current_style.line_cap == line_cap)
	return CAIRO_STATUS_SUCCESS;

    surface->cr.current_style.line_cap = line_cap;

    _cairo_output_stream_printf (surface->ctx->stream,
				 "//%s set-line-cap\n",
				 _line_cap_to_string (line_cap));
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_emit_line_join (cairo_script_surface_t *surface,
		 cairo_line_join_t line_join)
{
    assert (_cairo_script_surface_owns_context (surface));

    if (surface->cr.current_style.line_join == line_join)
	return CAIRO_STATUS_SUCCESS;

    surface->cr.current_style.line_join = line_join;

    _cairo_output_stream_printf (surface->ctx->stream,
				 "//%s set-line-join\n",
				 _line_join_to_string (line_join));
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_emit_miter_limit (cairo_script_surface_t *surface,
		   double miter_limit,
		   cairo_bool_t force)
{
    assert (_cairo_script_surface_owns_context (surface));

    if (! force && surface->cr.current_style.miter_limit == miter_limit)
	return CAIRO_STATUS_SUCCESS;

    surface->cr.current_style.miter_limit = miter_limit;

    _cairo_output_stream_printf (surface->ctx->stream,
				 "%f set-miter-limit\n",
				 miter_limit);
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_emit_dash (cairo_script_surface_t *surface,
	    const double *dash,
	    unsigned int num_dashes,
	    double offset,
	    cairo_bool_t force)
{
    unsigned int n;

    assert (_cairo_script_surface_owns_context (surface));

    if (force &&
	num_dashes == 0 &&
	surface->cr.current_style.num_dashes == 0)
    {
	return CAIRO_STATUS_SUCCESS;
    }

    if (! force &&
	(surface->cr.current_style.num_dashes == num_dashes &&
	 (num_dashes == 0 ||
	  (surface->cr.current_style.dash_offset == offset &&
	   memcmp (surface->cr.current_style.dash, dash,
		   sizeof (double) * num_dashes)))))
    {
	return CAIRO_STATUS_SUCCESS;
    }


    if (num_dashes) {
	surface->cr.current_style.dash = _cairo_realloc_ab
	    (surface->cr.current_style.dash,
	     num_dashes,
	     sizeof (double));
	memcpy (surface->cr.current_style.dash, dash,
		sizeof (double) * num_dashes);
    } else {
	if (surface->cr.current_style.dash != NULL) {
	    free (surface->cr.current_style.dash);
	    surface->cr.current_style.dash = NULL;
	}
    }

    surface->cr.current_style.num_dashes = num_dashes;
    surface->cr.current_style.dash_offset = offset;

    _cairo_output_stream_printf (surface->ctx->stream, "[");
    for (n = 0; n < num_dashes; n++) {
	_cairo_output_stream_printf (surface->ctx->stream, "%f", dash[n]);
	if (n < num_dashes-1)
	    _cairo_output_stream_puts (surface->ctx->stream, " ");
    }
    _cairo_output_stream_printf (surface->ctx->stream,
				 "] %f set-dash\n",
				 offset);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_emit_stroke_style (cairo_script_surface_t *surface,
		    const cairo_stroke_style_t *style,
		    cairo_bool_t force)
{
    cairo_status_t status;

    assert (_cairo_script_surface_owns_context (surface));

    status = _emit_line_width (surface, style->line_width, force);
    if (unlikely (status))
	return status;

    status = _emit_line_cap (surface, style->line_cap);
    if (unlikely (status))
	return status;

    status = _emit_line_join (surface, style->line_join);
    if (unlikely (status))
	return status;

    status = _emit_miter_limit (surface, style->miter_limit, force);
    if (unlikely (status))
	return status;

    status = _emit_dash (surface,
			 style->dash, style->num_dashes, style->dash_offset,
			 force);
    if (unlikely (status))
	return status;

    return CAIRO_STATUS_SUCCESS;
}

static const char *
_format_to_string (cairo_format_t format)
{
    static const char *names[] = {
	"ARGB32",	/* CAIRO_FORMAT_ARGB32 */
	"RGB24",	/* CAIRO_FORMAT_RGB24 */
	"A8",		/* CAIRO_FORMAT_A8 */
	"A1"		/* CAIRO_FORMAT_A1 */
    };
    assert (format < ARRAY_LENGTH (names));
    return names[format];
}

static cairo_status_t
_emit_solid_pattern (cairo_script_surface_t *surface,
		     const cairo_pattern_t *pattern)
{
    cairo_solid_pattern_t *solid = (cairo_solid_pattern_t *) pattern;

    if (solid->content & CAIRO_CONTENT_ALPHA &&
	! CAIRO_COLOR_IS_OPAQUE (&solid->color))
    {
	if (! (solid->content & CAIRO_CONTENT_COLOR) ||
	    (solid->color.red_short   == 0 &&
	     solid->color.green_short == 0 &&
	     solid->color.blue_short  == 0))
	{
	    _cairo_output_stream_printf (surface->ctx->stream,
					 "%f a",
					 solid->color.alpha);
	}
	else
	{
	    _cairo_output_stream_printf (surface->ctx->stream,
					 "%f %f %f %f rgba",
					 solid->color.red,
					 solid->color.green,
					 solid->color.blue,
					 solid->color.alpha);
	}
    }
    else
    {
	if (solid->color.red_short == solid->color.green_short &&
	    solid->color.red_short == solid->color.blue_short)
	{
	    _cairo_output_stream_printf (surface->ctx->stream,
					 "%f g",
					 solid->color.red);
	}
	else
	{
	    _cairo_output_stream_printf (surface->ctx->stream,
					 "%f %f %f rgb",
					 solid->color.red,
					 solid->color.green,
					 solid->color.blue);
	}
    }

    return CAIRO_STATUS_SUCCESS;
}


static cairo_status_t
_emit_gradient_color_stops (cairo_gradient_pattern_t *gradient,
			    cairo_output_stream_t *output)
{
    unsigned int n;

    for (n = 0; n < gradient->n_stops; n++) {
	_cairo_output_stream_printf (output,
				     " %f %f %f %f %f add-color-stop\n ",
				     gradient->stops[n].offset,
				     gradient->stops[n].color.red,
				     gradient->stops[n].color.green,
				     gradient->stops[n].color.blue,
				     gradient->stops[n].color.alpha);
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_emit_linear_pattern (cairo_script_surface_t *surface,
		      const cairo_pattern_t *pattern)
{
    cairo_linear_pattern_t *linear;

    linear = (cairo_linear_pattern_t *) pattern;

    _cairo_output_stream_printf (surface->ctx->stream,
				 "%f %f %f %f linear\n ",
				 _cairo_fixed_to_double (linear->p1.x),
				 _cairo_fixed_to_double (linear->p1.y),
				 _cairo_fixed_to_double (linear->p2.x),
				 _cairo_fixed_to_double (linear->p2.y));
    return _emit_gradient_color_stops (&linear->base, surface->ctx->stream);
}

static cairo_status_t
_emit_radial_pattern (cairo_script_surface_t *surface,
		      const cairo_pattern_t *pattern)
{
    cairo_radial_pattern_t *radial;

    radial = (cairo_radial_pattern_t *) pattern;

    _cairo_output_stream_printf (surface->ctx->stream,
				 "%f %f %f %f %f %f radial\n ",
				 _cairo_fixed_to_double (radial->c1.x),
				 _cairo_fixed_to_double (radial->c1.y),
				 _cairo_fixed_to_double (radial->r1),
				 _cairo_fixed_to_double (radial->c2.x),
				 _cairo_fixed_to_double (radial->c2.y),
				 _cairo_fixed_to_double (radial->r2));
    return _emit_gradient_color_stops (&radial->base, surface->ctx->stream);
}

static cairo_status_t
_emit_meta_surface_pattern (cairo_script_surface_t *surface,
			    const cairo_pattern_t *pattern)
{
    cairo_surface_pattern_t *surface_pattern;
    cairo_surface_t *source;
    cairo_surface_t *null_surface;
    cairo_surface_t *analysis_surface;
    cairo_surface_t *similar;
    cairo_status_t status;
    cairo_box_t bbox;

    surface_pattern = (cairo_surface_pattern_t *) pattern;
    source = surface_pattern->surface;

    /* first measure the extents */
    null_surface = _cairo_null_surface_create (source->content);
    analysis_surface = _cairo_analysis_surface_create (null_surface, -1, -1);
    cairo_surface_destroy (null_surface);

    status = analysis_surface->status;
    if (unlikely (status))
	return status;

    status = _cairo_meta_surface_replay (source, analysis_surface);
    _cairo_analysis_surface_get_bounding_box (analysis_surface, &bbox);
    cairo_surface_destroy (analysis_surface);
    if (unlikely (status))
	return status;

    similar = cairo_surface_create_similar (&surface->base,
					    source->content,
					    _cairo_fixed_to_double (bbox.p2.x-bbox.p1.x),
					    _cairo_fixed_to_double (bbox.p2.y-bbox.p1.y));
    if (similar->status)
	return similar->status;

    status = _cairo_meta_surface_replay (source, similar);
    if (unlikely (status)) {
	cairo_surface_destroy (similar);
	return status;
    }

    status = _emit_context (surface);
    if (unlikely (status)) {
	cairo_surface_destroy (similar);
	return status;
    }

    _cairo_output_stream_printf (surface->ctx->stream,
				 "s%lu pattern\n ",
				 ((cairo_script_surface_t *) similar)->id);
    cairo_surface_destroy (similar);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_emit_script_surface_pattern (cairo_script_surface_t *surface,
			      const cairo_pattern_t *pattern)
{
    cairo_surface_pattern_t *surface_pattern;
    cairo_script_surface_t *source;

    surface_pattern = (cairo_surface_pattern_t *) pattern;
    source = (cairo_script_surface_t *) surface_pattern->surface;

    _cairo_output_stream_printf (surface->ctx->stream,
				 "s%lu pattern\n ", source->id);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_write_image_surface (cairo_output_stream_t *output,
		      const cairo_image_surface_t *image)
{
    int stride, row, width;
    uint8_t row_stack[CAIRO_STACK_BUFFER_SIZE];
    uint8_t *rowdata;
    uint8_t *data;

    stride = image->stride;
    width = image->width;
    data = image->data;
#if WORDS_BIGENDIAN
    switch (image->format) {
    case CAIRO_FORMAT_A1:
	for (row = image->height; row--; ) {
	    _cairo_output_stream_write (output, data, (width+7)/8);
	    data += stride;
	}
	break;
    case CAIRO_FORMAT_A8:
	for (row = image->height; row--; ) {
	    _cairo_output_stream_write (output, data, width);
	    data += stride;
	}
	break;
    case CAIRO_FORMAT_RGB24:
	for (row = image->height; row--; ) {
	    int col;
	    rowdata = data;
	    for (col = width; col--; ) {
		_cairo_output_stream_write (output, rowdata, 3);
		rowdata+=4;
	    }
	    data += stride;
	}
	break;
    case CAIRO_FORMAT_ARGB32:
	for (row = image->height; row--; ) {
	    _cairo_output_stream_write (output, data, 4*width);
	    data += stride;
	}
	break;
    default:
	ASSERT_NOT_REACHED;
	break;
    }
#else
    if (stride > ARRAY_LENGTH (row_stack)) {
	rowdata = malloc (stride);
	if (unlikely (rowdata == NULL))
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);
    } else
	rowdata = row_stack;

    switch (image->format) {
    case CAIRO_FORMAT_A1:
	for (row = image->height; row--; ) {
	    int col;
	    for (col = 0; col < (width + 7)/8; col++)
		rowdata[col] = CAIRO_BITSWAP8 (data[col]);
	    _cairo_output_stream_write (output, rowdata, (width+7)/8);
	    data += stride;
	}
	break;
    case CAIRO_FORMAT_A8:
	for (row = image->height; row--; ) {
	    _cairo_output_stream_write (output, data, width);
	    data += stride;
	}
	break;
    case CAIRO_FORMAT_RGB24:
	for (row = image->height; row--; ) {
	    uint8_t *src = data;
	    int col;
	    for (col = 0; col < width; col++) {
		rowdata[3*col+2] = *src++;
		rowdata[3*col+1] = *src++;
		rowdata[3*col+0] = *src++;
		src++;
	    }
	    _cairo_output_stream_write (output, rowdata, 3*width);
	    data += stride;
	}
	break;
    case CAIRO_FORMAT_ARGB32:
	for (row = image->height; row--; ) {
	    uint32_t *src = (uint32_t *) data;
	    uint32_t *dst = (uint32_t *) rowdata;
	    int col;
	    for (col = 0; col < width; col++)
		dst[col] = bswap_32 (src[col]);
	    _cairo_output_stream_write (output, rowdata, 4*width);
	    data += stride;
	}
	break;
    default:
	ASSERT_NOT_REACHED;
	break;
    }
    if (rowdata != row_stack)
	free (rowdata);
#endif

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_emit_png_surface (cairo_script_surface_t *surface,
		     cairo_image_surface_t *image)
{
    cairo_output_stream_t *base85_stream;
    cairo_status_t status;
    const uint8_t *mime_data;
    unsigned int mime_data_length;

    cairo_surface_get_mime_data (&image->base, CAIRO_MIME_TYPE_PNG,
				 &mime_data, &mime_data_length);
    if (mime_data == NULL)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    _cairo_output_stream_printf (surface->ctx->stream,
				 "dict\n"
				 "  /width %d set\n"
				 "  /height %d set\n"
				 "  /format //%s set\n"
				 "  /mime-type (image/png) set\n"
				 "  /source <~",
				 image->width, image->height,
				 _format_to_string (image->format));

    base85_stream = _cairo_base85_stream_create (surface->ctx->stream);
    _cairo_output_stream_write (base85_stream, mime_data, mime_data_length);
    status = _cairo_output_stream_destroy (base85_stream);
    if (unlikely (status))
	return status;

    _cairo_output_stream_puts (surface->ctx->stream,
			       " set\n  image");
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_emit_image_surface (cairo_script_surface_t *surface,
		     cairo_image_surface_t *image)
{
    cairo_output_stream_t *base85_stream;
    cairo_output_stream_t *zlib_stream;
    cairo_status_t status, status2;
    const uint8_t *mime_data;
    unsigned int mime_data_length;

    status = _emit_png_surface (surface, image);
    if (_cairo_status_is_error (status)) {
	return status;
    } else if (status == CAIRO_INT_STATUS_UNSUPPORTED) {
	_cairo_output_stream_printf (surface->ctx->stream,
				     "dict\n"
				     "  /width %d set\n"
				     "  /height %d set\n"
				     "  /format //%s set\n"
				     "  /source <~",
				     image->width, image->height,
				     _format_to_string (image->format));

	if (image->width * image->height > 8) {
	    base85_stream = _cairo_base85_stream_create (surface->ctx->stream);
	    zlib_stream = _cairo_deflate_stream_create (base85_stream);

	    status = _write_image_surface (zlib_stream, image);

	    status2 = _cairo_output_stream_destroy (zlib_stream);
	    if (status == CAIRO_STATUS_SUCCESS)
		status = status2;
	    status2 = _cairo_output_stream_destroy (base85_stream);
	    if (status == CAIRO_STATUS_SUCCESS)
		status = status2;
	    if (unlikely (status))
		return status;

	    _cairo_output_stream_puts (surface->ctx->stream,
				       " /deflate filter set\n  image");
	} else {
	    base85_stream = _cairo_base85_stream_create (surface->ctx->stream);
	    status = _write_image_surface (base85_stream, image);
	    status2 = _cairo_output_stream_destroy (base85_stream);
	    if (status == CAIRO_STATUS_SUCCESS)
		status = status2;

	    _cairo_output_stream_puts (surface->ctx->stream,
				       " set\n  image");
	}
    }

    cairo_surface_get_mime_data (&image->base, CAIRO_MIME_TYPE_JPEG,
				 &mime_data, &mime_data_length);
    if (mime_data != NULL) {
	_cairo_output_stream_printf (surface->ctx->stream,
				     "\n (%s) <~",
				     CAIRO_MIME_TYPE_JPEG);

	base85_stream = _cairo_base85_stream_create (surface->ctx->stream);
	_cairo_output_stream_write (base85_stream, mime_data, mime_data_length);
	status = _cairo_output_stream_destroy (base85_stream);
	if (unlikely (status))
	    return status;

	_cairo_output_stream_puts (surface->ctx->stream,
				   " set-mime-data\n ");
    }

    _cairo_output_stream_puts (surface->ctx->stream,
			       " pattern\n ");

    return status;
}

static cairo_status_t
_emit_image_surface_pattern (cairo_script_surface_t *surface,
			     const cairo_pattern_t *pattern)
{
    cairo_surface_pattern_t *surface_pattern;
    cairo_surface_t *source;
    cairo_image_surface_t *image;
    void *image_extra;
    cairo_status_t status;

    surface_pattern = (cairo_surface_pattern_t *) pattern;
    source = surface_pattern->surface;

    /* XXX snapshot-cow */
    status = _cairo_surface_acquire_source_image (source, &image, &image_extra);
    if (unlikely (status))
	return status;

    status = _emit_image_surface (surface, image);

    _cairo_surface_release_source_image (source, image, image_extra);

    return status;
}

static cairo_status_t
_emit_surface_pattern (cairo_script_surface_t *surface,
		       const cairo_pattern_t *pattern)
{
    cairo_surface_pattern_t *surface_pattern;
    cairo_surface_t *source;

    surface_pattern = (cairo_surface_pattern_t *) pattern;
    source = surface_pattern->surface;

    switch ((int) source->type) {
    case CAIRO_INTERNAL_SURFACE_TYPE_META:
	return _emit_meta_surface_pattern (surface, pattern);
    case CAIRO_SURFACE_TYPE_SCRIPT:
	return _emit_script_surface_pattern (surface, pattern);
    default:
	return _emit_image_surface_pattern (surface, pattern);
    }
}

static cairo_status_t
_emit_pattern (cairo_script_surface_t *surface,
	       const cairo_pattern_t *pattern)
{
    cairo_status_t status;

    switch (pattern->type) {
    case CAIRO_PATTERN_TYPE_SOLID:
	/* solid colors do not need filter/extend/matrix */
	return _emit_solid_pattern (surface, pattern);

    case CAIRO_PATTERN_TYPE_LINEAR:
	status = _emit_linear_pattern (surface, pattern);
	break;
    case CAIRO_PATTERN_TYPE_RADIAL:
	status = _emit_radial_pattern (surface, pattern);
	break;
    case CAIRO_PATTERN_TYPE_SURFACE:
	status = _emit_surface_pattern (surface, pattern);
	break;

    default:
	ASSERT_NOT_REACHED;
	status = CAIRO_INT_STATUS_UNSUPPORTED;
    }
    if (unlikely (status))
	return status;

    if (! _cairo_matrix_is_identity (&pattern->matrix)) {
	_cairo_output_stream_printf (surface->ctx->stream,
				     " [%f %f %f %f %f %f] set-matrix\n ",
				     pattern->matrix.xx, pattern->matrix.yx,
				     pattern->matrix.xy, pattern->matrix.yy,
				     pattern->matrix.x0, pattern->matrix.y0);
    }

    _cairo_output_stream_printf (surface->ctx->stream,
				 " //%s set-extend\n "
				 " //%s set-filter\n ",
				 _extend_to_string (pattern->extend),
				 _filter_to_string (pattern->filter));

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_emit_identity (cairo_script_surface_t *surface,
		cairo_bool_t *matrix_updated)
{
    assert (_cairo_script_surface_owns_context (surface));

    if (_cairo_matrix_is_identity (&surface->cr.current_ctm))
	return CAIRO_STATUS_SUCCESS;

    _cairo_output_stream_puts (surface->ctx->stream,
			       "identity set-matrix\n");

    *matrix_updated = TRUE;
    cairo_matrix_init_identity (&surface->cr.current_ctm);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_emit_source (cairo_script_surface_t *surface,
	      cairo_operator_t op,
	      const cairo_pattern_t *source)
{
    cairo_bool_t matrix_updated = FALSE;
    cairo_status_t status;

    assert (_cairo_script_surface_owns_context (surface));

    if (op == CAIRO_OPERATOR_CLEAR) {
	/* the source is ignored, so don't change it */
	return CAIRO_STATUS_SUCCESS;
    }

    if (surface->cr.current_source == source)
	return CAIRO_STATUS_SUCCESS;

    if (_cairo_pattern_equal (surface->cr.current_source, source))
	return CAIRO_STATUS_SUCCESS;

    cairo_pattern_destroy (surface->cr.current_source);
    status = _cairo_pattern_create_copy (&surface->cr.current_source,
					 source);
    if (unlikely (status))
	return status;

    status = _emit_identity (surface, &matrix_updated);
    if (unlikely (status))
	return status;

    status = _emit_pattern (surface, source);
    if (unlikely (status))
	return status;

    _cairo_output_stream_puts (surface->ctx->stream,
			       " set-source\n");
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_path_move_to (void *closure,
	       const cairo_point_t *point)
{
    _cairo_output_stream_printf (closure,
				 " %f %f m",
				 _cairo_fixed_to_double (point->x),
				 _cairo_fixed_to_double (point->y));

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_path_line_to (void *closure,
	       const cairo_point_t *point)
{
    _cairo_output_stream_printf (closure,
				 " %f %f l",
				 _cairo_fixed_to_double (point->x),
				 _cairo_fixed_to_double (point->y));

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_path_curve_to (void *closure,
		const cairo_point_t *p1,
		const cairo_point_t *p2,
		const cairo_point_t *p3)
{
    _cairo_output_stream_printf (closure,
				 " %f %f %f %f %f %f c",
				 _cairo_fixed_to_double (p1->x),
				 _cairo_fixed_to_double (p1->y),
				 _cairo_fixed_to_double (p2->x),
				 _cairo_fixed_to_double (p2->y),
				 _cairo_fixed_to_double (p3->x),
				 _cairo_fixed_to_double (p3->y));

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_path_close (void *closure)
{
    _cairo_output_stream_printf (closure,
				 " h");

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_emit_path (cairo_script_surface_t *surface,
	    cairo_path_fixed_t *path)
{
    cairo_box_t box;
    cairo_status_t status;

    assert (_cairo_script_surface_owns_context (surface));
    assert (_cairo_matrix_is_identity (&surface->cr.current_ctm));

    if (_cairo_path_fixed_equal (&surface->cr.current_path, path))
	return CAIRO_STATUS_SUCCESS;

    _cairo_path_fixed_fini (&surface->cr.current_path);

    _cairo_output_stream_puts (surface->ctx->stream, "n");

    if (path == NULL) {
	_cairo_path_fixed_init (&surface->cr.current_path);
    } else if (_cairo_path_fixed_is_rectangle (path, &box)) {
	double x1 = _cairo_fixed_to_double (box.p1.x);
	double y1 = _cairo_fixed_to_double (box.p1.y);
	double x2 = _cairo_fixed_to_double (box.p2.x);
	double y2 = _cairo_fixed_to_double (box.p2.y);

	status = _cairo_path_fixed_init_copy (&surface->cr.current_path, path);
	if (unlikely (status))
	    return status;

	_cairo_output_stream_printf (surface->ctx->stream,
				     " %f %f %f %f rectangle",
				     x1, y1, x2 - x1, y2 - y1);
    } else {
	cairo_status_t status;

	status = _cairo_path_fixed_init_copy (&surface->cr.current_path, path);
	if (unlikely (status))
	    return status;

	status = _cairo_path_fixed_interpret (path,
					      CAIRO_DIRECTION_FORWARD,
					      _path_move_to,
					      _path_line_to,
					      _path_curve_to,
					      _path_close,
					      surface->ctx->stream);
	if (unlikely (status))
	    return status;
    }

    _cairo_output_stream_puts (surface->ctx->stream, "\n");

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_emit_matrix (cairo_script_surface_t *surface,
	      const cairo_matrix_t *ctm,
	      cairo_bool_t *matrix_updated)
{
    assert (_cairo_script_surface_owns_context (surface));

    if (memcmp (&surface->cr.current_ctm, ctm, sizeof (cairo_matrix_t)) == 0)
	return CAIRO_STATUS_SUCCESS;

    *matrix_updated = TRUE;
    surface->cr.current_ctm = *ctm;

    if (_cairo_matrix_is_identity (ctm)) {
	_cairo_output_stream_puts (surface->ctx->stream,
				   "identity set-matrix\n");
    } else {
	_cairo_output_stream_printf (surface->ctx->stream,
				   "[%f %f %f %f %f %f] set-matrix\n",
				   ctm->xx, ctm->yx,
				   ctm->xy, ctm->yy,
				   ctm->x0, ctm->y0);
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_emit_font_matrix (cairo_script_surface_t *surface,
		   const cairo_matrix_t *font_matrix)
{
    assert (_cairo_script_surface_owns_context (surface));

    if (memcmp (&surface->cr.current_font_matrix,
		font_matrix,
		sizeof (cairo_matrix_t)) == 0)
    {
	return CAIRO_STATUS_SUCCESS;
    }

    surface->cr.current_font_matrix = *font_matrix;

    if (_cairo_matrix_is_identity (font_matrix)) {
	_cairo_output_stream_puts (surface->ctx->stream,
				   "identity set-font-matrix\n");
    } else {
	_cairo_output_stream_printf (surface->ctx->stream,
				   "[%f %f %f %f %f %f] set-font-matrix\n",
				   font_matrix->xx, font_matrix->yx,
				   font_matrix->xy, font_matrix->yy,
				   font_matrix->x0, font_matrix->y0);
    }

    return CAIRO_STATUS_SUCCESS;
}

static const char *
_content_to_string (cairo_content_t content)
{
    switch (content) {
    case CAIRO_CONTENT_ALPHA: return "ALPHA";
    case CAIRO_CONTENT_COLOR: return "COLOR";
    default:
    case CAIRO_CONTENT_COLOR_ALPHA: return "COLOR_ALPHA";
    }
}

static cairo_surface_t *
_cairo_script_surface_create_similar (void	       *abstract_surface,
				      cairo_content_t	content,
				      int		width,
				      int		height)
{
    cairo_script_surface_t *surface, *other;
    cairo_script_vmcontext_t *ctx;
    cairo_status_t status;

    other = abstract_surface;
    ctx = other->ctx;

    if (other->id == (unsigned long) -1) {
	cairo_status_t status;

	status = _bitmap_next_id (&ctx->surface_id,
				  &other->id);
	if (unlikely (status))
	    return _cairo_surface_create_in_error (status);

	_cairo_output_stream_printf (ctx->stream,
				     "dict\n"
				     "  /width %f set\n"
				     "  /height %f set\n"
				     "  surface dup /s%lu exch def\n"
				     "context /c%lu exch def\n",
				     other->width,
				     other->height,
				     other->id,
				     other->id);
    }


    surface = _cairo_script_surface_create_internal (ctx, width, height);
    if (surface->base.status)
	return &surface->base;

    status = _bitmap_next_id (&ctx->surface_id,
			      &surface->id);
    if (unlikely (status)) {
	cairo_surface_destroy (&surface->base);
	return _cairo_surface_create_in_error (status);
    }

    if (ctx->current_target != NULL)
	_cairo_output_stream_printf (ctx->stream, "pop\n");

    _cairo_output_stream_printf (ctx->stream,
				 "s%lu %u %u //%s similar dup /s%lu exch def\n"
				 "context dup /c%lu exch def\n",
				 other->id, width, height,
				 _content_to_string (content),
				 surface->id,
				 surface->id);

    ctx->current_target = surface;

    return &surface->base;
}

static cairo_status_t
_vmcontext_destroy (cairo_script_vmcontext_t *ctx)
{
    cairo_status_t status;

    if (--ctx->ref)
	return _cairo_output_stream_flush (ctx->stream);

    while (ctx->fonts != NULL ){
	cairo_script_surface_font_private_t *font = ctx->fonts;
	ctx->fonts = font->next;
	_cairo_script_surface_scaled_font_fini (font->parent);
    }

    status = _cairo_output_stream_destroy (ctx->stream);

    free (ctx);

    return status;
}

static cairo_status_t
_cairo_script_surface_finish (void *abstract_surface)
{
    cairo_script_surface_t *surface = abstract_surface;
    cairo_status_t status;

    cairo_pattern_destroy (surface->cr.current_source);
    _cairo_path_fixed_fini (&surface->cr.current_path);

    if (surface->ctx->current_target == surface) {
	_cairo_output_stream_printf (surface->ctx->stream,
				     "pop\n");
	surface->ctx->current_target = NULL;
    }

    _cairo_output_stream_printf (surface->ctx->stream,
				 "/c%lu undef\n"
				 "/s%lu undef\n",
				 surface->id,
				 surface->id);

    _bitmap_release_id (&surface->ctx->surface_id, surface->id);

    status = _vmcontext_destroy (surface->ctx);

    return status;
}

static cairo_int_status_t
_cairo_script_surface_copy_page (void *abstract_surface)
{
    cairo_script_surface_t *surface = abstract_surface;
    cairo_status_t status;

    status = _emit_context (surface);
    if (unlikely (status))
	return status;

    _cairo_output_stream_puts (surface->ctx->stream, "copy-page\n");

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_script_surface_show_page (void *abstract_surface)
{
    cairo_script_surface_t *surface = abstract_surface;
    cairo_status_t status;

    status = _emit_context (surface);
    if (unlikely (status))
	return status;

    _cairo_output_stream_puts (surface->ctx->stream, "show-page\n");

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_script_surface_intersect_clip_path (void			*abstract_surface,
					   cairo_path_fixed_t	*path,
					   cairo_fill_rule_t	 fill_rule,
					   double		 tolerance,
					   cairo_antialias_t	 antialias)
{
    cairo_script_surface_t *surface = abstract_surface;
    cairo_bool_t matrix_updated = FALSE;
    cairo_status_t status;

    status = _emit_context (surface);
    if (unlikely (status))
	return status;

    if (path == NULL) {
	_cairo_output_stream_puts (surface->ctx->stream, "reset-clip\n");
	return CAIRO_STATUS_SUCCESS;
    }

    status = _emit_identity (surface, &matrix_updated);
    if (unlikely (status))
	return status;

    status = _emit_fill_rule (surface, fill_rule);
    if (unlikely (status))
	return status;

    status = _emit_tolerance (surface, tolerance, matrix_updated);
    if (unlikely (status))
	return status;

    status = _emit_antialias (surface, antialias);
    if (unlikely (status))
	return status;

    status = _emit_path (surface, path);
    if (unlikely (status))
	return status;

    _cairo_output_stream_puts (surface->ctx->stream, "clip+\n");

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_script_surface_paint (void			*abstract_surface,
			     cairo_operator_t		 op,
			     const cairo_pattern_t	*source,
			     cairo_rectangle_int_t	*extents)
{
    cairo_script_surface_t *surface = abstract_surface;
    cairo_status_t status;

    status = _emit_context (surface);
    if (unlikely (status))
	return status;

    status = _emit_operator (surface, op);
    if (unlikely (status))
	return status;

    status = _emit_source (surface, op, source);
    if (unlikely (status))
	return status;

    _cairo_output_stream_puts (surface->ctx->stream,
			       "paint\n");

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_script_surface_mask (void			*abstract_surface,
			    cairo_operator_t		 op,
			    const cairo_pattern_t	*source,
			    const cairo_pattern_t	*mask,
			    cairo_rectangle_int_t	*extents)
{
    cairo_script_surface_t *surface = abstract_surface;
    cairo_status_t status;

    status = _emit_context (surface);
    if (unlikely (status))
	return status;

    status = _emit_operator (surface, op);
    if (unlikely (status))
	return status;

    status = _emit_source (surface, op, source);
    if (unlikely (status))
	return status;

    status = _emit_pattern (surface, mask);
    if (unlikely (status))
	return status;

    _cairo_output_stream_puts (surface->ctx->stream,
			       " mask\n");

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_script_surface_stroke (void				*abstract_surface,
			      cairo_operator_t			 op,
			      const cairo_pattern_t		*source,
			      cairo_path_fixed_t		*path,
			      cairo_stroke_style_t		*style,
			      cairo_matrix_t			*ctm,
			      cairo_matrix_t			*ctm_inverse,
			      double				 tolerance,
			      cairo_antialias_t			 antialias,
			      cairo_rectangle_int_t		*extents)
{
    cairo_script_surface_t *surface = abstract_surface;
    cairo_bool_t matrix_updated = FALSE;
    cairo_status_t status;

    status = _emit_context (surface);
    if (unlikely (status))
	return status;

    status = _emit_identity (surface, &matrix_updated);
    if (unlikely (status))
	return status;

    status = _emit_path (surface, path);
    if (unlikely (status))
	return status;

    status = _emit_source (surface, op, source);
    if (unlikely (status))
	return status;

    status = _emit_matrix (surface, ctm, &matrix_updated);
    if (unlikely (status))
	return status;

    status = _emit_operator (surface, op);
    if (unlikely (status))
	return status;

    status = _emit_stroke_style (surface, style, matrix_updated);
    if (unlikely (status))
	return status;

    status = _emit_tolerance (surface, tolerance, matrix_updated);
    if (unlikely (status))
	return status;

    status = _emit_antialias (surface, antialias);
    if (unlikely (status))
	return status;

    _cairo_output_stream_puts (surface->ctx->stream, "stroke+\n");

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_script_surface_fill (void			*abstract_surface,
			    cairo_operator_t		 op,
			    const cairo_pattern_t	*source,
			    cairo_path_fixed_t		*path,
			    cairo_fill_rule_t		 fill_rule,
			    double			 tolerance,
			    cairo_antialias_t		 antialias,
			    cairo_rectangle_int_t	*extents)
{
    cairo_script_surface_t *surface = abstract_surface;
    cairo_bool_t matrix_updated = FALSE;
    cairo_status_t status;

    status = _emit_context (surface);
    if (unlikely (status))
	return status;

    status = _emit_operator (surface, op);
    if (unlikely (status))
	return status;

    status = _emit_identity (surface, &matrix_updated);
    if (unlikely (status))
	return status;

    status = _emit_source (surface, op, source);
    if (unlikely (status))
	return status;

    status = _emit_fill_rule (surface, fill_rule);
    if (unlikely (status))
	return status;

    status = _emit_tolerance (surface, tolerance, matrix_updated);
    if (unlikely (status))
	return status;

    status = _emit_antialias (surface, antialias);
    if (unlikely (status))
	return status;

    status = _emit_path (surface, path);
    if (unlikely (status))
	return status;

    _cairo_output_stream_puts (surface->ctx->stream, "fill+\n");

    return CAIRO_STATUS_SUCCESS;
}

static cairo_bool_t
_cairo_script_surface_has_show_text_glyphs (void *abstract_surface)
{
    return TRUE;
}

static const char *
_subpixel_order_to_string (cairo_subpixel_order_t subpixel_order)
{
    static const char *names[] = {
	"SUBPIXEL_ORDER_DEFAULT",	/* CAIRO_SUBPIXEL_ORDER_DEFAULT */
	"SUBPIXEL_ORDER_RGB",		/* CAIRO_SUBPIXEL_ORDER_RGB */
	"SUBPIXEL_ORDER_BGR",		/* CAIRO_SUBPIXEL_ORDER_BGR */
	"SUBPIXEL_ORDER_VRGB",		/* CAIRO_SUBPIXEL_ORDER_VRGB */
	"SUBPIXEL_ORDER_VBGR"		/* CAIRO_SUBPIXEL_ORDER_VBGR */
    };
    return names[subpixel_order];
}
static const char *
_hint_style_to_string (cairo_hint_style_t hint_style)
{
    static const char *names[] = {
	"HINT_STYLE_DEFAULT",	/* CAIRO_HINT_STYLE_DEFAULT */
	"HINT_STYLE_NONE",	/* CAIRO_HINT_STYLE_NONE */
	"HINT_STYLE_SLIGHT",	/* CAIRO_HINT_STYLE_SLIGHT */
	"HINT_STYLE_MEDIUM",	/* CAIRO_HINT_STYLE_MEDIUM */
	"HINT_STYLE_FULL"	/* CAIRO_HINT_STYLE_FULL */
    };
    return names[hint_style];
}
static const char *
_hint_metrics_to_string (cairo_hint_metrics_t hint_metrics)
{
    static const char *names[] = {
	 "HINT_METRICS_DEFAULT",	/* CAIRO_HINT_METRICS_DEFAULT */
	 "HINT_METRICS_OFF",		/* CAIRO_HINT_METRICS_OFF */
	 "HINT_METRICS_ON"		/* CAIRO_HINT_METRICS_ON */
    };
    return names[hint_metrics];
}

static cairo_status_t
_emit_font_options (cairo_script_surface_t *surface,
		    cairo_font_options_t *font_options)
{
    if (cairo_font_options_equal (&surface->cr.current_font_options,
				  font_options))
    {
	return CAIRO_STATUS_SUCCESS;
    }

    _cairo_output_stream_printf (surface->ctx->stream, "dict\n");

    if (font_options->antialias != surface->cr.current_font_options.antialias) {
	_cairo_output_stream_printf (surface->ctx->stream,
				     "  /antialias //%s set\n",
				     _antialias_to_string (font_options->antialias));
    }

    if (font_options->subpixel_order !=
	surface->cr.current_font_options.subpixel_order)
    {
	_cairo_output_stream_printf (surface->ctx->stream,
				     "  /subpixel-order //%s set\n",
				     _subpixel_order_to_string (font_options->subpixel_order));
    }

    if (font_options->hint_style !=
	surface->cr.current_font_options.hint_style)
    {
	_cairo_output_stream_printf (surface->ctx->stream,
				     "  /hint-style //%s set\n",
				     _hint_style_to_string (font_options->hint_style));
    }

    if (font_options->hint_metrics !=
	surface->cr.current_font_options.hint_metrics)
    {
	_cairo_output_stream_printf (surface->ctx->stream,
				     "  /hint-metrics //%s set\n",
				     _hint_metrics_to_string (font_options->hint_metrics));
    }

    _cairo_output_stream_printf (surface->ctx->stream,
				 "  set-font-options\n");

    surface->cr.current_font_options = *font_options;


    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_script_surface_scaled_font_fini (cairo_scaled_font_t *scaled_font)
{
    cairo_script_surface_font_private_t *font_private;

    font_private = scaled_font->surface_private;
    if (font_private != NULL) {
	_cairo_output_stream_printf (font_private->ctx->stream,
				     "/f%lu undef\n",
				     font_private->id);

	_bitmap_release_id (&font_private->ctx->font_id, font_private->id);

	if (font_private->prev != NULL)
	    font_private->prev = font_private->next;
	else
	    font_private->ctx->fonts = font_private->next;

	if (font_private->next != NULL)
	    font_private->next = font_private->prev;

	free (font_private);

	scaled_font->surface_private = NULL;
    }
}

static cairo_status_t
_emit_type42_font (cairo_script_surface_t *surface,
		   cairo_scaled_font_t *scaled_font)
{
    const cairo_scaled_font_backend_t *backend;
    cairo_script_surface_font_private_t *font_private;
    cairo_output_stream_t *base85_stream;
    cairo_output_stream_t *zlib_stream;
    cairo_status_t status, status2;
    unsigned long size;
    unsigned int load_flags;
    uint8_t *buf;

    backend = scaled_font->backend;
    if (backend->load_truetype_table == NULL)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    size = 0;
    status = backend->load_truetype_table (scaled_font, 0, 0, NULL, &size);
    if (unlikely (status))
	return status;

    buf = malloc (size);
    if (unlikely (buf == NULL))
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    status = backend->load_truetype_table (scaled_font, 0, 0, buf, NULL);
    if (unlikely (status)) {
	free (buf);
	return status;
    }

    load_flags = _cairo_ft_scaled_font_get_load_flags (scaled_font);
    _cairo_output_stream_printf (surface->ctx->stream,
				 "dict\n"
				 "  /type 42 set\n"
				 "  /size %lu set\n"
				 "  /index 0 set\n"
				 "  /flags %d set\n"
				 "  /source <~",
				 size, load_flags);

    base85_stream = _cairo_base85_stream_create (surface->ctx->stream);
    zlib_stream = _cairo_deflate_stream_create (base85_stream);

    _cairo_output_stream_write (zlib_stream, buf, size);
    free (buf);

    status2 = _cairo_output_stream_destroy (zlib_stream);
    if (status == CAIRO_STATUS_SUCCESS)
	status = status2;

    status2 = _cairo_output_stream_destroy (base85_stream);
    if (status == CAIRO_STATUS_SUCCESS)
	status = status2;

    font_private = scaled_font->surface_private;
    _cairo_output_stream_printf (surface->ctx->stream,
				 " /deflate filter set\n"
				 "  font dup /f%lu exch def set-font-face\n",
				 font_private->id);

    return status;
}

static cairo_status_t
_emit_scaled_font_init (cairo_script_surface_t *surface,
			cairo_scaled_font_t *scaled_font)
{
    cairo_script_surface_font_private_t *font_private;
    cairo_status_t status;

    font_private = malloc (sizeof (cairo_script_surface_font_private_t));
    if (unlikely (font_private == NULL))
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    font_private->ctx = surface->ctx;
    font_private->parent = scaled_font;
    font_private->subset_glyph_index = 0;
    font_private->has_sfnt = TRUE;

    font_private->next = font_private->ctx->fonts;
    font_private->prev = NULL;
    if (font_private->ctx->fonts != NULL)
	font_private->ctx->fonts->prev = font_private;
    font_private->ctx->fonts = font_private;

    status = _bitmap_next_id (&surface->ctx->font_id,
			      &font_private->id);
    if (unlikely (status)) {
	free (font_private);
	return status;
    }

    scaled_font->surface_private = font_private;
    scaled_font->surface_backend = &_cairo_script_surface_backend;

    status = _emit_context (surface);
    if (unlikely (status))
	return status;

    status = _emit_type42_font (surface, scaled_font);
    if (status != CAIRO_INT_STATUS_UNSUPPORTED)
	return status;

    font_private->has_sfnt = FALSE;
    _cairo_output_stream_printf (surface->ctx->stream,
				 "dict\n"
				 "  /type 3 set\n"
				 "  /metrics [%f %f %f %f %f] set\n"
				 "  /glyphs array set\n"
				 "  font dup /f%lu exch def set-font-face\n",
				 scaled_font->fs_extents.ascent,
				 scaled_font->fs_extents.descent,
				 scaled_font->fs_extents.height,
				 scaled_font->fs_extents.max_x_advance,
				 scaled_font->fs_extents.max_y_advance,
				 font_private->id);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_emit_scaled_font (cairo_script_surface_t *surface,
		   cairo_scaled_font_t *scaled_font)
{
    cairo_matrix_t matrix;
    cairo_font_options_t options;
    cairo_bool_t matrix_updated = FALSE;
    cairo_status_t status;
    cairo_script_surface_font_private_t *font_private;

    cairo_scaled_font_get_ctm (scaled_font, &matrix);
    status = _emit_matrix (surface, &matrix, &matrix_updated);
    if (unlikely (status))
	return status;

    if (! matrix_updated && surface->cr.current_scaled_font == scaled_font)
	return CAIRO_STATUS_SUCCESS;

    cairo_scaled_font_get_font_matrix (scaled_font, &matrix);
    status = _emit_font_matrix (surface, &matrix);
    if (unlikely (status))
	return status;

    cairo_scaled_font_get_font_options (scaled_font, &options);
    status = _emit_font_options (surface, &options);
    if (unlikely (status))
	return status;

    surface->cr.current_scaled_font = scaled_font;

    assert (scaled_font->surface_backend == NULL ||
	    scaled_font->surface_backend == &_cairo_script_surface_backend);

    font_private = scaled_font->surface_private;
    if (font_private == NULL) {
	status = _emit_scaled_font_init (surface, scaled_font);
	if (unlikely (status))
	    return status;
    } else {
	status = _emit_context (surface);
	if (unlikely (status))
	    return status;

	_cairo_output_stream_printf (surface->ctx->stream,
				     "f%lu set-font-face\n",
				     font_private->id);
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_emit_scaled_glyph_vector (cairo_script_surface_t *surface,
			   cairo_scaled_font_t *scaled_font,
			   cairo_scaled_glyph_t *scaled_glyph)
{
    cairo_script_surface_font_private_t *font_private;
    cairo_script_implicit_context_t old_cr;
    cairo_status_t status;
    unsigned long index;

    font_private = scaled_font->surface_private;
    index = ++font_private->subset_glyph_index;
    scaled_glyph->surface_private = (void *) index;

    _cairo_output_stream_printf (surface->ctx->stream,
				 "%lu dict\n"
				 "  /metrics [%f %f %f %f %f %f] set\n"
				 "  /render {\n",
				 index,
				 scaled_glyph->fs_metrics.x_bearing,
				 scaled_glyph->fs_metrics.y_bearing,
				 scaled_glyph->fs_metrics.width,
				 scaled_glyph->fs_metrics.height,
				 scaled_glyph->fs_metrics.x_advance,
				 scaled_glyph->fs_metrics.y_advance);

    if (! _cairo_matrix_is_identity (&scaled_font->scale_inverse)) {
	_cairo_output_stream_printf (surface->ctx->stream,
				     "[%f %f %f %f %f %f] transform\n",
				     scaled_font->scale_inverse.xx,
				     scaled_font->scale_inverse.yx,
				     scaled_font->scale_inverse.xy,
				     scaled_font->scale_inverse.yy,
				     scaled_font->scale_inverse.x0,
				     scaled_font->scale_inverse.y0);
    }

    old_cr = surface->cr;
    _cairo_script_implicit_context_init (&surface->cr);
    status = _cairo_meta_surface_replay (scaled_glyph->meta_surface,
					 &surface->base);
    surface->cr = old_cr;

    _cairo_output_stream_puts (surface->ctx->stream, "} set set\n");

    return status;
}

static cairo_status_t
_emit_scaled_glyph_bitmap (cairo_script_surface_t *surface,
			   cairo_scaled_font_t *scaled_font,
			   cairo_scaled_glyph_t *scaled_glyph)
{
    cairo_script_surface_font_private_t *font_private;
    cairo_status_t status;
    unsigned long index;

    font_private = scaled_font->surface_private;
    index = ++font_private->subset_glyph_index;
    scaled_glyph->surface_private = (void *) index;

    _cairo_output_stream_printf (surface->ctx->stream,
				 "%lu dict\n"
				 "  /metrics [%f %f %f %f %f %f] set\n"
				 "  /render {\n"
				 "%f %f translate\n",
				 index,
				 scaled_glyph->fs_metrics.x_bearing,
				 scaled_glyph->fs_metrics.y_bearing,
				 scaled_glyph->fs_metrics.width,
				 scaled_glyph->fs_metrics.height,
				 scaled_glyph->fs_metrics.x_advance,
				 scaled_glyph->fs_metrics.y_advance,
				 scaled_glyph->fs_metrics.x_bearing,
				 scaled_glyph->fs_metrics.y_bearing);

    status = _emit_image_surface (surface, scaled_glyph->surface);
    if (unlikely (status))
	return status;

    if (! _cairo_matrix_is_identity (&scaled_font->font_matrix)) {
	_cairo_output_stream_printf (surface->ctx->stream,
				     "  [%f %f %f %f %f %f] set-matrix\n",
				     scaled_font->font_matrix.xx,
				     scaled_font->font_matrix.yx,
				     scaled_font->font_matrix.xy,
				     scaled_font->font_matrix.yy,
				     scaled_font->font_matrix.x0,
				     scaled_font->font_matrix.y0);
    }
    _cairo_output_stream_puts (surface->ctx->stream,
				 "mask\n} set set\n");

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_emit_scaled_glyph_prologue (cairo_script_surface_t *surface,
			     cairo_scaled_font_t *scaled_font)
{
    cairo_script_surface_font_private_t *font_private;

    assert (scaled_font->surface_backend == &_cairo_script_surface_backend);

    font_private = scaled_font->surface_private;

    _cairo_output_stream_printf (surface->ctx->stream,
				 "f%lu /glyphs get\n",
				 font_private->id);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_emit_scaled_glyphs (cairo_script_surface_t *surface,
		     cairo_scaled_font_t *scaled_font,
		     cairo_glyph_t *glyphs,
		     unsigned int num_glyphs)
{
    cairo_script_surface_font_private_t *font_private;
    cairo_status_t status;
    unsigned int n;
    cairo_bool_t have_glyph_prologue = FALSE;

    if (num_glyphs == 0)
	return CAIRO_STATUS_SUCCESS;

    font_private = scaled_font->surface_private;
    if (font_private->has_sfnt)
	return CAIRO_STATUS_SUCCESS;

    _cairo_scaled_font_freeze_cache (scaled_font);
    for (n = 0; n < num_glyphs; n++) {
	cairo_scaled_glyph_t *scaled_glyph;

	status = _cairo_scaled_glyph_lookup (scaled_font,
					     glyphs[n].index,
					     CAIRO_SCALED_GLYPH_INFO_METRICS,
					     &scaled_glyph);
	if (unlikely (status))
	    break;

	if (scaled_glyph->surface_private != NULL)
	    continue;

	status = _cairo_scaled_glyph_lookup (scaled_font,
					     glyphs[n].index,
					     CAIRO_SCALED_GLYPH_INFO_META_SURFACE,
					     &scaled_glyph);
	if (_cairo_status_is_error (status))
	    break;

	if (status == CAIRO_STATUS_SUCCESS) {
	    if (! have_glyph_prologue) {
		status = _emit_scaled_glyph_prologue (surface, scaled_font);
		if (unlikely (status))
		    break;

		have_glyph_prologue = TRUE;
	    }

	    status = _emit_scaled_glyph_vector (surface,
						scaled_font,
						scaled_glyph);
	    if (unlikely (status))
		break;

	    continue;
	}

	status = _cairo_scaled_glyph_lookup (scaled_font,
					     glyphs[n].index,
					     CAIRO_SCALED_GLYPH_INFO_SURFACE,
					     &scaled_glyph);
	if (_cairo_status_is_error (status))
	    break;

	if (status == CAIRO_STATUS_SUCCESS) {
	    if (! have_glyph_prologue) {
		status = _emit_scaled_glyph_prologue (surface, scaled_font);
		if (unlikely (status))
		    break;

		have_glyph_prologue = TRUE;
	    }

	    status = _emit_scaled_glyph_bitmap (surface,
						scaled_font,
						scaled_glyph);
	    if (unlikely (status))
		break;

	    continue;
	}
    }
    _cairo_scaled_font_thaw_cache (scaled_font);

    if (have_glyph_prologue) {
	_cairo_output_stream_puts (surface->ctx->stream, "pop pop\n");
    }

    return status;
}

static cairo_int_status_t
_cairo_script_surface_show_text_glyphs (void			    *abstract_surface,
					cairo_operator_t	     op,
					const cairo_pattern_t	    *source,
					const char		    *utf8,
					int			     utf8_len,
					cairo_glyph_t		    *glyphs,
					int			     num_glyphs,
					const cairo_text_cluster_t  *clusters,
					int			     num_clusters,
					cairo_text_cluster_flags_t   backward,
					cairo_scaled_font_t	    *scaled_font,
					cairo_rectangle_int_t	    *extents)
{
    cairo_script_surface_t *surface = abstract_surface;
    cairo_script_surface_font_private_t *font_private;
    cairo_scaled_glyph_t *scaled_glyph;
    cairo_matrix_t matrix;
    cairo_status_t status;
    double x, y, ix, iy;
    int n;
    cairo_output_stream_t *base85_stream = NULL;

    status = _emit_context (surface);
    if (unlikely (status))
	return status;

    status = _emit_operator (surface, op);
    if (unlikely (status))
	return status;

    status = _emit_source (surface, op, source);
    if (unlikely (status))
	return status;

    status = _emit_scaled_font (surface, scaled_font);
    if (unlikely (status))
	return status;

    status = _emit_scaled_glyphs (surface, scaled_font, glyphs, num_glyphs);
    if (unlikely (status))
	return status;

    /* (utf8) [cx cy [glyphs]] [clusters] backward show_text_glyphs */
    /* [cx cy [glyphs]] show_glyphs */

    if (utf8 != NULL && clusters != NULL) {
	_cairo_output_stream_printf (surface->ctx->stream,
				     "(%s) ",
				     utf8);
    }

    matrix = surface->cr.current_ctm;
    status = cairo_matrix_invert (&matrix);
    assert (status == CAIRO_STATUS_SUCCESS);

    ix = x = glyphs[0].x;
    iy = y = glyphs[0].y;
    cairo_matrix_transform_point (&matrix, &ix, &iy);
    ix -= scaled_font->font_matrix.x0;
    iy -= scaled_font->font_matrix.y0;

    _cairo_scaled_font_freeze_cache (scaled_font);
    font_private = scaled_font->surface_private;

    _cairo_output_stream_printf (surface->ctx->stream,
				 "[%f %f ",
				 ix, iy);

    for (n = 0; n < num_glyphs; n++) {
	if (font_private->has_sfnt) {
	    if (glyphs[n].index > 256)
		break;
	} else {
	    status = _cairo_scaled_glyph_lookup (scaled_font,
						 glyphs[n].index,
						 CAIRO_SCALED_GLYPH_INFO_METRICS,
						 &scaled_glyph);
	    if (unlikely (status)) {
		_cairo_scaled_font_thaw_cache (scaled_font);
		return status;
	    }
	}
    }

    if (n == num_glyphs) {
	_cairo_output_stream_puts (surface->ctx->stream, "<~");
	base85_stream = _cairo_base85_stream_create (surface->ctx->stream);
    } else
	_cairo_output_stream_puts (surface->ctx->stream, "[");

    for (n = 0; n < num_glyphs; n++) {
	double dx, dy;

	status = _cairo_scaled_glyph_lookup (scaled_font,
					     glyphs[n].index,
					     CAIRO_SCALED_GLYPH_INFO_METRICS,
					     &scaled_glyph);
	if (unlikely (status))
	    break;

	if (fabs (glyphs[n].x - x) > 1e-5 || fabs (glyphs[n].y - y) > 1e-5) {
	    ix = x = glyphs[n].x;
	    iy = y = glyphs[n].y;
	    cairo_matrix_transform_point (&matrix, &ix, &iy);
	    ix -= scaled_font->font_matrix.x0;
	    iy -= scaled_font->font_matrix.y0;
	    if (base85_stream != NULL) {
		status = _cairo_output_stream_destroy (base85_stream);
		if (unlikely (status)) {
		    base85_stream = NULL;
		    break;
		}

		_cairo_output_stream_printf (surface->ctx->stream,
					     " %f %f <~",
					     ix, iy);
		base85_stream = _cairo_base85_stream_create (surface->ctx->stream);
	    } else {
		_cairo_output_stream_printf (surface->ctx->stream,
					     " ] %f %f [ ",
					     ix, iy);
	    }
	}
	if (base85_stream != NULL) {
	    uint8_t c;

	    if (font_private->has_sfnt)
		c = glyphs[n].index;
	    else
		c = (uint8_t) (long unsigned) scaled_glyph->surface_private;

	    _cairo_output_stream_write (base85_stream, &c, 1);
	} else {
	    if (font_private->has_sfnt)
		_cairo_output_stream_printf (surface->ctx->stream, " %lu",
					     glyphs[n].index);
	    else
		_cairo_output_stream_printf (surface->ctx->stream, " %lu",
					     (long unsigned) scaled_glyph->surface_private);
	}

        dx = scaled_glyph->metrics.x_advance;
        dy = scaled_glyph->metrics.y_advance;
	cairo_matrix_transform_distance (&scaled_font->ctm, &dx, &dy);
	x += dx;
	y += dy;
    }
    _cairo_scaled_font_thaw_cache (scaled_font);

    if (base85_stream != NULL) {
	cairo_status_t status2;

	status2 = _cairo_output_stream_destroy (base85_stream);
	if (status == CAIRO_STATUS_SUCCESS)
	    status = status2;
    } else {
	_cairo_output_stream_puts (surface->ctx->stream, " ]");
    }
    if (unlikely (status))
	return status;

    if (utf8 != NULL && clusters != NULL) {
	for (n = 0; n < num_clusters; n++) {
	    if (clusters[n].num_bytes > UCHAR_MAX ||
		clusters[n].num_glyphs > UCHAR_MAX)
	    {
		break;
	    }
	}

	if (n < num_clusters) {
	    _cairo_output_stream_puts (surface->ctx->stream, "] [ ");
	    for (n = 0; n < num_clusters; n++) {
		_cairo_output_stream_printf (surface->ctx->stream,
					     "%d %d ",
					     clusters[n].num_bytes,
					     clusters[n].num_glyphs);
	    }
	    _cairo_output_stream_puts (surface->ctx->stream, "]");
	}
	else
	{
	    _cairo_output_stream_puts (surface->ctx->stream, "] <~");
	    base85_stream = _cairo_base85_stream_create (surface->ctx->stream);
	    for (n = 0; n < num_clusters; n++) {
		uint8_t c[2];
		c[0] = clusters[n].num_bytes;
		c[1] = clusters[n].num_glyphs;
		_cairo_output_stream_write (base85_stream, c, 2);
	    }
	    status = _cairo_output_stream_destroy (base85_stream);
	    if (unlikely (status))
		return status;
	}

	_cairo_output_stream_printf (surface->ctx->stream,
				     " //%s show-text-glyphs\n",
				     _direction_to_string (backward));
    } else {
	_cairo_output_stream_puts (surface->ctx->stream,
				   "] show-glyphs\n");
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_script_surface_get_extents (void *abstract_surface,
				   cairo_rectangle_int_t *rectangle)
{
    cairo_script_surface_t *surface = abstract_surface;

    if (surface->width < 0 || surface->height < 0)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    rectangle->x = 0;
    rectangle->y = 0;
    rectangle->width = surface->width;
    rectangle->height = surface->height;

    return CAIRO_STATUS_SUCCESS;
}

static const cairo_surface_backend_t
_cairo_script_surface_backend = {
    CAIRO_SURFACE_TYPE_SCRIPT,
    _cairo_script_surface_create_similar,
    _cairo_script_surface_finish,
    NULL, //_cairo_script_surface_acquire_source_image,
    NULL, //_cairo_script_surface_release_source_image,
    NULL, /* acquire_dest_image */
    NULL, /* release_dest_image */
    NULL, /* clone_similar */
    NULL, /* composite */
    NULL, /* fill_rectangles */
    NULL, /* composite_trapezoids */
    NULL, /* create_span_renderer */
    NULL, /* check_span_renderer */
    _cairo_script_surface_copy_page,
    _cairo_script_surface_show_page,
    NULL, /* set_clip_region */
    _cairo_script_surface_intersect_clip_path,
    _cairo_script_surface_get_extents,
    NULL, /* old_show_glyphs */
    NULL, /* get_font_options */
    NULL, /* flush */
    NULL, /* mark_dirty_rectangle */
    _cairo_script_surface_scaled_font_fini,
    NULL, /* scaled_glyph_fini */

    /* The 5 high level operations */
    _cairo_script_surface_paint,
    _cairo_script_surface_mask,
    _cairo_script_surface_stroke,
    _cairo_script_surface_fill,
    NULL,

    NULL, //_cairo_script_surface_snapshot,

    NULL, /* is_similar */
    NULL, /* reset */
    NULL, /* fill_stroke */
    NULL, /* create_solid_pattern_surface */
    NULL, /* can_repaint_solid_pattern_surface */

    /* The alternate high-level text operation */
    _cairo_script_surface_has_show_text_glyphs,
    _cairo_script_surface_show_text_glyphs
};

static cairo_bool_t
_cairo_surface_is_script (cairo_surface_t *surface)
{
    return surface->backend == &_cairo_script_surface_backend;
}

static cairo_script_vmcontext_t *
_cairo_script_vmcontext_create (cairo_output_stream_t *stream)
{
    cairo_script_vmcontext_t *ctx;

    ctx = malloc (sizeof (cairo_script_vmcontext_t));
    if (unlikely (ctx == NULL))
	return NULL;

    memset (ctx, 0, sizeof (cairo_script_vmcontext_t));

    ctx->stream = stream;
    ctx->mode = CAIRO_SCRIPT_MODE_ASCII;

    return ctx;
}

static void
_cairo_script_implicit_context_init (cairo_script_implicit_context_t *cr)
{
    cr->current_operator = CAIRO_GSTATE_OPERATOR_DEFAULT;
    cr->current_fill_rule = CAIRO_GSTATE_FILL_RULE_DEFAULT;
    cr->current_tolerance = CAIRO_GSTATE_TOLERANCE_DEFAULT;
    cr->current_antialias = CAIRO_ANTIALIAS_DEFAULT;
    _cairo_stroke_style_init (&cr->current_style);
    cr->current_source = (cairo_pattern_t *) &_cairo_pattern_black.base;
    _cairo_path_fixed_init (&cr->current_path);
    cairo_matrix_init_identity (&cr->current_ctm);
    cairo_matrix_init_identity (&cr->current_font_matrix);
    _cairo_font_options_init_default (&cr->current_font_options);
    cr->current_scaled_font = NULL;
}

static cairo_script_surface_t *
_cairo_script_surface_create_internal (cairo_script_vmcontext_t *ctx,
				       double width,
				       double height)
{
    cairo_script_surface_t *surface;

    if (unlikely (ctx == NULL))
	return (cairo_script_surface_t *) _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    surface = malloc (sizeof (cairo_script_surface_t));
    if (unlikely (surface == NULL))
	return (cairo_script_surface_t *) _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    _cairo_surface_init (&surface->base,
			 &_cairo_script_surface_backend,
			 CAIRO_CONTENT_COLOR_ALPHA);

    surface->ctx = ctx;
    ctx->ref++;

    surface->width = width;
    surface->height = height;

    surface->id = (unsigned long) -1;

    _cairo_script_implicit_context_init (&surface->cr);

    return surface;
}

cairo_surface_t *
cairo_script_surface_create (const char		*filename,
			     double width,
			     double height)
{
    cairo_output_stream_t *stream;
    cairo_script_surface_t *surface;

    stream = _cairo_output_stream_create_for_filename (filename);
    if (_cairo_output_stream_get_status (stream))
	return _cairo_surface_create_in_error (_cairo_output_stream_destroy (stream));


    surface = _cairo_script_surface_create_internal
	(_cairo_script_vmcontext_create (stream), width, height);
    if (surface->base.status)
	return &surface->base;

    _cairo_output_stream_puts (surface->ctx->stream, "%!CairoScript\n");
    return &surface->base;
}

cairo_surface_t *
cairo_script_surface_create_for_stream (cairo_write_func_t	 write_func,
					void			*closure,
					double width,
					double height)
{
    cairo_output_stream_t *stream;

    stream = _cairo_output_stream_create (write_func, NULL, closure);
    if (_cairo_output_stream_get_status (stream))
	return _cairo_surface_create_in_error (_cairo_output_stream_destroy (stream));

    return &_cairo_script_surface_create_internal
	(_cairo_script_vmcontext_create (stream), width, height)->base;
}

void
cairo_script_surface_set_mode (cairo_surface_t *abstract_surface,
			       cairo_script_mode_t mode)
{
    cairo_script_surface_t *surface;
    cairo_status_t status_ignored;

    if (! _cairo_surface_is_script (abstract_surface)) {
	status_ignored = _cairo_surface_set_error (abstract_surface,
				  CAIRO_STATUS_SURFACE_TYPE_MISMATCH);
	return;
    }

    if (abstract_surface->status)
	return;

    surface = (cairo_script_surface_t *) abstract_surface;
    surface->ctx->mode = mode;
}

cairo_script_mode_t
cairo_script_surface_get_mode (cairo_surface_t *abstract_surface)
{
    cairo_script_surface_t *surface;

    if (! _cairo_surface_is_script (abstract_surface) ||
	abstract_surface->status)
    {
	return CAIRO_SCRIPT_MODE_ASCII;
    }

    surface = (cairo_script_surface_t *) abstract_surface;
    return surface->ctx->mode;
}
