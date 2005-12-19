/*
 * Copyright © 2004 Red Hat, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of
 * Red Hat, Inc. not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission. Red Hat, Inc. makes no representations about the
 * suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * RED HAT, INC. DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL RED HAT, INC. BE LIABLE FOR ANY SPECIAL,
 * INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
 * IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author: Carl D. Worth <cworth@cworth.org>
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <errno.h>
#include <string.h>
#if HAVE_FCFINI
#include <fontconfig/fontconfig.h>
#endif

#include "cairo-test.h"

#include "buffer-diff.h"
#include "read-png.h"
#include "write-png.h"
#include "xmalloc.h"

#ifdef _MSC_VER
#define vsnprintf _vsnprintf
#define access _access
#define F_OK 0
#endif

static void
xunlink (const char *pathname);

#define CAIRO_TEST_LOG_SUFFIX ".log"
#define CAIRO_TEST_PNG_SUFFIX "-out.png"
#define CAIRO_TEST_REF_SUFFIX "-ref.png"
#define CAIRO_TEST_RGB24_REF_SUFFIX "-rgb24-ref.png"
#define CAIRO_TEST_DIFF_SUFFIX "-diff.png"

/* Static data is messy, but we're coding for tests here, not a
 * general-purpose library, and it keeps the tests cleaner to avoid a
 * context object there, (though not a whole lot). */
FILE *cairo_test_log_file;

void
cairo_test_init (const char *test_name)
{
    char *log_name;

    xasprintf (&log_name, "%s%s", test_name, CAIRO_TEST_LOG_SUFFIX);
    xunlink (log_name);

    cairo_test_log_file = fopen (log_name, "a");
    if (cairo_test_log_file == NULL) {
	fprintf (stderr, "Error opening log file: %s\n", log_name);
	cairo_test_log_file = stderr;
    }
    free (log_name);
}

void
cairo_test_log (const char *fmt, ...)
{
    va_list va;

    va_start (va, fmt);
    vfprintf (cairo_test_log_file, fmt, va);
    va_end (va);
}

void
xasprintf (char **strp, const char *fmt, ...)
{
#ifdef HAVE_VASPRINTF    
    va_list va;
    int ret;
    
    va_start (va, fmt);
    ret = vasprintf (strp, fmt, va);
    va_end (va);

    if (ret < 0) {
	cairo_test_log ("Out of memory\n");
	exit (1);
    }
#else /* !HAVE_VASNPRINTF */
#define BUF_SIZE 1024
    va_list va;
    char buffer[BUF_SIZE];
    int ret;
    
    va_start (va, fmt);
    ret = vsnprintf (buffer, sizeof(buffer), fmt, va);
    va_end (va);

    if (ret < 0) {
	cairo_test_log ("Failure in vsnprintf\n");
	exit (1);
    }
    
    if (strlen (buffer) == sizeof(buffer) - 1) {
	cairo_test_log ("Overflowed fixed buffer\n");
	exit (1);
    }
    
    *strp = strdup (buffer);
    if (!*strp) {
	cairo_test_log ("Out of memory\n");
	exit (1);
    }
#endif /* !HAVE_VASNPRINTF */
}

static void
xunlink (const char *pathname)
{
    if (unlink (pathname) < 0 && errno != ENOENT) {
	cairo_test_log ("  Error: Cannot remove %s: %s\n",
			pathname, strerror (errno));
	exit (1);
    }
}

typedef cairo_surface_t *
(*cairo_test_create_target_surface_t) (cairo_test_t *test, cairo_format_t format,
				       void **closure);

typedef cairo_status_t
(*cairo_test_write_to_png_t) (cairo_surface_t *surface, const char *filename);

typedef void
(*cairo_test_cleanup_target_t) (void *closure);

typedef struct _cairo_test_target
{
    const char		       	       *name;
    cairo_format_t			reference_format;
    cairo_test_create_target_surface_t	create_target_surface;
    cairo_test_write_to_png_t		write_to_png;
    cairo_test_cleanup_target_t		cleanup_target;
    void		       	       *closure;
} cairo_test_target_t;

static char *
cairo_target_format_name (const cairo_test_target_t *target)
{
    char *format;

    if (target->reference_format == CAIRO_FORMAT_RGB24)
	format = "rgb24";
    else
	format = "argb32";
    return format;
}

static cairo_surface_t *
create_image_surface (cairo_test_t *test, cairo_format_t format,
		      void **closure)
{
    *closure = NULL;
    return cairo_image_surface_create (format, test->width, test->height);
}

#ifdef CAIRO_HAS_TEST_SURFACES

#include "test-fallback-surface.h"
#include "test-meta-surface.h"

static cairo_surface_t *
create_test_fallback_surface (cairo_test_t *test, cairo_format_t format,
			      void **closure)
{
    *closure = NULL;
    return _test_fallback_surface_create (format, test->width, test->height);
}

static cairo_surface_t *
create_test_meta_surface (cairo_test_t *test, cairo_format_t format,
			  void **closure)
{
    *closure = NULL;
    return _test_meta_surface_create (format, test->width, test->height);
}

#endif

#ifdef CAIRO_HAS_GLITZ_SURFACE
#include <glitz.h>
#include <cairo-glitz.h>

static const cairo_user_data_key_t glitz_closure_key;

typedef struct _glitz_target_closure_base {
    int width;
    int height;
    cairo_format_t format;
} glitz_target_closure_base_t;

static cairo_status_t
cairo_glitz_surface_write_to_png (cairo_surface_t *surface,
				  const char *filename)
{
    glitz_target_closure_base_t *closure;
    cairo_surface_t * imgsr;
    cairo_t         * imgcr;

    closure = cairo_surface_get_user_data (surface, &glitz_closure_key);
    imgsr = cairo_image_surface_create (closure->format, closure->width, closure->height);
    imgcr = cairo_create (imgsr);
    
    cairo_set_source_surface (imgcr, surface, 0, 0);
    cairo_paint (imgcr);

    cairo_surface_write_to_png (imgsr, filename);
    
    cairo_destroy (imgcr);
    cairo_surface_destroy (imgsr);
    
    return CAIRO_STATUS_SUCCESS;
}

