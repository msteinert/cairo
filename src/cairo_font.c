/*
 * Copyright © 2002 USC, Information Sciences Institute
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

cairo_int_status_t
_cairo_font_init (cairo_font_t *font, const struct cairo_font_backend *backend)
{
    if (font == NULL)
	return CAIRO_STATUS_SUCCESS;

    font->key = (unsigned char *) strdup (CAIRO_FONT_KEY_DEFAULT);
    cairo_matrix_set_identity (&font->matrix);

    font->backend = backend;

    return CAIRO_STATUS_SUCCESS;
}

cairo_int_status_t
_cairo_font_init_copy (cairo_font_t *font, cairo_font_t *other)
{
    if (other == NULL)
	return CAIRO_STATUS_SUCCESS;

    if (other->key) {
	font->key = (unsigned char *) strdup ((char *) other->key);
	if (font->key == NULL)
	    return CAIRO_STATUS_NO_MEMORY;
    }
    font->matrix = other->matrix;

    font->backend = other->backend;

    return CAIRO_STATUS_SUCCESS;
}

cairo_font_t *
_cairo_font_copy (cairo_font_t *font)
{
    if (font == NULL || font->backend->copy == NULL)
	return NULL;

    return font->backend->copy (font);
}

void
_cairo_font_fini (cairo_font_t *font)
{
    if (font == NULL)
	return;

    if (font->key)
	free (font->key);
    font->key = NULL;

    _cairo_matrix_fini (&font->matrix);

    if (font->backend->close)
	font->backend->close (font);
}

cairo_int_status_t
_cairo_font_select (cairo_font_t *font, const char *key)
{
    if (font == NULL)
	return CAIRO_STATUS_SUCCESS;

    if (font->backend->close)
	font->backend->close (font);

    if (font->key)
	free (font->key);

    font->key = (unsigned char *) strdup ((char *) key);
    if (font->key == NULL)
	return CAIRO_STATUS_NO_MEMORY;

    return CAIRO_STATUS_SUCCESS;
}

cairo_int_status_t
_cairo_font_scale (cairo_font_t *font, double scale)
{
    if (font == NULL)
	return CAIRO_STATUS_SUCCESS;

    cairo_matrix_scale (&font->matrix, scale, scale);

    return CAIRO_STATUS_SUCCESS;
}

cairo_int_status_t
_cairo_font_transform (cairo_font_t *font,
		       double a, double b,
		       double c, double d)
{
    cairo_matrix_t m;

    if (font == NULL)
	return CAIRO_STATUS_SUCCESS;

    cairo_matrix_set_affine (&m, a, b, c, d, 0, 0);
    cairo_matrix_multiply (&font->matrix, &m, &font->matrix);

    return CAIRO_STATUS_SUCCESS;
}

cairo_int_status_t
_cairo_font_text_extents (cairo_font_t *font,
			  cairo_matrix_t *ctm,
			  const unsigned char *utf8,
			  double *x, double *y,
			  double *width, double *height,
			  double *dx, double *dy)
{
    if (font == NULL)
	return CAIRO_STATUS_SUCCESS;

    if (!font->backend->text_extents)
	return CAIRO_STATUS_SUCCESS;

    return font->backend->text_extents (font, ctm, utf8, x, y, width, height, dx, dy);
}

cairo_int_status_t
_cairo_font_show_text (cairo_font_t		*font,
		       cairo_matrix_t		*ctm,
		       cairo_operator_t		operator,
		       cairo_surface_t		*source,
		       cairo_surface_t		*surface,
		       double			x,
		       double			y,
		       const unsigned char	*utf8)
{
    if (font == NULL)
	return CAIRO_STATUS_SUCCESS;

    if (!font->backend->show_text)
	return CAIRO_STATUS_SUCCESS;

    return font->backend->show_text (font, ctm, operator, source, surface, x, y, utf8);
}
