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

#include "cairoint.h"

#include "cairo-drm-private.h"
#include "cairo-drm-ioctl-private.h"
#include "cairo-drm-intel-private.h"
#include "cairo-freelist-private.h"

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <errno.h>

#define DRM_I915_GEM_EXECBUFFER	0x14
#define DRM_I915_GEM_BUSY	0x17
#define DRM_I915_GEM_THROTTLE	0x18
#define DRM_I915_GEM_CREATE	0x1b
#define DRM_I915_GEM_PREAD	0x1c
#define DRM_I915_GEM_PWRITE	0x1d
#define DRM_I915_GEM_MMAP	0x1e
#define DRM_I915_GEM_SET_DOMAIN	0x1f
#define DRM_I915_GEM_SET_TILING	0x21
#define DRM_I915_GEM_GET_TILING	0x22
#define DRM_I915_GEM_GET_APERTURE 0x23
#define DRM_I915_GEM_MMAP_GTT	0x24

struct drm_i915_gem_create {
	/**
	 * Requested size for the object.
	 *
	 * The (page-aligned) allocated size for the object will be returned.
	 */
	uint64_t size;
	/**
	 * Returned handle for the object.
	 *
	 * Object handles are nonzero.
	 */
	uint32_t handle;
	uint32_t pad;
};

struct drm_i915_gem_pread {
	/** Handle for the object being read. */
	uint32_t handle;
	uint32_t pad;
	/** Offset into the object to read from */
	uint64_t offset;
	/** Length of data to read */
	uint64_t size;
	/**
	 * Pointer to write the data into.
	 *
	 * This is a fixed-size type for 32/64 compatibility.
	 */
	uint64_t data_ptr;
};

struct drm_i915_gem_pwrite {
	/** Handle for the object being written to. */
	uint32_t handle;
	uint32_t pad;
	/** Offset into the object to write to */
	uint64_t offset;
	/** Length of data to write */
	uint64_t size;
	/**
	 * Pointer to read the data from.
	 *
	 * This is a fixed-size type for 32/64 compatibility.
	 */
	uint64_t data_ptr;
};

struct drm_i915_gem_mmap {
	/** Handle for the object being mapped. */
	uint32_t handle;
	uint32_t pad;
	/** Offset in the object to map. */
	uint64_t offset;
	/**
	 * Length of data to map.
	 *
	 * The value will be page-aligned.
	 */
	uint64_t size;
	/**
	 * Returned pointer the data was mapped at.
	 *
	 * This is a fixed-size type for 32/64 compatibility.
	 */
	uint64_t addr_ptr;
};

struct drm_i915_gem_mmap_gtt {
	/** Handle for the object being mapped. */
	uint32_t handle;
	uint32_t pad;
	/**
	 * Fake offset to use for subsequent mmap call
	 *
	 * This is a fixed-size type for 32/64 compatibility.
	 */
	uint64_t offset;
};

struct drm_i915_gem_set_domain {
	/** Handle for the object */
	uint32_t handle;

	/** New read domains */
	uint32_t read_domains;

	/** New write domain */
	uint32_t write_domain;
};

struct drm_i915_gem_relocation_entry {
	/**
	 * Handle of the buffer being pointed to by this relocation entry.
	 *
	 * It's appealing to make this be an index into the mm_validate_entry
	 * list to refer to the buffer, but this allows the driver to create
	 * a relocation list for state buffers and not re-write it per
	 * exec using the buffer.
	 */
	uint32_t target_handle;

	/**
	 * Value to be added to the offset of the target buffer to make up
	 * the relocation entry.
	 */
	uint32_t delta;

	/** Offset in the buffer the relocation entry will be written into */
	uint64_t offset;

	/**
	 * Offset value of the target buffer that the relocation entry was last
	 * written as.
	 *
	 * If the buffer has the same offset as last time, we can skip syncing
	 * and writing the relocation.  This value is written back out by
	 * the execbuffer ioctl when the relocation is written.
	 */
	uint64_t presumed_offset;

	/**
	 * Target memory domains read by this operation.
	 */
	uint32_t read_domains;

