/*
 * $XFree86: $
 *
 * Copyright © 2000 Keith Packard, member of The XFree86 Project, Inc.
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


#include "icint.h"
/* #include "picturestr.h" */
#include "icimage.h"

/*
 * General purpose compositing code optimized for minimal memory
 * references
 *
 * All work is done on canonical ARGB values, functions for fetching
 * and storing these exist for each format.
 */

/*
 * Combine src and mask using IN
 */

CARD32
IcCombineMaskU (IcCompositeOperand   *src,
		IcCompositeOperand   *msk)
{
    CARD32  x;
    CARD16  a;
    CARD16  t;
    CARD32  m,n,o,p;

    if (!msk)
	return (*src->fetch) (src);
    
    a = (*msk->fetch) (msk) >> 24;
    if (!a)
	return 0;
    
    x = (*src->fetch) (src);
    if (a == 0xff)
	return x;
    
    m = IcInU(x,0,a,t);
    n = IcInU(x,8,a,t);
    o = IcInU(x,16,a,t);
    p = IcInU(x,24,a,t);
    return m|n|o|p;
}

IcCompSrc
IcCombineMaskC (IcCompositeOperand   *src,
		IcCompositeOperand   *msk)
{
    IcCompSrc	s;
    CARD32	x;
    CARD32	a;
    CARD16	xa;
    CARD16	t;
    CARD32	m,n,o,p;

    if (!msk)
    {
	x = (*src->fetch) (src);
	s.value = x;
	x = x >> 24;
	x |= x << 8;
	x |= x << 16;
	s.alpha = x;
	return s;
    }
    
    a = (*msk->fetcha) (msk);
    if (!a)
    {
	s.value = 0;
	s.alpha = 0;
	return s;
    }
    
    x = (*src->fetch) (src);
    if (a == 0xffffffff)
    {
	s.value = x;
	x = x >> 24;
	x |= x << 8;
	x |= x << 16;
	s.alpha = x;
	return s;
    }
    
    m = IcInC(x,0,a,t);
    n = IcInC(x,8,a,t);
    o = IcInC(x,16,a,t);
    p = IcInC(x,24,a,t);
    s.value = m|n|o|p;
    xa = x >> 24;
    m = IcInU(a,0,xa,t);
    n = IcInU(a,8,xa,t);
    o = IcInU(a,16,xa,t);
    p = IcInU(a,24,xa,t);
    s.alpha = m|n|o|p;
    return s;
}

CARD32
IcCombineMaskValueC (IcCompositeOperand   *src,
		     IcCompositeOperand   *msk)
{
    CARD32	x;
    CARD32	a;
    CARD16	t;
    CARD32	m,n,o,p;

    if (!msk)
    {
	return (*src->fetch) (src);
    }
    
    a = (*msk->fetcha) (msk);
    if (!a)
	return a;
    
    x = (*src->fetch) (src);
    if (a == 0xffffffff)
	return x;
    
    m = IcInC(x,0,a,t);
    n = IcInC(x,8,a,t);
    o = IcInC(x,16,a,t);
    p = IcInC(x,24,a,t);
    return m|n|o|p;
}

/*
 * Combine src and mask using IN, generating only the alpha component
 */
CARD32
IcCombineMaskAlphaU (IcCompositeOperand   *src,
		     IcCompositeOperand   *msk)
{
    CARD32  x;
    CARD16  a;
    CARD16  t;

    if (!msk)
	return (*src->fetch) (src);
    
    a = (*msk->fetch) (msk) >> 24;
    if (!a)
	return 0;
    
    x = (*src->fetch) (src);
    if (a == 0xff)
	return x;
    
    return IcInU(x,24,a,t);
}

CARD32
IcCombineMaskAlphaC (IcCompositeOperand   *src,
		     IcCompositeOperand   *msk)
{
    CARD32	x;
    CARD32	a;
    CARD16	t;
    CARD32	m,n,o,p;

    if (!msk)
	return (*src->fetch) (src);
    
    a = (*msk->fetcha) (msk);
    if (!a)
	return 0;
    
    x = (*src->fetcha) (src);
    if (a == 0xffffffff)
	return x;
    
    m = IcInC(x,0,a,t);
    n = IcInC(x,8,a,t);
    o = IcInC(x,16,a,t);
    p = IcInC(x,24,a,t);
    return m|n|o|p;
}

/*
 * All of the composing functions
 */
void
IcCombineClear (IcCompositeOperand   *src,
		IcCompositeOperand   *msk,
		IcCompositeOperand   *dst)
{
    (*dst->store) (dst, 0);
}

void
IcCombineSrcU (IcCompositeOperand    *src,
	       IcCompositeOperand    *msk,
	       IcCompositeOperand    *dst)
{
    (*dst->store) (dst, IcCombineMaskU (src, msk));
}

void
IcCombineSrcC (IcCompositeOperand    *src,
	       IcCompositeOperand    *msk,
	       IcCompositeOperand    *dst)
{
    (*dst->store) (dst, IcCombineMaskValueC (src, msk));
}

void
IcCombineDst (IcCompositeOperand    *src,
	      IcCompositeOperand    *msk,
	      IcCompositeOperand    *dst)
{
    /* noop */
}

void
IcCombineOverU (IcCompositeOperand   *src,
		IcCompositeOperand   *msk,
		IcCompositeOperand   *dst)
{
    CARD32  s, d;
    CARD16  a;
    CARD16  t;
    CARD32  m,n,o,p;

    s = IcCombineMaskU (src, msk);
    a = ~s >> 24;
    if (a != 0xff)
    {
	if (a)
	{
	    d = (*dst->fetch) (dst);
	    m = IcOverU(s,d,0,a,t);
	    n = IcOverU(s,d,8,a,t);
	    o = IcOverU(s,d,16,a,t);
	    p = IcOverU(s,d,24,a,t);
	    s = m|n|o|p;
	}
	(*dst->store) (dst, s);
    }
}

void
IcCombineOverC (IcCompositeOperand   *src,
		IcCompositeOperand   *msk,
		IcCompositeOperand   *dst)
{
    IcCompSrc	cs;
    CARD32  s, d;
    CARD32  a;
    CARD16  t;
    CARD32  m,n,o,p;

    cs = IcCombineMaskC (src, msk);
    s = cs.value;
    a = ~cs.alpha;
    if (a != 0xffffffff)
    {
	if (a)
	{
	    d = (*dst->fetch) (dst);
	    m = IcOverC(s,d,0,a,t);
	    n = IcOverC(s,d,8,a,t);
	    o = IcOverC(s,d,16,a,t);
	    p = IcOverC(s,d,24,a,t);
	    s = m|n|o|p;
	}
	(*dst->store) (dst, s);
    }
}

void
IcCombineOverReverseU (IcCompositeOperand    *src,
		       IcCompositeOperand    *msk,
		       IcCompositeOperand    *dst)
{
    CARD32  s, d;
    CARD16  a;
    CARD16  t;
    CARD32  m,n,o,p;

    d = (*dst->fetch) (dst);
    a = ~d >> 24;
    if (a)
    {
	s = IcCombineMaskU (src, msk);
	if (a != 0xff)
	{
	    m = IcOverU(d,s,0,a,t);
	    n = IcOverU(d,s,8,a,t);
	    o = IcOverU(d,s,16,a,t);
	    p = IcOverU(d,s,24,a,t);
	    s = m|n|o|p;
	}
	(*dst->store) (dst, s);
    }
}

void
IcCombineOverReverseC (IcCompositeOperand    *src,
		       IcCompositeOperand    *msk,
		       IcCompositeOperand    *dst)
{
    CARD32  s, d;
    CARD32  a;
    CARD16  t;
    CARD32  m,n,o,p;

    d = (*dst->fetch) (dst);
    a = ~d >> 24;
    if (a)
    {
	s = IcCombineMaskValueC (src, msk);
	if (a != 0xff)
	{
	    m = IcOverU(d,s,0,a,t);
	    n = IcOverU(d,s,8,a,t);
	    o = IcOverU(d,s,16,a,t);
	    p = IcOverU(d,s,24,a,t);
	    s = m|n|o|p;
	}
	(*dst->store) (dst, s);
    }
}

void
IcCombineInU (IcCompositeOperand	    *src,
	      IcCompositeOperand	    *msk,
	      IcCompositeOperand	    *dst)
{
    CARD32  s, d;
    CARD16  a;
    CARD16  t;
    CARD32  m,n,o,p;

    d = (*dst->fetch) (dst);
    a = d >> 24;
    s = 0;
    if (a)
    {
	s = IcCombineMaskU (src, msk);
	if (a != 0xff)
	{
	    m = IcInU(s,0,a,t);
	    n = IcInU(s,8,a,t);
	    o = IcInU(s,16,a,t);
	    p = IcInU(s,24,a,t);
	    s = m|n|o|p;
	}
    }
    (*dst->store) (dst, s);
}

void
IcCombineInC (IcCompositeOperand	    *src,
	      IcCompositeOperand	    *msk,
	      IcCompositeOperand	    *dst)
{
    CARD32  s, d;
    CARD16  a;
    CARD16  t;
    CARD32  m,n,o,p;

    d = (*dst->fetch) (dst);
    a = d >> 24;
    s = 0;
    if (a)
    {
	s = IcCombineMaskValueC (src, msk);
	if (a != 0xff)
	{
	    m = IcInU(s,0,a,t);
	    n = IcInU(s,8,a,t);
	    o = IcInU(s,16,a,t);
	    p = IcInU(s,24,a,t);
	    s = m|n|o|p;
	}
    }
    (*dst->store) (dst, s);
}

void
IcCombineInReverseU (IcCompositeOperand  *src,
		     IcCompositeOperand  *msk,
		     IcCompositeOperand  *dst)
{
    CARD32  s, d;
    CARD16  a;
    CARD16  t;
    CARD32  m,n,o,p;

    s = IcCombineMaskAlphaU (src, msk);
    a = s >> 24;
    if (a != 0xff)
    {
	d = 0;
	if (a)
	{
	    d = (*dst->fetch) (dst);
	    m = IcInU(d,0,a,t);
	    n = IcInU(d,8,a,t);
	    o = IcInU(d,16,a,t);
	    p = IcInU(d,24,a,t);
	    d = m|n|o|p;
	}
	(*dst->store) (dst, d);
    }
}

void
IcCombineInReverseC (IcCompositeOperand  *src,
		     IcCompositeOperand  *msk,
		     IcCompositeOperand  *dst)
{
    CARD32  s, d;
    CARD32  a;
    CARD16  t;
    CARD32  m,n,o,p;

    s = IcCombineMaskAlphaC (src, msk);
    a = s;
    if (a != 0xffffffff)
    {
	d = 0;
	if (a)
	{
	    d = (*dst->fetch) (dst);
	    m = IcInC(d,0,a,t);
	    n = IcInC(d,8,a,t);
	    o = IcInC(d,16,a,t);
	    p = IcInC(d,24,a,t);
	    d = m|n|o|p;
	}
	(*dst->store) (dst, d);
    }
}

void
IcCombineOutU (IcCompositeOperand    *src,
	       IcCompositeOperand    *msk,
	       IcCompositeOperand    *dst)
{
    CARD32  s, d;
    CARD16  a;
    CARD16  t;
    CARD32  m,n,o,p;

    d = (*dst->fetch) (dst);
    a = ~d >> 24;
    s = 0;
    if (a)
    {
	s = IcCombineMaskU (src, msk);
	if (a != 0xff)
	{
	    m = IcInU(s,0,a,t);
	    n = IcInU(s,8,a,t);
	    o = IcInU(s,16,a,t);
	    p = IcInU(s,24,a,t);
	    s = m|n|o|p;
	}
    }
    (*dst->store) (dst, s);
}

void
IcCombineOutC (IcCompositeOperand    *src,
	       IcCompositeOperand    *msk,
	       IcCompositeOperand    *dst)
{
    CARD32  s, d;
    CARD16  a;
    CARD16  t;
    CARD32  m,n,o,p;

    d = (*dst->fetch) (dst);
    a = ~d >> 24;
    s = 0;
    if (a)
    {
	s = IcCombineMaskValueC (src, msk);
	if (a != 0xff)
	{
	    m = IcInU(s,0,a,t);
	    n = IcInU(s,8,a,t);
	    o = IcInU(s,16,a,t);
	    p = IcInU(s,24,a,t);
	    s = m|n|o|p;
	}
    }
    (*dst->store) (dst, s);
}

