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

#include <stdarg.h>
#include <stdio.h>
#include "cairoint.h"

struct _cairo_output_stream {
    cairo_write_func_t		write_data;
    cairo_destroy_func_t	destroy_closure;
    void			*closure;
    unsigned long		position;
    cairo_status_t		status;
};

cairo_output_stream_t *
_cairo_output_stream_create (cairo_write_func_t		write_data,
			     cairo_destroy_func_t	destroy_closure,
			     void			*closure)
{
    cairo_output_stream_t *stream;

    stream = malloc (sizeof (cairo_output_stream_t));
    if (stream == NULL)
	return NULL;

    stream->write_data = write_data;
    stream->destroy_closure = destroy_closure;
    stream->closure = closure;
    stream->position = 0;
    stream->status = CAIRO_STATUS_SUCCESS;

    return stream;
}

void
_cairo_output_stream_destroy (cairo_output_stream_t *stream)
{
    stream->destroy_closure (stream->closure);
    free (stream);
}

cairo_status_t
_cairo_output_stream_write (cairo_output_stream_t *stream,
			    const void *data, size_t length)
{
    stream->status = stream->write_data (stream->closure, data, length);
    stream->position += length;

    return stream->status;
}

cairo_status_t
_cairo_output_stream_printf (cairo_output_stream_t *stream,
			     const char *fmt, ...)
{
    unsigned char buffer[512];
    int length;
    va_list ap;

    va_start (ap, fmt);
    length = vsnprintf ((char *)buffer, sizeof buffer, fmt, ap);
    va_end (ap);

    /* FIXME: This function is only for internal use and callers are
     * required to ensure the length of the output fits in this
     * buffer.  If this is not good enough, we have to do the va_copy
     * thing, which requires some autoconf magic to do portably. */
    assert (length < sizeof buffer);
    stream->status = stream->write_data (stream->closure, buffer, length);
    stream->position += length;

    return stream->status;
}

long
_cairo_output_stream_get_position (cairo_output_stream_t *stream)
{
    return stream->position;
}

cairo_status_t
_cairo_output_stream_get_status (cairo_output_stream_t *stream)
{
    return stream->status;
}


/* Maybe this should be a configure time option, so embedded targets
 * don't have to pull in stdio. */

static cairo_status_t
stdio_write (void *closure, const unsigned char *data, unsigned int length)
{
	FILE *fp = closure;

	if (fwrite (data, 1, length, fp) == length)
		return CAIRO_STATUS_SUCCESS;

	return CAIRO_STATUS_WRITE_ERROR;
}

static void
stdio_destroy_closure (void *closure)
{
}

cairo_output_stream_t *
_cairo_output_stream_create_for_file (FILE *fp)
{
    return _cairo_output_stream_create (stdio_write,
					stdio_destroy_closure, fp);
}

