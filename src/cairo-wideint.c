/*
 * $Id: cairo-wideint.c,v 1.1 2004-05-28 19:37:15 keithp Exp $
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

#include "cairoint.h"

#if !HAVE_UINT64_T || !HAVE_UINT128_T

static const unsigned char top_bit[256] =
{
    0,1,2,2,3,3,3,3,4,4,4,4,4,4,4,4,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
    6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
    7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
    7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
    8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
    8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
    8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
    8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
};

#endif

#if HAVE_UINT64_T

#define _cairo_uint32s_to_uint64(h,l) ((uint64_t) (h) << 32 | (l))

const cairo_uquorem64_t
_cairo_uint64_divrem (cairo_uint64_t num, cairo_uint64_t den)
{
    cairo_uquorem64_t	qr;

    qr.quo = num / den;
    qr.rem = num % den;
    return qr;
}

#else

const cairo_uint64_t
_cairo_uint32_to_uint64 (uint32_t i)
{
    cairo_uint64_t	q;

    q.lo = i;
    q.hi = 0;
    return q;
}

const cairo_int64_t
_cairo_int32_to_int64 (int32_t i)
{
    cairo_uint64_t	q;

    q.lo = i;
    q.hi = i < 0 ? -1 : 0;
    return q;
}

static const cairo_uint64_t
_cairo_uint32s_to_uint64 (uint32_t h, uint32_t l)
{
    cairo_uint64_t	q;

    q.lo = l;
    q.hi = h;
    return q;
}

const cairo_uint64_t
_cairo_uint64_add (cairo_uint64_t a, cairo_uint64_t b)
{
    cairo_uint64_t	s;

    s.hi = a.hi + b.hi;
    s.lo = a.lo + b.lo;
    if (s.lo < a.lo)
	s.hi++;
    return s;
}

const cairo_uint64_t
_cairo_uint64_sub (cairo_uint64_t a, cairo_uint64_t b)
{
    cairo_uint64_t	s;

    s.hi = a.hi - b.hi;
    s.lo = a.lo - b.lo;
    if (s.lo > a.lo)
	s.hi--;
    return s;
}

#define uint32_lo(i)	((i) & 0xffff)
#define uint32_hi(i)	((i) >> 16)
#define uint32_carry16	((1) << 16)

const cairo_uint64_t
_cairo_uint32x32_64_mul (uint32_t a, uint32_t b)
{
    cairo_uint64_t  s;
    
    uint16_t	ah, al, bh, bl;
    uint32_t	r0, r1, r2, r3;

    al = uint32_lo (a);
    ah = uint32_hi (a);
    bl = uint32_lo (b);
    bh = uint32_hi (b);

    r0 = (uint32_t) al * bl;
    r1 = (uint32_t) al * bh;
    r2 = (uint32_t) ah * bl;
    r3 = (uint32_t) ah * bh;

    r1 += uint32_hi(r0);    /* no carry possible */
    r1 += r2;		    /* but this can carry */
    if (r1 < r2)	    /* check */
	r3 += uint32_carry16;

    s.hi = r3 + uint32_hi(r1);
    s.lo = (uint32_lo (r1) << 16) + uint32_lo (r0);
    return s;
}

const cairo_uint64_t
_cairo_uint64_mul (cairo_uint64_t a, cairo_uint64_t b)
{
    cairo_uint64_t	s;
    
    s = _cairo_uint32x32_64_mul (a.lo, b.lo);
    s.hi += a.lo * b.hi + a.hi * b.lo;
    return s;
}

const cairo_uint64_t
_cairo_uint64_lsl (cairo_uint64_t a, int shift)
{
    if (shift >= 32)
    {
	a.hi = a.lo;
	a.lo = 0;
	shift -= 32;
    }
    if (shift)
    {
	a.hi = a.hi << shift | a.lo >> (32 - shift);
	a.lo = a.lo << shift;
    }
    return a;
}

