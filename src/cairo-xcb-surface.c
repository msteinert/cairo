/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2002 University of Southern California
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
 */

#include "cairoint.h"
#include "cairo-xcb.h"
#include "cairo-xcb-xrender.h"
#include <xcb/xcb_renderutil.h>

#define AllPlanes               ((unsigned long)~0L)

static cairo_content_t
_xcb_render_format_to_content (xcb_render_pictforminfo_t *xrender_format)
{
    cairo_bool_t xrender_format_has_alpha;
    cairo_bool_t xrender_format_has_color;

    /* This only happens when using a non-Render server. Let's punt
     * and say there's no alpha here. */
    if (xrender_format == NULL)
	return CAIRO_CONTENT_COLOR;

    xrender_format_has_alpha = (xrender_format->direct.alpha_mask != 0);
    xrender_format_has_color = (xrender_format->direct.red_mask   != 0 ||
				xrender_format->direct.green_mask != 0 ||
				xrender_format->direct.blue_mask  != 0);

    if (xrender_format_has_alpha)
	if (xrender_format_has_color)
	    return CAIRO_CONTENT_COLOR_ALPHA;
	else
	    return CAIRO_CONTENT_ALPHA;
    else
	return CAIRO_CONTENT_COLOR;

}

/*
 * Instead of taking two round trips for each blending request,
 * assume that if a particular drawable fails GetImage that it will
 * fail for a "while"; use temporary pixmaps to avoid the errors
 */

#define CAIRO_ASSUME_PIXMAP	20

typedef struct cairo_xcb_surface {
    cairo_surface_t base;

    xcb_connection_t *dpy;
    xcb_screen_t *screen;

    xcb_gcontext_t gc;
    xcb_drawable_t drawable;
    int owns_pixmap;
    xcb_visualtype_t *visual;

    int use_pixmap;

    int render_major;
    int render_minor;

    int width;
    int height;
    int depth;

    xcb_rectangle_t *clip_rects;
    int num_clip_rects;

    xcb_render_picture_t picture;
    xcb_render_pictforminfo_t format;
    int has_format;
} cairo_xcb_surface_t;

#define CAIRO_SURFACE_RENDER_AT_LEAST(surface, major, minor)	\
	(((surface)->render_major > major) ||			\
	 (((surface)->render_major == major) && ((surface)->render_minor >= minor)))

#define CAIRO_SURFACE_RENDER_HAS_CREATE_PICTURE(surface)		CAIRO_SURFACE_RENDER_AT_LEAST((surface), 0, 0)
#define CAIRO_SURFACE_RENDER_HAS_COMPOSITE(surface)		CAIRO_SURFACE_RENDER_AT_LEAST((surface), 0, 0)

#define CAIRO_SURFACE_RENDER_HAS_FILL_RECTANGLE(surface)		CAIRO_SURFACE_RENDER_AT_LEAST((surface), 0, 1)
#define CAIRO_SURFACE_RENDER_HAS_FILL_RECTANGLES(surface)		CAIRO_SURFACE_RENDER_AT_LEAST((surface), 0, 1)

#define CAIRO_SURFACE_RENDER_HAS_DISJOINT(surface)			CAIRO_SURFACE_RENDER_AT_LEAST((surface), 0, 2)
#define CAIRO_SURFACE_RENDER_HAS_CONJOINT(surface)			CAIRO_SURFACE_RENDER_AT_LEAST((surface), 0, 2)

#define CAIRO_SURFACE_RENDER_HAS_TRAPEZOIDS(surface)		CAIRO_SURFACE_RENDER_AT_LEAST((surface), 0, 4)
#define CAIRO_SURFACE_RENDER_HAS_TRIANGLES(surface)		CAIRO_SURFACE_RENDER_AT_LEAST((surface), 0, 4)
#define CAIRO_SURFACE_RENDER_HAS_TRISTRIP(surface)			CAIRO_SURFACE_RENDER_AT_LEAST((surface), 0, 4)
#define CAIRO_SURFACE_RENDER_HAS_TRIFAN(surface)			CAIRO_SURFACE_RENDER_AT_LEAST((surface), 0, 4)

#define CAIRO_SURFACE_RENDER_HAS_PICTURE_TRANSFORM(surface)	CAIRO_SURFACE_RENDER_AT_LEAST((surface), 0, 6)
#define CAIRO_SURFACE_RENDER_HAS_FILTERS(surface)	CAIRO_SURFACE_RENDER_AT_LEAST((surface), 0, 6)

static void
_cairo_xcb_surface_ensure_gc (cairo_xcb_surface_t *surface);

static int
_CAIRO_FORMAT_DEPTH (cairo_format_t format)
{
    switch (format) {
    case CAIRO_FORMAT_A1:
	return 1;
    case CAIRO_FORMAT_A8:
	return 8;
    case CAIRO_FORMAT_RGB24:
	return 24;
    case CAIRO_FORMAT_ARGB32:
    default:
	return 32;
    }
}

