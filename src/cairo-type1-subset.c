/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2006 Red Hat, Inc
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
 * The Initial Developer of the Original Code is Red Hat, Inc.
 *
 * Contributor(s):
 *	Kristian Høgsberg <krh@redhat.com>
 */

/*
 * Useful links:
 * http://partners.adobe.com/public/developer/en/font/T1_SPEC.PDF
 */


#define _BSD_SOURCE /* for snprintf(), strdup() */
#include "cairoint.h"
#include "cairo-error-private.h"

#if CAIRO_HAS_FONT_SUBSET

#include "cairo-type1-private.h"
#include "cairo-scaled-font-subsets-private.h"
#include "cairo-output-stream-private.h"

/* XXX: Eventually, we need to handle other font backends */
#if CAIRO_HAS_FT_FONT

#include "cairo-ft-private.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H
#include FT_TYPE1_TABLES_H

#include <ctype.h>

typedef struct _cairo_type1_font_subset {
    cairo_scaled_font_subset_t *scaled_font_subset;

    struct {
	cairo_unscaled_font_t *unscaled_font;
	unsigned int font_id;
	char *base_font;
	unsigned int num_glyphs;
	double x_min, y_min, x_max, y_max;
	double ascent, descent;

	const char    *data;
	unsigned long  header_size;
	unsigned long  data_size;
	unsigned long  trailer_size;
    } base;

    FT_Face face;
    int num_glyphs;

    struct {
	int subset_index;
	double width;
	char *name;
    } *glyphs;

    cairo_output_stream_t *output;
    cairo_array_t contents;

    const char *rd, *nd;

    char *type1_data;
    unsigned int type1_length;
    char *type1_end;

    char *header_segment;
    int header_segment_size;
    char *eexec_segment;
    int eexec_segment_size;
    cairo_bool_t eexec_segment_is_ascii;

    char *cleartext;
    char *cleartext_end;

    int header_size;

    unsigned short eexec_key;
    cairo_bool_t hex_encode;
    int hex_column;
} cairo_type1_font_subset_t;


static cairo_status_t
_cairo_type1_font_subset_init (cairo_type1_font_subset_t  *font,
			       cairo_unscaled_font_t      *unscaled_font,
			       cairo_scaled_font_subset_t *scaled_font_subset,
			       cairo_bool_t                hex_encode)
{
    cairo_ft_unscaled_font_t *ft_unscaled_font;
    cairo_status_t status;
    FT_Face face;
    PS_FontInfoRec font_info;
    int i, j;

    ft_unscaled_font = (cairo_ft_unscaled_font_t *) unscaled_font;

    face = _cairo_ft_unscaled_font_lock_face (ft_unscaled_font);
    if (unlikely (face == NULL))
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    if (FT_Get_PS_Font_Info(face, &font_info) != 0) {
	status = CAIRO_INT_STATUS_UNSUPPORTED;
        goto fail1;
    }

    /* OpenType/CFF fonts also have a PS_FontInfoRec */
#if HAVE_FT_LOAD_SFNT_TABLE
    if (FT_IS_SFNT (face)) {
	status = CAIRO_INT_STATUS_UNSUPPORTED;
        goto fail1;
    }
#endif

    memset (font, 0, sizeof (*font));
    font->scaled_font_subset = scaled_font_subset;
    font->base.unscaled_font = _cairo_unscaled_font_reference (unscaled_font);
    font->base.num_glyphs = face->num_glyphs;
    font->base.x_min = face->bbox.xMin / (double)face->units_per_EM;
    font->base.y_min = face->bbox.yMin / (double)face->units_per_EM;
    font->base.x_max = face->bbox.xMax / (double)face->units_per_EM;
    font->base.y_max = face->bbox.yMax / (double)face->units_per_EM;
    font->base.ascent = face->ascender / (double)face->units_per_EM;
    font->base.descent = face->descender / (double)face->units_per_EM;

    if (face->family_name) {
	font->base.base_font = strdup (face->family_name);
	if (unlikely (font->base.base_font == NULL)) {
	    status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	    goto fail2;
	}
	for (i = 0, j = 0; font->base.base_font[j]; j++) {
	    if (font->base.base_font[j] == ' ')
		continue;
	    font->base.base_font[i++] = font->base.base_font[j];
	}
	font->base.base_font[i] = '\0';
    }

    font->glyphs = calloc (face->num_glyphs, sizeof font->glyphs[0]);
    if (unlikely (font->glyphs == NULL)) {
        status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	goto fail3;
    }

    font->hex_encode = hex_encode;
    font->num_glyphs = 0;
    for (i = 0; i < face->num_glyphs; i++)
	font->glyphs[i].subset_index = -1;

    _cairo_array_init (&font->contents, sizeof (char));

    _cairo_ft_unscaled_font_unlock_face (ft_unscaled_font);

    return CAIRO_STATUS_SUCCESS;

 fail3:
    if (font->base.base_font)
	free (font->base.base_font);
 fail2:
    _cairo_unscaled_font_destroy (unscaled_font);
 fail1:
    _cairo_ft_unscaled_font_unlock_face (ft_unscaled_font);

    return status;
}