void
IcCombineOutReverseU (IcCompositeOperand *src,
		      IcCompositeOperand *msk,
		      IcCompositeOperand *dst)
{
    CARD32  s, d;
    CARD16  a;
    CARD16  t;
    CARD32  m,n,o,p;

    s = IcCombineMaskAlphaU (src, msk);
    a = ~s >> 24;
    if (a != 0xff)
    {
	d = 0;
	if (a)
	{
	    d = (*dst->fetch) (dst);
	    m = IcInU(d,0,a,t);
	    n = IcInU(d,8,a,t);
	    o = IcInU(d,16,a,t);
	    p = IcInU(d,24,a,t);
	    d = m|n|o|p;
	}
	(*dst->store) (dst, d);
    }
}

void
IcCombineOutReverseC (IcCompositeOperand *src,
		      IcCompositeOperand *msk,
		      IcCompositeOperand *dst)
{
    CARD32  s, d;
    CARD32  a;
    CARD16  t;
    CARD32  m,n,o,p;

    s = IcCombineMaskAlphaC (src, msk);
    a = ~s;
    if (a != 0xffffffff)
    {
	d = 0;
	if (a)
	{
	    d = (*dst->fetch) (dst);
	    m = IcInC(d,0,a,t);
	    n = IcInC(d,8,a,t);
	    o = IcInC(d,16,a,t);
	    p = IcInC(d,24,a,t);
	    d = m|n|o|p;
	}
	(*dst->store) (dst, d);
    }
}

void
IcCombineAtopU (IcCompositeOperand   *src,
		IcCompositeOperand   *msk,
		IcCompositeOperand   *dst)
{
    CARD32  s, d;
    CARD16  ad, as;
    CARD16  t,u,v;
    CARD32  m,n,o,p;
    
    s = IcCombineMaskU (src, msk);
    d = (*dst->fetch) (dst);
    ad = ~s >> 24;
    as = d >> 24;
    m = IcGen(s,d,0,as,ad,t,u,v);
    n = IcGen(s,d,8,as,ad,t,u,v);
    o = IcGen(s,d,16,as,ad,t,u,v);
    p = IcGen(s,d,24,as,ad,t,u,v);
    (*dst->store) (dst, m|n|o|p);
}

void
IcCombineAtopC (IcCompositeOperand   *src,
		IcCompositeOperand   *msk,
		IcCompositeOperand   *dst)
{
    IcCompSrc	cs;
    CARD32  s, d;
    CARD32  ad;
    CARD16  as;
    CARD16  t, u, v;
    CARD32  m,n,o,p;
    
    cs = IcCombineMaskC (src, msk);
    d = (*dst->fetch) (dst);
    s = cs.value;
    ad = cs.alpha;
    as = d >> 24;
    m = IcGen(s,d,0,as,IcGet8(ad,0),t,u,v);
    n = IcGen(s,d,8,as,IcGet8(ad,8),t,u,v);
    o = IcGen(s,d,16,as,IcGet8(ad,16),t,u,v);
    p = IcGen(s,d,24,as,IcGet8(ad,24),t,u,v);
    (*dst->store) (dst, m|n|o|p);
}

void
IcCombineAtopReverseU (IcCompositeOperand    *src,
		       IcCompositeOperand    *msk,
		       IcCompositeOperand    *dst)
{
    CARD32  s, d;
    CARD16  ad, as;
    CARD16  t, u, v;
    CARD32  m,n,o,p;
    
    s = IcCombineMaskU (src, msk);
    d = (*dst->fetch) (dst);
    ad = s >> 24;
    as = ~d >> 24;
    m = IcGen(s,d,0,as,ad,t,u,v);
    n = IcGen(s,d,8,as,ad,t,u,v);
    o = IcGen(s,d,16,as,ad,t,u,v);
    p = IcGen(s,d,24,as,ad,t,u,v);
    (*dst->store) (dst, m|n|o|p);
}

void
IcCombineAtopReverseC (IcCompositeOperand    *src,
		       IcCompositeOperand    *msk,
		       IcCompositeOperand    *dst)
{
    IcCompSrc	cs;
    CARD32  s, d, ad;
    CARD16  as;
    CARD16  t, u, v;
    CARD32  m,n,o,p;
    
    cs = IcCombineMaskC (src, msk);
    d = (*dst->fetch) (dst);
    s = cs.value;
    ad = cs.alpha;
    as = ~d >> 24;
    m = IcGen(s,d,0,as,IcGet8(ad,0),t,u,v);
    n = IcGen(s,d,8,as,IcGet8(ad,8),t,u,v);
    o = IcGen(s,d,16,as,IcGet8(ad,16),t,u,v);
    p = IcGen(s,d,24,as,IcGet8(ad,24),t,u,v);
    (*dst->store) (dst, m|n|o|p);
}

void
IcCombineXorU (IcCompositeOperand    *src,
	       IcCompositeOperand    *msk,
	       IcCompositeOperand    *dst)
{
    CARD32  s, d;
    CARD16  ad, as;
    CARD16  t, u, v;
    CARD32  m,n,o,p;
    
    s = IcCombineMaskU (src, msk);
    d = (*dst->fetch) (dst);
    ad = ~s >> 24;
    as = ~d >> 24;
    m = IcGen(s,d,0,as,ad,t,u,v);
    n = IcGen(s,d,8,as,ad,t,u,v);
    o = IcGen(s,d,16,as,ad,t,u,v);
    p = IcGen(s,d,24,as,ad,t,u,v);
    (*dst->store) (dst, m|n|o|p);
}

void
IcCombineXorC (IcCompositeOperand    *src,
	       IcCompositeOperand    *msk,
	       IcCompositeOperand    *dst)
{
    IcCompSrc	cs;
    CARD32  s, d, ad;
    CARD16  as;
    CARD16  t, u, v;
    CARD32  m,n,o,p;
    
    cs = IcCombineMaskC (src, msk);
    d = (*dst->fetch) (dst);
    s = cs.value;
    ad = ~cs.alpha;
    as = ~d >> 24;
    m = IcGen(s,d,0,as,ad,t,u,v);
    n = IcGen(s,d,8,as,ad,t,u,v);
    o = IcGen(s,d,16,as,ad,t,u,v);
    p = IcGen(s,d,24,as,ad,t,u,v);
    (*dst->store) (dst, m|n|o|p);
}

void
IcCombineAddU (IcCompositeOperand    *src,
	       IcCompositeOperand    *msk,
	       IcCompositeOperand    *dst)
{
    CARD32  s, d;
    CARD16  t;
    CARD32  m,n,o,p;

    s = IcCombineMaskU (src, msk);
    if (s == ~0)
	(*dst->store) (dst, s);
    else
    {
	d = (*dst->fetch) (dst);
	if (s && d != ~0)
	{
	    m = IcAdd(s,d,0,t);
	    n = IcAdd(s,d,8,t);
	    o = IcAdd(s,d,16,t);
	    p = IcAdd(s,d,24,t);
	    (*dst->store) (dst, m|n|o|p);
	}
    }
}

void
IcCombineAddC (IcCompositeOperand    *src,
	       IcCompositeOperand    *msk,
	       IcCompositeOperand    *dst)
{
    CARD32  s, d;
    CARD16  t;
    CARD32  m,n,o,p;

    s = IcCombineMaskValueC (src, msk);
    if (s == ~0)
	(*dst->store) (dst, s);
    else
    {
	d = (*dst->fetch) (dst);
	if (s && d != ~0)
	{
	    m = IcAdd(s,d,0,t);
	    n = IcAdd(s,d,8,t);
	    o = IcAdd(s,d,16,t);
	    p = IcAdd(s,d,24,t);
	    (*dst->store) (dst, m|n|o|p);
	}
    }
}

void
IcCombineSaturateU (IcCompositeOperand   *src,
		    IcCompositeOperand   *msk,
		    IcCompositeOperand   *dst)
{
    CARD32  s = IcCombineMaskU (src, msk), d;
#if 0
    CARD16  sa, da;
    CARD16  ad, as;
    CARD16  t;
    CARD32  m,n,o,p;
    
    d = (*dst->fetch) (dst);
    sa = s >> 24;
    da = ~d >> 24;
    if (sa <= da)
    {
	m = IcAdd(s,d,0,t);
	n = IcAdd(s,d,8,t);
	o = IcAdd(s,d,16,t);
	p = IcAdd(s,d,24,t);
    }
    else
    {
	as = (da << 8) / sa;
	ad = 0xff;
	m = IcGen(s,d,0,as,ad,t,u,v);
	n = IcGen(s,d,8,as,ad,t,u,v);
	o = IcGen(s,d,16,as,ad,t,u,v);
	p = IcGen(s,d,24,as,ad,t,u,v);
    }
    (*dst->store) (dst, m|n|o|p);
#else
    if ((s >> 24) == 0xff)
	(*dst->store) (dst, s);
    else
    {
	d = (*dst->fetch) (dst);
	if ((s >> 24) > (d >> 24))
	    (*dst->store) (dst, s);
    }
#endif
}

void
IcCombineSaturateC (IcCompositeOperand   *src,
		    IcCompositeOperand   *msk,
		    IcCompositeOperand   *dst)
{
    IcCompSrc	cs;
    CARD32  s, d;
    CARD16  sa, sr, sg, sb, da;
    CARD16  t, u, v;
    CARD32  m,n,o,p;
    
    cs = IcCombineMaskC (src, msk);
    d = (*dst->fetch) (dst);
    s = cs.value;
    sa = (cs.alpha >> 24) & 0xff;
    sr = (cs.alpha >> 16) & 0xff;
    sg = (cs.alpha >>  8) & 0xff;
    sb = (cs.alpha      ) & 0xff;
    da = ~d >> 24;
    
    if (sb <= da)
	m = IcAdd(s,d,0,t);
    else
	m = IcGen (s, d, 0, (da << 8) / sb, 0xff, t, u, v);
    
    if (sg <= da)
	n = IcAdd(s,d,8,t);
    else
	n = IcGen (s, d, 8, (da << 8) / sg, 0xff, t, u, v);
    
    if (sr < da)
	o = IcAdd(s,d,16,t);
    else
	o = IcGen (s, d, 16, (da << 8) / sr, 0xff, t, u, v);

    if (sa <= da)
	p = IcAdd(s,d,24,t);
    else
	p = IcGen (s, d, 24, (da << 8) / sa, 0xff, t, u, v);
    
    (*dst->store) (dst, m|n|o|p);
}

/*
 * All of the disjoint composing functions

 The four entries in the first column indicate what source contributions
 come from each of the four areas of the picture -- areas covered by neither
 A nor B, areas covered only by A, areas covered only by B and finally
 areas covered by both A and B.
 
		Disjoint			Conjoint
		Fa		Fb		Fa		Fb
(0,0,0,0)	0		0		0		0
(0,A,0,A)	1		0		1		0
(0,0,B,B)	0		1		0		1
(0,A,B,A)	1		min((1-a)/b,1)	1		max(1-a/b,0)
(0,A,B,B)	min((1-b)/a,1)	1		max(1-b/a,0)	1		
(0,0,0,A)	max(1-(1-b)/a,0) 0		min(1,b/a)	0
(0,0,0,B)	0		max(1-(1-a)/b,0) 0		min(a/b,1)
(0,A,0,0)	min(1,(1-b)/a)	0		max(1-b/a,0)	0
(0,0,B,0)	0		min(1,(1-a)/b)	0		max(1-a/b,0)
(0,0,B,A)	max(1-(1-b)/a,0) min(1,(1-a)/b)	 min(1,b/a)	max(1-a/b,0)
(0,A,0,B)	min(1,(1-b)/a)	max(1-(1-a)/b,0) max(1-b/a,0)	min(1,a/b)
(0,A,B,0)	min(1,(1-b)/a)	min(1,(1-a)/b)	max(1-b/a,0)	max(1-a/b,0)

 */

