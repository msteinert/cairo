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
 * The Original Code is the cairo graphics library.
 *
 * The Initial Developer of the Original Code is Chris Wilson.
 */

#include "cairoint.h"

#include "cairo-drm-private.h"

#define LIBUDEV_I_KNOW_THE_API_IS_SUBJECT_TO_CHANGE
#include <libudev.h>
#include <fcntl.h>
#include <unistd.h> /* open(), close() */

static cairo_drm_device_t *_cairo_drm_known_devices;
static cairo_drm_device_t *_cairo_drm_default_device;

static const cairo_drm_device_t _nil_device = {
    CAIRO_REFERENCE_COUNT_INVALID,
    CAIRO_STATUS_NO_MEMORY
};

static const cairo_drm_device_t _invalid_device = {
    CAIRO_REFERENCE_COUNT_INVALID,
    CAIRO_STATUS_INVALID_CONTENT
};

cairo_drm_device_t *
_cairo_drm_device_create_in_error (cairo_status_t status)
{
    switch ((int) status) {
    default:
	ASSERT_NOT_REACHED;
    case CAIRO_STATUS_NO_MEMORY:
	return (cairo_drm_device_t *) &_nil_device;
    case CAIRO_STATUS_INVALID_CONTENT:
	return (cairo_drm_device_t *) &_invalid_device;
    }
}

static const char *
get_udev_property(struct udev_device *device, const char *name)
{
    struct udev_list_entry *entry;

    udev_list_entry_foreach (entry,
	                     udev_device_get_properties_list_entry (device))
    {
	if (strcmp (udev_list_entry_get_name (entry), name) == 0)
	    return udev_list_entry_get_value (entry);
    }

    return NULL;
}

cairo_drm_device_t *
_cairo_drm_device_init (cairo_drm_device_t *dev,
			int fd,
			dev_t devid,
			int max_surface_size)
{
    CAIRO_REFERENCE_COUNT_INIT (&dev->ref_count, 1);
    dev->status = CAIRO_STATUS_SUCCESS;

    dev->id = devid;
    dev->fd = fd;

    dev->max_surface_size = max_surface_size;

    dev->prev = NULL;
    dev->next = _cairo_drm_known_devices;
    if (_cairo_drm_known_devices != NULL)
	_cairo_drm_known_devices->prev = dev;
    _cairo_drm_known_devices = dev;

    if (_cairo_drm_default_device == NULL)
	_cairo_drm_default_device = cairo_drm_device_reference (dev);

    return dev;
}

cairo_drm_device_t *
cairo_drm_device_get (struct udev_device *device)
{
    static const struct dri_driver_entry {
	uint32_t vendor_id;
	uint32_t chip_id;
	cairo_drm_device_create_func_t create_func;
    } driver_map[] = {
	{ 0x8086, ~0, _cairo_drm_intel_device_create },
	{ 0x1002, ~0, _cairo_drm_radeon_device_create },
#if CAIRO_HAS_GALLIUM_SURFACE
	{ ~0, ~0, _cairo_drm_gallium_device_create },
#endif
    };

    cairo_drm_device_t *dev;
    dev_t devid;
    struct udev_device *parent;
    const char *pci_id;
    uint32_t vendor_id, chip_id;
    const char *path;
    int i, fd;

    devid = udev_device_get_devnum (device);

    CAIRO_MUTEX_LOCK (_cairo_drm_device_mutex);
    for (dev = _cairo_drm_known_devices; dev != NULL; dev = dev->next) {
	if (dev->id == devid) {
	    dev = cairo_drm_device_reference (dev);
	    goto DONE;
	}
    }

    dev = (cairo_drm_device_t *) &_nil_device;
    parent = udev_device_get_parent (device);
    pci_id = get_udev_property (parent, "PCI_ID");
    if (sscanf (pci_id, "%x:%x", &vendor_id, &chip_id) != 2) {
	_cairo_error_throw (CAIRO_STATUS_NO_MEMORY);
	goto DONE;
    }

#if CAIRO_HAS_GALLIUM_SURFACE
    if (getenv ("CAIRO_GALLIUM_FORCE"))
    {
	i = ARRAY_LENGTH (driver_map) - 1;
    }
    else
#endif
    {
	for (i = 0; i < ARRAY_LENGTH (driver_map); i++) {
	    if (driver_map[i].vendor_id == ~0U)
		break;

	    if (driver_map[i].vendor_id == vendor_id &&
		(driver_map[i].chip_id == ~0U || driver_map[i].chip_id == chip_id))
		break;
	}

	if (i == ARRAY_LENGTH (driver_map)) {
	    /* XXX should be no driver or something*/
	    _cairo_error_throw (CAIRO_STATUS_NO_MEMORY);
	    goto DONE;
	}
    }

    path = udev_device_get_devnode (device);
    if (path == NULL)
	path = "/dev/dri/card0"; /* XXX buggy udev? */

    fd = open (path, O_RDWR);
    if (fd == -1) {
	/* XXX more likely to be a permissions issue... */
	_cairo_error_throw (CAIRO_STATUS_FILE_NOT_FOUND);
	goto DONE;
    }

    dev = driver_map[i].create_func (fd, devid, vendor_id, chip_id);
    if (dev == NULL)
	close (fd);

  DONE:
    CAIRO_MUTEX_UNLOCK (_cairo_drm_device_mutex);

    return dev;
}
slim_hidden_def (cairo_drm_device_get);