#if CAIRO_CAN_TEST_GLITZ_GLX_SURFACE
#include <glitz-glx.h>

typedef struct _glitz_glx_target_closure {
    glitz_target_closure_base_t base;
    Display        *dpy;
    int             scr;
    Window          win;
} glitz_glx_target_closure_t;

static glitz_surface_t *
create_glitz_glx_surface (glitz_format_name_t	      formatname,
			  int			      width,
			  int			      height,
			  glitz_glx_target_closure_t *closure)
{
    Display                 * dpy = closure->dpy;
    int                       scr = closure->scr;
    glitz_drawable_format_t   templ;
    glitz_drawable_format_t * dformat = NULL;
    unsigned long             mask;
    glitz_drawable_t        * drawable = NULL;
    glitz_format_t          * format;
    glitz_surface_t         * sr;

    XSizeHints                xsh;
    XSetWindowAttributes      xswa;
    XVisualInfo             * vinfo;

    memset(&templ, 0, sizeof(templ));
    templ.color.red_size = 8;
    templ.color.green_size = 8;
    templ.color.blue_size = 8;
    templ.color.alpha_size = 8;
    templ.color.fourcc = GLITZ_FOURCC_RGB;
    templ.samples = 1;

    mask = GLITZ_FORMAT_SAMPLES_MASK | GLITZ_FORMAT_FOURCC_MASK |
	GLITZ_FORMAT_RED_SIZE_MASK | GLITZ_FORMAT_GREEN_SIZE_MASK |
	GLITZ_FORMAT_BLUE_SIZE_MASK;
    if (formatname == GLITZ_STANDARD_ARGB32)
	mask |= GLITZ_FORMAT_ALPHA_SIZE_MASK;

    /* Try for a pbuffer first */
    if (!getenv("CAIRO_TEST_FORCE_GLITZ_WINDOW"))
	dformat = glitz_glx_find_pbuffer_format (dpy, scr, mask, &templ, 0);

    if (dformat) {
	closure->win = NULL;

	drawable = glitz_glx_create_pbuffer_drawable (dpy, scr, dformat,
						      width, height);
	if (!drawable)
	    goto FAIL;
    } else {
	/* No pbuffer, try window */
	dformat = glitz_glx_find_window_format (dpy, scr, mask, &templ, 0);

	if (!dformat)
	    goto FAIL;

	vinfo = glitz_glx_get_visual_info_from_format(dpy,
						      DefaultScreen(dpy),
						      dformat);

	if (!vinfo)
	    goto FAIL;

	xsh.flags = PSize;
	xsh.x = 0;
	xsh.y = 0;
	xsh.width = width;
	xsh.height = height;

	xswa.colormap = XCreateColormap (dpy, RootWindow(dpy, scr),
					 vinfo->visual, AllocNone);
	closure->win = XCreateWindow (dpy, RootWindow(dpy, scr),
				      xsh.x, xsh.y, xsh.width, xsh.height,
				      0, vinfo->depth, CopyFromParent,
				      vinfo->visual, CWColormap, &xswa);
    
	drawable =
	    glitz_glx_create_drawable_for_window (dpy, scr,
						  dformat, closure->win,
						  width, height);

	if (!drawable)
	    goto DESTROY_WINDOW;
    }


    format = glitz_find_standard_format (drawable, formatname);
    if (!format)
	goto DESTROY_DRAWABLE;

    sr = glitz_surface_create (drawable, format, width, height, 0, NULL);
    if (!sr)
	goto DESTROY_DRAWABLE;

    if (closure->win == NULL || dformat->doublebuffer) {
	glitz_surface_attach (sr, drawable, GLITZ_DRAWABLE_BUFFER_BACK_COLOR);
    } else {
	XMapWindow (closure->dpy, closure->win);
	glitz_surface_attach (sr, drawable, GLITZ_DRAWABLE_BUFFER_FRONT_COLOR);
    }

    glitz_drawable_destroy (drawable);

    return sr;
 DESTROY_DRAWABLE:
    glitz_drawable_destroy (drawable);
 DESTROY_WINDOW:
    if (closure->win)
	XDestroyWindow (dpy, closure->win);
 FAIL:
    return NULL;
}

static cairo_surface_t *
create_cairo_glitz_glx_surface (cairo_test_t   *test,
				cairo_format_t  format,
				void          **closure)
{
    int width = test->width;
    int height = test->height;
    glitz_glx_target_closure_t *gxtc;
    glitz_surface_t  * glitz_surface;
    cairo_surface_t  * surface;

    *closure = gxtc = xmalloc (sizeof (glitz_glx_target_closure_t));

    if (width == 0)
	width = 1;
    if (height == 0)
	height = 1;
    
    gxtc->dpy = XOpenDisplay (NULL);
    if (!gxtc->dpy) {
	cairo_test_log ("Failed to open display: %s\n", XDisplayName(0));
	goto FAIL;
    }

    XSynchronize (gxtc->dpy, 1);

    gxtc->scr = DefaultScreen(gxtc->dpy);

    switch (format) {
    case CAIRO_FORMAT_RGB24:
	glitz_surface = create_glitz_glx_surface (GLITZ_STANDARD_RGB24, width, height, gxtc);
	break;
    case CAIRO_FORMAT_ARGB32:
	glitz_surface = create_glitz_glx_surface (GLITZ_STANDARD_ARGB32, width, height, gxtc);
	break;
    default:
	cairo_test_log ("Invalid format for glitz-glx test: %d\n", format);
	goto FAIL_CLOSE_DISPLAY;
    }
    if (!glitz_surface) {
	cairo_test_log ("Failed to create glitz-glx surface\n");
	goto FAIL_CLOSE_DISPLAY;
    }

    surface = cairo_glitz_surface_create (glitz_surface);

    gxtc->base.width = test->width;
    gxtc->base.height = test->height;
    gxtc->base.format = format;
    cairo_surface_set_user_data (surface, &glitz_closure_key,
				 gxtc, NULL);

    return surface;

 FAIL_CLOSE_DISPLAY:
    XCloseDisplay (gxtc->dpy);
 FAIL:
    return NULL;
}

