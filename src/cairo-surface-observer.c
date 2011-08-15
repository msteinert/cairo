/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2011 Intel Corporation
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
 *
 * The Initial Developer of the Original Code is Intel Corporation.
 *
 * Contributor(s):
 *      Chris Wilson <chris@chris-wilson.co.uk>
 */

#include "cairoint.h"

#include "cairo-composite-rectangles-private.h"
#include "cairo-error-private.h"
#include "cairo-image-surface-private.h"
#include "cairo-pattern-private.h"
#include "cairo-output-stream-private.h"
#include "cairo-surface-observer-private.h"
#include "cairo-surface-subsurface-private.h"
#include "cairo-reference-count-private.h"

static const cairo_surface_backend_t _cairo_surface_observer_backend;

/* observation/stats */

static void init_stats (struct stat *s)
{
    s->min = HUGE_VAL;
    s->max = -HUGE_VAL;
}

static void init_extents (struct extents *e)
{
    init_stats (&e->area);
}

static void init_pattern (struct pattern *p)
{
}

static void init_path (struct path *p)
{
}

static void init_clip (struct clip *c)
{
}

static void init_paint (struct paint *p)
{
    init_extents (&p->extents);
    init_pattern (&p->source);
    init_clip (&p->clip);
}

static void init_mask (struct mask *m)
{
    init_extents (&m->extents);
    init_pattern (&m->source);
    init_pattern (&m->mask);
    init_clip (&m->clip);
}

static void init_fill (struct fill *f)
{
    init_extents (&f->extents);
    init_pattern (&f->source);
    init_path (&f->path);
    init_clip (&f->clip);
}

static void init_stroke (struct stroke *s)
{
    init_extents (&s->extents);
    init_pattern (&s->source);
    init_path (&s->path);
    init_clip (&s->clip);
}

static void init_glyphs (struct glyphs *g)
{
    init_extents (&g->extents);
    init_pattern (&g->source);
    init_clip (&g->clip);
}

static void
log_init (cairo_observation_t *log)
{
    memset (log, 0, sizeof(*log));

    init_paint (&log->paint);
    init_mask (&log->mask);
    init_fill (&log->fill);
    init_stroke (&log->stroke);
    init_glyphs (&log->glyphs);
}

static cairo_surface_t*
get_pattern_surface (const cairo_pattern_t *pattern)
{
    return ((cairo_surface_pattern_t *)pattern)->surface;
}

static void
add_pattern (struct pattern *stats,
	     const cairo_pattern_t *source,
	     const cairo_surface_t *target)
{
    int classify;

    switch (source->type) {
    case CAIRO_PATTERN_TYPE_SURFACE:
	if (get_pattern_surface (source)->type == target->type)
	    classify = 0;
	else if (get_pattern_surface (source)->type == CAIRO_SURFACE_TYPE_RECORDING)
	    classify = 1;
	else
	    classify = 2;
	break;
    default:
    case CAIRO_PATTERN_TYPE_SOLID:
	classify = 3;
	break;
    case CAIRO_PATTERN_TYPE_LINEAR:
	classify = 4;
	break;
    case CAIRO_PATTERN_TYPE_RADIAL:
	classify = 5;
	break;
    case CAIRO_PATTERN_TYPE_MESH:
	classify = 6;
	break;
    }
    stats->type[classify]++;
}

static void
add_path (struct path *stats,
	  const cairo_path_fixed_t *path,
	  cairo_bool_t is_fill)
{
    int classify;

    /* XXX improve for stroke */
    classify = -1;
    if (is_fill) {
	if (path->fill_is_empty)
	    classify = 0;
	else if (_cairo_path_fixed_fill_is_rectilinear (path))
	    classify = 1;
    } else {
	if (_cairo_path_fixed_stroke_is_rectilinear (path))
	    classify = 1;
    }
    if (classify == 1 && ! path->fill_maybe_region)
	classify = 2;
    classify = 3 + path->has_curve_to != 0;
    stats->type[classify]++;
}

static void
add_clip (struct clip *stats,
	  const cairo_clip_t *clip)
{
    int classify;

    if (clip == NULL)
	classify = 0;
    else if (_cairo_clip_is_region (clip))
	classify = 1;
    else if (clip->path == NULL)
	classify = 2;
    else
	classify = 3;

    stats->type[classify]++;

}

