/*
 * Copyright © 2003 University of Southern California
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of the
 * University of Southern California not be used in advertising or
 * publicity pertaining to distribution of the software without
 * specific, written prior permission. The University of Southern
 * California makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 *
 * THE UNIVERSITY OF SOUTHERN CALIFORNIA DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL THE UNIVERSITY OF
 * SOUTHERN CALIFORNIA BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author: Carl D. Worth <cworth@isi.edu>
 */

#include "cairoint.h"

#include <time.h>
#include <zlib.h>

static const cairo_surface_backend_t cairo_ps_surface_backend;

void
cairo_set_target_ps (cairo_t	*cr,
		     FILE	*file,
		     double	width_inches,
		     double	height_inches,
		     double	x_pixels_per_inch,
		     double	y_pixels_per_inch)
{
    cairo_surface_t *surface;

    surface = cairo_ps_surface_create (file,
				       width_inches, height_inches,
				       x_pixels_per_inch, y_pixels_per_inch);
    if (surface == NULL) {
	cr->status = CAIRO_STATUS_NO_MEMORY;
	return;
    }

    cairo_set_target_surface (cr, surface);

    /* cairo_set_target_surface takes a reference, so we must destroy ours */
    cairo_surface_destroy (surface);
}

typedef struct cairo_ps_surface {
    cairo_surface_t base;

    /* PS-specific fields */
    FILE *file;

    double width_inches;
    double height_inches;
    double x_ppi;
    double y_ppi;

    int pages;

    cairo_image_surface_t *image;
} cairo_ps_surface_t;

static void
_cairo_ps_surface_erase (cairo_ps_surface_t *surface);

cairo_surface_t *
cairo_ps_surface_create (FILE	*file,
			 double	width_inches,
			 double height_inches,
			 double	x_pixels_per_inch,
			 double	y_pixels_per_inch)
{
    cairo_ps_surface_t *surface;
    int width, height;
    time_t now = time (0);

    surface = malloc (sizeof (cairo_ps_surface_t));
    if (surface == NULL)
	return NULL;

    _cairo_surface_init (&surface->base, &cairo_ps_surface_backend);

    surface->file = file;

    surface->width_inches = width_inches;
    surface->height_inches = height_inches;
    surface->x_ppi = x_pixels_per_inch;
    surface->y_ppi = x_pixels_per_inch;

    surface->pages = 0;

    width = (int) (x_pixels_per_inch * width_inches + 1.0);
    height = (int) (y_pixels_per_inch * height_inches + 1.0);

    surface->image = (cairo_image_surface_t *)
	cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);
    if (surface->image == NULL) {
	free (surface);
	return NULL;
    }

    _cairo_ps_surface_erase (surface);

    /* Document header */
    fprintf (file,
	     "%%!PS-Adobe-3.0\n"
	     "%%%%Creator: Cairo (http://cairographics.org)\n");
    fprintf (file,
	     "%%%%CreationDate: %s",
	     ctime (&now));
    fprintf (file,
	     "%%%%Copyright: 2003 Carl Worth and Keith Packard\n");
    fprintf (file,
	     "%%%%BoundingBox: %d %d %d %d\n",
	     0, 0, (int) (surface->width_inches * 72.0), (int) (surface->height_inches * 72.0));
    /* The "/FlateDecode filter" currently used is a feature of LanguageLevel 3 */
    fprintf (file,
	     "%%%%DocumentData: Clean7Bit\n"
	     "%%%%LanguageLevel: 3\n");
    fprintf (file,
	     "%%%%Orientation: Portrait\n"
	     "%%%%EndComments\n");

    return &surface->base;
}

static cairo_surface_t *
_cairo_ps_surface_create_similar (void		*abstract_src,
				 cairo_format_t	format,
				 int		width,
				 int		height)
{
    return NULL;
}

static void
_cairo_ps_surface_destroy (void *abstract_surface)
{
    cairo_ps_surface_t *surface = abstract_surface;

    /* Document footer */
    fprintf (surface->file, "%%%%EOF\n");

    cairo_surface_destroy (&surface->image->base);

    free (surface);
}