static void
cleanup_cairo_glitz_glx (void *closure)
{
    glitz_glx_target_closure_t *gxtc = closure;

    glitz_glx_fini ();

    if (gxtc->win)
	XDestroyWindow (gxtc->dpy, gxtc->win);

    XCloseDisplay (gxtc->dpy);

    free (gxtc);
}

#endif /* CAIRO_CAN_TEST_GLITZ_GLX_SURFACE */

#if CAIRO_CAN_TEST_GLITZ_AGL_SURFACE
#include <glitz-agl.h>

typedef struct _glitz_agl_target_closure {
    glitz_target_closure_base_t base;
} glitz_agl_target_closure_t;

static glitz_surface_t *
create_glitz_agl_surface (glitz_format_name_t formatname,
			  int width, int height,
			  glitz_agl_target_closure_t *closure)
{
    glitz_drawable_format_t *dformat;
    glitz_drawable_format_t templ;
    glitz_drawable_t *gdraw;
    glitz_format_t *format;
    glitz_surface_t *sr = NULL;
    unsigned long mask;

    memset(&templ, 0, sizeof(templ));
    templ.color.red_size = 8;
    templ.color.green_size = 8;
    templ.color.blue_size = 8;
    templ.color.alpha_size = 8;
    templ.color.fourcc = GLITZ_FOURCC_RGB;
    templ.samples = 1;

    mask = GLITZ_FORMAT_SAMPLES_MASK | GLITZ_FORMAT_FOURCC_MASK |
	GLITZ_FORMAT_RED_SIZE_MASK | GLITZ_FORMAT_GREEN_SIZE_MASK |
	GLITZ_FORMAT_BLUE_SIZE_MASK;
    if (formatname == GLITZ_STANDARD_ARGB32)
	mask |= GLITZ_FORMAT_ALPHA_SIZE_MASK;

    dformat = glitz_agl_find_pbuffer_format (mask, &templ, 0);
    if (!dformat) {
	cairo_test_log ("Glitz failed to find pbuffer format for template.");
	goto FAIL;
    }

    gdraw = glitz_agl_create_pbuffer_drawable (dformat, width, height);
    if (!gdraw) {
	cairo_test_log ("Glitz failed to create pbuffer drawable.");
	goto FAIL;
    }

    format = glitz_find_standard_format (gdraw, formatname);
    if (!format) {
	cairo_test_log ("Glitz failed to find standard format for drawable.");
	goto DESTROY_DRAWABLE;
    }

    sr = glitz_surface_create (gdraw, format, width, height, 0, NULL);
    if (!sr) {
	cairo_test_log ("Glitz failed to create a surface.");
	goto DESTROY_DRAWABLE;
    }

    glitz_surface_attach (sr, gdraw, GLITZ_DRAWABLE_BUFFER_FRONT_COLOR);

 DESTROY_DRAWABLE:
    glitz_drawable_destroy (gdraw);

 FAIL:
    return sr; /* will be NULL unless we create it and attach */
}

static cairo_surface_t *
create_cairo_glitz_agl_surface (cairo_test_t *test,
				cairo_format_t format,
				void **closure)
{
    glitz_surface_t *glitz_surface;
    cairo_surface_t *surface;
    glitz_agl_target_closure_t *aglc;

    glitz_agl_init ();

    *closure = aglc = xmalloc (sizeof (glitz_agl_target_closure_t));

    switch (format) {
    case CAIRO_FORMAT_RGB24:
	glitz_surface = create_glitz_agl_surface (GLITZ_STANDARD_RGB24, test->width, test->height, NULL);
	break;
    case CAIRO_FORMAT_ARGB32:
	glitz_surface = create_glitz_agl_surface (GLITZ_STANDARD_ARGB32, test->width, test->height, NULL);
	break;
    default:
	cairo_test_log ("Invalid format for glitz-agl test: %d\n", format);
	goto FAIL;
    }

    if (!glitz_surface)
	goto FAIL;

    surface = cairo_glitz_surface_create (glitz_surface);

    aglc->base.width = test->width;
    aglc->base.height = test->height;
    aglc->base.format = format;
    cairo_surface_set_user_data (surface, &glitz_closure_key, aglc, NULL);

    return surface;

 FAIL:
    return NULL;
}

static void
cleanup_cairo_glitz_agl (void *closure)
{
    free (closure);
    glitz_agl_fini ();
}

#endif /* CAIRO_CAN_TEST_GLITZ_AGL_SURFACE */

#if CAIRO_CAN_TEST_GLITZ_WGL_SURFACE
#include <glitz-wgl.h>

typedef struct _glitz_wgl_target_closure {
    glitz_target_closure_base_t base;
} glitz_wgl_target_closure_t;

static glitz_surface_t *
create_glitz_wgl_surface (glitz_format_name_t formatname,
			  int width, int height,
			  glitz_wgl_target_closure_t *closure)
{
    glitz_drawable_format_t *dformat;
    glitz_drawable_format_t templ;
    glitz_drawable_t *gdraw;
    glitz_format_t *format;
    glitz_surface_t *sr = NULL;
    unsigned long mask;

    memset(&templ, 0, sizeof(templ));
    templ.color.red_size = 8;
    templ.color.green_size = 8;
    templ.color.blue_size = 8;
    templ.color.alpha_size = 8;
    templ.color.fourcc = GLITZ_FOURCC_RGB;
    templ.samples = 1;

    mask = GLITZ_FORMAT_SAMPLES_MASK | GLITZ_FORMAT_FOURCC_MASK |
	GLITZ_FORMAT_RED_SIZE_MASK | GLITZ_FORMAT_GREEN_SIZE_MASK |
	GLITZ_FORMAT_BLUE_SIZE_MASK;
    if (formatname == GLITZ_STANDARD_ARGB32)
	mask |= GLITZ_FORMAT_ALPHA_SIZE_MASK;

    dformat = glitz_wgl_find_pbuffer_format (mask, &templ, 0);
    if (!dformat) {
	cairo_test_log ("Glitz failed to find pbuffer format for template.");
	goto FAIL;
    }

    gdraw = glitz_wgl_create_pbuffer_drawable (dformat, width, height);
    if (!gdraw) {
	cairo_test_log ("Glitz failed to create pbuffer drawable.");
	goto FAIL;
    }

    format = glitz_find_standard_format (gdraw, formatname);
    if (!format) {
	cairo_test_log ("Glitz failed to find standard format for drawable.");
	goto DESTROY_DRAWABLE;
    }

    sr = glitz_surface_create (gdraw, format, width, height, 0, NULL);
    if (!sr) {
	cairo_test_log ("Glitz failed to create a surface.");
	goto DESTROY_DRAWABLE;
    }

    glitz_surface_attach (sr, gdraw, GLITZ_DRAWABLE_BUFFER_FRONT_COLOR);

 DESTROY_DRAWABLE:
    glitz_drawable_destroy (gdraw);

 FAIL:
    return sr; /* will be NULL unless we create it and attach */
}