static void
stats_add (struct stat *s, double v)
{
    if (v < s->min)
	s->min = v;
    if (v > s->max)
	s->max = v;
    s->sum += v;
    s->sum_sq += v*v;
    s->count++;
}

static void
add_extents (struct extents *stats,
	     const cairo_composite_rectangles_t *extents)
{
    const cairo_rectangle_int_t *r = extents->is_bounded ? &extents->bounded :&extents->unbounded;
    stats_add (&stats->area, r->width * r->height);
    stats->bounded += extents->is_bounded != 0;
    stats->unbounded += extents->is_bounded == 0;
}

/* device interface */

static void
_cairo_device_observer_lock (void *_device)
{
    cairo_device_observer_t *device = (cairo_device_observer_t *) _device;
    cairo_device_acquire (device->target);
}

static void
_cairo_device_observer_unlock (void *_device)
{
    cairo_device_observer_t *device = (cairo_device_observer_t *) _device;
    cairo_device_release (device->target);
}

static cairo_status_t
_cairo_device_observer_flush (void *_device)
{
    cairo_device_observer_t *device = (cairo_device_observer_t *) _device;

    if (device->target == NULL)
	return CAIRO_STATUS_SUCCESS;

    cairo_device_flush (device->target);
    return device->target->status;
}

static void
_cairo_device_observer_finish (void *_device)
{
    cairo_device_observer_t *device = (cairo_device_observer_t *) _device;
    cairo_device_finish (device->target);
}

static void
_cairo_device_observer_destroy (void *_device)
{
    cairo_device_observer_t *device = (cairo_device_observer_t *) _device;
    cairo_device_destroy (device->target);
    free (device);
}

static const cairo_device_backend_t _cairo_device_observer_backend = {
    CAIRO_INTERNAL_DEVICE_TYPE_OBSERVER,

    _cairo_device_observer_lock,
    _cairo_device_observer_unlock,

    _cairo_device_observer_flush,
    _cairo_device_observer_finish,
    _cairo_device_observer_destroy,
};

static cairo_device_t *
_cairo_device_create_observer_internal (cairo_device_t *target)
{
    cairo_device_observer_t *device;

    device = malloc (sizeof (cairo_device_observer_t));
    if (unlikely (device == NULL))
	return _cairo_device_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    _cairo_device_init (&device->base, &_cairo_device_observer_backend);
    device->target = cairo_device_reference (target);

    log_init (&device->log);

    return &device->base;
}

/* surface interface */

static cairo_device_observer_t *
to_device (cairo_surface_observer_t *suface)
{
    return (cairo_device_observer_t *)suface->base.device;
}

static cairo_surface_t *
_cairo_surface_create_observer_internal (cairo_device_t *device,
					 cairo_surface_t *target)
{
    cairo_surface_observer_t *surface;

    surface = malloc (sizeof (cairo_surface_observer_t));
    if (unlikely (surface == NULL))
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    _cairo_surface_init (&surface->base,
			 &_cairo_surface_observer_backend, device,
			 target->content);

    log_init (&surface->log);

    surface->target = cairo_surface_reference (target);
    surface->base.type = surface->target->type;
    surface->base.is_clear = surface->target->is_clear;

    surface->log.num_surfaces++;
    to_device (surface)->log.num_surfaces++;

    return &surface->base;
}