static void
cairo_type1_font_subset_use_glyph (cairo_type1_font_subset_t *font, int glyph)
{
    if (font->glyphs[glyph].subset_index >= 0)
	return;

    font->glyphs[glyph].subset_index = font->num_glyphs++;
}

static cairo_bool_t
is_ps_delimiter(int c)
{
    static const char delimiters[] = "()[]{}<>/% \t\r\n";

    return strchr (delimiters, c) != NULL;
}

static const char *
find_token (const char *buffer, const char *end, const char *token)
{
    int i, length;
    /* FIXME: find substring really must be find_token */

    if (buffer == NULL)
	return NULL;

    length = strlen (token);
    for (i = 0; buffer + i < end - length + 1; i++)
	if (memcmp (buffer + i, token, length) == 0)
	    if ((i == 0 || token[0] == '/' || is_ps_delimiter(buffer[i - 1])) &&
		(buffer + i == end - length || is_ps_delimiter(buffer[i + length])))
		return buffer + i;

    return NULL;
}

static cairo_status_t
cairo_type1_font_subset_find_segments (cairo_type1_font_subset_t *font)
{
    unsigned char *p;
    const char *eexec_token;
    int size, i;

    p = (unsigned char *) font->type1_data;
    font->type1_end = font->type1_data + font->type1_length;
    if (p[0] == 0x80 && p[1] == 0x01) {
	font->header_segment_size =
	    p[2] | (p[3] << 8) | (p[4] << 16) | (p[5] << 24);
	font->header_segment = (char *) p + 6;

	p += 6 + font->header_segment_size;
	font->eexec_segment_size =
	    p[2] | (p[3] << 8) | (p[4] << 16) | (p[5] << 24);
	font->eexec_segment = (char *) p + 6;
	font->eexec_segment_is_ascii = (p[1] == 1);

        p += 6 + font->eexec_segment_size;
        while (p < (unsigned char *) (font->type1_end) && p[1] != 0x03) {
            size = p[2] | (p[3] << 8) | (p[4] << 16) | (p[5] << 24);
            p += 6 + size;
        }
        font->type1_end = (char *) p;
    } else {
	eexec_token = find_token ((char *) p, font->type1_end, "eexec");
	if (eexec_token == NULL)
	    return CAIRO_INT_STATUS_UNSUPPORTED;

	font->header_segment_size = eexec_token - (char *) p + strlen ("eexec\n");
	font->header_segment = (char *) p;
	font->eexec_segment_size = font->type1_length - font->header_segment_size;
	font->eexec_segment = (char *) p + font->header_segment_size;
	font->eexec_segment_is_ascii = TRUE;
	for (i = 0; i < 4; i++) {
	    if (!isxdigit(font->eexec_segment[i]))
		font->eexec_segment_is_ascii = FALSE;
	}
    }

    return CAIRO_STATUS_SUCCESS;
}

/* Search for the definition of key and erase it by overwriting with spaces.
 * This function is looks for definitions of the form:
 *
 * /key1 1234 def
 * /key2 [12 34 56] def
 *
 * ie a key defined as an integer or array of integers.
 *
 */
static void
cairo_type1_font_erase_dict_key (cairo_type1_font_subset_t *font,
				 const char *key)
{
    const char *start, *p, *segment_end;

    segment_end = font->header_segment + font->header_segment_size;

    start = font->header_segment;
    do {
	start = find_token (start, segment_end, key);
	if (start) {
	    p = start + strlen(key);
	    /* skip integers or array of integers */
	    while (p < segment_end &&
		   (_cairo_isspace(*p) ||
		    _cairo_isdigit(*p) ||
		    *p == '[' ||
		    *p == ']'))
	    {
		p++;
	    }

	    if (p + 3 < segment_end && memcmp(p, "def", 3) == 0) {
		/* erase definition of the key */
		memset((char *) start, ' ', p + 3 - start);
	    }
	    start += strlen(key);
	}
    } while (start);
}