static cairo_surface_t *
create_cairo_glitz_wgl_surface (cairo_test_t *test,
				cairo_format_t format,
				void **closure)
{
    glitz_surface_t *glitz_surface;
    cairo_surface_t *surface;
    glitz_wgl_target_closure_t *wglc;

    glitz_wgl_init (NULL);

    *closure = wglc = xmalloc (sizeof (glitz_wgl_target_closure_t));

    switch (format) {
    case CAIRO_FORMAT_RGB24:
	glitz_surface = create_glitz_wgl_surface (GLITZ_STANDARD_RGB24, test->width, test->height, NULL);
	break;
    case CAIRO_FORMAT_ARGB32:
	glitz_surface = create_glitz_wgl_surface (GLITZ_STANDARD_ARGB32, test->width, test->height, NULL);
	break;
    default:
	cairo_test_log ("Invalid format for glitz-wgl test: %d\n", format);
	goto FAIL;
    }

    if (!glitz_surface)
	goto FAIL;

    surface = cairo_glitz_surface_create (glitz_surface);

    wglc->base.width = test->width;
    wglc->base.height = test->height;
    wglc->base.format = format;
    cairo_surface_set_user_data (surface, &glitz_closure_key, wglc, NULL);

    return surface;

 FAIL:
    return NULL;
}

static void
cleanup_cairo_glitz_wgl (void *closure)
{
    free (closure);
    glitz_wgl_fini ();
}

#endif /* CAIRO_CAN_TEST_GLITZ_WGL_SURFACE */

#endif /* CAIRO_HAS_GLITZ_SURFACE */

#if 0 && CAIRO_HAS_QUARTZ_SURFACE
static cairo_surface_t *
create_quartz_surface (int width, int height, void **closure)
{
#error Not yet implemented
}

static void
cleanup_quartz (void *closure)
{
#error Not yet implemented
}
#endif

/* Testing the win32 surface isn't interesting, since for
 * ARGB images it just chains to the image backend
 */
#if CAIRO_HAS_WIN32_SURFACE
#include "cairo-win32.h"
typedef struct _win32_target_closure
{
  HDC dc;
  HBITMAP bmp;
} win32_target_closure_t;

static cairo_surface_t *
create_win32_surface (cairo_test_t *test, cairo_format_t format,
		      void **closure)
{
    int width = test->width;
    int height = test->height;

    BITMAPINFO bmpInfo;
    unsigned char *bits = NULL;
    win32_target_closure_t *data = malloc(sizeof(win32_target_closure_t));
    *closure = data;

    data->dc = CreateCompatibleDC(NULL);

    /* initialize the bitmapinfoheader */
    memset(&bmpInfo.bmiHeader, 0, sizeof(BITMAPINFOHEADER));
    bmpInfo.bmiHeader.biSize = sizeof (BITMAPINFOHEADER);
    bmpInfo.bmiHeader.biWidth = width;
    bmpInfo.bmiHeader.biHeight = -height;
    bmpInfo.bmiHeader.biPlanes = 1;
    bmpInfo.bmiHeader.biBitCount = 24;
    bmpInfo.bmiHeader.biCompression = BI_RGB;

    /* create a DIBSection */
    data->bmp = CreateDIBSection(data->dc, &bmpInfo, DIB_RGB_COLORS, (void**)&bits, NULL, 0);

    /* Flush GDI to make sure the DIBSection is actually created */
    GdiFlush();

    /* Select the bitmap in to the DC */
    SelectObject(data->dc, data->bmp);

    return cairo_win32_surface_create(data->dc);
}

static void
cleanup_win32 (void *closure)
{
  win32_target_closure_t *data = (win32_target_closure_t*)closure;
  DeleteObject(data->bmp);
  DeleteDC(data->dc);

  free(closure);
}
#endif

#if CAIRO_HAS_XCB_SURFACE
#include "cairo-xcb-xrender.h"
typedef struct _xcb_target_closure
{
    XCBConnection *c;
    XCBDRAWABLE drawable;
} xcb_target_closure_t;

/* XXX: This is a nasty hack. Something like this should be in XCB's
 * bindings for Render, not here in this test. */
static XCBRenderPICTFORMINFO
_format_from_cairo(XCBConnection *c, cairo_format_t fmt)
{
    XCBRenderPICTFORMINFO ret = {{ 0 }};
    struct tmpl_t {
	XCBRenderDIRECTFORMAT direct;
	CARD8 depth;
    };
    static const struct tmpl_t templates[] = {
	/* CAIRO_FORMAT_ARGB32 */
	{
	    {
		16, 0xff,
		8,  0xff,
		0,  0xff,
		24, 0xff
	    },
	    32
	},
	/* CAIRO_FORMAT_RGB24 */
	{
	    {
		16, 0xff,
		8,  0xff,
		0,  0xff,
		0,  0x00
	    },
	    24
	},
	/* CAIRO_FORMAT_A8 */
	{
	    {
		0,  0x00,
		0,  0x00,
		0,  0x00,
		0,  0xff
	    },
	    8
	},
	/* CAIRO_FORMAT_A1 */
	{
	    {
		0,  0x00,
		0,  0x00,
		0,  0x00,
		0,  0x01
	    },
	    1
	},
    };
    const struct tmpl_t *tmpl;
    XCBRenderQueryPictFormatsRep *r;
    XCBRenderPICTFORMINFOIter fi;

    if(fmt < 0 || fmt >= (sizeof(templates) / sizeof(*templates)))
	return ret;
    tmpl = templates + fmt;

    r = XCBRenderQueryPictFormatsReply(c, XCBRenderQueryPictFormats(c), 0);
    if(!r)
	return ret;

    for(fi = XCBRenderQueryPictFormatsFormatsIter(r); fi.rem; XCBRenderPICTFORMINFONext(&fi))
    {
	const XCBRenderDIRECTFORMAT *t, *f;
	if(fi.data->type != XCBRenderPictTypeDirect)
	    continue;
	if(fi.data->depth != tmpl->depth)
	    continue;
	t = &tmpl->direct;
	f = &fi.data->direct;
	if(t->red_mask && (t->red_mask != f->red_mask || t->red_shift != f->red_shift))
	    continue;
	if(t->green_mask && (t->green_mask != f->green_mask || t->green_shift != f->green_shift))
	    continue;
	if(t->blue_mask && (t->blue_mask != f->blue_mask || t->blue_shift != f->blue_shift))
	    continue;
	if(t->alpha_mask && (t->alpha_mask != f->alpha_mask || t->alpha_shift != f->alpha_shift))
	    continue;

	ret = *fi.data;
    }

    free(r);
    return ret;
}

