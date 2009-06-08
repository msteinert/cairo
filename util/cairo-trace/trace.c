/* cairo-trace - a utility to record and replay calls to the Cairo library.
 *
 * Copyright © 2008 Chris Wilson
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define _GNU_SOURCE

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <dlfcn.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <zlib.h>
#include <math.h>
#include <locale.h> /* for locale independent %f printing */
#include <ctype.h>

#include <cairo.h>
#if CAIRO_HAS_FT_FONT
# include <cairo-ft.h>
#endif

#ifndef CAIRO_TRACE_OUTDIR
#define CAIRO_TRACE_OUTDIR "."
#endif

#if HAVE_BYTESWAP_H
# include <byteswap.h>
#endif
#ifndef bswap_16
# define bswap_16(p) \
	(((((uint16_t)(p)) & 0x00ff) << 8) | \
	  (((uint16_t)(p))           >> 8));
#endif
#ifndef bswap_32
# define bswap_32(p) \
         (((((uint32_t)(p)) & 0x000000ff) << 24) | \
	  ((((uint32_t)(p)) & 0x0000ff00) << 8)  | \
	  ((((uint32_t)(p)) & 0x00ff0000) >> 8)  | \
	  ((((uint32_t)(p)))              >> 24));
#endif

#include "lookup-symbol.h"

/* Reverse the bits in a byte with 7 operations (no 64-bit):
 * Devised by Sean Anderson, July 13, 2001.
 * Source: http://graphics.stanford.edu/~seander/bithacks.html#ReverseByteWith32Bits
 */
#define CAIRO_BITSWAP8(c) ((((c) * 0x0802LU & 0x22110LU) | ((c) * 0x8020LU & 0x88440LU)) * 0x10101LU >> 16)

#if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ > 4)
#define CAIRO_PRINTF_FORMAT(fmt_index, va_index) \
	__attribute__((__format__(__printf__, fmt_index, va_index)))
#else
#define CAIRO_PRINTF_FORMAT(fmt_index, va_index)
#endif

/* XXX implement manual vprintf so that the user can control precision of
 * printed numbers.
 */

#define DLCALL(name, args...) ({ \
    static typeof (&name) name##_real; \
    if (name##_real == NULL) \
	name##_real = dlsym (RTLD_NEXT, #name); \
    (*name##_real) (args);  \
})

#define ARRAY_SIZE(a) (sizeof (a) / sizeof (a[0]))
#define ARRAY_LENGTH(a) ((int) ARRAY_SIZE(a))

#if SIZEOF_VOID_P == 4
#define PTR_SHIFT 2
#elif SIZEOF_VOID_P == 8
#define PTR_SHIFT 3
#else
#error Unexpected pointer size
#endif
#define BUCKET(b, ptr) (((unsigned long) (ptr) >> PTR_SHIFT) % ARRAY_LENGTH (b))

#if defined(__GNUC__) && (__GNUC__ > 2) && defined(__OPTIMIZE__)
#define _BOOLEAN_EXPR(expr)                   \
 __extension__ ({                             \
   int _boolean_var_;                         \
   if (expr)                                  \
      _boolean_var_ = 1;                      \
   else                                       \
      _boolean_var_ = 0;                      \
   _boolean_var_;                             \
})
#define LIKELY(expr) (__builtin_expect (_BOOLEAN_EXPR(expr), 1))
#define UNLIKELY(expr) (__builtin_expect (_BOOLEAN_EXPR(expr), 0))
#else
#define LIKELY(expr) (expr)
#define UNLIKELY(expr) (expr)
#endif

typedef struct _object Object;
typedef struct _type Type;

struct _object {
    const void *addr;
    Type *type;
    unsigned long int token;
    int width, height;
    bool foreign;
    bool defined;
    int operand;
    void *data;
    void (*destroy)(void *);
    Object *next, *prev;
};

struct _type {
    const char *name;

    enum operand_type {
	NONE,
	SURFACE,
	CONTEXT,
	FONT_FACE,
	PATTERN,
	SCALED_FONT,
	_N_OP_TYPES
    } op_type;
    const char *op_code;

    pthread_mutex_t mutex;
    struct _bitmap {
	unsigned long int min;
	unsigned long int count;
	unsigned int map[64];
	struct _bitmap *next;
    } map;
    Object *objects[607];
    Type *next;
};

static struct _type_table {
    pthread_mutex_t mutex;
    Type *op_types[_N_OP_TYPES];
} Types;

static FILE *logfile;
static bool _flush;
static bool _error;
static bool _line_info;
static bool _mark_dirty;
static const cairo_user_data_key_t destroy_key;

#if __GNUC__ >= 3
#define _emit_line_info() do { \
    if (_line_info && _write_lock ()) { \
	void *addr = __builtin_return_address(0); \
	char caller[1024]; \
	_trace_printf ("%% %s() called by %s\n", __FUNCTION__, \
		       lookup_symbol (caller, sizeof (caller), addr)); \
	_write_unlock (); \
    } \
} while (0)
#else
#define _emit_line_info()
#endif

static void
_type_release_token (Type *t, unsigned long int token)
{
    struct _bitmap *b, **prev = NULL;

    b = &t->map;
    while (b != NULL) {
	if (token < b->min + sizeof (b->map) * CHAR_BIT) {
	    unsigned int bit, elem;

	    token -= b->min;
	    elem = token / (sizeof (b->map[0]) * CHAR_BIT);
	    bit  = token % (sizeof (b->map[0]) * CHAR_BIT);
	    b->map[elem] &= ~(1 << bit);
	    if (! --b->count && prev) {
		*prev = b->next;
		free (b);
	    }
	    return;
	}
	prev = &b->next;
	b = b->next;
    }
}

static unsigned long int
_type_next_token (Type *t)
{
    struct _bitmap *b, *bb, **prev = NULL;
    unsigned long int min = 0;

    b = &t->map;
    while (b != NULL) {
	if (b->min != min)
	    break;

	if (b->count < sizeof (b->map) * CHAR_BIT) {
	    unsigned int n, m, bit;
	    for (n = 0; n < ARRAY_SIZE (b->map); n++) {
		if (b->map[n] == (unsigned int) -1)
		    continue;

		for (m=0, bit=1; m<sizeof (b->map[0])*CHAR_BIT; m++, bit<<=1) {
		    if ((b->map[n] & bit) == 0) {
			b->map[n] |= bit;
			b->count++;
			return n * sizeof (b->map[0])*CHAR_BIT + m + b->min;
		    }
		}
	    }
	}
	min += sizeof (b->map) * CHAR_BIT;

	prev = &b->next;
	b = b->next;
    }

    bb = malloc (sizeof (struct _bitmap));
    *prev = bb;
    bb->next = b;
    bb->min = min;
    bb->count = 1;
    bb->map[0] = 0x1;
    memset (bb->map + 1, 0, sizeof (bb->map) - sizeof (bb->map[0]));

    return min;
}

static void
_object_destroy (Object *obj)
{
    int bucket;

    pthread_mutex_lock (&obj->type->mutex);
    bucket = BUCKET (obj->type->objects, obj->addr);
    _type_release_token (obj->type, obj->token);

    if (obj->prev != NULL)
	obj->prev->next = obj->next;
    else
	obj->type->objects[bucket] = obj->next;

    if (obj->next != NULL)
	obj->next->prev = obj->prev;
    pthread_mutex_unlock (&obj->type->mutex);

    if (obj->data != NULL && obj->destroy != NULL)
	obj->destroy (obj->data);

    free (obj);
}

static void
_type_create (const char *typename,
	      enum operand_type op_type,
	      const char *op_code)
{
    Type *t;

    pthread_mutex_lock (&Types.mutex);

    t = malloc (sizeof (Type));
    t->name = typename;
    t->op_type = op_type;
    t->op_code = op_code;

    pthread_mutex_init (&t->mutex, NULL);

    t->map.min = 0;
    t->map.count = 0;
    memset (t->map.map, 0, sizeof (t->map.map));
    t->map.next = NULL;

    memset (t->objects, 0, sizeof (t->objects));

    t->next = NULL;

    Types.op_types[op_type] = t;
    pthread_mutex_unlock (&Types.mutex);
}

static Type *
_get_type (enum operand_type type)
{
    return Types.op_types[type];
}

static void
_type_destroy (Type *t)
{
    int n;
    struct _bitmap *b;

    for (n = 0; n < ARRAY_LENGTH (t->objects); n++) {
	Object *obj = t->objects[n];
	while (obj != NULL) {
	    Object *next = obj->next;
	    _object_destroy (obj);
	    obj = next;
	}
    }

    b = t->map.next;
    while (b != NULL) {
	struct _bitmap *next = b->next;
	free (b);
	b = next;
    }

    pthread_mutex_destroy (&t->mutex);
    free (t);
}

static Object *
_type_get_object (Type *type, const void *ptr)
{
    Object *obj;
    int bucket = BUCKET (type->objects, ptr);

    for (obj = type->objects[bucket]; obj != NULL; obj = obj->next) {
	if (obj->addr == ptr) {
	    if (obj->prev != NULL) { /* mru */
		obj->prev->next = obj->next;
		if (obj->next != NULL)
		    obj->next->prev = obj->prev;
		obj->prev = NULL;
		type->objects[bucket]->prev = obj;
		obj->next = type->objects[bucket];
		type->objects[bucket] = obj;
	    }
	    return obj;
	}
    }

    return NULL;
}

static Object *
_object_create (Type *type, const void *ptr)
{
    Object *obj;
    int bucket = BUCKET (type->objects, ptr);

    obj = malloc (sizeof (Object));
    obj->defined = false;
    obj->foreign = false;
    obj->operand = -1;
    obj->type = type;
    obj->addr = ptr;
    obj->token = _type_next_token (type);
    obj->data = NULL;
    obj->destroy = NULL;
    obj->prev = NULL;
    obj->next = type->objects[bucket];
    if (type->objects[bucket] != NULL)
	type->objects[bucket]->prev = obj;
    type->objects[bucket] = obj;

    return obj;
}

static void __attribute__ ((constructor))
_init_trace (void)
{
    pthread_mutex_init (&Types.mutex, NULL);

    _type_create ("unclassed", NONE, "");
    _type_create ("cairo_t", CONTEXT, "c");
    _type_create ("cairo_font_face_t", FONT_FACE, "f");
    _type_create ("cairo_pattern_t", PATTERN, "p");
    _type_create ("cairo_scaled_font_t", SCALED_FONT, "sf");
    _type_create ("cairo_surface_t", SURFACE, "s");
}

static void
_close_trace (void)
{
    if (logfile != NULL) {
	fclose (logfile);
	logfile = NULL;
    }
}

static void __attribute__ ((destructor))
_fini_trace (void)
{
    int n;

    _close_trace ();

    for (n = 0; n < ARRAY_LENGTH (Types.op_types); n++) {
	if (Types.op_types[n]) {
	    _type_destroy (Types.op_types[n]);
	    Types.op_types[n] = NULL;
	}
    }

    pthread_mutex_destroy (&Types.mutex);
}

/* Format a double in a locale independent way and trim trailing
 * zeros.  Based on code from Alex Larson <alexl@redhat.com>.
 * http://mail.gnome.org/archives/gtk-devel-list/2001-October/msg00087.html
 *
 * The code in the patch is copyright Red Hat, Inc under the LGPL.
 */
#define SIGNIFICANT_DIGITS_AFTER_DECIMAL 6
static void
_trace_dtostr (char *buffer, size_t size, double d)
{
    struct lconv *locale_data;
    const char *decimal_point;
    int decimal_point_len;
    char *p;
    int decimal_len;
    int num_zeros, decimal_digits;

    /* Omit the minus sign from negative zero. */
    if (d == 0.0)
	d = 0.0;

    locale_data = localeconv ();
    decimal_point = locale_data->decimal_point;
    decimal_point_len = strlen (decimal_point);

    /* Using "%f" to print numbers less than 0.1 will result in
     * reduced precision due to the default 6 digits after the
     * decimal point.
     *
     * For numbers is < 0.1, we print with maximum precision and count
     * the number of zeros between the decimal point and the first
     * significant digit. We then print the number again with the
     * number of decimal places that gives us the required number of
     * significant digits. This ensures the number is correctly
     * rounded.
     */
    if (fabs (d) >= 0.1) {
	snprintf (buffer, size, "%f", d);
    } else {
	snprintf (buffer, size, "%.18f", d);
	p = buffer;

	if (*p == '+' || *p == '-')
	    p++;

	while (isdigit (*p))
	    p++;

	if (strncmp (p, decimal_point, decimal_point_len) == 0)
	    p += decimal_point_len;

	num_zeros = 0;
	while (*p++ == '0')
	    num_zeros++;

	decimal_digits = num_zeros + SIGNIFICANT_DIGITS_AFTER_DECIMAL;

	if (decimal_digits < 18)
	    snprintf (buffer, size, "%.*f", decimal_digits, d);
    }
    p = buffer;

    if (*p == '+' || *p == '-')
	p++;

    while (isdigit (*p))
	p++;

    if (strncmp (p, decimal_point, decimal_point_len) == 0) {
	*p = '.';
	decimal_len = strlen (p + decimal_point_len);
	memmove (p + 1, p + decimal_point_len, decimal_len);
	p[1 + decimal_len] = 0;

	/* Remove trailing zeros and decimal point if possible. */
	for (p = p + decimal_len; *p == '0'; p--)
	    *p = 0;

	if (*p == '.') {
	    *p = 0;
	    p--;
	}
    }
}

enum {
    LENGTH_MODIFIER_LONG = 0x100
};

/* Here's a limited reimplementation of printf.  The reason for doing
 * this is primarily to special case handling of doubles.  We want
 * locale independent formatting of doubles and we want to trim
 * trailing zeros.  This is handled by dtostr() above, and the code
 * below handles everything else by calling snprintf() to do the
 * formatting.  This functionality is only for internal use and we
 * only implement the formats we actually use.
 */