static cairo_status_t
_cairo_surface_observer_finish (void *abstract_surface)
{
    cairo_surface_observer_t *surface = abstract_surface;

    cairo_surface_destroy (surface->target);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_surface_t *
_cairo_surface_observer_create_similar (void *abstract_other,
					cairo_content_t content,
					int width, int height)
{
    cairo_surface_observer_t *other = abstract_other;
    cairo_surface_t *target, *surface;

    target = NULL;
    if (other->target->backend->create_similar)
	target = other->target->backend->create_similar (other->target, content,
							 width, height);
    if (target == NULL)
	target = _cairo_image_surface_create_with_content (content,
							   width, height);

    surface = _cairo_surface_create_observer_internal (other->base.device,
						       target);
    cairo_surface_destroy (target);

    return surface;
}

static cairo_surface_t *
_cairo_surface_observer_create_similar_image (void *other,
					      cairo_format_t format,
					      int width, int height)
{
    cairo_surface_observer_t *surface = other;

    if (surface->target->backend->create_similar_image)
	return surface->target->backend->create_similar_image (surface->target,
							       format,
							       width, height);

    return NULL;
}

static cairo_surface_t *
_cairo_surface_observer_map_to_image (void *abstract_surface,
				      const cairo_rectangle_int_t *extents)
{
    cairo_surface_observer_t *surface = abstract_surface;

    if (surface->target->backend->map_to_image == NULL)
	return NULL;

    return surface->target->backend->map_to_image (surface->target, extents);
}

static cairo_int_status_t
_cairo_surface_observer_unmap_image (void *abstract_surface,
				     cairo_image_surface_t *image)
{
    cairo_surface_observer_t *surface = abstract_surface;

    if (surface->target->backend->unmap_image == NULL)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    return surface->target->backend->unmap_image (surface->target, image);
}

static cairo_int_status_t
_cairo_surface_observer_paint (void *abstract_surface,
			       cairo_operator_t op,
			       const cairo_pattern_t *source,
			       const cairo_clip_t *clip)
{
    cairo_surface_observer_t *surface = abstract_surface;
    cairo_device_observer_t *device = to_device (surface);
    cairo_composite_rectangles_t composite;
    cairo_rectangle_int_t extents;
    cairo_int_status_t status;

    /* XXX device locking */

    surface->log.paint.count++;
    surface->log.paint.operators[op]++;
    add_pattern (&surface->log.paint.source, source, surface->target);
    add_clip (&surface->log.paint.clip, clip);

    device->log.paint.count++;
    device->log.paint.operators[op]++;
    add_pattern (&device->log.paint.source, source, surface->target);
    add_clip (&device->log.paint.clip, clip);

    _cairo_surface_get_extents (surface->target, &extents);
    status = _cairo_composite_rectangles_init_for_paint (&composite,
							 &extents,
							 op, source,
							 clip);
    if (unlikely (status)) {
	surface->log.paint.noop++;
	device->log.paint.noop++;
	return status;
    }

    add_extents (&surface->log.paint.extents, &composite);
    add_extents (&device->log.paint.extents, &composite);
    _cairo_composite_rectangles_fini (&composite);

    /* XXX time? */
    return _cairo_surface_paint (surface->target,
				 op, source,
				 clip);
}

static cairo_int_status_t
_cairo_surface_observer_mask (void *abstract_surface,
			      cairo_operator_t op,
			      const cairo_pattern_t *source,
			      const cairo_pattern_t *mask,
			      const cairo_clip_t *clip)
{
    cairo_surface_observer_t *surface = abstract_surface;
    cairo_device_observer_t *device = to_device (surface);
    cairo_composite_rectangles_t composite;
    cairo_rectangle_int_t extents;
    cairo_int_status_t status;

    surface->log.mask.count++;
    surface->log.mask.operators[op]++;
    add_pattern (&surface->log.mask.source, source, surface->target);
    add_pattern (&surface->log.mask.mask, mask, surface->target);
    add_clip (&surface->log.mask.clip, clip);

    device->log.mask.count++;
    device->log.mask.operators[op]++;
    add_pattern (&device->log.mask.source, source, surface->target);
    add_pattern (&device->log.mask.mask, mask, surface->target);
    add_clip (&device->log.mask.clip, clip);

    _cairo_surface_get_extents (surface->target, &extents);
    status = _cairo_composite_rectangles_init_for_mask (&composite,
							&extents,
							op, source, mask,
							clip);
    if (unlikely (status)) {
	surface->log.mask.noop++;
	device->log.mask.noop++;
	return status;
    }

    add_extents (&surface->log.mask.extents, &composite);
    add_extents (&device->log.mask.extents, &composite);
    _cairo_composite_rectangles_fini (&composite);

    return _cairo_surface_mask (surface->target,
				op, source, mask,
				clip);
}

static cairo_int_status_t
_cairo_surface_observer_fill (void			*abstract_surface,
			      cairo_operator_t		op,
			      const cairo_pattern_t	*source,
			      const cairo_path_fixed_t	*path,
			      cairo_fill_rule_t		fill_rule,
			      double			 tolerance,
			      cairo_antialias_t		antialias,
			      const cairo_clip_t	*clip)
{
    cairo_surface_observer_t *surface = abstract_surface;
    cairo_device_observer_t *device = to_device (surface);
    cairo_composite_rectangles_t composite;
    cairo_rectangle_int_t extents;
    cairo_int_status_t status;

    surface->log.fill.count++;
    surface->log.fill.operators[op]++;
    surface->log.fill.fill_rule[fill_rule]++;
    surface->log.fill.antialias[antialias]++;
    add_pattern (&surface->log.fill.source, source, surface->target);
    add_path (&surface->log.fill.path, path, TRUE);
    add_clip (&surface->log.fill.clip, clip);

    device->log.fill.count++;
    device->log.fill.operators[op]++;
    device->log.fill.fill_rule[fill_rule]++;
    device->log.fill.antialias[antialias]++;
    add_pattern (&device->log.fill.source, source, surface->target);
    add_path (&device->log.fill.path, path, TRUE);
    add_clip (&device->log.fill.clip, clip);

    _cairo_surface_get_extents (surface->target, &extents);
    status = _cairo_composite_rectangles_init_for_fill (&composite,
							&extents,
							op, source, path,
							clip);
    if (unlikely (status)) {
	surface->log.fill.noop++;
	device->log.fill.noop++;
	return status;
    }

    add_extents (&surface->log.fill.extents, &composite);
    add_extents (&device->log.fill.extents, &composite);
    _cairo_composite_rectangles_fini (&composite);

    return _cairo_surface_fill (surface->target,
				op, source, path,
				fill_rule, tolerance, antialias,
				clip);
}

static cairo_int_status_t
_cairo_surface_observer_stroke (void				*abstract_surface,
				cairo_operator_t		 op,
				const cairo_pattern_t		*source,
				const cairo_path_fixed_t	*path,
				const cairo_stroke_style_t	*style,
				const cairo_matrix_t		*ctm,
				const cairo_matrix_t		*ctm_inverse,
				double				 tolerance,
				cairo_antialias_t		 antialias,
				const cairo_clip_t		*clip)
{
    cairo_surface_observer_t *surface = abstract_surface;
    cairo_device_observer_t *device = to_device (surface);
    cairo_composite_rectangles_t composite;
    cairo_rectangle_int_t extents;
    cairo_int_status_t status;

    surface->log.stroke.count++;
    surface->log.stroke.operators[op]++;
    surface->log.stroke.antialias[antialias]++;
    add_pattern (&surface->log.stroke.source, source, surface->target);
    add_path (&surface->log.stroke.path, path, FALSE);
    add_clip (&surface->log.stroke.clip, clip);

    device->log.stroke.count++;
    device->log.stroke.operators[op]++;
    device->log.stroke.antialias[antialias]++;
    add_pattern (&device->log.stroke.source, source, surface->target);
    add_path (&device->log.stroke.path, path, FALSE);
    add_clip (&device->log.stroke.clip, clip);

    _cairo_surface_get_extents (surface->target, &extents);
    status = _cairo_composite_rectangles_init_for_stroke (&composite,
							  &extents,
							  op, source,
							  path, style, ctm,
							  clip);
    if (unlikely (status)) {
	surface->log.stroke.noop++;
	device->log.stroke.noop++;
	return status;
    }

    add_extents (&surface->log.stroke.extents, &composite);
    add_extents (&device->log.stroke.extents, &composite);
    _cairo_composite_rectangles_fini (&composite);

    return _cairo_surface_stroke (surface->target,
				  op, source, path,
				  style, ctm, ctm_inverse,
				  tolerance, antialias,
				  clip);
}

static cairo_int_status_t
_cairo_surface_observer_glyphs (void			*abstract_surface,
				cairo_operator_t	 op,
				const cairo_pattern_t	*source,
				cairo_glyph_t		*glyphs,
				int			 num_glyphs,
				cairo_scaled_font_t	*scaled_font,
				const cairo_clip_t		*clip,
				int *remaining_glyphs)
{
    cairo_surface_observer_t *surface = abstract_surface;
    cairo_device_observer_t *device = to_device (surface);
    cairo_composite_rectangles_t composite;
    cairo_rectangle_int_t extents;
    cairo_int_status_t status;

    surface->log.glyphs.count++;
    surface->log.glyphs.operators[op]++;
    add_pattern (&surface->log.glyphs.source, source, surface->target);
    add_clip (&surface->log.glyphs.clip, clip);

    device->log.glyphs.count++;
    device->log.glyphs.operators[op]++;
    add_pattern (&device->log.glyphs.source, source, surface->target);
    add_clip (&device->log.glyphs.clip, clip);

    _cairo_surface_get_extents (surface->target, &extents);
    status = _cairo_composite_rectangles_init_for_glyphs (&composite,
							  &extents,
							  op, source,
							  scaled_font,
							  glyphs, num_glyphs,
							  clip,
							  NULL);
    if (unlikely (status)) {
	surface->log.glyphs.noop++;
	device->log.glyphs.noop++;
	return status;
    }

    add_extents (&surface->log.glyphs.extents, &composite);
    add_extents (&device->log.glyphs.extents, &composite);
    _cairo_composite_rectangles_fini (&composite);

    *remaining_glyphs = 0;
    return _cairo_surface_show_text_glyphs (surface->target, op, source,
					    NULL, 0,
					    glyphs, num_glyphs,
					    NULL, 0, 0,
					    scaled_font,
					    clip);
}

static cairo_status_t
_cairo_surface_observer_flush (void *abstract_surface)
{
    cairo_surface_observer_t *surface = abstract_surface;

    cairo_surface_flush (surface->target);
    return surface->target->status;
}

static cairo_status_t
_cairo_surface_observer_mark_dirty (void *abstract_surface,
				      int x, int y,
				      int width, int height)
{
    cairo_surface_observer_t *surface = abstract_surface;
    cairo_status_t status;

    printf ("mark-dirty (%d, %d) x (%d, %d)\n", x, y, width, height);

    status = CAIRO_STATUS_SUCCESS;
    if (surface->target->backend->mark_dirty_rectangle)
	status = surface->target->backend->mark_dirty_rectangle (surface->target,
						       x,y, width,height);

    return status;
}

static cairo_int_status_t
_cairo_surface_observer_copy_page (void *abstract_surface)
{
    cairo_surface_observer_t *surface = abstract_surface;
    cairo_status_t status;

    status = CAIRO_STATUS_SUCCESS;
    if (surface->target->backend->copy_page)
	status = surface->target->backend->copy_page (surface->target);

    return status;
}

static cairo_int_status_t
_cairo_surface_observer_show_page (void *abstract_surface)
{
    cairo_surface_observer_t *surface = abstract_surface;
    cairo_status_t status;

    status = CAIRO_STATUS_SUCCESS;
    if (surface->target->backend->show_page)
	status = surface->target->backend->show_page (surface->target);

    return status;
}

static cairo_bool_t
_cairo_surface_observer_get_extents (void *abstract_surface,
				     cairo_rectangle_int_t *extents)
{
    cairo_surface_observer_t *surface = abstract_surface;
    return _cairo_surface_get_extents (surface->target, extents);
}

static void
_cairo_surface_observer_get_font_options (void *abstract_surface,
					  cairo_font_options_t *options)
{
    cairo_surface_observer_t *surface = abstract_surface;

    if (surface->target->backend->get_font_options != NULL)
	surface->target->backend->get_font_options (surface->target, options);
}

static cairo_status_t
_cairo_surface_observer_acquire_source_image (void                    *abstract_surface,
						cairo_image_surface_t  **image_out,
						void                   **image_extra)
{
    cairo_surface_observer_t *surface = abstract_surface;

    surface->log.num_sources_acquired++;
    to_device (surface)->log.num_sources_acquired++;

    return _cairo_surface_acquire_source_image (surface->target,
						image_out, image_extra);
}

static void
_cairo_surface_observer_release_source_image (void                   *abstract_surface,
						cairo_image_surface_t  *image,
						void                   *image_extra)
{
    cairo_surface_observer_t *surface = abstract_surface;

    return _cairo_surface_release_source_image (surface->target, image, image_extra);
}

static cairo_surface_t *
_cairo_surface_observer_snapshot (void *abstract_surface)
{
    cairo_surface_observer_t *surface = abstract_surface;

    printf ("taking snapshot\n");

    /* XXX hook onto the snapshot so that we measure number of reads */

    if (surface->target->backend->snapshot)
	return surface->target->backend->snapshot (surface->target);

    return NULL;
}

static cairo_t *
_cairo_surface_observer_create_context(void *target)
{
    cairo_surface_observer_t *surface = target;

    if (_cairo_surface_is_subsurface (&surface->base))
	surface = (cairo_surface_observer_t *)
	    _cairo_surface_subsurface_get_target (&surface->base);

    surface->log.num_contexts++;
    to_device (surface)->log.num_contexts++;

    return surface->target->backend->create_context (target);
}

static const cairo_surface_backend_t _cairo_surface_observer_backend = {
    CAIRO_INTERNAL_SURFACE_TYPE_OBSERVER,
    _cairo_surface_observer_finish,

    _cairo_surface_observer_create_context,

    _cairo_surface_observer_create_similar,
    _cairo_surface_observer_create_similar_image,
    _cairo_surface_observer_map_to_image,
    _cairo_surface_observer_unmap_image,

    _cairo_surface_observer_acquire_source_image,
    _cairo_surface_observer_release_source_image,

    NULL, NULL, /* acquire, release dest */
    NULL, /* clone similar */
    NULL, /* composite */
    NULL, /* fill rectangles */
    NULL, /* composite trapezoids */
    NULL, /* create span renderer */
    NULL, /* check span renderer */

    _cairo_surface_observer_copy_page,
    _cairo_surface_observer_show_page,

    _cairo_surface_observer_get_extents,
    NULL, /* old_show_glyphs */
    _cairo_surface_observer_get_font_options,
    _cairo_surface_observer_flush,
    _cairo_surface_observer_mark_dirty,
    NULL, /* font_fini */
    NULL, /* glyph_fini */

    _cairo_surface_observer_paint,
    _cairo_surface_observer_mask,
    _cairo_surface_observer_stroke,
    _cairo_surface_observer_fill,
    _cairo_surface_observer_glyphs,

    _cairo_surface_observer_snapshot,
};

/**
 * cairo_surface_create_observer:
 * @target: an existing surface for which the observer will watch
 *
 * Create a new surface that exists solely to watch another is doing. In
 * the process it will log operations and times, which are fast, which are
 * slow, which are frequent, etc.
 *
 * Return value: a pointer to the newly allocated surface. The caller
 * owns the surface and should call cairo_surface_destroy() when done
 * with it.
 *
 * This function always returns a valid pointer, but it will return a
 * pointer to a "nil" surface if @other is already in an error state
 * or any other error occurs.
 *
 * Since: 1.12
 **/
cairo_surface_t *
cairo_surface_create_observer (cairo_surface_t *target)
{
    cairo_device_t *device;
    cairo_surface_t *surface;

    if (unlikely (target->status))
	return _cairo_surface_create_in_error (target->status);
    if (unlikely (target->finished))
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_SURFACE_FINISHED));

    device = _cairo_device_create_observer_internal (target->device);
    if (unlikely (device->status))
	return _cairo_surface_create_in_error (device->status);

    surface = _cairo_surface_create_observer_internal (device, target);
    cairo_device_destroy (device);

    return surface;
}

