/*
 * Copyright © 2006 Keith Packard
 * Copyright © 2006 Carl Worth
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

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>

#include "cairo-skiplist-private.h"

#define ELT_DATA(elt) (void *)	((char*) (elt) - list->data_size)
#define NEXT_TO_ELT(next)	(skip_elt_t *) ((char *) (next) - offsetof (skip_elt_t, next))

/*
 * Initialize an empty skip list
 */
void
skip_list_init (skip_list_t		*list,
		skip_list_compare_t	 compare,
		size_t			 elt_size)
{
    int i;

    list->compare = compare;
    list->elt_size = elt_size;
    list->data_size = elt_size - sizeof (skip_elt_t);

    for (i = 0; i < MAX_LEVEL; i++)
	list->chains[i] = NULL;

    list->max_level = 0;
}

/*
 * Generate a random level number, distributed
 * so that each level is 1/4 as likely as the one before
 *
 * Note that level numbers run 1 <= level <= MAX_LEVEL
 */
static int
random_level (void)
{
    /* tricky bit -- each bit is '1' 75% of the time */
    long int	bits = random () | random ();
    int	level = 0;

    while (++level < MAX_LEVEL)
    {
	if (bits & 1)
	    break;
	bits >>= 1;
    }
    return level;
}

/*
 * Insert 'data' into the list
 */
void *
skip_list_insert (skip_list_t *list, void *data)
{
    skip_elt_t **update[MAX_LEVEL];
    char *data_and_elt;
    skip_elt_t *elt, *prev, **next;
    int	    i, level, prev_index;

    level = random_level ();
    prev_index = level - 1;

    /*
     * Find links along each chain
     */
    prev = NULL;
    next = list->chains;
    for (i = list->max_level; --i >= 0; )
    {
	for (; (elt = next[i]); next = elt->next)
	{
	    if (list->compare (list, ELT_DATA(elt), data) > 0)
		break;
	}
        update[i] = next;
	if (i == prev_index && next != list->chains)
	    prev = NEXT_TO_ELT (next);
    }

    /*
     * Create new list element
     */
    if (level > list->max_level)
    {
	level = list->max_level + 1;
	prev_index = level - 1;
	update[list->max_level] = list->chains;
	list->max_level = level;
    }

    data_and_elt = malloc (list->elt_size + (level-1) * sizeof (skip_elt_t *));
    memcpy (data_and_elt, data, list->data_size);
    elt = (skip_elt_t *) (data_and_elt + list->data_size);

    elt->prev_index = prev_index;
    elt->prev = prev;

    /*
     * Insert into all chains
     */
    for (i = 0; i < level; i++)
    {
	elt->next[i] = update[i][i];
	if (elt->next[i] && elt->next[i]->prev_index == i)
	    elt->next[i]->prev = elt;
	update[i][i] = elt;
    }

    return data_and_elt;
}

void *
skip_list_find (skip_list_t *list, void *data)
{
    int i;
    skip_elt_t **next = list->chains;
    skip_elt_t *elt;

    /*
     * Walk chain pointers one level at a time
     */
    for (i = list->max_level; --i >= 0;)
	while (next[i] && list->compare (list, data, ELT_DATA(next[i])) > 0)
	{
	    next = next[i]->next;
	}
    /*
     * Here we are
     */
    elt = next[0];
    if (elt && list->compare (list, data, ELT_DATA (elt)) == 0)
	return ELT_DATA (elt);

    return NULL;
}

void
skip_list_delete (skip_list_t *list, void *data)
{
    skip_elt_t **update[MAX_LEVEL], *prev[MAX_LEVEL];
    skip_elt_t *elt, **next;
    int	i;

    /*
     * Find links along each chain
     */
    next = list->chains;
    for (i = list->max_level; --i >= 0; )
    {
	for (; (elt = next[i]); next = elt->next)
	{
	    if (list->compare (list, ELT_DATA (elt), data) >= 0)
		break;
	}
        update[i] = &next[i];
	if (next == list->chains)
	    prev[i] = NULL;
	else
	    prev[i] = NEXT_TO_ELT (next);
    }
    elt = next[0];
    assert (list->compare (list, ELT_DATA (elt), data) == 0);
    for (i = 0; i < list->max_level && *update[i] == elt; i++) {
	*update[i] = elt->next[i];
	if (elt->next[i] && elt->next[i]->prev_index == i)
	    elt->next[i]->prev = prev[i];
    }
    while (list->max_level > 0 && list->chains[list->max_level - 1] == NULL)
	list->max_level--;
    free (ELT_DATA (elt));
}

void
skip_list_delete_given (skip_list_t *list, skip_elt_t *given)
{
    skip_elt_t **update[MAX_LEVEL], *prev[MAX_LEVEL];
    skip_elt_t *elt, **next;
    int	i;

    /*
     * Find links along each chain
     */
    if (given->prev)
	next = given->prev->next;
    else
	next = list->chains;
    for (i = given->prev_index + 1; --i >= 0; )
    {
	for (; (elt = next[i]); next = elt->next)
	{
	    if (elt == given)
		break;
	}
        update[i] = &next[i];
	if (next == list->chains)
	    prev[i] = NULL;
	else
	    prev[i] = NEXT_TO_ELT (next);
    }
    elt = next[0];
    assert (elt == given);
    for (i = 0; i < (given->prev_index + 1) && *update[i] == elt; i++) {
	*update[i] = elt->next[i];
	if (elt->next[i] && elt->next[i]->prev_index == i)
	    elt->next[i]->prev = prev[i];
    }
    while (list->max_level > 0 && list->chains[list->max_level - 1] == NULL)
	list->max_level--;
    free (ELT_DATA (elt));
}
