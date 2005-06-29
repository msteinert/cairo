/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2004 Red Hat, Inc.
 * Copyright © 2005 Red Hat, Inc.
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
 * The Initial Developer of the Original Code is Red Hat, Inc.
 *
 * Contributor(s):
 *      Keith Packard <keithp@keithp.com>
 *	Graydon Hoare <graydon@redhat.com>
 *	Carl Worth <cworth@cworth.org>
 */

#include "cairo-hash-private.h"

/*
 * An entry can be in one of three states:
 *
 * FREE: Entry has never been used, terminates all searches.
 * 
 * LIVE: Entry is currently being used.
 *
 * DEAD: Entry had been live in the past. A dead entry can be reused
 *       but does not terminate a search for an exact entry.
 *
 * We expect keys will not be destroyed frequently, so our table does not
 * contain any explicit shrinking code nor any chain-coalescing code for
 * entries randomly deleted by memory pressure (except during rehashing, of
 * course). These assumptions are potentially bad, but they make the
 * implementation straightforward.
 *
 * Revisit later if evidence appears that we're using excessive memory from
 * a mostly-dead table.
 *
 * This table is open-addressed with double hashing. Each table size is a
 * prime chosen to be a little more than double the high water mark for a
 * given arrangement, so the tables should remain < 50% full. The table
 * size makes for the "first" hash modulus; a second prime (2 less than the
 * first prime) serves as the "second" hash modulus, which is co-prime and
 * thus guarantees a complete permutation of table indices.
 */

typedef enum {
    CAIRO_HASH_ENTRY_STATE_FREE = 0,
    CAIRO_HASH_ENTRY_STATE_LIVE = 1,
    CAIRO_HASH_ENTRY_STATE_DEAD = 2
} cairo_hash_entry_state_t;

typedef struct {
    cairo_hash_entry_state_t state;
    unsigned long hash_code;
    void *key;
    void *value;
} cairo_hash_entry_t;

/* 
 * This structure, and accompanying table, is borrowed/modified from the
 * file xserver/render/glyph.c in the freedesktop.org x server, with
 * permission (and suggested modification of doubling sizes) by Keith
 * Packard.
 */

typedef struct _cairo_hash_table_arrangement {
    unsigned long high_water_mark;
    unsigned long size;
    unsigned long rehash;
} cairo_hash_table_arrangement_t;

static const cairo_hash_table_arrangement_t hash_table_arrangements [] = {
    { 16,		43,		41        },
    { 32,		73,		71        },
    { 64,		151,		149       },
    { 128,		283,		281       },
    { 256,		571,		569       },
    { 512,		1153,		1151      },
    { 1024,		2269,		2267      },
    { 2048,		4519,		4517      },
    { 4096,		9013,		9011      },
    { 8192,		18043,		18041     },
    { 16384,		36109,		36107     },
    { 32768,		72091,		72089     },
    { 65536,		144409,		144407    },
    { 131072,		288361,		288359    },
    { 262144,		576883,		576881    },
    { 524288,		1153459,	1153457   },
    { 1048576,		2307163,	2307161   },
    { 2097152,		4613893,	4613891   },
    { 4194304,		9227641,	9227639   },
    { 8388608,		18455029,	18455027  },
    { 16777216,		36911011,	36911009  },
    { 33554432,		73819861,	73819859  },
    { 67108864,		147639589,	147639587 },
    { 134217728,	295279081,	295279079 },
    { 268435456,	590559793,	590559791 }
};

#define NUM_HASH_TABLE_ARRANGEMENTS (sizeof(hash_table_arrangements)/sizeof(hash_table_arrangements[0]))

struct _cairo_hash_table {
    cairo_compute_hash_func_t compute_hash;
    cairo_keys_equal_func_t keys_equal;
    cairo_destroy_func_t key_destroy;
    cairo_destroy_func_t value_destroy;

    const cairo_hash_table_arrangement_t *arrangement;
    cairo_hash_entry_t *entries;

    unsigned long live_entries;
};

