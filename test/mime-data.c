/*
 * Copyright © 2008 Chris Wilson
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of
 * Chris Wilson not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission. Chris Wilson makes no representations about the
 * suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * CHRIS WILSON DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL CHRIS WILSON BE LIABLE FOR ANY SPECIAL,
 * INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
 * IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author: Chris Wilson <chris@chris-wilson.co.uk>
 */

#include "cairo-test.h"

#include <stdio.h>
#include <errno.h>

/* Basic test to exercise the new mime-data embedding. */

static cairo_status_t
read_jpg_file (const cairo_test_context_t *ctx,
	       unsigned char **data_out,
	       unsigned int *length_out)
{
    const char jpg_filename[] = "romedalen.jpg";
    FILE *file;
    unsigned char *buf;
    unsigned int len;

    file = fopen (jpg_filename, "rb");
    if (file == NULL) {
	char filename[4096];

	/* try again with srcdir */
	snprintf (filename, sizeof (filename),
		  "%s/%s", ctx->srcdir, jpg_filename);
	file = fopen (filename, "rb");
    }
    if (file == NULL) {
	switch (errno) {
	case ENOMEM:
	    return CAIRO_STATUS_NO_MEMORY;
	default:
	    return CAIRO_STATUS_FILE_NOT_FOUND;
	}
    }

    fseek (file, 0, SEEK_END);
    len = ftell (file);
    fseek (file, 0, SEEK_SET);

    buf = xmalloc (len);
    *length_out = fread (buf, 1, len, file);
    fclose (file);
    if (*length_out != len) {
	free (buf);
	return CAIRO_STATUS_READ_ERROR;
    }

    *data_out = buf;
    return CAIRO_STATUS_SUCCESS;
}

static cairo_test_status_t
draw (cairo_t *cr, int width, int height)
{
    const cairo_test_context_t *ctx = cairo_test_get_context (cr);
    cairo_surface_t *image;
    unsigned char *jpg_data;
    unsigned int jpg_len;
    cairo_status_t status;

    status = read_jpg_file (ctx, &jpg_data, &jpg_len);
    if (status) {
	return cairo_test_status_from_status (ctx, status);
    }

    image = cairo_test_create_surface_from_png (ctx, "romedalen.png");
    status = cairo_surface_set_mime_data (image, CAIRO_MIME_TYPE_JPEG,
					  jpg_data, jpg_len, free);
    if (status) {
	cairo_surface_destroy (image);
	return cairo_test_status_from_status (ctx, status);
    }

    cairo_set_source_surface (cr, image, 0, 0);
    cairo_surface_destroy (image);
    cairo_paint (cr);

    return CAIRO_TEST_SUCCESS;
}

CAIRO_TEST (mime_data,
	    "Check that the mime-data embedding works",
	    "jpeg, api", /* keywords */
	    NULL, /* requirements */
	    10, 10,
	    NULL, draw)
