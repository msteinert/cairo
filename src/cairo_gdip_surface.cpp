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

/* export symbols, for cairo.dll only */

#pragma comment(linker, "/EXPORT:_cairo_create")
#pragma comment(linker, "/EXPORT:_cairo_reference")
#pragma comment(linker, "/EXPORT:_cairo_destroy")
#pragma comment(linker, "/EXPORT:_cairo_save")
#pragma comment(linker, "/EXPORT:_cairo_restore")
#pragma comment(linker, "/EXPORT:_cairo_copy")
#pragma comment(linker, "/EXPORT:_cairo_set_target_surface")
#pragma comment(linker, "/EXPORT:_cairo_set_target_image")

#ifdef CAIRO_HAS_PS_SURFACE
#pragma comment(linker, "/EXPORT:_cairo_set_target_ps")
#endif

#ifdef CAIRO_HAS_PS_SURFACE
#pragma comment(linker, "/EXPORT:_cairo_set_target_pdf")
#endif

#ifdef CAIRO_HAS_PNG_SURFACE
#pragma comment(linker, "/EXPORT:_cairo_set_target_png")
#endif

#ifdef CAIRO_HAS_GL_SURFACE
#pragma comment(linker, "/EXPORT:_cairo_set_target_gl")
#endif

#ifdef CAIRO_HAS_WIN32_SURFACE
#pragma comment(linker, "/EXPORT:_cairo_set_target_win32")
#endif

#pragma comment(linker, "/EXPORT:_cairo_set_operator")
#pragma comment(linker, "/EXPORT:_cairo_set_rgb_color")
#pragma comment(linker, "/EXPORT:_cairo_set_pattern")
#pragma comment(linker, "/EXPORT:_cairo_set_alpha")
#pragma comment(linker, "/EXPORT:_cairo_set_tolerance")
#pragma comment(linker, "/EXPORT:_cairo_set_fill_rule")
#pragma comment(linker, "/EXPORT:_cairo_set_line_width")
#pragma comment(linker, "/EXPORT:_cairo_set_line_cap")
#pragma comment(linker, "/EXPORT:_cairo_set_line_join")
#pragma comment(linker, "/EXPORT:_cairo_set_dash")
#pragma comment(linker, "/EXPORT:_cairo_set_miter_limit")
#pragma comment(linker, "/EXPORT:_cairo_set_line_cap")

#pragma comment(linker, "/EXPORT:_cairo_translate")
#pragma comment(linker, "/EXPORT:_cairo_scale")
#pragma comment(linker, "/EXPORT:_cairo_rotate")

#pragma comment(linker, "/EXPORT:_cairo_concat_matrix")
#pragma comment(linker, "/EXPORT:_cairo_set_matrix")
#pragma comment(linker, "/EXPORT:_cairo_default_matrix")
#pragma comment(linker, "/EXPORT:_cairo_identity_matrix")
#pragma comment(linker, "/EXPORT:_cairo_transform_point")
#pragma comment(linker, "/EXPORT:_cairo_transform_distance")
#pragma comment(linker, "/EXPORT:_cairo_inverse_transform_point")
#pragma comment(linker, "/EXPORT:_cairo_inverse_transform_distance")

#pragma comment(linker, "/EXPORT:_cairo_new_path")
#pragma comment(linker, "/EXPORT:_cairo_move_to")
#pragma comment(linker, "/EXPORT:_cairo_line_to")
#pragma comment(linker, "/EXPORT:_cairo_curve_to")
#pragma comment(linker, "/EXPORT:_cairo_arc")
#pragma comment(linker, "/EXPORT:_cairo_arc_negative")
#pragma comment(linker, "/EXPORT:_cairo_rel_move_to")
#pragma comment(linker, "/EXPORT:_cairo_rel_line_to")
#pragma comment(linker, "/EXPORT:_cairo_rel_curve_to")
#pragma comment(linker, "/EXPORT:_cairo_rectangle")
#pragma comment(linker, "/EXPORT:_cairo_close_path")