	/**
	 * Target memory domains written by this operation.
	 *
	 * Note that only one domain may be written by the whole
	 * execbuffer operation, so that where there are conflicts,
	 * the application will get -EINVAL back.
	 */
	uint32_t write_domain;
};

struct drm_i915_gem_exec_object {
	/**
	 * User's handle for a buffer to be bound into the GTT for this
	 * operation.
	 */
	uint32_t handle;

	/** Number of relocations to be performed on this buffer */
	uint32_t relocation_count;
	/**
	 * Pointer to array of struct drm_i915_gem_relocation_entry containing
	 * the relocations to be performed in this buffer.
	 */
	uint64_t relocs_ptr;

	/** Required alignment in graphics aperture */
	uint64_t alignment;

	/**
	 * Returned value of the updated offset of the object, for future
	 * presumed_offset writes.
	 */
	uint64_t offset;
};

struct drm_i915_gem_execbuffer {
	/**
	 * List of buffers to be validated with their relocations to be
	 * performend on them.
	 *
	 * This is a pointer to an array of struct drm_i915_gem_validate_entry.
	 *
	 * These buffers must be listed in an order such that all relocations
	 * a buffer is performing refer to buffers that have already appeared
	 * in the validate list.
	 */
	uint64_t buffers_ptr;
	uint32_t buffer_count;

	/** Offset in the batchbuffer to start execution from. */
	uint32_t batch_start_offset;
	/** Bytes used in batchbuffer from batch_start_offset */
	uint32_t batch_len;
	uint32_t DR1;
	uint32_t DR4;
	uint32_t num_cliprects;
	/** This is a struct drm_clip_rect *cliprects */
	uint64_t cliprects_ptr;
};

struct drm_i915_gem_busy {
	/** Handle of the buffer to check for busy */
	uint32_t handle;

	/** Return busy status (1 if busy, 0 if idle) */
	uint32_t busy;
};

struct drm_i915_gem_set_tiling {
	/** Handle of the buffer to have its tiling state updated */
	uint32_t handle;

	/**
	 * Tiling mode for the object (I915_TILING_NONE, I915_TILING_X,
	 * I915_TILING_Y).
	 *
	 * This value is to be set on request, and will be updated by the
	 * kernel on successful return with the actual chosen tiling layout.
	 *
	 * The tiling mode may be demoted to I915_TILING_NONE when the system
	 * has bit 6 swizzling that can't be managed correctly by GEM.
	 *
	 * Buffer contents become undefined when changing tiling_mode.
	 */
	uint32_t tiling_mode;

	/**
	 * Stride in bytes for the object when in I915_TILING_X or
	 * I915_TILING_Y.
	 */
	uint32_t stride;

	/**
	 * Returned address bit 6 swizzling required for CPU access through
	 * mmap mapping.
	 */
	uint32_t swizzle_mode;
};

struct drm_i915_gem_get_tiling {
	/** Handle of the buffer to get tiling state for. */
	uint32_t handle;

	/**
	 * Current tiling mode for the object (I915_TILING_NONE, I915_TILING_X,
	 * I915_TILING_Y).
	 */
	uint32_t tiling_mode;

	/**
	 * Returned address bit 6 swizzling required for CPU access through
	 * mmap mapping.
	 */
	uint32_t swizzle_mode;
};

struct drm_i915_gem_get_aperture {
	/** Total size of the aperture used by i915_gem_execbuffer, in bytes */
	uint64_t aper_size;

	/**
	 * Available space in the aperture used by i915_gem_execbuffer, in
	 * bytes
	 */
	uint64_t aper_available_size;
};


