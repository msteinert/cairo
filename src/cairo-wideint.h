/*
 * $Id: cairo-wideint.h,v 1.1 2004-05-28 19:37:15 keithp Exp $
 *
 * Copyright © 2004 Keith Packard
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Keith Packard not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Keith Packard makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * KEITH PACKARD DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL KEITH PACKARD BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef CAIRO_WIDEINT_H
#define CAIRO_WIDEINT_H

#include <stdint.h>

/*
 * 64-bit datatypes.  Two separate implementations, one using
 * built-in 64-bit signed/unsigned types another implemented
 * as a pair of 32-bit ints
 */

#define I __internal_linkage

#if !HAVE_UINT64_T

typedef struct _cairo_uint64 {
    uint32_t	lo, hi;
} cairo_uint64_t, cairo_int64_t;

const cairo_uint64_t I	_cairo_uint32_to_uint64 (uint32_t i);
#define			_cairo_uint64_to_uint32(a)  ((a).lo)
const cairo_uint64_t I	_cairo_uint64_add (cairo_uint64_t a, cairo_uint64_t b);
const cairo_uint64_t I	_cairo_uint64_sub (cairo_uint64_t a, cairo_uint64_t b);
const cairo_uint64_t I	_cairo_uint64_mul (cairo_uint64_t a, cairo_uint64_t b);
const cairo_uint64_t I	_cairo_uint32x32_64_mul (uint32_t a, uint32_t b);
const cairo_uint64_t I	_cairo_uint64_lsl (cairo_uint64_t a, int shift);
const cairo_uint64_t I	_cairo_uint64_rsl (cairo_uint64_t a, int shift);
const cairo_uint64_t I	_cairo_uint64_rsa (cairo_uint64_t a, int shift);
const int		_cairo_uint64_lt (cairo_uint64_t a, cairo_uint64_t b);
const int		_cairo_uint64_eq (cairo_uint64_t a, cairo_uint64_t b);
const cairo_uint64_t I	_cairo_uint64_negate (cairo_uint64_t a);
#define			_cairo_uint64_negative(a)   (((int32_t) ((a).hi)) < 0)
const cairo_uint64_t I	_cairo_uint64_not (cairo_uint64_t a);

#define			_cairo_uint64_to_int64(i)   (i)
#define			_cairo_int64_to_uint64(i)   (i)

const cairo_int64_t I	_cairo_int32_to_int64(int32_t i);
#define			_cairo_int64_to_int32(a)    ((int32_t) _cairo_uint64_to_uint32(a))
#define			_cairo_int64_add(a,b)	    _cairo_uint64_add (a,b)
#define			_cairo_int64_sub(a,b)	    _cairo_uint64_sub (a,b)
#define			_cairo_int64_mul(a,b)	    _cairo_uint64_mul (a,b)
#define			_cairo_int32x32_64_mul(a,b) _cairo_uint32x32_64_mul ((uint32_t) (a), (uint32_t) (b)))
const int		_cairo_int64_lt (cairo_uint64_t a, cairo_uint64_t b);
#define			_cairo_int64_eq(a,b)	    _cairo_uint64_eq (a,b)
#define			_cairo_int64_lsl(a,b)	    _cairo_uint64_lsl (a,b)
#define			_cairo_int64_rsl(a,b)	    _cairo_uint64_rsl (a,b)
#define			_cairo_int64_rsa(a,b)	    _cairo_uint64_rsa (a,b)
#define			_cairo_int64_negate(a)	    _cairo_uint64_negate(a)
#define			_cairo_int64_negative(a)    (((int32_t) ((a).hi)) < 0)
#define			_cairo_int64_not(a)	    _cairo_uint64_not(a)

#else

typedef uint64_t    cairo_uint64_t;
typedef int64_t	    cairo_int64_t;