static cairo_surface_t *
create_xcb_surface (int width, int height, void **closure)
{
    XCBSCREEN *root;
    xcb_target_closure_t *xtc;
    cairo_surface_t *surface;
    XCBConnection *c;
    XCBRenderPICTFORMINFO render_format;

    *closure = xtc = xmalloc (sizeof (xcb_target_closure_t));

    if (width == 0)
	width = 1;
    if (height == 0)
	height = 1;

    xtc->c = c = XCBConnectBasic();
    if (c == NULL) {
	cairo_test_log ("Failed to connect to X server through XCB\n");
	return NULL;
    }

    root = XCBConnSetupSuccessRepRootsIter(XCBGetSetup(c)).data;

    xtc->drawable.pixmap = XCBPIXMAPNew (c);
    {
	XCBDRAWABLE root_drawable;
	root_drawable.window = root->root;
	XCBCreatePixmap (c, 32, xtc->drawable.pixmap, root_drawable,
			 width, height);
    }

    render_format = _format_from_cairo (c, CAIRO_FORMAT_ARGB32);
    if (render_format.id.xid == 0)
	return NULL;
    surface = cairo_xcb_surface_create_with_xrender_format (c, xtc->drawable,
							    &render_format,
							    width, height);

    return surface;
}

static void
cleanup_xcb (void *closure)
{
    xcb_target_closure_t *xtc = closure;

    XCBFreePixmap (xtc->c, xtc->drawable.pixmap);
    XCBDisconnect (xtc->c);
    free (xtc);
}
#endif

#if CAIRO_HAS_XLIB_SURFACE
#include "cairo-xlib-xrender.h"
typedef struct _xlib_target_closure
{
    Display *dpy;
    Pixmap pixmap;
} xlib_target_closure_t;

static cairo_surface_t *
create_xlib_surface (cairo_test_t *test, cairo_format_t format,
		     void **closure)
{
    int width = test->width;
    int height = test->height;
    xlib_target_closure_t *xtc;
    cairo_surface_t *surface;
    Display *dpy;
    XRenderPictFormat *xrender_format;

    *closure = xtc = xmalloc (sizeof (xlib_target_closure_t));

    if (width == 0)
	width = 1;
    if (height == 0)
	height = 1;

    xtc->dpy = dpy = XOpenDisplay (NULL);
    if (xtc->dpy == NULL) {
	cairo_test_log ("Failed to open display: %s\n", XDisplayName(0));
	return NULL;
    }

    XSynchronize (xtc->dpy, 1);

    /* XXX: Currently we don't do any xlib testing when the X server
     * doesn't have the Render extension. We could do better here,
     * (perhaps by converting the tests from ARGB32 to RGB24). One
     * step better would be to always test the non-Render fallbacks
     * for each test even if the server does have the Render
     * extension. That would probably be through another
     * cairo_test_target which would use an extended version of
     * cairo_test_xlib_disable_render.  */
    switch (format) {
    case CAIRO_FORMAT_ARGB32:
	xrender_format = XRenderFindStandardFormat (dpy, PictStandardARGB32);
	break;
    case CAIRO_FORMAT_RGB24:
	xrender_format = XRenderFindStandardFormat (dpy, PictStandardRGB24);
	break;
    default:
	cairo_test_log ("Invalid format for xlib test: %d\n", format);
	return NULL;
    }
    if (xrender_format == NULL) {
	cairo_test_log ("X server does not have the Render extension.\n");
	return NULL;
    }
    
    xtc->pixmap = XCreatePixmap (dpy, DefaultRootWindow (dpy),
				 width, height, xrender_format->depth);

    surface = cairo_xlib_surface_create_with_xrender_format (dpy, xtc->pixmap,
							     DefaultScreenOfDisplay (dpy),
							     xrender_format,
							     width, height);
    return surface;
}

static void
cleanup_xlib (void *closure)
{
    xlib_target_closure_t *xtc = closure;

    XFreePixmap (xtc->dpy, xtc->pixmap);
    XCloseDisplay (xtc->dpy);
    free (xtc);
}
#endif

#if CAIRO_HAS_BEOS_SURFACE
/* BeOS test functions are external as they need to be C++ */
#include "cairo-test-beos.h"
#endif

#if CAIRO_HAS_PS_SURFACE
#include "cairo-ps.h"

cairo_user_data_key_t	ps_closure_key;

typedef struct _ps_target_closure
{
    char    *filename;
    int	    width, height;
} ps_target_closure_t;

static cairo_surface_t *
create_ps_surface (cairo_test_t *test, cairo_format_t format,
		   void **closure)
{
    int width = test->width;
    int height = test->height;
    ps_target_closure_t	*ptc;
    cairo_surface_t *surface;

    /* This is the only format supported by the PS surface backend. */
    assert (format == CAIRO_FORMAT_RGB24);

    *closure = ptc = xmalloc (sizeof (ps_target_closure_t));

    ptc->width = width;
    ptc->height = height;
    
    xasprintf (&ptc->filename, "%s-%s%s", test->name, "ps-rgb24-out", ".ps");
    surface = cairo_ps_surface_create (ptc->filename, width, height);
    if (cairo_surface_status (surface)) {
	free (ptc->filename);
	free (ptc);
	return NULL;
    }
    cairo_ps_surface_set_dpi (surface, 72., 72.);
    cairo_surface_set_user_data (surface, &ps_closure_key, ptc, NULL);
    return surface;
}