#define DRM_IOCTL_I915_GEM_EXECBUFFER	DRM_IOW(DRM_COMMAND_BASE + DRM_I915_GEM_EXECBUFFER, struct drm_i915_gem_execbuffer)
#define DRM_IOCTL_I915_GEM_BUSY		DRM_IOWR(DRM_COMMAND_BASE + DRM_I915_GEM_BUSY, struct drm_i915_gem_busy)
#define DRM_IOCTL_I915_GEM_THROTTLE	DRM_IO ( DRM_COMMAND_BASE + DRM_I915_GEM_THROTTLE)
#define DRM_IOCTL_I915_GEM_CREATE	DRM_IOWR(DRM_COMMAND_BASE + DRM_I915_GEM_CREATE, struct drm_i915_gem_create)
#define DRM_IOCTL_I915_GEM_PREAD	DRM_IOW (DRM_COMMAND_BASE + DRM_I915_GEM_PREAD, struct drm_i915_gem_pread)
#define DRM_IOCTL_I915_GEM_PWRITE	DRM_IOW (DRM_COMMAND_BASE + DRM_I915_GEM_PWRITE, struct drm_i915_gem_pwrite)
#define DRM_IOCTL_I915_GEM_MMAP		DRM_IOWR(DRM_COMMAND_BASE + DRM_I915_GEM_MMAP, struct drm_i915_gem_mmap)
#define DRM_IOCTL_I915_GEM_MMAP_GTT	DRM_IOWR(DRM_COMMAND_BASE + DRM_I915_GEM_MMAP_GTT, struct drm_i915_gem_mmap_gtt)
#define DRM_IOCTL_I915_GEM_SET_DOMAIN	DRM_IOW (DRM_COMMAND_BASE + DRM_I915_GEM_SET_DOMAIN, struct drm_i915_gem_set_domain)
#define DRM_IOCTL_I915_GEM_SET_TILING	DRM_IOWR (DRM_COMMAND_BASE + DRM_I915_GEM_SET_TILING, struct drm_i915_gem_set_tiling)
#define DRM_IOCTL_I915_GEM_GET_TILING	DRM_IOWR (DRM_COMMAND_BASE + DRM_I915_GEM_GET_TILING, struct drm_i915_gem_get_tiling)
#define DRM_IOCTL_I915_GEM_GET_APERTURE	DRM_IOR  (DRM_COMMAND_BASE + DRM_I915_GEM_GET_APERTURE, struct drm_i915_gem_get_aperture)

/* XXX madvise */
#ifndef DRM_I915_GEM_MADVISE
#define I915_MADV_WILLNEED	0
#define I915_MADV_DONTNEED	1

struct drm_i915_gem_madvise {
	uint32_t handle;
	uint32_t madv;
	uint32_t retained;
};
#define DRM_I915_GEM_MADVISE	0x26
#define DRM_IOCTL_I915_GEM_MADVISE	DRM_IOWR (DRM_COMMAND_BASE + DRM_I915_GEM_MADVISE, struct drm_i915_gem_madvise)
#endif


cairo_bool_t
intel_info (int fd, uint64_t *gtt_size)
{
    struct drm_i915_gem_get_aperture info;
    int ret;

    ret = ioctl (fd, DRM_IOCTL_I915_GEM_GET_APERTURE, &info);
    if (ret == -1)
	return FALSE;

    if (gtt_size != NULL)
	*gtt_size = info.aper_size;

    return TRUE;
}

void
intel_bo_write (const intel_device_t *device,
		intel_bo_t *bo,
		unsigned long offset,
		unsigned long size,
		const void *data)
{
    struct drm_i915_gem_pwrite pwrite;
    int ret;

    memset (&pwrite, 0, sizeof (pwrite));
    pwrite.handle = bo->base.handle;
    pwrite.offset = offset;
    pwrite.size = size;
    pwrite.data_ptr = (uint64_t) (uintptr_t) data;
    do {
	ret = ioctl (device->base.fd, DRM_IOCTL_I915_GEM_PWRITE, &pwrite);
    } while (ret == -1 && errno == EINTR);
}

void
intel_bo_read (const intel_device_t *device,
	       intel_bo_t *bo,
	       unsigned long offset,
	       unsigned long size,
	       void *data)
{
    struct drm_i915_gem_pread pread;
    int ret;

    memset (&pread, 0, sizeof (pread));
    pread.handle = bo->base.handle;
    pread.offset = offset;
    pread.size = size;
    pread.data_ptr = (uint64_t) (uintptr_t) data;
    do {
	ret = ioctl (device->base.fd, DRM_IOCTL_I915_GEM_PREAD, &pread);
    } while (ret == -1 && errno == EINTR);
}