const cairo_uint64_t
_cairo_uint64_rsl (cairo_uint64_t a, int shift)
{
    if (shift >= 32)
    {
	a.lo = a.hi;
	a.hi = 0;
	shift -= 32;
    }
    if (shift)
    {
	a.lo = a.lo >> shift | a.hi << (32 - shift);
	a.hi = a.hi >> shift;
    }
    return a;
}

#define _cairo_uint32_rsa(a,n)	((uint32_t) (((int32_t) (a)) >> (n)))

const cairo_int64_t
_cairo_uint64_rsa (cairo_int64_t a, int shift)
{
    if (shift >= 32)
    {
	a.lo = a.hi;
	a.hi = _cairo_uint32_rsa (a.hi, 31);
	shift -= 32;
    }
    if (shift)
    {
	a.lo = a.lo >> shift | a.hi << (32 - shift);
	a.hi = _cairo_uint32_rsa (a.hi, shift);
    }
    return a;
}

const int
_cairo_uint64_lt (cairo_uint64_t a, cairo_uint64_t b)
{
    return (a.hi < b.hi ||
	    (a.hi == b.hi && a.lo < b.lo));
}

const int
_cairo_uint64_eq (cairo_uint64_t a, cairo_uint64_t b)
{
    return a.hi == b.hi && a.lo == b.lo;
}

const int
_cairo_int64_lt (cairo_int64_t a, cairo_int64_t b)
{
    if (_cairo_int64_negative (a) && !_cairo_int64_negative (b))
	return 1;
    if (!_cairo_int64_negative (a) && _cairo_int64_negative (b))
	return 0;
    return _cairo_uint64_lt (a, b);
}

const cairo_uint64_t
_cairo_uint64_not (cairo_uint64_t a)
{
    a.lo = ~a.lo;
    a.hi = ~a.hi;
    return a;
}

const cairo_uint64_t
_cairo_uint64_negate (cairo_uint64_t a)
{
    a.lo = ~a.lo;
    a.hi = ~a.hi;
    if (++a.lo == 0)
	++a.hi;
    return a;
}

/*
 * The design of this algorithm comes from GCC,
 * but the actual implementation is new
 */

static const int
_cairo_leading_zeros32 (uint32_t i)
{
    int	top;
    
    if (i < 0x100)
	top = 0;
    else if (i < 0x10000)
	top = 8;
    else if (i < 0x1000000)
	top = 16;
    else
	top = 24;
    top = top + top_bit [i >> top];
    return 32 - top;
}

typedef struct _cairo_uquorem32_t {
    uint32_t	quo;
    uint32_t	rem;
} cairo_uquorem32_t;

/*
 * den >= num.hi
 */
static const cairo_uquorem32_t
_cairo_uint64x32_normalized_divrem (cairo_uint64_t num, uint32_t den)
{
    cairo_uquorem32_t	qr;
    uint32_t		q0, q1, r0, r1;
    uint16_t		d0, d1;
    uint32_t		t;

    d0 = den & 0xffff;
    d1 = den >> 16;

    q1 = num.hi / d1;
    r1 = num.hi % d1;

    t = q1 * d0;
    r1 = (r1 << 16) | (num.lo >> 16);
    if (r1 < t)
    {
	q1--;
	r1 += den;
	if (r1 >= den && r1 < t)
	{
	    q1--;
	    r1 += den;
	}
    }

    r1 -= t;

    q0 = r1 / d1;
    r0 = r1 % d1;
    t = q0 * d0;
    r0 = (r0 << 16) | (num.lo & 0xffff);
    if (r0 < t)
    {
	q0--;
	r0 += den;
	if (r0 >= den && r0 < t)
	{
	    q0--;
	    r0 += den;
	}
    }
    r0 -= t;
    qr.quo = (q1 << 16) | q0;
    qr.rem = r0;
    return qr;
}