static cairo_status_t
cairo_type1_font_subset_write_header (cairo_type1_font_subset_t *font,
					 const char *name)
{
    const char *start, *end, *segment_end;
    unsigned int i;

    /* FIXME:
     * This function assumes that /FontName always appears
     * before /Encoding. This appears to always be the case with Type1
     * fonts.
     *
     * The more recently added code for removing the UniqueID and XUID
     * keys can not make any assumptions about the position of the
     * keys in the dictionary so it is implemented by overwriting the
     * key definition with spaces before we start copying the font to
     * the output.
     *
     * This code should be rewritten to not make any assumptions about
     * the order of dictionary keys. This will allow UniqueID to be
     * stripped out instead of leaving a bunch of spaces in the
     * output.
     */
    cairo_type1_font_erase_dict_key (font, "/UniqueID");
    cairo_type1_font_erase_dict_key (font, "/XUID");

    segment_end = font->header_segment + font->header_segment_size;

    /* Type 1 fonts created by Fontforge have some PostScript code at
     * the start of the font that skips the font if the printer has a
     * cached copy of the font with the same unique id. This breaks
     * our subsetted font so we disable it by searching for the
     * PostScript operator "known" when used to check for the
     * "/UniqueID" dictionary key. We append " pop false " after it to
     * pop the result of this check off the stack and replace it with
     * "false" to make the PostScript code think "/UniqueID" does not
     * exist.
     */
    end = font->header_segment;
    start = find_token (font->header_segment, segment_end, "/UniqueID");
    if (start) {
	start += 9;
	while (start < segment_end && _cairo_isspace (*start))
	    start++;
	if (start + 5 < segment_end && memcmp(start, "known", 5) == 0) {
	    _cairo_output_stream_write (font->output, font->header_segment,
					start + 5 - font->header_segment);
	    _cairo_output_stream_printf (font->output, " pop false ");
	    end = start + 5;
	}
    }

    start = find_token (end, segment_end, "/FontName");
    if (start == NULL)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    _cairo_output_stream_write (font->output, end,
				start - end);

    _cairo_output_stream_printf (font->output, "/FontName /%s def", name);

    end = find_token (start, segment_end, "def");
    if (end == NULL)
	return CAIRO_INT_STATUS_UNSUPPORTED;
    end += 3;

    start = find_token (end, segment_end, "/Encoding");
    if (start == NULL)
	return CAIRO_INT_STATUS_UNSUPPORTED;
    _cairo_output_stream_write (font->output, end, start - end);

    _cairo_output_stream_printf (font->output,
				 "/Encoding 256 array\n"
				 "0 1 255 {1 index exch /.notdef put} for\n");
    if (font->scaled_font_subset->is_latin) {
	for (i = 1; i < 256; i++) {
	    int subset_glyph = font->scaled_font_subset->latin_to_subset_glyph_index[i];
	    int parent_glyph = font->scaled_font_subset->glyphs[subset_glyph];

	    if (subset_glyph > 0) {
		_cairo_output_stream_printf (font->output,
					     "dup %d /%s put\n",
					     i,
					     font->glyphs[parent_glyph].name);
	    }
	}
    } else {
	for (i = 1; i < font->base.num_glyphs; i++) {
	    if (font->glyphs[i].subset_index < 0)
		continue;
	    _cairo_output_stream_printf (font->output,
					 "dup %d /%s put\n",
					 font->glyphs[i].subset_index,
					 font->glyphs[i].name);
	}
    }
    _cairo_output_stream_printf (font->output, "readonly def");

    end = find_token (start, segment_end, "def");
    if (end == NULL)
	return CAIRO_INT_STATUS_UNSUPPORTED;
    end += 3;

    _cairo_output_stream_write (font->output, end, segment_end - end);

    return font->output->status;
}

static int
hex_to_int (int ch)
{
    if (ch <= '9')
	return ch - '0';
    else if (ch <= 'F')
	return ch - 'A' + 10;
    else
	return ch - 'a' + 10;
}

static cairo_status_t
cairo_type1_font_subset_write_encrypted (cairo_type1_font_subset_t *font,
					 const char *data, unsigned int length)
{
    const unsigned char *in, *end;
    int c, p;
    static const char hex_digits[16] = "0123456789abcdef";
    char digits[3];

    in = (const unsigned char *) data;
    end = (const unsigned char *) data + length;
    while (in < end) {
	p = *in++;
	c = p ^ (font->eexec_key >> 8);
	font->eexec_key = (c + font->eexec_key) * CAIRO_TYPE1_ENCRYPT_C1 + CAIRO_TYPE1_ENCRYPT_C2;

	if (font->hex_encode) {
	    digits[0] = hex_digits[c >> 4];
	    digits[1] = hex_digits[c & 0x0f];
	    digits[2] = '\n';
	    font->hex_column += 2;

	    if (font->hex_column == 78) {
		_cairo_output_stream_write (font->output, digits, 3);
		font->hex_column = 0;
	    } else {
		_cairo_output_stream_write (font->output, digits, 2);
	    }
	} else {
	    digits[0] = c;
	    _cairo_output_stream_write (font->output, digits, 1);
	}
    }

    return font->output->status;
}

