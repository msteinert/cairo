/*
 * Copyright © 2003 Red Hat Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and
 * its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies and that
 * both that copyright notice and this permission notice appear in
 * supporting documentation, and that the name of Red Hat Inc. not be used
 * in advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission. Red Hat Inc. makes no
 * representations about the suitability of this software for any purpose.
 * It is provided "as is" without express or implied warranty.
 *
 * RED HAT INC. DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL RED HAT INC. BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
 * USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 * Author: Graydon Hoare <graydon@redhat.com>
 */

#include "cairoint.h"
#include <fontconfig/fontconfig.h>
#include <fontconfig/fcfreetype.h>
#include <freetype/freetype.h>

typedef struct {
    cairo_font_t base;  
    FT_Face face;
    int owner_of_face_p;
    FcPattern *pattern;
} cairo_ft_font_t;


#define DOUBLE_TO_26_6(d) ((FT_F26Dot6)((d) * 63.0))
#define DOUBLE_FROM_26_6(t) (((double)((t) >> 6)) \
			     + ((double)((t) & 0x3F) / 63.0))
#define DOUBLE_TO_16_16(d) ((FT_Fixed)((d) * 65535.0))
#define DOUBLE_FROM_16_16(t) (((double)((t) >> 16)) \
			      + ((double)((t) & 0xFFFF) / 65535.0))

static FT_Library _cairo_ft_lib = NULL;
static int
_init_cairo_ft_lib (void)
{
    if (_cairo_ft_lib != NULL)
        return 1;
    
    if (FT_Init_FreeType (&_cairo_ft_lib) == 0
        && _cairo_ft_lib != NULL)
        return 1;
    
    return 0;  
}

/* implement the platform-specific interface */

cairo_font_t *
cairo_ft_font_create (FcPattern *pattern)
{
    cairo_ft_font_t *f = NULL;
    char *filename = NULL;
    FT_Face face = NULL;
    int own_face = 0;
    FcPattern *resolved = NULL;
    FcResult result = FcResultMatch;
    
    if (!_init_cairo_ft_lib ())
        return NULL;
    
    FcConfigSubstitute (0, pattern, FcMatchPattern);
    FcDefaultSubstitute (pattern);
    
    resolved = FcFontMatch (0, pattern, &result);
    if (result != FcResultMatch)
    {
        if (resolved)
            FcPatternDestroy (resolved);
        return NULL;
    }
    
    /* If the pattern has an FT_Face object, use that. */
    if (FcPatternGetFTFace (resolved, FC_FT_FACE, 0, &face) != FcResultMatch
        || face == NULL)
    {

        /* otherwise it had better have a filename */
        int open_res = 0;
        own_face = 1;
        result = FcPatternGetString (resolved, FC_FILE, 0, (FcChar8 **)(&filename));
      
        if (result == FcResultMatch)
            open_res = FT_New_Face (_cairo_ft_lib, filename, 0, &face);
      
        if (face == NULL)
            return NULL;
    }

    f = (cairo_ft_font_t *) cairo_ft_font_create_for_ft_face (face);
    if (f != NULL)
        f->pattern = FcPatternDuplicate (resolved);

    f->owner_of_face_p = own_face;

    FcPatternDestroy (resolved);
    return (cairo_font_t *) f;
}

FT_Face
cairo_ft_font_face (cairo_font_t *font)
{
    if (font == NULL)
        return NULL;

    return ((cairo_ft_font_t *) font)->face;
}

FcPattern *
cairo_ft_font_pattern (cairo_font_t  *font)
{
    if (font == NULL)
        return NULL;

    return ((cairo_ft_font_t *) font)->pattern;
}



/* implement the backend interface */

static cairo_font_t *
_cairo_ft_font_create (char                 *family, 
                       cairo_font_slant_t   slant, 
                       cairo_font_weight_t  weight)
{
    cairo_ft_font_t *ft_font = NULL;
    cairo_font_t *font = NULL;
    FcPattern * pat = NULL;
    int fcslant;
    int fcweight;

    pat = FcPatternCreate ();
    if (pat == NULL)
        return NULL;

    switch (weight)
    {
    case CAIRO_FONT_WEIGHT_BOLD:
        fcweight = FC_WEIGHT_BOLD;
        break;
    case CAIRO_FONT_WEIGHT_NORMAL:
    default:
        fcweight = FC_WEIGHT_MEDIUM;
        break;
    }

    switch (slant)
    {
    case CAIRO_FONT_SLANT_ITALIC:
        fcslant = FC_SLANT_ITALIC;
        break;
    case CAIRO_FONT_SLANT_OBLIQUE:
	fcslant = FC_SLANT_OBLIQUE;
        break;
    case CAIRO_FONT_SLANT_NORMAL:
    default:
        fcslant = FC_SLANT_ROMAN;
        break;
    }

    FcPatternAddString (pat, FC_FAMILY, family);
    FcPatternAddInteger (pat, FC_SLANT, fcslant);
    FcPatternAddInteger (pat, FC_WEIGHT, fcweight);

    font = cairo_ft_font_create (pat);
    ft_font = (cairo_ft_font_t *) font;

    FT_Set_Char_Size (ft_font->face,
                      DOUBLE_TO_26_6 (1.0),
                      DOUBLE_TO_26_6 (1.0),
                      0, 0);
  
    FcPatternDestroy (pat);
    return font;  
}


