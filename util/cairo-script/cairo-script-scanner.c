/*
 * Copyright © 2008 Chris Wilson <chris@chris-wilson.co.uk>
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
 * The Original Code is the cairo graphics library.
 *
 * The Initial Developer of the Original Code is Chris Wilson.
 *
 * Contributor(s):
 *	Chris Wilson <chris@chris-wilson.co.uk>
 */

#include "cairo-script-private.h"

#include <limits.h> /* INT_MAX */
#include <math.h> /* pow */
#include <stdio.h> /* EOF */
#include <string.h> /* memset */

/*
 * whitespace:
 * 0 - nul
 * 9 - tab
 * A - LF
 * C - FF
 * D - CR
 *
 * syntax delimiters
 * ( = 28, ) = 29 - literal strings
 * < = 3C, > = 3E - hex/base85 strings, dictionary name
 * [ = 5B, ] = 5D - array
 * { = 7B, } = 7C - procedure
 * / = 5C - literal marker
 * % = 25 - comment
 */

static cairo_status_t
buffer_init (csi_t *ctx, csi_buffer_t *buffer)
{
    cairo_status_t status = CSI_STATUS_SUCCESS;

    buffer->size = 16384;
    buffer->base = _csi_alloc (ctx, buffer->size);
    if (_csi_unlikely (buffer->base == NULL)) {
	status = _csi_error (CSI_STATUS_NO_MEMORY);
	buffer->size = 0;
    }

    buffer->ptr = buffer->base;
    buffer->end = buffer->base + buffer->size;

    return status;
}

static void
buffer_fini (csi_t *ctx, csi_buffer_t *buffer)
{
    _csi_free (ctx, buffer->base);
}

static inline void
_buffer_grow (csi_t *ctx, csi_scanner_t *scan)
{
    int newsize;
    int offset;
    char *base;

    if (_csi_unlikely (scan->buffer.size > INT_MAX / 2))
	longjmp (scan->jmpbuf,  _csi_error (CSI_STATUS_NO_MEMORY));

    offset = scan->buffer.ptr - scan->buffer.base;
    newsize = scan->buffer.size * 2;
    base = _csi_realloc (ctx, scan->buffer.base, newsize);
    if (_csi_unlikely (base == NULL))
	longjmp (scan->jmpbuf,  _csi_error (CSI_STATUS_NO_MEMORY));

    scan->buffer.base = base;
    scan->buffer.ptr  = base + offset;
    scan->buffer.end  = base + newsize;
    scan->buffer.size = newsize;
}

static inline void
buffer_check (csi_t *ctx, csi_scanner_t *scan, int count)
{
    if (_csi_unlikely (scan->buffer.ptr + count > scan->buffer.end))
	_buffer_grow (ctx, scan);
}

static inline void
buffer_add (csi_buffer_t *buffer, int c)
{
    *buffer->ptr++ = c;
}

static inline void
buffer_reset (csi_buffer_t *buffer)
{
    buffer->ptr = buffer->base;
}

static inline void
reset (csi_scanner_t *scan)
{
    scan->state = NONE;
}

static void
token_start (csi_scanner_t *scan)
{
    scan->state = TOKEN;
    buffer_reset (&scan->buffer);
}

static void
token_add (csi_t *ctx, csi_scanner_t *scan, int c)
{
    buffer_check (ctx, scan, 1);
    buffer_add (&scan->buffer, c);
}

static void
token_add_unchecked (csi_scanner_t *scan, int c)
{
    buffer_add (&scan->buffer, c);
}

