/*
 * Copyright © 2006 Red Hat, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of the
 * copyright holders not be used in advertising or publicity
 * pertaining to distribution of the software without specific,
 * written prior permission. The copyright holders make no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied
 * warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 *
 * Authors: Carl Worth <cworth@cworth.org>
 */

#include "cairo-perf.h"

/* We use _GNU_SOURCE for getline and strndup. */
#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <math.h>

/* We conditionally provide a custom implementation of getline and strndup
 * as needed. These aren't necessary full-fledged general purpose
 * implementations. They just get the job done for our purposes.
 */
#ifndef __USE_GNU

#define POORMANS_GETLINE_BUFFER_SIZE (65536)
ssize_t
getline (char **lineptr, size_t *n, FILE *stream)
{
    if (!*lineptr)
    {
        *n = POORMANS_GETLINE_BUFFER_SIZE;
        *lineptr = (char *) malloc (*n);
    }

    if (!fgets (*lineptr, *n, stream))
        return -1;

    if (!feof (stream) && !strchr (*lineptr, '\n'))
    {
        fprintf (stderr, "The poor man's implementation of getline in "
                          __FILE__ " needs a bigger buffer. Perhaps it's "
                         "time for a complete implementation of getline.\n");
        exit (0);
    }

    return strlen (*lineptr);
}
#undef POORMANS_GETLINE_BUFFER_SIZE

char *
strndup (const char *s, size_t n)
{
    size_t len;
    char *sdup;

    if (!s)
        return NULL;

    len = strlen (s);
    len = (n < len ? n : len);
    sdup = (char *) malloc (len + 1);
    if (sdup)
    {
        memcpy (sdup, s, len);
        sdup[len] = '\0';
    }

    return sdup;
}

#endif /* ifndef __USE_GNU */

typedef struct _test_report {
    int id;
    char *backend;
    char *content;
    char *name;
    int size;

    /* The samples only exists for "raw" reports */
    cairo_perf_ticks_t *samples;
    unsigned int samples_size;
    unsigned int samples_count;

    /* The stats are either read directly or computed from samples.
     * If the stats have not yet been computed from samples, then
     * iterations will be 0. */
    cairo_stats_t stats;
} test_report_t;

typedef struct _test_diff {
    test_report_t *old;
    test_report_t *new;
    double speedup;
} test_diff_t;

typedef struct _cairo_perf_report {
    const char *name;
    test_report_t *tests;
    int tests_size;
    int tests_count;
} cairo_perf_report_t;

typedef enum {
    TEST_REPORT_STATUS_SUCCESS,
    TEST_REPORT_STATUS_COMMENT,
    TEST_REPORT_STATUS_ERROR
} test_report_status_t;

/* Ad-hoc parsing, macros with a strong dependence on the calling
 * context, and plenty of other ugliness is here.  But at least it's
 * not perl... */
#define parse_error(...) fprintf(stderr, __VA_ARGS__); return TEST_REPORT_STATUS_ERROR;
#define skip_char(c)							\
do {									\
    if (*s && *s == (c)) {						\
	s++;								\
    } else {								\
	 parse_error ("expected '%c' but found '%c'", c, *s);		\
    }									\
} while (0)
#define skip_space() while (*s && (*s == ' ' || *s == '\t')) s++;
#define parse_int(result)						\
do {									\
    (result) = strtol (s, &end, 10);					\
    if (*s && end != s) {						\
	s = end;							\
    } else {								\
	parse_error("expected integer but found %s", s);		\
    }									\
} while (0)
#define parse_long_long(result)						\
do {									\
    (result) = strtoll (s, &end, 10);					\
    if (*s && end != s) {						\
	s = end;							\
    } else {								\
	parse_error("expected integer but found %s", s);		\
    }									\
} while (0)
#define parse_double(result)						\
do {									\
    (result) = strtod (s, &end);					\
    if (*s && end != s) {						\
	s = end;							\
    } else {								\
	parse_error("expected floating-point value but found %s", s);	\
    }									\
} while (0)
/* Here a string is simply a sequence of non-whitespace */
#define parse_string(result)						\
do {									\
    for (end = s; *end; end++)						\
	if (isspace (*end))						\
	    break;							\
    (result) = strndup (s, end - s);					\
    if ((result) == NULL) {						\
	fprintf (stderr, "Out of memory.\n");				\
	exit (1);							\
    }									\
    s = end;								\
} while (0)