#define			_cairo_uint32_to_uint64(i)  ((uint64_t) (i))
#define			_cairo_uint64_to_uint32(i)  ((uint32_t) (i))
#define			_cairo_uint64_add(a,b)	    ((a) + (b))
#define			_cairo_uint64_sub(a,b)	    ((a) - (b))
#define			_cairo_uint64_mul(a,b)	    ((a) * (b))
#define			_cairo_uint32x32_64_mul(a,b)	((uint64_t) (a) * (b))
#define			_cairo_uint64_lsl(a,b)	    ((a) << (b))
#define			_cairo_uint64_rsl(a,b)	    ((uint64_t) (a) >> (b))
#define			_cairo_uint64_rsa(a,b)	    ((uint64_t) ((int64_t) (a) >> (b)))
#define			_cairo_uint64_lt(a,b)	    ((a) < (b))
#define			_cairo_uint64_eq(a,b)	    ((a) == (b))
#define			_cairo_uint64_negate(a)	    ((uint64_t) -((int64_t) (a)))
#define			_cairo_uint64_negative(a)   ((int64_t) (a) < 0)
#define			_cairo_uint64_not(a)	    (~(a))

#define			_cairo_uint64_to_int64(i)   ((int64_t) (i))
#define			_cairo_int64_to_uint64(i)   ((uint64_t) (i))

#define			_cairo_int32_to_int64(i)    ((int64_t) (i))
#define			_cairo_int64_to_int32(i)    ((int32_t) (i))
#define			_cairo_int64_add(a,b)	    ((a) + (b))
#define			_cairo_int64_sub(a,b)	    ((a) - (b))
#define			_cairo_int64_mul(a,b)	    ((a) * (b))
#define			_cairo_int32x32_64_mul(a,b) ((int64_t) (a) * (b))
#define			_cairo_int64_lt(a,b)	    ((a) < (b))
#define			_cairo_int64_eq(a,b)	    ((a) == (b))
#define			_cairo_int64_lsl(a,b)	    ((a) << (b))
#define			_cairo_int64_rsl(a,b)	    ((int64_t) ((uint64_t) (a) >> (b)))
#define			_cairo_int64_rsa(a,b)	    ((int64_t) (a) >> (b))
#define			_cairo_int64_negate(a)	    (-(a))
#define			_cairo_int64_negative(a)    ((a) < 0)
#define			_cairo_int64_not(a)	    (~(a))

#endif

/*
 * 64-bit comparisions derived from lt or eq
 */
#define			_cairo_uint64_le(a,b)	    (!_cairo_uint64_gt(a,b))
#define			_cairo_uint64_ne(a,b)	    (!_cairo_uint64_eq(a,b))
#define			_cairo_uint64_ge(a,b)	    (!_cairo_uint64_lt(a,b))
#define			_cairo_uint64_gt(a,b)	    _cairo_uint64_lt(b,a)

#define			_cairo_int64_le(a,b)	    (!_cairo_int64_gt(a,b))
#define			_cairo_int64_ne(a,b)	    (!_cairo_int64_eq(a,b))
#define			_cairo_int64_ge(a,b)	    (!_cairo_int64_lt(a,b))
#define			_cairo_int64_gt(a,b)	    _cairo_int64_lt(b,a)

/*
 * As the C implementation always computes both, create
 * a function which returns both for the 'native' type as well
 */

typedef struct _cairo_uquorem64 {
    cairo_uint64_t	quo;
    cairo_uint64_t	rem;
} cairo_uquorem64_t;

typedef struct _cairo_quorem64 {
    cairo_int64_t	quo;
    cairo_int64_t	rem;
} cairo_quorem64_t;

const cairo_uquorem64_t I
_cairo_uint64_divrem (cairo_uint64_t num, cairo_uint64_t den);

const cairo_quorem64_t I
_cairo_int64_divrem (cairo_int64_t num, cairo_int64_t den);

/*
 * 128-bit datatypes.  Again, provide two implementations in
 * case the machine has a native 128-bit datatype.  GCC supports int128_t
 * on ia64
 */
 