static void
_cairo_ps_surface_erase (cairo_ps_surface_t *surface)
{
    cairo_color_t transparent;

    _cairo_color_init (&transparent);
    _cairo_color_set_rgb (&transparent, 0., 0., 0.);
    _cairo_color_set_alpha (&transparent, 0.);
    _cairo_surface_fill_rectangle (&surface->image->base,
				   CAIRO_OPERATOR_SRC,
				   &transparent,
				   0, 0,
				   surface->image->width,
				   surface->image->height);
}

/* XXX: We should re-work this interface to return both X/Y ppi values. */
static double
_cairo_ps_surface_pixels_per_inch (void *abstract_surface)
{
    cairo_ps_surface_t *surface = abstract_surface;

    return surface->y_ppi;
}

static cairo_image_surface_t *
_cairo_ps_surface_get_image (void *abstract_surface)
{
    cairo_ps_surface_t *surface = abstract_surface;

    cairo_surface_reference (&surface->image->base);

    return surface->image;
}

static cairo_status_t
_cairo_ps_surface_set_image (void			*abstract_surface,
			     cairo_image_surface_t	*image)
{
    cairo_ps_surface_t *surface = abstract_surface;

    if (image == surface->image)
	return CAIRO_STATUS_SUCCESS;

    /* XXX: Need to call _cairo_image_surface_set_image here, but it's
       not implemented yet. */

    return CAIRO_INT_STATUS_UNSUPPORTED;
}

static cairo_status_t
_cairo_ps_surface_set_matrix (void		*abstract_surface,
			      cairo_matrix_t	*matrix)
{
    cairo_ps_surface_t *surface = abstract_surface;

    return _cairo_image_surface_set_matrix (surface->image, matrix);
}

static cairo_status_t
_cairo_ps_surface_set_filter (void		*abstract_surface,
			      cairo_filter_t	filter)
{
    cairo_ps_surface_t *surface = abstract_surface;

    return _cairo_image_surface_set_filter (surface->image, filter);
}

static cairo_status_t
_cairo_ps_surface_set_repeat (void		*abstract_surface,
			      int		repeat)
{
    cairo_ps_surface_t *surface = abstract_surface;

    return _cairo_image_surface_set_repeat (surface->image, repeat);
}

static cairo_int_status_t
_cairo_ps_surface_composite (cairo_operator_t	operator,
			     cairo_surface_t	*generic_src,
			     cairo_surface_t	*generic_mask,
			     void		*abstract_dst,
			     int		src_x,
			     int		src_y,
			     int		mask_x,
			     int		mask_y,
			     int		dst_x,
			     int		dst_y,
			     unsigned int	width,
			     unsigned int	height)
{
    return CAIRO_INT_STATUS_UNSUPPORTED;
}

static cairo_int_status_t
_cairo_ps_surface_fill_rectangles (void			*abstract_surface,
				   cairo_operator_t	operator,
				   const cairo_color_t	*color,
				   cairo_rectangle_t	*rects,
				   int			num_rects)
{
    return CAIRO_INT_STATUS_UNSUPPORTED;
}

static cairo_int_status_t
_cairo_ps_surface_composite_trapezoids (cairo_operator_t	operator,
					cairo_surface_t		*generic_src,
					void			*abstract_dst,
					int			x_src,
					int			y_src,
					cairo_trapezoid_t	*traps,
					int			num_traps)
{
    return CAIRO_INT_STATUS_UNSUPPORTED;
}

