/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2005 Red Hat, Inc
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
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
 */

#include "cairo-win32-private.h"

const cairo_font_backend_t cairo_ft_font_backend;

#define LOGICAL_SCALE 32

typedef struct {
    cairo_font_t base;

    LOGFONT logfont;
    BYTE quality;

    /* We do drawing and metrics computation in a "logical space" which
     * is similar to font space, except that it is scaled by a factor
     * of the (desired font size) * (LOGICAL_SCALE). The multiplication
     * by LOGICAL_SCALE allows for sub-pixel precision.
     */
    double logical_scale;

    /* The size we should actually request the font at from Windows; differs
     * from the logical_scale because it is quantized for orthogonal
     * transformations
     */
    double logical_size;

    /* Transformations from device <=> logical space
     */
    cairo_matrix_t logical_to_device;
    cairo_matrix_t device_to_logical;

    /* We special case combinations of 90-degree-rotations, scales and
     * flips ... that is transformations that take the axes to the
     * axes. If preserve_axes is true, then swap_axes/swap_x/swap_y
     * encode the 8 possibilities for orientation (4 rotation angles with
     * and without a flip), and scale_x, scale_y the scale components.
     */
    cairo_bool_t preserve_axes;
    cairo_bool_t swap_axes;
    cairo_bool_t swap_x;
    cairo_bool_t swap_y;
    double x_scale;
    double y_scale;
    
} cairo_win32_font_t;

static void
_compute_transform (cairo_win32_font_t *font,
		    cairo_font_scale_t *sc)
{
    cairo_matrix_t normalized;
    int pixel_size;

    if (NEARLY_ZERO (sc->matrix[0][1]) && NEARLY_ZERO (sc->matrix[1][0])) {
	font->preserve_axes = TRUE;
	font->x_scale = sc->matrix[0][0];
	font->swap_x = (sc->matrix[0][0] < 0);
	font->y_scale = sc->matrix[1][1];
	font->swap_y = (sc->matrix[1][1] < 0);
	font->swap_axes = FALSE;
	
    } else if (NEARLY_ZERO (sc->matrix[0][0]) && NEARLY_ZERO (sc->matrix[1][1])) {
	font->preserve_axes = TRUE;
	font->x_scale = sc->matrix[0][1];
	font->swap_x = (sc->matrix[0][1] < 0);
	font->y_scale = sc->matrix[1][0];
	font->swap_y = (sc->matrix[1][0] < 0);
	font->swap_axes = TRUE;
    }

    if (font->preserve_axes) {
	if (font->swap_x)
	    font->x_scale = - font->x_scale;
	if (font->swap_y)
	    font->y_scale = - font->y_scale;
	
	font->logical_scale = LOGICAL_SCALE * font->y_scale;
	font->logical_size = LOGICAL_SCALE * floor (font->y_scale + 0.5);
    }

    /* The font matrix has x and y "scale" components which we extract and
     * use as character scale values.
     */
    cairo_matrix_set_affine (&font->logical_to_device,
			     sc->matrix[0][0],
			     sc->matrix[0][1],
			     sc->matrix[1][0],
			     sc->matrix[1][1], 
			     0, 0);

    if (!font->preserve_axes) {
	double x_scale, y_scale;
	
	_cairo_matrix_compute_scale_factors (&font->logical_to_device,
					     &font->x_scale, &font->y_scale,
					     TRUE);	/* XXX: Handle vertical text */

	font->logical_size = floor (LOGICAL_SCALE * y_scale + 0.5);
	font->logical_scale = LOGICAL_SCALE * y_scale + 0.5;
    }

    cairo_matrix_scale (&font->logical_to_device,
			1.0 / (LOGICAL_SCALE * font->y_scale),
			1.0 / (LOGICAL_SCALE * font->y_scale));

    font->device_to_logical = font->logical_to_device;
    if (!CAIRO_OK (cairo_matrix_invert (&font->device_to_logical)))
	cairo_matrix_set_identity (font->device_to_logical);
}

