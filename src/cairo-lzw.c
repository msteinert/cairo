/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2006 Red Hat, Inc.
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
 * The Initial Developer of the Original Code is University of Southern
 * California.
 *
 * Contributor(s):
 *	Carl D. Worth <cworth@cworth.org>
 */

#include "cairoint.h"

static unsigned long
_cairo_hash_bytes (const unsigned char *c, int size);

typedef struct _lzw_buf {
    unsigned char *data;
    int data_size;
    int num_data;
    uint32_t pending;
    int pending_bits;
} lzw_buf_t;

/* An lzw_buf_t is a simple, growable chunk of memory for holding
 * variable-size objects of up to 16 bits each.
 *
 * Initialize an lzw_buf_t to the given size in bytes.
 *
 * Returns CAIRO_STATUS_SUCCESS or CAIRO_STATUS_NO_MEMORY.
 */
static cairo_status_t
_lzw_buf_init (lzw_buf_t *buf, int size)
{
    if (size == 0)
	size = 16;

    buf->data = malloc (size);
    if (buf->data == NULL) {
	buf->data_size = 0;
	return CAIRO_STATUS_NO_MEMORY;
    }

    buf->data_size = size;
    buf->num_data = 0;
    buf->pending = 0;
    buf->pending_bits = 0;

    return CAIRO_STATUS_SUCCESS;
}

static void
_lzw_buf_fini (lzw_buf_t *buf)
{
    assert (buf->pending_bits == 0);

    free (buf->data);
    buf->data = 0;
}

static cairo_status_t
_lzw_buf_grow (lzw_buf_t *buf)
{
    int new_size = buf->data_size * 2;
    unsigned char *new_data;

    new_data = realloc (buf->data, new_size);
    if (new_data == NULL)
	return CAIRO_STATUS_NO_MEMORY;

    buf->data = new_data;
    buf->data_size = new_size;

    return CAIRO_STATUS_SUCCESS;
}

/* Store the lowest num_bits bits of values into buf.
 *
 * NOTE: The bits of value above size_in_bits must be 0, (so don't lie
 * about the size).
 *
 * See also _lzw_buf_store_pending which must be called after the last
 * call to _lzw_buf_store_bits.
 *
 * Returns CAIRO_STATUS_SUCCESS or CAIRO_STATUS_NO_MEMORY.
 */
static cairo_status_t
_lzw_buf_store_bits (lzw_buf_t *buf, uint16_t value, int num_bits)
{
    cairo_status_t status;

    assert (value <= (1 << num_bits) - 1);

    if (getenv ("CAIRO_DEBUG_LZW"))
	printf ("%d(%d) ", value, num_bits);

    buf->pending = (buf->pending << num_bits) | value;
    buf->pending_bits += num_bits;

    while (buf->pending_bits >= 8) {
	if (buf->num_data >= buf->data_size) {
	    status = _lzw_buf_grow (buf);
	    if (status)
		return status;
	}
	buf->data[buf->num_data++] = buf->pending >> (buf->pending_bits - 8);
	buf->pending_bits -= 8;
    }

    return CAIRO_STATUS_SUCCESS;
}

/* Store the last remaining pending bits into the buffer.
 *
 * NOTE: This function must be called after the last call to
 * _lzw_buf_store_bits.
 *
 * Returns CAIRO_STATUS_SUCCESS or CAIRO_STATUS_NO_MEMORY.
 */
static cairo_status_t
_lzw_buf_store_pending  (lzw_buf_t *buf)
{
    cairo_status_t status;

    if (buf->pending_bits == 0)
	return CAIRO_STATUS_SUCCESS;

    assert (buf->pending_bits < 8);

    if (buf->num_data >= buf->data_size) {
	status = _lzw_buf_grow (buf);
	if (status)
	    return status;
    }

    buf->data[buf->num_data++] = buf->pending << (8 - buf->pending_bits);
    buf->pending_bits = 0;

    return CAIRO_STATUS_SUCCESS;
}

typedef struct _lzw_symbol {
    cairo_hash_entry_t hash_entry;

    /* "key" is the symbol */
    unsigned char *data;
    int size;
    /* "value" is the code */
    int value;
} lzw_symbol_t;

#define LZW_BITS_MIN		9
#define LZW_BITS_MAX		12
#define LZW_BITS_BOUNDARY(bits)	((1<<(bits))-1)
#define LZW_MAX_SYMBOLS		(1<<LZW_BITS_MAX)

typedef struct _lzw_symbols {
    lzw_symbol_t symbols[LZW_MAX_SYMBOLS];
    int num_symbols;

    cairo_hash_table_t *table;
} lzw_symbols_t;

static cairo_bool_t
_lzw_symbols_equal (const void *key_a, const void *key_b)
{
    const lzw_symbol_t *symbol_a = key_a;
    const lzw_symbol_t *symbol_b = key_b;

    if (symbol_a->size != symbol_b->size)
	return FALSE;

    return ! memcmp (symbol_a->data, symbol_b->data, symbol_a->size);
}

