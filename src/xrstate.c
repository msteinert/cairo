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

#include <stdlib.h>
#include "xrint.h"

XrState *
XrStateCreate(Display *dpy)
{
    XrState *xrs;

    xrs = malloc(sizeof(XrState));

    XrStateInit(xrs, dpy);

    return xrs;
}

void
XrStateInit(XrState *xrs, Display *dpy)
{
    xrs->dpy = dpy;
    xrs->stack = NULL;
    XrStatePush(xrs);
}

void
XrStateDeinit(XrState *xrs)
{
    while (xrs->stack) {
	XrStatePop(xrs);
    }
}

void
XrStateDestroy(XrState *xrs)
{
    XrStateDeinit(xrs);
    free(xrs);
}

void
XrStatePush(XrState *xrs)
{
    XrGState *top;

    if (xrs->stack) {
	top = XrGStateClone(xrs->stack);
    } else {
	top = XrGStateCreate(xrs->dpy);
    }

    top->next = xrs->stack;
    xrs->stack = top;
}

void
XrStatePop(XrState *xrs)
{
    XrGState *top;

    if (xrs->stack) {
	top = xrs->stack;
	xrs->stack = top->next;

	XrGStateDestroy(top);
    }
}

