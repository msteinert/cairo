/* Cairo - a vector graphics library with display and print output
 *
 * Copyright © 2009 Chris Wilson
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
 */

#ifndef CAIRO_DRM_INTEL_PRIVATE_H
#define CAIRO_DRM_INTEL_PRIVATE_H

#include "cairo-compiler-private.h"
#include "cairo-types-private.h"
#include "cairo-drm-private.h"
#include "cairo-list-private.h"
#include "cairo-freelist-private.h"
#include "cairo-mutex-private.h"

/** @{
 * Intel memory domains
 *
 * Most of these just align with the various caches in
 * the system and are used to flush and invalidate as
 * objects end up cached in different domains.
 */
/** CPU cache */
#define I915_GEM_DOMAIN_CPU		0x00000001
/** Render cache, used by 2D and 3D drawing */
#define I915_GEM_DOMAIN_RENDER		0x00000002
/** Sampler cache, used by texture engine */
#define I915_GEM_DOMAIN_SAMPLER		0x00000004
/** Command queue, used to load batch buffers */
#define I915_GEM_DOMAIN_COMMAND		0x00000008
/** Instruction cache, used by shader programs */
#define I915_GEM_DOMAIN_INSTRUCTION	0x00000010
/** Vertex address cache */
#define I915_GEM_DOMAIN_VERTEX		0x00000020
/** GTT domain - aperture and scanout */
#define I915_GEM_DOMAIN_GTT		0x00000040
/** @} */

#define I915_TILING_NONE	0
#define I915_TILING_X		1
#define I915_TILING_Y		2

#define I915_BIT_6_SWIZZLE_NONE		0
#define I915_BIT_6_SWIZZLE_9		1
#define I915_BIT_6_SWIZZLE_9_10		2
#define I915_BIT_6_SWIZZLE_9_11		3
#define I915_BIT_6_SWIZZLE_9_10_11	4

#define INTEL_TILING_DEFAULT I915_TILING_Y


#define INTEL_BO_CACHE_BUCKETS 12 /* cache surfaces up to 16 MiB */

typedef struct _intel_bo {
    cairo_drm_bo_t base;

    cairo_list_t cache_list;

    uint32_t offset;
    void *virtual;

    uint32_t tiling;
    uint32_t swizzle;
    uint32_t stride;

    cairo_bool_t in_batch;
    uint32_t read_domains;
    uint32_t write_domain;
} intel_bo_t;

typedef struct _intel_device {
    cairo_drm_device_t base;

    size_t gtt_max_size;
    size_t gtt_avail_size;

    cairo_mutex_t bo_mutex;
    cairo_freepool_t bo_pool;
     struct _intel_bo_cache {
	cairo_list_t list;
	uint16_t min_entries;
	uint16_t num_entries;
    } bo_cache[INTEL_BO_CACHE_BUCKETS];
    size_t bo_cache_size;
    size_t bo_max_cache_size_high;
    size_t bo_max_cache_size_low;
} intel_device_t;

cairo_private cairo_bool_t
intel_info (int fd, uint64_t *gtt_size);

cairo_private cairo_status_t
intel_device_init (intel_device_t *device, int fd);

cairo_private void
intel_device_fini (intel_device_t *dev);

cairo_private cairo_drm_bo_t *
intel_bo_create (intel_device_t *dev,
	         uint32_t size,
	         cairo_bool_t gpu_target);

cairo_private void
intel_bo_release (void *_dev, void *_bo);

cairo_private cairo_drm_bo_t *
intel_bo_create_for_name (intel_device_t *dev, uint32_t name);

cairo_private void
intel_bo_set_tiling (intel_device_t *dev,
	             intel_bo_t *bo,
		     uint32_t tiling,
		     uint32_t stride);

cairo_private void
intel_bo_write (const intel_device_t *dev,
		intel_bo_t *bo,
		unsigned long offset,
		unsigned long size,
		const void *data);

cairo_private void
intel_bo_read (const intel_device_t *dev,
	       intel_bo_t *bo,
	       unsigned long offset,
	       unsigned long size,
	       void *data);

cairo_private void
intel_bo_wait (const intel_device_t *dev, intel_bo_t *bo);

cairo_private void *
intel_bo_map (const intel_device_t *dev, intel_bo_t *bo);

cairo_private void
intel_bo_unmap (intel_bo_t *bo);

cairo_private cairo_status_t
intel_bo_init (const intel_device_t *dev,
	       intel_bo_t *bo,
	       uint32_t size,
	       uint32_t initial_domain);

cairo_private cairo_status_t
intel_bo_init_for_name (const intel_device_t *dev,
			intel_bo_t *bo,
			uint32_t size,
			uint32_t name);

cairo_private cairo_surface_t *
intel_bo_get_image (const intel_device_t *device,
		    intel_bo_t *bo,
		    const cairo_drm_surface_t *surface);

cairo_private void
intel_throttle (intel_device_t *device);

#endif /* CAIRO_DRM_INTEL_PRIVATE_H */
