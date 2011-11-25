/* -*- Mode: c; tab-width: 8; c-basic-offset: 4; indent-tabs-mode: t; -*- */
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
 * The Initial Developer of the Original Code is Red Hat, Inc.
 *
 * Contributor(s):
 *	Chris Wilson <chris@chris-wilson.co.uk>
 */

#include "cairoint.h"
#include "cairo-error-private.h"
#include "cairo-pattern-private.h"

cairo_surface_t *
_cairo_raster_source_pattern_acquire (const cairo_pattern_t *abstract_pattern,
				      cairo_surface_t *target,
				      const cairo_rectangle_int_t *extents)
{
    cairo_raster_source_pattern_t *pattern =
	(cairo_raster_source_pattern_t *) abstract_pattern;

    if (pattern->acquire == NULL)
	return NULL;

    if (extents == NULL)
	extents = &pattern->extents;

    return pattern->acquire (&pattern->base, pattern->user_data,
			     target, extents);
}

void
_cairo_raster_source_pattern_release (const cairo_pattern_t *abstract_pattern,
				      cairo_surface_t *surface)
{
    cairo_raster_source_pattern_t *pattern =
	(cairo_raster_source_pattern_t *) abstract_pattern;

    if (pattern->release == NULL)
	return;

    return pattern->release (&pattern->base, pattern->user_data,
			     surface);
}

cairo_status_t
_cairo_raster_source_pattern_init_copy (cairo_pattern_t *abstract_pattern,
					const cairo_pattern_t *other)
{
    cairo_raster_source_pattern_t *pattern =
	(cairo_raster_source_pattern_t *) abstract_pattern;
    cairo_status_t status;

    VG (VALGRIND_MAKE_MEM_UNDEFINED (pattern, sizeof (cairo_raster_source_pattern_t)));
    memcpy(pattern, other, sizeof (cairo_raster_source_pattern_t));

    status = CAIRO_STATUS_SUCCESS;
    if (pattern->copy)
	status = pattern->copy (&pattern->base, pattern->user_data, other);

    return status;
}

cairo_status_t
_cairo_raster_source_pattern_snapshot (cairo_pattern_t *abstract_pattern)
{
    cairo_raster_source_pattern_t *pattern =
	(cairo_raster_source_pattern_t *) abstract_pattern;

    if (pattern->snapshot == NULL)
	return CAIRO_STATUS_SUCCESS;

    return pattern->snapshot (&pattern->base, pattern->user_data);
}

void
_cairo_raster_source_pattern_finish (cairo_pattern_t *abstract_pattern)
{
    cairo_raster_source_pattern_t *pattern =
	(cairo_raster_source_pattern_t *) abstract_pattern;

    if (pattern->finish == NULL)
	return;

    pattern->finish (&pattern->base, pattern->user_data);
}

cairo_pattern_t *
cairo_pattern_create_raster_source (void *user_data,
				    cairo_content_t content,
				    int width, int height)
{
    cairo_raster_source_pattern_t *pattern;

    CAIRO_MUTEX_INITIALIZE ();

    if (width < 0 || height < 0)
	return _cairo_pattern_create_in_error (CAIRO_STATUS_INVALID_SIZE);

    if (! CAIRO_CONTENT_VALID (content))
	return _cairo_pattern_create_in_error (CAIRO_STATUS_INVALID_CONTENT);

    pattern = calloc (1, sizeof (*pattern));
    if (unlikely (pattern == NULL))
	return _cairo_pattern_create_in_error (CAIRO_STATUS_NO_MEMORY);

    _cairo_pattern_init (&pattern->base,
			 CAIRO_PATTERN_TYPE_RASTER_SOURCE);
    CAIRO_REFERENCE_COUNT_INIT (&pattern->base.ref_count, 1);

    pattern->content = content;

    pattern->extents.x = 0;
    pattern->extents.y = 0;
    pattern->extents.width  = width;
    pattern->extents.height = height;

    pattern->user_data = user_data;

    return &pattern->base;
}

void
cairo_raster_source_pattern_set_callback_data (cairo_pattern_t *abstract_pattern,
					       void *data)
{
    cairo_raster_source_pattern_t *pattern;

    if (abstract_pattern->type != CAIRO_PATTERN_TYPE_RASTER_SOURCE)
	return;

    pattern = (cairo_raster_source_pattern_t *) abstract_pattern;
    pattern->user_data = data;
}