static void CAIRO_PRINTF_FORMAT(1, 0)
_trace_vprintf (const char *fmt, va_list ap)
{
#define SINGLE_FMT_BUFFER_SIZE 32
    char buffer[512], single_fmt[SINGLE_FMT_BUFFER_SIZE];
    int single_fmt_length;
    char *p;
    const char *f, *start;
    int length_modifier, width;
    cairo_bool_t var_width;
    int ret_ignored;

    f = fmt;
    p = buffer;
    while (*f != '\0') {
	if (*f != '%') {
	    *p++ = *f++;
	    continue;
	}

	start = f;
	f++;

	if (*f == '0')
	    f++;

        var_width = 0;
        if (*f == '*') {
            var_width = 1;
	    f++;
        }

	while (isdigit (*f))
	    f++;

	length_modifier = 0;
	if (*f == 'l') {
	    length_modifier = LENGTH_MODIFIER_LONG;
	    f++;
	}

	/* The only format strings exist in the cairo implementation
	 * itself. So there's an internal consistency problem if any
	 * of them is larger than our format buffer size. */
	single_fmt_length = f - start + 1;

	/* Reuse the format string for this conversion. */
	memcpy (single_fmt, start, single_fmt_length);
	single_fmt[single_fmt_length] = '\0';

	/* Flush contents of buffer before snprintf()'ing into it. */
	ret_ignored = fwrite (buffer, 1, p-buffer, logfile);

	/* We group signed and unsigned together in this switch, the
	 * only thing that matters here is the size of the arguments,
	 * since we're just passing the data through to sprintf(). */
	switch (*f | length_modifier) {
	case '%':
	    buffer[0] = *f;
	    buffer[1] = 0;
	    break;
	case 'd':
	case 'u':
	case 'o':
	case 'x':
	case 'X':
            if (var_width) {
                width = va_arg (ap, int);
                snprintf (buffer, sizeof buffer,
                          single_fmt, width, va_arg (ap, int));
            } else {
                snprintf (buffer, sizeof buffer, single_fmt, va_arg (ap, int));
            }
	    break;
	case 'd' | LENGTH_MODIFIER_LONG:
	case 'u' | LENGTH_MODIFIER_LONG:
	case 'o' | LENGTH_MODIFIER_LONG:
	case 'x' | LENGTH_MODIFIER_LONG:
	case 'X' | LENGTH_MODIFIER_LONG:
            if (var_width) {
                width = va_arg (ap, int);
                snprintf (buffer, sizeof buffer,
                          single_fmt, width, va_arg (ap, long int));
            } else {
                snprintf (buffer, sizeof buffer,
                          single_fmt, va_arg (ap, long int));
            }
	    break;
	case 's':
	    snprintf (buffer, sizeof buffer,
		      single_fmt, va_arg (ap, const char *));
	    break;
	case 'f':
	case 'g':
	    _trace_dtostr (buffer, sizeof buffer, va_arg (ap, double));
	    break;
	case 'c':
	    buffer[0] = va_arg (ap, int);
	    buffer[1] = 0;
	    break;
	default:
	    break;
	}
	p = buffer + strlen (buffer);
	f++;
    }

    ret_ignored = fwrite (buffer, 1, p-buffer, logfile);
}

static void CAIRO_PRINTF_FORMAT(1, 2)
_trace_printf (const char *fmt, ...)
{
    va_list ap;

    va_start (ap, fmt);
    _trace_vprintf (fmt, ap);
    va_end (ap);
}

static void
get_prog_name (char *buf, int length)
{
    FILE *file = fopen ("/proc/self/cmdline", "rb");
    *buf = '\0';
    if (file != NULL) {
	char *slash;

	slash = fgets (buf, length, file);
	fclose (file);
	if (slash == NULL)
	    return;

	slash = strrchr (buf, '/');
	if (slash != NULL) {
	    int len = strlen (slash+1);
	    memmove (buf, slash+1, len+1);
	}
    }
}

static void
_emit_header (void)
{
    char name[4096] = "";

    get_prog_name (name, sizeof (name));

    _trace_printf ("%%!CairoScript - %s\n", name);
    _trace_printf ("%%*** Warning CairoScript is still a new tracing format, and is subject to change.\n");
}

static bool
_init_logfile (void)
{
    static bool initialized;
    const char *filename;
    const char *env;

    if (initialized)
	return logfile != NULL;

    initialized = true;

    env = getenv ("CAIRO_TRACE_FLUSH");
    if (env != NULL)
	_flush = atoi (env);

    _line_info = true;
    env = getenv ("CAIRO_TRACE_LINE_INFO");
    if (env != NULL)
	_line_info = atoi (env);

    _mark_dirty = true;
    env = getenv ("CAIRO_TRACE_MARK_DIRTY");
    if (env != NULL)
	_mark_dirty = atoi (env);

    filename = getenv ("CAIRO_TRACE_FD");
    if (filename != NULL) {
	int fd = atoi (filename);
	logfile = fdopen (fd, "w");
	if (logfile == NULL) {
	    fprintf (stderr, "Failed to open trace file descriptor '%s': %s\n",
		       filename, strerror (errno));
	    return false;
	}
	goto done;
    }

    filename = getenv ("CAIRO_TRACE_OUTFILE_EXACT");
    if (filename == NULL) {
	char buf[4096], name[4096] = "";

	filename = getenv ("CAIRO_TRACE_OUTDIR");
	if (filename == NULL)
	    filename = CAIRO_TRACE_OUTDIR;

	get_prog_name (name, sizeof (name));
	if (*name == '\0')
	    strcpy (name, "cairo-trace.dat");

	snprintf (buf, sizeof (buf), "%s/%s.%d.trace",
		filename, name, getpid());

	filename = buf;
    }

    logfile = fopen (filename, "wb");
    if (logfile == NULL) {
	fprintf (stderr, "Failed to open trace file '%s': %s\n",
		   filename, strerror (errno));
	return false;
    }

    fprintf (stderr, "cairo-trace: Recording cairo trace data to %s\n",
	     filename);

done:
    atexit (_close_trace);
    _emit_header ();
    return true;
}

static bool
_write_lock (void)
{
    if (_error)
	return false;

    if (! _init_logfile ())
	return false;

    flockfile (logfile);
    return true;
}

static void
_write_unlock (void)
{
    if (logfile == NULL)
	return;

    funlockfile (logfile);

    if (_flush)
	fflush (logfile);
}


static Object *
_type_object_create (enum operand_type op_type, const void *ptr)
{
    Type *type;
    Object *obj;

    type = _get_type (op_type);

    pthread_mutex_lock (&type->mutex);
    obj = _object_create (type, ptr);
    pthread_mutex_unlock (&type->mutex);

    return obj;
}

static Object *
_get_object (enum operand_type op_type, const void *ptr)
{
    Type *type;
    Object *obj;

    type = _get_type (op_type);
    pthread_mutex_lock (&type->mutex);
    obj = _type_get_object (type, ptr);
    pthread_mutex_unlock (&type->mutex);

    return obj;
}

static Object *current_object[2048]; /* XXX limit operand stack */
static int current_stack_depth;

static void
_consume_operand (void)
{
    Object *obj;

    obj = current_object[--current_stack_depth];
    if (! obj->defined) {
	_trace_printf ("dup /%s%ld exch def\n",
		       obj->type->op_code,
		       obj->token);
	obj->defined = true;
    }
    obj->operand = -1;
}

static void
_exch_operands (void)
{
    Object *tmp;

    tmp = current_object[current_stack_depth-1];
    tmp->operand--;
    current_object[current_stack_depth-1] = current_object[current_stack_depth-2];
    current_object[current_stack_depth-2] = tmp;
    tmp = current_object[current_stack_depth-1];
    tmp->operand++;
}

static bool
_pop_operands_to_object (Object *obj)
{
    if (obj->operand == -1)
	return false;

    if (obj->operand == current_stack_depth - 2) {
	_exch_operands ();
	_trace_printf ("exch ");
	return true;
    }

    while (current_stack_depth > obj->operand + 1) {
	Object *c_obj;

	c_obj = current_object[--current_stack_depth];
	c_obj->operand = -1;
	if (! c_obj->defined) {
	    _trace_printf ("/%s%ld exch def\n",
		     c_obj->type->op_code,
		     c_obj->token);
	    c_obj->defined = true;
	} else {
	    _trace_printf ("pop %% %s%ld\n",
			   c_obj->type->op_code, c_obj->token);
	}
    }

    return true;
}

static bool
_pop_operands_to (enum operand_type t, const void *ptr)
{
    return _pop_operands_to_object (_get_object (t, ptr));
}

static bool
_is_current_object (Object *obj, int depth)
{
    if (current_stack_depth <= depth)
	return false;
    return current_object[current_stack_depth-depth-1] == obj;
}

static bool
_is_current (enum operand_type type, const void *ptr, int depth)
{
    return _is_current_object (_get_object (type, ptr), depth);
}

static void
_push_operand (enum operand_type t, const void *ptr)
{
    Object *obj = _get_object (t, ptr);

    if (current_stack_depth ==
	sizeof (current_object) / sizeof (current_object[0]))
    {
	int n;

	fprintf (stderr, "Operand stack overflow!\n");
	for (n = 0; n < current_stack_depth; n++) {
	    obj = current_object[n];

	    fprintf (stderr, "  [%3d] = %s%ld\n",
		     n, obj->type->op_code, obj->token);
	}

	abort ();
    }

    obj->operand = current_stack_depth;
    current_object[current_stack_depth++] = obj;
}

static void
_object_remove (Object *obj)
{
    if (obj->operand != -1) {
	if (obj->operand == current_stack_depth - 1) {
	    _trace_printf ("pop %% %s%ld destroyed\n",
			   obj->type->op_code, obj->token);
	} else if (obj->operand == current_stack_depth - 2) {
	    _exch_operands ();
	    _trace_printf ("exch pop %% %s%ld destroyed\n",
			   obj->type->op_code, obj->token);
	} else {
	    int n;

	    _trace_printf ("%d -1 roll pop %% %s%ld destroyed\n",
			   current_stack_depth - obj->operand,
			   obj->type->op_code, obj->token);

	    for (n = obj->operand; n < current_stack_depth - 1; n++) {
		current_object[n] = current_object[n+1];
		current_object[n]->operand = n;
	    }
	}
	current_stack_depth--;
    }
}

static void
_object_undef (void *ptr)
{
    Object *obj = ptr;

    if (_write_lock ()) {
	_object_remove (obj);

	if (obj->defined) {
	    _trace_printf ("/%s%ld undef\n",
			   obj->type->op_code, obj->token);
	}

	_write_unlock ();
    }

    _object_destroy (obj);
}

static long
_create_context_id (cairo_t *cr)
{
    Object *obj;

    obj = _get_object (CONTEXT, cr);
    if (obj == NULL) {
	obj = _type_object_create (CONTEXT, cr);
	DLCALL (cairo_set_user_data,
		cr, &destroy_key, obj, _object_undef);
    }

    return obj->token;
}

static long
_get_id (enum operand_type op_type, const void *ptr)
{
    Object *obj;

    obj = _get_object (op_type, ptr);
    if (obj == NULL) {
	if (logfile != NULL) {
	    _trace_printf ("%% Unknown object of type %s, trace is incomplete.",
			   _get_type (op_type)->name);
	}
	_error = true;
	return -1;
    }

    return obj->token;
}

static bool
_has_id (enum operand_type op_type, const void *ptr)
{
    return _get_object (op_type, ptr) != NULL;
}

static long
_get_context_id (cairo_t *cr)
{
    return _get_id (CONTEXT, cr);
}

static long
_create_font_face_id (cairo_font_face_t *font_face)
{
    Object *obj;

    obj = _get_object (FONT_FACE, font_face);
    if (obj == NULL) {
	obj = _type_object_create (FONT_FACE, font_face);
	DLCALL (cairo_font_face_set_user_data,
		font_face, &destroy_key, obj, _object_undef);
    }

    return obj->token;
}

static long
_get_font_face_id (cairo_font_face_t *font_face)
{
    return _get_id (FONT_FACE, font_face);
}

static bool
_has_font_face_id (cairo_font_face_t *font_face)
{
    return _has_id (FONT_FACE, font_face);
}

static void
_emit_font_face_id (cairo_font_face_t *font_face)
{
    Object *obj = _get_object (FONT_FACE, font_face);
    if (obj == NULL) {
	_trace_printf ("null ");
    } else {
	if (obj->defined) {
	    _trace_printf ("f%ld ", obj->token);
	} else {
	    _trace_printf ("%d index ", current_stack_depth - obj->operand - 1);
	}
    }
}

static bool
_has_pattern_id (cairo_pattern_t *pattern)
{
    return _has_id (PATTERN, pattern);
}

static long
_create_pattern_id (cairo_pattern_t *pattern)
{
    Object *obj;

    obj = _get_object (PATTERN, pattern);
    if (obj == NULL) {
	obj = _type_object_create (PATTERN, pattern);
	DLCALL (cairo_pattern_set_user_data,
		pattern, &destroy_key, obj, _object_undef);
    }

    return obj->token;
}

static long
_get_pattern_id (cairo_pattern_t *pattern)
{
    return _get_id (PATTERN, pattern);
}

static void
_emit_pattern_id (cairo_pattern_t *pattern)
{
    Object *obj = _get_object (PATTERN, pattern);
    if (obj == NULL) {
	_trace_printf ("null ");
    } else {
	if (obj->defined) {
	    _trace_printf ("p%ld ", obj->token);
	} else {
	    _trace_printf ("%d index ",
		     current_stack_depth - obj->operand - 1);
	}
    }
}

static long
_create_scaled_font_id (cairo_scaled_font_t *font)
{
    Object *obj;

    obj = _get_object (SCALED_FONT, font);
    if (obj == NULL) {
	obj = _type_object_create (SCALED_FONT, font);
	DLCALL (cairo_scaled_font_set_user_data,
		font, &destroy_key, obj, _object_undef);
    }

    return obj->token;
}

static long
_get_scaled_font_id (const cairo_scaled_font_t *font)
{
    return _get_id (SCALED_FONT, font);
}

static bool
_has_scaled_font_id (const cairo_scaled_font_t *font)
{
    return _has_id (SCALED_FONT, font);
}

static bool
_has_surface_id (const cairo_surface_t *surface)
{
    return _has_id (SURFACE, surface);
}

static long
_create_surface_id (cairo_surface_t *surface)
{
    Object *obj;

    obj = _get_object (SURFACE, surface);
    if (obj == NULL) {
	obj = _type_object_create (SURFACE, surface);
	DLCALL (cairo_surface_set_user_data,
		surface, &destroy_key, obj, _object_undef);
    }

    return obj->token;
}

static long
_get_surface_id (cairo_surface_t *surface)
{
    return _get_id (SURFACE, surface);
}

static bool
_matrix_is_identity (const cairo_matrix_t *m)
{
    return m->xx == 1. && m->yx == 0. &&
	   m->xy == 0. && m->yy == 1. &&
	   m->x0 == 0. && m->y0 == 0.;
}

#define BUFFER_SIZE 16384
struct _data_stream {
    z_stream zlib_stream;
    unsigned char zin_buf[BUFFER_SIZE];
    unsigned char zout_buf[BUFFER_SIZE];
    unsigned char four_tuple[4];
    int base85_pending;
};

static void
_write_zlib_data_start (struct _data_stream *stream)
{
    stream->zlib_stream.zalloc = Z_NULL;
    stream->zlib_stream.zfree  = Z_NULL;
    stream->zlib_stream.opaque  = Z_NULL;

    deflateInit (&stream->zlib_stream, Z_DEFAULT_COMPRESSION);

    stream->zlib_stream.next_in = stream->zin_buf;
    stream->zlib_stream.avail_in = 0;
    stream->zlib_stream.next_out = stream->zout_buf;
    stream->zlib_stream.avail_out = BUFFER_SIZE;
}

static void
_write_base85_data_start (struct _data_stream *stream)
{
    stream->base85_pending = 0;
}

static void
_write_data_start (struct _data_stream *stream)
{
    _write_zlib_data_start (stream);
    _write_base85_data_start (stream);

    _trace_printf ("<~");
}