const cairo_uquorem64_t
_cairo_uint64_divrem (cairo_uint64_t num, cairo_uint64_t den)
{
    cairo_uquorem32_t	qr32;
    cairo_uquorem64_t	qr;
    int			norm;
    uint32_t		q1, q0, r1, r0;

    if (den.hi == 0)
    {
	if (den.lo > num.hi)
	{
	    /* 0q = nn / 0d */

	    norm = _cairo_leading_zeros32 (den.lo);
	    if (norm)
	    {
		den.lo <<= norm;
		num = _cairo_uint64_lsl (num, norm);
	    }
	    q1 = 0;
	}
	else
	{
	    /* qq = NN / 0d */

	    if (den.lo == 0)
		den.lo = 1 / den.lo;

	    norm = _cairo_leading_zeros32 (den.lo);
	    if (norm)
	    {
		cairo_uint64_t	num1;
		den.lo <<= norm;
		num1 = _cairo_uint64_rsl (num, 32 - norm);
		qr32 = _cairo_uint64x32_normalized_divrem (num1, den.lo);
		q1 = qr32.quo;
		num.hi = qr32.rem;
		num.lo <<= norm;
	    }
	    else
	    {
		num.hi -= den.lo;
		q1 = 1;
	    }
	}
	qr32 = _cairo_uint64x32_normalized_divrem (num, den.lo);
	q0 = qr32.quo;
	r1 = 0;
	r0 = qr32.rem >> norm;
    }
    else
    {
	if (den.hi > num.hi)
	{
	    /* 00 = nn / DD */
	    q0 = q1 = 0;
	    r0 = num.lo;
	    r1 = num.hi;
	}
	else
	{
	    /* 0q = NN / dd */

	    norm = _cairo_leading_zeros32 (den.hi);
	    if (norm == 0)
	    {
		if (num.hi > den.hi || num.lo >= den.lo)
		{
		    q0 = 1;
		    num = _cairo_uint64_sub (num, den);
		}
		else
		    q0 = 0;
		
		q1 = 0;
		r0 = num.lo;
		r1 = num.hi;
	    }
	    else
	    {
		cairo_uint64_t	num1;
		cairo_uint64_t	part;

		num1 = _cairo_uint64_rsl (num, 32 - norm);
		den = _cairo_uint64_lsl (den, norm);

		qr32 = _cairo_uint64x32_normalized_divrem (num1, den.hi);
		part = _cairo_uint32x32_64_mul (qr32.quo, den.lo);

		q0 = qr32.quo;
		
		num.lo <<= norm;
		num.hi = qr32.rem;
		
		if (_cairo_uint64_gt (part, num))
		{
		    q0--;
		    part = _cairo_uint64_sub (part, den);
		}

		q1 = 0;

		num = _cairo_uint64_sub (num, part);
		num = _cairo_uint64_rsl (num, norm);
		r0 = num.lo;
		r1 = num.hi;
	    }
	}
    }
    qr.quo.lo = q0;
    qr.quo.hi = q1;
    qr.rem.lo = r0;
    qr.rem.hi = r1;
    return qr;
}

#endif /* !HAVE_UINT64_T */

const cairo_quorem64_t
_cairo_int64_divrem (cairo_int64_t num, cairo_int64_t den)
{
    int			num_neg = _cairo_int64_negative (num);
    int			den_neg = _cairo_int64_negative (den);
    cairo_uquorem64_t	uqr;
    cairo_quorem64_t	qr;

    if (num_neg)
	num = _cairo_int64_negate (num);
    if (den_neg)
	den = _cairo_int64_negate (den);
    uqr = _cairo_uint64_divrem (num, den);
    if (num_neg)
	qr.rem = _cairo_int64_negate (uqr.rem);
    else
	qr.rem = uqr.rem;
    if (num_neg != den_neg)
	qr.quo = (cairo_int64_t) _cairo_int64_negate (uqr.quo);
    else
	qr.quo = (cairo_int64_t) uqr.quo;
    return qr;
}

#if HAVE_UINT128_T

const cairo_uquorem128_t
_cairo_uint128_divrem (cairo_uint128_t num, cairo_uint128_t den)
{
    cairo_uquorem128_t	qr;

    qr.quo = num / den;
    qr.rem = num % den;
    return qr;
}

#else

