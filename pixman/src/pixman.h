#ifndef _PIXMAN_H_
#define _PIXMAN_H_


/* pixman.h - a merge of pixregion.h and ic.h */


/* from pixregion.h */


/***********************************************************

Copyright 1987, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.


Copyright 1987 by Digital Equipment Corporation, Maynard, Massachusetts.

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its 
documentation for any purpose and without fee is hereby granted, 
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in 
supporting documentation, and that the name of Digital not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.  

DIGITAL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

******************************************************************/
/* $Id: pixman.h,v 1.10 2003-12-12 18:47:59 cworth Exp $ */

/* libic.h */

/*
 * Copyright © 1998 Keith Packard
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


#if defined (__SVR4) && defined (__sun)
# include <sys/int_types.h>
#else
# if defined (__OpenBSD__)
#  include <inttypes.h>
# else 
#  include <stdint.h>
# endif
#endif


/* From slim_export.h and slim_import.h */
#if defined(WIN32) || defined(__CYGWIN__)
# if defined(_PIXREGIONINT_H_) || defined(_ICINT_H_)
#  define __external_linkage	__declspec(dllexport)
# else
#  define __external_linkage	__declspec(dllimport)
# endif
#else
# define __external_linkage
#endif

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

/* pixregion.h */

typedef struct _pixman_region16_t pixman_region16_t;

typedef struct _pixman_box16_t {
    short x1, y1, x2, y2;
} pixman_box16_t;

typedef enum {
    PIXMAN_REGION_STATUS_FAILURE,
    PIXMAN_REGION_STATUS_SUCCESS
} pixman_region_status_t;

/* creation/destruction */

extern pixman_region16_t * __external_linkage
pixman_region_create (void);

extern pixman_region16_t * __external_linkage
pixman_region_create_simple (pixman_box16_t *extents);

extern void __external_linkage
pixman_region_destroy (pixman_region16_t *region);

/* manipulation */

extern void __external_linkage
pixman_region_translate (pixman_region16_t *region, int x, int y);

extern pixman_region_status_t __external_linkage
pixman_region_copy (pixman_region16_t *dest, pixman_region16_t *source);

extern pixman_region_status_t __external_linkage
pixman_region_intersect (pixman_region16_t *newReg, pixman_region16_t *reg1, pixman_region16_t *reg2);

extern pixman_region_status_t __external_linkage
pixman_region_union (pixman_region16_t *newReg, pixman_region16_t *reg1, pixman_region16_t *reg2);

extern pixman_region_status_t __external_linkage
pixman_region_union_rect(pixman_region16_t *dest, pixman_region16_t *source,
		   int x, int y, unsigned int width, unsigned int height);

extern pixman_region_status_t __external_linkage
pixman_region_subtract (pixman_region16_t *regD, pixman_region16_t *regM, pixman_region16_t *regS);

extern pixman_region_status_t __external_linkage
pixman_region_inverse (pixman_region16_t *newReg, pixman_region16_t *reg1, pixman_box16_t *invRect);

/* XXX: Need to fix this so it doesn't depend on an X data structure
extern pixman_region16_t * __external_linkage
RectsTopixman_region16_t (int nrects, xRectanglePtr prect, int ctype);
*/

/* querying */

/* XXX: These should proably be combined: pixman_region16_tGetRects? */
extern int __external_linkage
pixman_region_num_rects (pixman_region16_t *region);

extern pixman_box16_t * __external_linkage
pixman_region_rects (pixman_region16_t *region);

/* XXX: Change to an enum */
#define rgnOUT 0
#define rgnIN  1
#define rgnPART 2

extern int __external_linkage
pixman_region_contains_point (pixman_region16_t *region, int x, int y, pixman_box16_t *box);

extern int __external_linkage
pixman_region_contains_rectangle (pixman_region16_t *pixman_region16_t, pixman_box16_t *prect);

extern int __external_linkage
pixman_region_not_empty (pixman_region16_t *region);

extern pixman_box16_t * __external_linkage
pixman_region_extents (pixman_region16_t *region);

/* mucking around */

/* WARNING: calling pixman_region_append may leave dest as an invalid
   region. Follow-up with pixman_region_validate to fix it up. */