static bool
_expand_four_tuple_to_five (unsigned char four_tuple[4],
			    unsigned char five_tuple[5])
{
    uint32_t value;
    int digit, i;
    bool all_zero = true;

    value = four_tuple[0] << 24 |
	    four_tuple[1] << 16 |
	    four_tuple[2] << 8  |
	    four_tuple[3] << 0;
    for (i = 0; i < 5; i++) {
	digit = value % 85;
	if (digit != 0 && all_zero)
	    all_zero = false;
	five_tuple[4-i] = digit + 33;
	value = value / 85;
    }

    return all_zero;
}

static void
_write_base85_data (struct _data_stream *stream,
		    const unsigned char	  *data,
		    unsigned int	   length)
{
    const unsigned char *ptr = data;
    unsigned char five_tuple[5];
    bool is_zero;
    int ret;

    while (length--) {
	stream->four_tuple[stream->base85_pending++] = *ptr++;
	if (stream->base85_pending == 4) {
	    is_zero = _expand_four_tuple_to_five (stream->four_tuple,
						  five_tuple);
	    if (is_zero)
		ret = fwrite ("z", 1, 1, logfile);
	    else
		ret = fwrite (five_tuple, 5, 1, logfile);
	    stream->base85_pending = 0;
	}
    }
}

static void
_write_zlib_data (struct _data_stream *stream, bool flush)
{
    bool finished;

    do {
	int ret = deflate (&stream->zlib_stream, flush ? Z_FINISH : Z_NO_FLUSH);
	if (flush || stream->zlib_stream.avail_out == 0) {
	    _write_base85_data (stream,
				stream->zout_buf,
				BUFFER_SIZE - stream->zlib_stream.avail_out);
	    stream->zlib_stream.next_out = stream->zout_buf;
	    stream->zlib_stream.avail_out = BUFFER_SIZE;
	}

	finished = true;
	if (stream->zlib_stream.avail_in != 0)
	    finished = false;
	if (flush && ret != Z_STREAM_END)
	    finished = false;
    } while (! finished);

    stream->zlib_stream.next_in = stream->zin_buf;
}

static void
_write_data (struct _data_stream *stream,
	     const void *data,
	     unsigned int length)
{
    unsigned int count;
    const unsigned char *p = data;

    while (length) {
	count = length;
	if (count > BUFFER_SIZE - stream->zlib_stream.avail_in)
	    count = BUFFER_SIZE - stream->zlib_stream.avail_in;
	memcpy (stream->zin_buf + stream->zlib_stream.avail_in, p, count);
	p += count;
	stream->zlib_stream.avail_in += count;
	length -= count;

	if (stream->zlib_stream.avail_in == BUFFER_SIZE)
	    _write_zlib_data (stream, false);
    }
}

static void
_write_zlib_data_end (struct _data_stream *stream)
{
    _write_zlib_data (stream, true);
    deflateEnd (&stream->zlib_stream);

}

static void
_write_base85_data_end (struct _data_stream *stream)
{
    unsigned char five_tuple[5];
    int ret;

    if (stream->base85_pending) {
	memset (stream->four_tuple + stream->base85_pending,
		0, 4 - stream->base85_pending);
	_expand_four_tuple_to_five (stream->four_tuple, five_tuple);
	ret = fwrite (five_tuple, stream->base85_pending+1, 1, logfile);
    }
}

static void
_write_data_end (struct _data_stream *stream)
{
    _write_zlib_data_end (stream);
    _write_base85_data_end (stream);

    _trace_printf ("~>");
}

static void
_emit_data (const void *data, unsigned int length)
{
    struct _data_stream stream;

    _write_data_start (&stream);
    _write_data (&stream, data, length);
    _write_data_end (&stream);
}

static const char *
_format_to_string (cairo_format_t format)
{
    const char *names[] = {
	"ARGB32",	/* CAIRO_FORMAT_ARGB32 */
	"RGB24",	/* CAIRO_FORMAT_RGB24 */
	"A8",		/* CAIRO_FORMAT_A8 */
	"A1"		/* CAIRO_FORMAT_A1 */
    };
    return names[format];
}

static const char *
_status_to_string (cairo_status_t status)
{
    const char *names[] = {
	"STATUS_SUCCESS",
	"STATUS_NO_MEMORY",
	"STATUS_INVALID_RESTORE",
	"STATUS_INVALID_POP_GROUP",
	"STATUS_NO_CURRENT_POINT",
	"STATUS_INVALID_MATRIX",
	"STATUS_INVALID_STATUS",
	"STATUS_NULL_POINTER",
	"STATUS_INVALID_STRING",
	"STATUS_INVALID_PATH_DATA",
	"STATUS_READ_ERROR",
	"STATUS_WRITE_ERROR",
	"STATUS_SURFACE_FINISHED",
	"STATUS_SURFACE_TYPE_MISMATCH",
	"STATUS_PATTERN_TYPE_MISMATCH",
	"STATUS_INVALID_CONTENT",
	"STATUS_INVALID_FORMAT",
	"STATUS_INVALID_VISUAL",
	"STATUS_FILE_NOT_FOUND",
	"STATUS_INVALID_DASH",
	"STATUS_INVALID_DSC_COMMENT",
	"STATUS_INVALID_INDEX",
	"STATUS_CLIP_NOT_REPRESENTABLE",
	"STATUS_TEMP_FILE_ERROR",
	"STATUS_INVALID_STRIDE",
	"STATUS_FONT_TYPE_MISMATCH",
	"STATUS_USER_FONT_IMMUTABLE",
	"STATUS_USER_FONT_ERROR",
	"STATUS_NEGATIVE_COUNT",
	"STATUS_INVALID_CLUSTERS",
	"STATUS_INVALID_SLANT",
	"STATUS_INVALID_WEIGHT"
    };
    return names[status];
}

static void CAIRO_PRINTF_FORMAT(2, 3)
_emit_image (cairo_surface_t *image,
	     const char *info,
	     ...)
{
    int stride, row, width, height;
    cairo_format_t format;
    uint8_t row_stack[BUFFER_SIZE];
    uint8_t *rowdata;
    uint8_t *data;
    struct _data_stream stream;

    if (cairo_surface_status (image)) {
	_trace_printf ("dict\n"
		       "  /status //%s set\n"
		       "  image",
		       _status_to_string (cairo_surface_status (image)));
	return;
    }

    width = cairo_image_surface_get_width (image);
    height = cairo_image_surface_get_height (image);
    stride = cairo_image_surface_get_stride (image);
    format = cairo_image_surface_get_format (image);
    data = cairo_image_surface_get_data (image);

    _trace_printf ("dict\n"
		   "  /width %d set\n"
		   "  /height %d set\n"
		   "  /format //%s set\n",
		   width, height,
		   _format_to_string (format));
    if (info != NULL) {
	va_list ap;

	va_start (ap, info);
	_trace_vprintf (info, ap);
	va_end (ap);
    }

    if (cairo_version () >= CAIRO_VERSION_ENCODE (1, 9, 0)) {
	const char *mime_types[] = {
	    CAIRO_MIME_TYPE_JPEG,
	    CAIRO_MIME_TYPE_PNG,
	    NULL
	}, **mime_type;

	for (mime_type = mime_types; *mime_type; mime_type++) {
	    const unsigned char *mime_data;
	    unsigned int mime_length;

	    cairo_surface_get_mime_data (image, *mime_type,
					 &mime_data, &mime_length);
	    if (mime_data != NULL) {
		_trace_printf ("  /mime-type (%s) set\n"
			       "  /source <~",
			       *mime_type);
		_write_base85_data_start (&stream);
		_write_base85_data (&stream, mime_data, mime_length);
		_write_base85_data_end (&stream);
		_trace_printf ("~> set\n"
			       "  image");
		return;
	    }
	}
    }

    _trace_printf ("  /source ");
    _write_data_start (&stream);

#ifdef WORDS_BIGENDIAN
    switch (format) {
    case CAIRO_FORMAT_A1:
	for (row = height; row--; ) {
	    _write_data (&stream, data, (width+7)/8);
	    data += stride;
	}
	break;
    case CAIRO_FORMAT_A8:
	for (row = height; row--; ) {
	    _write_data (&stream, data, width);
	    data += stride;
	}
	break;
    case CAIRO_FORMAT_RGB24:
	for (row = height; row--; ) {
	    int col;
	    rowdata = data;
	    for (col = width; col--; ) {
		_write_data (&stream, rowdata, 3);
		rowdata+=4;
	    }
	    data += stride;
	}
	break;
    case CAIRO_FORMAT_ARGB32:
	for (row = height; row--; ) {
	    _write_data (&stream, data, 4*width);
	    data += stride;
	}
	break;
    default:
	break;
    }
#else
    if (stride > ARRAY_LENGTH (row_stack)) {
	rowdata = malloc (stride);
	if (rowdata == NULL)
	    goto BAIL;
    } else
	rowdata = row_stack;

    switch (format) {
    case CAIRO_FORMAT_A1:
	for (row = height; row--; ) {
	    int col;
	    for (col = 0; col < (width + 7)/8; col++)
		rowdata[col] = CAIRO_BITSWAP8 (data[col]);
	    _write_data (&stream, rowdata, (width+7)/8);
	    data += stride;
	}
	break;
    case CAIRO_FORMAT_A8:
	for (row = height; row--; ) {
	    _write_data (&stream, rowdata, width);
	    data += stride;
	}
	break;
    case CAIRO_FORMAT_RGB24:
	for (row = height; row--; ) {
	    uint8_t *src = data;
	    int col;
	    for (col = 0; col < width; col++) {
		rowdata[3*col+2] = *src++;
		rowdata[3*col+1] = *src++;
		rowdata[3*col+0] = *src++;
		src++;
	    }
	    _write_data (&stream, rowdata, 3*width);
	    data += stride;
	}
	break;
    case CAIRO_FORMAT_ARGB32:
	for (row = height; row--; ) {
	    uint32_t *src = (uint32_t *) data;
	    uint32_t *dst = (uint32_t *) rowdata;
	    int col;
	    for (col = 0; col < width; col++)
		dst[col] = bswap_32 (src[col]);
	    _write_data (&stream, rowdata, 4*width);
	    data += stride;
	}
	break;
    default:
	break;
    }
    if (rowdata != row_stack)
	free (rowdata);

BAIL:
    _write_data_end (&stream);
#endif
    _trace_printf (" /deflate filter set\n"
		   "  image");
}

static void
_encode_string_literal (char *out, int max,
			const char *utf8, int len)
{
    char c;
    const char *end;

    *out++ = '(';
    max--;

    if (utf8 == NULL)
	goto DONE;

    if (len < 0)
	len = strlen (utf8);
    end = utf8 + len;

    while (utf8 < end) {
	if (max < 5)
	    break;

	switch ((c = *utf8++)) {
	case '\n':
	    *out++ = '\\';
	    *out++ = 'n';
	    max -= 2;
	    break;
	case '\r':
	    *out++ = '\\';
	    *out++ = 'r';
	    max -= 2;
	case '\t':
	    *out++ = '\\';
	    *out++ = 't';
	    max -= 2;
	    break;
	case '\b':
	    *out++ = '\\';
	    *out++ = 'b';
	    max -= 2;
	    break;
	case '\f':
	    *out++ = '\\';
	    *out++ = 'f';
	    max -= 2;
	    break;
	case '\\':
	case '(':
	case ')':
	    *out++ = '\\';
	    *out++ = c;
	    max -= 2;
	    break;
	default:
	    if (isprint (c) || isspace (c)) {
		*out++ = c;
	    } else {
		int octal = 0;
		while (c) {
		    octal *= 10;
		    octal += c&7;
		    c /= 8;
		}
		octal = snprintf (out, max, "\\%03d", octal);
		out += octal;
		max -= octal;
	    }
	    break;
	}
    }
DONE:
    *out++ = ')';
    *out = '\0';
}

static void
_emit_string_literal (const char *utf8, int len)
{
    char c;
    const char *end;

    if (utf8 == NULL) {
	_trace_printf ("()");
	return;
    }

    if (len < 0)
	len = strlen (utf8);
    end = utf8 + len;

    _trace_printf ("(");
    while (utf8 < end) {
	switch ((c = *utf8++)) {
	case '\n':
	    c = 'n';
	    goto ESCAPED_CHAR;
	case '\r':
	    c = 'r';
	    goto ESCAPED_CHAR;
	case '\t':
	    c = 't';
	    goto ESCAPED_CHAR;
	case '\b':
	    c = 'b';
	    goto ESCAPED_CHAR;
	case '\f':
	    c = 'f';
	    goto ESCAPED_CHAR;
	case '\\':
	case '(':
	case ')':
ESCAPED_CHAR:
	    _trace_printf ("\\%c", c);
	    break;
	default:
	    if (isprint (c) || isspace (c)) {
		_trace_printf ("%c", c);
	    } else {
		int octal = 0;
		while (c) {
		    octal *= 10;
		    octal += c&7;
		    c /= 8;
		}
		_trace_printf ("\\%03d", octal);
	    }
	    break;
	}
    }
    _trace_printf (")");
}

static void
_emit_current (Object *obj)
{
    if (obj != NULL && ! _pop_operands_to_object (obj)) {
	_trace_printf ("%s%ld\n", obj->type->op_code, obj->token);
	_push_operand (obj->type->op_type, obj->addr);
    }
}

static void
_emit_context (cairo_t *cr)
{
    _emit_current (_get_object (CONTEXT, cr));
}

static void
_emit_font_face (cairo_font_face_t *font_face)
{
    _emit_current (_get_object (FONT_FACE, font_face));
}

static void
_emit_pattern (cairo_pattern_t *pattern)
{
    _emit_current (_get_object (PATTERN, pattern));
}

static void
_emit_scaled_font (cairo_scaled_font_t *scaled_font)
{
    _emit_current (_get_object (SCALED_FONT, scaled_font));
}

static void
_emit_surface (cairo_surface_t *surface)
{
    _emit_current (_get_object (SURFACE, surface));
}

static void CAIRO_PRINTF_FORMAT(2, 3)
_emit_cairo_op (cairo_t *cr, const char *fmt, ...)
{
    va_list ap;

    if (cr == NULL || ! _write_lock ())
	return;

    _emit_context (cr);

    va_start (ap, fmt);
    _trace_vprintf ( fmt, ap);
    va_end (ap);

    _write_unlock ();
}

cairo_t *
cairo_create (cairo_surface_t *target)
{
    cairo_t *ret;
    long surface_id;
    long context_id;

    ret = DLCALL (cairo_create, target);
    context_id = _create_context_id (ret);

    _emit_line_info ();
    if (target != NULL && _write_lock ()) {
	surface_id = _get_surface_id (target);
	if (surface_id != -1) {
	    _get_object (SURFACE, target)->foreign = false;

	    /* we presume that we will continue to use the context */
	    if (_pop_operands_to (SURFACE, target)){
		_consume_operand ();
	    } else {
		_trace_printf ("s%ld ", surface_id);
	    }
	    _trace_printf ("context %% c%ld\n", context_id);
	    _push_operand (CONTEXT, ret);
	}
	_write_unlock ();
    }

    return ret;
}

