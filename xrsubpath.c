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

#define XR_SUBPATH_GROWTH_INC 10

XrSubPath *
XrSubPathCreate(void)
{
    XrSubPath *path;

    path = malloc(sizeof(XrSubPath));

    XrSubPathInit(path);

    return path;
}

void
XrSubPathInit(XrSubPath *path)
{
    path->num_pts = 0;

    path->pts_size = 0;
    path->pts = NULL;

    path->closed = 0;

    path->next = NULL;
}

void
XrSubPathInitCopy(XrSubPath *path, XrSubPath *other)
{
    *path = *other;

    path->pts = malloc(path->pts_size * sizeof(XPointDouble));
    *path->pts = *other->pts;
}

void
XrSubPathDeinit(XrSubPath *path)
{
    if (path->pts_size) {
	free(path->pts);
	path->pts_size = 0;
	path->num_pts = 0;
    }
}

void
XrSubPathDestroy(XrSubPath *path)
{
    XrSubPathDeinit(path);
    free(path);
}

XrSubPath *
XrSubPathClone(XrSubPath *path)
{
    XrSubPath *clone;

    clone = XrSubPathCreate();
    XrSubPathInitCopy(clone, path);

    return clone;
}

void
XrSubPathGetCurrentPoint(XrSubPath *path, XPointDouble *pt)
{
    if (path->num_pts) {
	*pt = path->pts[path->num_pts-1];
    } else {
	/* XXX: What to do for error handling? */
    }
}

void
XrSubPathSetCurrentPoint(XrSubPath *path, const XPointDouble *pt)
{
    if (path->num_pts) {
	path->pts[path->num_pts - 1] = *pt;
    } else {
	/* XXX: What to do for error handling? */
    }
}

static void
_XrSubPathGrow(XrSubPath *path)
{
    XPointDouble *new_pts;

    path->pts_size += XR_SUBPATH_GROWTH_INC;
    new_pts = realloc(path->pts, path->pts_size * sizeof(XPointDouble));

    if (new_pts) {
	path->pts = new_pts;
    } else {
	path->pts_size -= XR_SUBPATH_GROWTH_INC;
    }
}

void
XrSubPathAddPoint(XrSubPath *path, const XPointDouble *pt)
{
    if (path->num_pts == path->pts_size) {
	_XrSubPathGrow(path);
    }

    if (path->num_pts < path->pts_size) {
	path->pts[path->num_pts++] = *pt;
    }
}

void
XrSubPathClose(XrSubPath *path)
{
    path->closed = 1;
}
