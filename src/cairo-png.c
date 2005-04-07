/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2003 University of Southern California
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
 * The Initial Developer of the Original Code is University of Southern
 * California.
 *
 * Contributor(s):
 *	Carl D. Worth <cworth@cworth.org>
 *	Kristian Høgsberg <krh@redhat.com>
 */

#include <png.h>
#include "cairoint.h"
#include "cairo-png.h"


static void
unpremultiply_data (png_structp png, png_row_infop row_info, png_bytep data)
{
    int i;

    for (i = 0; i < row_info->rowbytes; i += 4) {
        unsigned char *b = &data[i];
        unsigned int pixel;
        unsigned char alpha;

	memcpy (&pixel, b, sizeof (unsigned int));
	alpha = (pixel & 0xff000000) >> 24;
        if (alpha == 0) {
	    b[0] = b[1] = b[2] = b[3] = 0;
	} else {
            b[0] = (((pixel & 0x0000ff) >>  0) * 255 + alpha / 2) / alpha;
            b[1] = (((pixel & 0x00ff00) >>  8) * 255 + alpha / 2) / alpha;
            b[2] = (((pixel & 0xff0000) >> 16) * 255 + alpha / 2) / alpha;
	    b[3] = alpha;
	}
    }
}

/**
 * cairo_image_surface_write_png:
 * @surface: a #cairo_surface_t, this must be an image surface
 * @file: a #FILE opened in write mode
 * 
 * Writes the image surface to the given #FILE pointer.  The file
 * should be opened in write mode and binary mode if applicable.
 * 
 * Return value: CAIRO_STATUS_SUCCESS if the PNG file was written
 * successfully.  Otherwise, CAIRO_STATUS_NO_MEMORY is returned if
 * memory could not be allocated for the operation,
 * CAIRO_STATUS_SURFACE_TYPE_MISMATCH if the surface is not an image
 * surface.
 **/
