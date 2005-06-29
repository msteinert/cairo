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

#ifndef CAIRO_HASH_PRIVATE_H
#define CAIRO_HASH_PRIVATE_H

#include "cairoint.h"

typedef struct _cairo_hash_table cairo_hash_table_t;

typedef unsigned long
(*cairo_compute_hash_func_t) (void *key);

typedef cairo_bool_t
(*cairo_keys_equal_func_t) (void *key_a, void *key_b);

typedef void
(*cairo_hash_callback_func_t) (void *key,
			       void *value,
			       void *closure);

cairo_private cairo_hash_table_t *
_cairo_hash_table_create (cairo_compute_hash_func_t compute_hash,
			  cairo_keys_equal_func_t   keys_equal,
			  cairo_destroy_func_t	    key_destroy,
			  cairo_destroy_func_t	    value_destroy);

cairo_private void
_cairo_hash_table_destroy (cairo_hash_table_t *hash_table);

cairo_private cairo_bool_t
_cairo_hash_table_lookup (cairo_hash_table_t *hash_table,
			  void		     *key,
			  void		    **value_return);

cairo_private cairo_status_t
_cairo_hash_table_insert (cairo_hash_table_t *hash_table,
			  void		     *key,
			  void		     *value);

cairo_private void
_cairo_hash_table_remove (cairo_hash_table_t *hash_table,
			  void		     *key);

cairo_private void
_cairo_hash_table_foreach (cairo_hash_table_t 	      *hash_table,
			   cairo_hash_callback_func_t  hash_callback,
			   void			      *closure);

#endif