static cairo_status_t
ps_surface_write_to_png (cairo_surface_t *surface, const char *filename)
{
    ps_target_closure_t *ptc = cairo_surface_get_user_data (surface, &ps_closure_key);
    char    command[4096];

    cairo_surface_finish (surface);
    sprintf (command, "gs -q -r72 -g%dx%d -dSAFER -dBATCH -dNOPAUSE -sDEVICE=png16m -sOutputFile=%s %s",
	     ptc->width, ptc->height, filename, ptc->filename);
    if (system (command) == 0)
	return CAIRO_STATUS_SUCCESS;
    return CAIRO_STATUS_WRITE_ERROR;
}

static void
cleanup_ps (void *closure)
{
    ps_target_closure_t *ptc = closure;
    free (ptc->filename);
    free (ptc);
}
#endif /* CAIRO_HAS_PS_SURFACE */

#if CAIRO_HAS_PDF_SURFACE && CAIRO_CAN_TEST_PDF_SURFACE
#include "cairo-pdf.h"

cairo_user_data_key_t pdf_closure_key;

typedef struct _pdf_target_closure
{
    char *filename;
    int   width;
    int   height;
} pdf_target_closure_t;

static cairo_surface_t *
create_pdf_surface (cairo_test_t	 *test,
		    cairo_format_t	  format,
		    void		**closure)
{
    int width = test->width;
    int height = test->height;
    pdf_target_closure_t *ptc;
    cairo_surface_t *surface;

    /* XXX: Is this the only format supported by the PDF surface backend? */
    assert (format == CAIRO_FORMAT_RGB24);

    *closure = ptc = xmalloc (sizeof (pdf_target_closure_t));

    ptc->width = width;
    ptc->height = height;
    
    xasprintf (&ptc->filename, "%s-%s%s", test->name, "pdf-rgb24-out", ".pdf");
    surface = cairo_pdf_surface_create (ptc->filename, width, height);
    if (cairo_surface_status (surface)) {
	free (ptc->filename);
	free (ptc);
	return NULL;
    }
    cairo_pdf_surface_set_dpi (surface, 72., 72.);
    cairo_surface_set_user_data (surface, &pdf_closure_key, ptc, NULL);
    return surface;
}

static cairo_status_t
pdf_surface_write_to_png (cairo_surface_t *surface, const char *filename)
{
    pdf_target_closure_t *ptc = cairo_surface_get_user_data (surface, &pdf_closure_key);
    char    command[4096];

    cairo_surface_finish (surface);
    sprintf (command, "./pdf2png %s %s 1",
	     ptc->filename, filename);

    if (system (command) != 0)
	return CAIRO_STATUS_WRITE_ERROR;

    return CAIRO_STATUS_SUCCESS;
}

static void
cleanup_pdf (void *closure)
{
    pdf_target_closure_t *ptc = closure;
    free (ptc->filename);
    free (ptc);
}
#endif /* CAIRO_HAS_PDF_SURFACE && CAIRO_CAN_TEST_PDF_SURFACE */

#if CAIRO_HAS_SVG_SURFACE && CAIRO_CAN_TEST_SVG_SURFACE
#include "cairo-svg.h"

cairo_user_data_key_t	svg_closure_key;

typedef struct _svg_target_closure
{
    char    *filename;
    int	    width, height;
} svg_target_closure_t;

static cairo_surface_t *
create_svg_surface (cairo_test_t *test, cairo_format_t format,
		    void **closure)
{
    int width = test->width;
    int height = test->height;
    svg_target_closure_t *ptc;
    cairo_surface_t *surface;

    /* This is the only format supported by the PS surface backend. */
    assert (format == CAIRO_FORMAT_RGB24);

    *closure = ptc = xmalloc (sizeof (svg_target_closure_t));

    ptc->width = width;
    ptc->height = height;
    
    xasprintf (&ptc->filename, "%s-%s%s", test->name, "svg-rgb24-out", ".svg");
    surface = cairo_svg_surface_create (ptc->filename, width, height);
    if (cairo_surface_status (surface)) {
	free (ptc->filename);
	free (ptc);
	return NULL;
    }
    cairo_surface_set_user_data (surface, &svg_closure_key, ptc, NULL);
    return surface;
}

static cairo_status_t
svg_surface_write_to_png (cairo_surface_t *surface, const char *filename)
{
    svg_target_closure_t *ptc = cairo_surface_get_user_data (surface, &svg_closure_key);
    char    command[4096];

    cairo_surface_finish (surface);

    sprintf (command, "./svg2png %s %s",
	     ptc->filename, filename);

    if (system (command) != 0)
	return CAIRO_STATUS_WRITE_ERROR;
    
    return CAIRO_STATUS_WRITE_ERROR;
}

static void
cleanup_svg (void *closure)
{
    svg_target_closure_t *ptc = closure;
    free (ptc->filename);
    free (ptc);
}
#endif /* CAIRO_HAS_SVG_SURFACE && CAIRO_CAN_TEST_SVG_SURFACE */