#if !HAVE_UINT128_T

typedef struct cairo_uint128 {
    cairo_uint64_t	lo, hi;
} cairo_uint128_t, cairo_int128_t;

const cairo_uint128_t I	_cairo_uint32_to_uint128 (uint32_t i);
const cairo_uint128_t I	_cairo_uint64_to_uint128 (cairo_uint64_t i);
#define			_cairo_uint128_to_uint64(a)	((a).lo)
#define			_cairo_uint128_to_uint32(a)	_cairo_uint64_to_uint32(_cairo_uint128_to_uint64(a))
const cairo_uint128_t I	_cairo_uint128_add (cairo_uint128_t a, cairo_uint128_t b);
const cairo_uint128_t I	_cairo_uint128_sub (cairo_uint128_t a, cairo_uint128_t b);
const cairo_uint128_t I	_cairo_uint128_mul (cairo_uint128_t a, cairo_uint128_t b);
const cairo_uint128_t I	_cairo_uint64x64_128_mul (cairo_uint64_t a, cairo_uint64_t b);
const cairo_uint128_t I	_cairo_uint128_lsl (cairo_uint128_t a, int shift);
const cairo_uint128_t I	_cairo_uint128_rsl (cairo_uint128_t a, int shift);
const cairo_uint128_t I	_cairo_uint128_rsa (cairo_uint128_t a, int shift);
const int		_cairo_uint128_lt (cairo_uint128_t a, cairo_uint128_t b);
const int		_cairo_uint128_eq (cairo_uint128_t a, cairo_uint128_t b);
const cairo_uint128_t I	_cairo_uint128_negate (cairo_uint128_t a);
#define			_cairo_uint128_negative(a)  (_cairo_uint64_negative(a.hi))
const cairo_uint128_t I	_cairo_uint128_not (cairo_uint128_t a);

#define			_cairo_uint128_to_int128_(i)	(i)
#define			_cairo_int128_to_uint128(i)	(i)

const cairo_int128_t I	_cairo_int32_to_int128 (int32_t i);
const cairo_int128_t I	_cairo_int64_to_int128 (cairo_int64_t i);
#define			_cairo_int128_to_int64(a)   ((cairo_int64_t) (a).lo);
#define			_cairo_int128_to_int32(a)   _cairo_int64_to_int32(_cairo_int128_to_int64(a))
#define			_cairo_int128_add(a,b)	    _cairo_uint128_add(a,b)
#define			_cairo_int128_sub(a,b)	    _cairo_uint128_sub(a,b)
#define			_cairo_int128_mul(a,b)	    _cairo_uint128_mul(a,b)
#define			_cairo_int64x64_128_mul(a,b) _cairo_uint64x64_128_mul ((cairo_uint64_t) (a), (cairo_uint64_t) (b))
#define			_cairo_int128_lsl(a,b)	    _cairo_uint128_lsl(a,b)
#define			_cairo_int128_rsl(a,b)	    _cairo_uint128_rsl(a,b)
#define			_cairo_int128_rsa(a,b)	    _cairo_uint128_rsa(a,b)
const int		_cairo_int128_lt (cairo_int128_t a, cairo_int128_t b);
#define			_cairo_int128_eq(a,b)	    _cairo_uint128_eq (a,b)
#define			_cairo_int128_negate(a)	    _cairo_uint128_negate(a)
#define			_cairo_int128_negative(a)   (_cairo_uint128_negative(a))
#define			_cairo_int128_not(a)	    _cairo_uint128_not(a)

#else	/* !HAVE_UINT128_T */

typedef uint128_t	cairo_uint128_t;
typedef int128_t	cairo_int128_t;