static test_report_status_t
test_report_parse (test_report_t *report, char *line)
{
    char *end;
    char *s = line;
    cairo_bool_t is_raw = FALSE;
    double min_time, median_time;

    /* The code here looks funny unless you understand that these are
     * all macro calls, (and then the code just looks sick). */
    if (*s == '\n')
	return TEST_REPORT_STATUS_COMMENT;

    skip_char ('[');
    skip_space ();
    if (*s == '#')
	return TEST_REPORT_STATUS_COMMENT;
    if (*s == '*') {
	s++;
	is_raw = TRUE;
    } else {
	parse_int (report->id);
    }
    skip_char (']');

    skip_space ();

    parse_string (report->backend);
    end = strrchr (report->backend, '-');
    if (*end)
	*end++ = '\0';
    report->content = end;

    skip_space ();

    parse_string (report->name);
    end = strrchr (report->name, '-');
    if (*end)
	*end++ = '\0';
    report->size = atoi (end);

    skip_space ();

    report->samples = NULL;
    report->samples_size = 0;
    report->samples_count = 0;

    if (is_raw) {
	parse_double (report->stats.ticks_per_ms);
	skip_space ();

	report->samples_size = 5;
	report->samples = xmalloc (report->samples_size * sizeof (cairo_perf_ticks_t));
	do {
	    if (report->samples_count == report->samples_size) {
		report->samples_size *= 2;
		report->samples = xrealloc (report->samples,
					    report->samples_size * sizeof (cairo_perf_ticks_t));
	    }
	    parse_long_long (report->samples[report->samples_count++]);
	    skip_space ();
	} while (*s && *s != '\n');
	report->stats.iterations = 0;
	skip_char ('\n');
    } else {
	parse_double (report->stats.min_ticks);
	skip_space ();

	parse_double (min_time);
	report->stats.ticks_per_ms = report->stats.min_ticks / min_time;

	skip_space ();

	parse_double (median_time);
	report->stats.median_ticks = median_time * report->stats.ticks_per_ms;

	skip_space ();

	parse_double (report->stats.std_dev);
	report->stats.std_dev /= 100.0;
	skip_char ('%');

	skip_space ();

	parse_int (report->stats.iterations);

	skip_space ();
	skip_char ('\n');
    }

    return TEST_REPORT_STATUS_SUCCESS;
}

static void
cairo_perf_report_load (cairo_perf_report_t *report, const char *filename)
{
    FILE *file;
    test_report_status_t status;
    int line_number = 0;
    char *line = NULL;
    size_t line_size = 0;

    report->name = filename;
    report->tests = NULL;
    report->tests_size = 0;
    report->tests_count = 0;

    file = fopen (filename, "r");
    if (file == NULL) {
	fprintf (stderr, "Failed to open %s: %s\n",
		 filename, strerror (errno));
	exit (1);
    }

    while (1) {
	if (report->tests_count == report->tests_size) {
	    report->tests_size = report->tests_size ? 2 * report->tests_size : 16;
	    report->tests = realloc (report->tests,
				     report->tests_size * sizeof (test_report_t));
	    if (report->tests == NULL) {
		fprintf (stderr, "Out of memory.\n");
		exit (1);
	    }
	}

	line_number++;
	if (getline (&line, &line_size, file) == -1)
	    break;

	status = test_report_parse (&report->tests[report->tests_count], line);
	if (status == TEST_REPORT_STATUS_ERROR)
	    fprintf (stderr, "Ignoring unrecognized line %d of %s:\n%s",
		     line_number, filename, line);
	if (status == TEST_REPORT_STATUS_SUCCESS)
	    report->tests_count++;
	/* Do nothing on TEST_REPORT_STATUS_COMMENT */
    }

    if (line)
	free (line);
}