static cairo_status_t
cairo_type1_font_subset_decrypt_eexec_segment (cairo_type1_font_subset_t *font)
{
    unsigned short r = CAIRO_TYPE1_PRIVATE_DICT_KEY;
    unsigned char *in, *end;
    char *out;
    int c, p;
    int i;

    in = (unsigned char *) font->eexec_segment;
    end = (unsigned char *) in + font->eexec_segment_size;

    font->cleartext = malloc (font->eexec_segment_size);
    if (unlikely (font->cleartext == NULL))
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    out = font->cleartext;
    while (in < end) {
	if (font->eexec_segment_is_ascii) {
	    c = *in++;
	    if (_cairo_isspace (c))
		continue;
	    c = (hex_to_int (c) << 4) | hex_to_int (*in++);
	} else {
	    c = *in++;
	}
	p = c ^ (r >> 8);
	r = (c + r) * CAIRO_TYPE1_ENCRYPT_C1 + CAIRO_TYPE1_ENCRYPT_C2;

	*out++ = p;
    }
    font->cleartext_end = out;

    /* Overwrite random bytes with spaces.
     *
     * The first 4 bytes of the cleartext are the random bytes
     * required by the encryption algorithm. When encrypting the
     * cleartext, the first ciphertext byte must not be a white space
     * character and the first 4 bytes must not be an ASCII Hex
     * character. Some fonts do not check that their randomly chosen
     * bytes results in ciphertext that complies with this
     * restriction. This may cause problems for some PDF consumers. By
     * replacing the random bytes with spaces, the first four bytes of
     * ciphertext will always be 0xf9, 0x83, 0xef, 0x00 which complies
     * with this restriction. Using spaces also means we don't have to
     * skip over the random bytes when parsing the cleartext.
     */
    for (i = 0; i < 4 && i < font->eexec_segment_size; i++)
	font->cleartext[i] = ' ';

    return CAIRO_STATUS_SUCCESS;
}

static const char *
skip_token (const char *p, const char *end)
{
    while (p < end && _cairo_isspace(*p))
	p++;

    while (p < end && !_cairo_isspace(*p))
	p++;

    if (p == end)
	return NULL;

    return p;
}

static int
cairo_type1_font_subset_lookup_glyph (cairo_type1_font_subset_t *font,
				      const char *glyph_name, int length)
{
    unsigned int i;

    for (i = 0; i < font->base.num_glyphs; i++) {
	if (font->glyphs[i].name &&
	    strncmp (font->glyphs[i].name, glyph_name, length) == 0 &&
	    font->glyphs[i].name[length] == '\0')
	    return i;
    }

    return -1;
}

static cairo_status_t
cairo_type1_font_subset_get_glyph_names_and_widths (cairo_type1_font_subset_t *font)
{
    unsigned int i;
    char buffer[256];
    FT_Error error;

    /* Get glyph names and width using the freetype API */
    for (i = 0; i < font->base.num_glyphs; i++) {
	if (font->glyphs[i].name != NULL)
	    continue;

	error = FT_Load_Glyph (font->face, i,
			       FT_LOAD_NO_SCALE | FT_LOAD_NO_HINTING |
			       FT_LOAD_NO_BITMAP | FT_LOAD_IGNORE_TRANSFORM);
	if (error != FT_Err_Ok) {
	    /* propagate fatal errors from FreeType */
	    if (error == FT_Err_Out_Of_Memory)
		return _cairo_error (CAIRO_STATUS_NO_MEMORY);

	    return CAIRO_INT_STATUS_UNSUPPORTED;
	}

	font->glyphs[i].width = font->face->glyph->metrics.horiAdvance / (double)font->face->units_per_EM;

	error = FT_Get_Glyph_Name(font->face, i, buffer, sizeof buffer);
	if (error != FT_Err_Ok) {
	    /* propagate fatal errors from FreeType */
	    if (error == FT_Err_Out_Of_Memory)
		return _cairo_error (CAIRO_STATUS_NO_MEMORY);

	    return CAIRO_INT_STATUS_UNSUPPORTED;
	}

	font->glyphs[i].name = strdup (buffer);
	if (unlikely (font->glyphs[i].name == NULL))
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);
    }

    return CAIRO_STATUS_SUCCESS;
}

static void
cairo_type1_font_subset_decrypt_charstring (const unsigned char *in, int size, unsigned char *out)
{
    unsigned short r = CAIRO_TYPE1_CHARSTRING_KEY;
    int c, p, i;

    for (i = 0; i < size; i++) {
        c = *in++;
	p = c ^ (r >> 8);
	r = (c + r) * CAIRO_TYPE1_ENCRYPT_C1 + CAIRO_TYPE1_ENCRYPT_C2;
	*out++ = p;
    }
}

static const unsigned char *
cairo_type1_font_subset_decode_integer (const unsigned char *p, int *integer)
{
    if (*p <= 246) {
        *integer = *p++ - 139;
    } else if (*p <= 250) {
        *integer = (p[0] - 247) * 256 + p[1] + 108;
        p += 2;
    } else if (*p <= 254) {
        *integer = -(p[0] - 251) * 256 - p[1] - 108;
        p += 2;
    } else {
        *integer = (p[1] << 24) | (p[2] << 16) | (p[3] << 8) | p[4];
        p += 5;
    }

    return p;
}