#define CombineAOut 1
#define CombineAIn  2
#define CombineBOut 4
#define CombineBIn  8

#define CombineClear	0
#define CombineA	(CombineAOut|CombineAIn)
#define CombineB	(CombineBOut|CombineBIn)
#define CombineAOver	(CombineAOut|CombineBOut|CombineAIn)
#define CombineBOver	(CombineAOut|CombineBOut|CombineBIn)
#define CombineAAtop	(CombineBOut|CombineAIn)
#define CombineBAtop	(CombineAOut|CombineBIn)
#define CombineXor	(CombineAOut|CombineBOut)

/* portion covered by a but not b */
CARD8
IcCombineDisjointOutPart (CARD8 a, CARD8 b)
{
    /* min (1, (1-b) / a) */
    
    b = ~b;		    /* 1 - b */
    if (b >= a)		    /* 1 - b >= a -> (1-b)/a >= 1 */
	return 0xff;	    /* 1 */
    return IcIntDiv(b,a);   /* (1-b) / a */
}

/* portion covered by both a and b */
CARD8
IcCombineDisjointInPart (CARD8 a, CARD8 b)
{
    /* max (1-(1-b)/a,0) */
    /*  = - min ((1-b)/a - 1, 0) */
    /*  = 1 - min (1, (1-b)/a) */

    b = ~b;		    /* 1 - b */
    if (b >= a)		    /* 1 - b >= a -> (1-b)/a >= 1 */
	return 0;	    /* 1 - 1 */
    return ~IcIntDiv(b,a);  /* 1 - (1-b) / a */
}

void
IcCombineDisjointGeneralU (IcCompositeOperand   *src,
			   IcCompositeOperand   *msk,
			   IcCompositeOperand   *dst,
			   CARD8		combine)
{
    CARD32  s, d;
    CARD32  m,n,o,p;
    CARD16  Fa, Fb, t, u, v;
    CARD8   sa, da;

    s = IcCombineMaskU (src, msk);
    sa = s >> 24;
    
    d = (*dst->fetch) (dst);
    da = d >> 24;
    
    switch (combine & CombineA) {
    default:
	Fa = 0;
	break;
    case CombineAOut:
	Fa = IcCombineDisjointOutPart (sa, da);
	break;
    case CombineAIn:
	Fa = IcCombineDisjointInPart (sa, da);
	break;
    case CombineA:
	Fa = 0xff;
	break;
    }
    
    switch (combine & CombineB) {
    default:
	Fb = 0;
	break;
    case CombineBOut:
	Fb = IcCombineDisjointOutPart (da, sa);
	break;
    case CombineBIn:
	Fb = IcCombineDisjointInPart (da, sa);
	break;
    case CombineB:
	Fb = 0xff;
	break;
    }
    m = IcGen (s,d,0,Fa,Fb,t,u,v);
    n = IcGen (s,d,8,Fa,Fb,t,u,v);
    o = IcGen (s,d,16,Fa,Fb,t,u,v);
    p = IcGen (s,d,24,Fa,Fb,t,u,v);
    s = m|n|o|p;
    (*dst->store) (dst, s);
}

void
IcCombineDisjointGeneralC (IcCompositeOperand   *src,
			   IcCompositeOperand   *msk,
			   IcCompositeOperand   *dst,
			   CARD8		combine)
{
    IcCompSrc	cs;
    CARD32  s, d;
    CARD32  m,n,o,p;
    CARD32  Fa;
    CARD16  Fb, t, u, v;
    CARD32  sa;
    CARD8   da;

    cs = IcCombineMaskC (src, msk);
    s = cs.value;
    sa = cs.alpha;
    
    d = (*dst->fetch) (dst);
    da = d >> 24;
    
    switch (combine & CombineA) {
    default:
	Fa = 0;
	break;
    case CombineAOut:
	m = IcCombineDisjointOutPart ((CARD8) (sa >> 0), da);
	n = IcCombineDisjointOutPart ((CARD8) (sa >> 8), da) << 8;
	o = IcCombineDisjointOutPart ((CARD8) (sa >> 16), da) << 16;
	p = IcCombineDisjointOutPart ((CARD8) (sa >> 24), da) << 24;
	Fa = m|n|o|p;
	break;
    case CombineAIn:
	m = IcCombineDisjointOutPart ((CARD8) (sa >> 0), da);
	n = IcCombineDisjointOutPart ((CARD8) (sa >> 8), da) << 8;
	o = IcCombineDisjointOutPart ((CARD8) (sa >> 16), da) << 16;
	p = IcCombineDisjointOutPart ((CARD8) (sa >> 24), da) << 24;
	Fa = m|n|o|p;
	break;
    case CombineA:
	Fa = 0xffffffff;
	break;
    }
    
    switch (combine & CombineB) {
    default:
	Fb = 0;
	break;
    case CombineBOut:
	Fb = IcCombineDisjointOutPart (da, sa);
	break;
    case CombineBIn:
	Fb = IcCombineDisjointInPart (da, sa);
	break;
    case CombineB:
	Fb = 0xff;
	break;
    }
    m = IcGen (s,d,0,IcGet8(Fa,0),Fb,t,u,v);
    n = IcGen (s,d,8,IcGet8(Fa,8),Fb,t,u,v);
    o = IcGen (s,d,16,IcGet8(Fa,16),Fb,t,u,v);
    p = IcGen (s,d,24,IcGet8(Fa,24),Fb,t,u,v);
    s = m|n|o|p;
    (*dst->store) (dst, s);
}

void
IcCombineDisjointOverU (IcCompositeOperand   *src,
			IcCompositeOperand   *msk,
			IcCompositeOperand   *dst)
{
    CARD32  s, d;
    CARD16  a;
    CARD16  t;
    CARD32  m,n,o,p;

    s = IcCombineMaskU (src, msk);
    a = s >> 24;
    if (a != 0x00)
    {
	if (a != 0xff)
	{
	    d = (*dst->fetch) (dst);
	    a = IcCombineDisjointOutPart (d >> 24, a);
	    m = IcOverU(s,d,0,a,t);
	    n = IcOverU(s,d,8,a,t);
	    o = IcOverU(s,d,16,a,t);
	    p = IcOverU(s,d,24,a,t);
	    s = m|n|o|p;
	}
	(*dst->store) (dst, s);
    }
}

void
IcCombineDisjointOverC (IcCompositeOperand   *src,
			IcCompositeOperand   *msk,
			IcCompositeOperand   *dst)
{
    IcCombineDisjointGeneralC (src, msk, dst, CombineAOver);
}

void
IcCombineDisjointOverReverseU (IcCompositeOperand    *src,
			       IcCompositeOperand    *msk,
			       IcCompositeOperand    *dst)
{
    IcCombineDisjointGeneralU (src, msk, dst, CombineBOver);
}

void
IcCombineDisjointOverReverseC (IcCompositeOperand    *src,
			       IcCompositeOperand    *msk,
			       IcCompositeOperand    *dst)
{
    IcCombineDisjointGeneralC (src, msk, dst, CombineBOver);
}

void
IcCombineDisjointInU (IcCompositeOperand	    *src,
		      IcCompositeOperand	    *msk,
		      IcCompositeOperand	    *dst)
{
    IcCombineDisjointGeneralU (src, msk, dst, CombineAIn);
}

void
IcCombineDisjointInC (IcCompositeOperand	    *src,
		      IcCompositeOperand	    *msk,
		      IcCompositeOperand	    *dst)
{
    IcCombineDisjointGeneralC (src, msk, dst, CombineAIn);
}

void
IcCombineDisjointInReverseU (IcCompositeOperand  *src,
			     IcCompositeOperand  *msk,
			     IcCompositeOperand  *dst)
{
    IcCombineDisjointGeneralU (src, msk, dst, CombineBIn);
}

void
IcCombineDisjointInReverseC (IcCompositeOperand  *src,
			     IcCompositeOperand  *msk,
			     IcCompositeOperand  *dst)
{
    IcCombineDisjointGeneralC (src, msk, dst, CombineBIn);
}

void
IcCombineDisjointOutU (IcCompositeOperand    *src,
		       IcCompositeOperand    *msk,
		       IcCompositeOperand    *dst)
{
    IcCombineDisjointGeneralU (src, msk, dst, CombineAOut);
}

void
IcCombineDisjointOutC (IcCompositeOperand    *src,
		       IcCompositeOperand    *msk,
		       IcCompositeOperand    *dst)
{
    IcCombineDisjointGeneralC (src, msk, dst, CombineAOut);
}

void
IcCombineDisjointOutReverseU (IcCompositeOperand *src,
			      IcCompositeOperand *msk,
			      IcCompositeOperand *dst)
{
    IcCombineDisjointGeneralU (src, msk, dst, CombineBOut);
}

void
IcCombineDisjointOutReverseC (IcCompositeOperand *src,
			      IcCompositeOperand *msk,
			      IcCompositeOperand *dst)
{
    IcCombineDisjointGeneralC (src, msk, dst, CombineBOut);
}

void
IcCombineDisjointAtopU (IcCompositeOperand   *src,
			IcCompositeOperand   *msk,
			IcCompositeOperand   *dst)
{
    IcCombineDisjointGeneralU (src, msk, dst, CombineAAtop);
}

void
IcCombineDisjointAtopC (IcCompositeOperand   *src,
			IcCompositeOperand   *msk,
			IcCompositeOperand   *dst)
{
    IcCombineDisjointGeneralC (src, msk, dst, CombineAAtop);
}

void
IcCombineDisjointAtopReverseU (IcCompositeOperand    *src,
			       IcCompositeOperand    *msk,
			       IcCompositeOperand    *dst)
{
    IcCombineDisjointGeneralU (src, msk, dst, CombineBAtop);
}

void
IcCombineDisjointAtopReverseC (IcCompositeOperand    *src,
			       IcCompositeOperand    *msk,
			       IcCompositeOperand    *dst)
{
    IcCombineDisjointGeneralC (src, msk, dst, CombineBAtop);
}

void
IcCombineDisjointXorU (IcCompositeOperand    *src,
		       IcCompositeOperand    *msk,
		       IcCompositeOperand    *dst)
{
    IcCombineDisjointGeneralU (src, msk, dst, CombineXor);
}

void
IcCombineDisjointXorC (IcCompositeOperand    *src,
		       IcCompositeOperand    *msk,
		       IcCompositeOperand    *dst)
{
    IcCombineDisjointGeneralC (src, msk, dst, CombineXor);
}

/* portion covered by a but not b */
CARD8
IcCombineConjointOutPart (CARD8 a, CARD8 b)
{
    /* max (1-b/a,0) */
    /* = 1-min(b/a,1) */
    
    /* min (1, (1-b) / a) */
    
    if (b >= a)		    /* b >= a -> b/a >= 1 */
	return 0x00;	    /* 0 */
    return ~IcIntDiv(b,a);   /* 1 - b/a */
}

/* portion covered by both a and b */
CARD8
IcCombineConjointInPart (CARD8 a, CARD8 b)
{
    /* min (1,b/a) */

    if (b >= a)		    /* b >= a -> b/a >= 1 */
	return 0xff;	    /* 1 */
    return IcIntDiv(b,a);   /* b/a */
}

void
IcCombineConjointGeneralU (IcCompositeOperand   *src,
			   IcCompositeOperand   *msk,
			   IcCompositeOperand   *dst,
			   CARD8		combine)
{
    CARD32  s, d;
    CARD32  m,n,o,p;
    CARD16  Fa, Fb, t, u, v;
    CARD8   sa, da;

    s = IcCombineMaskU (src, msk);
    sa = s >> 24;
    
    d = (*dst->fetch) (dst);
    da = d >> 24;
    
    switch (combine & CombineA) {
    default:
	Fa = 0;
	break;
    case CombineAOut:
	Fa = IcCombineConjointOutPart (sa, da);
	break;
    case CombineAIn:
	Fa = IcCombineConjointInPart (sa, da);
	break;
    case CombineA:
	Fa = 0xff;
	break;
    }
    
    switch (combine & CombineB) {
    default:
	Fb = 0;
	break;
    case CombineBOut:
	Fb = IcCombineConjointOutPart (da, sa);
	break;
    case CombineBIn:
	Fb = IcCombineConjointInPart (da, sa);
	break;
    case CombineB:
	Fb = 0xff;
	break;
    }
    m = IcGen (s,d,0,Fa,Fb,t,u,v);
    n = IcGen (s,d,8,Fa,Fb,t,u,v);
    o = IcGen (s,d,16,Fa,Fb,t,u,v);
    p = IcGen (s,d,24,Fa,Fb,t,u,v);
    s = m|n|o|p;
    (*dst->store) (dst, s);
}

