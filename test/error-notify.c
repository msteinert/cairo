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

#include <stdlib.h>

#include "cairo-test.h"

cairo_test_t test = {
    "error-notify",
    "Tests calls to cairo_error_notify",
    0, 0
};

typedef struct test_result {
    cairo_test_status_t status;
} test_result_t;

static void
toggle_status (void *closure, cairo_status_t status)
{
    test_result_t *result = closure;

    if (result->status == CAIRO_TEST_SUCCESS)
	result->status = CAIRO_TEST_FAILURE;
    else
	result->status = CAIRO_TEST_SUCCESS;
}

static cairo_test_status_t
do_test (cairo_t *cr, int width, int height)
{
    cairo_t *cr2;
    test_result_t result;

    /* We do all our testing in an alternate cr so that we don't leave
     * an error in the original. */

    cr2 = cairo_create (cairo_get_target (cr));

    cairo_set_error_notify (cr2, toggle_status, &result);

    cairo_test_log ("  Testing that error notify function is called:\t\t");
    {
	result.status = CAIRO_TEST_FAILURE;
	/* Trigger an error by trying to set a NULL source. */
	cairo_set_source (cr2, NULL);
	if (result.status != CAIRO_TEST_SUCCESS) {
	    cairo_test_log ("FAIL\n");
	    return CAIRO_TEST_FAILURE;
	} else {
	    cairo_test_log ("PASS\n");
	}
    }

    cairo_test_log ("  Testing that cr remains in an error state:\t\t");
    {
	result.status = CAIRO_TEST_FAILURE;
	cairo_move_to (cr2, 0.0, 0.0);
	if (result.status != CAIRO_TEST_SUCCESS) {
	    cairo_test_log ("FAIL\n");
	    return CAIRO_TEST_FAILURE;
	} else {
	    cairo_test_log ("PASS\n");
	}
    }

    cairo_test_log ("  Testing that error notify function can be cleared:\t");
    {
	cairo_set_error_notify (cr2, NULL, NULL);
	cairo_set_source (cr2, NULL);
	if (result.status != CAIRO_TEST_SUCCESS) {
	    cairo_test_log ("FAIL\n");
	    return CAIRO_TEST_FAILURE;
	} else {
	    cairo_test_log ("PASS\n");
	}
    }
    
    return CAIRO_TEST_SUCCESS;
}

int
main (void)
{
    return cairo_test (&test, do_test);
}