void
cairo_save (cairo_t *cr)
{
    _emit_line_info ();
    _emit_cairo_op (cr, "save\n");
    return DLCALL (cairo_save, cr);
}

void
cairo_restore (cairo_t *cr)
{
    _emit_line_info ();
    _emit_cairo_op (cr, "restore\n");
    return DLCALL (cairo_restore, cr);
}

void
cairo_push_group (cairo_t *cr)
{
    _emit_line_info ();
    _emit_cairo_op (cr, "//COLOR_ALPHA push-group\n");
    return DLCALL (cairo_push_group, cr);
}

static const char *
_content_to_string (cairo_content_t content)
{
    switch (content) {
    case CAIRO_CONTENT_ALPHA: return "ALPHA";
    case CAIRO_CONTENT_COLOR: return "COLOR";
    default:
    case CAIRO_CONTENT_COLOR_ALPHA: return "COLOR_ALPHA";
    }
}

void
cairo_push_group_with_content (cairo_t *cr, cairo_content_t content)
{
    _emit_line_info ();
    _emit_cairo_op (cr, "//%s push-group\n", _content_to_string (content));
    return DLCALL (cairo_push_group_with_content, cr, content);
}

cairo_pattern_t *
cairo_pop_group (cairo_t *cr)
{
    cairo_pattern_t *ret;

    ret = DLCALL (cairo_pop_group, cr);

    _emit_line_info ();
    _emit_cairo_op (cr, "pop-group %% p%ld\n", _create_pattern_id (ret));
    _push_operand (PATTERN, ret);

    return ret;
}

void
cairo_pop_group_to_source (cairo_t *cr)
{
    _emit_line_info ();
    _emit_cairo_op (cr, "pop-group set-source\n");
    return DLCALL (cairo_pop_group_to_source, cr);
}

static const char *
_operator_to_string (cairo_operator_t op)
{
    const char *names[] = {
	"CLEAR",	/* CAIRO_OPERATOR_CLEAR */

	"SOURCE",	/* CAIRO_OPERATOR_SOURCE */
	"OVER",		/* CAIRO_OPERATOR_OVER */
	"IN",		/* CAIRO_OPERATOR_IN */
	"OUT",		/* CAIRO_OPERATOR_OUT */
	"ATOP",		/* CAIRO_OPERATOR_ATOP */

	"DEST",		/* CAIRO_OPERATOR_DEST */
	"DEST_OVER",	/* CAIRO_OPERATOR_DEST_OVER */
	"DEST_IN",	/* CAIRO_OPERATOR_DEST_IN */
	"DEST_OUT",	/* CAIRO_OPERATOR_DEST_OUT */
	"DEST_ATOP",	/* CAIRO_OPERATOR_DEST_ATOP */

	"XOR",		/* CAIRO_OPERATOR_XOR */
	"ADD",		/* CAIRO_OPERATOR_ADD */
	"SATURATE"	/* CAIRO_OPERATOR_SATURATE */
    };
    return names[op];
}

void
cairo_set_operator (cairo_t *cr, cairo_operator_t op)
{
    _emit_line_info ();
    _emit_cairo_op (cr, "//%s set-operator\n", _operator_to_string (op));
    return DLCALL (cairo_set_operator, cr, op);
}

void
cairo_set_source_rgb (cairo_t *cr, double red, double green, double blue)
{
    _emit_line_info ();
    _emit_cairo_op (cr, "%g %g %g set-source-rgb\n", red, green, blue);
    return DLCALL (cairo_set_source_rgb, cr, red, green, blue);
}

void
cairo_set_source_rgba (cairo_t *cr, double red, double green, double blue, double alpha)
{
    _emit_line_info ();
    _emit_cairo_op (cr, "%g %g %g %g set-source-rgba\n",
		    red, green, blue, alpha);
    return DLCALL (cairo_set_source_rgba, cr, red, green, blue, alpha);
}

static void
_emit_source_image (cairo_surface_t *surface)
{
    Object *obj;
    cairo_surface_t *image;
    cairo_t *cr;

    obj = _get_object (SURFACE, surface);
    if (obj == NULL)
	return;

    image = DLCALL (cairo_image_surface_create,
		    CAIRO_FORMAT_ARGB32,
		    obj->width,
		    obj->height);
    cr = DLCALL (cairo_create, image);
    DLCALL (cairo_set_source_surface, cr, surface, 0, 0);
    DLCALL (cairo_paint, cr);
    DLCALL (cairo_destroy, cr);

    _emit_image (image, NULL);
    _trace_printf (" set-source-image ");
    DLCALL (cairo_surface_destroy, image);

    obj->foreign = false;
}

static void
_emit_source_image_rectangle (cairo_surface_t *surface,
			      int x, int y,
			      int width, int height)
{
    Object *obj;
    cairo_surface_t *image;
    cairo_t *cr;

    obj = _get_object (SURFACE, surface);
    if (obj == NULL)
	return;

    if (obj->foreign)
	return _emit_source_image (surface);

    image = DLCALL (cairo_image_surface_create,
		    CAIRO_FORMAT_ARGB32,
		    width,
		    height);
    cr = DLCALL (cairo_create, image);
    DLCALL (cairo_set_source_surface, cr, surface, x, y);
    DLCALL (cairo_paint, cr);
    DLCALL (cairo_destroy, cr);

    _emit_image (image, NULL);
    _trace_printf (" %d %d set-device-offset set-source-image ",
	     x, y);
    DLCALL (cairo_surface_destroy, image);
}

void
cairo_set_source_surface (cairo_t *cr, cairo_surface_t *surface, double x, double y)
{
    _emit_line_info ();
    if (cr != NULL && surface != NULL && _write_lock ()) {
	if (_is_current (SURFACE, surface, 0) &&
	    _is_current (CONTEXT, cr, 1))
	{
	    _consume_operand ();
	}
	else if (_is_current (SURFACE, surface, 1) &&
		 _is_current (CONTEXT, cr, 0))
	{
	    _trace_printf ("exch ");
	    _exch_operands ();
	    _consume_operand ();
	} else {
	    _emit_context (cr);
	    _trace_printf ("s%ld ", _get_surface_id (surface));
	}

	if (_get_object (SURFACE, surface)->foreign)
	    _emit_source_image (surface);

	_trace_printf ("pattern");
	if (x != 0. || y != 0.)
	    _trace_printf (" %g %g translate", -x, -y);

	_trace_printf (" set-source\n");
	_write_unlock ();
    }

    return DLCALL (cairo_set_source_surface, cr, surface, x, y);
}

void
cairo_set_source (cairo_t *cr, cairo_pattern_t *source)
{
    _emit_line_info ();
    if (cr != NULL && source != NULL && _write_lock ()) {
	if (_is_current (PATTERN, source, 0) &&
	    _is_current (CONTEXT, cr, 1))
	{
	    _consume_operand ();
	}
	else if (_is_current (PATTERN, source, 1) &&
		 _is_current (CONTEXT, cr, 0))
	{
	    _trace_printf ("exch ");
	    _exch_operands ();
	    _consume_operand ();
	}
	else
	{
	    _emit_context (cr);
	    _emit_pattern_id (source);
	}

	_trace_printf ("set-source\n");
	_write_unlock ();
    }

    return DLCALL (cairo_set_source, cr, source);
}

cairo_pattern_t *
cairo_get_source (cairo_t *cr)
{
    cairo_pattern_t *ret;

    ret = DLCALL (cairo_get_source, cr);

    if (! _has_pattern_id (ret)) {
	_emit_cairo_op (cr, "/source get /p%ld exch def\n",
			_create_pattern_id (ret));
	_get_object (PATTERN, ret)->defined = true;
    }

    return ret;
}

void
cairo_set_tolerance (cairo_t *cr, double tolerance)
{
    _emit_line_info ();
    _emit_cairo_op (cr, "%g set-tolerance\n", tolerance);
    return DLCALL (cairo_set_tolerance, cr, tolerance);
}

static const char *
_antialias_to_string (cairo_antialias_t antialias)
{
    const char *names[] = {
	"ANTIALIAS_DEFAULT",	/* CAIRO_ANTIALIAS_DEFAULT */
	"ANTIALIAS_NONE",	/* CAIRO_ANTIALIAS_NONE */
	"ANTIALIAS_GRAY",	/* CAIRO_ANTIALIAS_GRAY */
	"ANTIALIAS_SUBPIXEL"	/* CAIRO_ANTIALIAS_SUBPIXEL */
    };
    return names[antialias];
}

void
cairo_set_antialias (cairo_t *cr, cairo_antialias_t antialias)
{
    _emit_line_info ();
    _emit_cairo_op (cr,
		    "//%s set-antialias\n", _antialias_to_string (antialias));
    return DLCALL (cairo_set_antialias, cr, antialias);
}

static const char *
_fill_rule_to_string (cairo_fill_rule_t rule)
{
    const char *names[] = {
	"WINDING",	/* CAIRO_FILL_RULE_WINDING */
	"EVEN_ODD"	/* CAIRO_FILL_RILE_EVEN_ODD */
    };
    return names[rule];
}

void
cairo_set_fill_rule (cairo_t *cr, cairo_fill_rule_t fill_rule)
{
    _emit_line_info ();
    _emit_cairo_op (cr,
		    "//%s set-fill-rule\n", _fill_rule_to_string (fill_rule));
    return DLCALL (cairo_set_fill_rule, cr, fill_rule);
}

void
cairo_set_line_width (cairo_t *cr, double width)
{
    _emit_line_info ();
    _emit_cairo_op (cr, "%g set-line-width\n", width);
    return DLCALL (cairo_set_line_width, cr, width);
}

static const char *
_line_cap_to_string (cairo_line_cap_t line_cap)
{
    const char *names[] = {
	"LINE_CAP_BUTT",	/* CAIRO_LINE_CAP_BUTT */
	"LINE_CAP_ROUND",	/* CAIRO_LINE_CAP_ROUND */
	"LINE_CAP_SQUARE"	/* CAIRO_LINE_CAP_SQUARE */
    };
    return names[line_cap];
}

void
cairo_set_line_cap (cairo_t *cr, cairo_line_cap_t line_cap)
{
    _emit_line_info ();
    _emit_cairo_op (cr, "//%s set-line-cap\n", _line_cap_to_string (line_cap));
    return DLCALL (cairo_set_line_cap, cr, line_cap);
}

static const char *
_line_join_to_string (cairo_line_join_t line_join)
{
    const char *names[] = {
	"LINE_JOIN_MITER",	/* CAIRO_LINE_JOIN_MITER */
	"LINE_JOIN_ROUND",	/* CAIRO_LINE_JOIN_ROUND */
	"LINE_JOIN_BEVEL",	/* CAIRO_LINE_JOIN_BEVEL */
    };
    return names[line_join];
}

void
cairo_set_line_join (cairo_t *cr, cairo_line_join_t line_join)
{
    _emit_line_info ();
    _emit_cairo_op (cr,
		    "//%s set-line-join\n", _line_join_to_string (line_join));
    return DLCALL (cairo_set_line_join, cr, line_join);
}

void
cairo_set_dash (cairo_t *cr, const double *dashes, int num_dashes, double offset)
{
    _emit_line_info ();
    if (cr != NULL && _write_lock ()) {
	int n;

	_emit_context (cr);

	_trace_printf ("[");
	for (n = 0; n <  num_dashes; n++) {
	    if (n != 0)
		_trace_printf (" ");
	    _trace_printf ("%g", dashes[n]);
	}
	_trace_printf ("] %g set-dash\n", offset);

	_write_unlock ();
    }

    return DLCALL (cairo_set_dash, cr, dashes, num_dashes, offset);
}

void
cairo_set_miter_limit (cairo_t *cr, double limit)
{
    _emit_line_info ();
    _emit_cairo_op (cr, "%g set-miter-limit\n", limit);
    return DLCALL (cairo_set_miter_limit, cr, limit);
}

void
cairo_translate (cairo_t *cr, double tx, double ty)
{
    _emit_line_info ();
    _emit_cairo_op (cr, "%g %g translate\n", tx, ty);
    return DLCALL (cairo_translate, cr, tx, ty);
}

void
cairo_scale (cairo_t *cr, double sx, double sy)
{
    _emit_line_info ();
    _emit_cairo_op (cr, "%g %g scale\n", sx, sy);
    return DLCALL (cairo_scale, cr, sx, sy);
}

void
cairo_rotate (cairo_t *cr, double angle)
{
    _emit_line_info ();
    _emit_cairo_op (cr, "%g rotate\n", angle);
    return DLCALL (cairo_rotate, cr, angle);
}

void
cairo_transform (cairo_t *cr, const cairo_matrix_t *matrix)
{
    _emit_line_info ();
    _emit_cairo_op (cr, "%g %g %g %g %g %g matrix transform\n",
		    matrix->xx, matrix->yx,
		    matrix->xy, matrix->yy,
		    matrix->x0, matrix->y0);
    return DLCALL (cairo_transform, cr, matrix);
}

void
cairo_set_matrix (cairo_t *cr, const cairo_matrix_t *matrix)
{
    _emit_line_info ();
    if (_matrix_is_identity (matrix)) {
	_emit_cairo_op (cr, "identity set-matrix\n");
    } else {
	_emit_cairo_op (cr, "%g %g %g %g %g %g matrix set-matrix\n",
			matrix->xx, matrix->yx,
			matrix->xy, matrix->yy,
			matrix->x0, matrix->y0);
    }
    return DLCALL (cairo_set_matrix, cr, matrix);
}

cairo_surface_t *
cairo_get_target (cairo_t *cr)
{
    cairo_surface_t *ret;
    long surface_id;

    ret = DLCALL (cairo_get_target, cr);
    surface_id = _create_surface_id (ret);

    if (cr != NULL && ! _has_surface_id (ret)) {
	_emit_cairo_op (cr, "/target get /s%ld exch def\n", surface_id);
	_get_object (SURFACE, ret)->defined = true;
    }

    return ret;
}

cairo_surface_t *
cairo_get_group_target (cairo_t *cr)
{
    cairo_surface_t *ret;
    long surface_id;

    ret = DLCALL (cairo_get_group_target, cr);
    surface_id = _create_surface_id (ret);

    if (cr != NULL && ! _has_surface_id (ret)) {
	_emit_cairo_op (cr, "/group-target get /s%ld exch def\n", surface_id);
	_get_object (SURFACE, ret)->defined = true;
    }

    return ret;
}

void
cairo_identity_matrix (cairo_t *cr)
{
    _emit_line_info ();
    _emit_cairo_op (cr, "identity set-matrix\n");
    return DLCALL (cairo_identity_matrix, cr);
}


void
cairo_new_path (cairo_t *cr)
{
    _emit_line_info ();
    _emit_cairo_op (cr, "n ");
    return DLCALL (cairo_new_path, cr);
}

void
cairo_move_to (cairo_t *cr, double x, double y)
{
    _emit_cairo_op (cr, "%g %g m ", x, y);
    return DLCALL (cairo_move_to, cr, x, y);
}

