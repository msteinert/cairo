/*
 * $XFree86: $
 *
 * Copyright © 2002 University of Southern California
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of University
 * of Southern California not be used in advertising or publicity
 * pertaining to distribution of the software without specific,
 * written prior permission.  University of Southern California makes
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied
 * warranty.
 *
 * UNIVERSITY OF SOUTHERN CALIFORNIA DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL UNIVERSITY OF
 * SOUTHERN CALIFORNIA BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author: Carl Worth, USC, Information Sciences Institute */

#include "xrint.h"

static XrColor XR_COLOR_DEFAULT = { 1.0, 1.0, 1.0, 1.0, {0xffff, 0xffff, 0xffff, 0xffff}};

static void
_XrColorComputeRenderColor(XrColor *color);

void
XrColorInit(XrColor *color)
{
    *color = XR_COLOR_DEFAULT;
}

void
XrColorDeinit(XrColor *color)
{
    /* Nothing to do here */
}

static void
_XrColorComputeRenderColor(XrColor *color)
{
    color->render.red = color->red * color->alpha * 0xffff;
    color->render.green = color->green * color->alpha * 0xffff;
    color->render.blue = color->blue * color->alpha * 0xffff;
}

void
XrColorSetRGB(XrColor *color, double red, double green, double blue)
{
    color->red = red;
    color->green = green;
    color->blue = blue;

    _XrColorComputeRenderColor(color);
}

void
XrColorSetAlpha(XrColor *color, double alpha)
{
    color->alpha = alpha;

    _XrColorComputeRenderColor(color);
}