static BYTE
_get_system_quality (void)
{
    BOOL font_smoothing;

    if (!SystemParametersInfo (SPI_GETFONTSMOOTHING, 0, &font_smoothing, 0)) {
	_cairo_win32_print_gdi_error ();
	return FALSE;
    }

    if (font_smoothing) {
	OSVERSIONINFO &version_info;

	version_info.size = sizeof (OSVERSIONINFO);
	
	if (!GetVersionEx (&version_info)) {
	    _cairo_win32_print_gdi_error ();
	    return FALSE;
	}

	if (version_info.dwMajorVersion > 5 ||
	    (version_info.dwMajorVersion == 5 &&
	     version_info.dwMinorVersion >= 1))	{ /* XP or newer */
	    UINT smoothing_type;

	    if (!SystemParametersInfo (SPI_GETFONTSMOOTHINGTYPE,
				       0, &smoothing_type, 0)) {
		_cairo_win32_print_gdi_error ();
		return FALSE;
	    }

	    if (smoothing_type == FE_FONTSMOTHINGCLEARTYPE)
		return CLEARTYPE_QUALITY;
	}

	return ANTIALIASED_QUALITY;
    }

    return 
    
}

static cairo_font_t *
_win32_font_create (LOGFONT            *logfont,
		    cairo_font_scale_t *scale)
{
    cairo_win32_font_t *f;
    ft_font_transform_t sf,

    f = malloc (sizeof(cairo_win32_font_t));
    if (f == NULL) 
      return NULL;

    f->logfont = *logfont;
    f->quality = _get_system_quality ();

    _compute_transform (f, scale);
    
    _cairo_font_init ((cairo_font_t *)f, &sc, &cairo_win32_font_backend);

    return (cairo_font_t *)f;
}

/* implement the font backend interface */

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
    
    font = _win32_font_create (logfont, scale);
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

/* Taken from fontconfig sources, Copyright © 2000 Keith Packard */
int
_utf8_to_ucs4 (const char *src_orig,
	      unsigned long *dst,
	      int	    len)
{
    const FcChar8   *src = src_orig;
    FcChar8	    s;
    int		    extra;
    FcChar32	    result;

    if (len == 0)
	return 0;
    
    s = *src++;
    len--;
    
    if (!(s & 0x80))
    {
	result = s;
	extra = 0;
    } 
    else if (!(s & 0x40))
    {
	return -1;
    }
    else if (!(s & 0x20))
    {
	result = s & 0x1f;
	extra = 1;
    }
    else if (!(s & 0x10))
    {
	result = s & 0xf;
	extra = 2;
    }
    else if (!(s & 0x08))
    {
	result = s & 0x07;
	extra = 3;
    }
    else if (!(s & 0x04))
    {
	result = s & 0x03;
	extra = 4;
    }
    else if ( ! (s & 0x02))
    {
	result = s & 0x01;
	extra = 5;
    }
    else
    {
	return -1;
    }
    if (extra > len)
	return -1;
    
    while (extra--)
    {
	result <<= 6;
	s = *src++;
	
	if ((s & 0xc0) != 0x80)
	    return -1;
	
	result |= s & 0x3f;
    }
    *dst = result;
    return src - src_orig;
}

static cairo_status_t
_utf8_to_utf16 (char const *utf8, 
               FT_ULong  **ucs4, 
               size_t     *nchars)
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
        (*ucs4)[n++] = u;
        len -= step;
        utf8 += step;
    }
    *nchars = n;

    return CAIRO_STATUS_NO_MEMORY;
}

