#include <cairo-gl.h>
#include "cairo-script-interpreter.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include <sys/stat.h>
#include <sys/ioctl.h>
#include <xf86drmMode.h>
#include <i915_drm.h>

#define LIBUDEV_I_KNOW_THE_API_IS_SUBJECT_TO_CHANGE
#include <libudev.h>

static void
die (const char *fmt, ...)
{
    va_list ap;

    va_start (ap, fmt);
    vfprintf (stderr, fmt, ap);
    va_end (ap);

    exit (EXIT_FAILURE);
}

static EGLDisplay
_get_display (const char *card)
{
    EGLint major, minor;
    EGLDisplay display;
    struct udev *udev;
    struct udev_device *device;
    struct stat st;

    if (stat (card, &st) < 0) {
	die ("no such device\n");
	return NULL;
    }

    udev = udev_new ();
    device = udev_device_new_from_devnum (udev, 'c', st.st_rdev);
    if (device == NULL) {
	die ("failed to find device\n");
	return NULL;
    }
    display = eglCreateDisplayNative (device);
    udev_device_unref (device);
    udev_unref (udev);

    if (display == NULL) {
	die ("failed to open display\n");
	return NULL;
    }

    if (! eglInitialize (display, &major, &minor)) {
	die ("failed to initialize display\n");
	return NULL;
    }

    return display;
}

static cairo_surface_t *
_get_fb (const char *card)
{
    const EGLint config_attribs[] = {
	EGL_DEPTH_SIZE, 24,
	EGL_STENCIL_SIZE, 8,
	EGL_CONFIG_CAVEAT, EGL_NONE,
	EGL_NONE
    };
    const EGLint surface_attribs[] = {
	EGL_RENDER_BUFFER, EGL_BACK_BUFFER,
	EGL_NONE
    };

    EGLDisplay display;
    EGLContext context;
    EGLConfig config;
    EGLSurface s;

    cairo_gl_context_t *ctx;
    cairo_surface_t *surface;

    drmModeConnector *connector;
    drmModeRes *resources;
    drmModeEncoder *encoder;
    drmModeModeInfo *mode;
    struct drm_i915_gem_create create;
    struct drm_gem_flink flink;
    int i, ret, fd;
    uint32_t fb_id;

    display = _get_display (card);
    if (display == NULL) {
	die ("Unable to open display\n");
	return NULL;
    }

    if (! eglChooseConfig (display, config_attribs, &config, 1, NULL)) {
	die ("Unable to choose config\n");
	return NULL;
    }

    context = eglCreateContext (display, config, NULL, NULL);
    if (context == NULL) {
	die ("failed to create context\n");
	return NULL;
    }

    fd = eglGetDisplayFD (display);
    resources = drmModeGetResources (fd);
    if (resources == NULL) {
	die ("drmModeGetResources failed\n");
	return NULL;
    }

    for (i = 0; i < resources->count_connectors; i++) {
	connector = drmModeGetConnector (fd, resources->connectors[i]);
	if (connector == NULL)
	    continue;

	if (connector->connection == DRM_MODE_CONNECTED &&
	    connector->count_modes > 0)
	    break;

	drmModeFreeConnector (connector);
    }

    if (i == resources->count_connectors) {
	die ("No currently active connector found.\n");
	return NULL;
    }

    mode = &connector->modes[0];

    for (i = 0; i < resources->count_encoders; i++) {
	encoder = drmModeGetEncoder(fd, resources->encoders[i]);

	if (encoder == NULL)
	    continue;

	if (encoder->encoder_id == connector->encoder_id)
	    break;

	drmModeFreeEncoder(encoder);
    }

    /* Mode size at 32 bpp */
    create.size = mode->hdisplay * mode->vdisplay * 4;
    if (ioctl (fd, DRM_IOCTL_I915_GEM_CREATE, &create) != 0) {
	die ("gem create failed: %m\n");
	return NULL;
    }

    ret = drmModeAddFB (fd, mode->hdisplay, mode->vdisplay,
		       32, 32, mode->hdisplay * 4, create.handle, &fb_id);
    if (ret) {
	die ("failed to add fb: %m\n");
	return NULL;
    }

    ret = drmModeSetCrtc (fd, encoder->crtc_id, fb_id, 0, 0,
			 &connector->connector_id, 1, mode);
    if (ret) {
	die ("failed to set mode: %m\n");
	return NULL;
    }

    flink.handle = create.handle;
    if (ioctl (fd, DRM_IOCTL_GEM_FLINK, &flink) != 0) {
	die ("gem flink failed: %m\n");
	return NULL;
    }

    s = eglCreateSurfaceForName (display, config,
				 flink.name,
				 mode->hdisplay, mode->vdisplay,
				 mode->hdisplay * 4,
				 surface_attribs);
    if (s == NULL) {
	die ("failed to create surface\n");
	return NULL;
    }

    ctx = cairo_egl_context_create (display, context);
    surface = cairo_gl_surface_create_for_eagle (ctx, s,
						 mode->hdisplay,
						 mode->vdisplay);
    cairo_gl_context_destroy (ctx);

    return surface;
}

static cairo_surface_t *
_egl_surface_create (void *closure,
		     cairo_content_t content,
		     double width, double height)
{
    return cairo_surface_create_similar (closure, content, width, height);
}

static struct list {
    struct list *next;
    cairo_t *context;
    cairo_surface_t *surface;
} *list;

static cairo_t *
_egl_context_create (void *closure, cairo_surface_t *surface)
{
    cairo_t *cr = cairo_create (surface);
    struct list *l = malloc (sizeof (*l));
    l->next = list;
    l->context = cr;
    l->surface = cairo_surface_reference (surface);
    list = l;
    return cr;
}

static void
_egl_context_destroy (void *closure, void *ptr)
{
    struct list *l, **prev = &list;
    while ((l = *prev) != NULL) {
	if (l->context == ptr) {
	    if (cairo_surface_status (l->surface) == CAIRO_STATUS_SUCCESS) {
		cairo_t *cr = cairo_create (closure);
		cairo_set_source_surface (cr, l->surface, 0, 0);
		cairo_paint (cr);
		cairo_destroy (cr);
		cairo_gl_surface_swapbuffers (closure);
	    }

	    cairo_surface_destroy (l->surface);
	    *prev = l->next;
	    free (l);
	    return;
	}
	prev = &l->next;
    }
}

int
main (int argc, char **argv)
{
    const cairo_script_interpreter_hooks_t hooks = {
	.closure = _get_fb ("/dev/dri/card0"),
	.surface_create = _egl_surface_create,
	.context_create = _egl_context_create,
	.context_destroy = _egl_context_destroy
    };
    cairo_script_interpreter_t *csi;
    int i;

    csi = cairo_script_interpreter_create ();
    cairo_script_interpreter_install_hooks (csi, &hooks);
    for (i = 1; i < argc; i++)
	cairo_script_interpreter_run (csi, argv[i]);
    return cairo_script_interpreter_destroy (csi);
}