extern pixman_region_status_t __external_linkage
pixman_region_append (pixman_region16_t *dest, pixman_region16_t *region);

extern pixman_region_status_t __external_linkage
pixman_region_validate (pixman_region16_t *badreg, int *pOverlap);

/* Unclassified functionality
 * XXX: Do all of these need to be exported?
 */

extern void __external_linkage
pixman_region_reset (pixman_region16_t *region, pixman_box16_t *pBox);

extern void __external_linkage
pixman_region_empty (pixman_region16_t *region);


/* ic.h */


/* icformat.c */
typedef enum _pixman_operator_t {
    PIXMAN_OPERATOR_CLEAR,
    PIXMAN_OPERATOR_SRC,
    PIXMAN_OPERATOR_DST,
    PIXMAN_OPERATOR_OVER,
    PIXMAN_OPERATOR_OVER_REVERSE,
    PIXMAN_OPERATOR_IN,
    PIXMAN_OPERATOR_IN_REVERSE,
    PIXMAN_OPERATOR_OUT,
    PIXMAN_OPERATOR_OUT_REVERSE,
    PIXMAN_OPERATOR_ATOP,
    PIXMAN_OPERATOR_ATOP_REVERSE,
    PIXMAN_OPERATOR_XOR,
    PIXMAN_OPERATOR_ADD,
    PIXMAN_OPERATOR_SATURATE,
} pixman_operator_t;


typedef enum _pixman_format_tName {
    PIXMAN_FORMAT_NAME_AR_GB32,
    PIXMAN_FORMAT_NAME_RG_B24,
    PIXMAN_FORMAT_NAME_A8,
    PIXMAN_FORMAT_NAME_A1
} pixman_format_tName;

typedef struct _pixman_format_t pixman_format_t;

extern pixman_format_t * __external_linkage
pixman_format_create (pixman_format_tName name);

extern pixman_format_t * __external_linkage
pixman_format_create_masks (int bpp,
		     int alpha_mask,
		     int red_mask,
		     int green_mask,
		     int blue_mask);

extern void __external_linkage
pixman_format_destroy (pixman_format_t *format);

/* icimage.c */

typedef struct _pixman_image_t	pixman_image_t;

extern pixman_image_t * __external_linkage
pixman_image_create (pixman_format_t	*format,
	       int	width,
	       int	height);

/*
 * This single define controls the basic size of data manipulated
 * by this software; it must be log2(sizeof (pixman_bits_t) * 8)
 */

#ifndef IC_SHIFT
#  if defined(__alpha__) || defined(__alpha) || \
      defined(ia64) || defined(__ia64__) || \
      defined(__sparc64__) || \
      defined(__s390x__) || \
      defined(x86_64) || defined (__x86_64__)
#define IC_SHIFT 6
typedef uint64_t pixman_bits_t;
#  else
#define IC_SHIFT 5
typedef uint32_t pixman_bits_t;
#  endif
#endif

extern pixman_image_t * __external_linkage
pixman_image_create_for_data (pixman_bits_t *data, pixman_format_t *format, int width, int height, int bpp, int stride);

extern void __external_linkage
pixman_image_destroy (pixman_image_t *image);

extern int __external_linkage
pixman_image_set_clip_region (pixman_image_t	*image,
		      pixman_region16_t	*region);

typedef int pixman_fixed16_16_t;

typedef struct _pixman_point_fixed_t {
    pixman_fixed16_16_t  x, y;
} pixman_point_fixed_t;

typedef struct _pixman_line_fixed_t {
    pixman_point_fixed_t	p1, p2;
} pixman_line_fixed_t;

/* XXX: It's goofy that pixman_rectangle_t has integers while all the other
   datatypes have fixed-point values. (Though by design,
   pixman_fill_rectangles is designed to fill only whole pixels) */
typedef struct _pixman_rectangle_t {
    short x, y;
    unsigned short width, height;
} pixman_rectangle_t;

typedef struct _pixman_triangle_t {
    pixman_point_fixed_t	p1, p2, p3;
} pixman_triangle_t;

typedef struct _pixman_trapezoid_t {
    pixman_fixed16_16_t  top, bottom;
    pixman_line_fixed_t	left, right;
} pixman_trapezoid_t;