#define			_cairo_uint32_to_uint128(i) ((uint128_t) (i))
#define			_cairo_uint64_to_uint128(i) ((uint128_t) (i))
#define			_cairo_uint128_to_uint64(i) ((uint64_t) (i))
#define			_cairo_uint128_to_uint32(i) ((uint32_t) (i))
#define			_cairo_uint128_add(a,b)	    ((a) + (b))
#define			_cairo_uint128_sub(a,b)	    ((a) - (b))
#define			_cairo_uint128_mul(a,b)	    ((a) * (b))
#define			_cairo_uint64x64_128_mul(a,b)	((uint128_t) (a) * (b))
#define			_cairo_uint128_lsl(a,b)	    ((a) << (b))
#define			_cairo_uint128_rsl(a,b)	    ((uint128_t) (a) >> (b))
#define			_cairo_uint128_rsa(a,b)	    ((uint128_t) ((int128_t) (a) >> (b)))
#define			_cairo_uint128_lt(a,b)	    ((a) < (b))
#define			_cairo_uint128_eq(a,b)	    ((a) == (b))
#define			_cairo_uint128_negate(a)    ((uint128_t) -((int128_t) (a)))
#define			_cairo_uint128_negative(a)  ((int128_t) (a) < 0)
#define			_cairo_uint128_not(a)	    (~(a))

#define			_cairo_uint128_to_int128(i) ((int128_t) (i))
#define			_cairo_int128_to_uint128(i) ((uint128_t) (i))

#define			_cairo_int32_to_int128(i)   ((int128_t) (i))
#define			_cairo_int64_to_int128(i)   ((int128_t) (i))
#define			_cairo_int128_to_int64(i)   ((int64_t) (i))
#define			_cairo_int128_to_int32(i)   ((int32_t) (i))
#define			_cairo_int128_add(a,b)	    ((a) + (b))
#define			_cairo_int128_sub(a,b)	    ((a) - (b))
#define			_cairo_int128_mul(a,b)	    ((a) * (b))
#define			_cairo_int64x64_128_mul(a,b) ((int128_t) (a) * (b))
#define			_cairo_int128_lt(a,b)	    ((a) < (b))
#define			_cairo_int128_eq(a,b)	    ((a) == (b))
#define			_cairo_int128_lsl(a,b)	    ((a) << (b))
#define			_cairo_int128_rsl(a,b)	    ((int128_t) ((uint128_t) (a) >> (b)))
#define			_cairo_int128_rsa(a,b)	    ((int128_t) (a) >> (b))
#define			_cairo_int128_negate(a)	    (-(a))
#define			_cairo_int128_negative(a)   ((a) < 0)
#define			_cairo_int128_not(a)	    (~(a))

#endif	/* HAVE_UINT128_T */

typedef struct _cairo_uquorem128 {
    cairo_uint128_t	quo;
    cairo_uint128_t	rem;
} cairo_uquorem128_t;

typedef struct _cairo_quorem128 {
    cairo_int128_t	quo;
    cairo_int128_t	rem;
} cairo_quorem128_t;

const cairo_uquorem128_t I
_cairo_uint128_divrem (cairo_uint128_t num, cairo_uint128_t den);

const cairo_quorem128_t I
_cairo_int128_divrem (cairo_int128_t num, cairo_int128_t den);

#define			_cairo_uint128_le(a,b)	    (!_cairo_uint128_gt(a,b))
#define			_cairo_uint128_ne(a,b)	    (!_cairo_uint128_eq(a,b))
#define			_cairo_uint128_ge(a,b)	    (!_cairo_uint128_lt(a,b))
#define			_cairo_uint128_gt(a,b)	    _cairo_uint128_lt(b,a)

#define			_cairo_int128_le(a,b)	    (!_cairo_int128_gt(a,b))
#define			_cairo_int128_ne(a,b)	    (!_cairo_int128_eq(a,b))
#define			_cairo_int128_ge(a,b)	    (!_cairo_int128_lt(a,b))
#define			_cairo_int128_gt(a,b)	    _cairo_int128_lt(b,a)

#undef I

#endif /* CAIRO_WIDEINT_H */
