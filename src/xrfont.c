/*
 * $XFree86: $
 *
 * Copyright © 2002 Carl D. Worth
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of Carl
 * D. Worth not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission.  Carl D. Worth makes no representations about the
 * suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * CARL D. WORTH DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL CARL D. WORTH BE LIABLE FOR ANY SPECIAL,
 * INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
 * IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <string.h>

#include "xrint.h"

void
_XrFontInit(XrFont *font)
{
    font->key = (unsigned char *) strdup(XR_FONT_KEY_DEFAULT);

    font->dpy = NULL;
    font->xft_font = NULL;

    XrMatrixSetIdentity(&font->matrix);
}

XrStatus
_XrFontInitCopy(XrFont *font, XrFont *other)
{
    *font = *other;

    if (other->key) {
	font->key = (unsigned char *) strdup((char *) other->key);
	if (font->key == NULL)
	    return XrStatusNoMemory;
    }

    if (other->xft_font) {
	font->xft_font = XftFontCopy(other->dpy, other->xft_font);
	if (font->xft_font == NULL)
	    return XrStatusNoMemory;
    }

    return XrStatusSuccess;
}

void
_XrFontDeinit(XrFont *font)
{
    if (font->key)
	free(font->key);
    font->key = NULL;

    _XrMatrixFini(&font->matrix);

    if (font->xft_font)
	XftFontClose(font->dpy, font->xft_font);
    font->xft_font = NULL;
}

XrStatus
_XrFontSelect(XrFont *font, const char *key)
{
    if (font->xft_font)
	XftFontClose(font->dpy, font->xft_font);
    font->xft_font = NULL;

    if (font->key)
	free(font->key);

    font->key = (unsigned char *) strdup((char *) key);
    if (font->key == NULL)
	return XrStatusNoMemory;

    return XrStatusSuccess;
}

XrStatus
_XrFontScale(XrFont *font, double scale)
{
    XrMatrixScale(&font->matrix, scale, scale);

    return XrStatusSuccess;
}

XrStatus
_XrFontTransform(XrFont *font,
		 double a, double b,
		 double c, double d)
{
    XrMatrix m;

    XrMatrixSetAffine(&m, a, b, c, d, 0, 0);
    _XrMatrixMultiplyIntoRight(&m, &font->matrix);

    return XrStatusSuccess;
}

XrStatus
_XrFontResolveXftFont(XrFont *font, XrGState *gstate, XftFont **xft_font)
{
    FcPattern	*pattern;
    FcPattern	*match;
    FcResult	result;
    XrMatrix	matrix;
    FcMatrix	fc_matrix;
    double	expansion;
    double	font_size;
    
    if (font->xft_font) {
	*xft_font = font->xft_font;
	return XrStatusSuccess;
    }
    
    pattern = FcNameParse(font->key);

    matrix = gstate->ctm;

    _XrMatrixMultiplyIntoRight(&font->matrix, &matrix);

    /* Pull the scale factor out of the final matrix and use it to set
       the direct pixelsize of the font. This enables freetype to
       perform proper hinting at any size. */

    /* XXX: The determinant gives an area expansion factor, so the
       math below should be correct for the (common) case of uniform
       X/Y scaling. Is there anything different we would want to do
       for non-uniform X/Y scaling? */
    _XrMatrixComputeDeterminant (&matrix, &expansion);
    font_size = sqrt (expansion);
    FcPatternAddDouble (pattern, "pixelsize", font_size);
    XrMatrixScale (&matrix, 1.0 / font_size, 1.0 / font_size);

    fc_matrix.xx = matrix.m[0][0];
    fc_matrix.xy = matrix.m[0][1];
    fc_matrix.yx = matrix.m[1][0];
    fc_matrix.yy = matrix.m[1][1];

    FcPatternAddMatrix(pattern, "matrix", &fc_matrix);

    /* XXX: Need to abandon Xft and use Xc instead */
    /*      When I do that I can throw away these Display pointers */
    font->dpy = gstate->surface->dpy;
    match = XftFontMatch (font->dpy, DefaultScreen(font->dpy), pattern, &result);
    if (!match)
	return 0;
    
    font->xft_font = XftFontOpenPattern (font->dpy, match);

    *xft_font = font->xft_font;

    FcPatternDestroy (pattern);

    return XrStatusSuccess;
}
