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
#include <math.h>

#include "xrint.h"

static XrTransform XR_TRANSFORM_DEFAULT = {
    {1, 0,
     0, 1,
     0, 0}
};

void
XrTransformInit(XrTransform *transform)
{
    *transform = XR_TRANSFORM_DEFAULT;
}

void
XrTransformInitMatrix(XrTransform *transform,
		      double a, double b,
		      double c, double d,
		      double tx, double ty)
{
    transform->matrix[0] =  a; transform->matrix[1] =  b;
    transform->matrix[2] =  c; transform->matrix[3] =  d;
    transform->matrix[4] = tx; transform->matrix[5] = ty;
}

void
XrTransformInitTranslate(XrTransform *transform,
			 double tx, double ty)
{
    XrTransformInitMatrix(transform,
			  1, 0,
			  0, 1,
			  tx, ty);
}

void
XrTransformInitScale(XrTransform *transform,
		     double sx, double sy)
{
    XrTransformInitMatrix(transform,
			  sx,  0,
			  0, sy,
			  0, 0);
}

void
XrTransformInitRotate(XrTransform *transform,
		      double angle)
{
    XrTransformInitMatrix(transform,
			  cos(angle), sin(angle),
			  -sin(angle), cos(angle),
			  0, 0);
}

void
XrTransformDeinit(XrTransform *transform)
{
    /* Nothing to do here */
}

void
XrTransformCompose(XrTransform *t1, const XrTransform *t2)
{
    double new[6];

    new[0] = t2->matrix[0] * t1->matrix[0] + t2->matrix[1] * t1->matrix[2];
    new[1] = t2->matrix[0] * t1->matrix[1] + t2->matrix[1] * t1->matrix[3];
    new[2] = t2->matrix[2] * t1->matrix[0] + t2->matrix[3] * t1->matrix[2];
    new[3] = t2->matrix[2] * t1->matrix[1] + t2->matrix[3] * t1->matrix[3];
    new[4] = t2->matrix[4] * t1->matrix[0] + t2->matrix[5] * t1->matrix[2] + t1->matrix[4];
    new[5] = t2->matrix[4] * t1->matrix[1] + t2->matrix[5] * t1->matrix[3] + t1->matrix[5];

    memcpy(t1->matrix, new, 6 * sizeof(double));
}

void
XrTransformPointWithoutTranslate(XrTransform *transform, XPointDouble *pt)
{
    double new_x, new_y;

    new_x = (transform->matrix[0] * pt->x
	     + transform->matrix[2] * pt->y);
    new_y = (transform->matrix[1] * pt->x
	     + transform->matrix[3] * pt->y);

    pt->x = new_x;
    pt->y = new_y;
}

void
XrTransformPoint(XrTransform *transform, XPointDouble *pt)
{
    XrTransformPointWithoutTranslate(transform, pt);

    pt->x += transform->matrix[4];
    pt->y += transform->matrix[5];
}