static cairo_surface_t *
_cairo_xcb_surface_create_similar (void		       *abstract_src,
				   cairo_content_t	content,
				   int			width,
				   int			height)
{
    cairo_xcb_surface_t *src = abstract_src;
    xcb_connection_t *dpy = src->dpy;
    xcb_pixmap_t pixmap;
    cairo_xcb_surface_t *surface;
    cairo_format_t format = _cairo_format_from_content (content);
    xcb_render_pictforminfo_t *xrender_format;

    /* As a good first approximation, if the display doesn't have COMPOSITE,
     * we're better off using image surfaces for all temporary operations
     */
    if (!CAIRO_SURFACE_RENDER_HAS_COMPOSITE (src)) {
	return cairo_image_surface_create (format, width, height);
    }

    pixmap = xcb_generate_id (dpy);
    xcb_create_pixmap (dpy, _CAIRO_FORMAT_DEPTH (format),
		     pixmap, src->drawable,
		     width <= 0 ? 1 : width,
		     height <= 0 ? 1 : height);

    xrender_format = xcb_render_util_find_standard_format (xcb_render_util_query_formats (dpy), format);
    /* XXX: what to do if xrender_format is null? */
    surface = (cairo_xcb_surface_t *)
	cairo_xcb_surface_create_with_xrender_format (dpy, pixmap, src->screen,
						      xrender_format,
						      width, height);
    if (surface->base.status) {
	_cairo_error (CAIRO_STATUS_NO_MEMORY);
	return (cairo_surface_t*) &_cairo_surface_nil;
    }

    surface->owns_pixmap = TRUE;

    return &surface->base;
}

static cairo_status_t
_cairo_xcb_surface_finish (void *abstract_surface)
{
    cairo_xcb_surface_t *surface = abstract_surface;
    if (surface->picture)
	xcb_render_free_picture (surface->dpy, surface->picture);

    if (surface->owns_pixmap)
	xcb_free_pixmap (surface->dpy, surface->drawable);

    if (surface->gc)
	xcb_free_gc (surface->dpy, surface->gc);

    free (surface->clip_rects);

    surface->dpy = NULL;

    return CAIRO_STATUS_SUCCESS;
}

static int
_bits_per_pixel(xcb_connection_t *c, int depth)
{
    xcb_format_t *fmt = xcb_setup_pixmap_formats(xcb_get_setup(c));
    xcb_format_t *fmtend = fmt + xcb_setup_pixmap_formats_length(xcb_get_setup(c));

    for(; fmt != fmtend; ++fmt)
	if(fmt->depth == depth)
	    return fmt->bits_per_pixel;

    if(depth <= 4)
	return 4;
    if(depth <= 8)
	return 8;
    if(depth <= 16)
	return 16;
    return 32;
}

static int
_bytes_per_line(xcb_connection_t *c, int width, int bpp)
{
    int bitmap_pad = xcb_get_setup(c)->bitmap_format_scanline_pad;
    return ((bpp * width + bitmap_pad - 1) & -bitmap_pad) >> 3;
}

static cairo_bool_t
_CAIRO_MASK_FORMAT (cairo_format_masks_t *masks, cairo_format_t *format)
{
    switch (masks->bpp) {
    case 32:
	if (masks->alpha_mask == 0xff000000 &&
	    masks->red_mask == 0x00ff0000 &&
	    masks->green_mask == 0x0000ff00 &&
	    masks->blue_mask == 0x000000ff)
	{
	    *format = CAIRO_FORMAT_ARGB32;
	    return TRUE;
	}
	if (masks->alpha_mask == 0x00000000 &&
	    masks->red_mask == 0x00ff0000 &&
	    masks->green_mask == 0x0000ff00 &&
	    masks->blue_mask == 0x000000ff)
	{
	    *format = CAIRO_FORMAT_RGB24;
	    return TRUE;
	}
	break;
    case 8:
	if (masks->alpha_mask == 0xff)
	{
	    *format = CAIRO_FORMAT_A8;
	    return TRUE;
	}
	break;
    case 1:
	if (masks->alpha_mask == 0x1)
	{
	    *format = CAIRO_FORMAT_A1;
	    return TRUE;
	}
	break;
    }
    return FALSE;
}