static cairo_font_t *
_cairo_ft_font_copy (cairo_font_t *font)
{
    cairo_ft_font_t * ft_font_new = NULL;
    cairo_ft_font_t * ft_font = NULL;
  
    ft_font = (cairo_ft_font_t *)font;

    ft_font_new = (cairo_ft_font_t *)cairo_ft_font_create_for_ft_face (ft_font->face);
    if (ft_font_new == NULL)
        return NULL;

    if (ft_font_new != NULL && ft_font->pattern != NULL)
        ft_font_new->pattern = FcPatternDuplicate (ft_font->pattern);  

    return (cairo_font_t *)ft_font_new;
}

static void 
_cairo_ft_font_destroy (cairo_font_t *font)
{
    cairo_ft_font_t * ft_font = NULL;
  
    if (font == NULL)
        return;

    ft_font = (cairo_ft_font_t *)font;
  
    if (ft_font->face != NULL && ft_font->owner_of_face_p)
        FT_Done_Face (ft_font->face);
  
    if (ft_font->pattern != NULL)
        FcPatternDestroy (ft_font->pattern);

    free (ft_font);
}

static void 
_utf8_to_ucs4 (char const *utf8, 
               FT_ULong **ucs4, 
               size_t *nchars)
{
    int len = 0, step = 0;
    size_t n = 0, alloc = 0;
    FcChar32 u = 0;

    if (ucs4 == NULL || nchars == NULL)
        return;

    len = strlen (utf8);
    alloc = len;
    *ucs4 = malloc (sizeof (FT_ULong) * alloc);
    if (*ucs4 == NULL)
        return;
  
    while (len && (step = FcUtf8ToUcs4(utf8, &u, len)) > 0)
    {
        if (n == alloc)
        {
            alloc *= 2;
            *ucs4 = realloc (*ucs4, sizeof (FT_ULong) * alloc);
            if (*ucs4 == NULL)
                return;
        }	  
        (*ucs4)[n++] = u;
        len -= step;
        utf8 += step;
    }
    *nchars = alloc;
}

static void
_get_scale_factors(cairo_matrix_t *matrix, double *sx, double *sy)
{
    double e0, e1;
    e1 = 1.; e0 = 0.;

    cairo_matrix_transform_distance (matrix, &e1, &e0);
    *sx = sqrt(e1*e1 + e0*e0);

    e1 = 1.; e0 = 0.;
    cairo_matrix_transform_distance (matrix, &e0, &e1);
    *sy = sqrt(e1*e1 + e0*e0);    
}

static void
_install_font_matrix(cairo_matrix_t *matrix, FT_Face face)
{
    cairo_matrix_t normalized;
    double scale_x, scale_y;
    double xx, xy, yx, yy, tx, ty;
    FT_Matrix mat;
    
    /* The font matrix has x and y "scale" components which we extract and
     * use as pixel scale values. These influence the way freetype chooses
     * hints, as well as selecting different bitmaps in hand-rendered
     * fonts. We also copy the normalized matrix to freetype's
     * transformation.
     */

    _get_scale_factors(matrix, &scale_x, &scale_y);
    
    cairo_matrix_copy (&normalized, matrix);

    cairo_matrix_scale (&normalized, 1.0 / scale_x, 1.0 / scale_y);
    cairo_matrix_get_affine (&normalized, 
                             &xx /* 00 */ , &yx /* 01 */, 
                             &xy /* 10 */, &yy /* 11 */, 
                             &tx, &ty);

    mat.xx = DOUBLE_TO_16_16(xx);
    mat.xy = -DOUBLE_TO_16_16(xy);
    mat.yx = -DOUBLE_TO_16_16(yx);
    mat.yy = DOUBLE_TO_16_16(yy);  

    FT_Set_Transform(face, &mat, NULL);
    FT_Set_Char_Size(face,
		     DOUBLE_TO_26_6(scale_x),
		     DOUBLE_TO_26_6(scale_y),
		     0, 0);
}