void
cairo_new_sub_path (cairo_t *cr)
{
    _emit_cairo_op (cr, "N ");
    return DLCALL (cairo_new_sub_path, cr);
}

void
cairo_line_to (cairo_t *cr, double x, double y)
{
    _emit_cairo_op (cr, "%g %g l ", x, y);
    return DLCALL (cairo_line_to, cr, x, y);
}

void
cairo_curve_to (cairo_t *cr, double x1, double y1, double x2, double y2, double x3, double y3)
{
    _emit_cairo_op (cr, "%g %g %g %g %g %g c ", x1, y1, x2, y2, x3, y3);
    return DLCALL (cairo_curve_to, cr, x1, y1, x2, y2, x3, y3);
}

void
cairo_arc (cairo_t *cr, double xc, double yc, double radius, double angle1, double angle2)
{
    _emit_cairo_op (cr, "%g %g %g %g %g arc\n", xc, yc, radius, angle1, angle2);
    return DLCALL (cairo_arc, cr, xc, yc, radius, angle1, angle2);
}

void
cairo_arc_negative (cairo_t *cr, double xc, double yc, double radius, double angle1, double angle2)
{
    _emit_cairo_op (cr, "%g %g %g %g %g arc-\n",
		    xc, yc, radius, angle1, angle2);
    return DLCALL (cairo_arc_negative, cr, xc, yc, radius, angle1, angle2);
}

void
cairo_rel_move_to (cairo_t *cr, double dx, double dy)
{
    _emit_cairo_op (cr, "%g %g M ", dx, dy);
    return DLCALL (cairo_rel_move_to, cr, dx, dy);
}

void
cairo_rel_line_to (cairo_t *cr, double dx, double dy)
{
    _emit_cairo_op (cr, "%g %g L ", dx, dy);
    return DLCALL (cairo_rel_line_to, cr, dx, dy);
}

void
cairo_rel_curve_to (cairo_t *cr, double dx1, double dy1, double dx2, double dy2, double dx3, double dy3)
{
    _emit_cairo_op (cr, "%g %g %g %g %g %g C ",
		    dx1, dy1, dx2, dy2, dx3, dy3);
    return DLCALL (cairo_rel_curve_to, cr, dx1, dy1, dx2, dy2, dx3, dy3);
}

void
cairo_rectangle (cairo_t *cr, double x, double y, double width, double height)
{
    _emit_cairo_op (cr, "%g %g %g %g rectangle\n", x, y, width, height);
    return DLCALL (cairo_rectangle, cr, x, y, width, height);
}

void
cairo_close_path (cairo_t *cr)
{
    _emit_cairo_op (cr, "h\n");
    return DLCALL (cairo_close_path, cr);
}

void
cairo_paint (cairo_t *cr)
{
    _emit_line_info ();
    _emit_cairo_op (cr, "paint\n");
    DLCALL (cairo_paint, cr);
}

void
cairo_paint_with_alpha (cairo_t *cr, double alpha)
{
    _emit_line_info ();
    _emit_cairo_op (cr, "%g paint-with-alpha\n", alpha);
    DLCALL (cairo_paint_with_alpha, cr, alpha);
}

void
cairo_mask (cairo_t *cr, cairo_pattern_t *pattern)
{
    _emit_line_info ();
    if (cr != NULL && pattern != NULL && _write_lock ()) {
	if (_is_current (PATTERN, pattern, 0) &&
	    _is_current (CONTEXT, cr, 1))
	{
	    _consume_operand ();
	}
	else if (_is_current (PATTERN, pattern, 1) &&
		 _is_current (CONTEXT, cr, 0))
	{
	    _trace_printf ("exch ");
	    _exch_operands ();
	    _consume_operand ();
	} else {
	    _emit_context (cr);
	    _emit_pattern_id (pattern);
	}

	_trace_printf (" mask\n");
	_write_unlock ();
    }
    DLCALL (cairo_mask, cr, pattern);
}

void
cairo_mask_surface (cairo_t *cr, cairo_surface_t *surface, double x, double y)
{
    _emit_line_info ();
    if (cr != NULL && surface != NULL && _write_lock ()) {
	if (_is_current (SURFACE, surface, 0) &&
	    _is_current (CONTEXT, cr, 1))
	{
	    _consume_operand ();
	}
	else if (_is_current (SURFACE, surface, 1) &&
		 _is_current (CONTEXT, cr, 0))
	{
	    _trace_printf ("exch ");
	    _exch_operands ();
	    _consume_operand ();
	} else {
	    _emit_context (cr);
	    _trace_printf ("s%ld ", _get_surface_id (surface));
	}
	_trace_printf ("pattern");

	if (x != 0. || y != 0.)
	    _trace_printf (" %g %g translate", -x, -y);

	_trace_printf (" mask\n");
	_write_unlock ();
    }

    DLCALL (cairo_mask_surface, cr, surface, x, y);
}

void
cairo_stroke (cairo_t *cr)
{
    _emit_line_info ();
    _emit_cairo_op (cr, "stroke\n");
    DLCALL (cairo_stroke, cr);
}

void
cairo_stroke_preserve (cairo_t *cr)
{
    _emit_line_info ();
    _emit_cairo_op (cr, "stroke+\n");
    DLCALL (cairo_stroke_preserve, cr);
}

void
cairo_fill (cairo_t *cr)
{
    _emit_line_info ();
    _emit_cairo_op (cr, "fill\n");
    DLCALL (cairo_fill, cr);
}

void
cairo_fill_preserve (cairo_t *cr)
{
    _emit_line_info ();
    _emit_cairo_op (cr, "fill+\n");
    DLCALL (cairo_fill_preserve, cr);
}

void
cairo_copy_page (cairo_t *cr)
{
    _emit_line_info ();
    _emit_cairo_op (cr, "copy-page\n");
    return DLCALL (cairo_copy_page, cr);
}

void
cairo_show_page (cairo_t *cr)
{
    _emit_line_info ();
    _emit_cairo_op (cr, "show-page\n");
    return DLCALL (cairo_show_page, cr);
}

void
cairo_clip (cairo_t *cr)
{
    _emit_line_info ();
    _emit_cairo_op (cr, "clip\n");
    DLCALL (cairo_clip, cr);
}

void
cairo_clip_preserve (cairo_t *cr)
{
    _emit_line_info ();
    _emit_cairo_op (cr, "clip+\n");
    DLCALL (cairo_clip_preserve, cr);
}

void
cairo_reset_clip (cairo_t *cr)
{
    _emit_line_info ();
    _emit_cairo_op (cr, "reset-clip\n");
    return DLCALL (cairo_reset_clip, cr);
}


static const char *
_slant_to_string (cairo_font_slant_t font_slant)
{
    const char *names[] = {
	"SLANT_NORMAL",		/* CAIRO_FONT_SLANT_NORMAL */
	"SLANT_ITALIC",		/* CAIRO_FONT_SLANT_ITALIC */
	"SLANT_OBLIQUE"		/* CAIRO_FONT_SLANT_OBLIQUE */
    };
    return names[font_slant];
}

static const char *
_weight_to_string (cairo_font_weight_t font_weight)
{
    const char *names[] = {
	"WEIGHT_NORMAL",	/* CAIRO_FONT_WEIGHT_NORMAL */
	"WEIGHT_BOLD",		/* CAIRO_FONT_WEIGHT_BOLD */
    };
    return names[font_weight];
}

void
cairo_select_font_face (cairo_t *cr, const char *family, cairo_font_slant_t slant, cairo_font_weight_t weight)
{
    _emit_line_info ();
    if (cr != NULL && _write_lock ()) {
	_emit_context (cr);
	_emit_string_literal (family, -1);
	_trace_printf (" //%s //%s select-font-face\n",
		       _slant_to_string (slant),
		       _weight_to_string (weight));
	_write_unlock ();
    }
    return DLCALL (cairo_select_font_face, cr, family, slant, weight);
}

cairo_font_face_t *
cairo_get_font_face (cairo_t *cr)
{
    cairo_font_face_t *ret;
    long font_face_id;

    ret = DLCALL (cairo_get_font_face, cr);
    font_face_id = _create_font_face_id (ret);

    _emit_cairo_op (cr, "/font-face get\n");
    _push_operand (FONT_FACE, ret);

    return ret;
}

void
cairo_set_font_face (cairo_t *cr, cairo_font_face_t *font_face)
{
    _emit_line_info ();
    if (cr != NULL && font_face != NULL && _write_lock ()) {
	if (_is_current (FONT_FACE, font_face, 0) &&
	    _is_current (CONTEXT, cr, 1))
	{
	    _consume_operand ();
	}
	else if (_is_current (FONT_FACE, font_face, 1) &&
		 _is_current (CONTEXT, cr, 0))
	{
	    _trace_printf ("exch ");
	    _exch_operands ();
	    _consume_operand ();
	}
	else
	{
	    _emit_context (cr);
	    _emit_font_face_id (font_face);
	}

	_trace_printf ("set-font-face\n");
	_write_unlock ();
    }

    return DLCALL (cairo_set_font_face, cr, font_face);
}

void
cairo_set_font_size (cairo_t *cr, double size)
{
    _emit_line_info ();
    _emit_cairo_op (cr, "%g set-font-size\n", size);
    return DLCALL (cairo_set_font_size, cr, size);
}

void
cairo_set_font_matrix (cairo_t *cr, const cairo_matrix_t *matrix)
{
    _emit_line_info ();
    _emit_cairo_op (cr, "%g %g %g %g %g %g matrix set-font-matrix\n",
		    matrix->xx, matrix->yx,
		    matrix->xy, matrix->yy,
		    matrix->x0, matrix->y0);
    return DLCALL (cairo_set_font_matrix, cr, matrix);
}

static const char *
_subpixel_order_to_string (cairo_subpixel_order_t subpixel_order)
{
    const char *names[] = {
	"SUBPIXEL_ORDER_DEFAULT",	/* CAIRO_SUBPIXEL_ORDER_DEFAULT */
	"SUBPIXEL_ORDER_RGB",		/* CAIRO_SUBPIXEL_ORDER_RGB */
	"SUBPIXEL_ORDER_BGR",		/* CAIRO_SUBPIXEL_ORDER_BGR */
	"SUBPIXEL_ORDER_VRGB",		/* CAIRO_SUBPIXEL_ORDER_VRGB */
	"SUBPIXEL_ORDER_VBGR"		/* CAIRO_SUBPIXEL_ORDER_VBGR */
    };
    return names[subpixel_order];
}
static const char *
_hint_style_to_string (cairo_hint_style_t hint_style)
{
    const char *names[] = {
	"HINT_STYLE_DEFAULT",	/* CAIRO_HINT_STYLE_DEFAULT */
	"HINT_STYLE_NONE",	/* CAIRO_HINT_STYLE_NONE */
	"HINT_STYLE_SLIGHT",	/* CAIRO_HINT_STYLE_SLIGHT */
	"HINT_STYLE_MEDIUM",	/* CAIRO_HINT_STYLE_MEDIUM */
	"HINT_STYLE_FULL"	/* CAIRO_HINT_STYLE_FULL */
    };
    return names[hint_style];
}
static const char *
_hint_metrics_to_string (cairo_hint_metrics_t hint_metrics)
{
    const char *names[] = {
	 "HINT_METRICS_DEFAULT",	/* CAIRO_HINT_METRICS_DEFAULT */
	 "HINT_METRICS_OFF",		/* CAIRO_HINT_METRICS_OFF */
	 "HINT_METRICS_ON"		/* CAIRO_HINT_METRICS_ON */
    };
    return names[hint_metrics];
}
static void
_emit_font_options (const cairo_font_options_t *options)
{
    cairo_antialias_t antialias;
    cairo_subpixel_order_t subpixel_order;
    cairo_hint_style_t hint_style;
    cairo_hint_metrics_t hint_metrics;

    _trace_printf ("dict\n");

    antialias = cairo_font_options_get_antialias (options);
    if (antialias != CAIRO_ANTIALIAS_DEFAULT) {
	_trace_printf ("  /antialias //%s set\n",
		       _antialias_to_string (antialias));
    }

    subpixel_order = cairo_font_options_get_subpixel_order (options);
    if (subpixel_order != CAIRO_SUBPIXEL_ORDER_DEFAULT) {
	_trace_printf ("  /subpixel-order //%s set\n",
		       _subpixel_order_to_string (subpixel_order));
    }

    hint_style = cairo_font_options_get_hint_style (options);
    if (hint_style != CAIRO_HINT_STYLE_DEFAULT) {
	_trace_printf ("  /hint-style //%s set\n",
		       _hint_style_to_string (hint_style));
    }

    hint_metrics = cairo_font_options_get_hint_metrics (options);
    if (hint_style != CAIRO_HINT_METRICS_DEFAULT) {
	_trace_printf ("  /hint-metrics //%s set\n",
		       _hint_metrics_to_string (hint_metrics));
    }
}

void
cairo_set_font_options (cairo_t *cr, const cairo_font_options_t *options)
{
    _emit_line_info ();
    if (cr != NULL && options != NULL && _write_lock ()) {
	_emit_context (cr);
	_emit_font_options (options);
	_trace_printf ("  set-font-options\n");
	_write_unlock ();
    }

    return DLCALL (cairo_set_font_options, cr, options);
}

cairo_scaled_font_t *
cairo_get_scaled_font (cairo_t *cr)
{
    cairo_scaled_font_t *ret;

    ret = DLCALL (cairo_get_scaled_font, cr);

    if (cr != NULL && ! _has_scaled_font_id (ret)) {
	_emit_cairo_op (cr, "/scaled-font get /sf%ld exch def\n",
			_create_scaled_font_id (ret));
	_get_object (SCALED_FONT, ret)->defined = true;
    }

    return ret;
}

void
cairo_set_scaled_font (cairo_t *cr, const cairo_scaled_font_t *scaled_font)
{
    _emit_line_info ();
    if (cr != NULL && scaled_font != NULL) {
	if (_pop_operands_to (SCALED_FONT, scaled_font)) {
	    if (_is_current (CONTEXT, cr, 1)) {
		if (_write_lock ()) {
		    _consume_operand ();
		    _trace_printf ("set-scaled-font\n");
		    _write_unlock ();
		}
	    } else {
		if (_get_object (CONTEXT, cr)->defined) {
		    if (_write_lock ()) {
			_consume_operand ();
			_trace_printf ("c%ld exch set-scaled-font pop\n",
				       _get_context_id (cr));
			_write_unlock ();
		    }
		} else {
		    _emit_cairo_op (cr, "sf%ld set-scaled-font\n",
				    _get_scaled_font_id (scaled_font));
		}
	    }
	} else {
	    _emit_cairo_op (cr, "sf%ld set-scaled-font\n",
			    _get_scaled_font_id (scaled_font));
	}
    }
    return DLCALL (cairo_set_scaled_font, cr, scaled_font);
}

static void
_emit_matrix (const cairo_matrix_t *m)
{
    if (m->xx == 1.0 && m->yx == 0.0 &&
	m->xy == 0.0 && m->yy == 1.0 &&
	m->x0 == 0.0 && m->y0 == 0.0)
    {
	_trace_printf ("identity");
    }
    else
    {
	_trace_printf ("%g %g %g %g %g %g matrix",
		       m->xx, m->yx,
		       m->xy, m->yy,
		       m->x0, m->y0);
    }
}