static void
print_extents (cairo_output_stream_t *stream, const struct extents *e)
{
    _cairo_output_stream_printf (stream,
				 "  extents: total %g, avg %g [unbounded %d]\n",
				 e->area.sum,
				 e->area.sum / e->area.count,
				 e->unbounded);
}

static void
print_pattern (cairo_output_stream_t *stream,
	       const char *name,
	       const struct pattern *p)
{
    _cairo_output_stream_printf (stream,
				 "  %s: %d solid, %d native, %d record, %d other surface, %d linear, %d radial, %d mesh\n",
				 name,
				 p->type[3], /* solid first */
				 p->type[0],
				 p->type[1],
				 p->type[2],
				 p->type[4],
				 p->type[5],
				 p->type[6]);
}

static void
print_path (cairo_output_stream_t *stream,
	    const struct path *p)
{
    _cairo_output_stream_printf (stream,
				 "  path: %d empty, %d pixel-aligned, %d rectilinear, %d straight, %d curved\n",
				 p->type[0],
				 p->type[1],
				 p->type[2],
				 p->type[3],
				 p->type[4]);
}

static void
print_clip (cairo_output_stream_t *stream, const struct clip *c)
{
    _cairo_output_stream_printf (stream,
				 "  clip: %d none, %d region, %d boxes, %d general path\n",
				 c->type[0],
				 c->type[1],
				 c->type[2],
				 c->type[3]);
}

