/*
 * Copyright © 2002 USC, Information Sciences Institute
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of the
 * University of Southern California not be used in advertising or
 * publicity pertaining to distribution of the software without
 * specific, written prior permission. The University of Southern
 * California makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 *
 * THE UNIVERSITY OF SOUTHERN CALIFORNIA DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL THE UNIVERSITY OF
 * SOUTHERN CALIFORNIA BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author: Carl D. Worth <cworth@isi.edu>
 */

#include <stdlib.h>
#include "cairoint.h"

/* private functions */
static cairo_status_t
_cairo_path_add (cairo_path_t *path, cairo_path_op_t op, cairo_point_t *points, int num_pts);

static void
_cairo_path_add_op_buf (cairo_path_t *path, cairo_path_op_buf_t *op);

static cairo_status_t
_cairo_path_new_op_buf (cairo_path_t *path);

static void
_cairo_path_add_arg_buf (cairo_path_t *path, cairo_path_arg_buf_t *arg);

static cairo_status_t
_cairo_path_new_arg_buf (cairo_path_t *path);

static cairo_path_op_buf_t *
_cairo_path_op_buf_create (void);

static void
_cairo_path_op_buf_destroy (cairo_path_op_buf_t *buf);

static void
_cairo_path_op_buf_add (cairo_path_op_buf_t *op_buf, cairo_path_op_t op);

static cairo_path_arg_buf_t *
_cairo_path_arg_buf_create (void);

static void
_cairo_path_arg_buf_destroy (cairo_path_arg_buf_t *buf);

static void
_cairo_path_arg_buf_add (cairo_path_arg_buf_t *arg, cairo_point_t *points, int num_points);

void
_cairo_path_init (cairo_path_t *path)
{
    path->op_head = NULL;
    path->op_tail = NULL;

    path->arg_head = NULL;
    path->arg_tail = NULL;
}

cairo_status_t
_cairo_path_init_copy (cairo_path_t *path, cairo_path_t *other)
{
    cairo_path_op_buf_t *op, *other_op;
    cairo_path_arg_buf_t *arg, *other_arg;

    _cairo_path_init (path);

    for (other_op = other->op_head; other_op; other_op = other_op->next) {
	op = _cairo_path_op_buf_create ();
	if (op == NULL) {
	    return CAIRO_STATUS_NO_MEMORY;
	}
	*op = *other_op;
	_cairo_path_add_op_buf (path, op);
    }

    for (other_arg = other->arg_head; other_arg; other_arg = other_arg->next) {
	arg = _cairo_path_arg_buf_create ();
	if (arg == NULL) {
	    return CAIRO_STATUS_NO_MEMORY;
	}
	*arg = *other_arg;
	_cairo_path_add_arg_buf (path, arg);
    }

    return CAIRO_STATUS_SUCCESS;
}

void
_cairo_path_fini (cairo_path_t *path)
{
    cairo_path_op_buf_t *op;
    cairo_path_arg_buf_t *arg;

    while (path->op_head) {
	op = path->op_head;
	path->op_head = op->next;
	_cairo_path_op_buf_destroy (op);
    }
    path->op_tail = NULL;

    while (path->arg_head) {
	arg = path->arg_head;
	path->arg_head = arg->next;
	_cairo_path_arg_buf_destroy (arg);
    }
    path->arg_tail = NULL;
}

cairo_status_t
_cairo_path_move_to (cairo_path_t *path, double x, double y)
{
    cairo_point_t point;

    point.x = XDoubleToFixed (x);
    point.y = XDoubleToFixed (y);

    return _cairo_path_add (path, CAIRO_PATH_OP_MOVE_TO, &point, 1);
}

cairo_status_t
_cairo_path_line_to (cairo_path_t *path, double x, double y)
{
    cairo_point_t point;

    point.x = XDoubleToFixed (x);
    point.y = XDoubleToFixed (y);

    return _cairo_path_add (path, CAIRO_PATH_OP_LINE_TO, &point, 1);
}