cairo_hash_table_t *
_cairo_hash_table_create (cairo_compute_hash_func_t compute_hash,
			  cairo_keys_equal_func_t   keys_equal,
			  cairo_destroy_func_t	    key_destroy,
			  cairo_destroy_func_t	    value_destroy)
{    
    cairo_hash_table_t *hash_table;

    hash_table = malloc (sizeof (cairo_hash_table_t));
    if (hash_table == NULL)
	return NULL;

    hash_table->compute_hash = compute_hash;
    hash_table->keys_equal = keys_equal;
    hash_table->key_destroy = key_destroy;
    hash_table->value_destroy = value_destroy;

    hash_table->arrangement = &hash_table_arrangements[0];

    hash_table->live_entries = 0;

    hash_table->entries = calloc (hash_table->arrangement->size,
				  sizeof(cairo_hash_entry_t));
				 
    if (hash_table->entries == NULL) {
	free (hash_table);
	return NULL;
    }    

    return hash_table;
}

static void
_cairo_hash_table_destroy_entry (cairo_hash_table_t *hash_table,
				 cairo_hash_entry_t *entry)
{
    if (entry->state != CAIRO_HASH_ENTRY_STATE_LIVE)
	return;

    assert(hash_table->live_entries > 0);

    hash_table->live_entries--;

    if (hash_table->key_destroy)
	hash_table->key_destroy (entry->key);

    if (hash_table->value_destroy)
	hash_table->value_destroy (entry->value);

    entry->state = CAIRO_HASH_ENTRY_STATE_DEAD;
}

void
_cairo_hash_table_destroy (cairo_hash_table_t *hash_table)
{
    unsigned long i;
    if (hash_table == NULL)
	return;
	
    for (i = 0; i < hash_table->arrangement->size; i++)
	_cairo_hash_table_destroy_entry (hash_table,
					 &hash_table->entries[i]);
	
    free (hash_table->entries);
    hash_table->entries = NULL;

    free (hash_table);
}

/**
 * _cairo_hash_table_lookup_internal:
 *
 * @hash_table: a #cairo_hash_table_t to search
 * @key: the key to search on
 * @hash_code: the hash_code for @key
 * @key_unique: If TRUE, then caller asserts that no key already
 * exists that will compare equal to #key, so search can be
 * optimized. If unsure, set to FALSE and the code will always work.
 * 
 * Search the hashtable for a live entry for which
 * hash_table->keys_equal returns true. If no such entry exists then
 * return the first available (free or dead entry).
 *
 * If the key_unique flag is set, then the search will never call
 * hash_table->keys_equal and will act as if it always returned
 * false. This is useful as a performance optimization in special
 * circumstances where the caller knows that there is no existing
 * entry in the hash table with a matching key.
 *
 * Return value: The matching entry in the hash table (if
 * any). Otherwise, the first available entry. The caller should check
 * entry->state to check whether a match was found or not.
 **/
static cairo_hash_entry_t *
_cairo_hash_table_lookup_internal (cairo_hash_table_t *hash_table,
				   void		      *key,
				   unsigned long       hash_code,
				   cairo_bool_t	       key_is_unique)
{    
    cairo_hash_entry_t *entry, *first_available = NULL;
    unsigned long table_size, i, idx, step;
    
    table_size = hash_table->arrangement->size;

    idx = hash_code % table_size;
    step = 0;

    for (i = 0; i < table_size; ++i)
    {
	entry = &hash_table->entries[idx];

	switch (entry->state) {
	case CAIRO_HASH_ENTRY_STATE_FREE:
	    return entry;
	case CAIRO_HASH_ENTRY_STATE_LIVE:
	    if (! key_is_unique)
		if (hash_table->keys_equal (key, entry->key))
		    return entry;
	    break;
	case CAIRO_HASH_ENTRY_STATE_DEAD:
	    if (key_is_unique) {
		return entry;
	    } else {
		if (! first_available)
		    first_available = entry;
	    }
	    break;
	}

	if (step == 0) { 	    
	    step = hash_code % hash_table->arrangement->rehash;
	    if (step == 0)
		step = 1;
	}

	idx += step;
	if (idx >= table_size)
	    idx -= table_size;
    }

    /* 
     * The table should not have permitted you to get here if you were just
     * looking for a free slot: there should have been room.
     */
    assert (key_is_unique == 0);

    return first_available;
}