static csi_boolean_t
parse_number (csi_object_t *obj, const char *s, int len)
{
    int radix = 0;
    long long mantissa = 0;
    int exponent = 0;
    int sign = 1;
    int decimal = -1;
    int exponent_sign = 0;
    const char * const end = s + len;

    switch (*s) {
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
	mantissa = *s - '0';
    case '+':
	break;
    case '-':
	sign = -1;
	break;
    case '.':
	decimal = 0;
	break;
    default:
	return FALSE;
    }

    while (++s < end) {
	if (*s < '0') {
	    if (*s == '.') {
		if (_csi_unlikely (radix))
		    return FALSE;
		if (_csi_unlikely (decimal != -1))
		    return FALSE;
		if (_csi_unlikely (exponent_sign))
		    return FALSE;

		decimal = 0;
	    } else if (*s == '!') {
		if (_csi_unlikely (radix))
		    return FALSE;
		if (_csi_unlikely (decimal != -1))
		    return FALSE;
		if (_csi_unlikely (exponent_sign))
		    return FALSE;

		radix = mantissa;
		mantissa = 0;

		if (_csi_unlikely (radix < 2 || radix > 36))
		    return FALSE;
	    } else
		return FALSE;
	} else if (*s <= '9') {
	    int v = *s - '0';
	    if (_csi_unlikely (radix && v >= radix))
		return FALSE;

	    if (exponent_sign) {
		exponent = 10 * exponent + v;
	    } else {
		if (radix)
		    mantissa = radix * mantissa + v;
		else
		    mantissa = 10 * mantissa + v;
		if (decimal != -1)
		    decimal++;
	    }
	} else if (*s == 'E' || * s== 'e') {
	    if (radix == 0) {
		if (_csi_unlikely (s + 1 == end))
		    return FALSE;

		exponent_sign = 1;
		if (s[1] == '-') {
		    exponent_sign = -1;
		    s++;
		} else if (s[1] == '+')
		    s++;
	    } else {
		int v = 0xe;

		if (_csi_unlikely (v >= radix))
		    return FALSE;

		mantissa = radix * mantissa + v;
	    }
	} else if (*s < 'A') {
	    return FALSE;
	} else if (*s <= 'Z') {
	    int v = *s - 'A' + 0xA;

	    if (_csi_unlikely (v >= radix))
		return FALSE;

	    mantissa = radix * mantissa + v;
	} else if (*s < 'a') {
	    return FALSE;
	} else if (*s <= 'z') {
	    int v = *s - 'a' + 0xa;

	    if (_csi_unlikely (v >= radix))
		return FALSE;

	    mantissa = radix * mantissa + v;
	} else
	    return FALSE;
    }

    if (exponent_sign || decimal != -1) {
	if (mantissa == 0) {
	    obj->type = CSI_OBJECT_TYPE_REAL;
	    obj->datum.real = 0.;
	    return TRUE;
	} else {
	    int e;
	    double v;

	    v = mantissa;
	    e = exponent * exponent_sign;
	    if (decimal != -1)
		e -= decimal;
	    switch (e) {
	    case -7: v *= 0.0000001; break;
	    case -6: v *= 0.000001; break;
	    case -5: v *= 0.00001; break;
	    case -4: v *= 0.0001; break;
	    case -3: v *= 0.001; break;
	    case -2: v *= 0.01; break;
	    case -1: v *= 0.1; break;
	    case  0: break;
	    case  1: v *= 10; break;
	    case  2: v *= 100; break;
	    case  3: v *= 1000; break;
	    case  4: v *= 10000; break;
	    case  5: v *= 100000; break;
	    case  6: v *= 1000000; break;
	    default:
		    v *= pow (10, e); /* XXX */
		    break;
	    }

	    obj->type = CSI_OBJECT_TYPE_REAL;
	    obj->datum.real = sign * v;
	    return TRUE;
	}
    } else {
	obj->type = CSI_OBJECT_TYPE_INTEGER;
	obj->datum.integer = sign * mantissa;
	return TRUE;
    }
}

