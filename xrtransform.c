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
#include <math.h>

#include "xrint.h"

static XrTransform XR_TRANSFORM_DEFAULT = {
    {
	{1, 0},
	{0, 1},
	{0, 0}
    }
};

void
XrTransformInit(XrTransform *transform)
{
    *transform = XR_TRANSFORM_DEFAULT;
}

void
XrTransformDeinit(XrTransform *transform)
{
    /* nothing to do here */
}

void
XrTransformInitMatrix(XrTransform *transform,
		      double a, double b,
		      double c, double d,
		      double tx, double ty)
{
    transform->m[0][0] =  a; transform->m[0][1] =  b;
    transform->m[1][0] =  c; transform->m[1][1] =  d;
    transform->m[2][0] = tx; transform->m[2][1] = ty;
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
XrTransformMultiplyIntoLeft(XrTransform *t1, const XrTransform *t2)
{
    XrTransform new;

    XrTransformMultiply(t1, t2, &new);

    *t1 = new;
}

void
XrTransformMultiplyIntoRight(const XrTransform *t1, XrTransform *t2)
{
    XrTransform new;

    XrTransformMultiply(t1, t2, &new);

    *t2 = new;
}

void
XrTransformMultiply(const XrTransform *t1, const XrTransform *t2, XrTransform *new)
{
    int	    row, col, n;
    double  t;

    for (row = 0; row < 3; row++) {
	for (col = 0; col < 2; col++) {
	    if (row == 2)
		t = t2->m[2][col];
	    else
		t = 0;
	    for (n = 0; n < 2; n++) {
		t += t1->m[row][n] * t2->m[n][col];
	    }
	    new->m[row][col] = t;
	}
    }
}

void
XrTransformPointWithoutTranslate(XrTransform *transform, XPointDouble *pt)
{
    double new_x, new_y;

    new_x = (transform->m[0][0] * pt->x
	     + transform->m[1][0] * pt->y);
    new_y = (transform->m[0][1] * pt->x
	     + transform->m[1][1] * pt->y);

    pt->x = new_x;
    pt->y = new_y;
}

void
XrTransformPoint(XrTransform *transform, XPointDouble *pt)
{
    XrTransformPointWithoutTranslate(transform, pt);

    pt->x += transform->m[2][0];
    pt->y += transform->m[2][1];
}

void
XrTransformEigenValues(XrTransform *transform, double *lambda1, double *lambda2)
{
    /* The eigenvalues of an NxN matrix M are found by solving the polynomial:

       det(M - lI) = 0

       which for our 2x2 matrix:

       M = a b 
           c d

       gives:

       l^2 - (a+d)l + (ad - bc) = 0

       l = (a+d +/- sqrt(a^2 + 2ad + d^2 - 4(ad-bc))) / 2;
    */

    double a, b, c, d, rad;

    a = transform->m[0][0];
    b = transform->m[0][1];
    c = transform->m[1][0];
    d = transform->m[1][1];

    rad = sqrt(a*a + 2*a*d + d*d - 4*(a*d - b*c));
    *lambda1 = (a + d + rad) / 2.0;
    *lambda2 = (a + d - rad) / 2.0;
}

