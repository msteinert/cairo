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

#define Mask(n)	((n) == 32 ? 0xffffffff : ((1 << (n))-1))

IcFormat *
_IcFormatCreate (IcFormatName name)
{
    IcFormat *format;

    format = malloc (sizeof (IcFormat));
    if (format == NULL)
	return NULL;

    IcFormatInit (format, name);

    return format;
}

void
IcFormatInit (IcFormat *format, IcFormatName name)
{
/* XXX: What do we want to lodge in here?
    format->id = FakeClientID (0);
*/
    format->format_name = name;
    format->depth = PICT_FORMAT_BPP(name);

    switch (PICT_FORMAT_TYPE(name)) {
    case PICT_TYPE_ARGB:
	
	format->alphaMask = Mask(PICT_FORMAT_A(name));
	if (format->alphaMask)
	    format->alpha = (PICT_FORMAT_R(name) +
			     PICT_FORMAT_G(name) +
			     PICT_FORMAT_B(name));
	
	format->redMask = Mask(PICT_FORMAT_R(name));
	format->red = (PICT_FORMAT_G(name) + 
		       PICT_FORMAT_B(name));
	
	format->greenMask = Mask(PICT_FORMAT_G(name));
	format->green = PICT_FORMAT_B(name);
	
	format->blueMask = Mask(PICT_FORMAT_B(name));
	format->blue = 0;
	break;
	
    case PICT_TYPE_ABGR:
	
	format->alphaMask = Mask(PICT_FORMAT_A(name));
	if (format->alphaMask)
	    format->alpha = (PICT_FORMAT_B(name) +
			     PICT_FORMAT_G(name) +
			     PICT_FORMAT_R(name));
	
	format->blueMask = Mask(PICT_FORMAT_B(name));
	format->blue = (PICT_FORMAT_G(name) + 
			PICT_FORMAT_R(name));
	
	format->greenMask = Mask(PICT_FORMAT_G(name));
	format->green = PICT_FORMAT_R(name);
	
	format->redMask = Mask(PICT_FORMAT_R(name));
	format->red = 0;
	break;
	
    case PICT_TYPE_A:
	
	format->alpha = 0;
	format->alphaMask = Mask(PICT_FORMAT_A(name));
	
	/* remaining fields already set to zero */
	break;
	
/* XXX: We're not supporting indexed formats, right?
    case PICT_TYPE_COLOR:
    case PICT_TYPE_GRAY:
        format->type = PictTypeIndexed;
	format->index.pVisual = &pScreen->visuals[PICT_FORMAT_VIS(name)];
	break;
*/
    }
}

void
_IcFormatDestroy (IcFormat *format)
{
    free (format);
}