static void
token_end (csi_t *ctx, csi_scanner_t *scan, csi_file_t *src)
{
    cairo_status_t status;
    char *s;
    csi_object_t obj;
    int len;

    /*
     * Any token that consists entirely of regular characters and
     * cannot be interpreted as a number is treated as a name object
     * (more precisely, an executable name). All characters except
     * delimiters and white-space characters can appear in names,
     * including characters ordinarily considered to be punctuation.
     */

    if (_csi_unlikely (scan->buffer.ptr == scan->buffer.base))
	return;

    s = scan->buffer.base;
    len = scan->buffer.ptr - scan->buffer.base;

    if (s[0] == '{') { /* special case procedures */
	if (scan->build_procedure.type != CSI_OBJECT_TYPE_NULL) {
	    status = _csi_stack_push (ctx,
				      &scan->procedure_stack,
				      &scan->build_procedure);
	    if (_csi_unlikely (status))
		longjmp (scan->jmpbuf, status);
	}

	status = csi_array_new (ctx, 0, &scan->build_procedure);
	if (_csi_unlikely (status))
	    longjmp (scan->jmpbuf, status);

	scan->build_procedure.type |= CSI_OBJECT_ATTR_EXECUTABLE;
	reset (scan);
	return;
    } else if (s[0] == '}') {
	if (_csi_unlikely
	    (scan->build_procedure.type == CSI_OBJECT_TYPE_NULL))
	{
	    longjmp (scan->jmpbuf, _csi_error (CSI_STATUS_INVALID_SCRIPT));
	}

	if (scan->procedure_stack.len) {
	    csi_object_t *next;

	    next = _csi_stack_peek (&scan->procedure_stack, 0);
	    status = csi_array_append (ctx, next->datum.array,
				       &scan->build_procedure);
	    scan->build_procedure = *next;
	    scan->procedure_stack.len--;
	} else {
	    status = _csi_push_ostack (ctx, &scan->build_procedure);
	    scan->build_procedure.type = CSI_OBJECT_TYPE_NULL;
	}
	if (_csi_unlikely (status))
	    longjmp (scan->jmpbuf, status);

	reset (scan);
	return;
    }

    if (s[0] == '/') {
	if (len >= 2 && s[1] == '/') { /* substituted name */
	    status = csi_name_new (ctx, &obj, s + 2, len - 2);
	    if (_csi_unlikely (status))
		longjmp (scan->jmpbuf, status);

	    status = _csi_name_lookup (ctx, obj.datum.name, &obj);
	} else { /* literal name */
	    status = csi_name_new (ctx, &obj, s + 1, len - 1);
	}
	if (_csi_unlikely (status))
	    longjmp (scan->jmpbuf, status);
    } else {
	if (! parse_number (&obj, s, len)) {
	    status = csi_name_new (ctx, &obj, s, len);
	    if (_csi_unlikely (status))
		longjmp (scan->jmpbuf, status);

	    obj.type |= CSI_OBJECT_ATTR_EXECUTABLE;
	}
    }

    /* consume whitespace after token, before calling the interpreter */
    reset (scan);

    if (scan->build_procedure.type != CSI_OBJECT_TYPE_NULL) {
	status = csi_array_append (ctx,
				   scan->build_procedure.datum.array,
				   &obj);
    } else if (obj.type & CSI_OBJECT_ATTR_EXECUTABLE) {
	status = csi_object_execute (ctx, &obj);
    } else {
	status = _csi_push_ostack (ctx, &obj);
    }
    if (_csi_unlikely (status))
	longjmp (scan->jmpbuf, status);
}

static void
comment_start (csi_scanner_t *scan)
{
    /* XXX check for '!' interpreter mode?, '%' dsc setup? */
    scan->state = COMMENT;
}

static void
comment_end (csi_scanner_t *scan)
{
    reset (scan);
}

static void
string_start (csi_scanner_t *scan)
{
    scan->state = STRING;
    scan->string_p = 1;
    buffer_reset (&scan->buffer);
}

static void
string_inc_p (csi_scanner_t *scan)
{
    scan->string_p++;
}

