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

static XrMatrix XR_MATRIX_IDENTITY = {
    {
	{1, 0},
	{0, 1},
	{0, 0}
    }
};

static void
_XrMatrixScalarMultiply(XrMatrix *matrix, double scalar);

static void
_XrMatrixComputeAdjoint(XrMatrix *matrix);

XrMatrix *
XrMatrixCreate (void)
{
    XrMatrix *matrix;

    matrix = malloc (sizeof (XrMatrix));
    if (matrix == NULL)
	return NULL;

    _XrMatrixInit (matrix);

    return matrix;
}

void
_XrMatrixInit(XrMatrix *matrix)
{
    XrMatrixSetIdentity (matrix);
}

void
_XrMatrixFini(XrMatrix *matrix)
{
    /* nothing to do here */
}

void
XrMatrixDestroy (XrMatrix *matrix)
{
    _XrMatrixFini (matrix);
    free (matrix);
}

XrStatus
XrMatrixCopy(XrMatrix *matrix, const XrMatrix *other)
{
    *matrix = *other;

    return XrStatusSuccess;
}

XrStatus
XrMatrixSetIdentity(XrMatrix *matrix)
{
    *matrix = XR_MATRIX_IDENTITY;

    return XrStatusSuccess;
}

XrStatus
XrMatrixSetAffine (XrMatrix *matrix,
		   double a, double b,
		   double c, double d,
		   double tx, double ty)
{
    matrix->m[0][0] =  a; matrix->m[0][1] =  b;
    matrix->m[1][0] =  c; matrix->m[1][1] =  d;
    matrix->m[2][0] = tx; matrix->m[2][1] = ty;

    return XrStatusSuccess;
}

XrStatus
_XrMatrixSetTranslate(XrMatrix *matrix,
		       double tx, double ty)
{
    return XrMatrixSetAffine(matrix,
			     1, 0,
			     0, 1,
			     tx, ty);
}

XrStatus
XrMatrixTranslate (XrMatrix *matrix, double tx, double ty)
{
    XrMatrix tmp;

    _XrMatrixSetTranslate(&tmp, tx, ty);

    return _XrMatrixMultiplyIntoRight(&tmp, matrix);
}

XrStatus
_XrMatrixSetScale(XrMatrix *matrix,
		   double sx, double sy)
{
    return XrMatrixSetAffine(matrix,
			     sx,  0,
			     0, sy,
			     0, 0);
}

XrStatus
XrMatrixScale (XrMatrix *matrix, double sx, double sy)
{
    XrMatrix tmp;

    _XrMatrixSetScale (&tmp, sx, sy);

    return _XrMatrixMultiplyIntoRight (&tmp, matrix);
}

XrStatus
_XrMatrixSetRotate(XrMatrix *matrix,
		   double radians)
{
    return XrMatrixSetAffine(matrix,
			     cos(radians), sin(radians),
			     -sin(radians), cos(radians),
			     0, 0);
}

XrStatus
XrMatrixRotate (XrMatrix *matrix, double radians)
{
    XrMatrix tmp;

    _XrMatrixSetRotate (&tmp, radians);

    return _XrMatrixMultiplyIntoRight (&tmp, matrix);
}

XrStatus
_XrMatrixMultiplyIntoLeft(XrMatrix *m1, const XrMatrix *m2)
{
    return XrMatrixMultiply(m1, m1, m2);
}

XrStatus
_XrMatrixMultiplyIntoRight(const XrMatrix *m1, XrMatrix *m2)
{
    return XrMatrixMultiply(m2, m1, m2);
}

XrStatus
XrMatrixMultiply(XrMatrix *result, const XrMatrix *a, const XrMatrix *b)
{
    XrMatrix r;
    int	    row, col, n;
    double  t;

    for (row = 0; row < 3; row++) {
	for (col = 0; col < 2; col++) {
	    if (row == 2)
		t = b->m[2][col];
	    else
		t = 0;
	    for (n = 0; n < 2; n++) {
		t += a->m[row][n] * b->m[n][col];
	    }
	    r.m[row][col] = t;
	}
    }

    *result = r;

    return XrStatusSuccess;
}