void
IcCombineConjointGeneralC (IcCompositeOperand   *src,
			   IcCompositeOperand   *msk,
			   IcCompositeOperand   *dst,
			   CARD8		combine)
{
    IcCompSrc	cs;
    CARD32  s, d;
    CARD32  m,n,o,p;
    CARD32  Fa;
    CARD16  Fb, t, u, v;
    CARD32  sa;
    CARD8   da;

    cs = IcCombineMaskC (src, msk);
    s = cs.value;
    sa = cs.alpha;
    
    d = (*dst->fetch) (dst);
    da = d >> 24;
    
    switch (combine & CombineA) {
    default:
	Fa = 0;
	break;
    case CombineAOut:
	m = IcCombineConjointOutPart ((CARD8) (sa >> 0), da);
	n = IcCombineConjointOutPart ((CARD8) (sa >> 8), da) << 8;
	o = IcCombineConjointOutPart ((CARD8) (sa >> 16), da) << 16;
	p = IcCombineConjointOutPart ((CARD8) (sa >> 24), da) << 24;
	Fa = m|n|o|p;
	break;
    case CombineAIn:
	m = IcCombineConjointOutPart ((CARD8) (sa >> 0), da);
	n = IcCombineConjointOutPart ((CARD8) (sa >> 8), da) << 8;
	o = IcCombineConjointOutPart ((CARD8) (sa >> 16), da) << 16;
	p = IcCombineConjointOutPart ((CARD8) (sa >> 24), da) << 24;
	Fa = m|n|o|p;
	break;
    case CombineA:
	Fa = 0xffffffff;
	break;
    }
    
    switch (combine & CombineB) {
    default:
	Fb = 0;
	break;
    case CombineBOut:
	Fb = IcCombineConjointOutPart (da, sa);
	break;
    case CombineBIn:
	Fb = IcCombineConjointInPart (da, sa);
	break;
    case CombineB:
	Fb = 0xff;
	break;
    }
    m = IcGen (s,d,0,IcGet8(Fa,0),Fb,t,u,v);
    n = IcGen (s,d,8,IcGet8(Fa,8),Fb,t,u,v);
    o = IcGen (s,d,16,IcGet8(Fa,16),Fb,t,u,v);
    p = IcGen (s,d,24,IcGet8(Fa,24),Fb,t,u,v);
    s = m|n|o|p;
    (*dst->store) (dst, s);
}

void
IcCombineConjointOverU (IcCompositeOperand   *src,
			IcCompositeOperand   *msk,
			IcCompositeOperand   *dst)
{
    IcCombineConjointGeneralU (src, msk, dst, CombineAOver);
/*
    CARD32  s, d;
    CARD16  a;
    CARD16  t;
    CARD32  m,n,o,p;

    s = IcCombineMaskU (src, msk);
    a = s >> 24;
    if (a != 0x00)
    {
	if (a != 0xff)
	{
	    d = (*dst->fetch) (dst);
	    a = IcCombineConjointOutPart (d >> 24, a);
	    m = IcOverU(s,d,0,a,t);
	    n = IcOverU(s,d,8,a,t);
	    o = IcOverU(s,d,16,a,t);
	    p = IcOverU(s,d,24,a,t);
	    s = m|n|o|p;
	}
	(*dst->store) (dst, s);
    }
 */
}

void
IcCombineConjointOverC (IcCompositeOperand   *src,
			IcCompositeOperand   *msk,
			IcCompositeOperand   *dst)
{
    IcCombineConjointGeneralC (src, msk, dst, CombineAOver);
}

void
IcCombineConjointOverReverseU (IcCompositeOperand    *src,
			       IcCompositeOperand    *msk,
			       IcCompositeOperand    *dst)
{
    IcCombineConjointGeneralU (src, msk, dst, CombineBOver);
}

void
IcCombineConjointOverReverseC (IcCompositeOperand    *src,
			       IcCompositeOperand    *msk,
			       IcCompositeOperand    *dst)
{
    IcCombineConjointGeneralC (src, msk, dst, CombineBOver);
}

void
IcCombineConjointInU (IcCompositeOperand	    *src,
		      IcCompositeOperand	    *msk,
		      IcCompositeOperand	    *dst)
{
    IcCombineConjointGeneralU (src, msk, dst, CombineAIn);
}

void
IcCombineConjointInC (IcCompositeOperand	    *src,
		      IcCompositeOperand	    *msk,
		      IcCompositeOperand	    *dst)
{
    IcCombineConjointGeneralC (src, msk, dst, CombineAIn);
}

void
IcCombineConjointInReverseU (IcCompositeOperand  *src,
			     IcCompositeOperand  *msk,
			     IcCompositeOperand  *dst)
{
    IcCombineConjointGeneralU (src, msk, dst, CombineBIn);
}

void
IcCombineConjointInReverseC (IcCompositeOperand  *src,
			     IcCompositeOperand  *msk,
			     IcCompositeOperand  *dst)
{
    IcCombineConjointGeneralC (src, msk, dst, CombineBIn);
}

void
IcCombineConjointOutU (IcCompositeOperand    *src,
		       IcCompositeOperand    *msk,
		       IcCompositeOperand    *dst)
{
    IcCombineConjointGeneralU (src, msk, dst, CombineAOut);
}

void
IcCombineConjointOutC (IcCompositeOperand    *src,
		       IcCompositeOperand    *msk,
		       IcCompositeOperand    *dst)
{
    IcCombineConjointGeneralC (src, msk, dst, CombineAOut);
}

void
IcCombineConjointOutReverseU (IcCompositeOperand *src,
			      IcCompositeOperand *msk,
			      IcCompositeOperand *dst)
{
    IcCombineConjointGeneralU (src, msk, dst, CombineBOut);
}

void
IcCombineConjointOutReverseC (IcCompositeOperand *src,
			      IcCompositeOperand *msk,
			      IcCompositeOperand *dst)
{
    IcCombineConjointGeneralC (src, msk, dst, CombineBOut);
}

void
IcCombineConjointAtopU (IcCompositeOperand   *src,
			IcCompositeOperand   *msk,
			IcCompositeOperand   *dst)
{
    IcCombineConjointGeneralU (src, msk, dst, CombineAAtop);
}

void
IcCombineConjointAtopC (IcCompositeOperand   *src,
			IcCompositeOperand   *msk,
			IcCompositeOperand   *dst)
{
    IcCombineConjointGeneralC (src, msk, dst, CombineAAtop);
}

void
IcCombineConjointAtopReverseU (IcCompositeOperand    *src,
			       IcCompositeOperand    *msk,
			       IcCompositeOperand    *dst)
{
    IcCombineConjointGeneralU (src, msk, dst, CombineBAtop);
}

void
IcCombineConjointAtopReverseC (IcCompositeOperand    *src,
			       IcCompositeOperand    *msk,
			       IcCompositeOperand    *dst)
{
    IcCombineConjointGeneralC (src, msk, dst, CombineBAtop);
}

void
IcCombineConjointXorU (IcCompositeOperand    *src,
		       IcCompositeOperand    *msk,
		       IcCompositeOperand    *dst)
{
    IcCombineConjointGeneralU (src, msk, dst, CombineXor);
}

void
IcCombineConjointXorC (IcCompositeOperand    *src,
		       IcCompositeOperand    *msk,
		       IcCompositeOperand    *dst)
{
    IcCombineConjointGeneralC (src, msk, dst, CombineXor);
}

IcCombineFunc	IcCombineFuncU[] = {
    IcCombineClear,
    IcCombineSrcU,
    IcCombineDst,
    IcCombineOverU,
    IcCombineOverReverseU,
    IcCombineInU,
    IcCombineInReverseU,
    IcCombineOutU,
    IcCombineOutReverseU,
    IcCombineAtopU,
    IcCombineAtopReverseU,
    IcCombineXorU,
    IcCombineAddU,
    IcCombineDisjointOverU, /* Saturate */
    0,
    0,
    IcCombineClear,
    IcCombineSrcU,
    IcCombineDst,
    IcCombineDisjointOverU,
    IcCombineDisjointOverReverseU,
    IcCombineDisjointInU,
    IcCombineDisjointInReverseU,
    IcCombineDisjointOutU,
    IcCombineDisjointOutReverseU,
    IcCombineDisjointAtopU,
    IcCombineDisjointAtopReverseU,
    IcCombineDisjointXorU,
    0,
    0,
    0,
    0,
    IcCombineClear,
    IcCombineSrcU,
    IcCombineDst,
    IcCombineConjointOverU,
    IcCombineConjointOverReverseU,
    IcCombineConjointInU,
    IcCombineConjointInReverseU,
    IcCombineConjointOutU,
    IcCombineConjointOutReverseU,
    IcCombineConjointAtopU,
    IcCombineConjointAtopReverseU,
    IcCombineConjointXorU,
};

IcCombineFunc	IcCombineFuncC[] = {
    IcCombineClear,
    IcCombineSrcC,
    IcCombineDst,
    IcCombineOverC,
    IcCombineOverReverseC,
    IcCombineInC,
    IcCombineInReverseC,
    IcCombineOutC,
    IcCombineOutReverseC,
    IcCombineAtopC,
    IcCombineAtopReverseC,
    IcCombineXorC,
    IcCombineAddC,
    IcCombineDisjointOverC, /* Saturate */
    0,
    0,
    IcCombineClear,	    /* 0x10 */
    IcCombineSrcC,
    IcCombineDst,
    IcCombineDisjointOverC,
    IcCombineDisjointOverReverseC,
    IcCombineDisjointInC,
    IcCombineDisjointInReverseC,
    IcCombineDisjointOutC,
    IcCombineDisjointOutReverseC,
    IcCombineDisjointAtopC,
    IcCombineDisjointAtopReverseC,
    IcCombineDisjointXorC,  /* 0x1b */
    0,
    0,
    0,
    0,
    IcCombineClear,
    IcCombineSrcC,
    IcCombineDst,
    IcCombineConjointOverC,
    IcCombineConjointOverReverseC,
    IcCombineConjointInC,
    IcCombineConjointInReverseC,
    IcCombineConjointOutC,
    IcCombineConjointOutReverseC,
    IcCombineConjointAtopC,
    IcCombineConjointAtopReverseC,
    IcCombineConjointXorC,
};

/*
 * All of the fetch functions
 */

CARD32
IcFetch_a8r8g8b8 (IcCompositeOperand *op)
{
    IcBits  *line = op->u.drawable.line; CARD32 offset = op->u.drawable.offset;
    return ((CARD32 *)line)[offset >> 5];
}

CARD32
IcFetch_x8r8g8b8 (IcCompositeOperand *op)
{
    IcBits  *line = op->u.drawable.line; CARD32 offset = op->u.drawable.offset;
    return ((CARD32 *)line)[offset >> 5] | 0xff000000;
}

CARD32
IcFetch_a8b8g8r8 (IcCompositeOperand *op)
{
    IcBits  *line = op->u.drawable.line; CARD32 offset = op->u.drawable.offset;
    CARD32  pixel = ((CARD32 *)line)[offset >> 5];

    return ((pixel & 0xff000000) |
	    ((pixel >> 16) & 0xff) |
	    (pixel & 0x0000ff00) |
	    ((pixel & 0xff) << 16));
}

CARD32
IcFetch_x8b8g8r8 (IcCompositeOperand *op)
{
    IcBits  *line = op->u.drawable.line; CARD32 offset = op->u.drawable.offset;
    CARD32  pixel = ((CARD32 *)line)[offset >> 5];

    return ((0xff000000) |
	    ((pixel >> 16) & 0xff) |
	    (pixel & 0x0000ff00) |
	    ((pixel & 0xff) << 16));
}