void *
cairo_raster_source_pattern_get_callback_data (cairo_pattern_t *abstract_pattern)
{
    cairo_raster_source_pattern_t *pattern;

    if (abstract_pattern->type != CAIRO_PATTERN_TYPE_RASTER_SOURCE)
	return NULL;

    pattern = (cairo_raster_source_pattern_t *) abstract_pattern;
    return pattern->user_data;
}

void
cairo_raster_source_pattern_set_acquire (cairo_pattern_t *abstract_pattern,
					 cairo_raster_source_acquire_func_t acquire,
					 cairo_raster_source_release_func_t release)
{
    cairo_raster_source_pattern_t *pattern;

    if (abstract_pattern->type != CAIRO_PATTERN_TYPE_RASTER_SOURCE)
	return;

    pattern = (cairo_raster_source_pattern_t *) abstract_pattern;
    pattern->acquire = acquire;
    pattern->release = release;
}

void
cairo_raster_source_pattern_get_acquire (cairo_pattern_t *abstract_pattern,
					 cairo_raster_source_acquire_func_t *acquire,
					 cairo_raster_source_release_func_t *release)
{
    cairo_raster_source_pattern_t *pattern;

    if (abstract_pattern->type != CAIRO_PATTERN_TYPE_RASTER_SOURCE)
	return;

    pattern = (cairo_raster_source_pattern_t *) abstract_pattern;
    if (acquire)
	*acquire = pattern->acquire;
    if (release)
	*release = pattern->release;
}

void
cairo_raster_source_pattern_set_snapshot (cairo_pattern_t *abstract_pattern,
					  cairo_raster_source_snapshot_func_t snapshot)
{
    cairo_raster_source_pattern_t *pattern;

    if (abstract_pattern->type != CAIRO_PATTERN_TYPE_RASTER_SOURCE)
	return;

    pattern = (cairo_raster_source_pattern_t *) abstract_pattern;
    pattern->snapshot = snapshot;
}

cairo_raster_source_snapshot_func_t
cairo_raster_source_pattern_get_snapshot (cairo_pattern_t *abstract_pattern)
{
    cairo_raster_source_pattern_t *pattern;

    if (abstract_pattern->type != CAIRO_PATTERN_TYPE_RASTER_SOURCE)
	return NULL;

    pattern = (cairo_raster_source_pattern_t *) abstract_pattern;
    return pattern->snapshot;
}

void
cairo_raster_source_pattern_set_copy (cairo_pattern_t *abstract_pattern,
				      cairo_raster_source_copy_func_t copy)
{
    cairo_raster_source_pattern_t *pattern;

    if (abstract_pattern->type != CAIRO_PATTERN_TYPE_RASTER_SOURCE)
	return;

    pattern = (cairo_raster_source_pattern_t *) abstract_pattern;
    pattern->copy = copy;
}

cairo_raster_source_copy_func_t
cairo_raster_source_pattern_get_copy (cairo_pattern_t *abstract_pattern)
{
    cairo_raster_source_pattern_t *pattern;

    if (abstract_pattern->type != CAIRO_PATTERN_TYPE_RASTER_SOURCE)
	return NULL;

    pattern = (cairo_raster_source_pattern_t *) abstract_pattern;
    return pattern->copy;
}

void
cairo_raster_source_pattern_set_finish (cairo_pattern_t *abstract_pattern,
					cairo_raster_source_finish_func_t finish)
{
    cairo_raster_source_pattern_t *pattern;

    if (abstract_pattern->type != CAIRO_PATTERN_TYPE_RASTER_SOURCE)
	return;

    pattern = (cairo_raster_source_pattern_t *) abstract_pattern;
    pattern->finish = finish;
}

cairo_raster_source_finish_func_t
cairo_raster_source_pattern_get_finish (cairo_pattern_t *abstract_pattern)
{
    cairo_raster_source_pattern_t *pattern;

    if (abstract_pattern->type != CAIRO_PATTERN_TYPE_RASTER_SOURCE)
	return NULL;

    pattern = (cairo_raster_source_pattern_t *) abstract_pattern;
    return pattern->finish;
}
