/*
 * Copyright © 2010 Red Hat Inc.
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
 * Author: Benjamin Otte <otte@redhat.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <limits.h>

#include "cairo-test.h"

#if CAIRO_HAS_GL_SURFACE
#include <cairo-gl.h>
#endif
#if CAIRO_HAS_OS2_SURFACE
#include <cairo-os2.h>
#endif
#if CAIRO_HAS_PDF_SURFACE
#include <cairo-pdf.h>
#endif
#if CAIRO_HAS_PS_SURFACE
#include <cairo-ps.h>
#endif
#if CAIRO_HAS_XCB_SURFACE
#include <cairo-xcb.h>
#endif
#if CAIRO_HAS_XLIB_SURFACE
#include <cairo-xlib.h>
#endif

#define ARRAY_LENGTH(array) (sizeof (array) / sizeof ((array)[0]))

typedef cairo_test_status_t (* surface_test_func_t) (cairo_surface_t *surface);

static cairo_test_status_t
test_cairo_surface_create_similar (cairo_surface_t *surface)
{
    cairo_surface_t *similar;
    
    similar = cairo_surface_create_similar (surface, CAIRO_CONTENT_ALPHA, 100, 100);
    
    cairo_surface_destroy (similar);
    return CAIRO_TEST_SUCCESS;
}

static cairo_test_status_t
test_cairo_surface_create_for_rectangle (cairo_surface_t *surface)
{
    cairo_surface_t *similar;
    
    similar = cairo_surface_create_for_rectangle (surface, 1, 1, 8, 8);
    
    cairo_surface_destroy (similar);
    return CAIRO_TEST_SUCCESS;
}

static cairo_test_status_t
test_cairo_surface_reference (cairo_surface_t *surface)
{
    cairo_surface_destroy (cairo_surface_reference (surface));
    return CAIRO_TEST_SUCCESS;
}

static cairo_test_status_t
test_cairo_surface_finish (cairo_surface_t *surface)
{
    cairo_surface_finish (surface);
    return CAIRO_TEST_SUCCESS;
}

static cairo_test_status_t
test_cairo_surface_get_device (cairo_surface_t *surface)
{
    /* cairo_device_t *device = */cairo_surface_get_device (surface);
    return CAIRO_TEST_SUCCESS;
}

static cairo_test_status_t
test_cairo_surface_get_reference_count (cairo_surface_t *surface)
{
    unsigned int refcount = cairo_surface_get_reference_count (surface);
    if (refcount > 0)
        return CAIRO_TEST_SUCCESS;
    /* inert error surfaces have a refcount of 0 */
    return cairo_surface_status (surface) ? CAIRO_TEST_SUCCESS : CAIRO_TEST_ERROR;
}

static cairo_test_status_t
test_cairo_surface_status (cairo_surface_t *surface)
{
    cairo_status_t status = cairo_surface_status (surface);
    return status < CAIRO_STATUS_LAST_STATUS ? CAIRO_TEST_SUCCESS : CAIRO_TEST_ERROR;
}

static cairo_test_status_t
test_cairo_surface_get_type (cairo_surface_t *surface)
{
    /* cairo_surface_type_t type = */cairo_surface_get_type (surface);
    return CAIRO_TEST_SUCCESS;
}

static cairo_test_status_t
test_cairo_surface_get_content (cairo_surface_t *surface)
{
    cairo_content_t content = cairo_surface_get_content (surface);

    switch (content) {
    case CAIRO_CONTENT_COLOR:
    case CAIRO_CONTENT_ALPHA:
    case CAIRO_CONTENT_COLOR_ALPHA:
        return CAIRO_TEST_SUCCESS;
    default:
        return CAIRO_TEST_ERROR;
    }
}

static cairo_test_status_t
test_cairo_surface_set_user_data (cairo_surface_t *surface)
{
    static cairo_user_data_key_t key;
    cairo_status_t status;

    status = cairo_surface_set_user_data (surface, &key, &key, NULL);
    if (status == CAIRO_STATUS_NO_MEMORY)
        return CAIRO_TEST_NO_MEMORY;
    else if (status)
        return CAIRO_TEST_SUCCESS;

    if (cairo_surface_get_user_data (surface, &key) != &key)
        return CAIRO_TEST_ERROR;

    return CAIRO_TEST_SUCCESS;
}

static cairo_test_status_t
test_cairo_surface_set_mime_data (cairo_surface_t *surface)
{
    const char *mimetype = "text/x-uri";
    const char *data = "http://www.cairographics.org";
    cairo_status_t status;

    status = cairo_surface_set_mime_data (surface,
                                          mimetype,
                                          (const unsigned char *) data, strlen (data),
                                          NULL, NULL);
    return status ? CAIRO_TEST_SUCCESS : CAIRO_TEST_ERROR;
}

static cairo_test_status_t
test_cairo_surface_get_mime_data (cairo_surface_t *surface)
{
    const char *mimetype = "text/x-uri";
    const unsigned char *data;
    unsigned int length;

    cairo_surface_get_mime_data (surface, mimetype, &data, &length);
    return data == NULL && length == 0 ? CAIRO_TEST_SUCCESS : CAIRO_TEST_ERROR;
}

static cairo_test_status_t
test_cairo_surface_get_font_options (cairo_surface_t *surface)
{
    cairo_font_options_t *options;
    cairo_status_t status;

    options = cairo_font_options_create ();
    if (likely (!cairo_font_options_status (options)))
        cairo_surface_get_font_options (surface, options);
    status = cairo_font_options_status (options);
    cairo_font_options_destroy (options);
    return status ? CAIRO_TEST_ERROR : CAIRO_TEST_SUCCESS;
}

