/*
 * Copyright © 2004 Carl Worth
 * Copyright © 2006 Red Hat, Inc.
 * Copyright © 2009 Chris Wilson
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
 * Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA 02110-1335, USA
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
 * The Initial Developer of the Original Code is Carl Worth
 *
 * Contributor(s):
 *	Carl D. Worth <cworth@cworth.org>
 *	Chris Wilson <chris@chris-wilson.co.uk>
 */

/* Provide definitions for standalone compilation */
#include "cairoint.h"

#include "cairo-boxes-private.h"
#include "cairo-error-private.h"
#include "cairo-combsort-private.h"
#include "cairo-list-private.h"

#include <setjmp.h>

typedef struct _rectangle rectangle_t;
typedef struct _edge edge_t;

struct _edge {
    edge_t *next, *prev;
    edge_t *right;
    cairo_fixed_t x, top;
    int dir;
};

struct _rectangle {
    edge_t left, right;
    int32_t top, bottom;
};

#define UNROLL3(x) x x x

/* the parent is always given by index/2 */
#define PQ_PARENT_INDEX(i) ((i) >> 1)
#define PQ_FIRST_ENTRY 1

/* left and right children are index * 2 and (index * 2) +1 respectively */
#define PQ_LEFT_CHILD_INDEX(i) ((i) << 1)

typedef struct _sweep_line {
    rectangle_t **rectangles;
    rectangle_t **stop;
    edge_t head, tail;
    edge_t *insert_left, *insert_right;
    int32_t current_y;
    int32_t last_y;
    int stop_size;

    cairo_fill_rule_t fill_rule;

    cairo_bool_t do_traps;
    void *container;

    jmp_buf unwind;
} sweep_line_t;

#define DEBUG_TRAPS 0

#if DEBUG_TRAPS
static void
dump_traps (cairo_traps_t *traps, const char *filename)
{
    FILE *file;
    int n;

    if (getenv ("CAIRO_DEBUG_TRAPS") == NULL)
	return;

    file = fopen (filename, "a");
    if (file != NULL) {
	for (n = 0; n < traps->num_traps; n++) {
	    fprintf (file, "%d %d L:(%d, %d), (%d, %d) R:(%d, %d), (%d, %d)\n",
		     traps->traps[n].top,
		     traps->traps[n].bottom,
		     traps->traps[n].left.p1.x,
		     traps->traps[n].left.p1.y,
		     traps->traps[n].left.p2.x,
		     traps->traps[n].left.p2.y,
		     traps->traps[n].right.p1.x,
		     traps->traps[n].right.p1.y,
		     traps->traps[n].right.p2.x,
		     traps->traps[n].right.p2.y);
	}
	fprintf (file, "\n");
	fclose (file);
    }
}
#else
#define dump_traps(traps, filename)
#endif

static inline int
rectangle_compare_start (const rectangle_t *a,
			 const rectangle_t *b)
{
    return a->top - b->top;
}

static inline int
rectangle_compare_stop (const rectangle_t *a,
			 const rectangle_t *b)
{
    return a->bottom - b->bottom;
}

static inline void
pqueue_push (sweep_line_t *sweep, rectangle_t *rectangle)
{
    rectangle_t **elements;
    int i, parent;

    elements = sweep->stop;
    for (i = ++sweep->stop_size;
	 i != PQ_FIRST_ENTRY &&
	 rectangle_compare_stop (rectangle,
				 elements[parent = PQ_PARENT_INDEX (i)]) < 0;
	 i = parent)
    {
	elements[i] = elements[parent];
    }

    elements[i] = rectangle;
}

static inline void
rectangle_pop_stop (sweep_line_t *sweep)
{
    rectangle_t **elements = sweep->stop;
    rectangle_t *tail;
    int child, i;

    tail = elements[sweep->stop_size--];
    if (sweep->stop_size == 0) {
	elements[PQ_FIRST_ENTRY] = NULL;
	return;
    }

    for (i = PQ_FIRST_ENTRY;
	 (child = PQ_LEFT_CHILD_INDEX (i)) <= sweep->stop_size;
	 i = child)
    {
	if (child != sweep->stop_size &&
	    rectangle_compare_stop (elements[child+1],
				    elements[child]) < 0)
	{
	    child++;
	}

	if (rectangle_compare_stop (elements[child], tail) >= 0)
	    break;

	elements[i] = elements[child];
    }
    elements[i] = tail;
}

static inline rectangle_t *
rectangle_pop_start (sweep_line_t *sweep_line)
{
    return *sweep_line->rectangles++;
}