static int
_utf8_to_glyphs (cairo_font_t        *font,
		 const unsigned char *utf8,
		 cairo_glyph_t       **glyphs,
		 size_t              *nglyphs)
{
    cairo_ft_font_t *ft;
    double x = 0., y = 0.;
    size_t i;
    FT_ULong *ucs4 = NULL; 

    if (font == NULL)
        return 0;

    ft = (cairo_ft_font_t *)font;

    _utf8_to_ucs4 (utf8, &ucs4, nglyphs);

    if (ucs4 == NULL)
        return 0;

    *glyphs = (cairo_glyph_t *) malloc ((*nglyphs) * (sizeof (cairo_glyph_t)));
    if (*glyphs == NULL)
    {
        free (ucs4);
        return 0;
    }

    _install_font_matrix (&font->matrix, ft->face);

    for (i = 0; i < *nglyphs; i++)
    {            
        (*glyphs)[i].index = FT_Get_Char_Index (ft->face, ucs4[i]);
        (*glyphs)[i].x = x;
        (*glyphs)[i].y = y;

        FT_Load_Glyph (ft->face, (*glyphs)[i].index, FT_LOAD_DEFAULT);
	
        x += DOUBLE_FROM_26_6 (ft->face->glyph->advance.x);
        y -= DOUBLE_FROM_26_6 (ft->face->glyph->advance.y);
    }

    free (ucs4);
    return 1;
}

static cairo_status_t 
_cairo_ft_font_font_extents (cairo_font_t *font,
                             cairo_font_extents_t *extents)
{
    double scale_x, scale_y;
    cairo_ft_font_t *ft = (cairo_ft_font_t *)font;
    cairo_status_t status = CAIRO_STATUS_SUCCESS;

    _get_scale_factors(&font->matrix, &scale_x, &scale_y);

#define FONT_UNIT_TO_DEV(x) ((double)(x) / (double)(ft->face->units_per_EM))

    extents->ascent = FONT_UNIT_TO_DEV(ft->face->ascender) * scale_y;
    extents->descent = FONT_UNIT_TO_DEV(ft->face->descender) * scale_y;
    extents->height = FONT_UNIT_TO_DEV(ft->face->height) * scale_y;
    extents->max_x_advance = FONT_UNIT_TO_DEV(ft->face->max_advance_width) * scale_x;
    extents->max_y_advance = FONT_UNIT_TO_DEV(ft->face->max_advance_height) * scale_y;
    return status;
}

static cairo_status_t 
_cairo_ft_font_glyph_extents (cairo_font_t         *font,
                              cairo_glyph_t        *glyphs, 
                              int                  num_glyphs,
			      cairo_text_extents_t *extents)
{
    cairo_ft_font_t *ft;
    cairo_status_t status = CAIRO_STATUS_SUCCESS;

    ft = (cairo_ft_font_t *)font;

    /* FIXME: lift code from xft to do this */

    return status;
}

static cairo_status_t 
_cairo_ft_font_text_extents (cairo_font_t        *font,
                             const unsigned char *utf8,
			      cairo_text_extents_t *extents)
{
    cairo_glyph_t *glyphs;
    size_t nglyphs;
    cairo_status_t status = CAIRO_STATUS_SUCCESS;

    if (_utf8_to_glyphs (font, utf8, &glyphs, &nglyphs))
    {
        status = _cairo_ft_font_glyph_extents (font, glyphs, nglyphs, 
					       extents);      
        free (glyphs);
    }
    return status;
}
		     


