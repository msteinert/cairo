#!/usr/bin/perl -pi~
#
# Helper script to update source code from the
# libpixregion/libic functions, types, enums names to the libpixman names
#
# USAGE: perl update.pl FILES
#
# for example: perl fix.pl src/*.[ch]
#
# Dave Beckett 2003-12-10
#

# This order is significant.

# Firstly, do special cases
s/PixRegionBox/pixman_box16_t/g;
s/PixRegionPointInRegion/pixman_region_contains_point/g;
s/PixRegionRectIn/pixman_region_contains_rectangle/g;

# second, enum values
s/PixRegionStatusFailure/PIXMAN_REGION_STATUS_FAILURE/g;
s/PixRegionStatusSuccess/PIXMAN_REGION_STATUS_SUCCESS/g;
s/IcOperatorOverReverse/PIXMAN_OPERATOR_OVER_REVERSE/g;
s/IcOperatorAtopReverse/PIXMAN_OPERATOR_ATOP_REVERSE/g;
s/IcOperatorOutReverse/PIXMAN_OPERATOR_OUT_REVERSE/g;
s/IcOperatorInReverse/PIXMAN_OPERATOR_IN_REVERSE/g;
s/IcOperatorSaturate/PIXMAN_OPERATOR_SATURATE/g;
s/IcFormatNameARGB32/PIXMAN_FORMAT_NAME_AR_GB32/g;
s/IcFormatNameRGB24/PIXMAN_FORMAT_NAME_RG_B24/g;
s/IcFilterBilinear/PIXMAN_FILTER_BILINEAR/g;
s/IcOperatorClear/PIXMAN_OPERATOR_CLEAR/g;
s/IcFilterNearest/PIXMAN_FILTER_NEAREST/g;
s/IcOperatorOver/PIXMAN_OPERATOR_OVER/g;
s/IcOperatorAtop/PIXMAN_OPERATOR_ATOP/g;
s/IcFormatNameA8/PIXMAN_FORMAT_NAME_A8/g;
s/IcFormatNameA1/PIXMAN_FORMAT_NAME_A1/g;
s/IcOperatorSrc/PIXMAN_OPERATOR_SRC/g;
s/IcOperatorDst/PIXMAN_OPERATOR_DST/g;
s/IcOperatorOut/PIXMAN_OPERATOR_OUT/g;
s/IcOperatorXor/PIXMAN_OPERATOR_XOR/g;
s/IcOperatorAdd/PIXMAN_OPERATOR_ADD/g;
s/IcOperatorIn/PIXMAN_OPERATOR_IN/g;
s/IcFilterFast/PIXMAN_FILTER_FAST/g;
s/IcFilterGood/PIXMAN_FILTER_GOOD/g;
s/IcFilterBest/PIXMAN_FILTER_BEST/g;

# next enum types (shorter names than above and may prefix them)
s/IcBits/pixman_bits_t/g;
s/IcColor/pixman_color_t/g;
s/IcFixed16_16/pixman_fixed16_16_t/g;
s/IcFormat/pixman_format_t/g;
s/IcImage/pixman_image_t/g;
s/IcLineFixed/pixman_line_fixed_t/g;
s/IcPointFixed/pixman_point_fixed_t/g;
s/IcRectangle/pixman_rectangle_t/g;
s/IcTransform/pixman_transform_t/g;
s/IcTrapezoid/pixman_trapezoid_t/g;
s/IcTriangle/pixman_triangle_t/g;
s/IcVector/pixman_vector_t/g;
s/IcOperator/pixman_operator_t/g;
s/IcFilter/pixman_filter_t/g;
s/IcFormatName/pixman_format_name_t/g;
s/PixRegionStatus/pixman_region_status_t/g;

# functions (which also may have the type name)
s/PixRegionAppend/pixman_region_append/g;
s/PixRegionCopy/pixman_region_copy/g;
s/PixRegionCreate/pixman_region_create/g;
s/PixRegionCreateSimple/pixman_region_create_simple/g;
s/PixRegionDestroy/pixman_region_destroy/g;
s/PixRegionEmpty/pixman_region_empty/g;
s/PixRegionExtents/pixman_region_extents/g;
s/PixRegionIntersect/pixman_region_intersect/g;
s/PixRegionInverse/pixman_region_inverse/g;
s/PixRegionNotEmpty/pixman_region_not_empty/g;
s/PixRegionNumRects/pixman_region_num_rects/g;
s/PixRegionPointInRegion/pixman_region_point_in_region/g;
s/PixRegionRectIn/pixman_region_rect_in/g;
s/PixRegionRects/pixman_region_rects/g;
s/PixRegionReset/pixman_region_reset/g;
s/PixRegionSubtract/pixman_region_subtract/g;
s/PixRegionTranslate/pixman_region_translate/g;
s/PixRegionUnion/pixman_region_union/g;
s/PixRegionUnionRect/pixman_region_union_rect/g;
s/PixRegionValidate/pixman_region_validate/g;
s/IcColorToPixel/pixman_color_to_pixel/g;
s/IcComposite/pixman_composite/g;
s/IcCompositeTrapezoids/pixman_composite_trapezoids/g;
s/IcCompositeTriFan/pixman_composite_tri_fan/g;
s/IcCompositeTriStrip/pixman_composite_tri_strip/g;
s/IcCompositeTriangles/pixman_composite_triangles/g;
s/IcFillRectangle/pixman_fill_rectangle/g;
s/IcFillRectangles/pixman_fill_rectangles/g;
s/IcFormatCreate/pixman_format_create/g;
s/IcFormatCreateMasks/pixman_format_create_masks/g;
s/IcFormatDestroy/pixman_format_destroy/g;
s/IcImageCreate/pixman_image_create/g;
s/IcImageCreateForData/pixman_image_create_for_data/g;
s/IcImageDestroy/pixman_image_destroy/g;
s/IcImageGetData/pixman_image_get_data/g;
s/IcImageGetDepth/pixman_image_get_depth/g;
s/IcImageGetHeight/pixman_image_get_height/g;
s/IcImageGetStride/pixman_image_get_stride/g;
s/IcImageGetWidth/pixman_image_get_width/g;
s/IcImageSetClipRegion/pixman_image_set_clip_region/g;
s/IcImageSetFilter/pixman_image_set_filter/g;
s/IcImageSetRepeat/pixman_image_set_repeat/g;
s/IcImageSetTransform/pixman_image_set_transform/g;
s/IcPixelToColor/pixman_pixel_to_color/g;
# finally the type that is the most generic
s/PixRegion/pixman_region16_t/g;
