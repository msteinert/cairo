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

static XrTransform XR_TRANSFORM_IDENTITY = {
    {
	{1, 0},
	{0, 1},
	{0, 0}
    }
};

static void
_XrTransformScalarMultiply(XrTransform *transform, double scalar);

static void
_XrTransformComputeAdjoint(XrTransform *transform);

void
_XrTransformInitIdentity(XrTransform *transform)
{
    *transform = XR_TRANSFORM_IDENTITY;
}

void
_XrTransformDeinit(XrTransform *transform)
{
    /* nothing to do here */
}

void
_XrTransformInitMatrix(XrTransform *transform,
		       double a, double b,
		       double c, double d,
		       double tx, double ty)
{
    transform->m[0][0] =  a; transform->m[0][1] =  b;
    transform->m[1][0] =  c; transform->m[1][1] =  d;
    transform->m[2][0] = tx; transform->m[2][1] = ty;
}

void
_XrTransformInitTranslate(XrTransform *transform,
			  double tx, double ty)
{
    _XrTransformInitMatrix(transform,
			   1, 0,
			   0, 1,
			   tx, ty);
}

void
_XrTransformInitScale(XrTransform *transform,
		      double sx, double sy)
{
    _XrTransformInitMatrix(transform,
			   sx,  0,
			   0, sy,
			   0, 0);
}

void
_XrTransformInitRotate(XrTransform *transform,
		       double angle)
{
    _XrTransformInitMatrix(transform,
			   cos(angle), sin(angle),
			   -sin(angle), cos(angle),
			   0, 0);
}

void
_XrTransformMultiplyIntoLeft(XrTransform *t1, const XrTransform *t2)
{
    XrTransform new;

    _XrTransformMultiply(t1, t2, &new);

    *t1 = new;
}

void
_XrTransformMultiplyIntoRight(const XrTransform *t1, XrTransform *t2)
{
    XrTransform new;

    _XrTransformMultiply(t1, t2, &new);

    *t2 = new;
}

void
_XrTransformMultiply(const XrTransform *t1, const XrTransform *t2, XrTransform *new)
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
_XrTransformDistance(XrTransform *transform, double *dx, double *dy)
{
    double new_x, new_y;

    new_x = (transform->m[0][0] * *dx
	     + transform->m[1][0] * *dy);
    new_y = (transform->m[0][1] * *dx
	     + transform->m[1][1] * *dy);

    *dx = new_x;
    *dy = new_y;
}

void
_XrTransformPoint(XrTransform *transform, double *x, double *y)
{
    _XrTransformDistance(transform, x, y);

    *x += transform->m[2][0];
    *y += transform->m[2][1];
}

void
_XrTransformBoundingBox(XrTransform *transform,
			double *x, double *y,
			double *width, double *height)
{
    int i;
    double quad_x[4], quad_y[4];
    double dx1, dy1;
    double dx2, dy2;
    double min_x, max_x;
    double min_y, max_y;

    quad_x[0] = *x;
    quad_y[0] = *y;
    _XrTransformPoint(transform, &quad_x[0], &quad_y[0]);

    dx1 = *width;
    dy1 = 0;
    _XrTransformDistance(transform, &dx1, &dy1);
    quad_x[1] = quad_x[0] + dx1;
    quad_y[1] = quad_y[0] + dy1;

    dx2 = 0;
    dy2 = *height;
    _XrTransformDistance(transform, &dx2, &dy2);
    quad_x[2] = quad_x[0] + dx2;
    quad_y[2] = quad_y[0] + dy2;

    quad_x[3] = quad_x[0] + dx1 + dx2;
    quad_y[3] = quad_y[0] + dy1 + dy2;

    min_x = max_x = quad_x[0];
    min_y = max_y = quad_y[0];

    for (i=1; i < 4; i++) {
	if (quad_x[i] < min_x)
	    min_x = quad_x[i];
	if (quad_x[i] > max_x)
	    max_x = quad_x[i];

	if (quad_y[i] < min_y)
	    min_y = quad_y[i];
	if (quad_y[i] > max_y)
	    max_y = quad_y[i];
    }

    *x = min_x;
    *y = min_y;
    *width = max_x - min_x;
    *height = max_y - min_y;
}

static void
_XrTransformScalarMultiply(XrTransform *transform, double scalar)
{
    int row, col;

    for (row = 0; row < 3; row++)
	for (col = 0; col < 2; col++)
	    transform->m[row][col] *= scalar;
}

/* This function isn't a correct adjoint in that the implicit 1 in the
   homogeneous result should actually be ad-bc instead. But, since this
   adjoint is only used in the computation of the inverse, which
   divides by det(A)=ad-bc anyway, everything works out in the end. */
static void
_XrTransformComputeAdjoint(XrTransform *transform)
{
    /* adj(A) = transpose(C:cofactor(A,i,j)) */
    double a, b, c, d, tx, ty;

    a  = transform->m[0][0]; b  = transform->m[0][1];
    c  = transform->m[1][0]; d  = transform->m[1][1];
    tx = transform->m[2][0]; ty = transform->m[2][1];

    _XrTransformInitMatrix(transform,
			   d, -b,
			   -c, a,
			   c*ty - d*tx, b*tx - a*ty);
}

XrStatus
_XrTransformComputeInverse(XrTransform *transform)
{
    /* inv(A) = 1/det(A) * adj(A) */

    double a, b, c, d, det;

    a = transform->m[0][0]; b = transform->m[0][1];
    c = transform->m[1][0]; d = transform->m[1][1];

    det = a*d - b*c;

    if (det == 0)
	return XrStatusInvalidMatrix;

    _XrTransformComputeAdjoint(transform);
    _XrTransformScalarMultiply(transform, 1 / det);

    return XrStatusSuccess;
}

void
_XrTransformEigenValues(XrTransform *transform, double *lambda1, double *lambda2)
{
    /* The eigenvalues of an NxN matrix M are found by solving the polynomial:

       det(M - lI) = 0

       The zeros in our homogeneous 3x3 matrix make this equation equal
       to that formed by the sub-matrix:

       M = a b 
           c d

       by which:

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