static cairo_status_t 
_cairo_ft_font_text_to_glyphs (void			*abstract_font,
			       const unsigned char	*utf8,
			       cairo_glyph_t		**glyphs, 
			       int			*nglyphs)
{
    cairo_win32_font_t *font = abstract_font;
    cairo_status_t status;

    GCP_RESULTSW results;
    WCHAR glyphs[1024];
    int dx[1024];
    DWORD ret;

    status = 
    
    results.lStructSize = sizeof (GCP_RESULTS);
    results.lpOutString = NULL;
    results.lpOrder = NULL;
    results.lpDx = dx;
    results.lpCaretPos = NULL;
    results.lpClass = NULL;
    results.lpGlyphs = glyphs;
    results.nGlyphs = 1024;
    
    ret = GetCharacterPlacementW (hdc, strs[i], wcslen(strs[i]),
				  0,
				  &results, 
				  GCP_DIACRITIC | GCP_LIGATE | GCP_GLYPHSHAPE | GCP_REORDER);
    if (!ret)
	_print_gdi_error ("GetCharacterPlacement");

    

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t 
_cairo_win32_font_font_extents (void			*abstract_font,
				cairo_font_extents_t	*extents)
{
    cairo_win32_font_t *font = abstract_font;
    TEXTMETRIC metrics;
    HDC hdc;

    if (font->preserve_axes) {
	/* For 90-degree rotations (including 0), we get the metrics
	 * from the GDI in logical space, then convert back to font space
	 */
	hdc = _cairo_win32_font_acquire_scaled_dc (font);
	GetTextMetrics (hdc, &metrics);
	_cairo_win32_font_release_scaled_dc (font);

	extents->ascent = metrics.tmAscent / font->logical_scale;
	extents->descent = metrics.tmDescent / font->logical_scale;
	extents->height = (metrics.tmHeight + metrics.tmExternalLeading) / font->logical_scale;
	extents->max_x_advance = metrics.tmMaxCharWidth / font->logical_scale;
	extents->max_y_advance = 0;

    } else {
	/* For all other transformations, we use the design metrics
	 * of the font. The GDI results from GetTextMetrics() on a
	 * transformed font are inexplicably large and we want to
	 * avoid them.
	 */
	hdc = _cairo_win32_font_acquire_unscaled_dc (font);
	GetTextMetrics (hdc, &metrics);
	_cairo_win32_font_release_unscaled_dc (font);

	extents->ascent = (double)metrics.tmAscent / font->em_square;
	extents->descent = metrics.tmDescent * font->em_square;
	extents->height = (double)(metrics.tmHeight + mertrics.tmExternalLeading) / font->em_square;
	extents->max_x_advance = (double)(metrics.tmMaxCharWidth) / font->em_square;
	extents->max_y_advance = 0;
	
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t 
_cairo_win32_font_glyph_extents (void			*abstract_font,
				 cairo_glyph_t		*glyphs, 
				 int			 num_glyphs,
				 cairo_text_extents_t	*extents)
{
    cairo_win32_font_t *font = abstract_font;
    static const MAT2 matrix = { { 0, 1 }, { 0, 0 }, { 0, 0 }, { 0, 1 } };
    GLYPHMETRICS metrics;
    HDC hdc;

    /* We handle only the case num_glyphs == 1, glyphs[i].x == glyphs[0].y == 0.
     * This is all that the calling code triggers, and the backend interface
     * will eventually be changed to match
     */
    assert (num_glyphs == 1);

    if (font->preserve_axes) {
	/* If we aren't rotating / skewing the axes, then we get the metrics
	 * from the GDI in device space and convert to font space.
	 */
	hdc = _cairo_win32_font_acquire_scaled_dc (font);
	GetGlyphOutline(hdc, str[i], GGO_METRICS, &glyph_metrics, 0, NULL, &matrix);
	_cairo_win32_font_release_scaled_dc (font);

	if (font->swap_axes) {
	    extents->x_bearing = metrics.gmptGlyphOrigin.y / font->y_scale;
	    extents->y_bearing = metrics.gmptGlyphOrigin.x / font->x_scale;
	    extents->width = metrics.gmBlackBoxY / font->y_scale;
	    extents->height = metrics.gmBlackBoxX / font->x_scale;
	    extents->x_advance = metrics.gmCellIncY / font->x_scale;
	    extents->y_advance = metrics.gmCellIncX / font->y_scale;
	} else {
	    extents->x_bearing = metrics.gmptGlyphOrigin.x / font->x_scale;
	    extents->y_bearing = metrics.gmptGlyphOrigin.y / font->y_scale;
	    extents->width = metrics.gmBlackBoxX / font->x_scale;
	    extents->height = metrics.gmBlackBoxY / font->y_scale;
	    extents->x_advance = metrics.gmCellIncX / font->x_scale;
	    extents->y_advance = metrics.gmCellIncY / font->y_scale;
	}

	if (font->swap_x) {
	    extents->x_bearing = (- extents->x_bearing - extents->width);
	    extents->x_advance = - extents->x_advance;
	}

	if (font->swap_y) {
	    extents->y_bearing = (- extents->y_bearing - extents->height);
	    extents->y_advance = - extents->y_advance;
	}
	
    } else {
	/* For all other transformations, we use the design metrics
	 * of the font.
	 */
	hdc = _cairo_win32_font_acquire_unscaled_dc (font);
	GetGlyphOutline(hdc, str[i], GGO_METRICS, &glyph_metrics, 0, NULL, &matrix);
	_cairo_win32_font_release_unscaled_dc (font);

	extents->x_bearing = (double)metrics.gmptGlyphOrigin.x / font->em_square;
	extents->y_bearing = (double)metrics.gmptGlyphOrigin.y / font->em_square;
	extents->width = (double)metrics.gmBlackBoxX / font->em_square;
	extents->height = (double)metrics.gmBlackBoxY / font->em_square;
	extents->x_advance = (double)metrics.gmCellIncX / font->em_square;
	extents->y_advance = (double)metrics.gmCellIncY / font->em_square;
    }

    return CAIRO_STATUS_SUCCESS;
}


static cairo_status_t 
_cairo_ft_font_glyph_bbox (void		       		*abstract_font,
			   const cairo_glyph_t 		*glyphs,
			   int                 		 num_glyphs,
			   cairo_box_t         		*bbox)
{
    static const MAT2 matrix = { { 0, 1 }, { 0, 0 }, { 0, 0 }, { 0, 1 } };
    cairo_win32_font_t *font = abstract_font;
    int x1 = 0, x2 = 0, y1 = 0, y2 = 0;

    if (num_glyphs > 0) {
	HDC hdc = _cairo_win32_font_acquire_scaled_dc (font);
	int i;

	for (i = 0; i < num_glyphs; i++) {
	    int x = floor (0.5 + glyphs[i].x);
	    int y = floor (0.5 + glyphs[i].y);

	    GetGlyphOutline (hdc, glyphs[i], GGO_METRICS | GGO_GLYPH_INDEX,
			     &glyph_metrics, 0, NULL, &matrix);

	    if (i == 0 || x1 > x + metrics.gmptGlyphOrigin.x)
		x1 = x + metrics.gmptGlyphOrigin.x;
	    if (i == 0 || y1 > y + metrics.gmptGlyphOrigin.y)
		y1 = x + metrics.gmptGlyphOrigin.y;
	    if (i == 0 || x2 < x + metrics.gmptGlyphOrigin.x + metrics.gmBlackBoxX)
		x2 = x + metrics.gmptGlyphOrigin.x + metrics.gmBlackBoxX;
	    if (i == 0 || y2 < y + metrics.gmptGlyphOrigin.y + metrics.gmBlackBoxY)
		y2 = y + metrics.gmptGlyphOrigin.x + metrics.gmBlackBoxY;
	}
	
	_cairo_win32_font_release_scaled_dc (font);
    }

    bbox->p1.x = _cairo_fixed_from_int (x1);
    bbox->p1.y = _cairo_fixed_from_int (y1);
    bbox->p2.x = _cairo_fixed_from_int (x2);
    bbox->p2.y = _cairo_fixed_from_int (y2);
    
    return CAIRO_STATUS_SUCCESS;
}

#define FIXED_BUF_SIZE 1024

typedef struct {
    cairo_win32_font_t *font;
    HDC dc;
    
    cairo_array_t glyphs;
    cairo_array_t dx;
    
    int last_x;
    int last_y;
} cairo_glyph_state_t;

static void
_start_glyphs (cairo_glyph_state_t *state,
	       cairo_win32_font_t  *font,
	       HDC                  dc)
{
    state->dc = dc;
    state->font = font;

    _cairo_array_init (&state->glyphs, sizeof (WCHAR));
    _cairo_array_init (&state->dx, sizeof (int));
}

static void
_flush_glyphs (cairo_glyph_state_t *state)
{
    ExtTextOutW (state->dc,
		 state->start_x, state->last_y,
		 ETO_CLIPPED,
		 NULL,
		 (WCHAR *)state->glyphs.elements,
		 state->glyphs.num_elements,
		 (int *)state->dx.elements);
    
    _cairo_array_truncate (&state->glyphs, 0);
    _cairo_array_truncate (&state->dx, 0);
}

static void
_add_glyph (cairo_glyph_state_t *state,
	    unsigned long        index,
	    double               device_x,
	    double               device_y)
{
    double user_x = device_x;
    double user_y = device_y;
    WCHAR glyph_index = index;
    int logical_x, logical_y;

    cairo_matrix_transform_point (&state->font->device_to_logical, &user_x, &user_y);

    logical_x = DOUBLE_TO_LOGICAL (user_x);
    logical_y = DOUBLE_TO_LOGICAL (user_y);

    if (state->glyphs.num_elements > 0) {
	int dx;
	
	if (logical_y != state->last_y) {
	    _flush_glyphs (state);
	    state->start_x = logical_x;
	}
	
	dx = logical_x - state->last_x;
	_cairo_array_append (&state->dx, &logical_x, 1);
    } else {
	state->start_x = logical_x;
    }

    state->last_x = logical_x;
    state->last_y = logical_y;
    
    _cairo_array_append (&state->glyphs, &glyph_index, 1);
}

static void
_finish_glyphs (cairo_glyph_state_t *state,
		HDC                  dc)
{
    state->dc = dc;

    _flush_glyphs (state);

    _cairo_array_fini (&state->glyphs);
    _cairo_array_init (&state->dx);
}

static cairo_status_t
_draw_glyphs_on_surface (cairo_win32_surface_t *surface,
			 HBRUSH                 brush,
			 int                    x_offset,
			 int                    y_offset,
			 const cairo_glyph_t   *glyphs,
			 int                 	num_glyphs)
{
    cairo_glyph_state_t state;
    cairo_status status;
    int prev_mode;
    XFORM xform;
    XFORM prev_xform;
    BRUSH old_brush;

    old_brush = SelectObject (tmp_surface->dc, brush);
    if (!old_brush)
	return _print_gdi_error ();l

    prev_mode = GetGraphicsMode (surface->dc);
    SetGraphicsMode (surface->dc, GM_ADVANCED);
    
    GetWorldTransform (surface->dc, &prev_xform);
    
    xForm.eM11 = font->logical_to_device->m[0][0];
    xForm.eM21 = font->logical_to_device->m[1][0];
    xForm.eM12 = font->logical_to_device->m[0][1];
    xForm.eM22 = font->logical_to_device->m[1][1];
    xForm.eDx = matrix->m[2][0];
    xForm.eDy = matrix->m[2][1];

    SetWorldTransform (surface->dc, &xform);
  
    _start_glyphs (&glyph_state, tmp_surface->dc);

    for (i = 0; i < num_glyphs; i++) {
	status = _add_glyph (&glyph_state, glyphs[i].glyph, glyphs[i].x - x, glyphs[i].y - y);
	if (!CAIRO_OK (status))
	    goto FAIL;
    }

 FAIL:
    _finish_glyphs (&glyph_state);

    SetWorldTransform (surface>dc, &prev_xform);

    SetGraphicsMode (surface->dc, prev_mode);
    
    SelectObject (tmp_surface->dc, old_brush);
}
			 

static cairo_status_t 
_cairo_win32_font_show_glyphs (void		       *abstract_font,
			       cairo_operator_t    	operator,
			       cairo_pattern_t     	*pattern,
			       cairo_surface_t     	*surface,
			       int                 	source_x,
			       int                 	source_y,
			       int				dest_x,
			       int				dest_y,
			       unsigned int		width,
			       unsigned int		height,
			       const cairo_glyph_t 	*glyphs,
			       int                 	num_glyphs)
{
    cairo_win32_font_t *font = abstract_font;
    cairo_bbox_t bbox;
    cairo_win32_surface_t *win32_surface = (cairo_win32_surface_t *)surface;
    cairo_win32_surface_t *tmp_surface;
    int i;
    RECT r;

    if (_cairo_surface_is_win32 (surface) &&
	win32_surface->format == CAIRO_FORMAT_RGB24 &&
	operator == CAIRO_OPERATOR_OVER &&
	pattern->type == CAIRO_PATTERN_SOLID &&
	(pattern->color.alpha_short >> 8) == 255) {

	/* When compositing OVER on a GDI-understood surface, with a
	 * solid opaque color, we can just call ExtTextOut directly.
	 */
	COLORREF new_color;
	HBRUSH brush;
	
	new_color = RGB (pattern->color.red_short >> 8,
			 pattern->color.green_short >> 8,
			 pattern->color.blue_short >> 8);

	brush = CreateSolidBrush (new_color);
	if (!brush)
	    return _cairo_win32_print_gdi_error ("_cairo_win32_font_show_glyphs");
	
	_draw_glyphs_on_surface (win32_surface, brush, glyphs, num_glyphs, 0, 0);
	
	DeleteObject (new_brush);
    } else {
	
	/* Otherwise, we need to draw using software fallbacks. We create a mask 
	/* surface by drawing the the glyphs onto a DIB, white-on-black.
	 */
	tmp_surface = (cairo_win32_surface_t *)_cairo_win32_surface_create_dib (CAIRO_FORMAT_ARGB32, width, height);

	r.left = 0;
	r.top = 0;
	r.right = width;
	r.height = height;
	FillRect (hdc, &r, GetStockObject (BLACK_BRUSH));

	_draw_glyphs_on_surface (win32_surface, GetStockObject (WHITE_BRUSH),
				 glyphs, num_glyphs, x, y);

	if (font->quality == CLEARTYPE_QUALITY) {
	    /* For ClearType, we need a 4-channel mask. If we are compositing on
	     * a surface with alpha, we need to compute the alpha channel of
	     * the mask as the average of the other channels. But for a destination
	     * surface without alpha the alpha channel of the mask is ignored
	     */

	    if (win32_surface->format != CAIRO_FORMAT_ARGB24)
		_compute_argb32_mask_alpha (tmp_surface);
	    
	    mask_surface = tmp_surface;
	    
	} else {
	    mask_surface = _compute_a8_mask (tmp_surface);
	    cairo_suface_destroy (tmp_surface);
	}

	/* For operator == OVER, no-cleartype, a possible optimization here is to
	 * draw onto an intermediate ARGB32 surface and alpha-blend that with the
	 * destination
	 */
	status = _cairo_surface_composite (operator, pattern, 
					   &(mask_surface->base), 
					   surface,
					   source_x, source_y,
					   0, 0,
					   x, y,
					   width, height);
	
	cairo_surface_destroy (mask_surface);
    }
    

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
    cairo_font_scale_t sc;
    
    cairo_matrix_get_affine (scale,
			     &sc.matrix[0][0], &sc.matrix[0][1],
			     &sc.matrix[1][0], &sc.matrix[1][1],
			     NULL, NULL);

    return _win32_font_create (logfont, &sc);
}