cairo_scaled_font_t *
cairo_scaled_font_create (cairo_font_face_t *font_face,
			  const cairo_matrix_t *font_matrix,
			  const cairo_matrix_t *ctm,
			  const cairo_font_options_t *options)
{
    cairo_scaled_font_t *ret;
    long scaled_font_id;

    ret = DLCALL (cairo_scaled_font_create, font_face, font_matrix, ctm, options);
    scaled_font_id = _create_scaled_font_id (ret);

    _emit_line_info ();
    if (font_face != NULL &&
	font_matrix != NULL &&
	ctm != NULL &&
	options != NULL
	&& _write_lock ())
    {
	if (_pop_operands_to (FONT_FACE, font_face))
	    _consume_operand ();
	else
	    _trace_printf ("f%ld ", _get_font_face_id (font_face));

	_emit_matrix (font_matrix);
	_trace_printf (" ");

	_emit_matrix (ctm);
	_trace_printf (" ");

	_emit_font_options (options);

	if (_get_object (SCALED_FONT, ret)->defined) {
	    _trace_printf ("  scaled-font pop %% sf%ld\n",
			   scaled_font_id);
	} else {
	    _trace_printf ("  scaled-font dup /sf%ld exch def\n",
			   scaled_font_id);
	    _push_operand (SCALED_FONT, ret);

	    _get_object (SCALED_FONT, ret)->defined = true;
	}

	_write_unlock ();
    }

    return ret;
}

void
cairo_show_text (cairo_t *cr, const char *utf8)
{
    _emit_line_info ();
    if (cr != NULL && _write_lock ()) {
	_emit_context (cr);
	_emit_string_literal (utf8, -1);
	_trace_printf (" show-text\n");
	_write_unlock ();
    }
    DLCALL (cairo_show_text, cr, utf8);
}

#define TOLERANCE 1e-5
static void
_emit_glyphs (cairo_scaled_font_t *font,
	      const cairo_glyph_t *glyphs,
	      int num_glyphs)
{
    double x,y;
    int n;

    if (num_glyphs == 0) {
	_trace_printf ("[]");
	return;
    }

    for (n = 0; n < num_glyphs; n++) {
	if (glyphs[n].index > 255)
	    break;
    }

    x = glyphs->x;
    y = glyphs->y;
    if (n < num_glyphs) { /* need full glyph range */
	bool first;

	_trace_printf ("[%g %g [", x, y);
	first = true;
	while (num_glyphs--) {
	    cairo_text_extents_t extents;

	    if (fabs (glyphs->x - x) > TOLERANCE ||
		fabs (glyphs->y - y) > TOLERANCE)
	    {
		x = glyphs->x;
		y = glyphs->y;
		_trace_printf ("] %g %g [", x, y);
		first = true;
	    }

	    if (! first)
		_trace_printf (" ");
	    _trace_printf ("%lu", glyphs->index);
	    first = false;

	    cairo_scaled_font_glyph_extents (font, glyphs, 1, &extents);
	    x += extents.x_advance;
	    y += extents.y_advance;

	    glyphs++;
	}
	_trace_printf ("]]");
    } else {
	struct _data_stream stream;

	if (num_glyphs == 1) {
	    _trace_printf ("[%g %g <%02lx>]", x, y,  glyphs->index);
	} else {
	    _trace_printf ("[%g %g <~", x, y);
	    _write_base85_data_start (&stream);
	    while (num_glyphs--) {
		cairo_text_extents_t extents;
		unsigned char c;

		if (fabs (glyphs->x - x) > TOLERANCE ||
		    fabs (glyphs->y - y) > TOLERANCE)
		{
		    x = glyphs->x;
		    y = glyphs->y;
		    _write_base85_data_end (&stream);
		    _trace_printf ("~> %g %g <~", x, y);
		    _write_base85_data_start (&stream);
		}

		c = glyphs->index;
		_write_base85_data (&stream, &c, 1);

		cairo_scaled_font_glyph_extents (font, glyphs, 1, &extents);
		x += extents.x_advance;
		y += extents.y_advance;

		glyphs++;
	    }
	    _write_base85_data_end (&stream);
	    _trace_printf ("~>]");
	}
    }
}

void
cairo_show_glyphs (cairo_t *cr, const cairo_glyph_t *glyphs, int num_glyphs)
{
    _emit_line_info ();
    if (cr != NULL && glyphs != NULL && _write_lock ()) {
	cairo_scaled_font_t *font;

	_emit_context (cr);
	font = DLCALL (cairo_get_scaled_font, cr);

	_emit_glyphs (font, glyphs, num_glyphs);
	_trace_printf (" show-glyphs\n");
	_write_unlock ();
    }

    DLCALL (cairo_show_glyphs, cr, glyphs, num_glyphs);
}

static const char *
_direction_to_string (cairo_bool_t backward)
{
    const char *names[] = {
	"FORWARD",
	"BACKWARD"
    };
    return names[backward];
}

void
cairo_show_text_glyphs (cairo_t			   *cr,
			const char		   *utf8,
			int			    utf8_len,
			const cairo_glyph_t	   *glyphs,
			int			    num_glyphs,
			const cairo_text_cluster_t *clusters,
			int			    num_clusters,
			cairo_text_cluster_flags_t  backward)
{
    cairo_scaled_font_t *font;

    font = DLCALL (cairo_get_scaled_font, cr);

    _emit_line_info ();
    if (cr != NULL && glyphs != NULL && clusters != NULL && _write_lock ()) {
	int n;

	_emit_context (cr);

	_emit_string_literal (utf8, utf8_len);

	_emit_glyphs (font, glyphs, num_glyphs);
	_trace_printf ("  [");
	for (n = 0; n < num_clusters; n++) {
	    _trace_printf (" %d %d",
			   clusters[n].num_bytes,
			   clusters[n].num_glyphs);
	}
	_trace_printf (" ] //%s show-text-glyphs\n",
		       _direction_to_string (backward));

	_write_unlock ();
    }

    DLCALL (cairo_show_text_glyphs, cr,
	                            utf8, utf8_len,
				    glyphs, num_glyphs,
				    clusters, num_clusters,
				    backward);
}

void
cairo_text_path (cairo_t *cr, const char *utf8)
{
    _emit_line_info ();
    if (cr != NULL && _write_lock ()) {
	_emit_context (cr);
	_emit_string_literal (utf8, -1);
	_trace_printf (" text-path\n");
	_write_unlock ();
    }
    return DLCALL (cairo_text_path, cr, utf8);
}

void
cairo_glyph_path (cairo_t *cr, const cairo_glyph_t *glyphs, int num_glyphs)
{
    cairo_scaled_font_t *font;

    font = DLCALL (cairo_get_scaled_font, cr);

    _emit_line_info ();
    if (cr != NULL && glyphs != NULL && _write_lock ()) {
	_emit_context (cr);
	_emit_glyphs (font, glyphs, num_glyphs);
	_trace_printf (" glyph-path\n");

	_write_unlock ();
    }

    return DLCALL (cairo_glyph_path, cr, glyphs, num_glyphs);
}

void
cairo_append_path (cairo_t *cr, const cairo_path_t *path)
{
    /* XXX no support for named paths, so manually reconstruct */
    int i;
    cairo_path_data_t *p;

    _emit_line_info ();
    if (cr == NULL || path == NULL)
	return DLCALL (cairo_append_path, cr, path);

    for (i=0; i < path->num_data; i += path->data[i].header.length) {
	p = &path->data[i];
	switch (p->header.type) {
	case CAIRO_PATH_MOVE_TO:
	    if (p->header.length >= 2)
		cairo_move_to (cr, p[1].point.x, p[1].point.y);
	    break;
	case CAIRO_PATH_LINE_TO:
	    if (p->header.length >= 2)
		cairo_line_to (cr, p[1].point.x, p[1].point.y);
	    break;
	case CAIRO_PATH_CURVE_TO:
	    if (p->header.length >= 4)
		cairo_curve_to (cr,
				p[1].point.x, p[1].point.y,
				p[2].point.x, p[2].point.y,
				p[3].point.x, p[3].point.y);
	    break;
	case CAIRO_PATH_CLOSE_PATH:
	    if (p->header.length >= 1)
		cairo_close_path (cr);
	    break;
	default:
	    break;
	}
    }
}

cairo_surface_t *
cairo_image_surface_create (cairo_format_t format, int width, int height)
{
    cairo_surface_t *ret;
    long surface_id;
    const char *format_str;

    ret = DLCALL (cairo_image_surface_create, format, width, height);

    surface_id = _create_surface_id (ret);
    format_str = _format_to_string (format);

    _emit_line_info ();
    if (_write_lock ()) {
	_trace_printf ("dict\n"
		       "  /width %d set\n"
		       "  /height %d set\n"
		       "  /format //%s set\n"
		       "  image dup /s%ld exch def\n",
		       width, height, format_str, surface_id);
	_get_object (SURFACE, ret)->width = width;
	_get_object (SURFACE, ret)->height = height;
	_get_object (SURFACE, ret)->defined = true;
	_push_operand (SURFACE, ret);
	_write_unlock ();
    }

    return ret;
}

cairo_surface_t *
cairo_image_surface_create_for_data (unsigned char *data, cairo_format_t format, int width, int height, int stride)
{
    cairo_surface_t *ret;
    long surface_id;

    ret = DLCALL (cairo_image_surface_create_for_data, data, format, width, height, stride);
    surface_id = _create_surface_id (ret);

    _emit_line_info ();
    if (_write_lock ()) {
	/* cairo_image_surface_create_for_data() is both used to supply
	 * foreign pixel data to cairo and in order to read pixels back.
	 * Defer grabbing the pixel contents until we have to, but only for
	 * "large" images, for small images the overhead of embedding pixels
	 * is negligible.
	 */
	if (width * height < 128) {
	    _emit_image (ret, NULL);
	    _trace_printf (" dup /s%ld exch def\n",
			   surface_id);
	} else {
	    _trace_printf ("dict\n"
			   "  /width %d set\n"
			   "  /height %d set\n"
			   "  /format //%s set\n"
			   "  image dup /s%ld exch def\n",
			   width, height,
			   _format_to_string (format),
			   surface_id);

	    _get_object (SURFACE, ret)->foreign = true;
	}

	_get_object (SURFACE, ret)->width  = width;
	_get_object (SURFACE, ret)->height = height;
	_get_object (SURFACE, ret)->defined = true;
	_push_operand (SURFACE, ret);
	_write_unlock ();
    }

    return ret;
}

cairo_surface_t *
cairo_surface_create_similar (cairo_surface_t *other,
			      cairo_content_t content,
			      int width, int height)
{
    cairo_surface_t *ret;
    long other_id;
    long surface_id;

    ret = DLCALL (cairo_surface_create_similar, other, content, width, height);
    surface_id = _create_surface_id (ret);

    _emit_line_info ();
    if (other != NULL && _write_lock ()) {
	other_id = _get_surface_id (other);

	if (_pop_operands_to (SURFACE, other)) {
	    _consume_operand ();
	    _trace_printf ("%d %d //%s similar\n",
			   width,
			   height,
			   _content_to_string (content));
	} else {
	    _trace_printf ("s%ld %d %d //%s similar\n",
			   other_id,
			   width,
			   height,
			   _content_to_string (content));
	}
	_push_operand (SURFACE, ret);
	_write_unlock ();
    }

    return ret;
}

static void CAIRO_PRINTF_FORMAT(2, 3)
_emit_surface_op (cairo_surface_t *surface, const char *fmt, ...)
{
    va_list ap;

    if (surface == NULL || ! _write_lock ())
	return;

    _emit_surface (surface);

    va_start (ap, fmt);
    _trace_vprintf ( fmt, ap);
    va_end (ap);

    _write_unlock ();
}

void
cairo_surface_finish (cairo_surface_t *surface)
{
    _emit_line_info ();
    return DLCALL (cairo_surface_finish, surface);
}

void
cairo_surface_flush (cairo_surface_t *surface)
{
    _emit_line_info ();
    return DLCALL (cairo_surface_flush, surface);
}

void
cairo_surface_mark_dirty (cairo_surface_t *surface)
{
    _emit_line_info ();
    if (surface != NULL && _write_lock ()) {
	if (_mark_dirty) {
	    _emit_surface (surface);
	    _trace_printf ("%% mark-dirty\n");
	    _emit_source_image (surface);
	} else
	    _trace_printf ("%% s%ld mark-dirty\n", _get_surface_id (surface));
	_write_unlock ();
    }

    return DLCALL (cairo_surface_mark_dirty, surface);
}

void
cairo_surface_mark_dirty_rectangle (cairo_surface_t *surface,
				    int x, int y, int width, int height)
{
    _emit_line_info ();
    if (surface != NULL && _write_lock ()) {
	if (_mark_dirty) {
	    _emit_surface (surface);
	    _trace_printf ("%% %d %d %d %d mark-dirty-rectangle\n",
		            x, y, width, height);
	    _emit_source_image_rectangle (surface, x,y, width, height);
	} else
	    _trace_printf ("%% s%ld %d %d %d %d mark-dirty-rectangle\n",
		           _get_surface_id (surface), x, y, width, height);
	_write_unlock ();
    }

    return DLCALL (cairo_surface_mark_dirty_rectangle, surface, x, y, width, height);
}

void
cairo_surface_set_device_offset (cairo_surface_t *surface, double x_offset, double y_offset)
{
    _emit_line_info ();
    _emit_surface_op (surface, "%g %g set-device-offset\n",
		      x_offset, y_offset);
    return DLCALL (cairo_surface_set_device_offset, surface, x_offset, y_offset);
}

void
cairo_surface_set_fallback_resolution (cairo_surface_t *surface, double x_pixels_per_inch, double y_pixels_per_inch)
{
    _emit_line_info ();
    _emit_surface_op (surface, "%g %g set-fallback-resolution\n",
		      x_pixels_per_inch, y_pixels_per_inch);
    return DLCALL (cairo_surface_set_fallback_resolution, surface, x_pixels_per_inch, y_pixels_per_inch);
}

void
cairo_surface_copy_page (cairo_surface_t *surface)
{
    _emit_line_info ();
    _emit_surface_op (surface, "copy-page\n");
    return DLCALL (cairo_surface_copy_page, surface);
}

void
cairo_surface_show_page (cairo_surface_t *surface)
{
    _emit_line_info ();
    _emit_surface_op (surface, "show-page\n");
    return DLCALL (cairo_surface_show_page, surface);
}

cairo_status_t
cairo_surface_set_mime_data (cairo_surface_t		*surface,
                             const char			*mime_type,
                             const unsigned char	*data,
                             unsigned int		 length,
			     cairo_destroy_func_t	 destroy,
			     void			*closure)
{
    _emit_line_info ();
    if (surface != NULL && _write_lock ()) {
	_emit_surface (surface);
	_emit_string_literal (mime_type, -1);
	_trace_printf (" ");
	_emit_data (data, length);
	_trace_printf (" /deflate filter set-mime-data\n");

	_write_unlock ();
    }

    return DLCALL (cairo_surface_set_mime_data,
		   surface,
		   mime_type,
		   data, length,
		   destroy,
		   closure);
}

