/*
 * $XFree86: $
 *
 * Copyright © 2000 SuSE, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of SuSE not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  SuSE makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * SuSE DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL SuSE
 * BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN 
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author:  Keith Packard, SuSE, Inc.
 */

#include "icint.h"

/* GCC 3.4 supports a "population count" builtin, which on many targets is
   implemented with a single instruction.  There is a fallback definition
   in libgcc in case a target does not have one, which should be just as
   good as the static function below.  */
#if __GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 4)
# if __INT_MIN__ == 0x7fffffff
#  define Ones(mask)		__builtin_popcount(mask)
# else
#  define Ones(mask)		__builtin_popcountl((mask) & 0xffffffff)
# endif
#else
/* Otherwise fall back on HACKMEM 169.  */
static int
Ones(unsigned long mask)
{
    register unsigned long y;

    y = (mask >> 1) &033333333333;
    y = mask - y - ((y >>1) & 033333333333);
    return (((y + (y >> 3)) & 030707070707) % 077);
}
#endif

void
IcColorToPixel (const IcFormat	*format,
		const IcColor	*color,
		IcBits		*pixel)
{
    CARD32	    r, g, b, a;

    r = color->red >> (16 - Ones (format->redMask));
    g = color->green >> (16 - Ones (format->greenMask));
    b = color->blue >> (16 - Ones (format->blueMask));
    a = color->alpha >> (16 - Ones (format->alphaMask));
    r = r << format->red;
    g = g << format->green;
    b = b << format->blue;
    a = a << format->alpha;
    *pixel = r|g|b|a;
}
slim_hidden_def(IcColorToPixel)

static CARD16
IcFillColor (CARD32 pixel, int bits)
{
    while (bits < 16)
    {
	pixel |= pixel << bits;
	bits <<= 1;
    }
    return (CARD16) pixel;
}

void
IcPixelToColor (const IcFormat	*format,
		const IcBits	pixel,
		IcColor		*color)
{
    CARD32	    r, g, b, a;

    r = (pixel >> format->red) & format->redMask;
    g = (pixel >> format->green) & format->greenMask;
    b = (pixel >> format->blue) & format->blueMask;
    a = (pixel >> format->alpha) & format->alphaMask;
    color->red = IcFillColor (r, Ones (format->redMask));
    color->green = IcFillColor (r, Ones (format->greenMask));
    color->blue = IcFillColor (r, Ones (format->blueMask));
    color->alpha = IcFillColor (r, Ones (format->alphaMask));
}