static cairo_test_status_t
cairo_test_for_target (cairo_test_t *test,
		       cairo_test_draw_function_t draw,
		       cairo_test_target_t	 *target)
{
    cairo_test_status_t status;
    cairo_surface_t *surface;
    cairo_t *cr;
    char *png_name, *ref_name, *diff_name;
    char *srcdir;
    char *format;
    cairo_test_status_t ret;

    /* Get the strings ready that we'll need. */
    srcdir = getenv ("srcdir");
    if (!srcdir)
	srcdir = ".";
    format = cairo_target_format_name (target);
    
    xasprintf (&png_name, "%s-%s-%s%s", test->name,
	       target->name, format, CAIRO_TEST_PNG_SUFFIX);
    xasprintf (&ref_name, "%s/%s-%s-%s%s", srcdir, test->name,
	       target->name, format, CAIRO_TEST_REF_SUFFIX);
    if (access (ref_name, F_OK) != 0) {
	char	*ref_suffix;
	free (ref_name);

	if (target->reference_format == CAIRO_FORMAT_RGB24)
	    ref_suffix = CAIRO_TEST_RGB24_REF_SUFFIX;
	else
	    ref_suffix = CAIRO_TEST_REF_SUFFIX;
	xasprintf (&ref_name, "%s/%s%s", srcdir, test->name,
		   ref_suffix);
    }
    xasprintf (&diff_name, "%s-%s-%s%s", test->name,
	       target->name, format, CAIRO_TEST_DIFF_SUFFIX);

    /* Run the actual drawing code. */
    surface = (target->create_target_surface) (test, target->reference_format, &target->closure);
    if (surface == NULL) {
	cairo_test_log ("Error: Failed to set %s target\n", target->name);
	ret = CAIRO_TEST_UNTESTED;
	goto UNWIND_STRINGS;
    }

    cr = cairo_create (surface);

    cairo_save (cr);
    if (target->reference_format == CAIRO_FORMAT_RGB24)
	cairo_set_source_rgba (cr, 1, 1, 1, 1);
    else
	cairo_set_source_rgba (cr, 0, 0, 0, 0);
    cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
    cairo_paint (cr);
    cairo_restore (cr);

    status = (draw) (cr, test->width, test->height);

    /* Then, check all the different ways it could fail. */
    if (status) {
	cairo_test_log ("Error: Function under test failed\n");
	ret = status;
	goto UNWIND_CAIRO;
    }

    cairo_show_page (cr);

    if (cairo_status (cr) != CAIRO_STATUS_SUCCESS) {
	cairo_test_log ("Error: Function under test left cairo status in an error state: %s\n",
			cairo_status_to_string (cairo_status (cr)));
	ret = CAIRO_TEST_FAILURE;
	goto UNWIND_CAIRO;
    }

    /* Skip image check for tests with no image (width,height == 0,0) */
    if (test->width != 0 && test->height != 0) {
	int pixels_changed;
	(target->write_to_png) (surface, png_name);
	pixels_changed = image_diff (png_name, ref_name, diff_name);
	if (pixels_changed) {
	    if (pixels_changed > 0)
		cairo_test_log ("Error: %d pixels differ from reference image %s\n",
				pixels_changed, ref_name);
	    ret = CAIRO_TEST_FAILURE;
	    goto UNWIND_CAIRO;
	}
    }

    ret = CAIRO_TEST_SUCCESS;

UNWIND_CAIRO:
    cairo_destroy (cr);
    cairo_surface_destroy (surface);

    cairo_debug_reset_static_data ();

    if (target->cleanup_target)
	target->cleanup_target (target->closure);

UNWIND_STRINGS:
    free (png_name);
    free (ref_name);
    free (diff_name);

    return ret;
}

