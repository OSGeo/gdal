#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# ******************************************************************************
#
#  Project:  GDAL
#  Purpose:  util support modules for extent calculations
#  Author:   Idan Miara, <idan@miara.com>
#
# ******************************************************************************
#  Copyright (c) 2020, Idan Miara <idan@miara.com>
#
#  Permission is hereby granted, free of charge, to any person obtaining a
#  copy of this software and associated documentation files (the "Software"),
#  to deal in the Software without restriction, including without limitation
#  the rights to use, copy, modify, merge, publish, distribute, sublicense,
#  and/or sell copies of the Software, and to permit persons to whom the
#  Software is furnished to do so, subject to the following conditions:
#
#  The above copyright notice and this permission notice shall be included
#  in all copies or substantial portions of the Software.
#
#  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
#  OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
#  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
#  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
#  DEALINGS IN THE SOFTWARE.
# ******************************************************************************
import math
import tempfile
from numbers import Number
from enum import Enum
from typing import Optional, Union, Sequence, Dict, Tuple

from osgeo import gdal, osr

from osgeo_utils.auxiliary.rectangle import GeoRectangle
from osgeo_utils.auxiliary.base import GeoTransform


class Extent(Enum):
    IGNORE = 0
    FAIL = 1
    UNION = 2
    INTERSECT = 3


def parse_extent(extent: Union[str, Extent]) -> Extent:
    if isinstance(extent, str):
        return Extent[extent.upper()]
    elif isinstance(extent, Extent):
        return extent
    raise Exception('Error: Unknown extent %s' % extent)


class GT(Enum):
    SAME = -1
    ALMOST_SAME = -2
    COMPATIBLE_DIFF = -3
    INCOMPATIBLE_OFFSET = 0
    INCOMPATIBLE_PIXEL_SIZE = 1
    INCOMPATIBLE_ROTATION = 2
    NON_ZERO_ROTATION = 3


def gt_diff(gt0: GeoTransform, gt1: GeoTransform, diff_support: Dict[GT, bool], eps: Union[Number, Dict[GT, Number]]=0.0):
    if gt0 == gt1:
        return GT.SAME
    if isinstance(eps, Number):
        eps = {GT.INCOMPATIBLE_OFFSET: eps, GT.INCOMPATIBLE_PIXEL_SIZE: eps, GT.INCOMPATIBLE_ROTATION: eps}
    same = {
        GT.INCOMPATIBLE_OFFSET:     eps[GT.INCOMPATIBLE_OFFSET]     >= (abs(gt0[0] - gt1[0]) + abs(gt0[3] - gt1[3])),
        GT.INCOMPATIBLE_PIXEL_SIZE: eps[GT.INCOMPATIBLE_PIXEL_SIZE] >= (abs(gt0[1] - gt1[1]) + abs(gt0[5] - gt1[5])),
        GT.INCOMPATIBLE_ROTATION:   eps[GT.INCOMPATIBLE_ROTATION]   >= (abs(gt0[2] - gt1[2]) + abs(gt0[4] - gt1[4])),
        GT.NON_ZERO_ROTATION:       gt0[2] == gt0[4] == gt1[2] == gt1[4] == 0}
    if same[GT.INCOMPATIBLE_OFFSET] and same[GT.INCOMPATIBLE_PIXEL_SIZE] and same[GT.INCOMPATIBLE_ROTATION]:
        return GT.ALMOST_SAME
    for reason in same.keys():
        if not same[reason] and not diff_support[reason]:
            return reason  # incompatible gt, returns the reason
    return GT.COMPATIBLE_DIFF


def calc_geotransform_and_dimensions(geotransforms: Sequence[GeoTransform], dimensions, input_extent: Union[GeoRectangle, Extent] = None):
    # extents differ, but pixel size and rotation are the same.
    # we'll make a union or an intersection
    if geotransforms is None or len(geotransforms) != len(dimensions):
        raise Exception('Error! GeoTransforms and Dimensions have different lengths!')

    if isinstance(input_extent, GeoRectangle):
        gt = geotransforms[0]
        out_extent = input_extent.align(gt)
    elif isinstance(input_extent, Extent):
        out_extent: Optional[GeoRectangle] = None
        is_union = input_extent == Extent.UNION
        for gt, size in zip(geotransforms, dimensions):
            extent = GeoRectangle.from_geotransform_and_size(gt, size)
            out_extent = extent if out_extent is None else \
                out_extent.union(extent) if is_union else out_extent.intersect(extent)
    else:
        raise Exception(f'Unknown input extent format {input_extent}')

    if out_extent is None or out_extent.is_empty():
        return None, None, None
    else:
        pixel_size = (gt[1], gt[5])
        pix_extent = out_extent.to_pixels(pixel_size)
        gt = (out_extent.left,
              gt[1], gt[2],
              out_extent.up,
              gt[4], gt[5])
    return gt, (math.ceil(pix_extent.w), math.ceil(pix_extent.h)), out_extent


def make_temp_vrt(ds, extent: GeoRectangle):
    options = gdal.BuildVRTOptions(outputBounds=(extent.min_x, extent.min_y, extent.max_x, extent.max_y))
    vrt_filename = tempfile.mktemp(suffix='.vrt')
    vrt_ds = gdal.BuildVRT(vrt_filename, ds, options=options)
    if vrt_ds is None:
        raise Exception("Error! cannot create vrt. Cannot proceed")
    return vrt_filename, vrt_ds


def get_geotransform_and_size(ds: gdal.Dataset) -> Tuple[GeoTransform, Tuple[int, int]]:
    return ds.GetGeoTransform(), (ds.RasterXSize, ds.RasterYSize)


def calc_dx_dy_from_extent_and_count(extent: GeoRectangle, sample_count: int) -> Tuple[float, float]:
    (min_x, max_x, min_y, max_y) = extent.min_max
    w = max_x - min_x
    h = max_y - min_y
    pix_area = w * h / sample_count
    if pix_area <= 0 or w <= 0 or h <= 0:
        return 0, 0
    pix_len = math.sqrt(pix_area)
    return pix_len, pix_len


def transform_extent(extent: GeoRectangle,
                     transform: osr.CoordinateTransformation, sample_count: int = 1000) -> GeoRectangle:
    """ returns a transformed extent by transforming sample_count points along a given extent """
    if transform is None:
        return extent
    maxf = float("inf")
    (out_min_x, out_max_x, out_min_y, out_max_y) = (maxf, -maxf, maxf, -maxf)

    dx, dy = calc_dx_dy_from_extent_and_count(extent, sample_count)
    if dx == 0:
        return GeoRectangle.empty()

    y = float(extent.min_y)
    while y <= extent.max_y + dy:
        x = float(extent.min_x)
        while x <= extent.max_x + dx:
            tx, ty, tz = transform.TransformPoint(x, y)
            x += dx
            if not math.isfinite(tz):
                continue
            out_min_x = min(out_min_x, tx)
            out_max_x = max(out_max_x, tx)
            out_min_y = min(out_min_y, ty)
            out_max_y = max(out_max_y, ty)
        y += dy

    return GeoRectangle.from_min_max(out_min_x, out_max_x, out_min_y, out_max_y)