CARD32
IcFetch_r8g8b8 (IcCompositeOperand *op)
{
    IcBits  *line = op->u.drawable.line; CARD32 offset = op->u.drawable.offset;
    CARD8   *pixel = ((CARD8 *) line) + (offset >> 3);
#if IMAGE_BYTE_ORDER == MSBFirst
    return (0xff000000 |
	    (pixel[0] << 16) |
	    (pixel[1] << 8) |
	    (pixel[2]));
#else
    return (0xff000000 |
	    (pixel[2] << 16) |
	    (pixel[1] << 8) |
	    (pixel[0]));
#endif
}

CARD32
IcFetch_b8g8r8 (IcCompositeOperand *op)
{
    IcBits  *line = op->u.drawable.line; CARD32 offset = op->u.drawable.offset;
    CARD8   *pixel = ((CARD8 *) line) + (offset >> 3);
#if IMAGE_BYTE_ORDER == MSBFirst
    return (0xff000000 |
	    (pixel[2] << 16) |
	    (pixel[1] << 8) |
	    (pixel[0]));
#else
    return (0xff000000 |
	    (pixel[0] << 16) |
	    (pixel[1] << 8) |
	    (pixel[2]));
#endif
}

CARD32
IcFetch_r5g6b5 (IcCompositeOperand *op)
{
    IcBits  *line = op->u.drawable.line; CARD32 offset = op->u.drawable.offset;
    CARD32  pixel = ((CARD16 *) line)[offset >> 4];
    CARD32  r,g,b;

    r = ((pixel & 0xf800) | ((pixel & 0xe000) >> 5)) << 8;
    g = ((pixel & 0x07e0) | ((pixel & 0x0600) >> 6)) << 5;
    b = ((pixel & 0x001c) | ((pixel & 0x001f) << 5)) >> 2;
    return (0xff000000 | r | g | b);
}

CARD32
IcFetch_b5g6r5 (IcCompositeOperand *op)
{
    IcBits  *line = op->u.drawable.line; CARD32 offset = op->u.drawable.offset;
    CARD32  pixel = ((CARD16 *) line)[offset >> 4];
    CARD32  r,g,b;

    b = ((pixel & 0xf800) | ((pixel & 0xe000) >> 5)) >> 8;
    g = ((pixel & 0x07e0) | ((pixel & 0x0600) >> 6)) << 5;
    r = ((pixel & 0x001c) | ((pixel & 0x001f) << 5)) << 14;
    return (0xff000000 | r | g | b);
}

CARD32
IcFetch_a1r5g5b5 (IcCompositeOperand *op)
{
    IcBits  *line = op->u.drawable.line; CARD32 offset = op->u.drawable.offset;
    CARD32  pixel = ((CARD16 *) line)[offset >> 4];
    CARD32  a,r,g,b;

    a = (CARD32) ((CARD8) (0 - ((pixel & 0x8000) >> 15))) << 24;
    r = ((pixel & 0x7c00) | ((pixel & 0x7000) >> 5)) << 9;
    g = ((pixel & 0x03e0) | ((pixel & 0x0380) >> 5)) << 6;
    b = ((pixel & 0x001c) | ((pixel & 0x001f) << 5)) >> 2;
    return (a | r | g | b);
}

CARD32
IcFetch_x1r5g5b5 (IcCompositeOperand *op)
{
    IcBits  *line = op->u.drawable.line; CARD32 offset = op->u.drawable.offset;
    CARD32  pixel = ((CARD16 *) line)[offset >> 4];
    CARD32  r,g,b;

    r = ((pixel & 0x7c00) | ((pixel & 0x7000) >> 5)) << 9;
    g = ((pixel & 0x03e0) | ((pixel & 0x0380) >> 5)) << 6;
    b = ((pixel & 0x001c) | ((pixel & 0x001f) << 5)) >> 2;
    return (0xff000000 | r | g | b);
}

CARD32
IcFetch_a1b5g5r5 (IcCompositeOperand *op)
{
    IcBits  *line = op->u.drawable.line; CARD32 offset = op->u.drawable.offset;
    CARD32  pixel = ((CARD16 *) line)[offset >> 4];
    CARD32  a,r,g,b;

    a = (CARD32) ((CARD8) (0 - ((pixel & 0x8000) >> 15))) << 24;
    b = ((pixel & 0x7c00) | ((pixel & 0x7000) >> 5)) >> 7;
    g = ((pixel & 0x03e0) | ((pixel & 0x0380) >> 5)) << 6;
    r = ((pixel & 0x001c) | ((pixel & 0x001f) << 5)) << 14;
    return (a | r | g | b);
}

CARD32
IcFetch_x1b5g5r5 (IcCompositeOperand *op)
{
    IcBits  *line = op->u.drawable.line; CARD32 offset = op->u.drawable.offset;
    CARD32  pixel = ((CARD16 *) line)[offset >> 4];
    CARD32  r,g,b;

    b = ((pixel & 0x7c00) | ((pixel & 0x7000) >> 5)) >> 7;
    g = ((pixel & 0x03e0) | ((pixel & 0x0380) >> 5)) << 6;
    r = ((pixel & 0x001c) | ((pixel & 0x001f) << 5)) << 14;
    return (0xff000000 | r | g | b);
}

CARD32
IcFetch_a4r4g4b4 (IcCompositeOperand *op)
{
    IcBits  *line = op->u.drawable.line; CARD32 offset = op->u.drawable.offset;
    CARD32  pixel = ((CARD16 *) line)[offset >> 4];
    CARD32  a,r,g,b;

    a = ((pixel & 0xf000) | ((pixel & 0xf000) >> 4)) << 16;
    r = ((pixel & 0x0f00) | ((pixel & 0x0f00) >> 4)) << 12;
    g = ((pixel & 0x00f0) | ((pixel & 0x00f0) >> 4)) << 8;
    b = ((pixel & 0x000f) | ((pixel & 0x000f) << 4));
    return (a | r | g | b);
}
    
CARD32
IcFetch_x4r4g4b4 (IcCompositeOperand *op)
{
    IcBits  *line = op->u.drawable.line; CARD32 offset = op->u.drawable.offset;
    CARD32  pixel = ((CARD16 *) line)[offset >> 4];
    CARD32  r,g,b;

    r = ((pixel & 0x0f00) | ((pixel & 0x0f00) >> 4)) << 12;
    g = ((pixel & 0x00f0) | ((pixel & 0x00f0) >> 4)) << 8;
    b = ((pixel & 0x000f) | ((pixel & 0x000f) << 4));
    return (0xff000000 | r | g | b);
}
    
CARD32
IcFetch_a4b4g4r4 (IcCompositeOperand *op)
{
    IcBits  *line = op->u.drawable.line; CARD32 offset = op->u.drawable.offset;
    CARD32  pixel = ((CARD16 *) line)[offset >> 4];
    CARD32  a,r,g,b;

    a = ((pixel & 0xf000) | ((pixel & 0xf000) >> 4)) << 16;
    b = ((pixel & 0x0f00) | ((pixel & 0x0f00) >> 4)) << 12;
    g = ((pixel & 0x00f0) | ((pixel & 0x00f0) >> 4)) << 8;
    r = ((pixel & 0x000f) | ((pixel & 0x000f) << 4));
    return (a | r | g | b);
}
    
CARD32
IcFetch_x4b4g4r4 (IcCompositeOperand *op)
{
    IcBits  *line = op->u.drawable.line; CARD32 offset = op->u.drawable.offset;
    CARD32  pixel = ((CARD16 *) line)[offset >> 4];
    CARD32  r,g,b;

    b = ((pixel & 0x0f00) | ((pixel & 0x0f00) >> 4)) << 12;
    g = ((pixel & 0x00f0) | ((pixel & 0x00f0) >> 4)) << 8;
    r = ((pixel & 0x000f) | ((pixel & 0x000f) << 4));
    return (0xff000000 | r | g | b);
}
    
CARD32
IcFetch_a8 (IcCompositeOperand *op)
{
    IcBits  *line = op->u.drawable.line; CARD32 offset = op->u.drawable.offset;
    CARD32   pixel = ((CARD8 *) line)[offset>>3];
    
    return pixel << 24;
}

CARD32
IcFetcha_a8 (IcCompositeOperand *op)
{
    IcBits  *line = op->u.drawable.line; CARD32 offset = op->u.drawable.offset;
    CARD32   pixel = ((CARD8 *) line)[offset>>3];
    
    pixel |= pixel << 8;
    pixel |= pixel << 16;
    return pixel;
}

CARD32
IcFetch_r3g3b2 (IcCompositeOperand *op)
{
    IcBits  *line = op->u.drawable.line; CARD32 offset = op->u.drawable.offset;
    CARD32   pixel = ((CARD8 *) line)[offset>>3];
    CARD32  r,g,b;
    
    r = ((pixel & 0xe0) | ((pixel & 0xe0) >> 3) | ((pixel & 0xc0) >> 6)) << 16;
    g = ((pixel & 0x1c) | ((pixel & 0x18) >> 3) | ((pixel & 0x1c) << 3)) << 8;
    b = (((pixel & 0x03)     ) | 
	 ((pixel & 0x03) << 2) | 
	 ((pixel & 0x03) << 4) |
	 ((pixel & 0x03) << 6));
    return (0xff000000 | r | g | b);
}

CARD32
IcFetch_b2g3r3 (IcCompositeOperand *op)
{
    IcBits  *line = op->u.drawable.line; CARD32 offset = op->u.drawable.offset;
    CARD32   pixel = ((CARD8 *) line)[offset>>3];
    CARD32  r,g,b;
    
    b = (((pixel & 0xc0)     ) | 
	 ((pixel & 0xc0) >> 2) |
	 ((pixel & 0xc0) >> 4) |
	 ((pixel & 0xc0) >> 6));
    g = ((pixel & 0x38) | ((pixel & 0x38) >> 3) | ((pixel & 0x30) << 2)) << 8;
    r = (((pixel & 0x07)     ) | 
	 ((pixel & 0x07) << 3) | 
	 ((pixel & 0x06) << 6)) << 16;
    return (0xff000000 | r | g | b);
}

CARD32
IcFetch_a2r2g2b2 (IcCompositeOperand *op)
{
    IcBits  *line = op->u.drawable.line; CARD32 offset = op->u.drawable.offset;
    CARD32   pixel = ((CARD8 *) line)[offset>>3];
    CARD32   a,r,g,b;

    a = ((pixel & 0xc0) * 0x55) << 18;
    r = ((pixel & 0x30) * 0x55) << 12;
    g = ((pixel & 0x0c) * 0x55) << 6;
    b = ((pixel & 0x03) * 0x55);
    return a|r|g|b;
}

CARD32
IcFetch_a2b2g2r2 (IcCompositeOperand *op)
{
    IcBits  *line = op->u.drawable.line; CARD32 offset = op->u.drawable.offset;
    CARD32   pixel = ((CARD8 *) line)[offset>>3];
    CARD32   a,r,g,b;

    a = ((pixel & 0xc0) * 0x55) << 18;
    b = ((pixel & 0x30) * 0x55) >> 6;
    g = ((pixel & 0x0c) * 0x55) << 6;
    r = ((pixel & 0x03) * 0x55) << 16;
    return a|r|g|b;
}

CARD32
IcFetch_c8 (IcCompositeOperand *op)
{
    IcBits  *line = op->u.drawable.line; CARD32 offset = op->u.drawable.offset;
    CARD32   pixel = ((CARD8 *) line)[offset>>3];

    return op->indexed->rgba[pixel];
}

#define Fetch8(l,o)    (((CARD8 *) (l))[(o) >> 3])
#if IMAGE_BYTE_ORDER == MSBFirst
#define Fetch4(l,o)    ((o) & 2 ? Fetch8(l,o) & 0xf : Fetch8(l,o) >> 4)
#else
#define Fetch4(l,o)    ((o) & 2 ? Fetch8(l,o) >> 4 : Fetch8(l,o) & 0xf)
#endif

CARD32
IcFetch_a4 (IcCompositeOperand *op)
{
    IcBits  *line = op->u.drawable.line; CARD32 offset = op->u.drawable.offset;
    CARD32  pixel = Fetch4(line, offset);
    
    pixel |= pixel << 4;
    return pixel << 24;
}

