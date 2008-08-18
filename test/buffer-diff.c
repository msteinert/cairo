/* imagediff - Compare two images
 *
 * Copyright © 2004 Richard D. Worth
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of Richard Worth
 * not be used in advertising or publicity pertaining to distribution
 * of the software without specific, written prior permission.
 * Richard Worth makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 *
 * RICHARD WORTH DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN
 * NO EVENT SHALL RICHARD WORTH BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author: Richard D. Worth <richard@theworths.org> */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <errno.h>
#include <string.h>
#include <pixman.h>

#include "cairo-test.h"

#include "pdiff.h"
#include "buffer-diff.h"
#include "xmalloc.h"

/* Don't allow any differences greater than this value, even if pdiff
 * claims that the images are identical */
#define PERCEPTUAL_DIFF_THRESHOLD 25

static void
xunlink (const cairo_test_context_t *ctx, const char *pathname)
{
    if (unlink (pathname) < 0 && errno != ENOENT) {
	cairo_test_log (ctx, "  Error: Cannot remove %s: %s\n",
			pathname, strerror (errno));
	exit (CAIRO_TEST_FAILURE);
    }
}

/* Compare two buffers, returning the number of pixels that are
 * different and the maximum difference of any single color channel in
 * result_ret.
 *
 * This function should be rewritten to compare all formats supported by
 * cairo_format_t instead of taking a mask as a parameter.
 */
static void
buffer_diff_core (const unsigned char *_buf_a, int stride_a,
		  const unsigned char *_buf_b, int stride_b,
		  unsigned char *_buf_diff, int stride_diff,
		  int		width,
		  int		height,
		  uint32_t mask,
		  buffer_diff_result_t *result_ret)
{
    const uint32_t *buf_a = (const uint32_t*) _buf_a;
    const uint32_t *buf_b = (const uint32_t*) _buf_b;
    uint32_t *buf_diff = (uint32_t*) _buf_diff;
    int x, y;
    buffer_diff_result_t result = {0, 0};

    stride_a /= sizeof (uint32_t);
    stride_b /= sizeof (uint32_t);
    stride_diff /= sizeof (uint32_t);
    for (y = 0; y < height; y++)
    {
	const uint32_t *row_a = buf_a + y * stride_a;
	const uint32_t *row_b = buf_b + y * stride_b;
	uint32_t *row = buf_diff + y * stride_diff;
	for (x = 0; x < width; x++)
	{
	    /* check if the pixels are the same */
	    if ((row_a[x] & mask) != (row_b[x] & mask)) {
		int channel;
		uint32_t diff_pixel = 0;

		/* calculate a difference value for all 4 channels */
		for (channel = 0; channel < 4; channel++) {
		    int value_a = (row_a[x] >> (channel*8)) & 0xff;
		    int value_b = (row_b[x] >> (channel*8)) & 0xff;
		    unsigned int diff;
		    diff = abs (value_a - value_b);
		    if (diff > result.max_diff)
			result.max_diff = diff;
		    diff *= 4;  /* emphasize */
		    if (diff)
		        diff += 128; /* make sure it's visible */
		    if (diff > 255)
		        diff = 255;
		    diff_pixel |= diff << (channel*8);
		}

		result.pixels_changed++;
		row[x] = diff_pixel;
	    } else {
		row[x] = 0;
	    }
	    row[x] |= 0xff000000; /* Set ALPHA to 100% (opaque) */
	}
    }

    *result_ret = result;
}

/* Compares two image surfaces
 *
 * Provides number of pixels changed and maximum single-channel
 * difference in result.
 *
 * Also fills in a "diff" surface intended to visually show where the
 * images differ.
 */
