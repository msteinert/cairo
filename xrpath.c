/*
 * $XFree86: $
 *
 * Copyright © 2002 Carl D. Worth
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of Carl
 * D. Worth not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission.  Carl D. Worth makes no representations about the
 * suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * CARL D. WORTH DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL CARL D. WORTH BE LIABLE FOR ANY SPECIAL,
 * INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
 * IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdlib.h>
#include "xrint.h"

/* private functions */
static void
_XrPathAddOpBuf(XrPath *path, XrPathOpBuf *op);

void
_XrPathNewOpBuf(XrPath *path);

static void
_XrPathAddArgBuf(XrPath *path, XrPathArgBuf *arg);

void
_XrPathNewArgBuf(XrPath *path);

XrPathOpBuf *
_XrPathOpBufCreate(void);

void
_XrPathOpBufDestroy(XrPathOpBuf *buf);

void
_XrPathOpBufAdd(XrPathOpBuf *op_buf, XrPathOp op);

XrPathArgBuf *
_XrPathArgBufCreate(void);

void
_XrPathArgBufDestroy(XrPathArgBuf *buf);

void
_XrPathArgBufAdd(XrPathArgBuf *arg, XPointFixed *pts, int num_pts);

static void
_TranslatePointFixed(XPointFixed *pt, XPointFixed *offset);


XrPath *
XrPathCreate(void)
{
    XrPath *path;

    path = malloc(sizeof(XrPath));
    XrPathInit(path);

    return path;
}

void
XrPathInit(XrPath *path)
{
    path->op_head = NULL;
    path->op_tail = NULL;

    path->arg_head = NULL;
    path->arg_tail = NULL;
}

void
XrPathInitCopy(XrPath *path, XrPath *other)
{
    XrPathOpBuf *op, *other_op;
    XrPathArgBuf *arg, *other_arg;

    XrPathInit(path);

    for (other_op = other->op_head; other_op; other_op = other_op->next) {
	op = _XrPathOpBufCreate();
	*op = *other_op;
	_XrPathAddOpBuf(path, op);
    }

    for (other_arg = other->arg_head; other_arg; other_arg = other_arg->next) {
	arg = _XrPathArgBufCreate();
	*arg = *other_arg;
	_XrPathAddArgBuf(path, arg);
    }
}

void
XrPathDeinit(XrPath *path)
{
    XrPathOpBuf *op;
    XrPathArgBuf *arg;

    while (path->op_head) {
	op = path->op_head;
	path->op_head = op->next;
	_XrPathOpBufDestroy(op);
    }
    path->op_tail = NULL;

    while (path->arg_head) {
	arg = path->arg_head;
	path->arg_head = arg->next;
	_XrPathArgBufDestroy(arg);
    }
    path->arg_tail = NULL;
}

void
XrPathDestroy(XrPath *path)
{
    XrPathDeinit(path);
    free(path);
}

static void
_XrPathAddOpBuf(XrPath *path, XrPathOpBuf *op)
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

void
_XrPathNewOpBuf(XrPath *path)
{
    XrPathOpBuf *op;

    op = _XrPathOpBufCreate();
    _XrPathAddOpBuf(path, op);
}

static void
_XrPathAddArgBuf(XrPath *path, XrPathArgBuf *arg)
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

void
_XrPathNewArgBuf(XrPath *path)
{
    XrPathArgBuf *arg;

    arg = _XrPathArgBufCreate();
    _XrPathAddArgBuf(path, arg);
}


void
XrPathAdd(XrPath *path, XrPathOp op, XPointFixed *pts, int num_pts)
{
    if (path->op_tail == NULL || path->op_tail->num_ops + 1 > XR_PATH_BUF_SZ) {
	_XrPathNewOpBuf(path);
    }
    _XrPathOpBufAdd(path->op_tail, op);

    if (path->arg_tail == NULL || path->arg_tail->num_pts + num_pts > XR_PATH_BUF_SZ) {
	_XrPathNewArgBuf(path);
    }
    _XrPathArgBufAdd(path->arg_tail, pts, num_pts);
}

XrPathOpBuf *
_XrPathOpBufCreate(void)
{
    XrPathOpBuf *op;

    op = malloc(sizeof(XrPathOpBuf));

    op->num_ops = 0;
    op->next = NULL;

    return op;
}

void
_XrPathOpBufDestroy(XrPathOpBuf *op)
{
    op->num_ops = 0;
    free(op);
}

void
_XrPathOpBufAdd(XrPathOpBuf *op_buf, XrPathOp op)
{
    op_buf->op[op_buf->num_ops++] = op;
}

XrPathArgBuf *
_XrPathArgBufCreate(void)
{
    XrPathArgBuf *arg;

    arg = malloc(sizeof(XrPathArgBuf));

    arg->num_pts = 0;
    arg->next = NULL;

    return arg;
}