typedef struct _pixman_vector_t {
    pixman_fixed16_16_t    vector[3];
} pixman_vector_t;

typedef struct _pixman_transform_t {
    pixman_fixed16_16_t  matrix[3][3];
} pixman_transform_t;

typedef enum {
    PIXMAN_FILTER_FAST,
    PIXMAN_FILTER_GOOD,
    PIXMAN_FILTER_BEST,
    PIXMAN_FILTER_NEAREST,
    PIXMAN_FILTER_BILINEAR
} pixman_filter_t;

extern int __external_linkage
pixman_image_set_transform (pixman_image_t		*image,
		     pixman_transform_t	*transform);

extern void __external_linkage
pixman_image_set_repeat (pixman_image_t	*image,
		  int		repeat);

extern void __external_linkage
pixman_image_set_filter (pixman_image_t	*image,
		  pixman_filter_t	filter);

extern int __external_linkage
pixman_image_get_width (pixman_image_t	*image);

extern int __external_linkage
pixman_image_get_height (pixman_image_t	*image);

extern int __external_linkage
pixman_image_get_stride (pixman_image_t	*image);

extern int __external_linkage
pixman_image_get_depth (pixman_image_t	*image);

extern pixman_format_t * __external_linkage
pixman_image_get_format (pixman_image_t	*image);

extern pixman_bits_t * __external_linkage
pixman_image_get_data (pixman_image_t	*image);

/* iccolor.c */

/* XXX: Do we really need a struct here? Only pixman_rectangle_ts uses this. */
typedef struct {
    unsigned short   red;
    unsigned short   green;
    unsigned short   blue;
    unsigned short   alpha;
} pixman_color_t;

extern void __external_linkage
pixman_color_to_pixel (const pixman_format_t	*format,
		const pixman_color_t	*color,
		pixman_bits_t		*pixel);

extern void __external_linkage
pixman_pixel_to_color (const pixman_format_t	*format,
		pixman_bits_t		pixel,
		pixman_color_t		*color);

/* icrect.c */

extern void __external_linkage
pixman_fill_rectangle (pixman_operator_t	op,
		 pixman_image_t	*dst,
		 const pixman_color_t	*color,
		 int		x,
		 int		y,
		 unsigned int	width,
		 unsigned int	height);

extern void __external_linkage
pixman_fill_rectangles (pixman_operator_t		op,
		  pixman_image_t		*dst,
		  const pixman_color_t		*color,
		  const pixman_rectangle_t	*rects,
		  int			nRects);

/* ictrap.c */

/* XXX: Switch to enum for op */
extern void __external_linkage
pixman_composite_trapezoids (pixman_operator_t	op,
		       pixman_image_t		*src,
		       pixman_image_t		*dst,
		       int		xSrc,
		       int		ySrc,
		       const pixman_trapezoid_t *traps,
		       int		ntrap);

/* ictri.c */

extern void __external_linkage
pixman_composite_triangles (pixman_operator_t	op,
		      pixman_image_t		*src,
		      pixman_image_t		*dst,
		      int		xSrc,
		      int		ySrc,
		      const pixman_triangle_t	*tris,
		      int		ntris);

extern void __external_linkage
pixman_composite_tri_strip (pixman_operator_t		op,
		     pixman_image_t		*src,
		     pixman_image_t		*dst,
		     int		xSrc,
		     int		ySrc,
		     const pixman_point_fixed_t	*points,
		     int		npoints);


extern void __external_linkage
pixman_composite_tri_fan (pixman_operator_t		op,
		   pixman_image_t		*src,
		   pixman_image_t		*dst,
		   int			xSrc,
		   int			ySrc,
		   const pixman_point_fixed_t	*points,
		   int			npoints);

/* ic.c */

extern void __external_linkage
pixman_composite (pixman_operator_t	op,
	     pixman_image_t	*iSrc,
	     pixman_image_t    *iMask,
	     pixman_image_t    *iDst,
	     int      	xSrc,
	     int      	ySrc,
	     int      	xMask,
	     int      	yMask,
	     int      	xDst,
	     int      	yDst,
	     int	width,
	     int	height);



#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#undef __external_linkage

#endif /* _PIXMAN_H_ */