static cairo_int_status_t
_cairo_ps_surface_copy_page (void *abstract_surface)
{
    cairo_status_t status = CAIRO_STATUS_SUCCESS;
    cairo_ps_surface_t *surface = abstract_surface;
    int width = surface->image->width;
    int height = surface->image->height;
    FILE *file = surface->file;

    int i, x, y;

    cairo_surface_t *white_surface;
    char *rgb, *compressed;
    long rgb_size, compressed_size;

    cairo_color_t white;

    rgb_size = 3 * width * height;
    rgb = malloc (rgb_size);
    if (rgb == NULL) {
	status = CAIRO_STATUS_NO_MEMORY;
	goto BAIL0;
    }

    compressed_size = (int) (1.0 + 1.1 * rgb_size);
    compressed = malloc (compressed_size);
    if (compressed == NULL) {
	status = CAIRO_STATUS_NO_MEMORY;
	goto BAIL1;
    }

    /* PostScript can not represent the alpha channel, so we blend the
       current image over a white RGB surface to eliminate it. */
    white_surface = cairo_image_surface_create (CAIRO_FORMAT_RGB24, 1, 1);
    if (white_surface == NULL) {
	status = CAIRO_STATUS_NO_MEMORY;
	goto BAIL2;
    }

    _cairo_color_init (&white);
    _cairo_surface_fill_rectangle (white_surface,
				   CAIRO_OPERATOR_SRC,
				   &white,
				   0, 0, 1, 1);
    cairo_surface_set_repeat (white_surface, 1);
    _cairo_surface_composite (CAIRO_OPERATOR_OVER_REVERSE,
			      white_surface,
			      NULL,
			      &surface->image->base,
			      0, 0,
			      0, 0,
			      0, 0,
			      width, height);

    i = 0;
    for (y = 0; y < height; y++) {
	pixman_bits_t *pixel = (pixman_bits_t *) (surface->image->data + y * surface->image->stride);
	for (x = 0; x < width; x++, pixel++) {
	    rgb[i++] = (*pixel & 0x00ff0000) >> 16;
	    rgb[i++] = (*pixel & 0x0000ff00) >>  8;
	    rgb[i++] = (*pixel & 0x000000ff) >>  0;
	}
    }

    compress (compressed, &compressed_size, rgb, rgb_size);

    /* Page header */
    fprintf (file, "%%%%Page: %d\n", ++surface->pages);

    fprintf (file, "gsave\n");

    /* Image header goop */
    fprintf (file, "%g %g translate\n", 0.0, surface->height_inches * 72.0);
    fprintf (file, "%g %g scale\n", 72.0 / surface->x_ppi, 72.0 / surface->y_ppi);
    fprintf (file, "/DeviceRGB setcolorspace\n");
    fprintf (file, "<<\n");
    fprintf (file, "	/ImageType 1\n");
    fprintf (file, "	/Width %d\n", width);
    fprintf (file, "	/Height %d\n", height);
    fprintf (file, "	/BitsPerComponent 8\n");
    fprintf (file, "	/Decode [ 0 1 0 1 0 1 ]\n");
    fprintf (file, "	/DataSource currentfile /FlateDecode filter\n");
    fprintf (file, "	/ImageMatrix [ 1 0 0 -1 0 1 ]\n");
    fprintf (file, ">>\n");
    fprintf (file, "image\n");

    /* Compressed image data */
    fwrite (compressed, 1, compressed_size, file);

    fprintf (file, "showpage\n");

    fprintf (file, "grestore\n");

    /* Page footer */
    fprintf (file, "%%%%EndPage\n");

    cairo_surface_destroy (white_surface);
    BAIL2:
    free (compressed);
    BAIL1:
    free (rgb);
    BAIL0:
    return status;
}

static cairo_int_status_t
_cairo_ps_surface_show_page (void *abstract_surface)
{
    cairo_int_status_t status;
    cairo_ps_surface_t *surface = abstract_surface;

    status = _cairo_ps_surface_copy_page (surface);
    if (status)
	return status;

    _cairo_ps_surface_erase (surface);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_ps_surface_set_clip_region (void *abstract_surface,
				   pixman_region16_t *region)
{
    cairo_ps_surface_t *surface = abstract_surface;

    return _cairo_image_surface_set_clip_region (surface->image, region);
}

static cairo_int_status_t
_cairo_ps_surface_create_pattern (void *abstract_surface,
                                  cairo_pattern_t *pattern,
                                  cairo_box_t *extents)
{
    return CAIRO_INT_STATUS_UNSUPPORTED;
}

static const cairo_surface_backend_t cairo_ps_surface_backend = {
    _cairo_ps_surface_create_similar,
    _cairo_ps_surface_destroy,
    _cairo_ps_surface_pixels_per_inch,
    _cairo_ps_surface_get_image,
    _cairo_ps_surface_set_image,
    _cairo_ps_surface_set_matrix,
    _cairo_ps_surface_set_filter,
    _cairo_ps_surface_set_repeat,
    _cairo_ps_surface_composite,
    _cairo_ps_surface_fill_rectangles,
    _cairo_ps_surface_composite_trapezoids,
    _cairo_ps_surface_copy_page,
    _cairo_ps_surface_show_page,
    _cairo_ps_surface_set_clip_region,
    _cairo_ps_surface_create_pattern
};
