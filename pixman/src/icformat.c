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
#include "icimage.h"

/* XXX: Need to trim down (eliminate?) these includes. */

#include "misc.h"
/*
#include "scrnintstr.h"
#include "regionstr.h"
#include "validate.h"
#include "windowstr.h"
#include "input.h"
#include "colormapst.h"
#include "cursorstr.h"
#include "dixstruct.h"
#include "gcstruct.h"
#include "picturestr.h"
*/
#include "os.h"
#include "resource.h"
#include "servermd.h"

void
IcFormatInit (IcFormat *format, IcFormatName name);

#define Mask(n)	((n) == 32 ? 0xffffffff : ((1 << (n))-1))

IcFormat *
IcFormatCreate (IcFormatName name)
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
    format->depth = PICT_FORMAT_BPP(name);
    format->format = name;
    switch (PICT_FORMAT_TYPE(name)) {
    case PICT_TYPE_ARGB:
	format->type = PictTypeDirect;
	
	format->direct.alphaMask = Mask(PICT_FORMAT_A(name));
	if (format->direct.alphaMask)
	    format->direct.alpha = (PICT_FORMAT_R(name) +
				    PICT_FORMAT_G(name) +
				    PICT_FORMAT_B(name));
	
	format->direct.redMask = Mask(PICT_FORMAT_R(name));
	format->direct.red = (PICT_FORMAT_G(name) + 
			      PICT_FORMAT_B(name));
	
	format->direct.greenMask = Mask(PICT_FORMAT_G(name));
	format->direct.green = PICT_FORMAT_B(name);
	
	format->direct.blueMask = Mask(PICT_FORMAT_B(name));
	format->direct.blue = 0;
	break;
	
    case PICT_TYPE_ABGR:
	format->type = PictTypeDirect;
	
	format->direct.alphaMask = Mask(PICT_FORMAT_A(name));
	if (format->direct.alphaMask)
	    format->direct.alpha = (PICT_FORMAT_B(name) +
				    PICT_FORMAT_G(name) +
				    PICT_FORMAT_R(name));
	
	format->direct.blueMask = Mask(PICT_FORMAT_B(name));
	format->direct.blue = (PICT_FORMAT_G(name) + 
			       PICT_FORMAT_R(name));
	
	format->direct.greenMask = Mask(PICT_FORMAT_G(name));
	format->direct.green = PICT_FORMAT_R(name);
	
	format->direct.redMask = Mask(PICT_FORMAT_R(name));
	format->direct.red = 0;
	break;
	
    case PICT_TYPE_A:
	format->type = PictTypeDirect;
	
	format->direct.alpha = 0;
	format->direct.alphaMask = Mask(PICT_FORMAT_A(name));
	
	/* remaining fields already set to zero */
	break;
	
/* XXX: Supporting indexed formats requires more, (just pass in the visual?) 
    case PICT_TYPE_COLOR:
    case PICT_TYPE_GRAY:
        format->type = PictTypeIndexed;
	format->index.pVisual = &pScreen->visuals[PICT_FORMAT_VIS(name)];
	break;
*/
    }
}

void
IcFormatDestroy (IcFormat *format)
{
    free (format);
}