static void
_cairo_observation_print (cairo_output_stream_t *stream,
				  cairo_observation_t *log)
{
    _cairo_output_stream_printf (stream, "surfaces: %d\n",
				 log->num_surfaces);
    _cairo_output_stream_printf (stream, "contexts: %d\n",
				 log->num_contexts);
    _cairo_output_stream_printf (stream, "sources acquired: %d\n",
				 log->num_sources_acquired);

    _cairo_output_stream_printf (stream, "paint: count %d [no-op %d]\n",
				 log->paint.count, log->paint.noop);
    if (log->paint.count) {
	print_extents (stream, &log->paint.extents);
	print_pattern (stream, "source", &log->paint.source);
	print_clip (stream, &log->paint.clip);
    }

    _cairo_output_stream_printf (stream, "mask: count %d [no-op %d]\n",
				 log->mask.count, log->mask.noop);
    if (log->mask.count) {
	print_extents (stream, &log->mask.extents);
	print_pattern (stream, "source", &log->mask.source);
	print_pattern (stream, "mask", &log->mask.mask);
	print_clip (stream, &log->mask.clip);
    }

    _cairo_output_stream_printf (stream, "fill: count %d [no-op %d]\n",
				 log->fill.count, log->fill.noop);
    if (log->fill.count) {
	print_extents (stream, &log->fill.extents);
	print_pattern (stream, "source", &log->fill.source);
	print_path (stream, &log->fill.path);
	print_clip (stream, &log->fill.clip);
    }

    _cairo_output_stream_printf (stream, "stroke: count %d [no-op %d]\n",
				 log->stroke.count, log->stroke.noop);
    if (log->stroke.count) {
	print_extents (stream, &log->stroke.extents);
	print_pattern (stream, "source", &log->stroke.source);
	print_path (stream, &log->stroke.path);
	print_clip (stream, &log->stroke.clip);
    }

    _cairo_output_stream_printf (stream, "glyphs: count %d [no-op %d]\n",
				 log->glyphs.count, log->glyphs.noop);
    if (log->glyphs.count) {
	print_extents (stream, &log->glyphs.extents);
	print_pattern (stream, "source", &log->glyphs.source);
	print_clip (stream, &log->glyphs.clip);
    }
}