static int
string_dec_p (csi_scanner_t *scan)
{
    return --scan->string_p == 0;
}

static void
string_add (csi_t *ctx, csi_scanner_t *scan, int c)
{
    buffer_check (ctx, scan, 1);
    buffer_add (&scan->buffer, c);
}

static void
string_end (csi_t *ctx, csi_scanner_t *scan)
{
    csi_object_t obj;
    cairo_status_t status;

    status = csi_string_new (ctx,
			     &obj,
			     scan->buffer.base,
			     scan->buffer.ptr - scan->buffer.base);
    if (_csi_unlikely (status))
	longjmp (scan->jmpbuf, status);

    if (scan->build_procedure.type != CSI_OBJECT_TYPE_NULL)
	status = csi_array_append (ctx,
				   scan->build_procedure.datum.array,
				   &obj);
    else
	status = _csi_push_ostack (ctx, &obj);
    if (_csi_unlikely (status))
	longjmp (scan->jmpbuf, status);

    reset (scan);
}

static void
hex_start (csi_scanner_t *scan)
{
    scan->state = HEX;
    scan->accumulator_count = 0;
    scan->accumulator = 0;

    buffer_reset (&scan->buffer);
}

static int
hex_value (int c)
{
    if (c < '0')
	return EOF;
    if (c <= '9')
	return c - '0';
    c |= 32;
    if (c < 'a')
	return EOF;
    if (c <= 'f')
	return c - 'a' + 0xa;
    return EOF;
}

static void
hex_add (csi_t *ctx, csi_scanner_t *scan, int c)
{
    if (scan->accumulator_count == 0) {
	scan->accumulator |= hex_value (c) << 4;
	scan->accumulator_count = 1;
    } else {
	scan->accumulator |= hex_value (c) << 0;
	buffer_check (ctx, scan, 1);
	buffer_add (&scan->buffer, scan->accumulator);

	scan->accumulator = 0;
	scan->accumulator_count = 0;
    }
}

static void
hex_end (csi_t *ctx, csi_scanner_t *scan)
{
    csi_object_t obj;
    cairo_status_t status;

    if (scan->accumulator_count)
	hex_add (ctx, scan, '0');

    status = csi_string_new (ctx,
			     &obj,
			     scan->buffer.base,
			     scan->buffer.ptr - scan->buffer.base);
    if (_csi_unlikely (status))
	longjmp (scan->jmpbuf, status);

    if (scan->build_procedure.type != CSI_OBJECT_TYPE_NULL)
	status = csi_array_append (ctx,
				   scan->build_procedure.datum.array,
				   &obj);
    else
	status = _csi_push_ostack (ctx, &obj);
    if (_csi_unlikely (status))
	longjmp (scan->jmpbuf, status);

    reset (scan);
}

static void
base85_start (csi_scanner_t *scan)
{
    scan->state = BASE85;
    scan->accumulator = 0;
    scan->accumulator_count = 0;

    buffer_reset (&scan->buffer);
}

static void
base85_add (csi_t *ctx, csi_scanner_t *scan, int c)
{
    if (c == 'z') {
	if (_csi_unlikely (scan->accumulator_count != 0))
	    longjmp (scan->jmpbuf, _csi_error (CSI_STATUS_INVALID_SCRIPT));

	buffer_check (ctx, scan, 4);
	buffer_add (&scan->buffer, 0);
	buffer_add (&scan->buffer, 0);
	buffer_add (&scan->buffer, 0);
	buffer_add (&scan->buffer, 0);
    } else if (_csi_unlikely (c < '!' || c > 'u')) {
	longjmp (scan->jmpbuf, _csi_error (CSI_STATUS_INVALID_SCRIPT));
    } else {
	scan->accumulator = scan->accumulator*85 + c - '!';
	if (++scan->accumulator_count == 5) {
	    buffer_check (ctx, scan, 4);
	    buffer_add (&scan->buffer, (scan->accumulator >> 24) & 0xff);
	    buffer_add (&scan->buffer, (scan->accumulator >> 16) & 0xff);
	    buffer_add (&scan->buffer, (scan->accumulator >>  8) & 0xff);
	    buffer_add (&scan->buffer, (scan->accumulator >>  0) & 0xff);

	    scan->accumulator = 0;
	    scan->accumulator_count = 0;
	}
    }
}