const cairo_uint128_t
_cairo_uint32_to_uint128 (uint32_t i)
{
    cairo_uint128_t	q;

    q.lo = _cairo_uint32_to_uint64 (i);
    q.hi = _cairo_uint32_to_uint64 (0);
    return q;
}

const cairo_int128_t
_cairo_int32_to_int128 (int32_t i)
{
    cairo_int128_t	q;

    q.lo = _cairo_int32_to_int64 (i);
    q.hi = _cairo_int32_to_int64 (i < 0 ? -1 : 0);
    return q;
}

const cairo_uint128_t
_cairo_uint64_to_uint128 (cairo_uint64_t i)
{
    cairo_uint128_t	q;

    q.lo = i;
    q.hi = _cairo_uint32_to_uint64 (0);
    return q;
}

const cairo_int128_t
_cairo_int64_to_int128 (cairo_int64_t i)
{
    cairo_int128_t	q;

    q.lo = i;
    q.hi = _cairo_int32_to_int64 (_cairo_int64_negative(i) ? -1 : 0);
    return q;
}

const cairo_uint128_t
_cairo_uint128_add (cairo_uint128_t a, cairo_uint128_t b)
{
    cairo_uint128_t	s;

    s.hi = _cairo_uint64_add (a.hi, b.hi);
    s.lo = _cairo_uint64_add (a.lo, b.lo);
    if (_cairo_uint64_lt (s.lo, a.lo))
	s.hi = _cairo_uint64_add (s.hi, _cairo_uint32_to_uint64 (1));
    return s;
}

const cairo_uint128_t
_cairo_uint128_sub (cairo_uint128_t a, cairo_uint128_t b)
{
    cairo_uint128_t	s;

    s.hi = _cairo_uint64_sub (a.hi, b.hi);
    s.lo = _cairo_uint64_sub (a.lo, b.lo);
    if (_cairo_uint64_gt (s.lo, a.lo))
	s.hi = _cairo_uint64_sub (s.hi, _cairo_uint32_to_uint64(1));
    return s;
}

#if HAVE_UINT64_T

#define uint64_lo32(i)	((i) & 0xffffffff)
#define uint64_hi32(i)	((i) >> 32)
#define uint64_lo(i)	((i) & 0xffffffff)
#define uint64_hi(i)	((i) >> 32)
#define uint64_shift32(i)   ((i) << 32)
#define uint64_carry32	(((uint64_t) 1) << 32)

#else

#define uint64_lo32(i)	((i).lo)
#define uint64_hi32(i)	((i).hi)

static const cairo_uint64_t
uint64_lo (cairo_uint64_t i)
{
    cairo_uint64_t  s;

    s.lo = i.lo;
    s.hi = 0;
    return s;
}

static const cairo_uint64_t
uint64_hi (cairo_uint64_t i)
{
    cairo_uint64_t  s;

    s.lo = i.hi;
    s.hi = 0;
    return s;
}

static const cairo_uint64_t
uint64_shift32 (cairo_uint64_t i)
{
    cairo_uint64_t  s;

    s.lo = 0;
    s.hi = i.lo;
    return s;
}

static const cairo_uint64_t uint64_carry32 = { 0, 1 };

#endif

const cairo_uint128_t
_cairo_uint64x64_128_mul (cairo_uint64_t a, cairo_uint64_t b)
{
    cairo_uint128_t	s;
    uint32_t		ah, al, bh, bl;
    cairo_uint64_t	r0, r1, r2, r3;

    al = uint64_lo32 (a);
    ah = uint64_hi32 (a);
    bl = uint64_lo32 (b);
    bh = uint64_hi32 (b);

    r0 = _cairo_uint32x32_64_mul (al, bl);
    r1 = _cairo_uint32x32_64_mul (al, bh);
    r2 = _cairo_uint32x32_64_mul (ah, bl);
    r3 = _cairo_uint32x32_64_mul (ah, bh);

    r1 = _cairo_uint64_add (r1, uint64_hi (r0));    /* no carry possible */
    r1 = _cairo_uint64_add (r1, r2);	    	    /* but this can carry */
    if (_cairo_uint64_lt (r1, r2))		    /* check */
	r3 = _cairo_uint64_add (r3, uint64_carry32);

    s.hi = _cairo_uint64_add (r3, uint64_hi(r1));
    s.lo = _cairo_uint64_add (uint64_shift32 (r1),
				uint64_lo (r0));
    return s;
}