static cairo_test_status_t
cairo_test_expecting (cairo_test_t *test, cairo_test_draw_function_t draw,
		      cairo_test_status_t expectation)
{
    int i, num_targets;
    const char *tname;
    cairo_test_status_t status, ret;
    cairo_test_target_t **targets_to_test;
    cairo_test_target_t targets[] = 
	{
	    { "image", CAIRO_FORMAT_ARGB32,
	      create_image_surface, cairo_surface_write_to_png, NULL},
	    { "image", CAIRO_FORMAT_RGB24, 
	      create_image_surface, cairo_surface_write_to_png, NULL},
#ifdef CAIRO_HAS_TEST_SURFACES
	    { "test-fallback", CAIRO_FORMAT_ARGB32,
	      create_test_fallback_surface, cairo_surface_write_to_png, NULL },
	    { "test-meta", CAIRO_FORMAT_ARGB32,
	      create_test_meta_surface, cairo_surface_write_to_png, NULL },
#endif
#ifdef CAIRO_HAS_GLITZ_SURFACE
#if CAIRO_CAN_TEST_GLITZ_GLX_SURFACE
	    { "glitz-glx", CAIRO_FORMAT_ARGB32, 
		create_cairo_glitz_glx_surface, cairo_glitz_surface_write_to_png, 
		cleanup_cairo_glitz_glx }, 
	    { "glitz-glx", CAIRO_FORMAT_RGB24, 
		create_cairo_glitz_glx_surface, cairo_glitz_surface_write_to_png, 
		cleanup_cairo_glitz_glx }, 
#endif
#if CAIRO_CAN_TEST_GLITZ_AGL_SURFACE
	    { "glitz-agl", CAIRO_FORMAT_ARGB32, 
		create_cairo_glitz_agl_surface, cairo_glitz_surface_write_to_png, 
		cleanup_cairo_glitz_agl }, 
	    { "glitz-agl", CAIRO_FORMAT_RGB24, 
		create_cairo_glitz_agl_surface, cairo_glitz_surface_write_to_png, 
		cleanup_cairo_glitz_agl }, 
#endif
#if CAIRO_CAN_TEST_GLITZ_WGL_SURFACE
	    { "glitz-wgl", CAIRO_FORMAT_ARGB32, 
		create_cairo_glitz_wgl_surface, cairo_glitz_surface_write_to_png, 
		cleanup_cairo_glitz_wgl }, 
	    { "glitz-wgl", CAIRO_FORMAT_RGB24, 
		create_cairo_glitz_wgl_surface, cairo_glitz_surface_write_to_png, 
		cleanup_cairo_glitz_wgl }, 
#endif
#endif /* CAIRO_HAS_GLITZ_SURFACE */
#if 0 && CAIRO_HAS_QUARTZ_SURFACE
	    { "quartz", CAIRO_FORMAT_RGB24,
		create_quartz_surface, cairo_surface_write_to_png,
		cleanup_quartz },
#endif
#if CAIRO_HAS_WIN32_SURFACE
	    { "win32", CAIRO_FORMAT_RGB24,
		create_win32_surface, cairo_surface_write_to_png, cleanup_win32 },
#endif
#if CAIRO_HAS_XCB_SURFACE
	    { "xcb", CAIRO_FORMAT_ARGB32,
		create_xcb_surface, cairo_surface_write_to_png, cleanup_xcb},
#endif
#if CAIRO_HAS_XLIB_SURFACE
	    { "xlib", CAIRO_FORMAT_ARGB32, 
		create_xlib_surface, cairo_surface_write_to_png, cleanup_xlib},
	    { "xlib", CAIRO_FORMAT_RGB24, 
		create_xlib_surface, cairo_surface_write_to_png, cleanup_xlib},
#endif
#if CAIRO_HAS_PS_SURFACE
	    { "ps", CAIRO_FORMAT_RGB24, 
		create_ps_surface, ps_surface_write_to_png, cleanup_ps },
#endif
#if CAIRO_HAS_PDF_SURFACE && CAIRO_CAN_TEST_PDF_SURFACE
	    { "pdf", CAIRO_FORMAT_RGB24, 
		create_pdf_surface, pdf_surface_write_to_png, cleanup_pdf },
#endif
#if CAIRO_HAS_SVG_SURFACE && CAIRO_CAN_TEST_SVG_SURFACE
	    { "svg", CAIRO_FORMAT_RGB24,
		    create_svg_surface, svg_surface_write_to_png, cleanup_svg },
#endif
#if CAIRO_HAS_BEOS_SURFACE
	    { "beos", CAIRO_FORMAT_RGB24,
		create_beos_surface, cairo_surface_write_to_png, cleanup_beos},
	    { "beos_bitmap", CAIRO_FORMAT_RGB24,
		create_beos_bitmap_surface, cairo_surface_write_to_png, cleanup_beos_bitmap},
	    { "beos_bitmap", CAIRO_FORMAT_ARGB32,
		create_beos_bitmap_surface, cairo_surface_write_to_png, cleanup_beos_bitmap},
#endif
	};

    if ((tname = getenv ("CAIRO_TEST_TARGET")) != NULL) {
	const char *tname = getenv ("CAIRO_TEST_TARGET");
	num_targets = 0;
	targets_to_test = NULL;
	/* realloc isn't exactly the best thing here, but meh. */
	for (i = 0; i < sizeof(targets)/sizeof(targets[0]); i++) {
	    if (strcmp (targets[i].name, tname) == 0) {
		targets_to_test = realloc (targets_to_test, sizeof(cairo_test_target_t *) * (num_targets+1));
		targets_to_test[num_targets++] = &targets[i];
	    }
	}

	if (num_targets == 0) {
	    fprintf (stderr, "CAIRO_TEST_TARGET '%s' not found in targets list!\n", tname);
	    exit(-1);
	}
    } else {
	num_targets = sizeof(targets)/sizeof(targets[0]);
	targets_to_test = malloc (sizeof(cairo_test_target_t*) * num_targets);
	for (i = 0; i < num_targets; i++)
	    targets_to_test[i] = &targets[i];
    }

    cairo_test_init (test->name);

    /* The intended logic here is that we return overall SUCCESS
     * iff. there is at least one tested backend and that all tested
     * backends return SUCCESS. In other words:
     *
     *	if      any backend FAILURE
     *		-> FAILURE
     *	else if all backends UNTESTED
     *		-> FAILURE
     *	else    (== some backend SUCCESS)
     *		-> SUCCESS
     */
    ret = CAIRO_TEST_UNTESTED;
    for (i = 0; i < num_targets; i++) {
    	cairo_test_target_t *target = targets_to_test[i];
	cairo_test_log ("Testing %s with %s target\n", test->name, target->name);
	printf ("%s-%s-%s:\t", test->name, target->name, cairo_target_format_name(target));

	status = cairo_test_for_target (test, draw, target);


	cairo_test_log ("TEST: %s TARGET: %s FORMAT: %s RESULT: ",
			test->name, target->name, cairo_target_format_name(target));
	switch (status) {
	case CAIRO_TEST_SUCCESS:
	    printf ("PASS\n");
	    cairo_test_log ("PASS\n");
	    if (ret == CAIRO_TEST_UNTESTED)
		ret = CAIRO_TEST_SUCCESS;
	    break;
	case CAIRO_TEST_UNTESTED:
	    printf ("UNTESTED\n");
	    cairo_test_log ("UNTESTED\n");
	    break;
	default:
	case CAIRO_TEST_FAILURE:
	    if (expectation == CAIRO_TEST_FAILURE) {
		printf ("XFAIL\n");
		cairo_test_log ("XFAIL\n");
	    } else {
		printf ("FAIL\n");
		cairo_test_log ("FAIL\n");
	    }
	    ret = status;
	    break;
	}
    }
    if (ret == CAIRO_TEST_UNTESTED)
	ret = CAIRO_TEST_FAILURE;

    fclose (cairo_test_log_file);

    free (targets_to_test);

#if HAVE_FCFINI
    FcFini ();
#endif

    return ret;
}

cairo_test_status_t
cairo_test_expect_failure (cairo_test_t		      *test, 
			   cairo_test_draw_function_t  draw,
			   const char		      *because)
{
    printf ("\n%s is expected to fail:\n\t%s\n", test->name, because);
    return cairo_test_expecting (test, draw, CAIRO_TEST_FAILURE);
}

cairo_test_status_t
cairo_test (cairo_test_t *test, cairo_test_draw_function_t draw)
{
    printf ("\n");
    return cairo_test_expecting (test, draw, CAIRO_TEST_SUCCESS);
}

cairo_surface_t *
cairo_test_create_surface_from_png (const char *filename)
{
    cairo_surface_t *image;
    char *srcdir = getenv ("srcdir");

    image = cairo_image_surface_create_from_png (filename);
    if (cairo_surface_status(image)) { 
        /* expect not found when running with srcdir != builddir 
         * such as when 'make distcheck' is run
         */
	if (srcdir) {
	    char *srcdir_filename;
	    xasprintf (&srcdir_filename, "%s/%s", srcdir, filename);
	    image = cairo_image_surface_create_from_png (srcdir_filename);
	    free (srcdir_filename);
	}
	if (cairo_surface_status(image))
	    return NULL;
    }

    return image;
}

cairo_pattern_t *
cairo_test_create_pattern_from_png (const char *filename)
{
    cairo_surface_t *image;
    cairo_pattern_t *pattern;

    image = cairo_test_create_surface_from_png (filename);

    pattern = cairo_pattern_create_for_surface (image);

    cairo_pattern_set_extend (pattern, CAIRO_EXTEND_REPEAT);

    cairo_surface_destroy (image);

    return pattern;
}