#pragma comment(linker, "/EXPORT:_cairo_stroke")
#pragma comment(linker, "/EXPORT:_cairo_fill")
#pragma comment(linker, "/EXPORT:_cairo_copy_page")
#pragma comment(linker, "/EXPORT:_cairo_show_page")
#pragma comment(linker, "/EXPORT:_cairo_in_stroke")
#pragma comment(linker, "/EXPORT:_cairo_in_fill")
#pragma comment(linker, "/EXPORT:_cairo_stroke_extents")
#pragma comment(linker, "/EXPORT:_cairo_fill_extents")

#pragma comment(linker, "/EXPORT:_cairo_init_clip")
#pragma comment(linker, "/EXPORT:_cairo_clip")
#pragma comment(linker, "/EXPORT:_cairo_select_font")
#pragma comment(linker, "/EXPORT:_cairo_scale_font")
#pragma comment(linker, "/EXPORT:_cairo_transform_font")
#pragma comment(linker, "/EXPORT:_cairo_show_text")
#pragma comment(linker, "/EXPORT:_cairo_show_glyphs")
#pragma comment(linker, "/EXPORT:_cairo_current_font")
#pragma comment(linker, "/EXPORT:_cairo_current_font_extents")
#pragma comment(linker, "/EXPORT:_cairo_set_font")
#pragma comment(linker, "/EXPORT:_cairo_text_extents")
#pragma comment(linker, "/EXPORT:_cairo_glyph_extents")
#pragma comment(linker, "/EXPORT:_cairo_text_path")
#pragma comment(linker, "/EXPORT:_cairo_glyph_path")
#pragma comment(linker, "/EXPORT:_cairo_font_reference")
#pragma comment(linker, "/EXPORT:_cairo_font_destroy")
#pragma comment(linker, "/EXPORT:_cairo_font_set_transform")
#pragma comment(linker, "/EXPORT:_cairo_font_current_transform")

/*#pragma comment(linker, "/EXPORT:_cairo_ft_font_create")
#pragma comment(linker, "/EXPORT:_cairo_ft_font_create_for_ft_face")
*/
#if 0
/* hmm, this function doesn't exist, but __cairo_ft_font_destroy does */
#pragma comment(linker, "/EXPORT:_cairo_ft_font_destroy")
#endif
/*
#pragma comment(linker, "/EXPORT:_cairo_ft_font_face")
#pragma comment(linker, "/EXPORT:_cairo_ft_font_pattern")
*/
#pragma comment(linker, "/EXPORT:_cairo_show_surface")
#pragma comment(linker, "/EXPORT:_cairo_current_operator")
#pragma comment(linker, "/EXPORT:_cairo_current_rgb_color")
#pragma comment(linker, "/EXPORT:_cairo_current_pattern")
#pragma comment(linker, "/EXPORT:_cairo_current_alpha")
#pragma comment(linker, "/EXPORT:_cairo_current_tolerance")
#pragma comment(linker, "/EXPORT:_cairo_current_point")
#pragma comment(linker, "/EXPORT:_cairo_current_fill_rule")
#pragma comment(linker, "/EXPORT:_cairo_current_line_width")
#pragma comment(linker, "/EXPORT:_cairo_current_line_cap")
#pragma comment(linker, "/EXPORT:_cairo_current_line_join")
#pragma comment(linker, "/EXPORT:_cairo_current_rgb_color")
#pragma comment(linker, "/EXPORT:_cairo_current_miter_limit")
#pragma comment(linker, "/EXPORT:_cairo_current_matrix")
#pragma comment(linker, "/EXPORT:_cairo_current_target_surface")
#pragma comment(linker, "/EXPORT:_cairo_current_path")
#pragma comment(linker, "/EXPORT:_cairo_current_path_flat")

#pragma comment(linker, "/EXPORT:_cairo_status")
#pragma comment(linker, "/EXPORT:_cairo_status_string")

#pragma comment(linker, "/EXPORT:_cairo_surface_create_for_image")
#pragma comment(linker, "/EXPORT:_cairo_surface_create_similar")
#pragma comment(linker, "/EXPORT:_cairo_surface_reference")
#pragma comment(linker, "/EXPORT:_cairo_surface_destroy")
#pragma comment(linker, "/EXPORT:_cairo_surface_set_repeat")
#pragma comment(linker, "/EXPORT:_cairo_surface_set_matrix")
#pragma comment(linker, "/EXPORT:_cairo_surface_get_matrix")
#pragma comment(linker, "/EXPORT:_cairo_surface_set_filter")
#pragma comment(linker, "/EXPORT:_cairo_surface_get_filter")

