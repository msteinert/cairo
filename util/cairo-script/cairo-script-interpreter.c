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

#include <cairo.h>

#include "cairo-script-private.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <byteswap.h>
#include <math.h>

csi_status_t
_csi_error (csi_status_t status)
{
    return status;
}

/* XXX track global/local memory, cap etc, mark/sweep GC */
void *
_csi_alloc (csi_t *ctx, int size)
{
    if (_csi_unlikely (ctx->status))
	return NULL;

    return malloc (size);
}

void *
_csi_alloc0 (csi_t *ctx, int size)
{
    void *ptr;

    ptr = _csi_alloc (ctx, size);
    if (_csi_likely (ptr != NULL))
	memset (ptr, 0, size);

    return ptr;
}

void *
_csi_realloc (csi_t *ctx, void *ptr, int size)
{
    if (_csi_unlikely (ctx->status))
	return NULL;

    return realloc (ptr, size);
}

void
_csi_free (csi_t *ctx, void *ptr)
{
    if (_csi_unlikely (ptr == NULL))
	return;

    free (ptr);
}

void *
_csi_slab_alloc (csi_t *ctx, int size)
{
    if (_csi_unlikely (ctx->status))
	return NULL;

    return malloc (size);
}

void
_csi_slab_free (csi_t *ctx, void *ptr, int size)
{
    if (_csi_unlikely (ptr == NULL))
	return;

    free (ptr);
}

static csi_status_t
_add_operator (csi_t *ctx,
	       csi_dictionary_t *dict,
	       const csi_operator_def_t *def)
{
    csi_object_t name;
    csi_object_t operator;
    csi_status_t status;

    status = csi_name_new_static (ctx, &name, def->name);
    if (status)
	return status;

    status = csi_operator_new (ctx, &operator, def->op);
    if (status)
	return status;

    return csi_dictionary_put (ctx, dict, name.datum.name, &operator);
}

static csi_status_t
_add_integer_constant (csi_t *ctx,
		       csi_dictionary_t *dict,
		       const csi_integer_constant_def_t *def)
{
    csi_object_t name;
    csi_object_t constant;
    csi_status_t status;

    status = csi_name_new_static (ctx, &name, def->name);
    if (status)
	return status;

    status = csi_integer_new (ctx, &constant, def->value);
    if (status)
	return status;

    return csi_dictionary_put (ctx, dict, name.datum.name, &constant);
}

static csi_status_t
_init_dictionaries (csi_t *ctx)
{
    csi_status_t status;
    csi_stack_t *stack;
    csi_object_t obj;
    csi_dictionary_t *dict;
    const csi_operator_def_t *odef;
    const csi_integer_constant_def_t *idef;

    stack = &ctx->dstack;

    status = _csi_stack_init (ctx, stack, 4);
    if (status)
	return status;

    /* systemdict */
    status = csi_dictionary_new (ctx, &obj);
    if (status)
	return status;

    status = _csi_stack_push (ctx, stack, &obj);
    if (status)
	return status;

    dict = obj.datum.dictionary;

    /* fill systemdict with operators */
    for (odef = _csi_operators (); odef->name != NULL; odef++) {
	status = _add_operator (ctx, dict, odef);
	if (status)
	    return status;
    }

    /* add constants */
    for (idef = _csi_integer_constants (); idef->name != NULL; idef++) {
	status = _add_integer_constant (ctx, dict, idef);
	if (status)
	    return status;
    }

    /* and seal */
    //dict.type &= ~CSI_OBJECT_ATTR_WRITABLE;


    /* globaldict */
    status = csi_dictionary_new (ctx, &obj);
    if (status)
	return status;
    status = _csi_stack_push (ctx, stack, &obj);
    if (status)
	return status;

    /* userdict */
    status = csi_dictionary_new (ctx, &obj);
    if (status)
	return status;
    status = _csi_stack_push (ctx, stack, &obj);
    if (status)
	return status;

    return CSI_STATUS_SUCCESS;
}

/* intern string */

typedef struct _cairo_intern_string {
    csi_hash_entry_t hash_entry;
    int len;
    char *string;
} csi_intern_string_t;

static unsigned long
_intern_string_hash (const char *str, int len)
{
    const signed char *p = (const signed char *) str;
    unsigned int h = *p;

    for (p += 1; --len; p++)
	h = (h << 5) - h + *p;

    return h;
}

static cairo_bool_t
_intern_string_equal (const void *_a, const void *_b)
{
    const csi_intern_string_t *a = _a;
    const csi_intern_string_t *b = _b;

    if (a->len != b->len)
	return FALSE;

    return memcmp (a->string, b->string, a->len) == 0;
}

static void
_csi_init (csi_t *ctx)
{
    csi_status_t status;

    ctx->status = CSI_STATUS_SUCCESS;
    ctx->ref_count = 1;

    status = _csi_hash_table_init (&ctx->strings, _intern_string_equal);
    if (status)
	goto FAIL;

    status = _csi_stack_init (ctx, &ctx->ostack, 2048);
    if (status)
	goto FAIL;
    status = _init_dictionaries (ctx);
    if (status)
	goto FAIL;

    status = _csi_scanner_init (ctx, &ctx->scanner);
    if (status)
	goto FAIL;

    return;

FAIL:
    if (ctx->status == CSI_STATUS_SUCCESS)
	ctx->status = status;
}