const cairo_uint128_t
_cairo_uint128_mul (cairo_uint128_t a, cairo_uint128_t b)
{
    cairo_uint128_t	s;

    s = _cairo_uint64x64_128_mul (a.lo, b.lo);
    s.hi = _cairo_uint64_add (s.hi,
				_cairo_uint64_mul (a.lo, b.hi));
    s.hi = _cairo_uint64_add (s.hi,
				_cairo_uint64_mul (a.hi, b.lo));
    return s;
}

const cairo_uint128_t
_cairo_uint128_lsl (cairo_uint128_t a, int shift)
{
    if (shift >= 64)
    {
	a.hi = a.lo;
	a.lo = _cairo_uint32_to_uint64 (0);
	shift -= 64;
    }
    if (shift)
    {
	a.hi = _cairo_uint64_add (_cairo_uint64_lsl (a.hi, shift),
				    _cairo_uint64_rsl (a.lo, (64 - shift)));
	a.lo = _cairo_uint64_lsl (a.lo, shift);
    }
    return a;
}

const cairo_uint128_t
_cairo_uint128_rsl (cairo_uint128_t a, int shift)
{
    if (shift >= 64)
    {
	a.lo = a.hi;
	a.hi = _cairo_uint32_to_uint64 (0);
	shift -= 64;
    }
    if (shift)
    {
	a.lo = _cairo_uint64_add (_cairo_uint64_rsl (a.lo, shift),
				    _cairo_uint64_lsl (a.hi, (64 - shift)));
	a.hi = _cairo_uint64_rsl (a.hi, shift);
    }
    return a;
}

const cairo_uint128_t
_cairo_uint128_rsa (cairo_int128_t a, int shift)
{
    if (shift >= 64)
    {
	a.lo = a.hi;
	a.hi = _cairo_uint64_rsa (a.hi, 64-1);
	shift -= 64;
    }
    if (shift)
    {
	a.lo = _cairo_uint64_add (_cairo_uint64_rsl (a.lo, shift),
				    _cairo_uint64_lsl (a.hi, (64 - shift)));
	a.hi = _cairo_uint64_rsa (a.hi, shift);
    }
    return a;
}

const int
_cairo_uint128_lt (cairo_uint128_t a, cairo_uint128_t b)
{
    return (_cairo_uint64_lt (a.hi, b.hi) ||
	    (_cairo_uint64_eq (a.hi, b.hi) &&
	     _cairo_uint64_lt (a.lo, b.lo)));
}

const int
_cairo_int128_lt (cairo_int128_t a, cairo_int128_t b)
{
    if (_cairo_int128_negative (a) && !_cairo_int128_negative (b))
	return 1;
    if (!_cairo_int128_negative (a) && _cairo_int128_negative (b))
	return 0;
    return _cairo_uint128_lt (a, b);
}

const int
_cairo_uint128_eq (cairo_uint128_t a, cairo_uint128_t b)
{
    return (_cairo_uint64_eq (a.hi, b.hi) &&
	    _cairo_uint64_eq (a.lo, b.lo));
}

/*
 * The design of this algorithm comes from GCC,
 * but the actual implementation is new
 */

/*
 * den >= num.hi
 */