static int
test_diff_cmp (const void *a, const void *b)
{
    const test_diff_t *a_diff = a;
    const test_diff_t *b_diff = b;
    double a_change, b_change;

    a_change = a_diff->speedup;
    b_change = b_diff->speedup;

    /* First make all speedups come before all slowdowns. */
    if (a_change > 1.0 && b_change < 1.0)
	return -1;
    if (a_change < 1.0 && b_change > 1.0)
	return 1;

    /* Then, within each, sort by magnitude of speed change */
    if (a_change < 1.0)
	a_change = 1.0 / a_change;

    if (b_change < 1.0)
	b_change = 1.0 / b_change;

    /* Reverse sort so larger changes come first */
    if (a_change > b_change)
	return -1;
    if (a_change < b_change)
	return 1;
    return 0;
}

static int
test_report_cmp_backend_then_name (const void *a, const void *b)
{
    const test_report_t *a_test = a;
    const test_report_t *b_test = b;
    int cmp;

    cmp = strcmp (a_test->backend, b_test->backend);
    if (cmp)
	return cmp;

    cmp = strcmp (a_test->content, b_test->content);
    if (cmp)
	return cmp;

    cmp = strcmp (a_test->name, b_test->name);
    if (cmp)
	return cmp;

    if (a_test->size < b_test->size)
	return -1;
    if (a_test->size > b_test->size)
	return 1;
    return 0;
}

static void
cairo_perf_report_sort_and_compute_stats (cairo_perf_report_t *report)
{
    test_report_t *base, *next, *last, *t;

    /* First we sort, since the diff needs both lists in the same
     * order */
    qsort (report->tests, report->tests_count, sizeof (test_report_t),
	   test_report_cmp_backend_then_name);

    /* The sorting also brings all related raw reports together so we
     * can condense them and compute the stats.
     */
    base = &report->tests[0];
    last = &report->tests[report->tests_count - 1];
    while (base <= last) {
	next = base+1;
	if (next <= last) {
	    while (next <= last &&
		   test_report_cmp_backend_then_name (base, next) == 0)
	    {
		next++;
	    }
	    if (next != base) {
		unsigned int new_samples_count = base->samples_count;
		for (t = base + 1; t < next; t++)
		    new_samples_count += t->samples_count;
		if (new_samples_count > base->samples_size) {
		    base->samples_size = new_samples_count;
		    base->samples = xrealloc (base->samples,
					      base->samples_size * sizeof (cairo_perf_ticks_t));
		}
		for (t = base + 1; t < next; t++) {
		    memcpy (&base->samples[base->samples_count], t->samples,
			    t->samples_count * sizeof (cairo_perf_ticks_t));
		    base->samples_count += t->samples_count;
		}
	    }
	}
	if (base->samples)
	    _cairo_stats_compute (&base->stats, base->samples, base->samples_count);
	base = next;
    }
}

#define CHANGE_BAR_WIDTH 70
static void
print_change_bar (double change, double max_change)
{
    int units_per_cell = (int) ceil (max_change / CHANGE_BAR_WIDTH);

    /* For a 1.0x speedup we want a zero-size bar to show "no
     * change". */
    change -= 1.0;

    while (change > units_per_cell) {
	printf("█");
	change -= units_per_cell;
    }

    change /= units_per_cell;

    if (change > 7.5/8.0)
	printf("█");
    else if (change > 6.5/8.0)
	printf("▉");
    else if (change > 5.5/8.0)
	printf("▊");
    else if (change > 4.5/8.0)
	printf("▋");
    else if (change > 3.5/8.0)
	printf("▌");
    else if (change > 2.5/8.0)
	printf("▍");
    else if (change > 1.5/8.0)
	printf("▎");
    else if (change > 0.5/8.0)
	printf("▏");

    printf ("\n");
}