#pragma comment(linker, "/EXPORT:_cairo_image_surface_create")
#pragma comment(linker, "/EXPORT:_cairo_image_surface_create_for_data")
#pragma comment(linker, "/EXPORT:_cairo_pattern_create_for_surface")
#pragma comment(linker, "/EXPORT:_cairo_pattern_create_linear")
#pragma comment(linker, "/EXPORT:_cairo_pattern_create_radial")
#pragma comment(linker, "/EXPORT:_cairo_pattern_reference")
#pragma comment(linker, "/EXPORT:_cairo_pattern_destroy")
#pragma comment(linker, "/EXPORT:_cairo_pattern_add_color_stop")
#pragma comment(linker, "/EXPORT:_cairo_pattern_set_matrix")
#pragma comment(linker, "/EXPORT:_cairo_pattern_get_matrix")
#pragma comment(linker, "/EXPORT:_cairo_pattern_set_extend")
#pragma comment(linker, "/EXPORT:_cairo_pattern_get_extend")
#pragma comment(linker, "/EXPORT:_cairo_pattern_set_filter")
#pragma comment(linker, "/EXPORT:_cairo_pattern_get_filter")

#ifdef CAIRO_HAS_PS_SURFACE
#pragma comment(linker, "/EXPORT:_cairo_ps_surface_create")
#endif

#ifdef CAIRO_HAS_PNG_SURFACE
#pragma comment(linker, "/EXPORT:_cairo_png_surface_create")
#endif

#ifdef CAIRO_HAS_GL_SURFACE
#pragma comment(linker, "/EXPORT:_cairo_gl_surface_create")
#endif

#pragma comment(linker, "/EXPORT:_cairo_matrix_create")
#pragma comment(linker, "/EXPORT:_cairo_matrix_destroy")
#pragma comment(linker, "/EXPORT:_cairo_matrix_copy")
#pragma comment(linker, "/EXPORT:_cairo_matrix_set_identity")
#pragma comment(linker, "/EXPORT:_cairo_matrix_set_affine")
#pragma comment(linker, "/EXPORT:_cairo_matrix_get_affine")
#pragma comment(linker, "/EXPORT:_cairo_matrix_translate")
#pragma comment(linker, "/EXPORT:_cairo_matrix_scale")
#pragma comment(linker, "/EXPORT:_cairo_matrix_rotate")
#pragma comment(linker, "/EXPORT:_cairo_matrix_invert")
#pragma comment(linker, "/EXPORT:_cairo_matrix_multiply")
#pragma comment(linker, "/EXPORT:_cairo_matrix_transform_distance")
#pragma comment(linker, "/EXPORT:_cairo_matrix_transform_point")

#include <windows.h>

#include <gdiplus.h>
using namespace Gdiplus;

extern const cairo_surface_backend_t cairo_win32_surface_backend;

cairo_surface_t *_cairo_win32_surface_create (HDC dc);

void
cairo_set_target_win32(cairo_t *cr, HDC dc)
{
    cairo_surface_t *surface;

    surface = _cairo_win32_surface_create(dc);
    if (surface == NULL) {
        cr->status = CAIRO_STATUS_NO_MEMORY;
        return;
    }

    cairo_set_target_surface (cr, surface);

    /* cairo_set_target_surface takes a reference, so we must destroy ours */
    cairo_surface_destroy (surface);
}

typedef struct cairo_win32_surface {
    cairo_surface_t base;
    Graphics *gr;

    Brush *brush;
} cairo_win32_surface_t;


static void
_cairo_win32_surface_erase(cairo_win32_surface_t *surface);