static cairo_test_status_t
test_cairo_surface_flush (cairo_surface_t *surface)
{
    cairo_surface_flush (surface);
    return CAIRO_TEST_SUCCESS;
}

static cairo_test_status_t
test_cairo_surface_mark_dirty (cairo_surface_t *surface)
{
    cairo_surface_mark_dirty (surface);
    return CAIRO_TEST_SUCCESS;
}

static cairo_test_status_t
test_cairo_surface_mark_dirty_rectangle (cairo_surface_t *surface)
{
    cairo_surface_mark_dirty_rectangle (surface, 1, 1, 8, 8);
    return CAIRO_TEST_SUCCESS;
}

static cairo_test_status_t
test_cairo_surface_set_device_offset (cairo_surface_t *surface)
{
    cairo_surface_set_device_offset (surface, 5, 5);
    return CAIRO_TEST_SUCCESS;
}

static cairo_test_status_t
test_cairo_surface_get_device_offset (cairo_surface_t *surface)
{
    double x, y;

    cairo_surface_get_device_offset (surface, &x, &y);
    return CAIRO_TEST_SUCCESS;
}

static cairo_test_status_t
test_cairo_surface_set_fallback_resolution (cairo_surface_t *surface)
{
    cairo_surface_set_fallback_resolution (surface, 42, 42);
    return CAIRO_TEST_SUCCESS;
}

static cairo_test_status_t
test_cairo_surface_get_fallback_resolution (cairo_surface_t *surface)
{
    double x, y;

    cairo_surface_get_fallback_resolution (surface, &x, &y);
    return CAIRO_TEST_SUCCESS;
}

static cairo_test_status_t
test_cairo_surface_copy_page (cairo_surface_t *surface)
{
    cairo_surface_copy_page (surface);
    return CAIRO_TEST_SUCCESS;
}

static cairo_test_status_t
test_cairo_surface_show_page (cairo_surface_t *surface)
{
    cairo_surface_show_page (surface);
    return CAIRO_TEST_SUCCESS;
}

static cairo_test_status_t
test_cairo_surface_has_show_text_glyphs (cairo_surface_t *surface)
{
    cairo_surface_has_show_text_glyphs (surface);
    return CAIRO_TEST_SUCCESS;
}



#define TEST(name, sets_status) { #name, test_ ## name, sets_status }

struct {
    const char *name;
    surface_test_func_t func;
    cairo_bool_t modifies_surface;
} tests[] = {
    TEST (cairo_surface_create_similar, FALSE),
    TEST (cairo_surface_create_for_rectangle, FALSE),
    TEST (cairo_surface_reference, FALSE),
    TEST (cairo_surface_finish, TRUE),
    TEST (cairo_surface_get_device, FALSE),
    TEST (cairo_surface_get_reference_count, FALSE),
    TEST (cairo_surface_status, FALSE),
    TEST (cairo_surface_get_type, FALSE),
    TEST (cairo_surface_get_content, FALSE),
    TEST (cairo_surface_set_user_data, FALSE),
    TEST (cairo_surface_set_mime_data, TRUE),
    TEST (cairo_surface_get_mime_data, FALSE),
    TEST (cairo_surface_get_font_options, FALSE),
    TEST (cairo_surface_flush, TRUE),
    TEST (cairo_surface_mark_dirty, TRUE),
    TEST (cairo_surface_mark_dirty_rectangle, TRUE),
    TEST (cairo_surface_set_device_offset, TRUE),
    TEST (cairo_surface_get_device_offset, FALSE),
    TEST (cairo_surface_set_fallback_resolution, TRUE),
    TEST (cairo_surface_get_fallback_resolution, FALSE),
    TEST (cairo_surface_copy_page, TRUE),
    TEST (cairo_surface_show_page, TRUE),
    TEST (cairo_surface_has_show_text_glyphs, FALSE)
};

static cairo_test_status_t
preamble (cairo_test_context_t *ctx)
{
    return CAIRO_TEST_SUCCESS;
}

static cairo_test_status_t
draw (cairo_t *cr, int width, int height)
{
    const cairo_test_context_t *ctx = cairo_test_get_context (cr);
    cairo_surface_t *similar, *target;
    cairo_test_status_t test_status;
    cairo_status_t status;
    unsigned int i;

    target = cairo_get_target (cr);

    for (i = 0; i < ARRAY_LENGTH (tests); i++) {
        similar = cairo_surface_create_similar (target,
                                                cairo_surface_get_content (target),
                                                10, 10);
        cairo_surface_finish (similar);
        test_status = tests[i].func (similar);
        status = cairo_surface_status (similar);
        cairo_surface_destroy (similar);

        if (test_status != CAIRO_TEST_SUCCESS) {
            cairo_test_log (ctx,
                            "Failed test %s with %d\n",
                            tests[i].name, (int) test_status);
            return test_status;
        }

        if (tests[i].modifies_surface &&
            strcmp (tests[i].name, "cairo_surface_finish") &&
            strcmp (tests[i].name, "cairo_surface_flush") &&
            status != CAIRO_STATUS_SURFACE_FINISHED) {
            cairo_test_log (ctx,
                            "Failed test %s: Finished surface not set into error state\n",
                            tests[i].name);
            return CAIRO_TEST_ERROR;
        }
    }

    /* 565-compatible gray background */
    cairo_set_source_rgb (cr, 0.51613, 0.55555, 0.51613);
    cairo_paint (cr);

    return CAIRO_TEST_SUCCESS;
}

CAIRO_TEST (api_special_cases,
	    "Check surface functions properly handle wrong surface arguments",
	    "api", /* keywords */
	    NULL, /* requirements */
	    10, 10,
	    preamble, draw)
