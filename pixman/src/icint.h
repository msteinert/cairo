/* $XFree86: $
 *
 * Copyright © 2003 Carl Worth
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Carl Worth not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Carl Worth makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * CARL WORTH DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL CARL WORTH BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _ICINT_H_
#define _ICINT_H_

#include "ic.h"
#include "icimage.h"
#include "icrop.h"

/* Need definitions for XFixed, etc. */
#include "X11/extensions/Xrender.h"

/* need this for Xlib Region functions */
#include "Xutil.h"
#include "../../../lib/X11/region.h"

/* XXX: This is to avoid including colormap.h from the server includes */
typedef CARD32 Pixel;

/* XXX: This is to avoid including gc.h from the server includes */
/* clientClipType field in GC */
#define CT_NONE			0
#define CT_PIXMAP		1
#define CT_REGION		2
#define CT_UNSORTED		6
#define CT_YSORTED		10
#define CT_YXSORTED		14
#define CT_YXBANDED		18

struct _IcPixels {
    IcBits		*data;
    unsigned int	width;
    unsigned int	height;
    unsigned int	depth;
    unsigned int	bpp;
    unsigned int	stride;
    int			x;
    int			y;
    unsigned int	refcnt;
};

/* icutil.c */

IcBits
IcReplicatePixel (Pixel p, int bpp);

#endif
