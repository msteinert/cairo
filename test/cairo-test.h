/*
 * Copyright © 2004 Red Hat, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of
 * Red Hat, Inc. not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission. Red Hat, Inc. makes no representations about the
 * suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * RED HAT, INC. DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL RED HAT, INC. BE LIABLE FOR ANY SPECIAL,
 * INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
 * IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author: Carl D. Worth <cworth@cworth.org>
 */

#ifndef _CAIRO_TEST_H_
#define _CAIRO_TEST_H_

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <math.h>
#include <cairo.h>
#include <cairo-debug.h>

#if   HAVE_STDINT_H
# include <stdint.h>
#elif HAVE_INTTYPES_H
# include <inttypes.h>
#elif HAVE_SYS_INT_TYPES_H
# include <sys/int_types.h>
#elif defined(_MSC_VER)
typedef __int8 int8_t;
typedef unsigned __int8 uint8_t;
typedef __int16 int16_t;
typedef unsigned __int16 uint16_t;
typedef __int32 int32_t;
typedef unsigned __int32 uint32_t;
typedef __int64 int64_t;
typedef unsigned __int64 uint64_t;
# ifndef HAVE_UINT64_T
#  define HAVE_UINT64_T 1
# endif
#else
#error Cannot find definitions for fixed-width integral types (uint8_t, uint32_t, \etc.)
#endif

typedef enum cairo_test_status {
    CAIRO_TEST_SUCCESS = 0,
    CAIRO_TEST_FAILURE,
    CAIRO_TEST_UNTESTED
} cairo_test_status_t;

typedef struct cairo_test {
    char *name;
    char *description;
    int width;
    int height;
} cairo_test_t;

typedef cairo_test_status_t  (*cairo_test_draw_function_t) (cairo_t *cr, int width, int height);

/* cairo_test.c */
cairo_test_status_t
cairo_test (cairo_test_t *test, cairo_test_draw_function_t draw);

cairo_test_status_t
cairo_test_expect_failure (cairo_test_t		      *test, 
			   cairo_test_draw_function_t  draw,
			   const char		      *reason);

cairo_surface_t *
cairo_test_create_surface_from_png (const char *filename);

cairo_pattern_t *
cairo_test_create_pattern_from_png (const char *filename);

void
cairo_test_log (const char *fmt, ...);

void
xasprintf (char **strp, const char *fmt, ...);

#endif
