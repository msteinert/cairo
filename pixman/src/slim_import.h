/* slim - Shared Library Interface Macros
 *
 * Copyright © 2003 Richard Henderson
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of Richard Henderson
 * not be used in advertising or publicity pertaining to distribution
 * of the software without specific, written prior permission.
 * Richard Henderson makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 * 
 * RICHARD HENDERSON DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN
 * NO EVENT SHALL RICHARD HENDERSON BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author: Richard Henderson <rth@twiddle.net>
 */

/* This file is included multiple times.  Don't ifdef protect it.  */

/* This macro marks a symbol as being imported from an external library.
   This is essential for data symbols as otherwise the compiler can't
   generate the address properly.  It is advised for functions, as the
   compiler can generate the indirect call inline.

   The macro should be placed either immediately before a function name,

	extern int __external_linkage
	somefunction(void);

   or after a data name,

	extern int somedata __external_linkage;

   This header should be included after all other headers at the beginning
   of a package's external header, and __external_linkage should be #undef'ed
   at the end.  */
/* ??? Not marked with "slim" because that makes it look too much
   like the function name instead of just an attribute.  */

#if defined(WIN32) || defined(__CYGWIN__)
#define __external_linkage	__declspec(dllimport)
#else
#define __external_linkage
#endif
