/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2004 Stuart Parmenter
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
 * The Initial Developer of the Original Code is Stuart Parmenter.
 *
 * Contributor(s):
 *      Stuart Parmenter <pavlov@pavlov.net>
 */

extern "C" {
#include "cairoint.h"
}

#include <windows.h>

#include <gdiplus.h>
using namespace Gdiplus;

#if 0
#include <fontconfig/fontconfig.h>
#include <fontconfig/fcfreetype.h>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H
#include FT_IMAGE_H
#endif

typedef struct {
    cairo_font_t base;
    HDC hdc;
    HFONT hfont;
} cairo_win32_font_t;



static int
_utf8_to_glyphs (cairo_win32_font_t	*font,
		 const unsigned char	*utf8,
		 double			x0,
		 double			y0,
		 cairo_glyph_t		**glyphs,
		 size_t			*nglyphs)
{
    /* XXX implement me */
    *glyphs = NULL;
    *nglyphs = 0;

    return 0;
}

/* implement the platform-specific interface */

cairo_font_t *
cairo_win32_font_create (HFONT hfont)
{
    cairo_win32_font_t *f = (cairo_win32_font_t*)malloc(sizeof(cairo_win32_font_t));
    if (f == NULL)
        return NULL;

    f->hfont = hfont;

    _cairo_font_init (&f->base, &cairo_win32_font_backend);

    return (cairo_font_t *) f;
}

#if 0
FT_Face
cairo_win32_font_face (cairo_font_t *abstract_font)
{
    cairo_win32_font_t *font = (cairo_win32_font_t *) abstract_font;

    if (font == NULL)
        return NULL;

    return font->face;
}

FcPattern *
cairo_win32_font_pattern (cairo_font_t *abstract_font)
{
    cairo_win32_font_t *font = (cairo_win32_font_t *) abstract_font;

    if (font == NULL)
        return NULL;

    return font->pattern;
}
#endif

/* implement the backend interface */

static cairo_font_t *
_cairo_win32_font_create (const char           *family,
                          cairo_font_slant_t   slant, 
                          cairo_font_weight_t  weight)
{
    int fontHeight = 60;  // in Pixels in this case
    int fontWidth = 0;
    int italic = 0;
    int bold = FW_REGULAR;

    switch (slant) {
    case CAIRO_FONT_SLANT_ITALIC:
        italic = 1;
    case CAIRO_FONT_SLANT_OBLIQUE:
    case CAIRO_FONT_SLANT_NORMAL:
    default:
        break;
    }

    if (weight == CAIRO_FONT_WEIGHT_BOLD)
        bold = FW_BOLD;

    HFONT hfont = CreateFont(fontHeight,           // height of font
                             fontWidth,            // average character width
                             0,                    // angle of escapement
                             0,                    // base-line orientation angle
                             bold,                 // font weight
                             italic,               // italic attribute option
                             FALSE,                // underline attribute option
                             FALSE,                // strikeout attribute option
                             ANSI_CHARSET,         // character set identifier
                             OUT_DEFAULT_PRECIS,   // output precision
                             CLIP_DEFAULT_PRECIS,  // clipping precision
                             ANTIALIASED_QUALITY,  // output quality
                             FF_DONTCARE,          // pitch and family
                             family);              // typeface name

    return cairo_win32_font_create(hfont);
}

static cairo_font_t *
_cairo_win32_font_copy (void *abstract_font)
{
    cairo_win32_font_t *font_new = NULL;
    cairo_win32_font_t *font = (cairo_win32_font_t*)abstract_font;
  
    if (font->base.backend != &cairo_win32_font_backend)
        return NULL;

    font_new = (cairo_win32_font_t *) cairo_win32_font_create(font->hfont);
    if (font_new == NULL)
        return NULL;

    return (cairo_font_t *) font_new;
}

static void 
_cairo_win32_font_destroy (void *abstract_font)
{
    cairo_win32_font_t *font = (cairo_win32_font_t*)abstract_font;

    //delete font->font;

    free (font);
}