static void
base85_end (csi_t *ctx, csi_scanner_t *scan)
{
    csi_object_t obj;
    cairo_status_t status;

    buffer_check (ctx, scan, 4);

    switch (scan->accumulator_count) {
    case 0:
	break;
    case 1:
	longjmp (scan->jmpbuf, _csi_error (CSI_STATUS_INVALID_SCRIPT));
	break;

    case 2:
	scan->accumulator = scan->accumulator * (85*85*85) + 85*85*85 -1;
	buffer_add (&scan->buffer, (scan->accumulator >> 24) & 0xff);
	break;
    case 3:
	scan->accumulator = scan->accumulator * (85*85) + 85*85 -1;
	buffer_add (&scan->buffer, (scan->accumulator >> 24) & 0xff);
	buffer_add (&scan->buffer, (scan->accumulator >> 16) & 0xff);
	break;
    case 4:
	scan->accumulator = scan->accumulator * 85 + 84;
	buffer_add (&scan->buffer, (scan->accumulator >> 24) & 0xff);
	buffer_add (&scan->buffer, (scan->accumulator >> 16) & 0xff);
	buffer_add (&scan->buffer, (scan->accumulator >>  8) & 0xff);
	break;
    }

    status = csi_string_new (ctx,
			     &obj,
			     scan->buffer.base,
			     scan->buffer.ptr - scan->buffer.base);
    if (_csi_unlikely (status))
	longjmp (scan->jmpbuf, status);

    if (scan->build_procedure.type != CSI_OBJECT_TYPE_NULL)
	status = csi_array_append (ctx,
				   scan->build_procedure.datum.array,
				   &obj);
    else
	status = _csi_push_ostack (ctx, &obj);
    if (_csi_unlikely (status))
	longjmp (scan->jmpbuf, status);

    reset (scan);
}

