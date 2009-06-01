/*
 * Copyright © 2007 Michael Dominic K.
 * Copyright © 2008, 2009 Kristian Høgsberg
 * Copyright © 2009 Chris Wilson
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc., 59
 * Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */
#include <cairo-gl.h>

#include <math.h>
#include <stdio.h>

#include <sys/stat.h>
#include <sys/ioctl.h>
#include <xf86drmMode.h>
#include <i915_drm.h>

#define LIBUDEV_I_KNOW_THE_API_IS_SUBJECT_TO_CHANGE
#include <libudev.h>

typedef struct {
    float x;
    float y;
    float scale;
    float rotation;
    float r1, g1, b1, a1;
    float r2, b2, g2, a2;
} Flower;

#define N_FLOWERS 200
#define FLOWER_SIZE 128
static Flower flowers[N_FLOWERS];

static EGLDisplay
_get_display (void)
{
    EGLint major, minor;
    EGLDisplay display;
    struct udev *udev;
    struct udev_device *device;
    struct stat st;

    if (stat ("/dev/dri/card0", &st) < 0) {
	fprintf(stderr, "no such device\n");
	return NULL;
    }

    udev = udev_new ();
    device = udev_device_new_from_devnum (udev, 'c', st.st_rdev);
    if (device == NULL) {
	fprintf (stderr, "failed to find device\n");
	return NULL;
    }
    display = eglCreateDisplayNative (device);
    udev_device_unref (device);
    udev_unref (udev);

    if (display == NULL) {
	fprintf (stderr, "failed to open display\n");
	return NULL;
    }

    if (! eglInitialize (display, &major, &minor)) {
	fprintf (stderr, "failed to initialize display\n");
	return NULL;
    }

    return display;
}

static cairo_surface_t *
_get_fb (void)
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

    display = _get_display ();
    if (display == NULL) {
	fprintf (stderr, "Unable to open display\n");
	return NULL;
    }

    if (! eglChooseConfig (display, config_attribs, &config, 1, NULL)) {
	fprintf (stderr, "Unable to choose config\n");
	return NULL;
    }

    context = eglCreateContext (display, config, NULL, NULL);
    if (context == NULL) {
	fprintf (stderr, "failed to create context\n");
	return NULL;
    }

    fd = eglGetDisplayFD (display);
    resources = drmModeGetResources (fd);
    if (resources == NULL) {
	fprintf (stderr, "drmModeGetResources failed\n");
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
	fprintf (stderr, "No currently active connector found.\n");
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
	fprintf (stderr, "gem create failed: %m\n");
	return NULL;
    }

    ret = drmModeAddFB (fd, mode->hdisplay, mode->vdisplay,
		       32, 32, mode->hdisplay * 4, create.handle, &fb_id);
    if (ret) {
	fprintf (stderr, "failed to add fb: %m\n");
	return NULL;
    }

    ret = drmModeSetCrtc (fd, encoder->crtc_id, fb_id, 0, 0,
			 &connector->connector_id, 1, mode);
    if (ret) {
	fprintf (stderr, "failed to set mode: %m\n");
	return NULL;
    }

    flink.handle = create.handle;
    if (ioctl (fd, DRM_IOCTL_GEM_FLINK, &flink) != 0) {
	fprintf (stderr, "gem flink failed: %m\n");
	return NULL;
    }

    s = eglCreateSurfaceForName (display, config,
				 flink.name,
				 mode->hdisplay, mode->vdisplay,
				 mode->hdisplay * 4,
				 surface_attribs);
    if (s == NULL) {
	fprintf (stderr, "failed to create surface\n");
	return NULL;
    }

    ctx = cairo_egl_context_create (display, context);
    surface = cairo_gl_surface_create_for_eagle (ctx, s,
						 mode->hdisplay,
						 mode->vdisplay);
    cairo_gl_context_destroy (ctx);

    if (cairo_surface_status (surface)) {
	fprintf (stderr, "failed to create cairo surface\n");
	return NULL;
    }

    return surface;
}


static unsigned int
hars_petruska_f54_1_random (void)
{
#define rol(x,k) ((x << k) | (x >> (32-k)))
    static unsigned int x;
    return x = (x ^ rol (x, 5) ^ rol (x, 24)) + 0x37798849;
#undef rol
}

static void
random_colour (float *r, float *g, float *b, float *a)
{
    unsigned int x = hars_petruska_f54_1_random ();
    *r = (x & 255) / 255.; x >>= 8;
    *g = (x & 255) / 255.; x >>= 8;
    *b = (x & 255) / 255.; x >>= 8;
    *a = x / 255.;
}