#if CAIRO_HAS_PNG_FUNCTIONS
cairo_status_t
cairo_surface_write_to_png (cairo_surface_t *surface, const char *filename)
{
    _emit_line_info ();
    if (surface != NULL && _write_lock ()) {
	_trace_printf ("%% s%ld ", _get_surface_id (surface));
	_emit_string_literal (filename, -1);
	_trace_printf (" write-to-png\n");
	_write_unlock ();
    }
    return DLCALL (cairo_surface_write_to_png, surface, filename);
}

cairo_status_t
cairo_surface_write_to_png_stream (cairo_surface_t *surface,
				   cairo_write_func_t write_func,
				   void *data)
{
    _emit_line_info ();
    if (surface != NULL && _write_lock ()) {
	char symbol[1024];

	_trace_printf ("%% s%ld ", _get_surface_id (surface));
	_emit_string_literal (lookup_symbol (symbol, sizeof (symbol),
					     write_func),
			      -1);
	_trace_printf (" write-to-png-stream\n");
	_write_unlock ();
    }
    return DLCALL (cairo_surface_write_to_png_stream,
		   surface, write_func, data);
}
#endif

static void CAIRO_PRINTF_FORMAT(2, 3)
_emit_pattern_op (cairo_pattern_t *pattern, const char *fmt, ...)
{
    va_list ap;

    if (pattern == NULL || ! _write_lock ())
	return;

    _emit_pattern (pattern);

    va_start (ap, fmt);
    _trace_vprintf (fmt, ap);
    va_end (ap);

    _write_unlock ();
}

cairo_pattern_t *
cairo_pattern_create_rgb (double red, double green, double blue)
{
    cairo_pattern_t *ret;
    long pattern_id;

    ret = DLCALL (cairo_pattern_create_rgb, red, green, blue);
    pattern_id = _create_pattern_id (ret);

    _emit_line_info ();
    if (_write_lock ()) {
	_trace_printf ("/p%ld %g %g %g rgb def\n",
		       pattern_id, red, green, blue);
	_get_object (PATTERN, ret)->defined = true;
	_write_unlock ();
    }

    return ret;
}

cairo_pattern_t *
cairo_pattern_create_rgba (double red, double green, double blue, double alpha)
{
    cairo_pattern_t *ret;
    long pattern_id;

    ret = DLCALL (cairo_pattern_create_rgba, red, green, blue, alpha);
    pattern_id = _create_pattern_id (ret);

    _emit_line_info ();
    if (_write_lock ()) {
	_trace_printf ("/p%ld %g %g %g %g rgba def\n",
		       pattern_id, red, green, blue, alpha);
	_get_object (PATTERN, ret)->defined = true;
	_write_unlock ();
    }

    return ret;
}

cairo_pattern_t *
cairo_pattern_create_for_surface (cairo_surface_t *surface)
{
    cairo_pattern_t *ret;
    long pattern_id;
    long surface_id;

    ret = DLCALL (cairo_pattern_create_for_surface, surface);
    pattern_id = _create_pattern_id (ret);

    _emit_line_info ();
    if (surface != NULL && _write_lock ()) {
	surface_id = _get_surface_id (surface);

	if (_pop_operands_to (SURFACE, surface)) {
	    _consume_operand ();
	} else {
	    _trace_printf ("s%ld ", surface_id);
	}

	if (_get_object (SURFACE, surface)->foreign)
	    _emit_source_image (surface);

	_trace_printf ("pattern %% p%ld\n", pattern_id);
	_push_operand (PATTERN, ret);
	_write_unlock ();
    }

    return ret;
}

cairo_pattern_t *
cairo_pattern_create_linear (double x0, double y0, double x1, double y1)
{
    cairo_pattern_t *ret;
    long pattern_id;

    ret = DLCALL (cairo_pattern_create_linear, x0, y0, x1, y1);
    pattern_id = _create_pattern_id (ret);

    _emit_line_info ();
    if (_write_lock ()) {
	_trace_printf ("%g %g %g %g linear %% p%ld\n",
		       x0, y0, x1, y1, pattern_id);
	_push_operand (PATTERN, ret);
	_write_unlock ();
    }

    return ret;
}

cairo_pattern_t *
cairo_pattern_create_radial (double cx0, double cy0, double radius0, double cx1, double cy1, double radius1)
{
    cairo_pattern_t *ret;
    long pattern_id;

    ret = DLCALL (cairo_pattern_create_radial,
		  cx0, cy0, radius0,
		  cx1, cy1, radius1);
    pattern_id = _create_pattern_id (ret);

    _emit_line_info ();
    if (_write_lock ()) {
	_trace_printf ("%g %g %g %g %g %g radial %% p%ld\n",
		       cx0, cy0, radius0, cx1, cy1, radius1,
		       pattern_id);
	_push_operand (PATTERN, ret);
	_write_unlock ();
    }

    return ret;
}

void
cairo_pattern_add_color_stop_rgb (cairo_pattern_t *pattern, double offset, double red, double green, double blue)
{
    _emit_line_info ();
    _emit_pattern_op (pattern,
		      "%g %g %g %g 1 add-color-stop\n",
		      offset, red, green, blue);
    return DLCALL (cairo_pattern_add_color_stop_rgb, pattern, offset, red, green, blue);
}

void
cairo_pattern_add_color_stop_rgba (cairo_pattern_t *pattern, double offset, double red, double green, double blue, double alpha)
{
    _emit_line_info ();
    _emit_pattern_op (pattern,
		      "%g %g %g %g %g add-color-stop\n",
		      offset, red, green, blue, alpha);
    return DLCALL (cairo_pattern_add_color_stop_rgba, pattern, offset, red, green, blue, alpha);
}

void
cairo_pattern_set_matrix (cairo_pattern_t *pattern, const cairo_matrix_t *matrix)
{
    _emit_line_info ();
    if (_matrix_is_identity (matrix)) {
	_emit_pattern_op (pattern, "identity set-matrix\n");
    } else {
	_emit_pattern_op (pattern,
			  "%g %g %g %g %g %g matrix set-matrix\n",
			  matrix->xx, matrix->yx,
			  matrix->xy, matrix->yy,
			  matrix->x0, matrix->y0);
    }
    return DLCALL (cairo_pattern_set_matrix, pattern, matrix);
}

static const char *
_filter_to_string (cairo_filter_t filter)
{
    const char *names[] = {
	"FILTER_FAST",		/* CAIRO_FILTER_FAST */
	"FILTER_GOOD",		/* CAIRO_FILTER_GOOD */
	"FILTER_BEST",		/* CAIRO_FILTER_BEST */
	"FILTER_NEAREST",	/* CAIRO_FILTER_NEAREST */
	"FILTER_BILINEAR",	/* CAIRO_FILTER_BILINEAR */
	"FILTER_GAUSSIAN",	/* CAIRO_FILTER_GAUSSIAN */
    };
    return names[filter];
}

void
cairo_pattern_set_filter (cairo_pattern_t *pattern, cairo_filter_t filter)
{
    _emit_line_info ();
    _emit_pattern_op (pattern, "//%s set-filter\n", _filter_to_string (filter));
    return DLCALL (cairo_pattern_set_filter, pattern, filter);
}

static const char *
_extend_to_string (cairo_extend_t extend)
{
    const char *names[] = {
	"EXTEND_NONE",		/* CAIRO_EXTEND_NONE */
	"EXTEND_REPEAT",	/* CAIRO_EXTEND_REPEAT */
	"EXTEND_REFLECT",	/* CAIRO_EXTEND_REFLECT */
	"EXTEND_PAD"		/* CAIRO_EXTEND_PAD */
    };
    return names[extend];
}

void
cairo_pattern_set_extend (cairo_pattern_t *pattern, cairo_extend_t extend)
{
    _emit_line_info ();
    _emit_pattern_op (pattern, "//%s set-extend\n", _extend_to_string (extend));
    return DLCALL (cairo_pattern_set_extend, pattern, extend);
}

#if CAIRO_HAS_FT_FONT
cairo_font_face_t *
cairo_ft_font_face_create_for_pattern (FcPattern *pattern)
{
    cairo_font_face_t *ret;
    long font_face_id;

    ret = DLCALL (cairo_ft_font_face_create_for_pattern, pattern);
    font_face_id = _create_font_face_id (ret);

    _emit_line_info ();
    if (pattern != NULL && _write_lock ()) {
	Object *obj;
	FcChar8 *parsed;

	obj = _get_object (FONT_FACE, ret);
	if (obj->operand != -1)
	    _object_remove (obj);

	parsed = DLCALL (FcNameUnparse, pattern);
	_trace_printf ("dict\n"
		       "  /type 42 set\n"
		       "  /pattern ");
	_emit_string_literal ((char *) parsed, -1);
	_trace_printf (" set\n"
		       "  font\n");
	_push_operand (FONT_FACE, ret);
	_write_unlock ();

	free (parsed);
    }

    return ret;
}

typedef struct _ft_face_data {
    unsigned long index;
    unsigned long size;
    void *data;
} FtFaceData;

static void
_ft_face_data_destroy (void *arg)
{
    FtFaceData *data = arg;
    free (data->data);
    free (data);
}

cairo_font_face_t *
cairo_ft_font_face_create_for_ft_face (FT_Face face, int load_flags)
{
    cairo_font_face_t *ret;
    Object *obj;
    FtFaceData *data;
    long font_face_id;

    ret = DLCALL (cairo_ft_font_face_create_for_ft_face, face, load_flags);
    font_face_id = _create_font_face_id (ret);

    if (face == NULL)
	return ret;

    obj = _get_object (NONE, face);
    data = obj->data;
    if (data == NULL)
	return ret;

    _emit_line_info ();
    if (_write_lock ()) {
	obj = _get_object (FONT_FACE, ret);
	if (obj->operand != -1)
	    _object_remove (obj);

	_trace_printf ("dict\n"
		       "  /type 42 set\n"
		       "  /source ");
	_emit_data (data->data, data->size);
	_trace_printf (" /deflate filter set\n"
		       "  /size %lu set\n"
		       "  /index %lu set\n"
		       "  /flags %d set\n"
		       "  font\n",
		       data->size, data->index, load_flags);
	_push_operand (FONT_FACE, ret);
	_write_unlock ();
    }

    return ret;
}

static bool
_ft_read_file (FtFaceData *data, const char *path)
{
    char buf[4096];
    FILE *file;

    file = fopen (path, "rb");
    if (file != NULL) {
	size_t ret;
	unsigned long int allocated = 8192;
	data->data = malloc (allocated);
	do {
	    ret = fread (buf, 1, sizeof (buf), file);
	    if (ret == 0)
		break;
	    memcpy ((char *) data->data + data->size, buf, ret);
	    data->size += ret;
	    if (ret != sizeof (buf))
		break;

	    if (data->size == allocated) {
		allocated *= 2;
		data->data = realloc (data->data, allocated);
	    }
	} while (true);
	fclose (file);
    }

    return file != NULL;
}

FT_Error
FT_New_Face (FT_Library library, const char *pathname, FT_Long index, FT_Face *face)
{
    FT_Error ret;

    ret = DLCALL (FT_New_Face, library, pathname, index, face);
    if (ret == 0) {
	Object *obj = _type_object_create (NONE, *face);
	FtFaceData *data = malloc (sizeof (FtFaceData));
	data->index = index;
	data->size = 0;
	data->data = NULL;
	_ft_read_file (data, pathname);
	obj->data = data;
	obj->destroy = _ft_face_data_destroy;
    }

    return ret;
}

FT_Error
FT_New_Memory_Face (FT_Library library, const FT_Byte *mem, FT_Long size, FT_Long index, FT_Face *face)
{
    FT_Error ret;

    ret = DLCALL (FT_New_Memory_Face, library, mem, size, index, face);
    if (ret == 0) {
	Object *obj = _type_object_create (NONE, *face);
	FtFaceData *data = malloc (sizeof (FtFaceData));
	data->index = index;
	data->size = size;
	data->data = malloc (size);
	memcpy (data->data, mem, size);
	obj->data = data;
	obj->destroy = _ft_face_data_destroy;
    }

    return ret;
}

/* XXX
 * FT_New_Memory_Face() and FT_New_Face() appear to wrap FT_Open_Face() so we
 * get a redundant call to FT_Open_Face() from those paths (no PLT hiding
 * within FT, naughty library!) but we do not intercept a direct call to
 * FT_Open_Face(). So far this has not caused any issues, but it will one
 * day...
 */
#if 0
FT_Error
FT_Open_Face (FT_Library library, const FT_Open_Args *args, FT_Long index, FT_Face *face)
{
    FT_Error ret;

    ret = DLCALL (FT_Open_Face, library, args, index, face);
    if (args->flags & FT_OPEN_MEMORY)
	fprintf (stderr, "FT_Open_Face (mem=%p, %ld, %ld) = %p\n",
		args->memory_base, args->memory_size,
		index, *face);
    else if (args->flags & FT_OPEN_STREAM)
	fprintf (stderr, "FT_Open_Face (stream, %ld) = %p\n",
		index, *face);
    else if (args->flags & FT_OPEN_PATHNAME)
	fprintf (stderr, "FT_Open_Face (path=%s, %ld) = %p\n",
		args->pathname, index, *face);

    return ret;
}
#endif

FT_Error
FT_Done_Face (FT_Face face)
{
    _object_destroy (_get_object (NONE, face));

    return DLCALL (FT_Done_Face, face);
}
#endif

#if CAIRO_HAS_PS_SURFACE
#include<cairo-ps.h>

cairo_surface_t *
cairo_ps_surface_create (const char *filename, double width_in_points, double height_in_points)
{
    cairo_surface_t *ret;
    long surface_id;

    ret = DLCALL (cairo_ps_surface_create, filename, width_in_points, height_in_points);
    surface_id = _create_surface_id (ret);

    _emit_line_info ();
    if (_write_lock ()) {
	_trace_printf ("dict\n"
		       "  /type /PS set\n"
		       "  /filename ");
	_emit_string_literal (filename, -1);
	_trace_printf (" set\n"
		       "  /width %g set\n"
		       "  /height %g set\n"
		       "  surface %% s%ld\n",
		       width_in_points,
		       height_in_points,
		       surface_id);
	_get_object (SURFACE, ret)->width = width_in_points;
	_get_object (SURFACE, ret)->height = height_in_points;
	_push_operand (SURFACE, ret);
	_write_unlock ();
    }

    return ret;
}