static inline rectangle_t *
rectangle_peek_stop (sweep_line_t *sweep_line)
{
    return sweep_line->stop[PQ_FIRST_ENTRY];
}

CAIRO_COMBSORT_DECLARE (_rectangle_sort,
			rectangle_t *,
			rectangle_compare_start)

static void
sweep_line_init (sweep_line_t	 *sweep_line,
		 rectangle_t	**rectangles,
		 int		  num_rectangles,
		 cairo_fill_rule_t fill_rule,
		 cairo_bool_t	 do_traps,
		 void		*container)
{
    rectangles[-2] = NULL;
    rectangles[-1] = NULL;
    rectangles[num_rectangles] = NULL;
    sweep_line->rectangles = rectangles;
    sweep_line->stop = rectangles - 2;
    sweep_line->stop_size = 0;

    sweep_line->head.x = INT32_MIN;
    sweep_line->head.right = NULL;
    sweep_line->head.dir = 0;
    sweep_line->head.next = &sweep_line->tail;
    sweep_line->tail.x = INT32_MAX;
    sweep_line->tail.right = NULL;
    sweep_line->tail.dir = 0;
    sweep_line->tail.prev = &sweep_line->head;

    sweep_line->insert_left = &sweep_line->tail;
    sweep_line->insert_right = &sweep_line->tail;

    sweep_line->current_y = INT32_MIN;
    sweep_line->last_y = INT32_MIN;

    sweep_line->fill_rule = fill_rule;
    sweep_line->container = container;
    sweep_line->do_traps = do_traps;
}

static void
edge_end_box (sweep_line_t *sweep_line, edge_t *left, int32_t bot)
{
    cairo_status_t status = CAIRO_STATUS_SUCCESS;

    /* Only emit (trivial) non-degenerate trapezoids with positive height. */
    if (likely (left->top < bot)) {
	if (sweep_line->do_traps) {
	    cairo_line_t _left = {
		{ left->x, left->top },
		{ left->x, bot },
	    }, _right = {
		{ left->right->x, left->top },
		{ left->right->x, bot },
	    };
	    _cairo_traps_add_trap (sweep_line->container, left->top, bot, &_left, &_right);
	    status = _cairo_traps_status ((cairo_traps_t *) sweep_line->container);
	} else {
	    cairo_box_t box;

	    box.p1.x = left->x;
	    box.p1.y = left->top;
	    box.p2.x = left->right->x;
	    box.p2.y = bot;

	    status = _cairo_boxes_add (sweep_line->container,
				       CAIRO_ANTIALIAS_DEFAULT,
				       &box);
	}
    }
    if (unlikely (status))
	longjmp (sweep_line->unwind, status);

    left->right = NULL;
}

/* Start a new trapezoid at the given top y coordinate, whose edges
 * are `edge' and `edge->next'. If `edge' already has a trapezoid,
 * then either add it to the traps in `traps', if the trapezoid's
 * right edge differs from `edge->next', or do nothing if the new
 * trapezoid would be a continuation of the existing one. */
static inline void
edge_start_or_continue_box (sweep_line_t *sweep_line,
			    edge_t	*left,
			    edge_t	*right,
			    int		 top)
{
    if (left->right == right)
	return;

    if (left->right != NULL) {
	if (right != NULL && left->right->x == right->x) {
	    /* continuation on right, so just swap edges */
	    left->right = right;
	    return;
	}

	edge_end_box (sweep_line, left, top);
    }

    if (right != NULL && left->x != right->x) {
	left->top = top;
	left->right = right;
    }
}

