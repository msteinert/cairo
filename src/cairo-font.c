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

#include <string.h>

#include "cairoint.h"

void
_cairo_font_init (cairo_font_t *font)
{
    font->key = (unsigned char *) strdup (CAIRO_FONT_KEY_DEFAULT);

    font->dpy = NULL;
    font->xft_font = NULL;

    cairo_matrix_set_identity (&font->matrix);
}

cairo_status_t
_cairo_font_init_copy (cairo_font_t *font, cairo_font_t *other)
{
    *font = *other;

    if (other->key) {
	font->key = (unsigned char *) strdup ((char *) other->key);
	if (font->key == NULL)
	    return CAIRO_STATUS_NO_MEMORY;
    }

    if (other->xft_font) {
	font->xft_font = XftFontCopy (other->dpy, other->xft_font);
	if (font->xft_font == NULL)
	    return CAIRO_STATUS_NO_MEMORY;
    }

    return CAIRO_STATUS_SUCCESS;
}

void
_cairo_font_fini (cairo_font_t *font)
{
    if (font->key)
	free (font->key);
    font->key = NULL;

    _cairo_matrix_fini (&font->matrix);

    if (font->xft_font)
	XftFontClose (font->dpy, font->xft_font);
    font->xft_font = NULL;
}

cairo_status_t
_cairo_font_select (cairo_font_t *font, const char *key)
{
    if (font->xft_font)
	XftFontClose (font->dpy, font->xft_font);
    font->xft_font = NULL;

    if (font->key)
	free (font->key);

    font->key = (unsigned char *) strdup ((char *) key);
    if (font->key == NULL)
	return CAIRO_STATUS_NO_MEMORY;

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_font_scale (cairo_font_t *font, double scale)
{
    cairo_matrix_scale (&font->matrix, scale, scale);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
cairo_font_transform (cairo_font_t *font,
		      double a, double b,
		      double c, double d)
{
    cairo_matrix_t m;

    cairo_matrix_set_affine (&m, a, b, c, d, 0, 0);
    cairo_matrix_multiply (&font->matrix, &m, &font->matrix);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_font_resolve_xft_font (cairo_font_t *font, cairo_gstate_t *gstate, XftFont **xft_font)
{
    FcPattern	*pattern;
    FcPattern	*match;
    FcResult	result;
    cairo_matrix_t	matrix;
    FcMatrix	fc_matrix;
    double	expansion;
    double	font_size;
    
    if (font->xft_font) {
	*xft_font = font->xft_font;
	return CAIRO_STATUS_SUCCESS;
    }
    
    pattern = FcNameParse (font->key);

    matrix = gstate->ctm;

    cairo_matrix_multiply (&matrix, &font->matrix, &matrix);

    /* Pull the scale factor out of the final matrix and use it to set
       the direct pixelsize of the font. This enables freetype to
       perform proper hinting at any size. */

    /* XXX: The determinant gives an area expansion factor, so the
       math below should be correct for the (common) case of uniform
       X/Y scaling. Is there anything different we would want to do
       for non-uniform X/Y scaling? */
    _cairo_matrix_compute_determinant (&matrix, &expansion);
    font_size = sqrt (expansion);
    FcPatternAddDouble (pattern, "pixelsize", font_size);
    cairo_matrix_scale (&matrix, 1.0 / font_size, 1.0 / font_size);

    fc_matrix.xx = matrix.m[0][0];
    fc_matrix.xy = matrix.m[0][1];
    fc_matrix.yx = matrix.m[1][0];
    fc_matrix.yy = matrix.m[1][1];

    FcPatternAddMatrix (pattern, "matrix", &fc_matrix);

    /* XXX: Need to abandon Xft and use Xc instead */
    /*      When I do that I can throw away these Display pointers */
    font->dpy = gstate->surface->dpy;
    match = XftFontMatch (font->dpy, DefaultScreen (font->dpy), pattern, &result);
    if (!match)
	return 0;
    
    font->xft_font = XftFontOpenPattern (font->dpy, match);

    *xft_font = font->xft_font;

    FcPatternDestroy (pattern);

    return CAIRO_STATUS_SUCCESS;
}
