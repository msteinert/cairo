#include <png.h>

#include "cairoint.h"

static const cairo_surface_backend_t cairo_png_surface_backend;

void
cairo_set_target_png (cairo_t	*cr,
		      FILE	*file,
		      cairo_format_t	format,
		      int	       	width,
		      int		height)
{
    cairo_surface_t *surface;

    surface = cairo_png_surface_create (file, format, 
					width, height);

    if (surface == NULL) {
	cr->status = CAIRO_STATUS_NO_MEMORY;
	return;
    }

    cairo_set_target_surface (cr, surface);

    /* cairo_set_target_surface takes a reference, so we must destroy ours */
    cairo_surface_destroy (surface);
}

typedef struct cairo_png_surface {
    cairo_surface_t base;

    /* PNG-specific fields */
    FILE *file;

    png_structp png_w;
    png_infop png_i;

    cairo_image_surface_t *image;
} cairo_png_surface_t;


static void
_cairo_png_surface_erase (cairo_png_surface_t *surface);

cairo_surface_t *
cairo_png_surface_create (FILE			*file,
			  cairo_format_t	format,
			  int			width,
			  int			height)
{
    cairo_png_surface_t *surface;
    time_t now = time (NULL);
    png_time png_time;

    if (format == CAIRO_FORMAT_A8 ||
	format == CAIRO_FORMAT_A1 ||
	file == NULL)
	return NULL;

    surface = malloc (sizeof (cairo_png_surface_t));
    if (surface == NULL)
	goto failure;

    _cairo_surface_init (&surface->base, &cairo_png_surface_backend);
    surface->png_w = NULL;
    surface->png_i = NULL;

    surface->image = (cairo_image_surface_t *)
	cairo_image_surface_create (format, width, height);
    if (surface->image == NULL)
	goto failure;

    _cairo_png_surface_erase (surface);

    surface->file = file;

    surface->png_w = png_create_write_struct (PNG_LIBPNG_VER_STRING,
					      NULL, NULL, NULL);
    if (surface->png_w == NULL)
	goto failure;
    surface->png_i = png_create_info_struct (surface->png_w);
    if (surface->png_i == NULL)
	goto failure;

    if (setjmp (png_jmpbuf (surface->png_w)))
	goto failure;
    
    png_init_io (surface->png_w, surface->file);

    switch (format) {
    case CAIRO_FORMAT_ARGB32:
    png_set_IHDR (surface->png_w, surface->png_i,
		  width, height, 8, PNG_COLOR_TYPE_RGB_ALPHA,
		  PNG_INTERLACE_NONE,
		  PNG_COMPRESSION_TYPE_DEFAULT,
		  PNG_FILTER_TYPE_DEFAULT);
    break;
    case CAIRO_FORMAT_RGB24:
    png_set_IHDR (surface->png_w, surface->png_i,
		  width, height, 8, PNG_COLOR_TYPE_RGB,
		  PNG_INTERLACE_NONE,
		  PNG_COMPRESSION_TYPE_DEFAULT,
		  PNG_FILTER_TYPE_DEFAULT);
    break;
    case CAIRO_FORMAT_A8:
    case CAIRO_FORMAT_A1:
	/* These are not currently supported. */
	break;
    }

    png_convert_from_time_t (&png_time, now);
    png_set_tIME (surface->png_w, surface->png_i, &png_time);

    png_write_info (surface->png_w, surface->png_i);

    switch (format) {
    case CAIRO_FORMAT_ARGB32:
	png_set_bgr (surface->png_w);
	break;
    case CAIRO_FORMAT_RGB24:
	png_set_filler (surface->png_w, 0, PNG_FILLER_AFTER);
	png_set_bgr (surface->png_w);
	break;
    case CAIRO_FORMAT_A8:
    case CAIRO_FORMAT_A1:
	/* These are not currently supported. */
	break;
    }

    return &surface->base;


  failure:
    if (surface) {
	if (surface->image)
	    cairo_surface_destroy (&surface->image->base);
	if (surface->png_i)
	    png_destroy_write_struct (&surface->png_w, &surface->png_i);
	else if (surface->png_w)
	    png_destroy_write_struct (&surface->png_w, NULL);
	free (surface);
    }
    return NULL;
}


static cairo_surface_t *
_cairo_png_surface_create_similar (void		*abstract_src,
				   cairo_format_t	format,
				   int		width,
				   int		height)
{
    return NULL;
}