static cairo_status_t 
_cairo_ft_font_show_glyphs (cairo_font_t        *font,
                            cairo_operator_t    operator,
                            cairo_surface_t     *source,
                            cairo_surface_t     *surface,
                            double              x0,
                            double              y0,
                            const cairo_glyph_t *glyphs,
                            int                 num_glyphs)
{
    int i;
    cairo_ft_font_t *ft = NULL;
    FT_GlyphSlot glyphslot;
    cairo_surface_t *mask = NULL;

    double x, y;
    int width, height, stride;

    if (font == NULL 
        || source == NULL 
        || surface == NULL 
        || glyphs == NULL)
        return CAIRO_STATUS_NO_MEMORY;

    ft = (cairo_ft_font_t *)font;
    glyphslot = ft->face->glyph;
    _install_font_matrix (&font->matrix, ft->face);

    for (i = 0; i < num_glyphs; i++)
    {
        FT_Load_Glyph (ft->face, glyphs[i].index, FT_LOAD_DEFAULT);
        FT_Render_Glyph (glyphslot, FT_RENDER_MODE_NORMAL);
      
        width = glyphslot->bitmap.width;
        height = glyphslot->bitmap.rows;
        stride = glyphslot->bitmap.pitch;
   
	x = x0 + glyphs[i].x;
	y = y0 + glyphs[i].y;      

        /* X gets upset with zero-sized images (such as whitespace) */
        if (width * height != 0)
        {
	    unsigned char	*bitmap = glyphslot->bitmap.buffer;
	    
	    /*
	     * XXX 
	     * reformat to match libic alignment requirements.
	     * This should be done before rendering the glyph,
	     * but that requires using FT_Outline_Get_Bitmap
	     * function
	     */
	    if (stride & 3)
	    {
		int		nstride = (stride + 3) & ~3;
		unsigned char	*g, *b;
		int		h;
		
		bitmap = malloc (nstride * height);
		if (!bitmap)
		    return CAIRO_STATUS_NO_MEMORY;
		g = glyphslot->bitmap.buffer;
		b = bitmap;
		h = height;
		while (h--)
		{
		    memcpy (b, g, width);
		    b += nstride;
		    g += stride;
		}
		stride = nstride;
	    }
            mask = cairo_surface_create_for_image (bitmap,
                                                  CAIRO_FORMAT_A8,
                                                  width, height, stride);
            if (mask == NULL)
	    {
		if (bitmap != glyphslot->bitmap.buffer)
		    free (bitmap);
                return CAIRO_STATUS_NO_MEMORY;
	    }

            _cairo_surface_composite (operator, source, mask, surface,
                                      0, 0, 0, 0, 
				      x + glyphslot->bitmap_left, 
				      y - glyphslot->bitmap_top, 
                                      (double)width, (double)height);

            cairo_surface_destroy (mask);
	    if (bitmap != glyphslot->bitmap.buffer)
		free (bitmap);
        }
    }  
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t 
_cairo_ft_font_show_text (cairo_font_t        *font,
                          cairo_operator_t    operator,
                          cairo_surface_t     *source,
                          cairo_surface_t     *surface,
                          double              x0,
                          double              y0,
                          const unsigned char *utf8)
{
    cairo_glyph_t *glyphs;
    size_t nglyphs;
    
    if (_utf8_to_glyphs (font, utf8, &glyphs, &nglyphs))
    {
        cairo_status_t res;
        res = _cairo_ft_font_show_glyphs (font, operator, 
                                          source, surface, x0, y0, 
                                          glyphs, nglyphs);      
        free (glyphs);
        return res;
    }
    else
        return CAIRO_STATUS_NO_MEMORY;
}


static cairo_status_t 
_cairo_ft_font_glyph_path (cairo_font_t        *font,
                           cairo_path_t        *path, 
                           cairo_glyph_t       *glyphs, 
                           int                 num_glyphs)
{
    cairo_status_t status = CAIRO_STATUS_SUCCESS;
    cairo_ft_font_t *ft;
    
    ft = (cairo_ft_font_t *)font;
    
    /* FIXME: lift code from xft to do this */
    
    return status;
}

static cairo_status_t 
_cairo_ft_font_text_path (cairo_font_t        *font,
                          cairo_path_t        *path, 
                          const unsigned char *utf8)
{
    cairo_glyph_t *glyphs;
    size_t nglyphs;
    
    if (_utf8_to_glyphs (font, utf8, &glyphs, &nglyphs))
    {
        cairo_status_t res;
        res = _cairo_ft_font_glyph_path (font, path, glyphs, nglyphs);      
        free (glyphs);
        return res;
    }
    else
        return CAIRO_STATUS_NO_MEMORY;
}


cairo_font_t *
cairo_ft_font_create_for_ft_face (FT_Face face)
{
    cairo_ft_font_t *f = NULL;
    
    f = malloc (sizeof (cairo_ft_font_t));
    if (f == NULL)
	return NULL;
    memset (f, 0, sizeof (cairo_ft_font_t));
    
    _cairo_font_init (&f->base, 
		      &cairo_ft_font_backend);
    
    f->face = face;
    f->owner_of_face_p = 0;
    return (cairo_font_t *) f;
}


const struct cairo_font_backend cairo_ft_font_backend = {
    font_extents:          (void *) _cairo_ft_font_font_extents,
    text_extents:          (void *) _cairo_ft_font_text_extents,
    glyph_extents:         (void *) _cairo_ft_font_glyph_extents,
    show_text:             (void *) _cairo_ft_font_show_text,
    show_glyphs:           (void *) _cairo_ft_font_show_glyphs,
    text_path:             (void *) _cairo_ft_font_text_path,
    glyph_path:            (void *) _cairo_ft_font_glyph_path,
    create:                (void *) _cairo_ft_font_create,
    copy:                  (void *) _cairo_ft_font_copy,
    destroy:               (void *) _cairo_ft_font_destroy
};
