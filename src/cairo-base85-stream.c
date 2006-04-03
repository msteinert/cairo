/* cairo_output_stream.c: Output stream abstraction
 * 
 * Copyright © 2005 Red Hat, Inc
 *
 * This library is free software; you can redistribute it and/or
 * modify it either under the terms of the GNU Lesser General Public
 * License version 2.1 as published by the Free Software Foundation
 * (the "LGPL") or, at your option, under the terms of the Mozilla
 * Public License Version 1.1 (the "MPL"). If you do not alter this
 * notice, a recipient may use your version of this file under either
 * the MPL or the LGPL.
 *
 * You should have received a copy of the LGPL along with this library
 * in the file COPYING-LGPL-2.1; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 * You should have received a copy of the MPL along with this library
 * in the file COPYING-MPL-1.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY
 * OF ANY KIND, either express or implied. See the LGPL or the MPL for
 * the specific language governing rights and limitations.
 *
 * The Original Code is cairo_output_stream.c as distributed with the
 *   cairo graphics library.
 *
 * The Initial Developer of the Original Code is Red Hat, Inc.
 *
 * Author(s):
 *	Kristian Høgsberg <krh@redhat.com>
 */

#include "cairoint.h"

static cairo_bool_t
_convert_four_tuple (const unsigned char *four_tuple, char five_tuple[5])
{
    cairo_bool_t all_zero;
    uint32_t value;
    int digit, i;
    
    value = four_tuple[0] << 24 | four_tuple[1] << 16 | four_tuple[2] << 8 | four_tuple[3];
    all_zero = TRUE;
    for (i = 0; i < 5; i++) {
	digit = value % 85;
	if (digit != 0)
	    all_zero = FALSE;
	five_tuple[4-i] = digit + 33;
	value = value / 85;
    }
    return all_zero;
}

void
_cairo_output_stream_write_base85_string (cairo_output_stream_t *stream,
					  const char *data,
					  size_t length)
{
    unsigned char *ptr;
    unsigned char four_tuple[4];
    char five_tuple[5];
    int column;
    
    ptr = (unsigned char *)data;
    column = 0;
    while (length > 0) {
	if (length >= 4) {
	    if (_convert_four_tuple (ptr, five_tuple)) {
		column += 1;
		_cairo_output_stream_write (stream, "z", 1);
	    } else {
		column += 5;
		_cairo_output_stream_write (stream, five_tuple, 5);
	    }
	    length -= 4;
	    ptr += 4;
	} else { /* length < 4 */
	    memset (four_tuple, 0, 4);
	    memcpy (four_tuple, ptr, length);
	    _convert_four_tuple (four_tuple, five_tuple);
	    column += length + 1;
	    _cairo_output_stream_write (stream, five_tuple, length + 1);
	    length = 0;
	}
	if (column >= 72) {
	    _cairo_output_stream_write (stream, "\n", 1);
	    column = 0;
	}
    }

    if (column > 0) {
	_cairo_output_stream_write (stream, "\n", 1);
    }
}