cairo_surface_t *
cairo_ps_surface_create_for_stream (cairo_write_func_t write_func, void *closure, double width_in_points, double height_in_points)
{
    cairo_surface_t *ret;
    long surface_id;

    ret = DLCALL (cairo_ps_surface_create_for_stream, write_func, closure, width_in_points, height_in_points);
    surface_id = _create_surface_id (ret);

    _emit_line_info ();
    if (_write_lock ()) {
	_trace_printf ("dict\n"
		       "  /type /PS set\n"
		       "  /width %g set\n"
		       "  /height %g set\n"
		       "  surface %% s%ld\n",
		       width_in_points,
		       height_in_points,
		       surface_id);
	_get_object (SURFACE, ret)->width = width_in_points;
	_get_object (SURFACE, ret)->height = height_in_points;
	_push_operand (SURFACE, ret);
	_write_unlock ();
    }

    return ret;
}

void
cairo_ps_surface_set_size (cairo_surface_t *surface, double width_in_points, double height_in_points)
{
    _emit_line_info ();
    return DLCALL (cairo_ps_surface_set_size, surface, width_in_points, height_in_points);
}

#endif

#if CAIRO_HAS_PDF_SURFACE
#include <cairo-pdf.h>

cairo_surface_t *
cairo_pdf_surface_create (const char *filename, double width_in_points, double height_in_points)
{
    cairo_surface_t *ret;
    long surface_id;

    ret = DLCALL (cairo_pdf_surface_create, filename, width_in_points, height_in_points);
    surface_id = _create_surface_id (ret);

    _emit_line_info ();
    if (_write_lock ()) {
	_trace_printf ("dict\n"
		       "  /type /PDF set\n"
		       "  /filename ");
	_emit_string_literal (filename, -1);
	_trace_printf (" set\n"
		       "  /width %g set\n"
		       "  /height %g set\n"
		       "  surface %% s%ld\n",
		       width_in_points,
		       height_in_points,
		       surface_id);
	_get_object (SURFACE, ret)->width = width_in_points;
	_get_object (SURFACE, ret)->height = height_in_points;
	_push_operand (SURFACE, ret);
	_write_unlock ();
    }

    return ret;
}

cairo_surface_t *
cairo_pdf_surface_create_for_stream (cairo_write_func_t write_func, void *closure, double width_in_points, double height_in_points)
{
    cairo_surface_t *ret;
    long surface_id;

    ret = DLCALL (cairo_pdf_surface_create_for_stream, write_func, closure, width_in_points, height_in_points);
    surface_id = _create_surface_id (ret);

    _emit_line_info ();
    if (_write_lock ()) {
	_trace_printf ("dict\n"
		       "  /type /PDF set\n"
		       "  /width %g set\n"
		       "  /height %g set\n"
		       "  surface %% s%ld\n",
		       width_in_points,
		       height_in_points,
		       surface_id);
	_get_object (SURFACE, ret)->width = width_in_points;
	_get_object (SURFACE, ret)->height = height_in_points;
	_push_operand (SURFACE, ret);
	_write_unlock ();
    }
    return ret;
}

void
cairo_pdf_surface_set_size (cairo_surface_t *surface, double width_in_points, double height_in_points)
{
    _emit_line_info ();
    return DLCALL (cairo_pdf_surface_set_size, surface, width_in_points, height_in_points);
}
#endif

#if CAIRO_HAS_SVG_SURFACE
#include <cairo-svg.h>

cairo_surface_t *
cairo_svg_surface_create (const char *filename, double width, double height)
{
    cairo_surface_t *ret;
    long surface_id;

    ret = DLCALL (cairo_svg_surface_create, filename, width, height);
    surface_id = _create_surface_id (ret);

    _emit_line_info ();
    if (_write_lock ()) {
	_trace_printf ("dict\n"
		       "  /type /SVG set\n"
		       "  /filename ");
	_emit_string_literal (filename, -1);
	_trace_printf (" set\n"
		       "  /width %g set\n"
		       "  /height %g set\n"
		       "  surface %% s%ld\n",
		       width,
		       height,
		       surface_id);
	_get_object (SURFACE, ret)->width = width;
	_get_object (SURFACE, ret)->height = height;
	_push_operand (SURFACE, ret);
	_write_unlock ();
    }

    return ret;
}

cairo_surface_t *
cairo_svg_surface_create_for_stream (cairo_write_func_t write_func, void *closure, double width, double height)
{
    cairo_surface_t *ret;
    long surface_id;

    ret = DLCALL (cairo_svg_surface_create_for_stream, write_func, closure, width, height);
    surface_id = _create_surface_id (ret);

    _emit_line_info ();
    if (_write_lock ()) {
	_trace_printf ("dict\n"
		       "  /type /SVG set\n"
		       "  /width %g set\n"
		       "  /height %g set\n"
		       "  surface %% s%ld\n",
		       width,
		       height,
		       surface_id);
	_get_object (SURFACE, ret)->width = width;
	_get_object (SURFACE, ret)->height = height;
	_push_operand (SURFACE, ret);
	_write_unlock ();
    }

    return ret;
}

#endif

#if CAIRO_HAS_PNG_FUNCTIONS
cairo_surface_t *
cairo_image_surface_create_from_png (const char *filename)
{
    cairo_surface_t *ret;
    long surface_id;

    ret = DLCALL (cairo_image_surface_create_from_png, filename);

    surface_id = _create_surface_id (ret);

    _emit_line_info ();
    if (_write_lock ()) {
	char filename_string[4096];

	_encode_string_literal (filename_string, sizeof (filename_string),
				filename, -1);
	_emit_image (ret, "  /filename %s set\n", filename_string);
	_trace_printf (" dup /s%ld exch def\n",
		       surface_id);
	_get_object (SURFACE, ret)->width = cairo_image_surface_get_width (ret);
	_get_object (SURFACE, ret)->height = cairo_image_surface_get_height (ret);
	_get_object (SURFACE, ret)->defined = true;
	_push_operand (SURFACE, ret);
	_write_unlock ();
    }

    return ret;
}

cairo_surface_t *
cairo_image_surface_create_from_png_stream (cairo_read_func_t read_func, void *closure)
{
    cairo_surface_t *ret;
    long surface_id;

    ret = DLCALL (cairo_image_surface_create_from_png_stream, read_func, closure);
    surface_id = _create_surface_id (ret);

    _emit_line_info ();
    if (_write_lock ()) {
	_emit_image (ret, NULL);
	_trace_printf (" dup /s%ld exch def\n",
		       surface_id);
	_get_object (SURFACE, ret)->width = cairo_image_surface_get_width (ret);
	_get_object (SURFACE, ret)->height = cairo_image_surface_get_height (ret);
	_get_object (SURFACE, ret)->defined = true;
	_push_operand (SURFACE, ret);
	_write_unlock ();
    }

    return ret;
}
#endif

#if CAIRO_HAS_XLIB_SURFACE
#include <cairo-xlib.h>

cairo_surface_t *
cairo_xlib_surface_create (Display *dpy,
			   Drawable drawable,
			   Visual *visual,
			   int width, int height)
{
    cairo_surface_t *ret;
    long surface_id;

    ret = DLCALL (cairo_xlib_surface_create,
	          dpy, drawable, visual, width, height);
    surface_id = _create_surface_id (ret);

    _emit_line_info ();
    if (_write_lock ()) {
	_trace_printf ("dict\n"
		       "  /type /xlib set\n"
		       "  /drawable 16!%lx set\n"
		       "  /content //%s set\n"
		       "  /width %d set\n"
		       "  /height %d set\n"
		       "  surface dup /s%ld exch def\n",
		       drawable,
		       _content_to_string (cairo_surface_get_content (ret)),
		       width,
		       height,
		       surface_id);
	_get_object (SURFACE, ret)->defined = true;
	_get_object (SURFACE, ret)->width  = width;
	_get_object (SURFACE, ret)->height = height;
	_get_object (SURFACE, ret)->foreign = true;
	_push_operand (SURFACE, ret);
	_write_unlock ();
    }

    return ret;
}

cairo_surface_t *
cairo_xlib_surface_create_for_bitmap (Display *dpy,
				      Pixmap bitmap,
				      Screen *screen,
				      int width, int height)
{
    cairo_surface_t *ret;
    long surface_id;

    ret = DLCALL (cairo_xlib_surface_create_for_bitmap,
	          dpy, bitmap, screen, width, height);
    surface_id = _create_surface_id (ret);

    _emit_line_info ();
    if (_write_lock ()) {
	_trace_printf ("dict\n"
		       "  /type /xlib set\n"
		       "  /drawable 16!%lx set\n"
		       "  /content //%s set\n"
		       "  /width %d set\n"
		       "  /height %d set\n"
		       "  /depth 1 set\n"
		       "  surface dup /s%ld exch def\n",
		       bitmap,
		       _content_to_string (cairo_surface_get_content (ret)),
		       width,
		       height,
		       surface_id);
	_get_object (SURFACE, ret)->defined = true;
	_get_object (SURFACE, ret)->width  = width;
	_get_object (SURFACE, ret)->height = height;
	_get_object (SURFACE, ret)->foreign = true;
	_push_operand (SURFACE, ret);
	_write_unlock ();
    }

    return ret;
}

#if CAIRO_HAS_XLIB_XRENDER_SURFACE
#include <cairo-xlib-xrender.h>
cairo_surface_t *
cairo_xlib_surface_create_with_xrender_format (Display *dpy,
					       Drawable drawable,
					       Screen *screen,
					       XRenderPictFormat *format,
					       int width, int height)
{
    cairo_surface_t *ret;
    long surface_id;

    ret = DLCALL (cairo_xlib_surface_create_with_xrender_format,
	          dpy, drawable, screen, format, width, height);
    surface_id = _create_surface_id (ret);

    _emit_line_info ();
    if (_write_lock ()) {
	_trace_printf ("dict\n"
		       "  /type /xrender set\n"
		       "  /drawable 16!%lx set\n"
		       "  /content //%s set\n"
		       "  /width %d set\n"
		       "  /height %d set\n"
		       "  /depth %d set\n"
		       "  surface dup /s%ld exch def\n",
		       drawable,
		       _content_to_string (cairo_surface_get_content (ret)),
		       width,
		       height,
		       format->depth,
		       surface_id);
	_get_object (SURFACE, ret)->defined = true;
	_get_object (SURFACE, ret)->width  = width;
	_get_object (SURFACE, ret)->height = height;
	_get_object (SURFACE, ret)->foreign = true;
	_push_operand (SURFACE, ret);
	_write_unlock ();
    }

    return ret;
}
#endif
#endif

#if CAIRO_HAS_SCRIPT_SURFACE
#include <cairo-script.h>
cairo_surface_t *
cairo_script_surface_create (const char *filename,
			     double width,
			     double height)
{
    cairo_surface_t *ret;
    long surface_id;

    ret = DLCALL (cairo_script_surface_create, filename, width, height);
    surface_id = _create_surface_id (ret);

    _emit_line_info ();
    if (_write_lock ()) {
	_trace_printf ("dict\n"
		       "  /type /script set\n"
		       "  /filename ");
	_emit_string_literal (filename, -1);
	_trace_printf (" set\n"
		       "  /width %g set\n"
		       "  /height %g set\n"
		       "  surface dup /s%ld exch def\n",
		       width, height,
		       surface_id);
	_get_object (SURFACE, ret)->width = width;
	_get_object (SURFACE, ret)->height = height;
	_get_object (SURFACE, ret)->defined = true;
	_push_operand (SURFACE, ret);
	_write_unlock ();
    }

    return ret;
}

cairo_surface_t *
cairo_script_surface_create_for_stream (cairo_write_func_t write_func,
					void *data,
					double width,
					double height)
{
    cairo_surface_t *ret;
    long surface_id;

    ret = DLCALL (cairo_script_surface_create_for_stream,
		  write_func, data, width, height);
    surface_id = _create_surface_id (ret);

    _emit_line_info ();
    if (_write_lock ()) {
	_trace_printf ("dict\n"
		       "  /type /script set\n"
		       "  /width %g set\n"
		       "  /height %g set\n"
		       "  surface dup /s%ld exch def\n",
		       width, height,
		       surface_id);
	_get_object (SURFACE, ret)->width = width;
	_get_object (SURFACE, ret)->height = height;
	_get_object (SURFACE, ret)->defined = true;
	_push_operand (SURFACE, ret);
	_write_unlock ();
    }

    return ret;
}
#endif

#if CAIRO_HAS_TEST_SURFACES
#include <test-fallback-surface.h>
cairo_surface_t *
_cairo_test_fallback_surface_create (cairo_content_t	content,
				     int		width,
				     int		height)
{
    cairo_surface_t *ret;
    long surface_id;

    ret = DLCALL (_cairo_test_fallback_surface_create, content, width, height);
    surface_id = _create_surface_id (ret);

    _emit_line_info ();
    if (_write_lock ()) {
	_trace_printf ("dict\n"
		       "  /type /test-fallback set\n"
		       "  /content //%s set\n"
		       "  /width %d set\n"
		       "  /height %d set\n"
		       "  surface dup /s%ld exch def\n",
		       _content_to_string (content),
		       width, height,
		       surface_id);
	_get_object (SURFACE, ret)->width = width;
	_get_object (SURFACE, ret)->height = height;
	_get_object (SURFACE, ret)->defined = true;
	_push_operand (SURFACE, ret);
	_write_unlock ();
    }

    return ret;
}

#include <test-paginated-surface.h>
cairo_surface_t *
_cairo_test_paginated_surface_create_for_data (unsigned char	*data,
					       cairo_content_t	 content,
					       int		 width,
					       int		 height,
					       int		 stride)
{
    cairo_surface_t *ret;
    long surface_id;

    ret = DLCALL (_cairo_test_paginated_surface_create_for_data,
		  data, content, width, height, stride);
    surface_id = _create_surface_id (ret);

    _emit_line_info ();
    if (_write_lock ()) {
	/* XXX store initial data? */
	_trace_printf ("dict\n"
		       "  /type /test-paginated set\n"
		       "  /content //%s set\n"
		       "  /width %d set\n"
		       "  /height %d set\n"
		       "  /stride %d set\n"
		       "  surface dup /s%ld exch def\n",
		       _content_to_string (content),
		       width, height, stride,
		       surface_id);
	_get_object (SURFACE, ret)->width = width;
	_get_object (SURFACE, ret)->height = height;
	_get_object (SURFACE, ret)->defined = true;
	_push_operand (SURFACE, ret);
	_write_unlock ();
    }

    return ret;
}

#include <test-meta-surface.h>
cairo_surface_t *
_cairo_test_meta_surface_create (cairo_content_t	content,
				 int			width,
				 int			height)
{
    cairo_surface_t *ret;
    long surface_id;

    ret = DLCALL (_cairo_test_meta_surface_create, content, width, height);
    surface_id = _create_surface_id (ret);

    _emit_line_info ();
    if (_write_lock ()) {
	_trace_printf ("dict\n"
		       "  /type /test-meta set\n"
		       "  /content //%s set\n"
		       "  /width %d set\n"
		       "  /height %d set\n"
		       "  surface dup /s%ld exch def\n",
		       _content_to_string (content),
		       width, height,
		       surface_id);
	_get_object (SURFACE, ret)->width = width;
	_get_object (SURFACE, ret)->height = height;
	_get_object (SURFACE, ret)->defined = true;
	_push_operand (SURFACE, ret);
	_write_unlock ();
    }

    return ret;
}
#endif