static cairo_status_t
_get_image_surface (cairo_xcb_surface_t     *surface,
		    cairo_rectangle_int16_t *interest_rect,
		    cairo_image_surface_t  **image_out,
		    cairo_rectangle_int16_t *image_rect)
{
    cairo_image_surface_t *image;
    xcb_get_image_reply_t *imagerep;
    int bpp, bytes_per_line;
    int x1, y1, x2, y2;
    unsigned char *data;
    cairo_format_t format;
    cairo_format_masks_t masks;

    x1 = 0;
    y1 = 0;
    x2 = surface->width;
    y2 = surface->height;

    if (interest_rect) {
	cairo_rectangle_int16_t rect;

	rect.x = interest_rect->x;
	rect.y = interest_rect->y;
	rect.width = interest_rect->width;
	rect.height = interest_rect->height;

	if (rect.x > x1)
	    x1 = rect.x;
	if (rect.y > y1)
	    y1 = rect.y;
	if (rect.x + rect.width < x2)
	    x2 = rect.x + rect.width;
	if (rect.y + rect.height < y2)
	    y2 = rect.y + rect.height;

	if (x1 >= x2 || y1 >= y2) {
	    *image_out = NULL;
	    return CAIRO_STATUS_SUCCESS;
	}
    }

    if (image_rect) {
	image_rect->x = x1;
	image_rect->y = y1;
	image_rect->width = x2 - x1;
	image_rect->height = y2 - y1;
    }

    /* XXX: This should try to use the XShm extension if available */

    if (surface->use_pixmap == 0)
    {
	xcb_generic_error_t *error;
	imagerep = xcb_get_image_reply(surface->dpy,
				    xcb_get_image(surface->dpy, XCB_IMAGE_FORMAT_Z_PIXMAP,
						surface->drawable,
						x1, y1,
						x2 - x1, y2 - y1,
						AllPlanes), &error);

	/* If we get an error, the surface must have been a window,
	 * so retry with the safe code path.
	 */
	if (error)
	    surface->use_pixmap = CAIRO_ASSUME_PIXMAP;
    }
    else
    {
	surface->use_pixmap--;
	imagerep = NULL;
    }

    if (!imagerep)
    {
	/* xcb_get_image_t from a window is dangerous because it can
	 * produce errors if the window is unmapped or partially
	 * outside the screen. We could check for errors and
	 * retry, but to keep things simple, we just create a
	 * temporary pixmap
	 */
	xcb_pixmap_t pixmap;
	pixmap = xcb_generate_id (surface->dpy);
	xcb_create_pixmap (surface->dpy,
			 surface->depth,
			 pixmap,
			 surface->drawable,
			 x2 - x1, y2 - y1);
	_cairo_xcb_surface_ensure_gc (surface);

	xcb_copy_area (surface->dpy, surface->drawable, pixmap, surface->gc,
		     x1, y1, 0, 0, x2 - x1, y2 - y1);

	imagerep = xcb_get_image_reply(surface->dpy,
				    xcb_get_image(surface->dpy, XCB_IMAGE_FORMAT_Z_PIXMAP,
						pixmap,
						x1, y1,
						x2 - x1, y2 - y1,
						AllPlanes), 0);
	xcb_free_pixmap (surface->dpy, pixmap);

    }
    if (!imagerep)
	return CAIRO_STATUS_NO_MEMORY;

    bpp = _bits_per_pixel(surface->dpy, imagerep->depth);
    bytes_per_line = _bytes_per_line(surface->dpy, surface->width, bpp);

    data = malloc (bytes_per_line * surface->height);
    if (data == NULL) {
	free (imagerep);
	return CAIRO_STATUS_NO_MEMORY;
    }

    memcpy (data, xcb_get_image_data (imagerep), bytes_per_line * surface->height);
    free (imagerep);

    /*
     * Compute the pixel format masks from either an xcb_visualtype_t or
     * a xcb_render_pctforminfo_t, failing we assume the drawable is an
     * alpha-only pixmap as it could only have been created that way
     * through the cairo_xlib_surface_create_for_bitmap function.
     */
    if (surface->visual) {
	masks.bpp = bpp;
	masks.alpha_mask = 0;
	masks.red_mask = surface->visual->red_mask;
	masks.green_mask = surface->visual->green_mask;
	masks.blue_mask = surface->visual->blue_mask;
    } else if (surface->has_format) {
	masks.bpp = bpp;
	masks.red_mask = (unsigned long)surface->format.direct.red_mask << surface->format.direct.red_shift;
	masks.green_mask = (unsigned long)surface->format.direct.green_mask << surface->format.direct.green_shift;
	masks.blue_mask = (unsigned long)surface->format.direct.blue_mask << surface->format.direct.blue_shift;
	masks.alpha_mask = (unsigned long)surface->format.direct.alpha_mask << surface->format.direct.alpha_shift;
    } else {
	masks.bpp = bpp;
	masks.red_mask = 0;
	masks.green_mask = 0;
	masks.blue_mask = 0;
	if (surface->depth < 32)
	    masks.alpha_mask = (1 << surface->depth) - 1;
	else
	    masks.alpha_mask = 0xffffffff;
    }

    /*
     * Prefer to use a standard pixman format instead of the
     * general masks case.
     */
    if (_CAIRO_MASK_FORMAT (&masks, &format)) {
	image = (cairo_image_surface_t *)
	    cairo_image_surface_create_for_data (data,
						 format,
						 x2 - x1,
						 y2 - y1,
						 bytes_per_line);
	if (image->base.status)
	    goto FAIL;
    } else {
	/*
	 * XXX This can't work.  We must convert the data to one of the
	 * supported pixman formats.  Pixman needs another function
	 * which takes data in an arbitrary format and converts it
	 * to something supported by that library.
	 */
	image = (cairo_image_surface_t *)
	    _cairo_image_surface_create_with_masks (data,
						    &masks,
						    x2 - x1,
						    y2 - y1,
						    bytes_per_line);
	if (image->base.status)
	    goto FAIL;
    }

    /* Let the surface take ownership of the data */
    _cairo_image_surface_assume_ownership_of_data (image);

    *image_out = image;
    return CAIRO_STATUS_SUCCESS;

 FAIL:
    free (data);
    return CAIRO_STATUS_NO_MEMORY;
}

static void
_cairo_xcb_surface_set_picture_clip_rects (cairo_xcb_surface_t *surface)
{
    if (surface->num_clip_rects)
	xcb_render_set_picture_clip_rectangles (surface->dpy, surface->picture,
					   0, 0,
					   surface->num_clip_rects,
					   surface->clip_rects);
}

static void
_cairo_xcb_surface_set_gc_clip_rects (cairo_xcb_surface_t *surface)
{
    if (surface->num_clip_rects)
	xcb_set_clip_rectangles(surface->dpy, XCB_CLIP_ORDERING_YX_SORTED, surface->gc,
			     0, 0,
			     surface->num_clip_rects,
			     surface->clip_rects );
}

static void
_cairo_xcb_surface_ensure_gc (cairo_xcb_surface_t *surface)
{
    if (surface->gc)
	return;

    surface->gc = xcb_generate_id(surface->dpy);
    xcb_create_gc (surface->dpy, surface->gc, surface->drawable, 0, 0);
    _cairo_xcb_surface_set_gc_clip_rects(surface);
}

