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

#include "xrint.h"

static XrColor XR_COLOR_DEFAULT = { 1.0, 1.0, 1.0, 1.0, {0xffff, 0xffff, 0xffff, 0xffff}};

static void
_XrColorComputeXcColor(XrColor *color);

void
_XrColorInit(XrColor *color)
{
    *color = XR_COLOR_DEFAULT;
}

void
_XrColorDeinit(XrColor *color)
{
    /* Nothing to do here */
}

static void
_XrColorComputeXcColor(XrColor *color)
{
    color->xc_color.red = color->red * color->alpha * 0xffff;
    color->xc_color.green = color->green * color->alpha * 0xffff;
    color->xc_color.blue = color->blue * color->alpha * 0xffff;
    color->xc_color.alpha = color->alpha * 0xffff;
}

void
_XrColorSetRGB(XrColor *color, double red, double green, double blue)
{
    color->red = red;
    color->green = green;
    color->blue = blue;

    _XrColorComputeXcColor(color);
}

void
_XrColorGetRGB(XrColor *color, double *red, double *green, double *blue)
{
    *red = color->red;
    *green = color->green;
    *blue = color->blue;
}

void
_XrColorSetAlpha(XrColor *color, double alpha)
{
    color->alpha = alpha;

    _XrColorComputeXcColor(color);
}