CARD32
IcFetcha_a4 (IcCompositeOperand *op)
{
    IcBits  *line = op->u.drawable.line; CARD32 offset = op->u.drawable.offset;
    CARD32  pixel = Fetch4(line, offset);
    
    pixel |= pixel << 4;
    pixel |= pixel << 8;
    pixel |= pixel << 16;
    return pixel;
}

CARD32
IcFetch_r1g2b1 (IcCompositeOperand *op)
{
    IcBits  *line = op->u.drawable.line; CARD32 offset = op->u.drawable.offset;
    CARD32  pixel = Fetch4(line, offset);
    CARD32  r,g,b;

    r = ((pixel & 0x8) * 0xff) << 13;
    g = ((pixel & 0x6) * 0x55) << 7;
    b = ((pixel & 0x1) * 0xff);
    return 0xff000000|r|g|b;
}

CARD32
IcFetch_b1g2r1 (IcCompositeOperand *op)
{
    IcBits  *line = op->u.drawable.line; CARD32 offset = op->u.drawable.offset;
    CARD32  pixel = Fetch4(line, offset);
    CARD32  r,g,b;

    b = ((pixel & 0x8) * 0xff) >> 3;
    g = ((pixel & 0x6) * 0x55) << 7;
    r = ((pixel & 0x1) * 0xff) << 16;
    return 0xff000000|r|g|b;
}

CARD32
IcFetch_a1r1g1b1 (IcCompositeOperand *op)
{
    IcBits  *line = op->u.drawable.line; CARD32 offset = op->u.drawable.offset;
    CARD32  pixel = Fetch4(line, offset);
    CARD32  a,r,g,b;

    a = ((pixel & 0x8) * 0xff) << 21;
    r = ((pixel & 0x4) * 0xff) << 14;
    g = ((pixel & 0x2) * 0xff) << 7;
    b = ((pixel & 0x1) * 0xff);
    return a|r|g|b;
}

CARD32
IcFetch_a1b1g1r1 (IcCompositeOperand *op)
{
    IcBits  *line = op->u.drawable.line; CARD32 offset = op->u.drawable.offset;
    CARD32  pixel = Fetch4(line, offset);
    CARD32  a,r,g,b;

    a = ((pixel & 0x8) * 0xff) << 21;
    r = ((pixel & 0x4) * 0xff) >> 3;
    g = ((pixel & 0x2) * 0xff) << 7;
    b = ((pixel & 0x1) * 0xff) << 16;
    return a|r|g|b;
}

CARD32
IcFetch_c4 (IcCompositeOperand *op)
{
    IcBits  *line = op->u.drawable.line; CARD32 offset = op->u.drawable.offset;
    CARD32  pixel = Fetch4(line, offset);

    return op->indexed->rgba[pixel];
}

CARD32
IcFetcha_a1 (IcCompositeOperand *op)
{
    IcBits  *line = op->u.drawable.line; CARD32 offset = op->u.drawable.offset;
    CARD32  pixel = ((CARD32 *)line)[offset >> 5];
    CARD32  a;
#if BITMAP_BIT_ORDER == MSBFirst
    a = pixel >> (0x1f - (offset & 0x1f));
#else
    a = pixel >> (offset & 0x1f);
#endif
    a = a & 1;
    a |= a << 1;
    a |= a << 2;
    a |= a << 4;
    a |= a << 8;
    a |= a << 16;
    return a;
}

CARD32
IcFetch_a1 (IcCompositeOperand *op)
{
    IcBits  *line = op->u.drawable.line; CARD32 offset = op->u.drawable.offset;
    CARD32  pixel = ((CARD32 *)line)[offset >> 5];
    CARD32  a;
#if BITMAP_BIT_ORDER == MSBFirst
    a = pixel >> (0x1f - (offset & 0x1f));
#else
    a = pixel >> (offset & 0x1f);
#endif
    a = a & 1;
    a |= a << 1;
    a |= a << 2;
    a |= a << 4;
    return a << 24;
}

CARD32
IcFetch_g1 (IcCompositeOperand *op)
{
    IcBits  *line = op->u.drawable.line; CARD32 offset = op->u.drawable.offset;
    CARD32  pixel = ((CARD32 *)line)[offset >> 5];
    CARD32  a;
#if BITMAP_BIT_ORDER == MSBFirst
    a = pixel >> (0x1f - (offset & 0x1f));
#else
    a = pixel >> (offset & 0x1f);
#endif
    a = a & 1;
    return op->indexed->rgba[a];
}

/*
 * All the store functions
 */

#define Splita(v)	CARD32	a = ((v) >> 24), r = ((v) >> 16) & 0xff, g = ((v) >> 8) & 0xff, b = (v) & 0xff
#define Split(v)	CARD32	r = ((v) >> 16) & 0xff, g = ((v) >> 8) & 0xff, b = (v) & 0xff

void
IcStore_a8r8g8b8 (IcCompositeOperand *op, CARD32 value)
{
    IcBits  *line = op->u.drawable.line; CARD32 offset = op->u.drawable.offset;
    ((CARD32 *)line)[offset >> 5] = value;
}

void
IcStore_x8r8g8b8 (IcCompositeOperand *op, CARD32 value)
{
    IcBits  *line = op->u.drawable.line; CARD32 offset = op->u.drawable.offset;
    ((CARD32 *)line)[offset >> 5] = value & 0xffffff;
}

void
IcStore_a8b8g8r8 (IcCompositeOperand *op, CARD32 value)
{
    IcBits  *line = op->u.drawable.line; CARD32 offset = op->u.drawable.offset;
    Splita(value);
    ((CARD32 *)line)[offset >> 5] = a << 24 | b << 16 | g << 8 | r;
}

void
IcStore_x8b8g8r8 (IcCompositeOperand *op, CARD32 value)
{
    IcBits  *line = op->u.drawable.line; CARD32 offset = op->u.drawable.offset;
    Split(value);
    ((CARD32 *)line)[offset >> 5] = b << 16 | g << 8 | r;
}

void
IcStore_r8g8b8 (IcCompositeOperand *op, CARD32 value)
{
    IcBits  *line = op->u.drawable.line; CARD32 offset = op->u.drawable.offset;
    CARD8   *pixel = ((CARD8 *) line) + (offset >> 3);
    Split(value);
#if IMAGE_BYTE_ORDER == MSBFirst
    pixel[0] = r;
    pixel[1] = g;
    pixel[2] = b;
#else
    pixel[0] = b;
    pixel[1] = g;
    pixel[2] = r;
#endif
}

void
IcStore_b8g8r8 (IcCompositeOperand *op, CARD32 value)
{
    IcBits  *line = op->u.drawable.line; CARD32 offset = op->u.drawable.offset;
    CARD8   *pixel = ((CARD8 *) line) + (offset >> 3);
    Split(value);
#if IMAGE_BYTE_ORDER == MSBFirst
    pixel[0] = b;
    pixel[1] = g;
    pixel[2] = r;
#else
    pixel[0] = r;
    pixel[1] = g;
    pixel[2] = b;
#endif
}

void
IcStore_r5g6b5 (IcCompositeOperand *op, CARD32 value)
{
    IcBits  *line = op->u.drawable.line; CARD32 offset = op->u.drawable.offset;
    CARD16  *pixel = ((CARD16 *) line) + (offset >> 4);
    Split(value);
    *pixel = (((r << 8) & 0xf800) |
	      ((g << 3) & 0x07e0) |
	      ((b >> 3)         ));
}

void
IcStore_b5g6r5 (IcCompositeOperand *op, CARD32 value)
{
    IcBits  *line = op->u.drawable.line; CARD32 offset = op->u.drawable.offset;
    CARD16  *pixel = ((CARD16 *) line) + (offset >> 4);
    Split(value);
    *pixel = (((b << 8) & 0xf800) |
	      ((g << 3) & 0x07e0) |
	      ((r >> 3)         ));
}

void
IcStore_a1r5g5b5 (IcCompositeOperand *op, CARD32 value)
{
    IcBits  *line = op->u.drawable.line; CARD32 offset = op->u.drawable.offset;
    CARD16  *pixel = ((CARD16 *) line) + (offset >> 4);
    Splita(value);
    *pixel = (((a << 8) & 0x8000) |
	      ((r << 7) & 0x7c00) |
	      ((g << 2) & 0x03e0) |
	      ((b >> 3)         ));
}

void
IcStore_x1r5g5b5 (IcCompositeOperand *op, CARD32 value)
{
    IcBits  *line = op->u.drawable.line; CARD32 offset = op->u.drawable.offset;
    CARD16  *pixel = ((CARD16 *) line) + (offset >> 4);
    Split(value);
    *pixel = (((r << 7) & 0x7c00) |
	      ((g << 2) & 0x03e0) |
	      ((b >> 3)         ));
}

void
IcStore_a1b5g5r5 (IcCompositeOperand *op, CARD32 value)
{
    IcBits  *line = op->u.drawable.line; CARD32 offset = op->u.drawable.offset;
    CARD16  *pixel = ((CARD16 *) line) + (offset >> 4);
    Splita(value);
    *pixel = (((a << 8) & 0x8000) |
	      ((b << 7) & 0x7c00) |
	      ((g << 2) & 0x03e0) |
	      ((r >> 3)         ));
}

void
IcStore_x1b5g5r5 (IcCompositeOperand *op, CARD32 value)
{
    IcBits  *line = op->u.drawable.line; CARD32 offset = op->u.drawable.offset;
    CARD16  *pixel = ((CARD16 *) line) + (offset >> 4);
    Split(value);
    *pixel = (((b << 7) & 0x7c00) |
	      ((g << 2) & 0x03e0) |
	      ((r >> 3)         ));
}

void
IcStore_a4r4g4b4 (IcCompositeOperand *op, CARD32 value)
{
    IcBits  *line = op->u.drawable.line; CARD32 offset = op->u.drawable.offset;
    CARD16  *pixel = ((CARD16 *) line) + (offset >> 4);
    Splita(value);
    *pixel = (((a << 8) & 0xf000) |
	      ((r << 4) & 0x0f00) |
	      ((g     ) & 0x00f0) |
	      ((b >> 4)         ));
}

void
IcStore_x4r4g4b4 (IcCompositeOperand *op, CARD32 value)
{
    IcBits  *line = op->u.drawable.line; CARD32 offset = op->u.drawable.offset;
    CARD16  *pixel = ((CARD16 *) line) + (offset >> 4);
    Split(value);
    *pixel = (((r << 4) & 0x0f00) |
	      ((g     ) & 0x00f0) |
	      ((b >> 4)         ));
}

void
IcStore_a4b4g4r4 (IcCompositeOperand *op, CARD32 value)
{
    IcBits  *line = op->u.drawable.line; CARD32 offset = op->u.drawable.offset;
    CARD16  *pixel = ((CARD16 *) line) + (offset >> 4);
    Splita(value);
    *pixel = (((a << 8) & 0xf000) |
	      ((b << 4) & 0x0f00) |
	      ((g     ) & 0x00f0) |
	      ((r >> 4)         ));
}

void
IcStore_x4b4g4r4 (IcCompositeOperand *op, CARD32 value)
{
    IcBits  *line = op->u.drawable.line; CARD32 offset = op->u.drawable.offset;
    CARD16  *pixel = ((CARD16 *) line) + (offset >> 4);
    Split(value);
    *pixel = (((b << 4) & 0x0f00) |
	      ((g     ) & 0x00f0) |
	      ((r >> 4)         ));
}

void
IcStore_a8 (IcCompositeOperand *op, CARD32 value)
{
    IcBits  *line = op->u.drawable.line; CARD32 offset = op->u.drawable.offset;
    CARD8   *pixel = ((CARD8 *) line) + (offset >> 3);
    *pixel = value >> 24;
}

void
IcStore_r3g3b2 (IcCompositeOperand *op, CARD32 value)
{
    IcBits  *line = op->u.drawable.line; CARD32 offset = op->u.drawable.offset;
    CARD8   *pixel = ((CARD8 *) line) + (offset >> 3);
    Split(value);
    *pixel = (((r     ) & 0xe0) |
	      ((g >> 3) & 0x1c) |
	      ((b >> 6)       ));
}