static int
scan_none (csi_t *ctx,
	   csi_scanner_t *scan,
	   csi_file_t *src)
{
    int c, next;
    union {
	int i;
	float f;
    } u;

    while ((c = csi_file_getc (src)) != EOF) {
	csi_object_t obj = { CSI_OBJECT_TYPE_NULL };

	switch (c) {
	case 0xa:
	    scan->line_number++;
	case 0x0:
	case 0x9:
	case 0xc:
	case 0xd:
	case 0x20: /* ignore whitespace */
	    break;

	case '%':
	    comment_start (scan);
	    return 1;

	case '(':
	    string_start (scan);
	    return 1;

	case '[': /* needs special case */
	case ']':
	case '{':
	case '}':
	    token_start (scan);
	    token_add_unchecked (scan, c);
	    token_end (ctx, scan, src);
	    return 1;

	case '<':
	    next = csi_file_getc (src);
	    switch (next) {
	    case EOF:
		csi_file_putc (src, '<');
		return 0;
	    case '<':
		/* dictionary name */
		token_start (scan);
		token_add_unchecked (scan, '<');
		token_add_unchecked (scan, '<');
		token_end (ctx, scan, src);
		return 1;
	    case '~':
		base85_start (scan);
		return 1;
	    default:
		csi_file_putc (src, next);
		hex_start (scan);
		return 1;
	    }
	    break;

	    /* binary token */
	case 128:
	case 129:
	case 130:
	case 131:
	    /* binary object sequence */
	    break;
	case 132: /* 32-bit integer, MSB */
	    break;
	case 133: /* 32-bit integer, LSB */
	    break;
	case 134: /* 16-bit integer, MSB */
	    break;
	case 135: /* 16-bit integer, LSB */
	    break;
	case 136: /* 8-bit integer */
	    break;
	case 137: /* 16/32-bit fixed point */
	    break;
	case 138: /* 32-bit real, MSB */
	    csi_file_read (src, &u.i, 4);
#if ! WORDS_BIGENDIAN
	    u.i = bswap_32 (u.i);
#endif
	    csi_real_new (&obj, u.f);
	    break;
	case 139: /* 32-bit real, LSB */
	    csi_file_read (src, &u.f, 4);
#if WORDS_BIGENDIAN
	    u.i = bswap_32 (u.i);
#endif
	    csi_real_new (&obj, u.f);
	    break;
	case 140: /* 32-bit real, native */
	    csi_file_read (src, &u.f, 4);
	    csi_real_new (&obj, u.f);
	    break;
	case 141: /* boolean */
	    break;
	case 142: /* string of length 1n */
	    break;
	case 143: /* string of length 2n (MSB) */
	    break;
	case 144: /* string of length 2n (LSB) */
	    break;
	case 145: /* literal system name */
	    break;
	case 146: /* executable system name */
	    break;
	case 147: /* reserved */
	    break;
	case 148: /* reserved */
	    break;
	case 149: /* homogeneous array */
	    break;

	    /* unassigned */
	case 150:
	case 151:
	case 152:
	case 153:
	case 154:
	case 155:
	case 156:
	case 157:
	case 158:
	case 159:
	    longjmp (scan->jmpbuf, _csi_error (CSI_STATUS_INVALID_SCRIPT));
	    return 0;

	case '#': /* PDF 1.2 escape code */
	    {
		int c_hi = csi_file_getc (src);
		int c_lo = csi_file_getc (src);
		c = (hex_value (c_hi) << 4) | hex_value (c_lo);
	    }
	    /* fall-through */
	default:
	    token_start (scan);
	    token_add_unchecked (scan, c);
	    return 1;
	}

	if (obj.type != CSI_OBJECT_TYPE_NULL) {
	    cairo_status_t status;

	    if (scan->build_procedure.type != CSI_OBJECT_TYPE_NULL)
		status = csi_array_append (ctx,
					   scan->build_procedure.datum.array,
					   &obj);
	    else
		status = csi_object_execute (ctx, &obj);
	    if (_csi_unlikely (status))
		longjmp (scan->jmpbuf, status);
	}
    }

    return 0;
}

static int
scan_token (csi_t *ctx, csi_scanner_t *scan, csi_file_t *src)
{
    int c;

    while ((c = csi_file_getc (src)) != EOF) {
	switch (c) {
	case 0xa:
	    scan->line_number++;
	case 0x0:
	case 0x9:
	case 0xc:
	case 0xd:
	case 0x20:
	    token_end (ctx, scan, src);
	    return 1;

	    /* syntax delimiters */
	case '%':
	    token_end (ctx, scan, src);
	    comment_start (scan);
	    return 1;
	    /* syntax error? */
	case '(':
	    token_end (ctx, scan, src);
	    string_start (scan);
	    return 1;
	    /* XXX syntax error? */
	case ')':
	    token_end (ctx, scan, src);
	    return 1;
	case '/':
	    /* need to special case '^//?' */
	    if (scan->buffer.ptr > scan->buffer.base+1 ||
		scan->buffer.base[0] != '/')
	    {
		token_end (ctx, scan, src);
		token_start (scan);
	    }
	    token_add_unchecked (scan, '/');
	    return 1;

	case '{':
	case '}':
	case ']':
	    token_end (ctx, scan, src);
	    token_start (scan);
	    token_add_unchecked (scan, c);
	    token_end (ctx, scan, src);
	    return 1;

	case '<':
	    csi_file_putc (src, '<');
	    token_end (ctx, scan, src);
	    return 1;

	case '#': /* PDF 1.2 escape code */
	    {
		int c_hi = csi_file_getc (src);
		int c_lo = csi_file_getc (src);
		c = (hex_value (c_hi) << 4) | hex_value (c_lo);
	    }
	    /* fall-through */
	default:
	    token_add (ctx, scan, c);
	    break;
	}
    }
    token_end (ctx, scan, src);

    return 0;
}