void
cairo_surface_observer_print (cairo_surface_t *abstract_surface,
			      cairo_write_func_t write_func,
			      void *closure)
{
    cairo_output_stream_t *stream;
    cairo_surface_observer_t *surface;

    if (unlikely (CAIRO_REFERENCE_COUNT_IS_INVALID (&abstract_surface->ref_count)))
	return;

    if (! _cairo_surface_is_observer (abstract_surface))
	return;

    surface = (cairo_surface_observer_t *) abstract_surface;

    stream = _cairo_output_stream_create (write_func, NULL, closure);
    _cairo_observation_print (stream, &surface->log);
    _cairo_output_stream_destroy (stream);
}

void
cairo_device_observer_print (cairo_device_t *abstract_device,
			     cairo_write_func_t write_func,
			     void *closure)
{
    cairo_output_stream_t *stream;
    cairo_device_observer_t *device;

    if (unlikely (CAIRO_REFERENCE_COUNT_IS_INVALID (&abstract_device->ref_count)))
	return;

    if (! _cairo_device_is_observer (abstract_device))
	return;

    device = (cairo_device_observer_t *) abstract_device;

    stream = _cairo_output_stream_create (write_func, NULL, closure);
    _cairo_observation_print (stream, &device->log);
    _cairo_output_stream_destroy (stream);
}