static inline void
active_edges_to_traps (sweep_line_t *sweep)
{
    int top = sweep->current_y;
    edge_t *pos;

    if (sweep->last_y == sweep->current_y)
	return;

    pos = sweep->head.next;
    if (pos == &sweep->tail)
	return;

    if (sweep->fill_rule == CAIRO_FILL_RULE_WINDING) {
	do {
	    edge_t *left, *right;
	    int winding;

	    left = pos;
	    winding = left->dir;

	    right = left->next;

	    /* Check if there is a co-linear edge with an existing trap */
	    if (left->right == NULL) {
		while (unlikely (right->x == left->x)) {
		    winding += right->dir;
		    if (right->right != NULL) {
			/* continuation on left */
			left->top = right->top;
			left->right = right->right;
			right->right = NULL;
			winding -= right->dir;
			break;
		    }

		    right = right->next;
		}

		if (winding == 0) {
		    pos = right;
		    continue;
		}
	    }

	    /* Greedily search for the closing edge, so that we generate the
	     * maximal span width with the minimal number of trapezoids.
	     */

	    do {
		/* End all subsumed traps */
		if (unlikely (right->right != NULL))
		    edge_end_box (sweep, right, top);

		winding += right->dir;
		if (winding == 0 && right->x != right->next->x)
		    break;

		right = right->next;
	    } while (TRUE);

	    edge_start_or_continue_box (sweep, left, right, top);

	    pos = right->next;
	} while (pos != &sweep->tail);
    } else {
	do {
	    edge_t *right = pos->next;
	    int count = 0;

	    do {
		/* End all subsumed traps */
		if (unlikely (right->right != NULL))
		    edge_end_box (sweep, right, top);

		    /* skip co-linear edges */
		if (++count & 1 && right->x != right->next->x)
		    break;

		right = right->next;
	    } while (TRUE);

	    edge_start_or_continue_box (sweep, pos, right, top);

	    pos = right->next;
	} while (pos != &sweep->tail);
    }

    sweep->last_y = sweep->current_y;
}

static inline void
sweep_line_delete_edge (sweep_line_t *sweep_line, edge_t *edge)
{
    if (edge->right != NULL) {
	edge_t *next = edge->next;
	if (next->x == edge->x) {
	    next->top = edge->top;
	    next->right = edge->right;
	} else
	    edge_end_box (sweep_line, edge, sweep_line->current_y);
    }

    if (sweep_line->insert_left == edge)
	sweep_line->insert_left = edge->next;
    if (sweep_line->insert_right == edge)
	sweep_line->insert_right = edge->next;

    edge->prev->next = edge->next;
    edge->next->prev = edge->prev;
}

static inline cairo_bool_t
sweep_line_delete (sweep_line_t	*sweep, rectangle_t	*rectangle)
{
    cairo_bool_t update;

    update = TRUE;
    if (sweep->fill_rule == CAIRO_FILL_RULE_WINDING &&
	rectangle->left.prev->dir == rectangle->left.dir)
    {
	update = rectangle->left.next != &rectangle->right;
    }

    sweep_line_delete_edge (sweep, &rectangle->left);
    sweep_line_delete_edge (sweep, &rectangle->right);

    rectangle_pop_stop (sweep);
    return update;
}

static inline void
insert_edge (edge_t *edge, edge_t *pos)
{
    if (pos->x != edge->x) {
	if (pos->x > edge->x) {
	    do {
		if (pos->prev->x <= edge->x)
		    break;
		pos = pos->prev;
	    } while (TRUE);
	} else {
	    do {
		pos = pos->next;
		if (pos->x >= edge->x)
		    break;
	    } while (TRUE);
	}
    }

    pos->prev->next = edge;
    edge->prev = pos->prev;
    edge->next = pos;
    pos->prev = edge;
}

static inline cairo_bool_t
sweep_line_insert (sweep_line_t	*sweep,
		   rectangle_t	*rectangle)
{
    edge_t *pos;

    /* right edge */
    pos = sweep->insert_right;
    insert_edge (&rectangle->right, pos);
    sweep->insert_right = &rectangle->right;

    /* left edge */
    pos = sweep->insert_left;
    if (pos->x > sweep->insert_right->x)
	pos = sweep->insert_right->prev;
    insert_edge (&rectangle->left, pos);
    sweep->insert_left = &rectangle->left;

    pqueue_push (sweep, rectangle);

    if (sweep->fill_rule == CAIRO_FILL_RULE_WINDING &&
	rectangle->left.prev->dir == rectangle->left.dir)
    {
	return rectangle->left.next != &rectangle->right;
    }

    return TRUE;
}