static const cairo_uquorem64_t
_cairo_uint128x64_normalized_divrem (cairo_uint128_t num, cairo_uint64_t den)
{
    cairo_uquorem64_t	qr64;
    cairo_uquorem64_t	qr;
    uint32_t		q0, q1;
    cairo_uint64_t	r0, r1;
    uint32_t		d0, d1;
    cairo_uint64_t	t;

    d0 = uint64_lo32 (den);
    d1 = uint64_hi32 (den);

    qr64 = _cairo_uint64_divrem (num.hi, _cairo_uint32_to_uint64 (d1));
    q1 = _cairo_uint64_to_uint32 (qr64.quo);
    r1 = qr64.rem;

    t = _cairo_uint32x32_64_mul (q1, d0);
    
    r1 = _cairo_uint64_add (_cairo_uint64_lsl (r1, 32),
			    _cairo_uint64_rsl (num.lo, 32));
    
    if (_cairo_uint64_lt (r1, t))
    {
	q1--;
	r1 = _cairo_uint64_add (r1, den);
	if (_cairo_uint64_ge (r1, den) && _cairo_uint64_lt (r1, t))
	{
	    q1--;
	    r1 = _cairo_uint64_add (r1, den);
	}
    }

    r1 = _cairo_uint64_sub (r1, t);

    qr64 = _cairo_uint64_divrem (r1, _cairo_uint32_to_uint64 (d1));

    q0 = _cairo_uint64_to_uint32 (qr64.quo);
    r0 = qr64.rem;

    t = _cairo_uint32x32_64_mul (q0, d0);

    r0 = _cairo_uint64_add (_cairo_uint64_lsl (r0, 32),
			    _cairo_uint32_to_uint64 (_cairo_uint64_to_uint32 (num.lo)));
    if (_cairo_uint64_lt (r0, t))
    {
	q0--;
	r0 = _cairo_uint64_add (r0, den);
	if (_cairo_uint64_ge (r0, den) && _cairo_uint64_lt (r0, t))
	{
	    q0--;
	    r0 = _cairo_uint64_add (r0, den);
	}
    }

    r0 = _cairo_uint64_sub (r0, t);

    qr.quo = _cairo_uint32s_to_uint64 (q1, q0);
    qr.rem = r0;
    return qr;
}

#if HAVE_UINT64_T

static const int
_cairo_leading_zeros64 (cairo_uint64_t q)
{
    int	top = 0;
    
    if (q >= (uint64_t) 0x10000 << 16)
    {
	top += 32;
	q >>= 32;
    }
    if (q >= (uint64_t) 0x10000)
    {
	top += 16;
	q >>= 16;
    }
    if (q >= (uint64_t) 0x100)
    {
	top += 8;
	q >>= 8;
    }
    top += top_bit [q];
    return 64 - top;
}

#else

static const int
_cairo_leading_zeros64 (cairo_uint64_t d)
{
    if (d.hi)
	return _cairo_leading_zeros32 (d.hi);
    else
	return 32 + _cairo_leading_zeros32 (d.lo);
}

#endif

