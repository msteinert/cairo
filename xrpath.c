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

static XrError
_XrPathNewOpBuf(XrPath *path);

static void
_XrPathAddArgBuf(XrPath *path, XrPathArgBuf *arg);

static XrError
_XrPathNewArgBuf(XrPath *path);

static XrPathOpBuf *
_XrPathOpBufCreate(void);

static void
_XrPathOpBufDestroy(XrPathOpBuf *buf);

static void
_XrPathOpBufAdd(XrPathOpBuf *op_buf, XrPathOp op);

static XrPathArgBuf *
_XrPathArgBufCreate(void);

static void
_XrPathArgBufDestroy(XrPathArgBuf *buf);

static void
_XrPathArgBufAdd(XrPathArgBuf *arg, XPointFixed *pts, int num_pts);

static void
_TranslatePointFixed(XPointFixed *pt, XPointFixed *offset);


void
XrPathInit(XrPath *path)
{
    path->op_head = NULL;
    path->op_tail = NULL;

    path->arg_head = NULL;
    path->arg_tail = NULL;
}

XrError
XrPathInitCopy(XrPath *path, XrPath *other)
{
    XrPathOpBuf *op, *other_op;
    XrPathArgBuf *arg, *other_arg;

    XrPathInit(path);

    for (other_op = other->op_head; other_op; other_op = other_op->next) {
	op = _XrPathOpBufCreate();
	if (op == NULL) {
	    return XrErrorNoMemory;
	}
	*op = *other_op;
	_XrPathAddOpBuf(path, op);
    }

    for (other_arg = other->arg_head; other_arg; other_arg = other_arg->next) {
	arg = _XrPathArgBufCreate();
	if (arg == NULL) {
	    return XrErrorNoMemory;
	}
	*arg = *other_arg;
	_XrPathAddArgBuf(path, arg);
    }

    return XrErrorSuccess;
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

static XrError
_XrPathNewOpBuf(XrPath *path)
{
    XrPathOpBuf *op;

    op = _XrPathOpBufCreate();
    if (op == NULL)
	return XrErrorNoMemory;

    _XrPathAddOpBuf(path, op);

    return XrErrorSuccess;
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

static XrError
_XrPathNewArgBuf(XrPath *path)
{
    XrPathArgBuf *arg;

    arg = _XrPathArgBufCreate();

    if (arg == NULL)
	return XrErrorNoMemory;

    _XrPathAddArgBuf(path, arg);

    return XrErrorSuccess;
}


XrError
XrPathAdd(XrPath *path, XrPathOp op, XPointFixed *pts, int num_pts)
{
    XrError err;

    if (path->op_tail == NULL || path->op_tail->num_ops + 1 > XR_PATH_BUF_SZ) {
	err = _XrPathNewOpBuf(path);
	if (err)
	    return err;
    }
    _XrPathOpBufAdd(path->op_tail, op);

    if (path->arg_tail == NULL || path->arg_tail->num_pts + num_pts > XR_PATH_BUF_SZ) {
	err = _XrPathNewArgBuf(path);
	if (err)
	    return err;
    }
    _XrPathArgBufAdd(path->arg_tail, pts, num_pts);

    return XrErrorSuccess;
}

static XrPathOpBuf *
_XrPathOpBufCreate(void)
{
    XrPathOpBuf *op;

    op = malloc(sizeof(XrPathOpBuf));

    if (op) {
	op->num_ops = 0;
	op->next = NULL;
    }

    return op;
}

static void
_XrPathOpBufDestroy(XrPathOpBuf *op)
{
    op->num_ops = 0;
    free(op);
}

static void
_XrPathOpBufAdd(XrPathOpBuf *op_buf, XrPathOp op)
{
    op_buf->op[op_buf->num_ops++] = op;
}

static XrPathArgBuf *
_XrPathArgBufCreate(void)
{
    XrPathArgBuf *arg;

    arg = malloc(sizeof(XrPathArgBuf));

    if (arg) {
	arg->num_pts = 0;
	arg->next = NULL;
    }

    return arg;
}

static void
_XrPathArgBufDestroy(XrPathArgBuf *arg)
{
    arg->num_pts = 0;
    free(arg);
}

static void
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

XrError
XrPathInterpret(XrPath *path, XrPathDirection dir, XrPathCallbacks *cb, void *closure)
{
    XrError err;
    int i;
    XrPathOpBuf *op_buf;
    XrPathOp op;
    XrPathArgBuf *arg_buf = path->arg_head;
    int arg_i = 0;
    XPointFixed pt;
    XPointFixed current = {0, 0};
    XPointFixed first = {0, 0};
    int has_current = 0;
    int has_edge = 0;
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
		if (has_edge) {
		    err = (*cb->DoneSubPath) (closure, XrSubPathDoneCap);
		    if (err)
			return err;
		}
		START_ARGS(1);
		NEXT_ARG(pt);
		END_ARGS(1);
		first = pt;
		current = pt;
		has_current = 1;
		has_edge = 0;
		break;
	    case XrPathOpLineTo:
		START_ARGS(1);
		NEXT_ARG(pt);
		END_ARGS(1);
		if (has_current) {
		    err = (*cb->AddEdge)(closure, &current, &pt);
		    if (err)
			return err;
		    current = pt;
		    has_edge = 1;
		} else {
		    first = pt;
		    current = pt;
		    has_current = 1;
		}
		break;
	    case XrPathOpRelMoveTo:
		if (has_edge) {
		    err = (*cb->DoneSubPath) (closure, XrSubPathDoneCap);
		    if (err)
			return err;
		}
		START_ARGS(1);
		NEXT_ARG(pt);
		END_ARGS(1);
		_TranslatePointFixed(&pt, &current);
		first = pt;
		current = pt;
		has_current = 1;
		has_edge = 0;
		break;
	    case XrPathOpRelLineTo:
		START_ARGS(1);
		NEXT_ARG(pt);
		END_ARGS(1);
		_TranslatePointFixed(&pt, &current);
		if (has_current) {
		    err = (*cb->AddEdge)(closure, &current, &pt);
		    if (err)
			return err;
		    current = pt;
		    has_edge = 1;
		} else {
		    first = pt;
		    current = pt;
		    has_current = 1;
		}
		break;
	    case XrPathOpClosePath:
		if (has_edge) {
		    (*cb->AddEdge)(closure, &current, &first);
		    (*cb->DoneSubPath) (closure, XrSubPathDoneJoin);
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
        (*cb->DoneSubPath) (closure, XrSubPathDoneCap);

    return XrErrorSuccess;
}