cairo_surface_t *
_cairo_win32_surface_create(HDC dc)
{
    cairo_win32_surface_t *surface;

    surface = (cairo_win32_surface_t*)malloc(sizeof(cairo_win32_surface_t));
    if (surface == NULL)
        return NULL;

    surface->gr = new Graphics(dc);
    surface->brush = NULL;
//    surface->gr->TranslateTransform(-2000*2.5, -3000*2.2);
//    surface->gr->ScaleTransform(20, 20);

    surface->gr->SetSmoothingMode(SmoothingModeAntiAlias);

    /* do pixmap creation, etc */

    _cairo_surface_init(&surface->base, &cairo_win32_surface_backend);

    _cairo_win32_surface_erase(surface);

    return &surface->base;
}

static cairo_surface_t *
_cairo_win32_surface_create_similar(void          *abstract_src,
                                    cairo_format_t format,
                                    int            drawable,
                                    int            width,
                                    int            height)
{
    return NULL;
}

static void
_cairo_win32_surface_destroy (void *abstract_surface)
{
    cairo_win32_surface_t *surface = (cairo_win32_surface_t *)abstract_surface;

    delete surface->gr;
    delete surface->brush;

    free(surface);
}

static void
_cairo_win32_surface_erase(cairo_win32_surface_t *surface)
{
    surface->gr->Clear(Color(255, 0, 0, 0));
}

/* XXX: We should re-work this interface to return both X/Y ppi values. */
static double
_cairo_win32_surface_pixels_per_inch(void *abstract_surface)
{
    cairo_win32_surface_t *surface = (cairo_win32_surface_t *)abstract_surface;

    return surface->gr->GetDpiY();
}

static Image *
make_image(cairo_image_surface_t *image)
{
    Rect r(0, 0, image->width, image->height);
    Bitmap *b = new Bitmap(image->width, image->height, PixelFormat32bppPARGB);
    BitmapData data;

    b->LockBits(&r, ImageLockModeWrite, PixelFormat32bppPARGB, &data);

    memcpy(data.Scan0, image->data, image->stride * image->height);

    b->UnlockBits(&data);

    return b;
}

static cairo_image_surface_t *
_cairo_win32_surface_get_image(void *abstract_surface)
{
    cairo_win32_surface_t *surface = (cairo_win32_surface_t *)abstract_surface;

    /* need to figure out how to get the data from a Graphics and turn it in to an Image */
    return NULL;
}


