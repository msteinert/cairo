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

/* private functions */
static void
_XrPathAddSubPath(XrPath *path, XrSubPath *subpath);

XrPath *
XrPathCreate(void)
{
    XrPath *path;

    path = malloc(sizeof(XrPath));
    return path;
}

void
XrPathInit(XrPath *path)
{
    path->head = NULL;
    path->tail = NULL;
}

void
XrPathInitCopy(XrPath *path, XrPath *other)
{
    XrSubPath *subpath, *othersub;

    XrPathInit(path);

    for (othersub = other->head; othersub; othersub = othersub->next) {
	subpath = XrSubPathClone(othersub);
	_XrPathAddSubPath(path, subpath);
    }
}

void
XrPathDeinit(XrPath *path)
{
    XrSubPath *subpath;

    while (path->head) {
	subpath = path->head;
	path->head = subpath->next;
	XrSubPathDestroy(subpath);
    }
    path->tail = NULL;
}

void
XrPathDestroy(XrPath *path)
{
    free(path);
}

XrPath *
XrPathClone(XrPath *path)
{
    XrPath *clone;

    clone = XrPathCreate();
    XrPathInitCopy(clone, path);
    return clone;
}

void
XrPathGetCurrentPoint(XrPath *path, XPointDouble *pt)
{
    XrSubPathGetCurrentPoint(path->tail, pt);
}

int
XrPathNumSubPaths(XrPath *path)
{
    XrSubPath *subpath;
    int num_subpaths;
    
    num_subpaths = 0;
    for (subpath = path->head; subpath; subpath = subpath->next) {
	num_subpaths++;
    }

    return num_subpaths;
}

static void
_XrPathAddSubPath(XrPath *path, XrSubPath *subpath)
{
    subpath->next = NULL;

    if (path->tail) {
	path->tail->next = subpath;
    } else {
	path->head = subpath;
    }

    path->tail = subpath;
}

void
XrPathNewSubPath(XrPath *path)
{
    XrSubPath *subpath;

    subpath = XrSubPathCreate();
    _XrPathAddSubPath(path, subpath);
}

void
XrPathAddPoint(XrPath *path, const XPointDouble *pt)
{
    XrSubPathAddPoint(path->tail, pt);
}

void
XrPathMoveTo(XrPath *path, const XPointDouble *pt)
{
    XrSubPath *subpath;

    subpath = path->tail;

    if (subpath == NULL || subpath->num_pts > 1) {
	XrPathNewSubPath(path);
	XrPathAddPoint(path, pt);
    } else {
	XrSubPathSetCurrentPoint(subpath, pt);
    }
}

void
XrPathLineTo(XrPath *path, const XPointDouble *pt)
{
    XrPathAddPoint(path, pt);
}

void
XrPathClose(XrPath *path)
{
    XrSubPathClose(path->tail);
    XrPathNewSubPath(path);
}