void *
intel_bo_map (const intel_device_t *device, intel_bo_t *bo)
{
    struct drm_i915_gem_set_domain set_domain;
    int ret;
    uint32_t domain;

    assert (bo->virtual == NULL);

    if (bo->tiling != I915_TILING_NONE) {
	struct drm_i915_gem_mmap_gtt mmap_arg;
	void *ptr;

	mmap_arg.handle = bo->base.handle;
	mmap_arg.offset = 0;

	/* Get the fake offset back... */
	do {
	    ret = ioctl (device->base.fd,
			 DRM_IOCTL_I915_GEM_MMAP_GTT, &mmap_arg);
	} while (ret == -1 && errno == EINTR);
	if (unlikely (ret != 0)) {
	    _cairo_error_throw (CAIRO_STATUS_NO_MEMORY);
	    return NULL;
	}

	/* and mmap it */
	ptr = mmap (0, bo->base.size, PROT_READ | PROT_WRITE,
		    MAP_SHARED, device->base.fd,
		    mmap_arg.offset);
	if (unlikely (ptr == MAP_FAILED)) {
	    _cairo_error_throw (CAIRO_STATUS_NO_MEMORY);
	    return NULL;
	}

	bo->virtual = ptr;
    } else {
	struct drm_i915_gem_mmap mmap_arg;

	mmap_arg.handle = bo->base.handle;
	mmap_arg.offset = 0;
	mmap_arg.size = bo->base.size;
	mmap_arg.addr_ptr = 0;

	do {
	    ret = ioctl (device->base.fd, DRM_IOCTL_I915_GEM_MMAP, &mmap_arg);
	} while (ret == -1 && errno == EINTR);
	if (unlikely (ret != 0)) {
	    _cairo_error_throw (CAIRO_STATUS_NO_MEMORY);
	    return NULL;
	}

	bo->virtual = (void *) (uintptr_t) mmap_arg.addr_ptr;
    }

    domain = bo->tiling == I915_TILING_NONE ?
	     I915_GEM_DOMAIN_CPU : I915_GEM_DOMAIN_GTT;
    set_domain.handle = bo->base.handle;
    set_domain.read_domains = domain;
    set_domain.write_domain = domain;

    do {
	ret = ioctl (device->base.fd,
		     DRM_IOCTL_I915_GEM_SET_DOMAIN, &set_domain);
    } while (ret == -1 && errno == EINTR);

    if (ret != 0) {
	    _cairo_error_throw (CAIRO_STATUS_NO_MEMORY);
	    return NULL;
    }

    return bo->virtual;
}

void
intel_bo_unmap (intel_bo_t *bo)
{
    munmap (bo->virtual, bo->base.size);
    bo->virtual = NULL;
}

static cairo_bool_t
intel_bo_is_inactive (const intel_device_t *device, const intel_bo_t *bo)
{
    struct drm_i915_gem_busy busy;

    /* Is this buffer busy for our intended usage pattern? */
    busy.handle = bo->base.handle;
    busy.busy = 1;
    ioctl (device->base.fd, DRM_IOCTL_I915_GEM_BUSY, &busy);

    return ! busy.busy;
}

static inline int
pot (int v)
{
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    return v;
}

static void
intel_bo_cache_remove (intel_device_t *device,
	               intel_bo_t *bo,
		       int bucket)
{
    _cairo_drm_bo_close (&device->base, &bo->base);

    cairo_list_del (&bo->cache_list);

    if (device->bo_cache[bucket].num_entries-- >
	device->bo_cache[bucket].min_entries)
    {
	device->bo_cache_size -= bo->base.size;
    }

    _cairo_freepool_free (&device->bo_pool, bo);
}

static cairo_bool_t
intel_bo_madvise (intel_device_t *device,
		  intel_bo_t *bo,
		  int advice)
{
    struct drm_i915_gem_madvise madv;

    madv.handle = bo->base.handle;
    madv.madv = advice;
    madv.retained = TRUE;
    ioctl (device->base.fd, DRM_IOCTL_I915_GEM_MADVISE, &madv);
    return madv.retained;
}