static void
_intern_string_pluck (void *entry, void *closure)
{
    csi_t *ctx = closure;

    _csi_hash_table_remove (&ctx->strings, entry);
    _csi_free (ctx, entry);
}

static void
_csi_fini (csi_t *ctx)
{
    _csi_stack_fini (ctx, &ctx->ostack);
    _csi_stack_fini (ctx, &ctx->dstack);
    _csi_scanner_fini (ctx, &ctx->scanner);

    _csi_hash_table_foreach (&ctx->strings, _intern_string_pluck, ctx);
    _csi_hash_table_fini (&ctx->strings);
}

csi_status_t
_csi_name_define (csi_t *ctx, csi_name_t name, csi_object_t *obj)
{
    return csi_dictionary_put (ctx,
			ctx->dstack.objects[ctx->dstack.len-1].datum.dictionary,
			name,
			obj);
}

csi_status_t
_csi_name_lookup (csi_t *ctx, csi_name_t name, csi_object_t *obj)
{
    int i;

    for (i = ctx->dstack.len; i--; ) {
	csi_dictionary_t *dict;
	csi_dictionary_entry_t *entry;

	dict = ctx->dstack.objects[i].datum.dictionary;
	entry = _csi_hash_table_lookup (&dict->hash_table,
					(csi_hash_entry_t *) &name);
	if (entry != NULL) {
	    *obj = entry->value;
	    return CSI_STATUS_SUCCESS;
	}
    }

    return _csi_error (CSI_STATUS_INVALID_SCRIPT);
}

csi_status_t
_csi_name_undefine (csi_t *ctx, csi_name_t name)
{
    unsigned int i;

    for (i = ctx->dstack.len; --i; ) {
	if (csi_dictionary_has (ctx->dstack.objects[i].datum.dictionary,
				name))
	{
	    csi_dictionary_remove (ctx,
				   ctx->dstack.objects[i].datum.dictionary,
				   name);
	    return CSI_STATUS_SUCCESS;
	}
    }

    return _csi_error (CSI_STATUS_INVALID_SCRIPT);
}

csi_status_t
_csi_intern_string (csi_t *ctx, const char **str_inout, int len)
{
    char *str = (char *) *str_inout;
    csi_intern_string_t tmpl, *istring;
    csi_status_t status = CSI_STATUS_SUCCESS;

    if (len < 0)
	len = strlen (str);
    tmpl.hash_entry.hash = _intern_string_hash (str, len);
    tmpl.len = len;
    tmpl.string = (char *) str;

    istring = _csi_hash_table_lookup (&ctx->strings, &tmpl.hash_entry);
    if (istring == NULL) {
	istring = _csi_alloc (ctx, sizeof (csi_intern_string_t) + len + 1);
	if (istring != NULL) {
	    istring->hash_entry.hash = tmpl.hash_entry.hash;
	    istring->len = tmpl.len;
	    istring->string = (char *) (istring + 1);
	    memcpy (istring->string, str, len);
	    istring->string[len] = '\0';

	    status = _csi_hash_table_insert (&ctx->strings,
					     &istring->hash_entry);
	    if (_csi_unlikely (status)) {
		_csi_free (ctx, istring);
		return status;
	    }
	} else
	    return _csi_error (CSI_STATUS_NO_MEMORY);
    }

    *str_inout = istring->string;
    return CSI_STATUS_SUCCESS;
}

/* Public */

static csi_t _csi_nil = { -1, CSI_STATUS_NO_MEMORY };

csi_t *
cairo_script_interpreter_create (void)
{
    csi_t *ctx;

    ctx = calloc (1, sizeof (csi_t));
    if (ctx == NULL)
	return (csi_t *) &_csi_nil;

    _csi_init (ctx);

    return ctx;
}

void
cairo_script_interpreter_install_hooks (csi_t *ctx,
					const csi_hooks_t *hooks)
{
    if (ctx->status)
	return;

    ctx->hooks = *hooks;
}

cairo_status_t
cairo_script_interpreter_run (csi_t *ctx, const char *filename)
{
    csi_object_t file;

    if (ctx->status)
	return ctx->status;

    ctx->status = csi_file_new (ctx, &file, filename, "r");
    if (ctx->status)
	return ctx->status;

    file.type |= CSI_OBJECT_ATTR_EXECUTABLE;

    ctx->status = csi_object_execute (ctx, &file);
    csi_object_free (ctx, &file);

    return ctx->status;
}

cairo_status_t
cairo_script_interpreter_feed_string (csi_t *ctx, const char *line, int len)
{
    csi_object_t file;

    if (ctx->status)
	return ctx->status;

    if (len < 0)
	len = strlen (line);
    ctx->status = csi_file_new_for_bytes (ctx, &file, line, len);
    if (ctx->status)
	return ctx->status;

    file.type |= CSI_OBJECT_ATTR_EXECUTABLE;

    ctx->status = csi_object_execute (ctx, &file);
    csi_object_free (ctx, &file);

    return ctx->status;
}

csi_t *
cairo_script_interpreter_reference (csi_t *ctx)
{
    ctx->ref_count++;
    return ctx;
}
slim_hidden_def (cairo_script_interpreter_reference);

cairo_status_t
cairo_script_interpreter_destroy (csi_t *ctx)
{
    csi_status_t status;

    status = ctx->status;
    if (--ctx->ref_count)
	return status;

    _csi_fini (ctx);
    free (ctx);

    return status;
}
slim_hidden_def (cairo_script_interpreter_destroy);
