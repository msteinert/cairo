/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2004 Red Hat, Inc
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
 *	Kristian Høgsberg <krh@redhat.com>
 */

#include "cairoint.h"

/**
 * _cairo_array_init:
 * 
 * Initialize a new cairo_array object to store objects each of size
 * @element_size.
 *
 * The #cairo_array_t object provides grow-by-doubling storage. It
 * never intereprets the data passed to it, nor does it provide any
 * sort of callback mechanism for freeing resources held onto by
 * stored objects.
 *
 * When finished using the array, _cairo_array_fini() should be
 * called to free resources allocated during use of the array.
 **/
void
_cairo_array_init (cairo_array_t *array, int element_size)
{
    array->size = 0;
    array->num_elements = 0;
    array->element_size = element_size;
    array->elements = NULL;
}

/**
 * _cairo_array_fini:
 *
 * Free all resources associated with @array. After this call, @array
 * should not be used again without a subsequent call to
 * _cairo_array_init() again first.
 **/
void
_cairo_array_fini (cairo_array_t *array)
{
    free (array->elements);
}

/**
 * _cairo_array_grow_by:
 * 
 * Increase the size of @array (if needed) so that there are at least
 * @additional free spaces in the array. The actual size of the array
 * is always increased by doubling as many times as necessary.
 **/
cairo_status_t
_cairo_array_grow_by (cairo_array_t *array, int additional)
{
    char *new_elements;
    int old_size = array->size;
    int required_size = array->num_elements + additional;
    int new_size;

    if (required_size <= old_size)
	return CAIRO_STATUS_SUCCESS;

    if (old_size == 0)
	new_size = 1;
    else
	new_size = old_size * 2;

    while (new_size < required_size)
	new_size = new_size * 2;

    array->size = new_size;
    new_elements = realloc (array->elements,
			    array->size * array->element_size);

    if (new_elements == NULL) {
	array->size = old_size;
	return CAIRO_STATUS_NO_MEMORY;
    }

    array->elements = new_elements;

    return CAIRO_STATUS_SUCCESS;
}

/**
 * _cairo_array_truncate:
 * 
 * Truncate size of the array to @num_elements if less than the
 * current size. No memory is actually freed. The stored objects
 * beyond @num_elements are simply "forgotten".
 **/
void
_cairo_array_truncate (cairo_array_t *array, int num_elements)
{
    if (num_elements < array->num_elements)
	array->num_elements = num_elements;
}

/**
 * _cairo_array_index:
 * 
 * Return value: A pointer to object stored at @index. If the
 * resulting value is assigned to a pointer to an object of the same
 * element_size as initially passed to _cairo_array_init() then that
 * pointer may be used for further direct indexing with []. For
 * example:
 *
 * 	cairo_array_t array;
 *	double *values;
 *
 *	_cairo_array_init (&array, sizeof(double));
 *	... calls to _cairo_array_append() here ...
 *
 *	values = _cairo_array_index (&array, 0);
 *      for (i = 0; i < _cairo_array_num_elements (&array); i++)
 *	    ... use values[i] here ...
 **/
void *
_cairo_array_index (cairo_array_t *array, int index)
{
    assert (0 <= index && index < array->num_elements);

    return (void *) &array->elements[index * array->element_size];
}

/**
 * _cairo_array_copy_element:
 * 
 * Copy a single element out of the array from index @index into the
 * location pointed to by @dst.
 **/
void
_cairo_array_copy_element (cairo_array_t *array, int index, void *dst)
{
    memcpy (dst, _cairo_array_index (array, index), array->element_size);
}

/**
 * _cairo_array_append:
 * 
 * Append one or more elements onto the end of @array.
 *
 * Return value: The address of the initial element as stored in the
 * array or NULL if out of memory.
 **/
void *
_cairo_array_append (cairo_array_t *array,
		     const void *elements, int num_elements)
{
    cairo_status_t status;
    void *dest;

    status = _cairo_array_grow_by (array, num_elements);
    if (status != CAIRO_STATUS_SUCCESS)
	return NULL;

    assert (array->num_elements + num_elements <= array->size);

    dest = &array->elements[array->num_elements * array->element_size];
    array->num_elements += num_elements;

    if (elements != NULL)
	memcpy (dest, elements, num_elements * array->element_size);

    return dest;
}

/**
 * _cairo_array_num_elements:
 * 
 * Return value: The number of elements stored in @array.
 **/