void
_XrPathArgBufDestroy(XrPathArgBuf *arg)
{
    arg->num_pts = 0;
    free(arg);
}

void
_XrPathArgBufAdd(XrPathArgBuf *arg, XPointFixed *pts, int num_pts)
{
    int i;

    for (i=0; i < num_pts; i++) {
	arg->pt[arg->num_pts++] = pts[i];
    }
}

static void
_TranslatePointFixed(XPointFixed *pt, XPointFixed *offset)
{
    pt->x += offset->x;
    pt->y += offset->y;
}

void
XrPathStrokeTraps(XrPath *path, XrGState *gstate, XrTraps *traps)
{
    static XrPathCallbacks cb = { XrStrokerAddEdge };
    XrStroker stroker;

    XrStrokerInit(&stroker, gstate, traps);

    XrPathInterpret(path, XrPathDirectionForward, &cb, &stroker);

    XrStrokerDeinit(&stroker);
}

void
XrPathFillTraps(XrPath *path, XrTraps *traps, int winding)
{
    static XrPathCallbacks cb = { XrPolygonAddEdge };
    XrPolygon polygon;

    XrPolygonInit(&polygon);

    XrPathInterpret(path, XrPathDirectionForward, &cb, &polygon);
    XrTrapsTessellatePolygon(traps, &polygon, winding);

    XrPolygonDeinit(&polygon);
}

#define START_ARGS(n)			\
{				       	\
    if (dir != XrPathDirectionForward)	\
    {					\
	if (arg_i == 0) {		\
	    arg_buf = arg_buf->prev;	\
	    arg_i = arg_buf->num_pts;	\
	}				\
	arg_i -= n;			\
    }					\
}					

#define NEXT_ARG(pt)			\
{					\
    (pt) = arg_buf->pt[arg_i];		\
    arg_i++;				\
    if (arg_i >= arg_buf->num_pts) {	\
	arg_buf = arg_buf->next;	\
	arg_i = 0;			\
    }					\
}

#define END_ARGS(n)			\
{				       	\
    if (dir != XrPathDirectionForward)	\
    {					\
	arg_i -= n;			\
    }					\
}

void
XrPathInterpret(XrPath *path, XrPathDirection dir, XrPathCallbacks *cb, void *closure)
{
    int i;
    XrPathOpBuf *op_buf;
    XrPathOp op;
    XrPathArgBuf *arg_buf = path->arg_head;
    int arg_i = 0;
    XPointFixed pt;
    XPointFixed current = {0, 0};
    XPointFixed first = {0, 0};
    int has_current = 0;
    int step = (dir == XrPathDirectionForward) ? 1 : -1;

    for (op_buf = (dir == XrPathDirectionForward) ? path->op_head : path->op_tail;
	 op_buf;
	 op_buf = (dir == XrPathDirectionForward) ? op_buf->next : op_buf->prev)
    {
	int start, stop;
	if (dir == XrPathDirectionForward)
	{
	    start = 0;
	    stop = op_buf->num_ops;
	} else {
	    start = op_buf->num_ops - 1;
	    stop = -1;
	}
	    
	for (i=start; i != stop; i += step)
	{
	    op = op_buf->op[i];

	    switch (op) {
	    case XrPathOpMoveTo:
		START_ARGS(1);
		NEXT_ARG(pt);
		END_ARGS(1);
		first = pt;
		current = pt;
		has_current = 1;
		break;
	    case XrPathOpLineTo:
		START_ARGS(1);
		NEXT_ARG(pt);
		END_ARGS(1);
		if (has_current) {
		    (*cb->AddEdge)(closure, &current, &pt);
		    current = pt;
		} else {
		    first = pt;
		    current = pt;
		    has_current = 1;
		}
		break;
	    case XrPathOpRelMoveTo:
		START_ARGS(1);
		NEXT_ARG(pt);
		END_ARGS(1);
		_TranslatePointFixed(&pt, &current);
		first = pt;
		current = pt;
		has_current = 1;
		break;
	    case XrPathOpRelLineTo:
		START_ARGS(1);
		NEXT_ARG(pt);
		END_ARGS(1);
		_TranslatePointFixed(&pt, &current);
		if (has_current) {
		    (*cb->AddEdge)(closure, &current, &pt);
		    current = pt;
		} else {
		    first = pt;
		    current = pt;
		    has_current = 1;
		}
		break;
	    case XrPathOpClosePath:
		(*cb->AddEdge)(closure, &current, &first);
		current.x = 0;
		current.y = 0;
		first.x = 0;
		first.y = 0;
		has_current = 0;
		break;
	    }

	}
    }
}