static void
compare_surfaces (const cairo_test_context_t  *ctx,
	          cairo_surface_t	*surface_a,
		  cairo_surface_t	*surface_b,
		  cairo_surface_t	*surface_diff,
		  buffer_diff_result_t	*result)
{
    /* These default values were taken straight from the
     * perceptualdiff program. We'll probably want to tune these as
     * necessary. */
    double gamma = 2.2;
    double luminance = 100.0;
    double field_of_view = 45.0;
    int discernible_pixels_changed;

    /* First, we run cairo's old buffer_diff algorithm which looks for
     * pixel-perfect images, (we do this first since the test suite
     * runs about 3x slower if we run pdiff_compare first).
     */
    buffer_diff_core (cairo_image_surface_get_data (surface_a),
		      cairo_image_surface_get_stride (surface_a),
		      cairo_image_surface_get_data (surface_b),
		      cairo_image_surface_get_stride (surface_b),
		      cairo_image_surface_get_data (surface_diff),
		      cairo_image_surface_get_stride (surface_diff),
		      cairo_image_surface_get_width (surface_a),
		      cairo_image_surface_get_height (surface_a),
		      cairo_surface_get_content (surface_a) & CAIRO_CONTENT_ALPHA ?  0xffffffff : 0x00ffffff,
		      result);
    if (result->pixels_changed == 0)
	return;

    cairo_test_log (ctx,
	            "%d pixels differ (with maximum difference of %d) from reference image\n",
		    result->pixels_changed, result->max_diff);

    /* Then, if there are any different pixels, we give the pdiff code
     * a crack at the images. If it decides that there are no visually
     * discernible differences in any pixels, then we accept this
     * result as good enough.
     *
     * Only let pdiff have a crack at the comparison if the max difference
     * is lower than a threshold, otherwise some problems could be masked.
     */
    if (result->max_diff < PERCEPTUAL_DIFF_THRESHOLD) {
        discernible_pixels_changed = pdiff_compare (surface_a, surface_b,
                                                    gamma, luminance, field_of_view);
        if (discernible_pixels_changed == 0) {
            result->pixels_changed = 0;
            cairo_test_log (ctx,
		            "But perceptual diff finds no visually discernible difference.\n"
                            "Accepting result.\n");
        }
    }
}

void
buffer_diff_noalpha (const unsigned char *buf_a,
		     const unsigned char *buf_b,
		     unsigned char *buf_diff,
		     int	   width,
		     int	   height,
		     int	   stride,
		     buffer_diff_result_t *result)
{
    buffer_diff_core(buf_a, stride,
		     buf_b, stride,
		     buf_diff, stride,
		     width, height,
		     0x00ffffff,
		     result);
}

static cairo_status_t
stdio_write_func (void *closure, const unsigned char *data, unsigned int length)
{
    FILE *file = closure;

    if (fwrite (data, 1, length, file) != length)
	return CAIRO_STATUS_WRITE_ERROR;

    return CAIRO_STATUS_SUCCESS;
}

/* Flatten an ARGB surface by blending it over white. The resulting
 * surface, (still in ARGB32 format, but with only alpha==1.0
 * everywhere) is returned in the same surface pointer.
 *
 * The original surface will be destroyed.
 *
 * The (x,y) value specify an origin of interest for the original
 * image. The flattened image will be generated only from the box
 * extending from (x,y) to (width,height).
 */
static void
flatten_surface (const cairo_test_context_t *ctx,
	         cairo_surface_t **surface,
		 int x, int y)
{
    cairo_surface_t *flat;
    cairo_t *cr;

    flat = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
				       cairo_image_surface_get_width (*surface) - x,
				       cairo_image_surface_get_height (*surface) - y);
    cairo_surface_set_device_offset (flat, -x, -y);

    cr = cairo_create (flat);
    cairo_surface_destroy (flat);

    cairo_set_source_rgb (cr, 1, 1, 1);
    cairo_paint (cr);

    cairo_set_source_surface (cr, *surface, 0, 0);
    cairo_surface_destroy (*surface);
    cairo_paint (cr);

    *surface = cairo_surface_reference (cairo_get_target (cr));
    cairo_destroy (cr);
}

/* Given an image surface, create a new surface that has the same
 * contents as the sub-surface with its origin at x,y.
 *
 * The original surface will be destroyed.
 */
static void
extract_sub_surface (const cairo_test_context_t *ctx,
	             cairo_surface_t **surface,
		     int x, int y)
{
    cairo_surface_t *sub;
    cairo_t *cr;

    sub = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
				      cairo_image_surface_get_width (*surface) - x,
				      cairo_image_surface_get_height (*surface) - y);

    /* We don't use a device offset like flatten_surface. That's not
     * for any important reason, (the results should be
     * identical). This style just seemed more natural to me this
     * time, so I'm leaving both here so I can look at both to see
     * which I like better. */
    cr = cairo_create (sub);
    cairo_surface_destroy (sub);

    cairo_set_source_surface (cr, *surface, -x, -y);
    cairo_surface_destroy (*surface);
    cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
    cairo_paint (cr);

    *surface = cairo_surface_reference (cairo_get_target (cr));
    cairo_destroy (cr);
}