static cairo_status_t 
_cairo_win32_font_font_extents (void                    *abstract_font,
                                cairo_font_extents_t    *extents)
{
    cairo_win32_font_t *font = (cairo_win32_font_t*)abstract_font;

    TEXTMETRIC metrics;
    GetTextMetrics(font->hdc, &metrics);

    extents->ascent = metrics.tmAscent;
    extents->descent = metrics.tmDescent;
    extents->height = metrics.tmHeight;
    extents->max_x_advance = 0; /* XXX */
    extents->max_y_advance = 0; /* XXX */


#if 0
    FT_Face face = font->face;
    double scale_x, scale_y;

    double upm = face->units_per_EM;

    _cairo_matrix_compute_scale_factors (&font->base.matrix, &scale_x, &scale_y);

    extents->ascent =        face->ascender / upm * scale_y;
    extents->descent =       face->descender / upm * scale_y;
    extents->height =        face->height / upm * scale_y;
    extents->max_x_advance = face->max_advance_width / upm * scale_x;
    extents->max_y_advance = face->max_advance_height / upm * scale_y;
#endif
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t 
_cairo_win32_font_glyph_extents (void                   *abstract_font,
                                 cairo_glyph_t          *glyphs, 
                                 int                    num_glyphs,
                                 cairo_text_extents_t   *extents)
{
    cairo_win32_font_t *font = (cairo_win32_font_t*)abstract_font;

    int i;
    for (i = 0; i < num_glyphs; ++i) {
        GLYPHMETRICS metrics;
        GetGlyphOutline(font->hdc, 'a', GGO_METRICS, &metrics, 0, NULL, NULL);

        extents->width += metrics.gmBlackBoxX;
        extents->height += metrics.gmBlackBoxY;
        /* metrics has:
            UINT  gmBlackBoxX; 
            UINT  gmBlackBoxY; 
            POINT gmptGlyphOrigin; 
            short gmCellIncX; 
            short gmCellIncY; 

           extents has:
            double x_bearing;
            double y_bearing;
            double width;
            double height;
            double x_advance;
            double y_advance;
        */
    }

#if 0
    int i;
    cairo_win32_font_t *font = abstract_font;
    cairo_point_double_t origin;
    cairo_point_double_t glyph_min, glyph_max;
    cairo_point_double_t total_min, total_max;
    FT_Error error;
    FT_Face face = font->face;
    FT_GlyphSlot glyph = face->glyph;
    FT_Glyph_Metrics *metrics = &glyph->metrics;

    if (num_glyphs == 0)
    {
        extents->x_bearing = 0.0;
        extents->y_bearing = 0.0;
        extents->width  = 0.0;
        extents->height = 0.0;
        extents->x_advance = 0.0;
        extents->y_advance = 0.0;

        return CAIRO_STATUS_SUCCESS;
    }

    origin.x = glyphs[0].x;
    origin.y = glyphs[0].y;

    _install_font_matrix (&font->base.matrix, face);

    for (i = 0; i < num_glyphs; i++)
    {
        error = FT_Load_Glyph (face, glyphs[i].index, FT_LOAD_DEFAULT);
        /* XXX: What to do in this error case? */
        if (error)
            continue;

        /* XXX: Need to add code here to check the font's FcPattern
           for FC_VERTICAL_LAYOUT and if set get vertBearingX/Y
           instead. This will require that
           cairo_win32_font_create_for_ft_face accept an
           FcPattern. */
        glyph_min.x = glyphs[i].x + DOUBLE_FROM_26_6 (metrics->horiBearingX);
        glyph_min.y = glyphs[i].y - DOUBLE_FROM_26_6 (metrics->horiBearingY);
        glyph_max.x = glyph_min.x + DOUBLE_FROM_26_6 (metrics->width);
        glyph_max.y = glyph_min.y + DOUBLE_FROM_26_6 (metrics->height);
    
        if (i==0) {
            total_min = glyph_min;
            total_max = glyph_max;
        } else {
            if (glyph_min.x < total_min.x)
                total_min.x = glyph_min.x;
            if (glyph_min.y < total_min.y)
                total_min.y = glyph_min.y;

            if (glyph_max.x > total_max.x)
                total_max.x = glyph_max.x;
            if (glyph_max.y > total_max.y)
                total_max.y = glyph_max.y;
        }
    }

    extents->x_bearing = total_min.x - origin.x;
    extents->y_bearing = total_min.y - origin.y;
    extents->width     = total_max.x - total_min.x;
    extents->height    = total_max.y - total_min.y;
    extents->x_advance = glyphs[i-1].x + DOUBLE_FROM_26_6 (metrics->horiAdvance) - origin.x;
    extents->y_advance = glyphs[i-1].y + 0 - origin.y;
#endif
    return CAIRO_STATUS_SUCCESS;
}


static cairo_status_t 
_cairo_win32_font_text_extents (void                    *abstract_font,
                                const unsigned char     *utf8,
                                cairo_text_extents_t    *extents)
{
    cairo_win32_font_t *font = (cairo_win32_font_t*)abstract_font;

    cairo_glyph_t *glyphs;
    size_t nglyphs;
    cairo_status_t status = CAIRO_STATUS_SUCCESS;

    if (_utf8_to_glyphs (font, utf8, 0, 0, &glyphs, &nglyphs))
    {
        status = _cairo_win32_font_glyph_extents (font, glyphs, nglyphs, 
                                                  extents);      
        free (glyphs);
    }

    return status;
}

static cairo_status_t 
_cairo_win32_font_glyph_bbox (void                *abstract_font,
                              cairo_surface_t     *surface,
                              const cairo_glyph_t *glyphs,
                              int                 num_glyphs,
                              cairo_box_t         *bbox)
{
    cairo_win32_font_t *font = (cairo_win32_font_t*)abstract_font;
#if 0
    cairo_surface_t *mask = NULL;
    cairo_glyph_size_t size;

    cairo_fixed_t x1, y1, x2, y2;
    int i;

    bbox->p1.x = bbox->p1.y = CAIRO_MAXSHORT << 16;
    bbox->p2.x = bbox->p2.y = CAIRO_MINSHORT << 16;

    if (font == NULL
        || surface == NULL
        || glyphs == NULL)
        return CAIRO_STATUS_NO_MEMORY;

    for (i = 0; i < num_glyphs; i++)
    {
        mask = _cairo_font_lookup_glyph (&font->base, surface,
                                         &glyphs[i], &size);
        if (mask == NULL)
            continue;

        x1 = _cairo_fixed_from_double (glyphs[i].x + size.x);
        y1 = _cairo_fixed_from_double (glyphs[i].y - size.y);
        x2 = x1 + _cairo_fixed_from_double (size.width);
        y2 = y1 + _cairo_fixed_from_double (size.height);
        
        if (x1 < bbox->p1.x)
            bbox->p1.x = x1;
        
        if (y1 < bbox->p1.y)
            bbox->p1.y = y1;
        
        if (x2 > bbox->p2.x)
            bbox->p2.x = x2;
        
        if (y2 > bbox->p2.y)
            bbox->p2.y = y2;
        
        if (mask)
            cairo_surface_destroy (mask);
    }
#endif
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t 
_cairo_win32_font_text_bbox (void                     *abstract_font,
                          cairo_surface_t     *surface,
                          double              x0,
                          double              y0,
                          const unsigned char *utf8,
                          cairo_box_t *bbox)
{
    cairo_win32_font_t *font = (cairo_win32_font_t*)abstract_font;

    cairo_glyph_t *glyphs;
    size_t num_glyphs;
    
    if (_utf8_to_glyphs (font, utf8, x0, y0, &glyphs, &num_glyphs))
    {
        cairo_status_t res;
        res = _cairo_win32_font_glyph_bbox (font, surface,
                                         glyphs, num_glyphs, bbox);
        free (glyphs);
        return res;
    }
    else
        return CAIRO_STATUS_NO_MEMORY;
}

static cairo_status_t 
_cairo_win32_font_show_glyphs (void             *abstract_font,
                            cairo_operator_t    op,
                            cairo_surface_t     *source,
                            cairo_surface_t     *surface,
                            int                 source_x,
                            int                 source_y,
                            const cairo_glyph_t *glyphs,
                            int                 num_glyphs)
{
    cairo_win32_font_t *font = (cairo_win32_font_t*)abstract_font;
#if 0
    cairo_status_t status;
    cairo_surface_t *mask = NULL;
    cairo_glyph_size_t size;

    double x, y;
    int i;

    if (font == NULL 
        || source == NULL 
        || surface == NULL 
        || glyphs == NULL)
        return CAIRO_STATUS_NO_MEMORY;

    for (i = 0; i < num_glyphs; i++)
    {
        mask = _cairo_font_lookup_glyph (&font->base, surface,
                                         &glyphs[i], &size);
        if (mask == NULL)
            continue;
   
        x = glyphs[i].x;
        y = glyphs[i].y;

        status = _cairo_surface_composite (operator, source, mask, surface,
                                           source_x + x + size.x,
                                           source_y + y - size.y,
                                           0, 0, 
                                           x + size.x, 
                                           y - size.y, 
                                           (double) size.width,
                                           (double) size.height);
        
        cairo_surface_destroy (mask);

        if (status)
            return status;
    }  
#endif
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t 
_cairo_win32_font_show_text (void                     *abstract_font,
                          cairo_operator_t    op,
                          cairo_surface_t     *source,
                          cairo_surface_t     *surface,
                          int                 source_x,
                          int                 source_y,
                          double              x0,
                          double              y0,
                          const unsigned char *utf8)
{
    cairo_win32_font_t *font = (cairo_win32_font_t*)abstract_font;

    cairo_glyph_t *glyphs;
    size_t num_glyphs;
    
    if (_utf8_to_glyphs (font, utf8, x0, y0, &glyphs, &num_glyphs))
    {
        cairo_status_t res;
        res = _cairo_win32_font_show_glyphs (font, op, 
                                          source, surface,
                                          source_x, source_y,
                                          glyphs, num_glyphs);      
        free (glyphs);
        return res;
    }
    else
        return CAIRO_STATUS_NO_MEMORY;
}

static cairo_status_t 
_cairo_win32_font_glyph_path (void                      *abstract_font,
                           cairo_glyph_t        *glyphs, 
                           int                  num_glyphs,
                           cairo_path_t         *path)
{
#if 0
    int i;
    cairo_win32_font_t *font = abstract_font;
    FT_GlyphSlot glyph;
    FT_Error error;
    FT_Outline_Funcs outline_funcs = {
        _move_to,
        _line_to,
        _conic_to,
        _cubic_to,
        0, /* shift */
        0, /* delta */
    };

    glyph = font->face->glyph;
    _install_font_matrix (&font->base.matrix, font->face);

    for (i = 0; i < num_glyphs; i++)
    {
        FT_Matrix invert_y = {
            DOUBLE_TO_16_16 (1.0), 0,
            0, DOUBLE_TO_16_16 (-1.0),
        };

        error = FT_Load_Glyph (font->face, glyphs[i].index, FT_LOAD_DEFAULT);
        /* XXX: What to do in this error case? */
        if (error)
            continue;
        /* XXX: Do we want to support bitmap fonts here? */
        if (glyph->format == ft_glyph_format_bitmap)
            continue;

        /* Font glyphs have an inverted Y axis compared to cairo. */
        FT_Outline_Transform (&glyph->outline, &invert_y);
        FT_Outline_Translate (&glyph->outline,
                              DOUBLE_TO_26_6(glyphs[i].x),
                              DOUBLE_TO_26_6(glyphs[i].y));
        FT_Outline_Decompose (&glyph->outline, &outline_funcs, path);
    }
    _cairo_path_close_path (path);
#endif
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t 
_cairo_win32_font_text_path (void                     *abstract_font,
                          double              x,
                          double              y,
                          const unsigned char *utf8,
                          cairo_path_t        *path)
{
#if 0
    cairo_win32_font_t *font = abstract_font;
    cairo_glyph_t *glyphs;
    size_t nglyphs;
    
    if (_utf8_to_glyphs (font, utf8, x, y, &glyphs, &nglyphs))
    {
        cairo_status_t res;
        res = _cairo_win32_font_glyph_path (font, glyphs, nglyphs, path);
        free (glyphs);
        return res;
    }
    else
#endif
        return CAIRO_STATUS_NO_MEMORY;
}

static cairo_surface_t *
_cairo_win32_font_create_glyph (void *abstract_font,
                                const cairo_glyph_t *glyph,
                                cairo_glyph_size_t *return_size)
{
#if 0
    cairo_win32_font_t *font = abstract_font;
    cairo_image_surface_t *image;
    FT_GlyphSlot glyphslot;
    unsigned int width, height, stride;
    FT_Outline *outline;s
    FT_BBox cbox;
    FT_Bitmap bitmap;
    
    glyphslot = font->face->glyph;
    _install_font_matrix (&font->base.matrix, font->face);

    FT_Load_Glyph (font->face, glyph->index, FT_LOAD_DEFAULT);

    outline = &glyphslot->outline;
    
    FT_Outline_Get_CBox (outline, &cbox);

    cbox.xMin &= -64;
    cbox.yMin &= -64;
    cbox.xMax = (cbox.xMax + 63) & -64;
    cbox.yMax = (cbox.yMax + 63) & -64;

    width = (unsigned int) ((cbox.xMax - cbox.xMin) >> 6);
    height = (unsigned int) ((cbox.yMax - cbox.yMin) >> 6);
    stride = (width + 3) & -4;
    
    bitmap.pixel_mode = ft_pixel_mode_grays;
    bitmap.num_grays  = 256;
    bitmap.width = width;
    bitmap.rows = height;
    bitmap.pitch = stride;
    
    if (width * height == 0) 
        return NULL;
    
    bitmap.buffer = malloc (stride * height);
    if (bitmap.buffer == NULL)
        return NULL;
        
    memset (bitmap.buffer, 0x0, stride * height);

    FT_Outline_Translate (outline, -cbox.xMin, -cbox.yMin);
    FT_Outline_Get_Bitmap (glyphslot->library, outline, &bitmap);
    
    image = (cairo_image_surface_t *)
        cairo_image_surface_create_for_data ((char *) bitmap.buffer,
                                             CAIRO_FORMAT_A8,
                                             width, height, stride);
    if (image == NULL) {
        free (bitmap.buffer);
        return NULL;
    }

    _cairo_image_surface_assume_ownership_of_data (image);

    return_size->width = (unsigned short) width;
    return_size->height = (unsigned short) height;
    return_size->x = (short) (cbox.xMin >> 6);
    return_size->y = (short) (cbox.yMax >> 6);
    
    return &image->base;
#endif
    return NULL;
}

const struct cairo_font_backend cairo_win32_font_backend = {
    _cairo_win32_font_create,
    _cairo_win32_font_copy,
    _cairo_win32_font_destroy,
    _cairo_win32_font_font_extents,
    _cairo_win32_font_text_extents,
    _cairo_win32_font_glyph_extents,
    _cairo_win32_font_text_bbox,
    _cairo_win32_font_glyph_bbox,
    _cairo_win32_font_show_text,
    _cairo_win32_font_show_glyphs,
    _cairo_win32_font_text_path,
    _cairo_win32_font_glyph_path,
    _cairo_win32_font_create_glyph
};