const cairo_uquorem128_t
_cairo_uint128_divrem (cairo_uint128_t num, cairo_uint128_t den)
{
    cairo_uquorem64_t	qr64;
    cairo_uquorem128_t	qr;
    int			norm;
    cairo_uint64_t	q1, q0, r1, r0;

    if (_cairo_uint64_eq (den.hi, _cairo_uint32_to_uint64 (0)))
    {
	if (_cairo_uint64_gt (den.lo, num.hi))
	{
	    /* 0q = nn / 0d */

	    norm = _cairo_leading_zeros64 (den.lo);
	    if (norm)
	    {
		den.lo = _cairo_uint64_lsl (den.lo, norm);
		num = _cairo_uint128_lsl (num, norm);
	    }
	    q1 = _cairo_uint32_to_uint64 (0);
	}
	else
	{
	    /* qq = NN / 0d */

	    if (_cairo_uint64_eq (den.lo, _cairo_uint32_to_uint64 (0)))
		den.lo = _cairo_uint64_divrem (_cairo_uint32_to_uint64 (1),
						 den.lo).quo;

	    norm = _cairo_leading_zeros64 (den.lo);
	    if (norm)
	    {
		cairo_uint128_t	num1;

		den.lo = _cairo_uint64_lsl (den.lo, norm);
		num1 = _cairo_uint128_rsl (num, 64 - norm);
		qr64 = _cairo_uint128x64_normalized_divrem (num1, den.lo);
		q1 = qr64.quo;
		num.hi = qr64.rem;
		num.lo = _cairo_uint64_lsl (num.lo, norm);
	    }
	    else
	    {
		num.hi = _cairo_uint64_sub (num.hi, den.lo);
		q1 = _cairo_uint32_to_uint64 (1);
	    }
	}
	qr64 = _cairo_uint128x64_normalized_divrem (num, den.lo);
	q0 = qr64.quo;
	r1 = _cairo_uint32_to_uint64 (0);
	r0 = _cairo_uint64_rsl (qr64.rem, norm);
    }
    else
    {
	if (_cairo_uint64_gt (den.hi, num.hi))
	{
	    /* 00 = nn / DD */
	    q0 = q1 = _cairo_uint32_to_uint64 (0);
	    r0 = num.lo;
	    r1 = num.hi;
	}
	else
	{
	    /* 0q = NN / dd */

	    norm = _cairo_leading_zeros64 (den.hi);
	    if (norm == 0)
	    {
		if (_cairo_uint64_gt (num.hi, den.hi) ||
		    _cairo_uint64_ge (num.lo, den.lo))
		{
		    q0 = _cairo_uint32_to_uint64 (1);
		    num = _cairo_uint128_sub (num, den);
		}
		else
		    q0 = _cairo_uint32_to_uint64 (0);
		
		q1 = _cairo_uint32_to_uint64 (0);
		r0 = num.lo;
		r1 = num.hi;
	    }
	    else
	    {
		cairo_uint128_t	num1;
		cairo_uint128_t	part;

		num1 = _cairo_uint128_rsl (num, 64 - norm);
		den = _cairo_uint128_lsl (den, norm);

		qr64 = _cairo_uint128x64_normalized_divrem (num1, den.hi);
		part = _cairo_uint64x64_128_mul (qr64.quo, den.lo);

		q0 = qr64.quo;
		
		num.lo = _cairo_uint64_lsl (num.lo, norm);
		num.hi = qr64.rem;
		
		if (_cairo_uint128_gt (part, num))
		{
		    q0 = _cairo_uint64_sub (q0, _cairo_uint32_to_uint64 (1));
		    part = _cairo_uint128_sub (part, den);
		}

		q1 = _cairo_uint32_to_uint64 (0);

		num = _cairo_uint128_sub (num, part);
		num = _cairo_uint128_rsl (num, norm);
		r0 = num.lo;
		r1 = num.hi;
	    }
	}
    }
    qr.quo.lo = q0;
    qr.quo.hi = q1;
    qr.rem.lo = r0;
    qr.rem.hi = r1;
    return qr;
}

const cairo_int128_t
_cairo_int128_negate (cairo_int128_t a)
{
    a.lo = _cairo_uint64_not (a.lo);
    a.hi = _cairo_uint64_not (a.hi);
    return _cairo_uint128_add (a, _cairo_uint32_to_uint128 (1));
}

const cairo_int128_t
_cairo_int128_not (cairo_int128_t a)
{
    a.lo = _cairo_uint64_not (a.lo);
    a.hi = _cairo_uint64_not (a.hi);
    return a;
}

#endif /* !HAVE_UINT128_T */

const cairo_quorem128_t
_cairo_int128_divrem (cairo_int128_t num, cairo_int128_t den)
{
    int			num_neg = _cairo_int128_negative (num);
    int			den_neg = _cairo_int128_negative (den);
    cairo_uquorem128_t	uqr;
    cairo_quorem128_t	qr;

    if (num_neg)
	num = _cairo_int128_negate (num);
    if (den_neg)
	den = _cairo_int128_negate (den);
    uqr = _cairo_uint128_divrem (num, den);
    if (num_neg)
	qr.rem = _cairo_int128_negate (uqr.rem);
    else
	qr.rem = uqr.rem;
    if (num_neg != den_neg)
	qr.quo = _cairo_int128_negate (uqr.quo);
    else
	qr.quo = uqr.quo;
    return qr;
}
