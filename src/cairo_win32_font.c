/*
 * Copyright © 2005 Red Hat Inc.
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
 * Author: Owen Taylor <otaylor@redhat.com>
 */

#include "cairoint.h"

const cairo_font_backend_t cairo_ft_font_backend;

/*
 * The simple 2x2 matrix is converted into separate scale and shape
 * factors so that hinting works right
 */

typedef struct {
    double  x_scale, y_scale;
    double  shape[2][2];
} ft_font_transform_t;

static void
_compute_transform (ft_font_transform_t *sf,
		    cairo_font_scale_t  *sc)
{
    cairo_matrix_t normalized;
    double tx, ty;
    
    /* The font matrix has x and y "scale" components which we extract and
     * use as character scale values. These influence the way freetype
     * chooses hints, as well as selecting different bitmaps in
     * hand-rendered fonts. We also copy the normalized matrix to
     * freetype's transformation.
     */

    cairo_matrix_set_affine (&normalized,
			     sc->matrix[0][0],
			     sc->matrix[0][1],
			     sc->matrix[1][0],
			     sc->matrix[1][1], 
			     0, 0);

    _cairo_matrix_compute_scale_factors (&normalized, 
					 &sf->x_scale, &sf->y_scale,
					 /* XXX */ 1);
    cairo_matrix_scale (&normalized, 1.0 / sf->x_scale, 1.0 / sf->y_scale);
    cairo_matrix_get_affine (&normalized, 
			     &sf->shape[0][0], &sf->shape[0][1],
			     &sf->shape[1][0], &sf->shape[1][1],
			     &tx, &ty);
}

/* Temporarily scales an unscaled font to the give scale. We catch
 * scaling to the same size, since changing a FT_Face is expensive.
 */
static void
_ft_unscaled_font_set_scale (ft_unscaled_font_t *unscaled,
			     cairo_font_scale_t *scale)
{
    int need_scale;
    ft_font_transform_t sf;
    FT_Matrix mat;

    assert (unscaled->face != NULL);
    
    if (scale->matrix[0][0] == unscaled->current_scale.matrix[0][0] &&
	scale->matrix[0][1] == unscaled->current_scale.matrix[0][1] &&
	scale->matrix[1][0] == unscaled->current_scale.matrix[1][0] &&
	scale->matrix[1][1] == unscaled->current_scale.matrix[1][1])
	return;

    unscaled->current_scale = *scale;
	
    _compute_transform (&sf, scale);

    unscaled->x_scale = sf.x_scale;
    unscaled->y_scale = sf.y_scale;
	
    mat.xx = DOUBLE_TO_16_16(sf.shape[0][0]);
    mat.yx = - DOUBLE_TO_16_16(sf.shape[0][1]);
    mat.xy = - DOUBLE_TO_16_16(sf.shape[1][0]);
    mat.yy = DOUBLE_TO_16_16(sf.shape[1][1]);

    if (need_scale) {
	FT_Set_Transform(unscaled->face, &mat, NULL);
	
	FT_Set_Pixel_Sizes(unscaled->face,
			   (FT_UInt) sf.x_scale,
			   (FT_UInt) sf.y_scale);
    }
}

/* implement the font backend interface */

typedef struct {
    cairo_font_t base;
    FcPattern *pattern;
    int load_flags;
    ft_unscaled_font_t *unscaled;
} cairo_ft_font_t;

static void 
_utf8_to_ucs4 (char const *utf8, 
               FT_ULong **ucs4, 
               size_t *nchars)
{
    int len = 0, step = 0;
    size_t n = 0, alloc = 0;
    FcChar32 u = 0;

    if (utf8 == NULL || ucs4 == NULL || nchars == NULL)
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
    *nchars = n;
}

static BYTE
_get_system_quality (void)
{
    BOOL font_smoothing;

    if (!SystemParametersInfo (SPI_GETFONTSMOOTHING, 0, &font_smoothing, 0)) {
	_print_gdi_error ();
	return FALSE;
    }

    if (font_smoothing) {
	OSVERSIONINFO &version_info;

	version_info.size = sizeof (OSVERSIONINFO);
	
	if (!GetVersionEx (&version_info)) {
	    _print_gdi_error ();
	    return FALSE;
	}

	if (version_info.dwMajorVersion > 5 ||
	    (version_info.dwMajorVersion == 5 &&
	     version_info.dwMinorVersion >= 1))	{ /* XP or newer */
	    UINT smoothing_type;

	    if (!SystemParametersInfo (SPI_GETFONTSMOOTHINGTYPE,
				       0, &smoothing_type, 0)) {
		_print_gdi_error ();
		return FALSE;
	    }

	    if (smoothing_type == FE_FONTSMOTHINGCLEARTYPE)
		return CLEARTYPE_QUALITY;
	}

	return ANTIALIASED_QUALITY;
    }

    return 
    
}