static cairo_status_t
_cairo_bentley_ottmann_tessellate_rectangular (rectangle_t	**rectangles,
					       int			  num_rectangles,
					       cairo_fill_rule_t	  fill_rule,
					       cairo_bool_t		 do_traps,
					       void			*container)
{
    sweep_line_t sweep_line;
    rectangle_t *rectangle;
    cairo_status_t status;
    cairo_bool_t update = FALSE;

    sweep_line_init (&sweep_line,
		     rectangles, num_rectangles,
		     fill_rule,
		     do_traps, container);
    if ((status = setjmp (sweep_line.unwind)))
	return status;

    rectangle = rectangle_pop_start (&sweep_line);
    do {
	if (rectangle->top != sweep_line.current_y) {
	    rectangle_t *stop;

	    stop = rectangle_peek_stop (&sweep_line);
	    while (stop != NULL && stop->bottom < rectangle->top) {
		if (stop->bottom != sweep_line.current_y) {
		    if (update) {
			active_edges_to_traps (&sweep_line);
			update = FALSE;
		    }

		    sweep_line.current_y = stop->bottom;
		}

		update |= sweep_line_delete (&sweep_line, stop);
		stop = rectangle_peek_stop (&sweep_line);
	    }

	    if (update) {
		active_edges_to_traps (&sweep_line);
		update = FALSE;
	    }

	    sweep_line.current_y = rectangle->top;
	}

	update |= sweep_line_insert (&sweep_line, rectangle);
    } while ((rectangle = rectangle_pop_start (&sweep_line)) != NULL);

    while ((rectangle = rectangle_peek_stop (&sweep_line)) != NULL) {
	if (rectangle->bottom != sweep_line.current_y) {
	    if (update) {
		active_edges_to_traps (&sweep_line);
		update = FALSE;
	    }

	    sweep_line.current_y = rectangle->bottom;
	}

	update |= sweep_line_delete (&sweep_line, rectangle);
    }

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_bentley_ottmann_tessellate_rectangular_traps (cairo_traps_t *traps,
						     cairo_fill_rule_t fill_rule)
{
    rectangle_t stack_rectangles[CAIRO_STACK_ARRAY_LENGTH (rectangle_t)];
    rectangle_t *stack_rectangles_ptrs[ARRAY_LENGTH (stack_rectangles) + 3];
    rectangle_t *rectangles, **rectangles_ptrs;
    cairo_status_t status;
    int i;

    if (unlikely (traps->num_traps <= 1))
	return CAIRO_STATUS_SUCCESS;

    assert (traps->is_rectangular);

    dump_traps (traps, "bo-rects-traps-in.txt");

    rectangles = stack_rectangles;
    rectangles_ptrs = stack_rectangles_ptrs;
    if (traps->num_traps > ARRAY_LENGTH (stack_rectangles)) {
	rectangles = _cairo_malloc_ab_plus_c (traps->num_traps,
					      sizeof (rectangle_t) +
					      sizeof (rectangle_t *),
					      3*sizeof (rectangle_t *));
	if (unlikely (rectangles == NULL))
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);

	rectangles_ptrs = (rectangle_t **) (rectangles + traps->num_traps);
    }

    for (i = 0; i < traps->num_traps; i++) {
	if (traps->traps[i].left.p1.x < traps->traps[i].right.p1.x) {
	    rectangles[i].left.x = traps->traps[i].left.p1.x;
	    rectangles[i].left.dir = 1;

	    rectangles[i].right.x = traps->traps[i].right.p1.x;
	    rectangles[i].right.dir = -1;
	} else {
	    rectangles[i].right.x = traps->traps[i].left.p1.x;
	    rectangles[i].right.dir = 1;

	    rectangles[i].left.x = traps->traps[i].right.p1.x;
	    rectangles[i].left.dir = -1;
	}

	rectangles[i].left.right = NULL;
	rectangles[i].right.right = NULL;

	rectangles[i].top = traps->traps[i].top;
	rectangles[i].bottom = traps->traps[i].bottom;

	rectangles_ptrs[i+2] = &rectangles[i];
    }
    /* XXX incremental sort */
    _rectangle_sort (rectangles_ptrs+2, i);

    _cairo_traps_clear (traps);
    status = _cairo_bentley_ottmann_tessellate_rectangular (rectangles_ptrs+2, i,
							    fill_rule,
							    TRUE, traps);
    traps->is_rectilinear = TRUE;
    traps->is_rectangular = TRUE;

    if (rectangles != stack_rectangles)
	free (rectangles);

    dump_traps (traps, "bo-rects-traps-out.txt");

    return status;
}