static int
scan_hex (csi_t *ctx, csi_scanner_t *scan, csi_file_t *src)
{
    int c;

    while ((c = csi_file_getc (src)) != EOF) {
	switch (c) {
	case 0xa:
	    scan->line_number++;
	case 0x0:
	case 0x9:
	case 0xc:
	case 0xd:
	case 0x20: /* ignore whitespace */
	    break;

	case '>':
	    hex_end (ctx, scan); /* fixup odd digit with '0' */
	    return 1;

	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
	case 'a':
	case 'b':
	case 'c':
	case 'd':
	case 'e':
	case 'f':
	case 'A':
	case 'B':
	case 'C':
	case 'D':
	case 'E':
	case 'F':
	    hex_add (ctx, scan, c);
	    break;

	default:
	    longjmp (scan->jmpbuf, _csi_error (CSI_STATUS_INVALID_SCRIPT));
	    return 0;
	}
    }

    longjmp (scan->jmpbuf, _csi_error (CSI_STATUS_INVALID_SCRIPT));
    return 0;
}

static int
scan_base85 (csi_t *ctx, csi_scanner_t *scan, csi_file_t *src)
{
    int c, next;

    while ((c = csi_file_getc (src)) != EOF) {
	switch (c) {
	case '~':
	    next = csi_file_getc (src);
	    switch (next) {
	    case EOF:
		return 0;

	    case '>':
		base85_end (ctx, scan);
		return 1;
	    }
	    csi_file_putc (src, next);

	    /* fall-through */
	default:
	    base85_add (ctx, scan, c);
	    break;
	}
    }

    longjmp (scan->jmpbuf, _csi_error (CSI_STATUS_INVALID_SCRIPT));
    return 0;
}

static int
scan_string (csi_t *ctx, csi_scanner_t *scan, csi_file_t *src)
{
    int c, next;

    while ((c = csi_file_getc (src)) != EOF) {
	switch (c) {
	case '\\': /* escape */
	    next = csi_file_getc (src);
	    switch (next) {
	    case EOF:
		longjmp (scan->jmpbuf, _csi_error (CSI_STATUS_INVALID_SCRIPT));
		return 0;

	    case 'n':
		string_add (ctx, scan, '\n');
		break;
	    case 'r':
		string_add (ctx, scan, '\r');
		break;
	    case 't':
		string_add (ctx, scan, '\t');
		break;
	    case 'b':
		string_add (ctx, scan, '\b');
		break;
	    case 'f':
		string_add (ctx, scan, '\f');
		break;
	    case '\\':
		string_add (ctx, scan, '\\');
		break;
	    case '(':
		string_add (ctx, scan, '(');
		break;
	    case ')':
		string_add (ctx, scan, ')');
		break;

	    case '0': case '1': case '2': case '3':
	    case '4': case '5': case '6': case '7':
		{ /* octal code: \d{1,3} */
		    int i;

		    c = next - '0';

		    for (i = 0; i < 2; i++) {
			next = csi_file_getc (src);
			switch (next) {
			case EOF:
			    return 0;

			case '0': case '1': case '2': case '3':
			case '4': case '5': case '6': case '7':
			    c = 8*c + next-'0';
			    break;

			default:
			    csi_file_putc (src, next);
			    goto octal_code_done;
			}
		    }
  octal_code_done:
		    string_add (ctx, scan, c);
		}
		break;

	    case 0xa:
		/* skip the newline */
		next = csi_file_getc (src); /* might be compound LFCR */
		switch (next) {
		case EOF:
		    return 0;
		case 0xc:
		    break;
		default:
		    csi_file_putc (src, next);
		    break;
		}
		scan->line_number++;
		break;
	    case 0xc:
		break;

	    default:
		/* ignore the '\' */
		break;
	    }
	    break;

	case '(':
	    string_inc_p (scan);
	    string_add (ctx, scan, c);
	    break;

	case ')':
	    if (string_dec_p (scan)) {
		string_end (ctx, scan);
		return 1;
	    } else
		string_add (ctx, scan, c);
	    break;

	default:
	    string_add (ctx, scan, c);
	    break;
	}
    }

    longjmp (scan->jmpbuf, _csi_error (CSI_STATUS_INVALID_SCRIPT));
    return 0;
}