static void
intel_bo_cache_purge (intel_device_t *device)
{
    int bucket;

    for (bucket = 0; bucket < INTEL_BO_CACHE_BUCKETS; bucket++) {
	intel_bo_t *bo, *next;

	cairo_list_foreach_entry_safe (bo, next,
		                       intel_bo_t,
		                       &device->bo_cache[bucket].list,
				       cache_list)
	{
	    if (! intel_bo_madvise (device, bo, I915_MADV_DONTNEED))
		intel_bo_cache_remove (device, bo, bucket);
	}
    }
}

cairo_drm_bo_t *
intel_bo_create (intel_device_t *device,
	         uint32_t size,
	         cairo_bool_t gpu_target)
{
    intel_bo_t *bo = NULL;
    uint32_t cache_size;
    struct drm_i915_gem_create create;
    int bucket;
    int ret;

    cache_size = pot ((size + 4095) & -4096);
    bucket = ffs (cache_size / 4096) - 1;
    CAIRO_MUTEX_LOCK (device->bo_mutex);
    if (bucket < INTEL_BO_CACHE_BUCKETS) {
	size = cache_size;

	/* Our goal is to avoid clflush which occur on CPU->GPU
	 * transitions, so we want to minimise reusing CPU
	 * write buffers. However, by the time a buffer is freed
	 * it is most likely in the GPU domain anyway (readback is rare!).
	 */
  retry:
	if (gpu_target) {
	    do {
		cairo_list_foreach_entry_reverse (bo,
						  intel_bo_t,
						  &device->bo_cache[bucket].list,
						  cache_list)
		{
		    /* For a gpu target, by the time our batch fires, the
		     * GPU will have finished using this buffer. However,
		     * changing tiling may require a fence deallocation and
		     * cause serialisation...
		     */

		    if (! intel_bo_madvise (device, bo, I915_MADV_WILLNEED)) {
			intel_bo_cache_remove (device, bo, bucket);
			goto retry;
		    }

		    if (device->bo_cache[bucket].num_entries-- >
			    device->bo_cache[bucket].min_entries)
		    {
			device->bo_cache_size -= bo->base.size;
		    }
		    cairo_list_del (&bo->cache_list);
		    CAIRO_MUTEX_UNLOCK (device->bo_mutex);
		    goto DONE;
		}

		/* As it is unlikely to trigger clflush, we can use the
		 * first available buffer into which we fit.
		 */
	    } while (++bucket < INTEL_BO_CACHE_BUCKETS);
	} else {
	    if (! cairo_list_is_empty (&device->bo_cache[bucket].list)) {
		bo = cairo_list_first_entry (&device->bo_cache[bucket].list,
					     intel_bo_t, cache_list);
		if (intel_bo_is_inactive (device, bo)) {
		    if (! intel_bo_madvise (device, bo, I915_MADV_WILLNEED)) {
			intel_bo_cache_remove (device, bo, bucket);
			goto retry;
		    }

		    if (device->bo_cache[bucket].num_entries-- >
			device->bo_cache[bucket].min_entries)
		    {
			device->bo_cache_size -= bo->base.size;
		    }
		    cairo_list_del (&bo->cache_list);
		    CAIRO_MUTEX_UNLOCK (device->bo_mutex);
		    goto DONE;
		}
	    }
	}
    }

    if (device->bo_cache_size > device->bo_max_cache_size_high) {
	intel_bo_cache_purge (device);

	/* trim caches by discarding the most recent buffer in each bucket */
	while (device->bo_cache_size > device->bo_max_cache_size_low) {
	    for (bucket = INTEL_BO_CACHE_BUCKETS; bucket--; ) {
		if (device->bo_cache[bucket].num_entries >
		    device->bo_cache[bucket].min_entries)
		{
		    bo = cairo_list_last_entry (&device->bo_cache[bucket].list,
						intel_bo_t, cache_list);

		    intel_bo_cache_remove (device, bo, bucket);
		}
	    }
	}
    }

    /* no cached buffer available, allocate fresh */
    bo = _cairo_freepool_alloc (&device->bo_pool);
    CAIRO_MUTEX_UNLOCK (device->bo_mutex);
    if (unlikely (bo == NULL)) {
	_cairo_error_throw (CAIRO_STATUS_NO_MEMORY);
	return NULL;
    }

    cairo_list_init (&bo->cache_list);

    bo->base.name = 0;
    bo->base.size = size;

    bo->offset = 0;
    bo->virtual = NULL;

    bo->tiling = I915_TILING_NONE;
    bo->stride = 0;
    bo->swizzle = I915_BIT_6_SWIZZLE_NONE;

    bo->in_batch = FALSE;
    bo->read_domains = 0;
    bo->write_domain = 0;

    create.size = size;
    create.handle = 0;
    ret = ioctl (device->base.fd, DRM_IOCTL_I915_GEM_CREATE, &create);
    if (unlikely (ret != 0)) {
	_cairo_error_throw (CAIRO_STATUS_NO_MEMORY);
	free (bo);
	return NULL;
    }

    bo->base.handle = create.handle;

DONE:
    CAIRO_REFERENCE_COUNT_INIT (&bo->base.ref_count, 1);

    return &bo->base;
}