static cairo_status_t
use_standard_encoding_glyph (cairo_type1_font_subset_t *font, int index)
{
    const char *glyph_name;

    if (index < 0 || index > 255)
	return CAIRO_STATUS_SUCCESS;

    glyph_name = _cairo_ps_standard_encoding_to_glyphname (index);
    if (glyph_name == NULL)
	return CAIRO_STATUS_SUCCESS;

    index = cairo_type1_font_subset_lookup_glyph (font,
						  glyph_name,
						  strlen(glyph_name));
    if (index < 0)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    cairo_type1_font_subset_use_glyph (font, index);

    return CAIRO_STATUS_SUCCESS;
}

#define TYPE1_CHARSTRING_COMMAND_ESCAPE		(12)
#define TYPE1_CHARSTRING_COMMAND_SEAC		(32 + 6)

static cairo_status_t
cairo_type1_font_subset_look_for_seac(cairo_type1_font_subset_t *font,
				      const char *name, int name_length,
				      const char *encrypted_charstring, int encrypted_charstring_length)
{
    cairo_status_t status;
    unsigned char *charstring;
    const unsigned char *end;
    const unsigned char *p;
    int stack[5], sp, value;
    int command;

    charstring = malloc (encrypted_charstring_length);
    if (unlikely (charstring == NULL))
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    cairo_type1_font_subset_decrypt_charstring ((const unsigned char *)
						encrypted_charstring,
						encrypted_charstring_length,
						charstring);
    end = charstring + encrypted_charstring_length;

    p = charstring + 4;
    sp = 0;

    while (p < end) {
        if (*p < 32) {
	    command = *p++;

	    if (command == TYPE1_CHARSTRING_COMMAND_ESCAPE)
		command = 32 + *p++;

	    switch (command) {
	    case TYPE1_CHARSTRING_COMMAND_SEAC:
		/* The seac command takes five integer arguments.  The
		 * last two are glyph indices into the PS standard
		 * encoding give the names of the glyphs that this
		 * glyph is composed from.  All we need to do is to
		 * make sure those glyphs are present in the subset
		 * under their standard names. */
		status = use_standard_encoding_glyph (font, stack[3]);
		if (unlikely (status))
		    return status;

		status = use_standard_encoding_glyph (font, stack[4]);
		if (unlikely (status))
		    return status;

		sp = 0;
		break;

	    default:
		sp = 0;
		break;
	    }
        } else {
            /* integer argument */
	    p = cairo_type1_font_subset_decode_integer (p, &value);
	    if (sp < 5)
		stack[sp++] = value;
        }
    }

    free (charstring);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
write_used_glyphs (cairo_type1_font_subset_t *font,
		   const char *name, int name_length,
		   const char *charstring, int charstring_length)
{
    cairo_status_t status;
    char buffer[256];
    int length;

    length = snprintf (buffer, sizeof buffer,
		       "/%.*s %d %s ",
		       name_length, name, charstring_length, font->rd);
    status = cairo_type1_font_subset_write_encrypted (font, buffer, length);
    if (unlikely (status))
	return status;

    status = cairo_type1_font_subset_write_encrypted (font,
					              charstring,
						      charstring_length);
    if (unlikely (status))
	return status;

    length = snprintf (buffer, sizeof buffer, "%s\n", font->nd);
    status = cairo_type1_font_subset_write_encrypted (font, buffer, length);
    if (unlikely (status))
	return status;

    return CAIRO_STATUS_SUCCESS;
}

typedef cairo_status_t (*glyph_func_t) (cairo_type1_font_subset_t *font,
			                const char *name, int name_length,
			                const char *charstring, int charstring_length);

static cairo_status_t
cairo_type1_font_subset_for_each_glyph (cairo_type1_font_subset_t *font,
					const char *dict_start,
					const char *dict_end,
					glyph_func_t func,
					const char **dict_out)
{
    int charstring_length, name_length, glyph_index;
    const char *p, *charstring, *name;
    char *end;

    /* We're looking at '/' in the name of the first glyph.  The glyph
     * definitions are on the form:
     *
     *   /name 23 RD <23 binary bytes> ND
     *
     * or alternatively using -| and |- instead of RD and ND.
     *
     * We parse the glyph name and see if it is in the subset.  If it
     * is, we call the specified callback with the glyph name and
     * glyph data, otherwise we just skip it.  We need to parse
     * through a glyph definition; we can't just find the next '/',
     * since the binary data could contain a '/'.
     */

    p = dict_start;

    while (*p == '/') {
	name = p + 1;
	p = skip_token (p, dict_end);
	name_length = p - name;

	charstring_length = strtol (p, &end, 10);
	if (p == end)
	    return CAIRO_INT_STATUS_UNSUPPORTED;

	/* Skip past -| or RD to binary data.  There is exactly one space
	 * between the -| or RD token and the encrypted data, thus '+ 1'. */
	charstring = skip_token (end, dict_end) + 1;

	/* Skip binary data and |- or ND token. */
	p = skip_token (charstring + charstring_length, dict_end);
	while (p < dict_end && _cairo_isspace(*p))
	    p++;

	/* In case any of the skip_token() calls above reached EOF, p will
	 * be equal to dict_end. */
	if (p == dict_end)
	    return CAIRO_INT_STATUS_UNSUPPORTED;

	glyph_index = cairo_type1_font_subset_lookup_glyph (font,
							    name, name_length);
	if (font->glyphs[glyph_index].subset_index >= 0) {
	    cairo_status_t status = func (font,
		                          name, name_length,
					  charstring, charstring_length);
	    if (unlikely (status))
		return status;
	}
    }

    *dict_out = p;

    return CAIRO_STATUS_SUCCESS;
}


static cairo_status_t
cairo_type1_font_subset_write_private_dict (cairo_type1_font_subset_t *font,
					    const char                *name)
{
    cairo_status_t status;
    const char *p, *charstrings, *dict_start;
    const char *closefile_token;
    char buffer[32], *glyph_count_end;
    int num_charstrings, length;

    /* The private dict holds hint information, common subroutines and
     * the actual glyph definitions (charstrings).
     *
     * FIXME: update this comment.
     *
     * What we do here is scan directly the /CharString token, which
     * marks the beginning of the glyph definitions.  Then we parse
     * through the glyph definitions and weed out the glyphs not in
     * our subset.  Everything else before and after the glyph
     * definitions is copied verbatim to the output.  It might be
     * worthwile to figure out which of the common subroutines are
     * used by the glyphs in the subset and get rid of the rest. */

    /* FIXME: The /Subrs array contains binary data and could
     * conceivably have "/CharStrings" in it, so we might need to skip
     * this more cleverly. */
    charstrings = find_token (font->cleartext, font->cleartext_end, "/CharStrings");
    if (charstrings == NULL)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    /* Scan past /CharStrings and the integer following it. */
    p = charstrings + strlen ("/CharStrings");
    num_charstrings = strtol (p, &glyph_count_end, 10);
    if (p == glyph_count_end)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    /* Look for a '/' which marks the beginning of the first glyph
     * definition. */
    for (p = glyph_count_end; p < font->cleartext_end; p++)
	if (*p == '/')
	    break;
    if (p == font->cleartext_end)
	return CAIRO_INT_STATUS_UNSUPPORTED;
    dict_start = p;

    status = cairo_type1_font_subset_get_glyph_names_and_widths (font);
    if (unlikely (status))
	return status;

    /* Now that we have the private dictionary broken down in
     * sections, do the first pass through the glyph definitions to
     * figure out which subrs and othersubrs are use and which extra
     * glyphs may be required by the seac operator. */
    status = cairo_type1_font_subset_for_each_glyph (font,
						     dict_start,
						     font->cleartext_end,
						     cairo_type1_font_subset_look_for_seac,
						     &p);
    if (unlikely (status))
	return status;

    closefile_token = find_token (p, font->cleartext_end, "closefile");
    if (closefile_token == NULL)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    status = cairo_type1_font_subset_get_glyph_names_and_widths (font);
    if (unlikely (status))
	return status;

    /* We're ready to start outputting. First write the header,
     * i.e. the public part of the font dict.*/
    status = cairo_type1_font_subset_write_header (font, name);
    if (unlikely (status))
	return status;

    font->base.header_size = _cairo_output_stream_get_position (font->output);


    /* Start outputting the private dict.  First output everything up
     * to the /CharStrings token. */
    status = cairo_type1_font_subset_write_encrypted (font, font->cleartext,
					         charstrings - font->cleartext);
    if (unlikely (status))
	return status;

    /* Write out new charstring count */
    length = snprintf (buffer, sizeof buffer,
		       "/CharStrings %d", font->num_glyphs);
    status = cairo_type1_font_subset_write_encrypted (font, buffer, length);
    if (unlikely (status))
	return status;

    /* Write out text between the charstring count and the first
     * charstring definition */
    status = cairo_type1_font_subset_write_encrypted (font, glyph_count_end,
	                                          dict_start - glyph_count_end);
    if (unlikely (status))
	return status;

    /* Write out the charstring definitions for each of the glyphs in
     * the subset. */
    status = cairo_type1_font_subset_for_each_glyph (font,
						     dict_start,
						     font->cleartext_end,
						     write_used_glyphs,
						     &p);
    if (unlikely (status))
	return status;

    /* Output what's left between the end of the glyph definitions and
     * the end of the private dict to the output. */
    status = cairo_type1_font_subset_write_encrypted (font, p,
	                        closefile_token - p + strlen ("closefile") + 1);
    if (unlikely (status))
	return status;

    if (font->hex_encode)
	_cairo_output_stream_write (font->output, "\n", 1);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
cairo_type1_font_subset_write_trailer(cairo_type1_font_subset_t *font)
{
    const char *cleartomark_token;
    int i;
    static const char zeros[65] =
	"0000000000000000000000000000000000000000000000000000000000000000\n";


    for (i = 0; i < 8; i++)
	_cairo_output_stream_write (font->output, zeros, sizeof zeros);

    cleartomark_token = find_token (font->type1_data, font->type1_end, "cleartomark");
    if (cleartomark_token) {
	/* Some fonts have conditional save/restore around the entire
	 * font dict, so we need to retain whatever postscript code
	 * that may come after 'cleartomark'. */

	_cairo_output_stream_write (font->output, cleartomark_token,
				    font->type1_end - cleartomark_token);
    } else if (!font->eexec_segment_is_ascii) {
	/* Fonts embedded in PDF may omit the fixed-content portion
	 * that includes the 'cleartomark' operator. Type 1 in PDF is
	 * always binary. */

	_cairo_output_stream_printf (font->output, "cleartomark");
    } else {
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    /* some fonts do not have a newline at the end of the last line */
    _cairo_output_stream_printf (font->output, "\n");

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
type1_font_write (void *closure, const unsigned char *data, unsigned int length)
{
    cairo_type1_font_subset_t *font = closure;

    return _cairo_array_append_multiple (&font->contents, data, length);
}

static cairo_status_t
cairo_type1_font_subset_write (cairo_type1_font_subset_t *font,
			       const char *name)
{
    cairo_status_t status;

    status = cairo_type1_font_subset_find_segments (font);
    if (unlikely (status))
	return status;

    status = cairo_type1_font_subset_decrypt_eexec_segment (font);
    if (unlikely (status))
	return status;

    /* Determine which glyph definition delimiters to use. */
    if (find_token (font->cleartext, font->cleartext_end, "/-|") != NULL) {
	font->rd = "-|";
	font->nd = "|-";
    } else if (find_token (font->cleartext, font->cleartext_end, "/RD") != NULL) {
	font->rd = "RD";
	font->nd = "ND";
    } else {
	/* Don't know *what* kind of font this is... */
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    font->eexec_key = CAIRO_TYPE1_PRIVATE_DICT_KEY;
    font->hex_column = 0;

    status = cairo_type1_font_subset_write_private_dict (font, name);
    if (unlikely (status))
	return status;

    font->base.data_size = _cairo_output_stream_get_position (font->output) -
	font->base.header_size;

    status = cairo_type1_font_subset_write_trailer (font);
    if (unlikely (status))
	return status;

    font->base.trailer_size =
	_cairo_output_stream_get_position (font->output) -
	font->base.header_size - font->base.data_size;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
cairo_type1_font_subset_generate (void       *abstract_font,
				  const char *name)

{
    cairo_type1_font_subset_t *font = abstract_font;
    cairo_ft_unscaled_font_t *ft_unscaled_font;
    unsigned long ret;
    cairo_status_t status;

    ft_unscaled_font = (cairo_ft_unscaled_font_t *) font->base.unscaled_font;
    font->face = _cairo_ft_unscaled_font_lock_face (ft_unscaled_font);
    if (unlikely (font->face == NULL))
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    font->type1_length = font->face->stream->size;
    font->type1_data = malloc (font->type1_length);
    if (unlikely (font->type1_data == NULL)) {
	status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	goto fail;
    }

    if (font->face->stream->read != NULL) {
	/* Note that read() may be implemented as a macro, thanks POSIX!, so we
	 * need to wrap the following usage in parentheses in order to
	 * disambiguate it for the pre-processor - using the verbose function
	 * pointer dereference for clarity.
	 */
	ret = (* font->face->stream->read) (font->face->stream, 0,
					    (unsigned char *) font->type1_data,
					    font->type1_length);
	if (ret != font->type1_length) {
	    status = _cairo_error (CAIRO_STATUS_READ_ERROR);
	    goto fail;
	}
    } else {
	memcpy (font->type1_data,
		font->face->stream->base, font->type1_length);
    }

    status = _cairo_array_grow_by (&font->contents, 4096);
    if (unlikely (status))
	goto fail;

    font->output = _cairo_output_stream_create (type1_font_write, NULL, font);
    if (unlikely ((status = font->output->status)))
	goto fail;

    status = cairo_type1_font_subset_write (font, name);
    if (unlikely (status))
	goto fail;

    font->base.data = _cairo_array_index (&font->contents, 0);

 fail:
    _cairo_ft_unscaled_font_unlock_face (ft_unscaled_font);

    return status;
}

static cairo_status_t
_cairo_type1_font_subset_fini (cairo_type1_font_subset_t *font)
{
    cairo_status_t status = CAIRO_STATUS_SUCCESS;
    unsigned int i;

    /* If the subset generation failed, some of the pointers below may
     * be NULL depending on at which point the error occurred. */

    _cairo_array_fini (&font->contents);

    free (font->type1_data);
    if (font->glyphs != NULL) {
	for (i = 0; i < font->base.num_glyphs; i++)
	    free (font->glyphs[i].name);
    }

    _cairo_unscaled_font_destroy (font->base.unscaled_font);

    if (font->output != NULL)
	status = _cairo_output_stream_destroy (font->output);

    if (font->base.base_font)
	free (font->base.base_font);
    free (font->glyphs);

    return status;
}

cairo_status_t
_cairo_type1_subset_init (cairo_type1_subset_t		*type1_subset,
			  const char			*name,
			  cairo_scaled_font_subset_t	*scaled_font_subset,
                          cairo_bool_t                   hex_encode)
{
    cairo_type1_font_subset_t font;
    cairo_status_t status, status_ignored;
    unsigned long parent_glyph, length;
    unsigned int i;
    cairo_unscaled_font_t *unscaled_font;
    char buf[30];

    /* XXX: Need to fix this to work with a general cairo_unscaled_font_t. */
    if (!_cairo_scaled_font_is_ft (scaled_font_subset->scaled_font))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if (_cairo_ft_scaled_font_is_vertical (scaled_font_subset->scaled_font))
        return CAIRO_INT_STATUS_UNSUPPORTED;

    unscaled_font = _cairo_ft_scaled_font_get_unscaled_font (scaled_font_subset->scaled_font);

    status = _cairo_type1_font_subset_init (&font, unscaled_font, scaled_font_subset, hex_encode);
    if (unlikely (status))
	return status;

    for (i = 0; i < scaled_font_subset->num_glyphs; i++) {
	parent_glyph = scaled_font_subset->glyphs[i];
	cairo_type1_font_subset_use_glyph (&font, parent_glyph);
    }

    status = cairo_type1_font_subset_generate (&font, name);
    if (unlikely (status))
	goto fail1;

    if (font.base.base_font) {
	type1_subset->base_font = strdup (font.base.base_font);
    } else {
        snprintf(buf, sizeof (buf), "CairoFont-%u-%u",
                 scaled_font_subset->font_id, scaled_font_subset->subset_id);
	type1_subset->base_font = strdup (buf);
    }
    if (unlikely (type1_subset->base_font == NULL))
	goto fail1;

    type1_subset->widths = calloc (sizeof (double), font.num_glyphs);
    if (unlikely (type1_subset->widths == NULL))
	goto fail2;
    for (i = 0; i < font.base.num_glyphs; i++) {
	if (font.glyphs[i].subset_index < 0)
	    continue;
	type1_subset->widths[font.glyphs[i].subset_index] =
	    font.glyphs[i].width;
    }

    type1_subset->x_min = font.base.x_min;
    type1_subset->y_min = font.base.y_min;
    type1_subset->x_max = font.base.x_max;
    type1_subset->y_max = font.base.y_max;
    type1_subset->ascent = font.base.ascent;
    type1_subset->descent = font.base.descent;

    length = font.base.header_size +
	     font.base.data_size +
	     font.base.trailer_size;
    type1_subset->data = malloc (length);
    if (unlikely (type1_subset->data == NULL))
	goto fail3;

    memcpy (type1_subset->data,
	    _cairo_array_index (&font.contents, 0), length);

    type1_subset->header_length = font.base.header_size;
    type1_subset->data_length = font.base.data_size;
    type1_subset->trailer_length = font.base.trailer_size;

    return _cairo_type1_font_subset_fini (&font);

 fail3:
    free (type1_subset->widths);
 fail2:
    free (type1_subset->base_font);
 fail1:
    status_ignored = _cairo_type1_font_subset_fini (&font);

    return status;
}

void
_cairo_type1_subset_fini (cairo_type1_subset_t *subset)
{
    free (subset->base_font);
    free (subset->widths);
    free (subset->data);
}

cairo_bool_t
_cairo_type1_scaled_font_is_type1 (cairo_scaled_font_t *scaled_font)
{
    cairo_ft_unscaled_font_t *unscaled;
    FT_Face face;
    PS_FontInfoRec font_info;
    cairo_bool_t is_type1 = FALSE;

    if (!_cairo_scaled_font_is_ft (scaled_font))
       return FALSE;
    unscaled = (cairo_ft_unscaled_font_t *) _cairo_ft_scaled_font_get_unscaled_font (scaled_font);
    face = _cairo_ft_unscaled_font_lock_face (unscaled);
    if (!face)
        return FALSE;

    if (FT_Get_PS_Font_Info(face, &font_info) == 0)
        is_type1 = TRUE;

    /* OpenType/CFF fonts also have a PS_FontInfoRec */
#if HAVE_FT_LOAD_SFNT_TABLE
    if (FT_IS_SFNT (face))
        is_type1 = FALSE;
#endif

    _cairo_ft_unscaled_font_unlock_face (unscaled);

    return is_type1;
}

#endif /* CAIRO_HAS_FT_FONT */

#endif /* CAIRO_HAS_FONT_SUBSET */
