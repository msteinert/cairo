/***********************************************************

Copyright 1987, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.


Copyright 1987 by Digital Equipment Corporation, Maynard, Massachusetts.

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its 
documentation for any purpose and without fee is hereby granted, 
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in 
supporting documentation, and that the name of Digital not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.  

DIGITAL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

******************************************************************/
/* $Id: pixman.h,v 1.3 2003-04-25 20:21:42 cworth Exp $ */

#ifndef PIXREGION_H
#define PIXREGION_H

typedef struct _PixRegion PixRegion;

typedef struct _PixRegionBox {
    short x1, y1, x2, y2;
} PixRegionBox;

typedef enum {
    PixRegionStatusFailure,
    PixRegionStatusSuccess
} PixRegionStatus;

/* creation/destruction */

PixRegion *
PixRegionCreate (void);

PixRegion *
PixRegionCreateSimple (PixRegionBox *extents);

void
PixRegionDestroy (PixRegion *region);

/* manipulation */

void
PixRegionTranslate (PixRegion *region, int x, int y);

PixRegionStatus
PixRegionCopy (PixRegion *dest, PixRegion *source);

PixRegionStatus
PixRegionIntersect (PixRegion *newReg, PixRegion *reg1, PixRegion *reg2);

PixRegionStatus
PixRegionUnion (PixRegion *newReg, PixRegion *reg1, PixRegion *reg2);

PixRegionStatus
PixRegionUnionRect(PixRegion *dest, PixRegion *source,
		   int x, int y, unsigned int width, unsigned int height);

PixRegionStatus
PixRegionSubtract (PixRegion *regD, PixRegion *regM, PixRegion *regS);

PixRegionStatus
PixRegionInverse (PixRegion *newReg, PixRegion *reg1, PixRegionBox *invRect);

/* XXX: Need to fix this so it doesn't depend on an X data structure
PixRegion *
RectsToPixRegion (int nrects, xRectanglePtr prect, int ctype);
*/

/* querying */

/* XXX: These should proably be combined: PixRegionGetRects? */
int
PixRegionNumRects (PixRegion *region);

PixRegionBox *
PixRegionRects (PixRegion *region);

/* XXX: Change to an enum */
#define rgnOUT 0
#define rgnIN  1
#define rgnPART 2

int
PixRegionPointInRegion (PixRegion *region, int x, int y, PixRegionBox *box);

int
PixRegionRectIn (PixRegion *PixRegion, PixRegionBox *prect);

int
PixRegionNotEmpty (PixRegion *region);

PixRegionBox *
PixRegionExtents (PixRegion *region);

/* mucking around */

/* WARNING: calling PixRegionAppend may leave dest as an invalid
   region. Follow-up with PixRegionValidate to fix it up. */
PixRegionStatus
PixRegionAppend (PixRegion *dest, PixRegion *region);

PixRegionStatus
PixRegionValidate (PixRegion *badreg, int *pOverlap);

/* Unclassified functionality
 * XXX: Do all of these need to be exported?
 */

void
PixRegionReset (PixRegion *region, PixRegionBox *pBox);

void
PixRegionEmpty (PixRegion *region);

#endif /* PIXREGION_H */