cairo_drm_bo_t *
intel_bo_create_for_name (intel_device_t *device, uint32_t name)
{
    struct drm_i915_gem_get_tiling get_tiling;
    cairo_status_t status;
    intel_bo_t *bo;
    int ret;

    CAIRO_MUTEX_LOCK (device->bo_mutex);
    bo = _cairo_freepool_alloc (&device->bo_pool);
    CAIRO_MUTEX_UNLOCK (device->bo_mutex);
    if (unlikely (bo == NULL)) {
	_cairo_error_throw (CAIRO_STATUS_NO_MEMORY);
	return NULL;
    }

    status = _cairo_drm_bo_open_for_name (&device->base, &bo->base, name);
    if (unlikely (status)) {
	_cairo_freepool_free (&device->bo_pool, bo);
	return NULL;
    }

    CAIRO_REFERENCE_COUNT_INIT (&bo->base.ref_count, 1);
    cairo_list_init (&bo->cache_list);

    bo->offset = 0;
    bo->virtual = NULL;

    bo->in_batch = FALSE;
    bo->read_domains = 0;
    bo->write_domain = 0;

    memset (&get_tiling, 0, sizeof (get_tiling));
    get_tiling.handle = bo->base.handle;

    ret = ioctl (device->base.fd, DRM_IOCTL_I915_GEM_GET_TILING, &get_tiling);
    if (unlikely (ret != 0)) {
	_cairo_error_throw (CAIRO_STATUS_NO_MEMORY);
	_cairo_drm_bo_close (&device->base, &bo->base);
	_cairo_freepool_free (&device->bo_pool, bo);
	return NULL;
    }

    bo->tiling = get_tiling.tiling_mode;
    bo->swizzle = get_tiling.swizzle_mode;
    // bo->stride = get_tiling.stride; /* XXX not available from get_tiling */

    return &bo->base;
}

void
intel_bo_release (void *_dev, void *_bo)
{
    intel_device_t *device = _dev;
    intel_bo_t *bo = _bo;
    int bucket;

    bucket = INTEL_BO_CACHE_BUCKETS;
    if (bo->base.size & -bo->base.size)
	bucket = ffs (bo->base.size / 4096) - 1;

    CAIRO_MUTEX_LOCK (device->bo_mutex);
    if (bo->base.name == 0 && bucket < INTEL_BO_CACHE_BUCKETS) {
	if (++device->bo_cache[bucket].num_entries >
	    device->bo_cache[bucket].min_entries)
	{
	    device->bo_cache_size += bo->base.size;
	}

	cairo_list_add_tail (&bo->cache_list, &device->bo_cache[bucket].list);

	intel_bo_madvise (device, bo, I915_MADV_DONTNEED);
    }
    else
    {
	_cairo_drm_bo_close (&device->base, &bo->base);
	_cairo_freepool_free (&device->bo_pool, bo);
    }
    CAIRO_MUTEX_UNLOCK (device->bo_mutex);
}