cairo_status_t
_cairo_path_curve_to (cairo_path_t *path,
		      double x1, double y1,
		      double x2, double y2,
		      double x3, double y3)
{
    cairo_point_t point[3];

    point[0].x = XDoubleToFixed (x1);
    point[0].y = XDoubleToFixed (y1);

    point[1].x = XDoubleToFixed (x2);
    point[1].y = XDoubleToFixed (y2);

    point[2].x = XDoubleToFixed (x3);
    point[2].y = XDoubleToFixed (y3);

    return _cairo_path_add (path, CAIRO_PATH_OP_CURVE_TO, point, 3);
}

cairo_status_t
_cairo_path_close_path (cairo_path_t *path)
{
    return _cairo_path_add (path, CAIRO_PATH_OP_CLOSE_PATH, NULL, 0);
}

static cairo_status_t
_cairo_path_add (cairo_path_t *path, cairo_path_op_t op, cairo_point_t *points, int num_points)
{
    cairo_status_t status;

    if (path->op_tail == NULL || path->op_tail->num_ops + 1 > CAIRO_PATH_BUF_SZ) {
	status = _cairo_path_new_op_buf (path);
	if (status)
	    return status;
    }
    _cairo_path_op_buf_add (path->op_tail, op);

    if (path->arg_tail == NULL || path->arg_tail->num_points + num_points > CAIRO_PATH_BUF_SZ) {
	status = _cairo_path_new_arg_buf (path);
	if (status)
	    return status;
    }
    _cairo_path_arg_buf_add (path->arg_tail, points, num_points);

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_path_add_op_buf (cairo_path_t *path, cairo_path_op_buf_t *op)
{
    op->next = NULL;
    op->prev = path->op_tail;

    if (path->op_tail) {
	path->op_tail->next = op;
    } else {
	path->op_head = op;
    }

    path->op_tail = op;
}

static cairo_status_t
_cairo_path_new_op_buf (cairo_path_t *path)
{
    cairo_path_op_buf_t *op;

    op = _cairo_path_op_buf_create ();
    if (op == NULL)
	return CAIRO_STATUS_NO_MEMORY;

    _cairo_path_add_op_buf (path, op);

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_path_add_arg_buf (cairo_path_t *path, cairo_path_arg_buf_t *arg)
{
    arg->next = NULL;
    arg->prev = path->arg_tail;

    if (path->arg_tail) {
	path->arg_tail->next = arg;
    } else {
	path->arg_head = arg;
    }

    path->arg_tail = arg;
}

static cairo_status_t
_cairo_path_new_arg_buf (cairo_path_t *path)
{
    cairo_path_arg_buf_t *arg;

    arg = _cairo_path_arg_buf_create ();

    if (arg == NULL)
	return CAIRO_STATUS_NO_MEMORY;

    _cairo_path_add_arg_buf (path, arg);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_path_op_buf_t *
_cairo_path_op_buf_create (void)
{
    cairo_path_op_buf_t *op;

    op = malloc (sizeof (cairo_path_op_buf_t));

    if (op) {
	op->num_ops = 0;
	op->next = NULL;
    }

    return op;
}

static void
_cairo_path_op_buf_destroy (cairo_path_op_buf_t *op)
{
    free (op);
}

static void
_cairo_path_op_buf_add (cairo_path_op_buf_t *op_buf, cairo_path_op_t op)
{
    op_buf->op[op_buf->num_ops++] = op;
}

static cairo_path_arg_buf_t *
_cairo_path_arg_buf_create (void)
{
    cairo_path_arg_buf_t *arg;

    arg = malloc (sizeof (cairo_path_arg_buf_t));

    if (arg) {
	arg->num_points = 0;
	arg->next = NULL;
    }

    return arg;
}

static void
_cairo_path_arg_buf_destroy (cairo_path_arg_buf_t *arg)
{
    free (arg);
}

static void
_cairo_path_arg_buf_add (cairo_path_arg_buf_t *arg, cairo_point_t *points, int num_points)
{
    int i;

    for (i=0; i < num_points; i++) {
	arg->points[arg->num_points++] = points[i];
    }
}

#define CAIRO_PATH_OP_MAX_ARGS 3

static int const num_args[] = 
{
    1, /* cairo_path_move_to */
    1, /* cairo_path_op_line_to */
    3, /* cairo_path_op_curve_to */
    0, /* cairo_path_op_close_path */
};

cairo_status_t
_cairo_path_interpret (cairo_path_t *path, cairo_direction_t dir, const cairo_path_callbacks_t *cb, void *closure)
{
    cairo_status_t status;
    int i, arg;
    cairo_path_op_buf_t *op_buf;
    cairo_path_op_t op;
    cairo_path_arg_buf_t *arg_buf = path->arg_head;
    int buf_i = 0;
    cairo_point_t point[CAIRO_PATH_OP_MAX_ARGS];
    cairo_point_t current = {0, 0};
    cairo_point_t first = {0, 0};
    int has_current = 0;
    int has_edge = 0;
    int step = (dir == CAIRO_DIRECTION_FORWARD) ? 1 : -1;

    for (op_buf = (dir == CAIRO_DIRECTION_FORWARD) ? path->op_head : path->op_tail;
	 op_buf;
	 op_buf = (dir == CAIRO_DIRECTION_FORWARD) ? op_buf->next : op_buf->prev)
    {
	int start, stop;
	if (dir == CAIRO_DIRECTION_FORWARD) {
	    start = 0;
	    stop = op_buf->num_ops;
	} else {
	    start = op_buf->num_ops - 1;
	    stop = -1;
	}

	for (i=start; i != stop; i += step) {
	    op = op_buf->op[i];

	    if (dir == CAIRO_DIRECTION_REVERSE) {
		if (buf_i == 0) {
		    arg_buf = arg_buf->prev;
		    buf_i = arg_buf->num_points;
		}
		buf_i -= num_args[op];
	    }

	    for (arg = 0; arg < num_args[op]; arg++) {
		point[arg] = arg_buf->points[buf_i];
		buf_i++;
		if (buf_i >= arg_buf->num_points) {
		    arg_buf = arg_buf->next;
		    buf_i = 0;
		}
	    }

	    if (dir == CAIRO_DIRECTION_REVERSE) {
		buf_i -= num_args[op];
	    }

	    switch (op) {
	    case CAIRO_PATH_OP_MOVE_TO:
		if (has_edge) {
		    status = (*cb->done_sub_path) (closure, CAIRO_SUB_PATH_DONE_CAP);
		    if (status)
			return status;
		}
		first = point[0];
		current = point[0];
		has_current = 1;
		has_edge = 0;
		break;
	    case CAIRO_PATH_OP_LINE_TO:
		if (has_current) {
		    status = (*cb->add_edge) (closure, &current, &point[0]);
		    if (status)
			return status;
		    current = point[0];
		    has_edge = 1;
		} else {
		    first = point[0];
		    current = point[0];
		    has_current = 1;
		    has_edge = 0;
		}
		break;
	    case CAIRO_PATH_OP_CURVE_TO:
		if (has_current) {
		    status = (*cb->add_spline) (closure, &current, &point[0], &point[1], &point[2]);
		    if (status)
			return status;
		    current = point[2];
		    has_edge = 1;
		} else {
		    first = point[2];
		    current = point[2];
		    has_current = 1;
		    has_edge = 0;
		}
		break;
	    case CAIRO_PATH_OP_CLOSE_PATH:
		if (has_edge) {
		    (*cb->add_edge) (closure, &current, &first);
		    (*cb->done_sub_path) (closure, CAIRO_SUB_PATH_DONE_JOIN);
		}
		current.x = 0;
		current.y = 0;
		first.x = 0;
		first.y = 0;
		has_current = 0;
		has_edge = 0;
		break;
	    }
	}
    }
    if (has_edge)
        (*cb->done_sub_path) (closure, CAIRO_SUB_PATH_DONE_CAP);

    return (*cb->done_path) (closure);
}

