/*
 * $XFree86: $
 *
 * Copyright © 2003 USC, Information Sciences Institute
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of
 * Information Sciences Institute not be used in advertising or
 * publicity pertaining to distribution of the software without
 * specific, written prior permission.  Information Sciences Institute
 * makes no representations about the suitability of this software for
 * any purpose.  It is provided "as is" without express or implied
 * warranty.
 *
 * INFORMATION SCIENCES INSTITUTE DISCLAIMS ALL WARRANTIES WITH REGARD
 * TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL INFORMATION SCIENCES
 * INSTITUTE BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA
 * OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include "xrint.h"

typedef struct _XrPathBounder {
    int has_pt;

    XFixed min_x;
    XFixed min_y;
    XFixed max_x;
    XFixed max_y;
} XrPathBounder;

static void
_XrPathBounderInit(XrPathBounder *bounder);

static void
_XrPathBounderDeinit(XrPathBounder *bounder);

static XrStatus
_XrPathBounderAddPoint(XrPathBounder *bounder, XPointFixed *pt);

static XrStatus
_XrPathBounderAddEdge(void *closure, XPointFixed *p1, XPointFixed *p2);

static XrStatus
_XrPathBounderAddSpline(void *closure,
			XPointFixed *a, XPointFixed *b, XPointFixed *c, XPointFixed *d);

static XrStatus
_XrPathBounderDoneSubPath(void *closure, XrSubPathDone done);

static XrStatus
_XrPathBounderDonePath(void *closure);

static void
_XrPathBounderInit(XrPathBounder *bounder)
{
    bounder->has_pt = 0;
}

static void
_XrPathBounderDeinit(XrPathBounder *bounder)
{
    bounder->has_pt = 0;
}

static XrStatus
_XrPathBounderAddPoint(XrPathBounder *bounder, XPointFixed *pt)
{
    if (bounder->has_pt) {
	if (pt->x < bounder->min_x)
	    bounder->min_x = pt->x;
	
	if (pt->y < bounder->min_y)
	    bounder->min_y = pt->y;
	
	if (pt->x > bounder->max_x)
	    bounder->max_x = pt->x;
	
	if (pt->y > bounder->max_y)
	    bounder->max_y = pt->y;
    } else {
	bounder->min_x = pt->x;
	bounder->min_y = pt->y;
	bounder->max_x = pt->x;
	bounder->max_y = pt->y;

	bounder->has_pt = 1;
    }
	
    return XrStatusSuccess;
}

static XrStatus
_XrPathBounderAddEdge(void *closure, XPointFixed *p1, XPointFixed *p2)
{
    XrPathBounder *bounder = closure;

    _XrPathBounderAddPoint(bounder, p1);
    _XrPathBounderAddPoint(bounder, p2);

    return XrStatusSuccess;
}

static XrStatus
_XrPathBounderAddSpline(void *closure,
			XPointFixed *a, XPointFixed *b, XPointFixed *c, XPointFixed *d)
{
    XrPathBounder *bounder = closure;

    _XrPathBounderAddPoint(bounder, a);
    _XrPathBounderAddPoint(bounder, b);
    _XrPathBounderAddPoint(bounder, c);
    _XrPathBounderAddPoint(bounder, d);

    return XrStatusSuccess;
}

static XrStatus
_XrPathBounderDoneSubPath(void *closure, XrSubPathDone done)
{
    return XrStatusSuccess;
}

static XrStatus
_XrPathBounderDonePath(void *closure)
{
    return XrStatusSuccess;
}

/* XXX: Perhaps this should compute a PixRegion rather than 4 doubles */
XrStatus
_XrPathBounds(XrPath *path, double *x1, double *y1, double *x2, double *y2)
{
    XrStatus status;
    static XrPathCallbacks cb = {
	_XrPathBounderAddEdge,
	_XrPathBounderAddSpline,
	_XrPathBounderDoneSubPath,
	_XrPathBounderDonePath
    };

    XrPathBounder bounder;

    _XrPathBounderInit(&bounder);

    status = _XrPathInterpret(path, XrPathDirectionForward, &cb, &bounder);
    if (status) {
	*x1 = *y1 = *x2 = *y2 = 0.0;
	_XrPathBounderDeinit(&bounder);
	return status;
    }

    *x1 = XFixedToDouble(bounder.min_x);
    *y1 = XFixedToDouble(bounder.min_y);
    *x2 = XFixedToDouble(bounder.max_x);
    *y2 = XFixedToDouble(bounder.max_y);

    _XrPathBounderDeinit(&bounder);

    return XrStatusSuccess;
}