void
IcStore_b2g3r3 (IcCompositeOperand *op, CARD32 value)
{
    IcBits  *line = op->u.drawable.line; CARD32 offset = op->u.drawable.offset;
    CARD8   *pixel = ((CARD8 *) line) + (offset >> 3);
    Split(value);
    *pixel = (((b     ) & 0xe0) |
	      ((g >> 3) & 0x1c) |
	      ((r >> 6)       ));
}

void
IcStore_a2r2g2b2 (IcCompositeOperand *op, CARD32 value)
{
    IcBits  *line = op->u.drawable.line; CARD32 offset = op->u.drawable.offset;
    CARD8   *pixel = ((CARD8 *) line) + (offset >> 3);
    Splita(value);
    *pixel = (((a     ) & 0xc0) |
	      ((r >> 2) & 0x30) |
	      ((g >> 4) & 0x0c) |
	      ((b >> 6)       ));
}

void
IcStore_c8 (IcCompositeOperand *op, CARD32 value)
{
    IcBits  *line = op->u.drawable.line; CARD32 offset = op->u.drawable.offset;
    CARD8   *pixel = ((CARD8 *) line) + (offset >> 3);
    *pixel = IcIndexToEnt24(op->indexed,value);
}

void
IcStore_g8 (IcCompositeOperand *op, CARD32 value)
{
    IcBits  *line = op->u.drawable.line; CARD32 offset = op->u.drawable.offset;
    CARD8   *pixel = ((CARD8 *) line) + (offset >> 3);
    *pixel = IcIndexToEntY24(op->indexed,value);
}

#define Store8(l,o,v)  (((CARD8 *) l)[(o) >> 3] = (v))
#if IMAGE_BYTE_ORDER == MSBFirst
#define Store4(l,o,v)  Store8(l,o,((o) & 4 ? \
				   (Fetch8(l,o) & 0xf0) | (v) : \
				   (Fetch8(l,o) & 0x0f) | ((v) << 4)))
#else
#define Store4(l,o,v)  Store8(l,o,((o) & 4 ? \
				   (Fetch8(l,o) & 0x0f) | ((v) << 4) : \
				   (Fetch8(l,o) & 0xf0) | (v)))
#endif

void
IcStore_a4 (IcCompositeOperand *op, CARD32 value)
{
    IcBits  *line = op->u.drawable.line; CARD32 offset = op->u.drawable.offset;
    Store4(line,offset,value>>28);
}

void
IcStore_r1g2b1 (IcCompositeOperand *op, CARD32 value)
{
    IcBits  *line = op->u.drawable.line; CARD32 offset = op->u.drawable.offset;
    CARD32  pixel;
    
    Split(value);
    pixel = (((r >> 4) & 0x8) |
	     ((g >> 5) & 0x6) |
	     ((b >> 7)      ));
    Store4(line,offset,pixel);
}

void
IcStore_b1g2r1 (IcCompositeOperand *op, CARD32 value)
{
    IcBits  *line = op->u.drawable.line; CARD32 offset = op->u.drawable.offset;
    CARD32  pixel;
    
    Split(value);
    pixel = (((b >> 4) & 0x8) |
	     ((g >> 5) & 0x6) |
	     ((r >> 7)      ));
    Store4(line,offset,pixel);
}

void
IcStore_a1r1g1b1 (IcCompositeOperand *op, CARD32 value)
{
    IcBits  *line = op->u.drawable.line; CARD32 offset = op->u.drawable.offset;
    CARD32  pixel;
    Splita(value);
    pixel = (((a >> 4) & 0x8) |
	     ((r >> 5) & 0x4) |
	     ((g >> 6) & 0x2) |
	     ((b >> 7)      ));
    Store4(line,offset,pixel);
}

void
IcStore_a1b1g1r1 (IcCompositeOperand *op, CARD32 value)
{
    IcBits  *line = op->u.drawable.line; CARD32 offset = op->u.drawable.offset;
    CARD32  pixel;
    Splita(value);
    pixel = (((a >> 4) & 0x8) |
	     ((b >> 5) & 0x4) |
	     ((g >> 6) & 0x2) |
	     ((r >> 7)      ));
    Store4(line,offset,pixel);
}

void
IcStore_c4 (IcCompositeOperand *op, CARD32 value)
{
    IcBits  *line = op->u.drawable.line; CARD32 offset = op->u.drawable.offset;
    CARD32  pixel;
    
    pixel = IcIndexToEnt24(op->indexed,value);
    Store4(line,offset,pixel);
}

void
IcStore_g4 (IcCompositeOperand *op, CARD32 value)
{
    IcBits  *line = op->u.drawable.line; CARD32 offset = op->u.drawable.offset;
    CARD32  pixel;
    
    pixel = IcIndexToEntY24(op->indexed,value);
    Store4(line,offset,pixel);
}

void
IcStore_a1 (IcCompositeOperand *op, CARD32 value)
{
    IcBits  *line = op->u.drawable.line; CARD32 offset = op->u.drawable.offset;
    CARD32  *pixel = ((CARD32 *) line) + (offset >> 5);
    CARD32  mask = IcStipMask(offset & 0x1f, 1);

    value = value & 0x80000000 ? mask : 0;
    *pixel = (*pixel & ~mask) | value;
}

void
IcStore_g1 (IcCompositeOperand *op, CARD32 value)
{
    IcBits  *line = op->u.drawable.line; CARD32 offset = op->u.drawable.offset;
    CARD32  *pixel = ((CARD32 *) line) + (offset >> 5);
    CARD32  mask = IcStipMask(offset & 0x1f, 1);

    value = IcIndexToEntY24(op->indexed,value) ? mask : 0;
    *pixel = (*pixel & ~mask) | value;
}

CARD32
IcFetch_external (IcCompositeOperand *op)
{
    CARD32  rgb = (*op[1].fetch) (&op[1]);
    CARD32  a = (*op[2].fetch) (&op[2]);

    return (rgb & 0xffffff) | (a & 0xff000000);
}


CARD32
IcFetcha_external (IcCompositeOperand *op)
{
    return (*op[2].fetch) (&op[2]);
}

void
IcStore_external (IcCompositeOperand *op, CARD32 value)
{
    (*op[1].store) (&op[1], value | 0xff000000);
    (*op[2].store) (&op[2], value & 0xff000000);
}

CARD32
IcFetch_transform (IcCompositeOperand *op)
{
    return 0;
/* XXX: Still need to port this function
    PictVector	v;
    int		x, y;
    int		minx, maxx, miny, maxy;
    int		n;
    CARD32	rtot, gtot, btot, atot;
    CARD32	xerr, yerr;
    CARD32	bits;

    v.vector[0] = IntToxFixed(op->u.transform.x);
    v.vector[1] = IntToxFixed(op->u.transform.y);
    v.vector[2] = xFixed1;
    if (!PictureTransformPoint (op->u.transform.transform, &v))
	return 0;
    switch (op->u.transform.filter) {
    case PictFilterNearest:
	y = xFixedToInt (v.vector[1]) + op->u.transform.top_y;
	x = xFixedToInt (v.vector[0]) + op->u.transform.left_x;
	if (PixRegionPointInRegion (op->clip, x, y))
	{
	    (*op[1].set) (&op[1], x, y);
	    bits = (*op[1].fetch) (&op[1]);
	}
	else
	    bits = 0;
	break;
    case PictFilterBilinear:
	rtot = gtot = btot = atot = 0;
	miny = xFixedToInt (v.vector[1]) + op->u.transform.top_y;
	maxy = xFixedToInt (xFixedCeil (v.vector[1])) + op->u.transform.top_y;
	
	minx = xFixedToInt (v.vector[0]) + op->u.transform.left_x;
	maxx = xFixedToInt (xFixedCeil (v.vector[0])) + op->u.transform.left_x;
	
	yerr = xFixed1 - xFixedFrac (v.vector[1]);
	for (y = miny; y <= maxy; y++)
	{
	    CARD32	lrtot = 0, lgtot = 0, lbtot = 0, latot = 0;
	    
	    xerr = xFixed1 - xFixedFrac (v.vector[0]);
	    for (x = minx; x <= maxx; x++)
	    {
		if (PixRegionPointInRegion (op->clip, x, y))
		{
		    (*op[1].set) (&op[1], x, y);
		    bits = (*op[1].fetch) (&op[1]);
		    {
			Splita(bits);
			lrtot += r * xerr;
			lgtot += g * xerr;
			lbtot += b * xerr;
			latot += a * xerr;
			n++;
		    }
		}
		xerr = xFixed1 - xerr;
	    }
	    rtot += (lrtot >> 10) * yerr;
	    gtot += (lgtot >> 10) * yerr;
	    btot += (lbtot >> 10) * yerr;
	    atot += (latot >> 10) * yerr;
	    yerr = xFixed1 - yerr;
	}
	if ((atot >>= 22) > 0xff) atot = 0xff;
	if ((rtot >>= 22) > 0xff) rtot = 0xff;
	if ((gtot >>= 22) > 0xff) gtot = 0xff;
	if ((btot >>= 22) > 0xff) btot = 0xff;
	bits = ((atot << 24) |
		(rtot << 16) |
		(gtot <<  8) |
		(btot       ));
	break;
    default:
	bits = 0;
	break;
    }
    return bits;
*/
}

CARD32
IcFetcha_transform (IcCompositeOperand *op)
{
    return 0;
/* XXX: Still need to port this function
    PictVector	v;
    int		x, y;
    int		minx, maxx, miny, maxy;
    int		n;
    CARD32	rtot, gtot, btot, atot;
    CARD32	xerr, yerr;
    CARD32	bits;

    v.vector[0] = IntToxFixed(op->u.transform.x);
    v.vector[1] = IntToxFixed(op->u.transform.y);
    v.vector[2] = xFixed1;
    if (!PictureTransformPoint (op->u.transform.transform, &v))
	return 0;
    switch (op->u.transform.filter) {
    case PictFilterNearest:
	y = xFixedToInt (v.vector[1]) + op->u.transform.left_x;
	x = xFixedToInt (v.vector[0]) + op->u.transform.top_y;
	if (PixRegionPointInRegion (op->clip, x, y))
	{
	    (*op[1].set) (&op[1], x, y);
	    bits = (*op[1].fetcha) (&op[1]);
	}
	else
	    bits = 0;
	break;
    case PictFilterBilinear:
	rtot = gtot = btot = atot = 0;
	
	miny = xFixedToInt (v.vector[1]) + op->u.transform.top_y;
	maxy = xFixedToInt (xFixedCeil (v.vector[1])) + op->u.transform.top_y;
	
	minx = xFixedToInt (v.vector[0]) + op->u.transform.left_x;
	maxx = xFixedToInt (xFixedCeil (v.vector[0])) + op->u.transform.left_x;
	
	yerr = xFixed1 - xFixedFrac (v.vector[1]);
	for (y = miny; y <= maxy; y++)
	{
	    CARD32	lrtot = 0, lgtot = 0, lbtot = 0, latot = 0;
	    xerr = xFixed1 - xFixedFrac (v.vector[0]);
	    for (x = minx; x <= maxx; x++)
	    {
		if (PixRegionPointInRegion (op->clip, x, y))
		{
		    (*op[1].set) (&op[1], x, y);
		    bits = (*op[1].fetcha) (&op[1]);
		    {
			Splita(bits);
			lrtot += r * xerr;
			lgtot += g * xerr;
			lbtot += b * xerr;
			latot += a * xerr;
			n++;
		    }
		}
		x++;
		xerr = xFixed1 - xerr;
	    }
	    rtot += (lrtot >> 10) * yerr;
	    gtot += (lgtot >> 10) * yerr;
	    btot += (lbtot >> 10) * yerr;
	    atot += (latot >> 10) * yerr;
	    y++;
	    yerr = xFixed1 - yerr;
	}
	if ((atot >>= 22) > 0xff) atot = 0xff;
	if ((rtot >>= 22) > 0xff) rtot = 0xff;
	if ((gtot >>= 22) > 0xff) gtot = 0xff;
	if ((btot >>= 22) > 0xff) btot = 0xff;
	bits = ((atot << 24) |
		(rtot << 16) |
		(gtot <<  8) |
		(btot       ));
	break;
    default:
	bits = 0;
	break;
    }
    return bits;
*/
}