static cairo_status_t
_lzw_symbols_init (lzw_symbols_t *symbols)
{
    symbols->num_symbols = 0;

    symbols->table = _cairo_hash_table_create (_lzw_symbols_equal);
    if (symbols->table == NULL)
	return CAIRO_STATUS_NO_MEMORY;

    return CAIRO_STATUS_SUCCESS;
}

static void
_lzw_symbols_fini (lzw_symbols_t *symbols)
{
    int i;

    for (i=0; i < symbols->num_symbols; i++)
	_cairo_hash_table_remove (symbols->table, &symbols->symbols[i].hash_entry);

    symbols->num_symbols = 0;

    _cairo_hash_table_destroy (symbols->table);
}

static cairo_bool_t
_lzw_symbols_has (lzw_symbols_t *symbols,
		  lzw_symbol_t  *symbol,
		  lzw_symbol_t **code)
{
    symbol->hash_entry.hash = _cairo_hash_bytes (symbol->data, symbol->size);

    return _cairo_hash_table_lookup (symbols->table,
				     &symbol->hash_entry,
				     (cairo_hash_entry_t **) code);
}

static cairo_status_t
_lzw_symbols_store (lzw_symbols_t *symbols,
		    lzw_symbol_t  *symbol)
{
    cairo_status_t status;

    symbol->hash_entry.hash = _cairo_hash_bytes (symbol->data, symbol->size);

    symbols->symbols[symbols->num_symbols] = *symbol;

    status = _cairo_hash_table_insert (symbols->table, &symbols->symbols[symbols->num_symbols].hash_entry);
    if (status)
	return status;

    symbols->num_symbols++;

    return CAIRO_STATUS_SUCCESS;
}

#define LZW_CODE_CLEAR_TABLE	256
#define LZW_CODE_EOD		257
#define LZW_CODE_FIRST		258

unsigned char *
_cairo_lzw_compress (unsigned char *data, unsigned long *size_in_out)
{
    cairo_status_t status;
    int bytes_remaining = *size_in_out;
    lzw_symbols_t symbols;
    lzw_symbol_t symbol, *tmp, *code;
    lzw_buf_t buf;
    int code_next = LZW_CODE_FIRST;
    int code_bits = LZW_BITS_MIN;

    status = _lzw_buf_init (&buf, *size_in_out / 4);
    if (status)
	return NULL;

    status = _lzw_symbols_init (&symbols);
    if (status) {
	_lzw_buf_fini (&buf);
	return NULL;
    }

    _lzw_buf_store_bits (&buf, LZW_CODE_CLEAR_TABLE, code_bits);
    
    symbol.data = data;
    symbol.size = 2;

    while (bytes_remaining) {
	code = NULL;
	while (symbol.size <= bytes_remaining &&
	       _lzw_symbols_has (&symbols, &symbol, &tmp))
	{
	    code = tmp;
	    symbol.size++;
	}

	if (code)
	    _lzw_buf_store_bits (&buf, code->value, code_bits);
	else
	    _lzw_buf_store_bits (&buf, symbol.data[0], code_bits);

	if (symbol.size == bytes_remaining + 1)
	    break;

	symbol.value = code_next++;
	_lzw_symbols_store (&symbols, &symbol);

	/* XXX: This is just for compatibility testing against libtiff. */
	if (code_next == LZW_BITS_BOUNDARY(LZW_BITS_MAX) - 1) {
	    _lzw_symbols_fini (&symbols);
	    _lzw_symbols_init (&symbols);
	    _lzw_buf_store_bits (&buf, LZW_CODE_CLEAR_TABLE, code_bits);
	    code_bits = LZW_BITS_MIN;
	    code_next = LZW_CODE_FIRST;
	}

	if (code_next > LZW_BITS_BOUNDARY(code_bits))
	{
	    code_bits++;
	    if (code_bits > LZW_BITS_MAX) {
		_lzw_symbols_fini (&symbols);
		_lzw_symbols_init (&symbols);
		_lzw_buf_store_bits (&buf, LZW_CODE_CLEAR_TABLE, code_bits);
		code_bits = LZW_BITS_MIN;
		code_next = LZW_CODE_FIRST;
	    }
	}

	if (code) {
	    symbol.data += (symbol.size - 1);
	    bytes_remaining -= (symbol.size - 1);
	} else {
	    symbol.data += 1;
	    bytes_remaining -= 1;
	}
	symbol.size = 2;
    }

    _lzw_buf_store_bits (&buf, LZW_CODE_EOD, code_bits);

    _lzw_buf_store_pending (&buf);

    _lzw_symbols_fini (&symbols);

    *size_in_out = buf.num_data;
    return buf.data;
}

static unsigned long
_cairo_hash_bytes (const unsigned char *c, int size)
{
    /* This is the djb2 hash. */
    unsigned long hash = 5381;
    while (size--)
	hash = ((hash << 5) + hash) + *c++;
    return hash;
}
