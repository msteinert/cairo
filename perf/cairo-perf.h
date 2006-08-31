/*
 * Copyright © 2006 Red Hat, Inc.
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

#ifndef _CAIRO_PERF_H_
#define _CAIRO_PERF_H_

#include "cairo-boilerplate.h"

extern unsigned int iterations;

typedef void (*cairo_perf_func_t) (cairo_t *cr, int width, int height);

#define DECL_PERF_FUNC(func) void func (cairo_t *cr, int width, int height)

DECL_PERF_FUNC (paint_setup);
DECL_PERF_FUNC (paint_alpha_setup);
DECL_PERF_FUNC (paint);

/* XXX: Obviously bogus as we bring up the infrastructure. */
#define PERF_LOOP_INIT do {
#define PERF_LOOP_FINI } while (0);

#endif