cairo_drm_device_t *
cairo_drm_device_get_for_fd (int fd)
{
    struct stat st;
    struct udev *udev;
    struct udev_device *device;
    cairo_drm_device_t *dev = NULL;

    if (fstat (fd, &st) < 0 || ! S_ISCHR (st.st_mode)) {
	//_cairo_error_throw (CAIRO_STATUS_INVALID_DEVICE);
	_cairo_error_throw (CAIRO_STATUS_NO_MEMORY);
	return (cairo_drm_device_t *) &_nil_device;
    }

    udev = udev_new ();

    device = udev_device_new_from_devnum (udev, 'c', st.st_rdev);
    if (device != NULL) {
	dev = cairo_drm_device_get (device);
	udev_device_unref (device);
    }

    udev_unref (udev);

    return dev;
}

cairo_drm_device_t *
cairo_drm_device_default (void)
{
    struct udev *udev;
    struct udev_enumerate *e;
    struct udev_list_entry *entry;
    cairo_drm_device_t *dev;

    /* optimistic atomic pointer read */
    dev = _cairo_drm_default_device;
    if (dev != NULL)
	return dev;

    udev = udev_new();
    if (udev == NULL) {
	_cairo_error_throw (CAIRO_STATUS_NO_MEMORY);
	return (cairo_drm_device_t *) &_nil_device;
    }

    e = udev_enumerate_new (udev);
    udev_enumerate_add_match_subsystem (e, "drm");
    udev_enumerate_scan_devices (e);
    udev_list_entry_foreach (entry, udev_enumerate_get_list_entry (e)) {
	struct udev_device *device;

	device =
	    udev_device_new_from_syspath (udev,
		    udev_list_entry_get_name (entry));

	dev = cairo_drm_device_get (device);

	udev_device_unref (device);

	if (dev != NULL) {
	    if (dev->fd == -1) { /* try again, we may find a usable card */
		cairo_drm_device_destroy (dev);
		dev = NULL;
	    } else
		break;
	}
    }
    udev_enumerate_unref (e);
    udev_unref (udev);

    cairo_drm_device_destroy (dev); /* owned by _cairo_drm_default_device */
    return dev;
}
slim_hidden_def (cairo_drm_device_default);

void
_cairo_drm_device_reset_static_data (void)
{
    if (_cairo_drm_default_device != NULL) {
	cairo_drm_device_destroy (_cairo_drm_default_device);
	_cairo_drm_default_device = NULL;
    }
}

cairo_drm_device_t *
cairo_drm_device_reference (cairo_drm_device_t *device)
{
    if (device == NULL ||
	CAIRO_REFERENCE_COUNT_IS_INVALID (&device->ref_count))
    {
	return device;
    }

    assert (CAIRO_REFERENCE_COUNT_HAS_REFERENCE (&device->ref_count));
    _cairo_reference_count_inc (&device->ref_count);

    return device;
}
slim_hidden_def (cairo_drm_device_reference);

int
cairo_drm_device_get_fd (cairo_drm_device_t *device)
{
    if (device->status)
	return -1;

    return device->fd;
}

cairo_status_t
cairo_drm_device_status (cairo_drm_device_t *device)
{
    if (device == NULL)
	return CAIRO_STATUS_NULL_POINTER;

    return device->status;
}

void
_cairo_drm_device_fini (cairo_drm_device_t *device)
{
    CAIRO_MUTEX_LOCK (_cairo_drm_device_mutex);
    if (device->prev != NULL)
	device->prev->next = device->next;
    else
	_cairo_drm_known_devices = device->next;
    if (device->next != NULL)
	device->next->prev = device->prev;
    CAIRO_MUTEX_UNLOCK (_cairo_drm_device_mutex);

    if (device->fd != -1)
	close (device->fd);
}

void
cairo_drm_device_destroy (cairo_drm_device_t *device)
{
    if (device == NULL ||
	CAIRO_REFERENCE_COUNT_IS_INVALID (&device->ref_count))
    {
	return;
    }

    assert (CAIRO_REFERENCE_COUNT_HAS_REFERENCE (&device->ref_count));
    if (! _cairo_reference_count_dec_and_test (&device->ref_count))
	return;

    device->device.destroy (device);
}
slim_hidden_def (cairo_drm_device_destroy);

void
cairo_drm_device_throttle (cairo_drm_device_t *dev)
{
    cairo_status_t status;

    if (unlikely (dev->status))
	return;

    if (dev->device.throttle == NULL)
	return;

    status = dev->device.throttle (dev);
    if (unlikely (status))
	_cairo_status_set_error (&dev->status, status);
}