static void
_cairo_png_surface_destroy (void *abstract_surface)
{
    cairo_png_surface_t *surface = abstract_surface;
    int i;
    png_byte *row;

    if (setjmp (png_jmpbuf (surface->png_w)))
	goto failure;

    row = surface->image->data;
    for (i=0; i < surface->image->height; i++) {
	png_write_row (surface->png_w, row);
	row += surface->image->stride;
    }

    png_write_end (surface->png_w, surface->png_i);

  failure:
    png_destroy_write_struct (&surface->png_w, &surface->png_i);

    cairo_surface_destroy (&surface->image->base);

    free (surface);
}

static void
_cairo_png_surface_erase (cairo_png_surface_t *surface)
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

static double
_cairo_png_surface_pixels_per_inch (void *abstract_surface)
{
    return 96.0;
}

static cairo_image_surface_t *
_cairo_png_surface_get_image (void *abstract_surface)
{
    cairo_png_surface_t *surface = abstract_surface;

    cairo_surface_reference (&surface->image->base);

    return surface->image;
}

static cairo_status_t
_cairo_png_surface_set_image (void			*abstract_surface,
			      cairo_image_surface_t	*image)
{
    cairo_png_surface_t *surface = abstract_surface;

    if (image == surface->image)
	return CAIRO_STATUS_SUCCESS;

    /* XXX: Need to call _cairo_image_surface_set_image here, but it's
       not implemented yet. */

    return CAIRO_INT_STATUS_UNSUPPORTED;
}

static cairo_status_t
_cairo_png_surface_set_matrix (void		*abstract_surface,
			       cairo_matrix_t	*matrix)
{
    cairo_png_surface_t *surface = abstract_surface;

    return _cairo_image_surface_set_matrix (surface->image, matrix);
}

static cairo_status_t
_cairo_png_surface_set_filter (void		*abstract_surface,
			       cairo_filter_t	filter)
{
    cairo_png_surface_t *surface = abstract_surface;

    return _cairo_image_surface_set_filter (surface->image, filter);
}

static cairo_status_t
_cairo_png_surface_set_repeat (void		*abstract_surface,
			       int		repeat)
{
    cairo_png_surface_t *surface = abstract_surface;

    return _cairo_image_surface_set_repeat (surface->image, repeat);
}

static cairo_int_status_t
_cairo_png_surface_composite (cairo_operator_t	operator,
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
_cairo_png_surface_fill_rectangles (void			*abstract_surface,
				    cairo_operator_t	operator,
				    const cairo_color_t	*color,
				    cairo_rectangle_t	*rects,
				    int			num_rects)
{
    return CAIRO_INT_STATUS_UNSUPPORTED;
}

static cairo_int_status_t
_cairo_png_surface_composite_trapezoids (cairo_operator_t	operator,
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
_cairo_png_surface_copy_page (void *abstract_surface)
{
    return CAIRO_INT_STATUS_UNSUPPORTED;
}

static cairo_int_status_t
_cairo_png_surface_show_page (void *abstract_surface)
{
    return CAIRO_INT_STATUS_UNSUPPORTED;
}

static cairo_int_status_t
_cairo_png_surface_set_clip_region (void *abstract_surface,
				    pixman_region16_t *region)
{
    cairo_png_surface_t *surface = abstract_surface;

    return _cairo_image_surface_set_clip_region (surface->image, region);
}

static cairo_int_status_t
_cairo_png_surface_create_pattern (void *abstract_surface,
                                   cairo_pattern_t *pattern,
                                   cairo_box_t *extents)
{
    return CAIRO_INT_STATUS_UNSUPPORTED;
}

static const cairo_surface_backend_t cairo_png_surface_backend = {
    _cairo_png_surface_create_similar,
    _cairo_png_surface_destroy,
    _cairo_png_surface_pixels_per_inch,
    _cairo_png_surface_get_image,
    _cairo_png_surface_set_image,
    _cairo_png_surface_set_matrix,
    _cairo_png_surface_set_filter,
    _cairo_png_surface_set_repeat,
    _cairo_png_surface_composite,
    _cairo_png_surface_fill_rectangles,
    _cairo_png_surface_composite_trapezoids,
    _cairo_png_surface_copy_page,
    _cairo_png_surface_show_page,
    _cairo_png_surface_set_clip_region,
    _cairo_png_surface_create_pattern
};
