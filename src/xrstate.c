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

#include <stdlib.h>
#include "xrint.h"

XrState *
XrStateCreate(Display *dpy)
{
    XrError err;
    XrState *xrs;

    xrs = malloc(sizeof(XrState));

    if (xrs) {
	err = XrStateInit(xrs, dpy);
	if (err) {
	    free(xrs);
	    return NULL;
	}
    }

    return xrs;
}

XrError
XrStateInit(XrState *xrs, Display *dpy)
{
    xrs->dpy = dpy;
    xrs->stack = NULL;
    xrs->error = XrErrorSuccess;

    return XrStatePush(xrs);
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

XrError
XrStatePush(XrState *xrs)
{
    XrGState *top;

    if (xrs->stack) {
	top = XrGStateClone(xrs->stack);
    } else {
	top = XrGStateCreate(xrs->dpy);
    }

    if (top == NULL)
	return XrErrorNoMemory;

    top->next = xrs->stack;
    xrs->stack = top;

    return XrErrorSuccess;
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