cairo_status_t
cairo_surface_write_png (cairo_surface_t *surface, FILE *file)
{
    int i;
    cairo_status_t status = CAIRO_STATUS_SUCCESS;
    cairo_image_surface_t *image;
    void *image_extra;
    png_struct *png;
    png_info *info;
    png_time pt;
    png_byte **rows;
    png_color_16 white;
    int png_color_type;
    int depth;

    status = _cairo_surface_acquire_source_image (surface,
						  &image,
						  &image_extra);

    if (status != CAIRO_STATUS_SUCCESS)
      return status;

    rows = malloc (image->height * sizeof(png_byte*));
    if (rows == NULL) {
        status = CAIRO_STATUS_NO_MEMORY;
	goto BAIL0;
    }

    for (i = 0; i < image->height; i++)
	rows[i] = (png_byte *) image->data + i * image->stride;

    png = png_create_write_struct (PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (png == NULL) {
	status = CAIRO_STATUS_NO_MEMORY;
	goto BAIL1;
    }

    info = png_create_info_struct (png);
    if (info == NULL) {
	status = CAIRO_STATUS_NO_MEMORY;
	goto BAIL2;
    }

    if (setjmp (png_jmpbuf (png))) {
	status = CAIRO_STATUS_NO_MEMORY;
	goto BAIL2;
    }
    
    png_init_io (png, file);

    switch (image->format) {
    case CAIRO_FORMAT_ARGB32:
	depth = 8;
	png_color_type = PNG_COLOR_TYPE_RGB_ALPHA;
	break;
    case CAIRO_FORMAT_RGB24:
	depth = 8;
	png_color_type = PNG_COLOR_TYPE_RGB;
	break;
    case CAIRO_FORMAT_A8:
	depth = 8;
	png_color_type = PNG_COLOR_TYPE_GRAY;
	break;
    case CAIRO_FORMAT_A1:
	depth = 1;
	png_color_type = PNG_COLOR_TYPE_GRAY;
	break;
    default:
	status = CAIRO_STATUS_NULL_POINTER;
	goto BAIL2;
    }

    png_set_IHDR (png, info,
		  image->width,
		  image->height, depth,
		  png_color_type,
		  PNG_INTERLACE_NONE,
		  PNG_COMPRESSION_TYPE_DEFAULT,
		  PNG_FILTER_TYPE_DEFAULT);

    white.red = 0xff;
    white.blue = 0xff;
    white.green = 0xff;
    png_set_bKGD (png, info, &white);

    png_convert_from_time_t (&pt, time (NULL));
    png_set_tIME (png, info, &pt);

    png_set_write_user_transform_fn (png, unpremultiply_data);
    if (image->format == CAIRO_FORMAT_ARGB32 ||
	image->format == CAIRO_FORMAT_RGB24)
	png_set_bgr (png);
    if (image->format == CAIRO_FORMAT_RGB24)
	png_set_filler (png, 0, PNG_FILLER_AFTER);

    png_write_info (png, info);
    png_write_image (png, rows);
    png_write_end (png, info);

BAIL2:
    png_destroy_write_struct (&png, &info);
BAIL1:
    free (rows);
BAIL0:
    _cairo_surface_release_source_image (surface, image, image_extra);

    return status;
}

static void
premultiply_data (png_structp   png,
                  png_row_infop row_info,
                  png_bytep     data)
{
    int i;

    for (i = 0; i < row_info->rowbytes; i += 4) {
	unsigned char  *base = &data[i];
	unsigned char  blue = base[0];
	unsigned char  green = base[1];
	unsigned char  red = base[2];
	unsigned char  alpha = base[3];
	unsigned long	p;

	red = ((unsigned) red * (unsigned) alpha + 127) / 255;
	green = ((unsigned) green * (unsigned) alpha + 127) / 255;
	blue = ((unsigned) blue * (unsigned) alpha + 127) / 255;
	p = (alpha << 24) | (red << 16) | (green << 8) | (blue << 0);
	memcpy (base, &p, sizeof (unsigned long));
    }
}

/**
 * cairo_image_surface_create_for_png:
 * @file: a #FILE 
 * @width: if not %NULL, the width of the image surface is written to
 *   this address
 * @height: if not %NULL, the height of the image surface is written to
 *   this address
 * 
 * Creates a new image surface and initializes the contents to the
 * given PNG file.  If width or height are not %NULL the dimensions of
 * the image surface will be written to those addresses.
 * 
 * Return value: a new #cairo_surface_t initialized with the contents
 * of the PNG file or %NULL if the file is not a valid PNG file or
 * memory could not be allocated for the operation.
 **/
cairo_surface_t *
cairo_image_surface_create_for_png (FILE *file, int *width, int *height)
{
    cairo_surface_t *surface;
    png_byte *data;
    int i;
#define PNG_SIG_SIZE 8
    unsigned char png_sig[PNG_SIG_SIZE];
    int sig_bytes;
    png_struct *png;
    png_info *info;
    png_uint_32 png_width, png_height, stride;
    int depth, color_type, interlace;
    unsigned int pixel_size;
    png_byte **row_pointers;

    sig_bytes = fread (png_sig, 1, PNG_SIG_SIZE, file);
    if (png_check_sig (png_sig, sig_bytes) == 0)
	goto BAIL1; /* FIXME: ERROR_NOT_PNG */

    /* XXX: Perhaps we'll want some other error handlers? */
    png = png_create_read_struct (PNG_LIBPNG_VER_STRING,
                                  NULL,
                                  NULL,
                                  NULL);
    if (png == NULL)
	goto BAIL1;

    info = png_create_info_struct (png);
    if (info == NULL)
	goto BAIL2;

    png_init_io (png, file);
    png_set_sig_bytes (png, sig_bytes);

    png_read_info (png, info);

    png_get_IHDR (png, info,
                  &png_width, &png_height, &depth,
                  &color_type, &interlace, NULL, NULL);
    stride = 4 * png_width;

    /* convert palette/gray image to rgb */
    if (color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb (png);

    /* expand gray bit depth if needed */
    if (color_type == PNG_COLOR_TYPE_GRAY && depth < 8)
        png_set_gray_1_2_4_to_8 (png);
    /* transform transparency to alpha */
    if (png_get_valid(png, info, PNG_INFO_tRNS))
        png_set_tRNS_to_alpha (png);

    if (depth == 16)
        png_set_strip_16 (png);

    if (depth < 8)
        png_set_packing (png);

    /* convert grayscale to RGB */
    if (color_type == PNG_COLOR_TYPE_GRAY
        || color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb (png);

    if (interlace != PNG_INTERLACE_NONE)
        png_set_interlace_handling (png);

    png_set_bgr (png);
    png_set_filler (png, 0xff, PNG_FILLER_AFTER);

    png_set_read_user_transform_fn (png, premultiply_data);

    png_read_update_info (png, info);

    pixel_size = 4;
    data = malloc (png_width * png_height * pixel_size);
    if (data == NULL)
	goto BAIL2;

    row_pointers = malloc (png_height * sizeof(char *));
    if (row_pointers == NULL)
	goto BAIL3;

    for (i = 0; i < png_height; i++)
        row_pointers[i] = &data[i * png_width * pixel_size];

    png_read_image (png, row_pointers);
    png_read_end (png, info);

    free (row_pointers);
    png_destroy_read_struct (&png, &info, NULL);
    fclose (file);

    if (width != NULL)
	*width = png_width;
    if (height != NULL)
	*height = png_height;

    surface = cairo_image_surface_create_for_data (data,
						   CAIRO_FORMAT_ARGB32,
						   png_width, png_height, stride);
    _cairo_image_surface_assume_ownership_of_data ((cairo_image_surface_t*)surface);

    return surface;

 BAIL3:
    free (data);
 BAIL2:
    png_destroy_read_struct (&png, NULL, NULL);
 BAIL1:
    fclose (file);

    return NULL;
}
#undef PNG_SIG_SIZE