int
_cairo_array_num_elements (cairo_array_t *array)
{
    return array->num_elements;
}

/* cairo_user_data_array_t */

typedef struct {
    const cairo_user_data_key_t *key;
    void *user_data;
    cairo_destroy_func_t destroy;
} cairo_user_data_slot_t;

/**
 * _cairo_user_data_array_init:
 * @array: a #cairo_user_data_array_t
 * 
 * Initializes a #cairo_user_data_array_t structure for future
 * use. After initialization, the array has no keys. Call
 * _cairo_user_data_array_fini() to free any allocated memory
 * when done using the array.
 **/
void
_cairo_user_data_array_init (cairo_user_data_array_t *array)
{
    _cairo_array_init (array, sizeof (cairo_user_data_slot_t));
}

/**
 * _cairo_user_data_array_fini:
 * @array: a #cairo_user_data_array_t
 * 
 * Destroys all current keys in the user data array and deallocates
 * any memory allocated for the array itself.
 **/
void
_cairo_user_data_array_fini (cairo_user_data_array_t *array)
{
    int i, num_slots;
    cairo_user_data_slot_t *slots;

    num_slots = array->num_elements;
    slots = (cairo_user_data_slot_t *) array->elements;
    for (i = 0; i < num_slots; i++) {
	if (slots[i].user_data != NULL && slots[i].destroy != NULL)
	    slots[i].destroy (slots[i].user_data);
    }

    _cairo_array_fini (array);
}

/**
 * _cairo_user_data_array_get_data:
 * @array: a #cairo_user_data_array_t
 * @key: the address of the #cairo_user_data_key_t the user data was
 * attached to
 * 
 * Returns user data previously attached using the specified
 * key.  If no user data has been attached with the given key this
 * function returns %NULL.
 * 
 * Return value: the user data previously attached or %NULL.
 **/
void *
_cairo_user_data_array_get_data (cairo_user_data_array_t     *array,
				 const cairo_user_data_key_t *key)
{
    int i, num_slots;
    cairo_user_data_slot_t *slots;

    /* We allow this to support degenerate objects such as
     * cairo_image_surface_nil. */
    if (array == NULL)
	return NULL;

    num_slots = array->num_elements;
    slots = (cairo_user_data_slot_t *) array->elements;
    for (i = 0; i < num_slots; i++) {
	if (slots[i].key == key)
	    return slots[i].user_data;
    }

    return NULL;
}

/**
 * _cairo_user_data_array_set_data:
 * @array: a #cairo_user_data_array_t
 * @key: the address of a #cairo_user_data_key_t to attach the user data to
 * @user_data: the user data to attach
 * @destroy: a #cairo_destroy_func_t which will be called when the
 * user data array is destroyed or when new user data is attached using the
 * same key.
 * 
 * Attaches user data to a user data array.  To remove user data,
 * call this function with the key that was used to set it and %NULL
 * for @data.
 *
 * Return value: %CAIRO_STATUS_SUCCESS or %CAIRO_STATUS_NO_MEMORY if a
 * slot could not be allocated for the user data.
 **/
cairo_status_t
_cairo_user_data_array_set_data (cairo_user_data_array_t     *array,
				 const cairo_user_data_key_t *key,
				 void			     *user_data,
				 cairo_destroy_func_t	      destroy)
{
    int i, num_slots;
    cairo_user_data_slot_t *slots, *s;

    s = NULL;
    num_slots = array->num_elements;
    slots = (cairo_user_data_slot_t *) array->elements;
    for (i = 0; i < num_slots; i++) {
	if (slots[i].key == key) {
	    if (slots[i].user_data != NULL && slots[i].destroy != NULL)
		slots[i].destroy (slots[i].user_data);
	    s = &slots[i];
	    break;
	}
	if (user_data && slots[i].user_data == NULL) {
	    s = &slots[i];	/* Have to keep searching for an exact match */
	}
    }

    if (user_data == NULL) {
	if (s != NULL) {
	    s->key = NULL;
	    s->user_data = NULL;
	    s->destroy = NULL;
	}

	return CAIRO_STATUS_SUCCESS;
	
    } else {
	if (s == NULL)
	    s = _cairo_array_append (array, NULL, 1);
	if (s == NULL)
	    return CAIRO_STATUS_NO_MEMORY;

	s->key = key;
	s->user_data = user_data;
	s->destroy = destroy;
    }

    return CAIRO_STATUS_SUCCESS;
}