void
intel_bo_set_tiling (intel_device_t *device,
	             intel_bo_t *bo,
		     uint32_t tiling,
		     uint32_t stride)
{
    struct drm_i915_gem_set_tiling set_tiling;
    int ret;

    if (bo->tiling == tiling &&
	(tiling == I915_TILING_NONE || bo->stride == stride))
    {
	return;
    }

    assert (! bo->in_batch);

    if (bo->virtual)
	intel_bo_unmap (bo);

    set_tiling.handle = bo->base.handle;
    set_tiling.tiling_mode = tiling;
    set_tiling.stride = stride;

    ret = ioctl (device->base.fd, DRM_IOCTL_I915_GEM_SET_TILING, &set_tiling);
    if (ret == 0) {
	bo->tiling = set_tiling.tiling_mode;
	bo->swizzle = set_tiling.swizzle_mode;
	bo->stride = set_tiling.stride;
    }
}

cairo_surface_t *
intel_bo_get_image (const intel_device_t *device,
		    intel_bo_t *bo,
		    const cairo_drm_surface_t *surface)
{
    cairo_image_surface_t *image;
    uint8_t *dst;
    int size, row;

    image = (cairo_image_surface_t *)
	cairo_image_surface_create (surface->format,
				    surface->width,
				    surface->height);
    if (unlikely (image->base.status))
	return &image->base;

    if (bo->tiling == I915_TILING_NONE) {
	if (image->stride == surface->stride) {
	    size = surface->stride * surface->height;
	    intel_bo_read (device, bo, 0, size, image->data);
	} else {
	    int offset;

	    size = surface->width;
	    if (surface->format != CAIRO_FORMAT_A8)
		size *= 4;

	    offset = 0;
	    row = surface->height;
	    dst = image->data;
	    while (row--) {
		intel_bo_read (device, bo, offset, size, dst);
		offset += surface->stride;
		dst += image->stride;
	    }
	}
    } else {
	const uint8_t *src;

	src = intel_bo_map (device, bo);
	if (unlikely (src == NULL))
	    return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

	size = surface->width;
	if (surface->format != CAIRO_FORMAT_A8)
	    size *= 4;

	row = surface->height;
	dst = image->data;
	while (row--) {
	    memcpy (dst, src, size);
	    dst += image->stride;
	    src += surface->stride;
	}

	intel_bo_unmap (bo);
    }

    return &image->base;
}

static void
_intel_device_init_bo_cache (intel_device_t *device)
{
    int i;

    CAIRO_MUTEX_INIT (device->bo_mutex);
    device->bo_cache_size = 0;
    device->bo_max_cache_size_high = device->gtt_max_size / 2;
    device->bo_max_cache_size_low = device->gtt_max_size / 4;

    for (i = 0; i < INTEL_BO_CACHE_BUCKETS; i++) {
	struct _intel_bo_cache *cache = &device->bo_cache[i];

	cairo_list_init (&cache->list);

	/* 256*4k ... 4*16MiB */
	if (i <= 6)
	    cache->min_entries = 1 << (6 - i);
	else
	    cache->min_entries = 0;
	cache->num_entries = 0;
    }

    _cairo_freepool_init (&device->bo_pool, sizeof (intel_bo_t));
}

cairo_status_t
intel_device_init (intel_device_t *device, int fd)
{
    struct drm_i915_gem_get_aperture aperture;
    int ret;

    ret = ioctl (fd, DRM_IOCTL_I915_GEM_GET_APERTURE, &aperture);
    if (ret != 0)
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    device->gtt_max_size = aperture.aper_size;
    device->gtt_avail_size = aperture.aper_available_size;

    _intel_device_init_bo_cache (device);

    return CAIRO_STATUS_SUCCESS;
}

static void
_intel_bo_cache_fini (intel_device_t *device)
{
    int bucket;

    for (bucket = 0; bucket < INTEL_BO_CACHE_BUCKETS; bucket++) {
	struct _intel_bo_cache *cache = &device->bo_cache[bucket];
	intel_bo_t *bo;

	cairo_list_foreach_entry (bo, intel_bo_t, &cache->list, cache_list)
	    _cairo_drm_bo_close (&device->base, &bo->base);
    }

    _cairo_freepool_fini (&device->bo_pool);
    CAIRO_MUTEX_FINI (device->bo_mutex);
}

void
intel_device_fini (intel_device_t *device)
{
    _intel_bo_cache_fini (device);
    _cairo_drm_device_fini (&device->base);
}

void
intel_throttle (intel_device_t *device)
{
    ioctl (device->base.fd, DRM_IOCTL_I915_GEM_THROTTLE);
}