static cairo_status_t
_cairo_hash_table_resize (cairo_hash_table_t *hash_table,
			  unsigned long       proposed_size)
{
    cairo_hash_table_t tmp;
    cairo_hash_entry_t *entry;
    unsigned long new_size, i;

    if (hash_table->arrangement->high_water_mark >= proposed_size)
	return CAIRO_STATUS_SUCCESS;

    tmp = *hash_table;

    for (i = 0; i < NUM_HASH_TABLE_ARRANGEMENTS; i++)
	if (hash_table_arrangements[i].high_water_mark >= proposed_size) {
	    tmp.arrangement = &hash_table_arrangements[i];
	    break;
	}
    /* This code is being abused if we can't make a table big enough. */
    assert (i < NUM_HASH_TABLE_ARRANGEMENTS);

    new_size = tmp.arrangement->size;
    tmp.entries = calloc (new_size, sizeof (cairo_hash_entry_t));
    if (tmp.entries == NULL) 
	return CAIRO_STATUS_NO_MEMORY;
        
    for (i = 0; i < hash_table->arrangement->size; ++i) {
	if (hash_table->entries[i].state == CAIRO_HASH_ENTRY_STATE_LIVE) {
	    entry = _cairo_hash_table_lookup_internal (&tmp,
						       hash_table->entries[i].key,
						       hash_table->entries[i].hash_code,
						       TRUE);
	    assert (entry->state == CAIRO_HASH_ENTRY_STATE_FREE);
	    *entry = hash_table->entries[i];
	}
    }
    free (hash_table->entries);
    hash_table->entries = tmp.entries;
    hash_table->arrangement = tmp.arrangement;
    return CAIRO_STATUS_SUCCESS;
}

cairo_bool_t
_cairo_hash_table_lookup (cairo_hash_table_t *hash_table,
			  void		     *key,
			  void		    **value_return)
{
    unsigned long hash_code;
    cairo_hash_entry_t *entry;

    hash_code = hash_table->compute_hash (key);

    /* See if we have an entry in the table already. */
    entry = _cairo_hash_table_lookup_internal (hash_table, key,
					       hash_code, FALSE);
    if (entry->state == CAIRO_HASH_ENTRY_STATE_LIVE) {
	*value_return = entry->value;
	return TRUE;
    }

    *value_return = NULL;
    return FALSE;
}

cairo_status_t
_cairo_hash_table_insert (cairo_hash_table_t *hash_table,
			  void		     *key,
			  void		     *value)
{
    cairo_status_t status;
    unsigned long hash_code;
    cairo_hash_entry_t *entry;

    hash_code = hash_table->compute_hash (key);
    
    /* Ensure there is room in the table in case we need to add a new
     * entry. */
    status = _cairo_hash_table_resize (hash_table,
				       hash_table->live_entries + 1);
    if (status != CAIRO_STATUS_SUCCESS) {
	return status;
    }

    entry = _cairo_hash_table_lookup_internal (hash_table, key,
					       hash_code, FALSE);
    
    if (entry->state == CAIRO_HASH_ENTRY_STATE_LIVE)
    {
	/* Duplicate entry. Preserve old key, replace value. */
	if (hash_table->key_destroy)
	    hash_table->key_destroy (key);
	if (hash_table->value_destroy)
	    hash_table->value_destroy (entry->value);
	entry->value = value;
    }
    else
    {
	/* New entry. Store value and increment statistics. */
	entry->state = CAIRO_HASH_ENTRY_STATE_LIVE;
	entry->key = key;
	entry->hash_code = hash_code;
	entry->value = value;

	hash_table->live_entries++;
    }

    return status;
}

void
_cairo_hash_table_remove (cairo_hash_table_t *hash_table,
			  void		     *key)
{
    unsigned long hash_code;
    cairo_hash_entry_t *entry;

    hash_code = hash_table->compute_hash (key);

    /* See if we have an entry in the table already. */
    entry = _cairo_hash_table_lookup_internal (hash_table, key,
					       hash_code, FALSE);
    if (entry->state == CAIRO_HASH_ENTRY_STATE_LIVE)
      	_cairo_hash_table_destroy_entry (hash_table, entry);
}

void
_cairo_hash_table_foreach (cairo_hash_table_t	      *hash_table,
			   cairo_hash_callback_func_t  hash_callback,
			   void			      *closure)
{
    unsigned long i;
    cairo_hash_entry_t *entry;

    if (hash_table == NULL)
	return;
	
    for (i = 0; i < hash_table->arrangement->size; i++) {
	entry = &hash_table->entries[i];
	if (entry->state == CAIRO_HASH_ENTRY_STATE_LIVE)
	    hash_callback (entry->key, entry->value, closure);
    }
}