XrStatus
XrMatrixTransformDistance(XrMatrix *matrix, double *dx, double *dy)
{
    double new_x, new_y;

    new_x = (matrix->m[0][0] * *dx
	     + matrix->m[1][0] * *dy);
    new_y = (matrix->m[0][1] * *dx
	     + matrix->m[1][1] * *dy);

    *dx = new_x;
    *dy = new_y;

    return XrStatusSuccess;
}

XrStatus
XrMatrixTransformPoint(XrMatrix *matrix, double *x, double *y)
{
    XrMatrixTransformDistance(matrix, x, y);

    *x += matrix->m[2][0];
    *y += matrix->m[2][1];

    return XrStatusSuccess;
}

XrStatus
_XrMatrixTransformBoundingBox(XrMatrix *matrix,
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
    XrMatrixTransformPoint(matrix, &quad_x[0], &quad_y[0]);

    dx1 = *width;
    dy1 = 0;
    XrMatrixTransformDistance(matrix, &dx1, &dy1);
    quad_x[1] = quad_x[0] + dx1;
    quad_y[1] = quad_y[0] + dy1;

    dx2 = 0;
    dy2 = *height;
    XrMatrixTransformDistance(matrix, &dx2, &dy2);
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

    return XrStatusSuccess;
}

static void
_XrMatrixScalarMultiply(XrMatrix *matrix, double scalar)
{
    int row, col;

    for (row = 0; row < 3; row++)
	for (col = 0; col < 2; col++)
	    matrix->m[row][col] *= scalar;
}

/* This function isn't a correct adjoint in that the implicit 1 in the
   homogeneous result should actually be ad-bc instead. But, since this
   adjoint is only used in the computation of the inverse, which
   divides by det(A)=ad-bc anyway, everything works out in the end. */
static void
_XrMatrixComputeAdjoint(XrMatrix *matrix)
{
    /* adj(A) = transpose(C:cofactor(A,i,j)) */
    double a, b, c, d, tx, ty;

    a  = matrix->m[0][0]; b  = matrix->m[0][1];
    c  = matrix->m[1][0]; d  = matrix->m[1][1];
    tx = matrix->m[2][0]; ty = matrix->m[2][1];

    XrMatrixSetAffine(matrix,
		      d, -b,
		      -c, a,
		      c*ty - d*tx, b*tx - a*ty);
}

XrStatus
XrMatrixInvert (XrMatrix *matrix)
{
    /* inv(A) = 1/det(A) * adj(A) */
    double det;

    _XrMatrixComputeDeterminant (matrix, &det);
    
    if (det == 0)
	return XrStatusInvalidMatrix;

    _XrMatrixComputeAdjoint (matrix);
    _XrMatrixScalarMultiply (matrix, 1 / det);

    return XrStatusSuccess;
}

XrStatus
_XrMatrixComputeDeterminant(XrMatrix *matrix, double *det)
{
    double a, b, c, d;

    a = matrix->m[0][0]; b = matrix->m[0][1];
    c = matrix->m[1][0]; d = matrix->m[1][1];

    *det = a*d - b*c;

    return XrStatusSuccess;
}

XrStatus
_XrMatrixComputeEigenValues (XrMatrix *matrix, double *lambda1, double *lambda2)
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

    a = matrix->m[0][0];
    b = matrix->m[0][1];
    c = matrix->m[1][0];
    d = matrix->m[1][1];

    rad = sqrt(a*a + 2*a*d + d*d - 4*(a*d - b*c));
    *lambda1 = (a + d + rad) / 2.0;
    *lambda2 = (a + d - rad) / 2.0;

    return XrStatusSuccess;
}
