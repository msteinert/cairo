/*
 * Copyright © 2006 Joonas Pihlaja
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */

#include "cairoint.h"

#include "cairo-freelist-private.h"

void
_cairo_freelist_init (cairo_freelist_t *freelist, unsigned nodesize)
{
    memset (freelist, 0, sizeof (cairo_freelist_t));
    freelist->nodesize = nodesize;
}

void
_cairo_freelist_fini (cairo_freelist_t *freelist)
{
    cairo_freelist_node_t *node = freelist->first_free_node;
    while (node) {
	cairo_freelist_node_t *next;

	VG (VALGRIND_MAKE_MEM_DEFINED (node, sizeof (node->next)));
	next = node->next;

	free (node);
	node = next;
    }
}

void *
_cairo_freelist_alloc (cairo_freelist_t *freelist)
{
    if (freelist->first_free_node) {
	cairo_freelist_node_t *node;

	node = freelist->first_free_node;
	VG (VALGRIND_MAKE_MEM_DEFINED (node, sizeof (node->next)));
	freelist->first_free_node = node->next;
	VG (VALGRIND_MAKE_MEM_UNDEFINED (node, freelist->nodesize));

	return node;
    }

    return malloc (freelist->nodesize);
}

void *
_cairo_freelist_calloc (cairo_freelist_t *freelist)
{
    void *node = _cairo_freelist_alloc (freelist);
    if (node)
	memset (node, 0, freelist->nodesize);
    return node;
}

void
_cairo_freelist_free (cairo_freelist_t *freelist, void *voidnode)
{
    cairo_freelist_node_t *node = voidnode;
    if (node) {
	node->next = freelist->first_free_node;
	freelist->first_free_node = node;
	VG (VALGRIND_MAKE_MEM_NOACCESS (node, freelist->nodesize));
    }
}


void
_cairo_freepool_init (cairo_freepool_t *freepool, unsigned nodesize)
{
    int poolsize;
    char *ptr;

    freepool->first_free_node = NULL;
    freepool->pools = NULL;
    freepool->nodesize = nodesize;

    poolsize = sizeof (freepool->embedded_pool);
    ptr = freepool->embedded_pool + poolsize - freepool->nodesize;

    poolsize /= freepool->nodesize;
    while (poolsize--) {
	cairo_freelist_node_t *node = (cairo_freelist_node_t *) ptr;
	ptr -= freepool->nodesize;

	node->next = freepool->first_free_node;
	freepool->first_free_node = node;
	VG (VALGRIND_MAKE_MEM_NOACCESS (node, freepool->nodesize));
    }
}

void
_cairo_freepool_fini (cairo_freepool_t *freepool)
{
    cairo_freelist_node_t *node = freepool->pools;
    while (node != NULL) {
	cairo_freelist_node_t *next;

	VG (VALGRIND_MAKE_MEM_DEFINED (node, sizeof (node->next)));
	next = node->next;

	free (node);
	node = next;
    }
    VG (VALGRIND_MAKE_MEM_NOACCESS (freepool, sizeof (freepool)));
}

void *
_cairo_freepool_alloc_from_new_pool (cairo_freepool_t *freepool)
{
    cairo_freelist_node_t *node;
    char *ptr;
    int poolsize;

    poolsize = (128 * freepool->nodesize + 8191) & -8192;
    node = malloc (poolsize);
    if (node == NULL)
	return node;

    node->next = freepool->pools;
    freepool->pools = node;

    ptr = (char *) node + poolsize - freepool->nodesize;

    poolsize -= sizeof (cairo_freelist_node_t);
    poolsize /= freepool->nodesize;

    while (--poolsize) {
	node = (cairo_freelist_node_t *) ptr;
	ptr -= freepool->nodesize;

	node->next = freepool->first_free_node;
	freepool->first_free_node = node;
	VG (VALGRIND_MAKE_MEM_NOACCESS (node, freepool->nodesize));
    }

    return ptr;
}