static cairo_bool_t
same_size (cairo_surface_t *a, cairo_surface_t *b)
{
    unsigned int width_a, height_a;
    unsigned int width_b, height_b;

    width_a = cairo_image_surface_get_width (a);
    height_a = cairo_image_surface_get_height (a);

    width_b = cairo_image_surface_get_width (b);
    height_b = cairo_image_surface_get_height (b);

    return width_a == width_b && height_a == height_b;
}

/* Image comparison code courtesy of Richard Worth <richard@theworths.org>
 * Returns number of pixels changed, (or -1 on error).
 * Also saves a "diff" image intended to visually show where the
 * images differ.
 *
 * The return value simply indicates whether a check was successfully
 * made, (as opposed to a file-not-found condition or similar). It
 * does not indicate anything about how much the images differ. For
 * that, see result.
 *
 * One failure mode is if the two images provided do not have the same
 * dimensions. In this case, this function will return
 * CAIRO_STATUS_SURFACE_TYPE_MISMATCH (which is a bit of an abuse, but
 * oh well).
 */
cairo_status_t
image_diff (const cairo_test_context_t *ctx,
	    cairo_surface_t *surface_a,
	    cairo_surface_t *surface_b,
	    cairo_surface_t *surface_diff,
	    buffer_diff_result_t *result)
{
    if (cairo_surface_status (surface_a))
	return cairo_surface_status (surface_a);

    if (cairo_surface_status (surface_b))
	return cairo_surface_status (surface_b);

    if (cairo_surface_status (surface_diff))
	return cairo_surface_status (surface_diff);

    if (! same_size (surface_a, surface_b) ||
	! same_size (surface_a, surface_diff))
    {
	cairo_test_log (ctx, "Error: Image size mismatch\n");
	return CAIRO_STATUS_SURFACE_TYPE_MISMATCH;
    }

    compare_surfaces (ctx, surface_a, surface_b, surface_diff, result);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
write_png (cairo_surface_t *surface, const char *filename)
{
    cairo_status_t status;
    FILE *png_file;

    if (filename != NULL) {
	png_file = fopen (filename, "wb");
	if (png_file == NULL) {
	    switch (errno) {
	    case ENOMEM:
		return CAIRO_STATUS_NO_MEMORY;
	    default:
		return CAIRO_STATUS_WRITE_ERROR;
	    }
	}
    } else
	png_file = stdout;

    status = cairo_surface_write_to_png_stream (surface,
						stdio_write_func,
						png_file);

    if (png_file != stdout)
	fclose (png_file);

    return status;
}



cairo_status_t
png_diff (const cairo_test_context_t *ctx,
	  const char *filename_a,
	  const char *filename_b,
	  const char *filename_diff,
	  int		ax,
	  int		ay,
	  int		bx,
	  int		by,
	  buffer_diff_result_t *result)
{
    cairo_surface_t *surface_a;
    cairo_surface_t *surface_b;
    cairo_surface_t *surface_diff;
    cairo_status_t status;

    surface_a = cairo_image_surface_create_from_png (filename_a);
    status = cairo_surface_status (surface_a);
    if (status) {
	cairo_test_log (ctx, "Error: Failed to create surface from %s: %s\n",
			filename_a, cairo_status_to_string (status));
	return status;
    }

    surface_b = cairo_image_surface_create_from_png (filename_b);
    status = cairo_surface_status (surface_b);
    if (status) {
	cairo_test_log (ctx, "Error: Failed to create surface from %s: %s\n",
			filename_b, cairo_status_to_string (status));
	cairo_surface_destroy (surface_a);
	return status;
    }

    if (ax || ay) {
	extract_sub_surface (ctx, &surface_a, ax, ay);
	ax = ay = 0;
    }

    if (bx || by) {
	extract_sub_surface (ctx, &surface_b, bx, by);
	bx = by = 0;
    }

    status = cairo_surface_status (surface_a);
    if (status) {
	cairo_test_log (ctx, "Error: Failed to extract surface from %s: %s\n",
			filename_a, cairo_status_to_string (status));
	cairo_surface_destroy (surface_a);
	cairo_surface_destroy (surface_b);
	return status;
    }
    status = cairo_surface_status (surface_b);
    if (status) {
	cairo_test_log (ctx, "Error: Failed to extract surface from %s: %s\n",
			filename_b, cairo_status_to_string (status));
	cairo_surface_destroy (surface_a);
	cairo_surface_destroy (surface_b);
	return status;
    }

    surface_diff = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
					       cairo_image_surface_get_width (surface_a),
					       cairo_image_surface_get_height (surface_a));
    status = cairo_surface_status (surface_diff);
    if (status) {
	cairo_test_log (ctx, "Error: Failed to allocate surface to hold differences\n");
	cairo_surface_destroy (surface_a);
	cairo_surface_destroy (surface_b);
	return CAIRO_STATUS_NO_MEMORY;
    }

    status = image_diff (ctx,
			 surface_a, surface_b, surface_diff,
			 result);

    cairo_surface_destroy (surface_a);
    cairo_surface_destroy (surface_b);
    cairo_surface_destroy (surface_diff);

    xunlink (ctx, filename_diff);
    if (status == CAIRO_STATUS_SUCCESS &&
	result->pixels_changed)
    {
	status = write_png (surface_diff, filename_diff);
    }


    return status;
}