static cairo_status_t
_cairo_ft_font_create (const char          *family, 
                       cairo_font_slant_t   slant, 
                       cairo_font_weight_t  weight,
		       cairo_font_scale_t  *scale,
		       cairo_font_t       **font_out)
{
    LOGFONT logfont;
    cairo_font_t *font;

    logfont.lfHeight = 0;	/* filled in later */
    logfont.lfWidth = 0;	/* filled in later */
    logfont.lfEscapement = 0;	/* filled in later */
    logfont.lfOrientation = 0;	/* filled in later */
    logfont.lfOrientation = 0;	/* filled in later */

    switch (weight) {
    case CAIRO_FONT_WEIGHT_NORMAL:
    default:
	logfont.lfWeight = FW_NORMAL;
    break:
    case CAIRO_FONT_WEIGHT_BOLD:
	logfont.lfWeight = FW_BOLD;
	break;
    }

    switch (slant) {
    case CAIRO_FONT_SLANT_NORMAL:
    default:
	logfont.lfItalic = FALSE;
	break;
    case CAIRO_FONT_SLANT_ITALIC:
    case CAIRO_FONT_SLANT_OBLIQUE:
	logfont.lfItalic = TRUE;
	break;
    }

    logfont.lfUnderline = FALSE;
    logfont.lfStrikethrough = FALSE;
    /* The docs for LOGFONT discourage using this, since the
     * interpretation is locale-specific, but it's not clear what
     * would be a better alternative.
     */
    logfont.lfCharset = DEFAULT_CHARSET; 
    logfont.lfOutPrecision = OUT_DEFAULT_PRECIS;
    logfont.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    logfont.lfQuality = DEFAULT_QUALITY;
    logfont.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
    logfont.lfFaceName = utf8_to_utf16 (family);

    if (!logfont.lfFaceName)
	return CAIRO_STATUS_NO_MEMORY;
    
    font = cairo_win32_font_create_for_logfont (logfont, scale);
    if (!font)
	return CAIRO_STATUS_NO_MEMORY;

    *font_out = font;

    return CAIRO_STATUS_SUCCESS;
}

static void 
_cairo_ft_font_destroy_font (void *abstract_font)
{
    cairo_win32_font_t *font = abstract_font;

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_ft_font_get_glyph_cache_key (void                    *abstract_font,
				    cairo_glyph_cache_key_t *key)
{
    return CAIRO_STATUS_NO_MEMORY;
}

static cairo_status_t 
_cairo_ft_font_text_to_glyphs (void			*abstract_font,
			       const unsigned char	*utf8,
			       cairo_glyph_t		**glyphs, 
			       int			*nglyphs)
{
    cairo_win32_font_t *font = abstract_font;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t 
_cairo_ft_font_font_extents (void			*abstract_font,
                             cairo_font_extents_t	*extents)
{
    cairo_win32_font_t *font = abstract_font;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t 
_cairo_ft_font_glyph_extents (void			*abstract_font,
                              cairo_glyph_t		*glyphs, 
                              int			num_glyphs,
			      cairo_text_extents_t	*extents)
{
    cairo_win32_font_t *font = abstract_font;

    return CAIRO_STATUS_SUCCESS;
}


static cairo_status_t 
_cairo_ft_font_glyph_bbox (void		       		*abstract_font,
			   const cairo_glyph_t 		*glyphs,
			   int                 		num_glyphs,
			   cairo_box_t         		*bbox)
{
    cairo_win32_font_t *font = abstract_font;
    
    return CAIRO_STATUS_SUCCESS;
}


static cairo_status_t 
_cairo_ft_font_show_glyphs (void			*abstract_font,
                            cairo_operator_t    	operator,
                            cairo_surface_t     	*source,
			    cairo_surface_t     	*surface,
			    int                 	source_x,
			    int                 	source_y,
                            const cairo_glyph_t 	*glyphs,
                            int                 	num_glyphs)
{
    cairo_win32_font_t *font = abstract_font;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t 
_cairo_win32_font_glyph_path (void			*abstract_font,
			      cairo_glyph_t		*glyphs, 
			      int			num_glyphs,
			      cairo_path_t		*path)
{
    cairo_win32_font_t *font = abstract_font;
  
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t 
_cairo_win32_font_create_glyph (cairo_image_glyph_cache_entry_t *val)    
{
    return CAIRO_STATUS_NO_MEMORY;
}

const cairo_font_backend_t cairo_win32_font_backend = {
    _cairo_win32_font_create,
    _cairo_win32_font_destroy_font,
    _cairo_win32_font_destroy_unscaled_font,
    _cairo_win32_font_font_extents,
    _cairo_win32_font_text_to_glyphs,
    _cairo_win32_font_glyph_extents,
    _cairo_win32_font_glyph_bbox,
    _cairo_win32_font_show_glyphs,
    _cairo_win32_font_glyph_path,
    _cairo_win32_font_get_glyph_cache_key,
    _cairo_win32_font_create_glyph
};

/* implement the platform-specific interface */

cairo_font_t *
cairo_win32_font_create_for_logfont (LOGFONT        *logfont,
				     cairo_matrix_t *scale)
{
    cairo_win32_font_t *f;

    f = malloc (sizeof(cairo_win32_font_t));
    if (f == NULL) 
      return NULL;

    f->logfont = *logfont;


    _cairo_font_init ((cairo_font_t *)f, &sc, &cairo_win32_font_backend);

    return (cairo_font_t *)f;
}
