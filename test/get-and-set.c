/*
 * Copyright © 2005 Red Hat, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of
 * Red Hat, Inc. not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission. Red Hat, Inc. makes no representations about the
 * suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * RED HAT, INC. DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL RED HAT, INC. BE LIABLE FOR ANY SPECIAL,
 * INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
 * IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author: Carl D. Worth <cworth@cworth.org>
 */

#include "cairo_test.h"

cairo_test_t test = {
    "get_and_set",
    "Tests calls to the most trivial cairo_get and cairo_set functions",
    0, 0
};

typedef struct {
    cairo_operator_t operator;
    double tolerance;
    double point_x;
    double point_y;
    cairo_fill_rule_t fill_rule;
    double line_width;
    cairo_line_cap_t line_cap;
    cairo_line_join_t line_join;
    double miter_limit;
    /* XXX: Add cairo_matrix_t here when it is exposed */
} settings_t;

/* Two sets of settings, no defaults */
settings_t settings[] = {
    {
	CAIRO_OPERATOR_IN,
	2.0,
	12.3,
	4.56,
	CAIRO_FILL_RULE_EVEN_ODD,
	7.7,
	CAIRO_LINE_CAP_SQUARE,
	CAIRO_LINE_JOIN_ROUND,
	3.14
    },
    {
	CAIRO_OPERATOR_ATOP,
	5.25,
	99.99,
	0.001,
	CAIRO_FILL_RULE_WINDING,
	2.17,
	CAIRO_LINE_CAP_ROUND,
	CAIRO_LINE_JOIN_BEVEL,
	1000.0
    }
};

static void
settings_set (cairo_t *cr, settings_t *settings)
{
    cairo_set_operator (cr, settings->operator);
    cairo_set_tolerance (cr, settings->tolerance);
    cairo_move_to (cr, settings->point_x, settings->point_y);
    cairo_set_fill_rule (cr, settings->fill_rule);
    cairo_set_line_width (cr, settings->line_width);
    cairo_set_line_cap (cr, settings->line_cap);
    cairo_set_line_join (cr, settings->line_join);
    cairo_set_miter_limit (cr, settings->miter_limit);
}

static void
settings_get (cairo_t *cr, settings_t *settings)
{
    settings->operator = cairo_get_operator (cr);
    settings->tolerance = cairo_get_tolerance (cr);
    cairo_get_current_point (cr, &settings->point_x, &settings->point_y);
    settings->fill_rule = cairo_get_fill_rule (cr);
    settings->line_width = cairo_get_line_width (cr);
    settings->line_cap = cairo_get_line_cap (cr);
    settings->line_join = cairo_get_line_join (cr);
    settings->miter_limit = cairo_get_miter_limit (cr);
}

/* Maximum error is one part of our fixed-point grid */
#define EPSILON (1.0 / 65536.0)

static int
DOUBLES_WITHIN_EPSILON(double a, double b) {
    double delta = fabs(a - b);
    return delta < EPSILON;
}

static int
settings_equal (settings_t *a, settings_t *b)
{
    return (a->operator == b->operator &&
	    a->tolerance == b->tolerance &&
	    DOUBLES_WITHIN_EPSILON (a->point_x, b->point_x) &&
	    DOUBLES_WITHIN_EPSILON (a->point_y, b->point_y) &&
	    a->fill_rule == b->fill_rule &&
	    a->line_width == b->line_width &&
	    a->line_cap == b->line_cap &&
	    a->line_join == b->line_join &&
	    a->miter_limit == b->miter_limit);
}

static cairo_test_status_t
get_and_set (cairo_t *cr, int width, int height)
{
    settings_t check;

    settings_set (cr, &settings[0]);
    
    cairo_save (cr);
    {
	settings_set (cr, &settings[1]);
	settings_get (cr, &check);

	if (!settings_equal (&settings[1], &check))
	    return CAIRO_TEST_FAILURE;
    }
    cairo_restore (cr);

    settings_get (cr, &check);

    if (!settings_equal (&settings[0], &check))
	return CAIRO_TEST_FAILURE;

    return CAIRO_TEST_SUCCESS;
}

int
main (void)
{
    return cairo_test (&test, get_and_set);
}