cairo_status_t
png_diff_flattened (const cairo_test_context_t *ctx,
		    const char *filename_a,
		    const char *filename_b,
		    const char *filename_diff,
		    int	  ax,
		    int	  ay,
		    int	  bx,
		    int	  by,
		    buffer_diff_result_t *result)
{
    cairo_surface_t *surface_a;
    cairo_surface_t *surface_b;
    cairo_surface_t *surface_diff;
    cairo_status_t status;

    surface_a = cairo_image_surface_create_from_png (filename_a);
    status = cairo_surface_status (surface_a);
    if (status) {
	cairo_test_log (ctx, "Error: Failed to create surface from %s: %s\n",
			filename_a, cairo_status_to_string (status));
	return status;
    }

    surface_b = cairo_image_surface_create_from_png (filename_b);
    status = cairo_surface_status (surface_b);
    if (status) {
	cairo_test_log (ctx, "Error: Failed to create surface from %s: %s\n",
			filename_b, cairo_status_to_string (status));
	cairo_surface_destroy (surface_a);
	return status;
    }

    flatten_surface (ctx, &surface_a, ax, ay);
    flatten_surface (ctx, &surface_b, bx, by);

    status = cairo_surface_status (surface_a);
    if (status) {
	cairo_test_log (ctx, "Error: Failed to extract surface from %s: %s\n",
			filename_a, cairo_status_to_string (status));
	cairo_surface_destroy (surface_a);
	cairo_surface_destroy (surface_b);
	return status;
    }
    status = cairo_surface_status (surface_b);
    if (status) {
	cairo_test_log (ctx, "Error: Failed to extract surface from %s: %s\n",
			filename_b, cairo_status_to_string (status));
	cairo_surface_destroy (surface_a);
	cairo_surface_destroy (surface_b);
	return status;
    }

    surface_diff = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
					       cairo_image_surface_get_width (surface_a),
					       cairo_image_surface_get_height (surface_a));
    status = cairo_surface_status (surface_diff);
    if (status) {
	cairo_test_log (ctx, "Error: Failed to allocate surface to hold differences\n");
	cairo_surface_destroy (surface_a);
	cairo_surface_destroy (surface_b);
	return CAIRO_STATUS_NO_MEMORY;
    }

    status = image_diff (ctx,
			 surface_a, surface_b, surface_diff,
			 result);

    xunlink (ctx, filename_diff);
    if (status == CAIRO_STATUS_SUCCESS &&
	result->pixels_changed)
    {
	status = write_png (surface_diff, filename_diff);
    }

    cairo_surface_destroy (surface_a);
    cairo_surface_destroy (surface_b);
    cairo_surface_destroy (surface_diff);

    return status;
}