static int
scan_comment (csi_t *ctx, csi_scanner_t *scan, csi_file_t *src)
{
    int c;

    /* discard until newline */
    while ((c = csi_file_getc (src)) != EOF) {
	switch (c) {
	case 0xa:
	    scan->line_number++;
	case 0xc:
	    comment_end (scan);
	    return 1;
	}
    }

    return 0;
}

csi_status_t
_csi_scan_file (csi_t *ctx, csi_scanner_t *scan, csi_file_t *src)
{
    static int (* const func[]) (csi_t *, csi_scanner_t *, csi_file_t *) = {
	scan_none,
	scan_token,
	scan_comment,
	scan_string,
	scan_hex,
	scan_base85,
    };
    csi_status_t status;

    /* This function needs to be reentrant to handle recursive scanners.
     * i.e. one script executes a second.
     */

    if (scan->depth++ == 0) {
	if ((status = setjmp (scan->jmpbuf))) {
	    scan->depth = 0;
	    return status;
	}
    }

    scan->line_number = 0; /* XXX broken by recursive scanning */
    while (func[scan->state] (ctx, scan, src))
	;

    --scan->depth;
    return CSI_STATUS_SUCCESS;
}

#if 0
cairo_status_t
_csi_tokenize_string (csi_t *ctx,
		      const char *code, int len,
		      csi_object_t **array_out)
{
    csi_scanner_t scan;
    csi_object_t *src;
    cairo_status_t status;

    status = _csi_scanner_init (&scan, ctx);
    if (status)
	return status;

    scan.build_procedure = csi_array_new (ctx, 0);
    if (scan.build_procedure == NULL)
	goto CLEANUP_SCAN;
    csi_object_set_literal (scan.build_procedure, FALSE);

    src = csi_file_new_for_string (ctx, (const uint8_t *) code, len);
    if (src == NULL) {
	status = _csi_error (CSI_STATUS_NO_MEMORY);
	goto CLEANUP_SCAN;
    }

    status = _csi_scan_object (&scan, src);
    if (status)
	goto CLEANUP_SRC;

    *array_out = scan.build_procedure;
    scan.build_procedure = NULL;

CLEANUP_SRC:
    csi_object_free (src);

CLEANUP_SCAN:
    _csi_scanner_fini (&scan);

    return status;
}
#endif

csi_status_t
_csi_scanner_init (csi_t *ctx, csi_scanner_t *scanner)
{
    csi_status_t status;

    memset (scanner, 0, sizeof (csi_scanner_t));

    status = buffer_init (ctx, &scanner->buffer);
    if (status)
	return status;

    status = _csi_stack_init (ctx, &scanner->procedure_stack, 4);
    if (status)
	return status;

    reset (scanner);

    return CSI_STATUS_SUCCESS;
}

void
_csi_scanner_fini (csi_t *ctx, csi_scanner_t *scanner)
{
    buffer_fini (ctx, &scanner->buffer);
    _csi_stack_fini (ctx, &scanner->procedure_stack);
    if (scanner->build_procedure.type != CSI_OBJECT_TYPE_NULL)
	csi_object_free (ctx, &scanner->build_procedure);
}