IcAccessMap icAccessMap[] = {
    /* 32bpp formats */
    { PICT_a8r8g8b8,	IcFetch_a8r8g8b8,	IcFetch_a8r8g8b8,	IcStore_a8r8g8b8 },
    { PICT_x8r8g8b8,	IcFetch_x8r8g8b8,	IcFetch_x8r8g8b8,	IcStore_x8r8g8b8 },
    { PICT_a8b8g8r8,	IcFetch_a8b8g8r8,	IcFetch_a8b8g8r8,	IcStore_a8b8g8r8 },
    { PICT_x8b8g8r8,	IcFetch_x8b8g8r8,	IcFetch_x8b8g8r8,	IcStore_x8b8g8r8 },

    /* 24bpp formats */
    { PICT_r8g8b8,	IcFetch_r8g8b8,		IcFetch_r8g8b8,		IcStore_r8g8b8 },
    { PICT_b8g8r8,	IcFetch_b8g8r8,		IcFetch_b8g8r8,		IcStore_b8g8r8 },

    /* 16bpp formats */
    { PICT_r5g6b5,	IcFetch_r5g6b5,		IcFetch_r5g6b5,		IcStore_r5g6b5 },
    { PICT_b5g6r5,	IcFetch_b5g6r5,		IcFetch_b5g6r5,		IcStore_b5g6r5 },

    { PICT_a1r5g5b5,	IcFetch_a1r5g5b5,	IcFetch_a1r5g5b5,	IcStore_a1r5g5b5 },
    { PICT_x1r5g5b5,	IcFetch_x1r5g5b5,	IcFetch_x1r5g5b5,	IcStore_x1r5g5b5 },
    { PICT_a1b5g5r5,	IcFetch_a1b5g5r5,	IcFetch_a1b5g5r5,	IcStore_a1b5g5r5 },
    { PICT_x1b5g5r5,	IcFetch_x1b5g5r5,	IcFetch_x1b5g5r5,	IcStore_x1b5g5r5 },
    { PICT_a4r4g4b4,	IcFetch_a4r4g4b4,	IcFetch_a4r4g4b4,	IcStore_a4r4g4b4 },
    { PICT_x4r4g4b4,	IcFetch_x4r4g4b4,	IcFetch_x4r4g4b4,	IcStore_x4r4g4b4 },
    { PICT_a4b4g4r4,	IcFetch_a4b4g4r4,	IcFetch_a4b4g4r4,	IcStore_a4b4g4r4 },
    { PICT_x4b4g4r4,	IcFetch_x4b4g4r4,	IcFetch_x4b4g4r4,	IcStore_x4b4g4r4 },

    /* 8bpp formats */
    { PICT_a8,		IcFetch_a8,		IcFetcha_a8,		IcStore_a8 },
    { PICT_r3g3b2,	IcFetch_r3g3b2,		IcFetch_r3g3b2,		IcStore_r3g3b2 },
    { PICT_b2g3r3,	IcFetch_b2g3r3,		IcFetch_b2g3r3,		IcStore_b2g3r3 },
    { PICT_a2r2g2b2,	IcFetch_a2r2g2b2,	IcFetch_a2r2g2b2,	IcStore_a2r2g2b2 },
    { PICT_c8,		IcFetch_c8,		IcFetch_c8,		IcStore_c8 },
    { PICT_g8,		IcFetch_c8,		IcFetch_c8,		IcStore_g8 },

    /* 4bpp formats */
    { PICT_a4,		IcFetch_a4,		IcFetcha_a4,		IcStore_a4 },
    { PICT_r1g2b1,	IcFetch_r1g2b1,		IcFetch_r1g2b1,		IcStore_r1g2b1 },
    { PICT_b1g2r1,	IcFetch_b1g2r1,		IcFetch_b1g2r1,		IcStore_b1g2r1 },
    { PICT_a1r1g1b1,	IcFetch_a1r1g1b1,	IcFetch_a1r1g1b1,	IcStore_a1r1g1b1 },
    { PICT_a1b1g1r1,	IcFetch_a1b1g1r1,	IcFetch_a1b1g1r1,	IcStore_a1b1g1r1 },
    { PICT_c4,		IcFetch_c4,		IcFetch_c4,		IcStore_c4 },
    { PICT_g4,		IcFetch_c4,		IcFetch_c4,		IcStore_g4 },

    /* 1bpp formats */
    { PICT_a1,		IcFetch_a1,		IcFetcha_a1,		IcStore_a1 },
    { PICT_g1,		IcFetch_g1,		IcFetch_g1,		IcStore_g1 },
};
#define NumAccessMap (sizeof icAccessMap / sizeof icAccessMap[0])

static void
IcStepOver (IcCompositeOperand *op)
{
    op->u.drawable.offset += op->u.drawable.bpp;
}

static void
IcStepDown (IcCompositeOperand *op)
{
    op->u.drawable.line += op->u.drawable.stride;
    op->u.drawable.offset = op->u.drawable.start_offset;
}

static void
IcSet (IcCompositeOperand *op, int x, int y)
{
    op->u.drawable.line = op->u.drawable.top_line + y * op->u.drawable.stride;
    op->u.drawable.offset = op->u.drawable.left_offset + x * op->u.drawable.bpp;
}

static void
IcStepOver_external (IcCompositeOperand *op)
{
    (*op[1].over) (&op[1]);
    (*op[2].over) (&op[2]);
}

static void
IcStepDown_external (IcCompositeOperand *op)
{
    (*op[1].down) (&op[1]);
    (*op[2].down) (&op[2]);
}

static void
IcSet_external (IcCompositeOperand *op, int x, int y)
{
    (*op[1].set) (&op[1], x, y);
    (*op[2].set) (&op[2], 
		  x - op->u.external.alpha_dx,
		  y - op->u.external.alpha_dy);
}

static void
IcStepOver_transform (IcCompositeOperand *op)
{
    op->u.transform.x++;   
}

static void
IcStepDown_transform (IcCompositeOperand *op)
{
    op->u.transform.y++;
    op->u.transform.x = op->u.transform.start_x;
}

static void
IcSet_transform (IcCompositeOperand *op, int x, int y)
{
    op->u.transform.x = x - op->u.transform.left_x;
    op->u.transform.y = y - op->u.transform.top_y;
}


Bool
IcBuildCompositeOperand (IcImage	    *image,
			 IcCompositeOperand op[4],
			 INT16		    x,
			 INT16		    y,
			 Bool		    transform,
			 Bool		    alpha)
{
    /* Check for transform */
    if (transform && image->transform)
    {
	if (!IcBuildCompositeOperand (image, &op[1], 0, 0, FALSE, alpha))
	    return FALSE;
	
	op->u.transform.top_y = image->pixels->y;
	op->u.transform.left_x = image->pixels->x;
	
	op->u.transform.start_x = x - op->u.transform.left_x;
	op->u.transform.x = op->u.transform.start_x;
	op->u.transform.y = y - op->u.transform.top_y;
	op->u.transform.transform = image->transform;
	op->u.transform.filter = image->filter;
	
	op->fetch = IcFetch_transform;
	op->fetcha = IcFetcha_transform;
	op->store = 0;
	op->over = IcStepOver_transform;
	op->down = IcStepDown_transform;
	op->set = IcSet_transform;
        op->indexed = (IcIndexedPtr) image->image_format->index.devPrivate;
	op->clip = op[1].clip;
	
	return TRUE;
    }
    /* Check for external alpha */
    else if (alpha && image->alphaMap)
    {
	if (!IcBuildCompositeOperand (image, &op[1], x, y, FALSE, FALSE))
	    return FALSE;
	if (!IcBuildCompositeOperand (image->alphaMap, &op[2],
				      x - image->alphaOrigin.x,
				      y - image->alphaOrigin.y,
				      FALSE, FALSE))
	    return FALSE;
	op->u.external.alpha_dx = image->alphaOrigin.x;
	op->u.external.alpha_dy = image->alphaOrigin.y;

	op->fetch = IcFetch_external;
	op->fetcha = IcFetcha_external;
	op->store = IcStore_external;
	op->over = IcStepOver_external;
	op->down = IcStepDown_external;
	op->set = IcSet_external;
        op->indexed = (IcIndexedPtr) image->image_format->index.devPrivate;
	op->clip = op[1].clip;
	
	return TRUE;
    }
    /* Build simple operand */
    else
    {
	int	    i;
	int	    xoff, yoff;

	for (i = 0; i < NumAccessMap; i++)
	    if (icAccessMap[i].format == image->format)
	    {
		IcBits	*bits;
		IcStride	stride;
		int		bpp;

		op->fetch = icAccessMap[i].fetch;
		op->fetcha = icAccessMap[i].fetcha;
		op->store = icAccessMap[i].store;
		op->over = IcStepOver;
		op->down = IcStepDown;
		op->set = IcSet;
		op->indexed = (IcIndexedPtr) image->image_format->index.devPrivate;
		op->clip = image->pCompositeClip;

		IcGetPixels (image->pixels, bits, stride, bpp,
			     xoff, yoff);
		if (image->repeat && image->pixels->width == 1 && 
		    image->pixels->height == 1)
		{
		    bpp = 0;
		    stride = 0;
		}
		/*
		 * Coordinates of upper left corner of drawable
		 */
		op->u.drawable.top_line = bits + yoff * stride;
		op->u.drawable.left_offset = xoff * bpp;

		/*
		 * Starting position within drawable
		 */
		op->u.drawable.start_offset = op->u.drawable.left_offset + x * bpp;
		op->u.drawable.line = op->u.drawable.top_line + y * stride;
		op->u.drawable.offset = op->u.drawable.start_offset;

		op->u.drawable.stride = stride;
		op->u.drawable.bpp = bpp;
		return TRUE;
	    }
	return FALSE;
    }
}

void
IcCompositeGeneral (CARD8	op,
		    IcImage	*iSrc,
		    IcImage	*iMask,
		    IcImage	*iDst,
		    INT16	xSrc,
		    INT16	ySrc,
		    INT16	xMask,
		    INT16	yMask,
		    INT16	xDst,
		    INT16	yDst,
		    CARD16	width,
		    CARD16	height)
{
    IcCompositeOperand	src[4],msk[4],dst[4],*pmsk;
    IcCompositeOperand	*srcPict, *srcAlpha;
    IcCompositeOperand	*dstPict, *dstAlpha;
    IcCompositeOperand	*mskPict = 0, *mskAlpha = 0;
    IcCombineFunc	f;
    int			w;

    if (!IcBuildCompositeOperand (iSrc, src, xSrc, ySrc, TRUE, TRUE))
	return;
    if (!IcBuildCompositeOperand (iDst, dst, xDst, yDst, FALSE, TRUE))
	return;
    if (iSrc->alphaMap)
    {
	srcPict = &src[1];
	srcAlpha = &src[2];
    }
    else
    {
	srcPict = &src[0];
	srcAlpha = 0;
    }
    if (iDst->alphaMap)
    {
	dstPict = &dst[1];
	dstAlpha = &dst[2];
    }
    else
    {
	dstPict = &dst[0];
	dstAlpha = 0;
    }
    f = IcCombineFuncU[op];
    if (iMask)
    {
	if (!IcBuildCompositeOperand (iMask, msk, xMask, yMask, TRUE, TRUE))
	    return;
	pmsk = msk;
	if (iMask->componentAlpha)
	    f = IcCombineFuncC[op];
	if (iMask->alphaMap)
	{
	    mskPict = &msk[1];
	    mskAlpha = &msk[2];
	}
	else
	{
	    mskPict = &msk[0];
	    mskAlpha = 0;
	}
    }
    else
	pmsk = 0;
    while (height--)
    {
	w = width;
	
	while (w--)
	{
	    (*f) (src, pmsk, dst);
	    (*src->over) (src);
	    (*dst->over) (dst);
	    if (pmsk)
		(*pmsk->over) (pmsk);
	}
	(*src->down) (src);
	(*dst->down) (dst);
	if (pmsk)
	    (*pmsk->down) (pmsk);
    }
}