static cairo_status_t
_cairo_win32_surface_set_image(void                  *abstract_surface,
                               cairo_image_surface_t *image)
{
    cairo_win32_surface_t *surface = (cairo_win32_surface_t *)abstract_surface;

    Image *img = make_image(image);
    surface->gr->DrawImage(img, 0, 0);
    delete img;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_win32_surface_set_matrix(void           *abstract_surface,
                                cairo_matrix_t *matrix)
{
    cairo_win32_surface_t *surface = (cairo_win32_surface_t *)abstract_surface;

    Matrix m((float)matrix->m[0][0], (float)matrix->m[1][0], (float)matrix->m[2][0],
             (float)matrix->m[0][1], (float)matrix->m[1][1], (float)matrix->m[2][1]);


    surface->gr->SetTransform(&m);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_win32_surface_set_filter (void           *abstract_surface,
                                 cairo_filter_t filter)
{
    cairo_win32_surface_t *surface = (cairo_win32_surface_t *)abstract_surface;
    SmoothingMode mode;

    switch (filter) {
        case CAIRO_FILTER_FAST:
            mode = SmoothingModeNone;
            break;
        case CAIRO_FILTER_GOOD:
        case CAIRO_FILTER_BEST:
        case CAIRO_FILTER_NEAREST:
        case CAIRO_FILTER_BILINEAR:
        case CAIRO_FILTER_GAUSSIAN:
        default:
            mode = SmoothingModeAntiAlias; 
            break;
    }
    surface->gr->SetSmoothingMode(mode);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_win32_surface_set_repeat (void           *abstract_surface,
                                 int             repeat)
{
    /* what is this function supposed to do? */
    cairo_win32_surface_t *surface = (cairo_win32_surface_t *)abstract_surface;

    return (cairo_status_t)CAIRO_INT_STATUS_UNSUPPORTED;
}

static cairo_int_status_t
_cairo_win32_surface_composite(cairo_operator_t   op,
                               cairo_surface_t    *generic_src,
                               cairo_surface_t    *generic_mask,
                               void               *abstract_dst,
                               int                src_x,
                               int                src_y,
                               int                mask_x,
                               int                mask_y,
                               int                dst_x,
                               int                dst_y,
                               unsigned int       width,
                               unsigned int       height)
{
    return CAIRO_INT_STATUS_UNSUPPORTED;
}


static cairo_int_status_t
_cairo_win32_surface_fill_rectangles(void                 *abstract_surface,
                                     cairo_operator_t     op,
                                     const cairo_color_t  *color,
                                     cairo_rectangle_t    *rects,
                                     int                  num_rects)
{
    cairo_win32_surface_t *surface = (cairo_win32_surface_t *)abstract_surface;

    SolidBrush brush(Color(color->alpha_short, color->red_short, color->green_short, color->blue_short));

    RectF *r = new RectF[num_rects]; /* should really allocate a small number here on the stack and use those if possible */
    for (int i = 0; i < num_rects; ++i) {
        r[i].X = rects[i].x;
        r[i].Y = rects[i].y;
        r[i].Width = rects[i].width;
        r[i].Height = rects[i].height;
    }

    surface->gr->FillRectangles(&brush, r, num_rects);

    delete[] r;

    return (cairo_int_status_t)CAIRO_STATUS_SUCCESS;
}



static int
_cairo_win32_extract_rectangle (cairo_trapezoid_t *trap,
                                RectF *rect)
{
    if (trap->left.p1.x == trap->left.p2.x &&
        trap->right.p1.x == trap->right.p2.x &&
        trap->left.p1.y == trap->right.p1.y &&
        trap->left.p2.y == trap->right.p2.y) {

        double x = _cairo_fixed_to_double (trap->left.p1.x);
        double y = _cairo_fixed_to_double (trap->left.p1.y);
        rect->X = (float)x;
        rect->Y = (float)y;
        rect->Width = (float)(_cairo_fixed_to_double(trap->right.p1.x) - x);
        rect->Height = (float)(_cairo_fixed_to_double(trap->left.p2.y) - y);

        return 1;
    }

    return 0;
}

static cairo_int_status_t
_cairo_win32_surface_composite_trapezoids (cairo_operator_t     op,
                                           cairo_surface_t      *generic_src,
                                           void                 *abstract_dst,
                                           int                  x_src,
                                           int                  y_src,
                                           cairo_trapezoid_t    *traps,
                                           int                  num_traps)
{
    cairo_win32_surface_t *surface = (cairo_win32_surface_t *)abstract_dst;

    static int zzz = 0;
    zzz += num_traps;

    Image *img = NULL;
    Brush *brush = NULL; /* i'd really like to not allocate this on the heap.. */

#define FIXED_TO_FLOAT(x) (float)_cairo_fixed_to_double((x))

    /* ugh.. figure out if we're a "native" pattern or an image_surface pattern.. */
    if (generic_src->backend == &cairo_win32_surface_backend) {
        brush = ((cairo_win32_surface_t*)generic_src)->brush;
        /* XXX move this outside so that TextureBrushes can take advantage of it */
	/* Check to see if we can represent these traps as a rectangle. */
	RectF rect;
        if (num_traps == 1 && _cairo_win32_extract_rectangle(traps, &rect)) {
            surface->gr->FillRectangle(brush, rect);
            return (cairo_int_status_t)CAIRO_STATUS_SUCCESS;
        }
    } else {
        /* I would really love to move this code in to create_pattern if possible */
        img = make_image((cairo_image_surface_t*)generic_src);
        brush = new TextureBrush(img);

        // XXX this will probably break.  Don't know why we have +1 and +2 offsets..
        float xoff = (FIXED_TO_FLOAT(traps[0].left.p1.x) + 0.5f);
        float yoff = (FIXED_TO_FLOAT(traps[0].left.p1.y) + 1.5f);
        static_cast<TextureBrush*>(brush)->TranslateTransform((int)(-x_src + xoff),
                                                              (int)(-y_src + yoff));
    }

    CompositingMode mode;
    switch (op) {
        case CAIRO_OPERATOR_OVER:
            mode = CompositingModeSourceOver;
            break;
        case CAIRO_OPERATOR_CLEAR:
            mode = CompositingModeSourceCopy;
            break;
        case CAIRO_OPERATOR_SRC:
        case CAIRO_OPERATOR_DST:
        case CAIRO_OPERATOR_OVER_REVERSE:
        case CAIRO_OPERATOR_IN:
        case CAIRO_OPERATOR_IN_REVERSE:
        case CAIRO_OPERATOR_OUT:
        case CAIRO_OPERATOR_OUT_REVERSE:
        case CAIRO_OPERATOR_ATOP:
        case CAIRO_OPERATOR_ATOP_REVERSE:
        case CAIRO_OPERATOR_XOR:
        case CAIRO_OPERATOR_ADD:
        case CAIRO_OPERATOR_SATURATE:
            mode = CompositingModeSourceOver;
            break;
    }
    surface->gr->SetCompositingMode(mode);

    PointF points[4];
    for (int i = 0; i < num_traps; ++i) {
        float top = FIXED_TO_FLOAT(traps[i].top);
        float bottom = FIXED_TO_FLOAT(traps[i].bottom);

        /* left line */
        points[0].X = FIXED_TO_FLOAT(traps[i].left.p1.x);
        points[0].Y = FIXED_TO_FLOAT(traps[i].left.p1.y);
        points[1].X = FIXED_TO_FLOAT(traps[i].left.p2.x);
        points[1].Y = FIXED_TO_FLOAT(traps[i].left.p2.y);

        /* right line */
        points[2].X = FIXED_TO_FLOAT(traps[i].right.p2.x);
        points[2].Y = FIXED_TO_FLOAT(traps[i].right.p2.y);
        points[3].X = FIXED_TO_FLOAT(traps[i].right.p1.x);
        points[3].Y = FIXED_TO_FLOAT(traps[i].right.p1.y);

        surface->gr->FillPolygon(brush, points, 4);
        //Pen p(Color(255,0,0,0), 1.0f/10.0f);
        //surface->gr->DrawPolygon(&p, points, 4);
    }

#undef FIXED_TO_FLOAT

    delete img;
    return (cairo_int_status_t)CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_win32_surface_copy_page (void *abstract_surface)
{
    return CAIRO_INT_STATUS_UNSUPPORTED;
}


static cairo_int_status_t
_cairo_win32_surface_show_page (void *abstract_surface)
{
    return CAIRO_INT_STATUS_UNSUPPORTED;
}


static cairo_int_status_t
_cairo_win32_surface_set_clip_region (void *abstract_surface,
                                      pixman_region16_t *region)
{
    cairo_win32_surface_t *surface = (cairo_win32_surface_t *)abstract_surface;

    pixman_box16_t *box;
    int n = pixman_region_num_rects(region);

    if (n == 0)
        return (cairo_int_status_t)CAIRO_STATUS_SUCCESS;

    box = pixman_region_rects(region);

    surface->gr->ResetClip();
    for (int i = 0; i < n; ++i) {
        Rect r(box[i].x1, box[i].y1, box[i].x2 - box[i].x1, box[i].y2 - box[i].y1);
        surface->gr->SetClip(r, CombineModeUnion); /* do I need to do replace once and then union? */
    }

    return (cairo_int_status_t)CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_win32_surface_create_pattern (void *abstract_surface,
                                     cairo_pattern_t *pattern,
                                     cairo_box_t *box)
{
    /* SOLID is easy -> SolidBrush
       LINEAR we can only do a gradient from 1 color to another -> LinearGradientBrush
              We should do this if thats all we need, otherwise use software.
       RADIAL we can't really do it.  Let it fall back to software I guess. */


    /* in the case of PATTERN_SOLID, we really just want to create a brush and hand that back... */
    if (pattern->type == CAIRO_PATTERN_SOLID) {

        /* ugh, this surface creation code should _really_ live somewhere else */
        cairo_win32_surface_t *src = (cairo_win32_surface_t*)malloc(sizeof(cairo_win32_surface_t));
        if (src == NULL)
            return CAIRO_INT_STATUS_UNSUPPORTED;

        _cairo_surface_init(&src->base, &cairo_win32_surface_backend);
        pattern->source = &src->base;

        src->gr = NULL;
        src->brush = new SolidBrush(Color(pattern->color.alpha_short,
                                          pattern->color.red_short,
                                          pattern->color.green_short,
                                          pattern->color.blue_short));
 

        return (cairo_int_status_t)CAIRO_STATUS_SUCCESS;
    }

    else if (pattern->type == CAIRO_PATTERN_LINEAR && pattern->n_stops == 2) {

        /* ugh, this surface creation code should _really_ live somewhere else */
        cairo_win32_surface_t *src = (cairo_win32_surface_t*)malloc(sizeof(cairo_win32_surface_t));
        if (src == NULL)
            return CAIRO_INT_STATUS_UNSUPPORTED;

        _cairo_surface_init(&src->base, &cairo_win32_surface_backend);
        pattern->source = &src->base;

        src->gr = NULL;

        RectF r((float)pattern->u.linear.point0.x,
                (float)pattern->u.linear.point0.y,
                ((box->p2.x + 65535) >> 16) - (box->p1.x >> 16),
                ((box->p2.y + 65535) >> 16) - (box->p1.y >> 16));

        src->brush = new LinearGradientBrush(r,
                                             Color(pattern->stops[0].color_char[3],
                                                   pattern->stops[0].color_char[0],
                                                   pattern->stops[0].color_char[1],
                                                   pattern->stops[0].color_char[2]),
                                             Color(pattern->stops[1].color_char[3],
                                                   pattern->stops[1].color_char[0],
                                                   pattern->stops[1].color_char[1],
                                                   pattern->stops[1].color_char[2]),
                                             90.0, FALSE);

        static_cast<LinearGradientBrush*>(src->brush)->TranslateTransform((float)pattern->source_offset.x,
                                                                          (float)pattern->source_offset.y);
        return (cairo_int_status_t)CAIRO_STATUS_SUCCESS;
    }

    else if (pattern->type == CAIRO_PATTERN_RADIAL) {
#if 0 /* XXX not sure this will work.. do we have to draw with FillPath() in order for this brush to work properly? */
        /* use PathGradientBrush here */

        /* ugh, this surface creation code should _really_ live somewhere else */
        cairo_win32_surface_t *src = (cairo_win32_surface_t*)malloc(sizeof(cairo_win32_surface_t));
        if (src == NULL)
            return CAIRO_INT_STATUS_UNSUPPORTED;

        _cairo_surface_init(&src->base, &cairo_win32_surface_backend);
        pattern->source = &src->base;

        src->gr = NULL;

	PointF center(pattern->u.radial.center1.x, pattern->u.radial.center1.y);

        GraphicsPath path;
        path.AddEllipse(0, 0, 140, 70);

        // Use the path to construct a brush.
        PathGradientBrush *brush = new PathGradientBrush(&path);
        src->brush = brush;

        // Set the color at the center of the path to blue.
        brush->SetCenterColor(Color(50, 100, 0, 255));

        // Set the color along the entire boundary of the path to aqua.
        Color colors[] = {Color(255, 0, 255, 255)};
        int count = 1;
        brush->SetSurroundColors(colors, &count);

        brush->SetCenterPoint(center);

        return (cairo_int_status_t)CAIRO_STATUS_SUCCESS;
#endif

    }
    return CAIRO_INT_STATUS_UNSUPPORTED;
}

static const cairo_surface_backend_t cairo_win32_surface_backend = {
    _cairo_win32_surface_create_similar,
    _cairo_win32_surface_destroy,
    _cairo_win32_surface_pixels_per_inch,
    _cairo_win32_surface_get_image,
    _cairo_win32_surface_set_image,
    _cairo_win32_surface_set_matrix,
    _cairo_win32_surface_set_filter,
    _cairo_win32_surface_set_repeat,
    _cairo_win32_surface_composite,
    _cairo_win32_surface_fill_rectangles,
    _cairo_win32_surface_composite_trapezoids,
    _cairo_win32_surface_copy_page,
    _cairo_win32_surface_show_page,
    _cairo_win32_surface_set_clip_region,
    _cairo_win32_surface_create_pattern,
};