#define MAX(a,b) ((a) > (b) ? (a) : (b))
static void
cairo_perf_report_diff (cairo_perf_report_t	*old,
			cairo_perf_report_t	*new,
			double			 min_change)
{
    int i, i_old, i_new;
    test_report_t *o, *n;
    int cmp;
    test_diff_t *diff, *diffs;
    int num_diffs = 0;
    int printed_speedup = 0, printed_slowdown = 0;
    double change, max_change;

    diffs = xmalloc (MAX (old->tests_count, new->tests_count) * sizeof (test_diff_t));

    cairo_perf_report_sort_and_compute_stats (old);
    cairo_perf_report_sort_and_compute_stats (new);

    i_old = 0;
    i_new = 0;
    while (i_old < old->tests_count && i_new < new->tests_count) {
	o = &old->tests[i_old];
	n = &new->tests[i_new];

	/* We expect iterations values of 0 when mutltiple raw reports
	 * for the same test have been condensed into the stats of the
	 * first. So we just skip these later reports that have no
	 * stats. */
	if (o->stats.iterations == 0) {
	    i_old++;
	    continue;
	}
	if (n->stats.iterations == 0) {
	    i_new++;
	    continue;
	}

	cmp = test_report_cmp_backend_then_name (o, n);
	if (cmp < 0) {
	    fprintf (stderr, "Only in old: %s %s\n", o->backend, o->name);
	    i_old++;
	    continue;
	}
	if (cmp > 0) {
	    fprintf (stderr, "Only in new: %s %s\n", n->backend, n->name);
	    i_new++;
	    continue;
	}

	diffs[num_diffs].old = o;
	diffs[num_diffs].new = n;
	diffs[num_diffs].speedup = (double) o->stats.min_ticks / n->stats.min_ticks;
	num_diffs++;

	i_old++;
	i_new++;
    }

    qsort (diffs, num_diffs, sizeof (test_diff_t), test_diff_cmp);

    max_change = 1.0;
    for (i = 0; i < num_diffs; i++) {
	change = diffs[i].speedup;
	if (change < 1.0)
	    change = 1.0 / change;
	if (change > max_change)
	    max_change = change;
    }

    for (i = 0; i < num_diffs; i++) {
	diff = &diffs[i];

	change = diff->speedup;
	if (change < 1.0)
	    change = 1.0 / change;

	/* Discard as uninteresting a change which is less than the
	 * minimum change required, (default may be overriden on
	 * command-line). */
	if (change - 1.0 < min_change)
	    continue;

	/* Also discard as uninteresting if the change is less than
	 * the sum each of the standard deviations. */
	if (change - 1.0 < diff->old->stats.std_dev + diff->new->stats.std_dev)
	    continue;

	if (diff->speedup > 1.0 && ! printed_speedup) {
	    printf ("Speedups\n"
		    "========\n");
	    printed_speedup = 1;
	}
	if (diff->speedup < 1.0 && ! printed_slowdown) {
	    printf ("Slowdowns\n"
		    "=========\n");
	    printed_slowdown = 1;
	}

	printf ("%5s-%-4s %26s-%-3d  %6.2f %4.2f%% -> %6.2f %4.2f%%: %5.2fx ",
		diff->old->backend, diff->old->content,
		diff->old->name, diff->old->size,
		diff->old->stats.min_ticks / diff->old->stats.ticks_per_ms,
		diff->old->stats.std_dev * 100,
		diff->new->stats.min_ticks / diff->new->stats.ticks_per_ms,
		diff->new->stats.std_dev * 100,
		change);

	if (diff->speedup > 1.0)
	    printf ("speedup\n");
	else
	    printf ("slowdown\n");

	print_change_bar (change, max_change);
    }

    free (diffs);
}

static void
usage (const char *argv0)
{
    fprintf (stderr, "Usage: %s file1 file2 [minimum_significant_change[%%]]\n", argv0);
    fprintf (stderr,
	     "Computes significant performance differences for cairo performance reports.\n"
	     "Each file should be the output of the cairo-perf program (or \"make perf\").\n"
	     "The third argument is used to supress all changes below some threshold.\n"
	     "The default value of 5%% ignores any speeedup or slowdown of 5%% or less,\n"
	     "A value of 0 will cause all output to be reported.\n");
}

int
main (int argc, const char *argv[])
{
    const char *old_filename, *new_filename;
    cairo_perf_report_t old, new;
    double min_change;
    char *end;

    if (argc < 3) {
	usage (argv[0]);
	return 1;
    }

    old_filename = argv[1];
    new_filename = argv[2];

    min_change = 0.05;
    if (argc >= 4) {
	min_change = strtod (argv[3], &end);
	if (*end && *end == '%')
	    min_change = min_change / 100.0;
    }

    cairo_perf_report_load (&old, old_filename);
    cairo_perf_report_load (&new, new_filename);

    cairo_perf_report_diff (&old, &new, min_change);

    return 0;
}

