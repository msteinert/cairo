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
_XrStateCreate(Display *dpy)
{
    XrStatus status;
    XrState *xrs;

    xrs = malloc(sizeof(XrState));

    if (xrs) {
	status = _XrStateInit(xrs, dpy);
	if (status) {
	    free(xrs);
	    return NULL;
	}
    }

    return xrs;
}

XrStatus
_XrStateInit(XrState *xrs, Display *dpy)
{
    xrs->dpy = dpy;
    xrs->stack = NULL;
    xrs->status = XrStatusSuccess;

    return _XrStatePush(xrs);
}

void
_XrStateDeinit(XrState *xrs)
{
    while (xrs->stack) {
	_XrStatePop(xrs);
    }
}

void
_XrStateDestroy(XrState *xrs)
{
    _XrStateDeinit(xrs);
    free(xrs);
}

XrStatus
_XrStatePush(XrState *xrs)
{
    XrGState *top;

    if (xrs->stack) {
	top = _XrGStateClone(xrs->stack);
    } else {
	top = _XrGStateCreate(xrs->dpy);
    }

    if (top == NULL)
	return XrStatusNoMemory;

    top->next = xrs->stack;
    xrs->stack = top;

    return XrStatusSuccess;
}

void
_XrStatePop(XrState *xrs)
{
    XrGState *top;

    if (xrs->stack) {
	top = xrs->stack;
	xrs->stack = top->next;

	_XrGStateDestroy(top);
    }
}