static void
randomize_flower (Flower *flower, int width, int height)
{
    flower->x = (hars_petruska_f54_1_random() & 8191) * width / 8191.;
    flower->y = (hars_petruska_f54_1_random() & 8191) * height / 8191.;
    flower->scale = 10 + (hars_petruska_f54_1_random() & 511) * 140 / 512.;
    flower->rotation = (hars_petruska_f54_1_random() & 511) * M_PI / 256;

    random_colour (&flower->r1,
	           &flower->g1,
	           &flower->b1,
	           &flower->a1);
    random_colour (&flower->r2,
	           &flower->g2,
	           &flower->b2,
	           &flower->a2);
}

static void
randomize_flowers (int width, int height)
{
    int i;

    for (i = 0; i < N_FLOWERS; i++)
        randomize_flower (&flowers [i], width, height);
}

static cairo_pattern_t *
create_flower (cairo_surface_t *target, int size)
{
    cairo_surface_t *surface;
    cairo_pattern_t *mask;
    cairo_t *cr;

    surface = cairo_surface_create_similar (target,
	                                    CAIRO_CONTENT_ALPHA, size, size);
    cr = cairo_create (surface);
    cairo_surface_destroy (surface);

    cairo_scale (cr, size/2, size/2);
    cairo_translate (cr, 1., 1.);
    cairo_move_to (cr, 0, 0);
    cairo_curve_to (cr, -0.9, 0, -0.9, -0.9, -0.9, -0.9);
    cairo_curve_to (cr, 0.0, -0.9, 0, 0, 0, 0);
    cairo_curve_to (cr, 0.9, 0.0, 0.9, -0.9, 0.9, -0.9);
    cairo_curve_to (cr, 0.0, -0.9, 0.0, 0.0, 0.0, 0.0);
    cairo_curve_to (cr, 0.9, 0.0, 0.9, 0.9, 0.9, 0.9);
    cairo_curve_to (cr, 0.0, 0.9, 0.0, 0.0, 0.0, 0.0);
    cairo_curve_to (cr, -0.9, 0.0, -0.9, 0.9, -0.9, 0.9);
    cairo_curve_to (cr, 0.0, 0.9, 0.0, 0.0, 0.0, 0.0);
    cairo_set_source_rgb (cr, 1, 1, 1);
    cairo_fill (cr);

    mask = cairo_pattern_create_for_surface (cairo_get_target (cr));
    cairo_destroy (cr);

    return mask;
}

static void
paint (cairo_surface_t *surface,
       cairo_pattern_t *mask,
       int mask_size)
{
    cairo_t *cr;
    int i;

    cr = cairo_create (surface);
    for (i = 0; i < N_FLOWERS; i++) {
        cairo_pattern_t *pattern;
	cairo_matrix_t matrix;

        cairo_matrix_init_identity (&matrix);
        cairo_matrix_translate (&matrix, flowers[i].x, flowers[i].y);
        cairo_matrix_scale (&matrix,
			    flowers[i].scale/mask_size,
			    flowers[i].scale/mask_size);
        cairo_matrix_rotate (&matrix, flowers[i].rotation);
        cairo_set_matrix (cr, &matrix);

        pattern = cairo_pattern_create_linear (0, -mask_size, 0, mask_size);
        cairo_pattern_add_color_stop_rgba (pattern, 0,
		flowers[i].r1, flowers[i].g1, flowers[i].b1, flowers[i].a1);
        cairo_pattern_add_color_stop_rgba (pattern, 1,
		flowers[i].r2, flowers[i].g2, flowers[i].b2, flowers[i].a2);
        cairo_set_source (cr, pattern);
        cairo_pattern_destroy (pattern);

	cairo_mask (cr, mask);
    }
    cairo_destroy (cr);
}

int
main (int argc, char *argv[])
{
    cairo_surface_t *surface;
    cairo_pattern_t *flower;

    surface = _get_fb ();
    if (surface == NULL) {
	fprintf (stderr, "Failed to create framebuffer\n");
	return 1;
    }

    flower = create_flower (surface, FLOWER_SIZE);
    while (1) {
	randomize_flowers (cairo_gl_surface_get_width (surface),
			   cairo_gl_surface_get_height (surface));
	paint (surface, flower, FLOWER_SIZE);
	cairo_gl_surface_swapbuffers (surface);
    }

    cairo_pattern_destroy (flower);
    cairo_surface_destroy (surface);

    return 0;
}