static cairo_status_t
_draw_image_surface (cairo_xcb_surface_t    *surface,
		     cairo_image_surface_t  *image,
		     int                    dst_x,
		     int                    dst_y)
{
    int bpp, data_len;

    _cairo_xcb_surface_ensure_gc (surface);
    bpp = _bits_per_pixel(surface->dpy, image->depth);
    data_len = _bytes_per_line(surface->dpy, image->width, bpp) * image->height;
    xcb_put_image(surface->dpy, XCB_IMAGE_FORMAT_Z_PIXMAP, surface->drawable, surface->gc,
	      image->width,
	      image->height,
	      dst_x, dst_y,
	      /* left_pad */ 0, image->depth,
	      data_len, image->data);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_xcb_surface_acquire_source_image (void                    *abstract_surface,
					 cairo_image_surface_t  **image_out,
					 void                   **image_extra)
{
    cairo_xcb_surface_t *surface = abstract_surface;
    cairo_image_surface_t *image;
    cairo_status_t status;

    status = _get_image_surface (surface, NULL, &image, NULL);
    if (status)
	return status;

    *image_out = image;
    *image_extra = NULL;

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_xcb_surface_release_source_image (void                   *abstract_surface,
					 cairo_image_surface_t  *image,
					 void                   *image_extra)
{
    cairo_surface_destroy (&image->base);
}

static cairo_status_t
_cairo_xcb_surface_acquire_dest_image (void                    *abstract_surface,
				       cairo_rectangle_int16_t *interest_rect,
				       cairo_image_surface_t  **image_out,
				       cairo_rectangle_int16_t *image_rect_out,
				       void                   **image_extra)
{
    cairo_xcb_surface_t *surface = abstract_surface;
    cairo_image_surface_t *image;
    cairo_status_t status;

    status = _get_image_surface (surface, interest_rect, &image, image_rect_out);
    if (status)
	return status;

    *image_out = image;
    *image_extra = NULL;

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_xcb_surface_release_dest_image (void                   *abstract_surface,
				       cairo_rectangle_int16_t      *interest_rect,
				       cairo_image_surface_t  *image,
				       cairo_rectangle_int16_t      *image_rect,
				       void                   *image_extra)
{
    cairo_xcb_surface_t *surface = abstract_surface;

    /* ignore errors */
    _draw_image_surface (surface, image, image_rect->x, image_rect->y);

    cairo_surface_destroy (&image->base);
}

/*
 * Return whether two xcb surfaces share the same
 * screen.  Both core and Render drawing require this
 * when using multiple drawables in an operation.
 */
static cairo_bool_t
_cairo_xcb_surface_same_screen (cairo_xcb_surface_t *dst,
				cairo_xcb_surface_t *src)
{
    return dst->dpy == src->dpy && dst->screen == src->screen;
}

static cairo_status_t
_cairo_xcb_surface_clone_similar (void			*abstract_surface,
				  cairo_surface_t	*src,
				  cairo_surface_t     **clone_out)
{
    cairo_xcb_surface_t *surface = abstract_surface;
    cairo_xcb_surface_t *clone;

    if (src->backend == surface->base.backend ) {
	cairo_xcb_surface_t *xcb_src = (cairo_xcb_surface_t *)src;

	if (_cairo_xcb_surface_same_screen(surface, xcb_src)) {
	    *clone_out = cairo_surface_reference (src);

	    return CAIRO_STATUS_SUCCESS;
	}
    } else if (_cairo_surface_is_image (src)) {
	cairo_image_surface_t *image_src = (cairo_image_surface_t *)src;
	cairo_content_t content = _cairo_content_from_format (image_src->format);

	if (surface->base.status)
	    return surface->base.status;

	clone = (cairo_xcb_surface_t *)
	    _cairo_xcb_surface_create_similar (surface, content,
					       image_src->width, image_src->height);
	if (clone->base.status)
	    return CAIRO_STATUS_NO_MEMORY;

	_draw_image_surface (clone, image_src, 0, 0);

	*clone_out = &clone->base;

	return CAIRO_STATUS_SUCCESS;
    }

    return CAIRO_INT_STATUS_UNSUPPORTED;
}

static cairo_status_t
_cairo_xcb_surface_set_matrix (cairo_xcb_surface_t *surface,
			       cairo_matrix_t	   *matrix)
{
    xcb_render_transform_t xtransform;

    if (!surface->picture)
	return CAIRO_STATUS_SUCCESS;

    xtransform.matrix11 = _cairo_fixed_from_double (matrix->xx);
    xtransform.matrix12 = _cairo_fixed_from_double (matrix->xy);
    xtransform.matrix13 = _cairo_fixed_from_double (matrix->x0);

    xtransform.matrix21 = _cairo_fixed_from_double (matrix->yx);
    xtransform.matrix22 = _cairo_fixed_from_double (matrix->yy);
    xtransform.matrix23 = _cairo_fixed_from_double (matrix->y0);

    xtransform.matrix31 = 0;
    xtransform.matrix32 = 0;
    xtransform.matrix33 = _cairo_fixed_from_double (1);

    if (!CAIRO_SURFACE_RENDER_HAS_PICTURE_TRANSFORM (surface))
    {
	static const xcb_render_transform_t identity = {
	    1 << 16, 0x00000, 0x00000,
	    0x00000, 1 << 16, 0x00000,
	    0x00000, 0x00000, 1 << 16
	};

	if (memcmp (&xtransform, &identity, sizeof (xcb_render_transform_t)) == 0)
	    return CAIRO_STATUS_SUCCESS;

	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    xcb_render_set_picture_transform (surface->dpy, surface->picture, xtransform);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_xcb_surface_set_filter (cairo_xcb_surface_t *surface,
			       cairo_filter_t	   filter)
{
    const char *render_filter;

    if (!surface->picture)
	return CAIRO_STATUS_SUCCESS;

    if (!CAIRO_SURFACE_RENDER_HAS_FILTERS (surface))
    {
	if (filter == CAIRO_FILTER_FAST || filter == CAIRO_FILTER_NEAREST)
	    return CAIRO_STATUS_SUCCESS;

	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    switch (filter) {
    case CAIRO_FILTER_FAST:
	render_filter = "fast";
	break;
    case CAIRO_FILTER_GOOD:
	render_filter = "good";
	break;
    case CAIRO_FILTER_BEST:
	render_filter = "best";
	break;
    case CAIRO_FILTER_NEAREST:
	render_filter = "nearest";
	break;
    case CAIRO_FILTER_BILINEAR:
	render_filter = "bilinear";
	break;
    case CAIRO_FILTER_GAUSSIAN:
    default:
	render_filter = "best";
	break;
    }

    xcb_render_set_picture_filter(surface->dpy, surface->picture,
			     strlen(render_filter), render_filter, 0, NULL);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_xcb_surface_set_repeat (cairo_xcb_surface_t *surface, int repeat)
{
    uint32_t mask = XCB_RENDER_CP_REPEAT;
    uint32_t pa[] = { repeat };

    if (!surface->picture)
	return CAIRO_STATUS_SUCCESS;

    xcb_render_change_picture (surface->dpy, surface->picture, mask, pa);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_xcb_surface_set_attributes (cairo_xcb_surface_t	      *surface,
				   cairo_surface_attributes_t *attributes)
{
    cairo_int_status_t status;

    status = _cairo_xcb_surface_set_matrix (surface, &attributes->matrix);
    if (status)
	return status;

    switch (attributes->extend) {
    case CAIRO_EXTEND_NONE:
	_cairo_xcb_surface_set_repeat (surface, 0);
	break;
    case CAIRO_EXTEND_REPEAT:
	_cairo_xcb_surface_set_repeat (surface, 1);
	break;
    case CAIRO_EXTEND_REFLECT:
	return CAIRO_INT_STATUS_UNSUPPORTED;
    case CAIRO_EXTEND_PAD:
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    status = _cairo_xcb_surface_set_filter (surface, attributes->filter);
    if (status)
	return status;

    return CAIRO_STATUS_SUCCESS;
}

static int
_render_operator (cairo_operator_t op)
{
    switch (op) {
    case CAIRO_OPERATOR_CLEAR:
	return XCB_RENDER_PICT_OP_CLEAR;
    case CAIRO_OPERATOR_SOURCE:
	return XCB_RENDER_PICT_OP_SRC;
    case CAIRO_OPERATOR_DEST:
	return XCB_RENDER_PICT_OP_DST;
    case CAIRO_OPERATOR_OVER:
	return XCB_RENDER_PICT_OP_OVER;
    case CAIRO_OPERATOR_DEST_OVER:
	return XCB_RENDER_PICT_OP_OVER_REVERSE;
    case CAIRO_OPERATOR_IN:
	return XCB_RENDER_PICT_OP_IN;
    case CAIRO_OPERATOR_DEST_IN:
	return XCB_RENDER_PICT_OP_IN_REVERSE;
    case CAIRO_OPERATOR_OUT:
	return XCB_RENDER_PICT_OP_OUT;
    case CAIRO_OPERATOR_DEST_OUT:
	return XCB_RENDER_PICT_OP_OUT_REVERSE;
    case CAIRO_OPERATOR_ATOP:
	return XCB_RENDER_PICT_OP_ATOP;
    case CAIRO_OPERATOR_DEST_ATOP:
	return XCB_RENDER_PICT_OP_ATOP_REVERSE;
    case CAIRO_OPERATOR_XOR:
	return XCB_RENDER_PICT_OP_XOR;
    case CAIRO_OPERATOR_ADD:
	return XCB_RENDER_PICT_OP_ADD;
    case CAIRO_OPERATOR_SATURATE:
	return XCB_RENDER_PICT_OP_SATURATE;
    default:
	return XCB_RENDER_PICT_OP_OVER;
    }
}

static cairo_int_status_t
_cairo_xcb_surface_composite (cairo_operator_t		op,
			      cairo_pattern_t		*src_pattern,
			      cairo_pattern_t		*mask_pattern,
			      void			*abstract_dst,
			      int			src_x,
			      int			src_y,
			      int			mask_x,
			      int			mask_y,
			      int			dst_x,
			      int			dst_y,
			      unsigned int		width,
			      unsigned int		height)
{
    cairo_surface_attributes_t	src_attr, mask_attr;
    cairo_xcb_surface_t		*dst = abstract_dst;
    cairo_xcb_surface_t		*src;
    cairo_xcb_surface_t		*mask;
    cairo_int_status_t		status;

    if (!CAIRO_SURFACE_RENDER_HAS_COMPOSITE (dst))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    status = _cairo_pattern_acquire_surfaces (src_pattern, mask_pattern,
					      &dst->base,
					      src_x, src_y,
					      mask_x, mask_y,
					      width, height,
					      (cairo_surface_t **) &src,
					      (cairo_surface_t **) &mask,
					      &src_attr, &mask_attr);
    if (status)
	return status;

    status = _cairo_xcb_surface_set_attributes (src, &src_attr);
    if (status == CAIRO_STATUS_SUCCESS)
    {
	if (mask)
	{
	    status = _cairo_xcb_surface_set_attributes (mask, &mask_attr);
	    if (status == CAIRO_STATUS_SUCCESS)
		xcb_render_composite (dst->dpy,
				    _render_operator (op),
				    src->picture,
				    mask->picture,
				    dst->picture,
				    src_x + src_attr.x_offset,
				    src_y + src_attr.y_offset,
				    mask_x + mask_attr.x_offset,
				    mask_y + mask_attr.y_offset,
				    dst_x, dst_y,
				    width, height);
	}
	else
	{
	    static xcb_render_picture_t maskpict = { 0 };

	    xcb_render_composite (dst->dpy,
				_render_operator (op),
				src->picture,
				maskpict,
				dst->picture,
				src_x + src_attr.x_offset,
				src_y + src_attr.y_offset,
				0, 0,
				dst_x, dst_y,
				width, height);
	}
    }

    if (mask)
	_cairo_pattern_release_surface (mask_pattern, &mask->base, &mask_attr);

    _cairo_pattern_release_surface (src_pattern, &src->base, &src_attr);

    return status;
}

static cairo_int_status_t
_cairo_xcb_surface_fill_rectangles (void			     *abstract_surface,
				     cairo_operator_t	      op,
				     const cairo_color_t	*     color,
				     cairo_rectangle_int16_t *rects,
				     int			      num_rects)
{
    cairo_xcb_surface_t *surface = abstract_surface;
    xcb_render_color_t render_color;

    if (!CAIRO_SURFACE_RENDER_HAS_FILL_RECTANGLE (surface))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    render_color.red   = color->red_short;
    render_color.green = color->green_short;
    render_color.blue  = color->blue_short;
    render_color.alpha = color->alpha_short;

    /* XXX: This xcb_rectangle_t cast is evil... it needs to go away somehow. */
    xcb_render_fill_rectangles (surface->dpy,
			   _render_operator (op),
			   surface->picture,
			   render_color, num_rects, (xcb_rectangle_t *) rects);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_xcb_surface_composite_trapezoids (cairo_operator_t	op,
					 cairo_pattern_t	*pattern,
					 void			*abstract_dst,
					 cairo_antialias_t	antialias,
					 int			src_x,
					 int			src_y,
					 int			dst_x,
					 int			dst_y,
					 unsigned int		width,
					 unsigned int		height,
					 cairo_trapezoid_t	*traps,
					 int			num_traps)
{
    cairo_surface_attributes_t	attributes;
    cairo_xcb_surface_t		*dst = abstract_dst;
    cairo_xcb_surface_t		*src;
    cairo_int_status_t		status;
    int				render_reference_x, render_reference_y;
    int				render_src_x, render_src_y;
    int				cairo_format;
    xcb_render_pictforminfo_t	*render_format;

    if (!CAIRO_SURFACE_RENDER_HAS_TRAPEZOIDS (dst))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    status = _cairo_pattern_acquire_surface (pattern, &dst->base,
					     src_x, src_y, width, height,
					     (cairo_surface_t **) &src,
					     &attributes);
    if (status)
	return status;

    if (traps[0].left.p1.y < traps[0].left.p2.y) {
	render_reference_x = _cairo_fixed_integer_floor (traps[0].left.p1.x);
	render_reference_y = _cairo_fixed_integer_floor (traps[0].left.p1.y);
    } else {
	render_reference_x = _cairo_fixed_integer_floor (traps[0].left.p2.x);
	render_reference_y = _cairo_fixed_integer_floor (traps[0].left.p2.y);
    }

    render_src_x = src_x + render_reference_x - dst_x;
    render_src_y = src_y + render_reference_y - dst_y;

    switch (antialias) {
    case CAIRO_ANTIALIAS_NONE:
	cairo_format = CAIRO_FORMAT_A1;
	break;
    case CAIRO_ANTIALIAS_DEFAULT:
    case CAIRO_ANTIALIAS_GRAY:
    case CAIRO_ANTIALIAS_SUBPIXEL:
    default:
	cairo_format = CAIRO_FORMAT_A8;
	break;
    }
    render_format = xcb_render_util_find_standard_format (xcb_render_util_query_formats (dst->dpy), cairo_format);
    /* XXX: what to do if render_format is null? */

    /* XXX: The XTrapezoid cast is evil and needs to go away somehow. */
    status = _cairo_xcb_surface_set_attributes (src, &attributes);
    if (status == CAIRO_STATUS_SUCCESS)
	xcb_render_trapezoids (dst->dpy,
			     _render_operator (op),
			     src->picture, dst->picture,
			     render_format->id,
			     render_src_x + attributes.x_offset,
			     render_src_y + attributes.y_offset,
			     num_traps, (xcb_render_trapezoid_t *) traps);

    _cairo_pattern_release_surface (pattern, &src->base, &attributes);

    return status;
}

static cairo_int_status_t
_cairo_xcb_surface_set_clip_region (void              *abstract_surface,
				    pixman_region16_t *region)
{
    cairo_xcb_surface_t *surface = abstract_surface;

    if (surface->clip_rects) {
	free (surface->clip_rects);
	surface->clip_rects = NULL;
    }

    surface->num_clip_rects = 0;

    if (region == NULL) {
	if (surface->gc) {
	    uint32_t mask = XCB_GC_CLIP_MASK;
	    uint32_t pa[] = { XCB_NONE };

	    xcb_change_gc (surface->dpy, surface->gc, mask, pa);
	}

	if (surface->has_format && surface->picture) {
	    uint32_t mask = XCB_RENDER_CP_CLIP_MASK;
	    uint32_t pa[] = { XCB_NONE };

	    xcb_render_change_picture (surface->dpy, surface->picture, mask, pa);
	}
    } else {
	pixman_box16_t *boxes;
	xcb_rectangle_t *rects = NULL;
	int n_boxes, i;

	n_boxes = pixman_region_num_rects (region);
	if (n_boxes > 0) {
	    rects = malloc (sizeof(xcb_rectangle_t) * n_boxes);
	    if (rects == NULL)
		return CAIRO_STATUS_NO_MEMORY;
	} else {
	    rects = NULL;
	}

	boxes = pixman_region_rects (region);

	for (i = 0; i < n_boxes; i++) {
	    rects[i].x = boxes[i].x1;
	    rects[i].y = boxes[i].y1;
	    rects[i].width = boxes[i].x2 - boxes[i].x1;
	    rects[i].height = boxes[i].y2 - boxes[i].y1;
	}

	surface->clip_rects = rects;
	surface->num_clip_rects = n_boxes;

	if (surface->gc)
	    _cairo_xcb_surface_set_gc_clip_rects (surface);

	if (surface->picture)
	    _cairo_xcb_surface_set_picture_clip_rects (surface);
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_xcb_surface_get_extents (void		        *abstract_surface,
				cairo_rectangle_int16_t *rectangle)
{
    cairo_xcb_surface_t *surface = abstract_surface;

    rectangle->x = 0;
    rectangle->y = 0;

    rectangle->width  = surface->width;
    rectangle->height = surface->height;

    return CAIRO_STATUS_SUCCESS;
}

static const cairo_surface_backend_t cairo_xcb_surface_backend = {
    CAIRO_SURFACE_TYPE_XCB,
    _cairo_xcb_surface_create_similar,
    _cairo_xcb_surface_finish,
    _cairo_xcb_surface_acquire_source_image,
    _cairo_xcb_surface_release_source_image,
    _cairo_xcb_surface_acquire_dest_image,
    _cairo_xcb_surface_release_dest_image,
    _cairo_xcb_surface_clone_similar,
    _cairo_xcb_surface_composite,
    _cairo_xcb_surface_fill_rectangles,
    _cairo_xcb_surface_composite_trapezoids,
    NULL, /* copy_page */
    NULL, /* show_page */
    _cairo_xcb_surface_set_clip_region,
    NULL, /* intersect_clip_path */
    _cairo_xcb_surface_get_extents,
    NULL, /* old_show_glyphs */
    NULL, /* get_font_options */
    NULL, /* flush */
    NULL, /* mark_dirty_rectangle */
    NULL, /* scaled_font_fini */
    NULL  /* scaled_glyph_fini */
};

/**
 * _cairo_surface_is_xcb:
 * @surface: a #cairo_surface_t
 *
 * Checks if a surface is a #cairo_xcb_surface_t
 *
 * Return value: True if the surface is an xcb surface
 **/
static cairo_bool_t
_cairo_surface_is_xcb (cairo_surface_t *surface)
{
    return surface->backend == &cairo_xcb_surface_backend;
}

static cairo_surface_t *
_cairo_xcb_surface_create_internal (xcb_connection_t	     *dpy,
				    xcb_drawable_t		      drawable,
				    xcb_screen_t		     *screen,
				    xcb_visualtype_t	     *visual,
				    xcb_render_pictforminfo_t    *format,
				    int			      width,
				    int			      height,
				    int			      depth)
{
    cairo_xcb_surface_t *surface;
    const xcb_render_query_version_reply_t *r;

    surface = malloc (sizeof (cairo_xcb_surface_t));
    if (surface == NULL) {
	_cairo_error (CAIRO_STATUS_NO_MEMORY);
	return (cairo_surface_t*) &_cairo_surface_nil;
    }

    _cairo_surface_init (&surface->base, &cairo_xcb_surface_backend,
			 _xcb_render_format_to_content (format));

    surface->dpy = dpy;
    surface->screen = screen;

    surface->gc = XCB_NONE;
    surface->drawable = drawable;
    surface->owns_pixmap = FALSE;
    surface->visual = visual;
    if (format) {
	surface->format = *format;
	surface->has_format = 1;
    } else {
	surface->format.id = XCB_NONE;
	surface->has_format = 0;
    }
    surface->use_pixmap = 0;
    surface->width = width;
    surface->height = height;
    surface->depth = depth;

    surface->clip_rects = NULL;
    surface->num_clip_rects = 0;

    if (format) {
	surface->depth = format->depth;
    } else if (visual) {
	xcb_depth_iterator_t depths;
	xcb_visualtype_iterator_t visuals;

	/* This is ugly, but we have to walk over all visuals
	 * for the screen to find the depth.
	 */
	depths = xcb_screen_allowed_depths_iterator(screen);
	for(; depths.rem; xcb_depth_next(&depths))
	{
	    visuals = xcb_depth_visuals_iterator(depths.data);
	    for(; visuals.rem; xcb_visualtype_next(&visuals))
	    {
		if(visuals.data->visual_id == visual->visual_id)
		{
		    surface->depth = depths.data->depth;
		    goto found;
		}
	    }
	}
    found:
	;
    }

    surface->render_major = -1;
    surface->render_minor = -1;

    r = xcb_render_util_query_version(dpy);
    if (r) {
	surface->render_major = r->major_version;
	surface->render_minor = r->minor_version;
    }

    surface->picture = XCB_NONE;

    if (CAIRO_SURFACE_RENDER_HAS_CREATE_PICTURE (surface))
    {
	static const xcb_render_pictformat_t nil = { 0 };
	const xcb_render_pictformat_t *pict_format = &nil;

	if (format) {
	    pict_format = &format->id;
	} else if (visual) {
	    xcb_render_pictvisual_t *pict_visual;
	    pict_visual = xcb_render_util_find_visual_format (xcb_render_util_query_formats (dpy), visual->visual_id);
	    if (pict_visual)
		pict_format = &pict_visual->format;
	} else if (depth == 1) {
	    xcb_render_pictforminfo_t *format_info;
	    format_info = xcb_render_util_find_standard_format (xcb_render_util_query_formats (dpy), CAIRO_FORMAT_A1);
	    if (format_info)
		pict_format = &format_info->id;
	}

	/* XXX: if pict_format is nil, should we still call CreatePicture? */
	surface->picture = xcb_generate_id(dpy);
	xcb_render_create_picture (dpy, surface->picture, drawable,
				*pict_format, 0, NULL);
    }

    return (cairo_surface_t *) surface;
}

static xcb_screen_t *
_cairo_xcb_screen_from_visual (xcb_connection_t *c, xcb_visualtype_t *visual)
{
    xcb_depth_iterator_t d;
    xcb_screen_iterator_t s = xcb_setup_roots_iterator(xcb_get_setup(c));
    for (; s.rem; xcb_screen_next(&s))
    {
	if (s.data->root_visual == visual->visual_id)
	    return s.data;

	d = xcb_screen_allowed_depths_iterator(s.data);
	for (; d.rem; xcb_depth_next(&d))
	{
	    xcb_visualtype_iterator_t v = xcb_depth_visuals_iterator(d.data);
	    for (; v.rem; xcb_visualtype_next(&v))
	    {
		if (v.data->visual_id == visual->visual_id)
		    return s.data;
	    }
	}
    }
    return NULL;
}

/**
 * cairo_xcb_surface_create:
 * @c: an XCB connection
 * @drawable: an XCB drawable
 * @visual: the visual to use for drawing to @drawable. The depth
 *          of the visual must match the depth of the drawable.
 *          Currently, only TrueColor visuals are fully supported.
 * @width: the current width of @drawable.
 * @height: the current height of @drawable.
 *
 * Creates an XCB surface that draws to the given drawable.
 * The way that colors are represented in the drawable is specified
 * by the provided visual.
 *
 * NOTE: If @drawable is a window, then the function
 * cairo_xcb_surface_set_size must be called whenever the size of the
 * window changes.
 *
 * Return value: the newly created surface
 **/
cairo_surface_t *
cairo_xcb_surface_create (xcb_connection_t *c,
			  xcb_drawable_t	 drawable,
			  xcb_visualtype_t *visual,
			  int		 width,
			  int		 height)
{
    xcb_screen_t	*screen = _cairo_xcb_screen_from_visual (c, visual);

    if (screen == NULL) {
	_cairo_error (CAIRO_STATUS_INVALID_VISUAL);
	return (cairo_surface_t*) &_cairo_surface_nil;
    }

    return _cairo_xcb_surface_create_internal (c, drawable, screen,
					       visual, NULL,
					       width, height, 0);
}

/**
 * cairo_xcb_surface_create_for_bitmap:
 * @c: an XCB connection
 * @bitmap: an XCB Pixmap (a depth-1 pixmap)
 * @screen: an XCB Screen
 * @width: the current width of @bitmap
 * @height: the current height of @bitmap
 *
 * Creates an XCB surface that draws to the given bitmap.
 * This will be drawn to as a CAIRO_FORMAT_A1 object.
 *
 * Return value: the newly created surface
 **/
cairo_surface_t *
cairo_xcb_surface_create_for_bitmap (xcb_connection_t     *c,
				     xcb_pixmap_t		bitmap,
				     xcb_screen_t	       *screen,
				     int		width,
				     int		height)
{
    return _cairo_xcb_surface_create_internal (c, bitmap, screen,
					       NULL, NULL,
					       width, height, 1);
}

/**
 * cairo_xcb_surface_create_with_xrender_format:
 * @c: an XCB connection
 * @drawable: an XCB drawable
 * @screen: the XCB screen associated with @drawable
 * @format: the picture format to use for drawing to @drawable. The
 *          depth of @format mush match the depth of the drawable.
 * @width: the current width of @drawable
 * @height: the current height of @drawable
 *
 * Creates an XCB surface that draws to the given drawable.
 * The way that colors are represented in the drawable is specified
 * by the provided picture format.
 *
 * NOTE: If @drawable is a Window, then the function
 * cairo_xcb_surface_set_size must be called whenever the size of the
 * window changes.
 *
 * Return value: the newly created surface.
 **/
cairo_surface_t *
cairo_xcb_surface_create_with_xrender_format (xcb_connection_t	    *c,
					      xcb_drawable_t	     drawable,
					      xcb_screen_t		    *screen,
					      xcb_render_pictforminfo_t *format,
					      int		     width,
					      int		     height)
{
    return _cairo_xcb_surface_create_internal (c, drawable, screen,
					       NULL, format,
					       width, height, 0);
}

/**
 * cairo_xcb_surface_set_size:
 * @surface: a #cairo_surface_t for the XCB backend
 * @width: the new width of the surface
 * @height: the new height of the surface
 *
 * Informs cairo of the new size of the XCB drawable underlying the
 * surface. For a surface created for a window (rather than a pixmap),
 * this function must be called each time the size of the window
 * changes. (For a subwindow, you are normally resizing the window
 * yourself, but for a toplevel window, it is necessary to listen for
 * ConfigureNotify events.)
 *
 * A pixmap can never change size, so it is never necessary to call
 * this function on a surface created for a pixmap.
 **/
void
cairo_xcb_surface_set_size (cairo_surface_t *surface,
			    int              width,
			    int              height)
{
    cairo_xcb_surface_t *xcb_surface = (cairo_xcb_surface_t *)surface;

    if (! _cairo_surface_is_xcb (surface)) {
	_cairo_surface_set_error (surface, CAIRO_STATUS_SURFACE_TYPE_MISMATCH);
	return;
    }

    xcb_surface->width = width;
    xcb_surface->height = height;
}