cairo_status_t
_cairo_bentley_ottmann_tessellate_boxes (const cairo_boxes_t *in,
					 cairo_fill_rule_t fill_rule,
					 cairo_boxes_t *out)
{
    rectangle_t stack_rectangles[CAIRO_STACK_ARRAY_LENGTH (rectangle_t)];
    rectangle_t *stack_rectangles_ptrs[ARRAY_LENGTH (stack_rectangles) + 3];
    rectangle_t *rectangles, **rectangles_ptrs;
    rectangle_t *stack_rectangles_chain[CAIRO_STACK_ARRAY_LENGTH (rectangle_t *) ];
    rectangle_t **rectangles_chain;
    const struct _cairo_boxes_chunk *chunk;
    cairo_status_t status;
    int i, j, y_min, y_max;

    if (unlikely (in->num_boxes == 0)) {
	_cairo_boxes_clear (out);
	return CAIRO_STATUS_SUCCESS;
    }

    if (in->num_boxes == 1) {
	if (in == out) {
	    cairo_box_t *box = &in->chunks.base[0];

	    if (box->p1.x > box->p2.x) {
		cairo_fixed_t tmp = box->p1.x;
		box->p1.x = box->p2.x;
		box->p2.x = tmp;
	    }
	} else {
	    cairo_box_t box = in->chunks.base[0];

	    if (box.p1.x > box.p2.x) {
		cairo_fixed_t tmp = box.p1.x;
		box.p1.x = box.p2.x;
		box.p2.x = tmp;
	    }

	    _cairo_boxes_clear (out);
	    status = _cairo_boxes_add (out, CAIRO_ANTIALIAS_DEFAULT, &box);
	    assert (status == CAIRO_STATUS_SUCCESS);
	}
	return CAIRO_STATUS_SUCCESS;
    }

    y_min = INT_MAX; y_max = INT_MIN;
    for (chunk = &in->chunks; chunk != NULL; chunk = chunk->next) {
	const cairo_box_t *box = chunk->base;
	for (i = 0; i < chunk->count; i++) {
	    if (box[i].p1.y < y_min)
		y_min = box[i].p1.y;
	    if (box[i].p1.y > y_max)
		y_max = box[i].p1.y;
	}
    }
    y_min = _cairo_fixed_integer_floor (y_min);
    y_max = _cairo_fixed_integer_floor (y_max) + 1;
    y_max -= y_min;

    rectangles_chain = stack_rectangles_chain;
    if (y_max > ARRAY_LENGTH (stack_rectangles_chain)) {
	rectangles_chain = _cairo_malloc_ab (y_max, sizeof (rectangle_t *));
	if (unlikely (rectangles_chain == NULL))
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);
    }
    memset (rectangles_chain, 0, y_max * sizeof (rectangle_t*));

    rectangles = stack_rectangles;
    rectangles_ptrs = stack_rectangles_ptrs;
    if (in->num_boxes > ARRAY_LENGTH (stack_rectangles)) {
	rectangles = _cairo_malloc_ab_plus_c (in->num_boxes,
					      sizeof (rectangle_t) +
					      sizeof (rectangle_t *),
					      3*sizeof (rectangle_t *));
	if (unlikely (rectangles == NULL)) {
	    if (rectangles_chain != stack_rectangles_chain)
		free (rectangles_chain);
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);
	}

	rectangles_ptrs = (rectangle_t **) (rectangles + in->num_boxes);
    }

    j = 0;
    for (chunk = &in->chunks; chunk != NULL; chunk = chunk->next) {
	const cairo_box_t *box = chunk->base;
	for (i = 0; i < chunk->count; i++) {
	    int h;

	    if (box[i].p1.x < box[i].p2.x) {
		rectangles[j].left.x = box[i].p1.x;
		rectangles[j].left.dir = 1;

		rectangles[j].right.x = box[i].p2.x;
		rectangles[j].right.dir = -1;
	    } else {
		rectangles[j].right.x = box[i].p1.x;
		rectangles[j].right.dir = 1;

		rectangles[j].left.x = box[i].p2.x;
		rectangles[j].left.dir = -1;
	    }

	    rectangles[j].left.right = NULL;
	    rectangles[j].right.right = NULL;

	    rectangles[j].top = box[i].p1.y;
	    rectangles[j].bottom = box[i].p2.y;

	    h = _cairo_fixed_integer_floor (box[i].p1.y) - y_min;
	    rectangles[j].left.next = (edge_t *)rectangles_chain[h];
	    rectangles_chain[h] = &rectangles[j];
	    j++;
	}
    }

    j = 2;
    for (y_min = 0; y_min < y_max; y_min++) {
	rectangle_t *r;
	int start = j;
	for (r = rectangles_chain[y_min]; r; r = (rectangle_t *)r->left.next)
	    rectangles_ptrs[j++] = r;
	if (j > start + 1)
		_rectangle_sort (rectangles_ptrs + start, j - start);
    }

    if (rectangles_chain != stack_rectangles_chain)
	free (rectangles_chain);

    _cairo_boxes_clear (out);
    status = _cairo_bentley_ottmann_tessellate_rectangular (rectangles_ptrs+2, j-2,
							    fill_rule,
							    FALSE, out);
    if (rectangles != stack_rectangles)
	free (rectangles);

    return status;
}
