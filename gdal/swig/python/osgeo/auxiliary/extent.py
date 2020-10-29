#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# ******************************************************************************
#
#  Project:  GDAL
#  Purpose:  module for handling extents
#  Author:   Idan Miara <idan@miara.com>
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

import tempfile
import math

from osgeo import gdal

from osgeo.auxiliary.geo_rectangle import GeoRectangle, get_points_extent
from osgeo.auxiliary import base


def calc_dx_dy(extent, sample_count):
    # extent: GeoRectangle
    # sample_count: int
    (min_x, max_x, min_y, max_y) = extent.min_max
    w = max_x - min_x
    h = max_y - min_y
    pix_area = w * h / sample_count
    if pix_area <= 0 or w <= 0 or h <= 0:
        return 0, 0
    pix_len = math.sqrt(pix_area)
    return pix_len, pix_len


def translate_extent(extent, transform, sample_count=1000):
    # extent: GeoRectangle
    if transform is None:
        return extent
    maxf = float("inf")
    (out_min_x, out_max_x, out_min_y, out_max_y) = (maxf, -maxf, maxf, -maxf)

    dx, dy = calc_dx_dy(extent, sample_count)
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


def get_geotransform_and_size(ds):
    return ds.GetGeoTransform(), (ds.RasterXSize, ds.RasterYSize)


def get_points_extent_from_ds(ds):
    geo_transform, size = get_geotransform_and_size(ds)
    points_extent = get_points_extent(geo_transform, *size)
    return points_extent, geo_transform


def dist(p1x, p1y, p2x, p2y):
    return math.sqrt((p2y - p1y) ** 2 + (p2x - p1x) ** 2)


def transform_resolution_p(transform, dx, dy, px, py):
    p1x, p1y, _ = transform.TransformPoint(px, py + dx, 0)
    p2x, p2y, _ = transform.TransformPoint(px, py + dy, 0)
    return dist(p1x, p1y, p2x, p2y)


def transform_resolution_old(transform, input_res, extent):
    # extent: GeoRectangle
    (xmin, xmax, ymin, ymax) = extent.min_max
    [dx, dy] = input_res
    out_res_x = min(
        transform_resolution_p(transform, 0, dy, xmin, ymin),
        transform_resolution_p(transform, 0, -dy, xmin, ymax),
        transform_resolution_p(transform, 0, -dy, xmax, ymax),
        transform_resolution_p(transform, 0, dy, xmax, ymin),
    )
    if (ymin > 0) and (ymax < 0):
        out_res_x = min(
            out_res_x,
            transform_resolution_p(transform, 0, dy, xmin, 0),
            transform_resolution_p(transform, 0, dy, xmax, 0),
        )
    out_res_x = round_to_sig(out_res_x, -1)
    out_res = (out_res_x, -out_res_x)
    return out_res


def transform_resolution(transform, input_res, extent, equal_res_both_axis=True, only_y_axis=True, sample_count=1000):
    # extent: GeoRectangle

    dx, dy = calc_dx_dy(extent, sample_count)
    out_x = []
    out_y = []
    y = float(extent.min_y)
    while y <= extent.max_y:
        x = float(extent.min_x)
        while x < extent.max_x:
            out_y.append(transform_resolution_p(transform, 0, input_res[1], x, y))
            if not only_y_axis:
                out_x.append(transform_resolution_p(transform, input_res[0], 0, x, y))
            x += dx
        y += dy

    if only_y_axis:
        out_y.sort()
        out_x = out_y
    elif equal_res_both_axis:
        out_y.extend(out_x)
        out_y.sort()
        out_x = out_y
    else:
        out_x.sort()
        out_y.sort()
    out_r = [out_x, out_y]

    # choose the median resolution
    out_res = [round_to_sig(r[round(len(r) / 2)], -1) for r in out_r]
    out_res[1] = -out_res[1]
    return out_res


def round_to_sig(d, extra_digits=-5):
    if (d == 0) or math.isnan(d) or math.isinf(d):
        return 0
    if abs(d) > 1e-20:
        digits = int(math.floor(math.log10(abs(d) + 1e-20)))
    else:
        digits = int(math.floor(math.log10(abs(d))))
    digits = digits + extra_digits
    return round(d, -digits)


def get_vec_extent(lyr):
    result = None
    for feature in lyr:
        geom = feature.GetGeometryRef()
        envelope = geom.GetEnvelope()
        r = GeoRectangle.from_min_max(*envelope)
        if result is None:
            result = r
        else:
            result = result.union(r)
    return result


def open_ds(filename_or_ds):
    return gdal.Open(str(filename_or_ds)) if base.is_path_like(filename_or_ds) else filename_or_ds


def get_extent(filename_or_ds):
    # returns -> GeoRectangle
    ds = open_ds(filename_or_ds)
    gt, size = get_geotransform_and_size(ds)
    return GeoRectangle.from_geotransform_and_size(gt, size)


def calc_geo_offsets(src_gt, src_size, dst_gt, dst_size):
    src = GeoRectangle.from_geotransform_and_size_to_pix(src_gt, src_size)
    dst = GeoRectangle.from_geotransform_and_size_to_pix(dst_gt, dst_size)
    offset = (dst.x - src.x, dst.y - src.y)
    src_offset = (max(0, offset[0]), max(0, offset[1]))
    dst_offset = (max(0, -offset[0]), max(0, -offset[1]))
    return src_offset, dst_offset


def calc_geotransform_and_dimensions(geotransforms, dimensions, input_extent=None):
    # input_extent: GeoRectangle
    # extents differ, but pixel size and rotation are the same.
    # we'll make a union or an intersection
    if geotransforms is None or len(geotransforms) != len(dimensions):
        raise Exception('Error! GeoTransforms and Dimensions have different lengths!')
    if isinstance(input_extent, GeoRectangle):
        gt = geotransforms[0]
        out_extent = input_extent.align(gt)
    else:
        out_extent = None  # : GeoRectangle
        is_union = input_extent == 2
        for gt, size in zip(geotransforms, dimensions):
            extent = GeoRectangle.from_geotransform_and_size(gt, size)
            out_extent = extent if out_extent is None else \
                out_extent.union(extent) if is_union else out_extent.intersect(extent)

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


def make_temp_vrt(ds, extent):
    # extent: GeoRectangle
    options = gdal.BuildVRTOptions(outputBounds=(extent.min_x, extent.min_y, extent.max_x, extent.max_y))
    vrt_filename = tempfile.mktemp(suffix='.vrt')
    vrt_ds = gdal.BuildVRT(vrt_filename, ds, options=options)
    if vrt_ds is None:
        raise Exception("Error! cannot create vrt. Cannot proceed")
    return vrt_filename, vrt_ds


def make_temp_vrt_old(filename, ds, data_type, projection, bands_count, gt, dimensions, ref_gt, ref_dimensions):
    drv = gdal.GetDriverByName("VRT")
    dt = gdal.GetDataTypeByName(data_type)

    # vrt_filename = filename + ".vrt"
    vrt_filename = tempfile.mktemp(suffix='.vrt')
    vrt_ds = drv.Create(vrt_filename, ref_dimensions[0], ref_dimensions[1], bands_count, dt)
    vrt_ds.SetGeoTransform(ref_gt)
    vrt_ds.SetProjection(projection)

    src_offset, dst_offset = calc_geo_offsets(gt, dimensions, ref_gt, ref_dimensions)
    if src_offset is None:
        raise Exception("Error! The requested extent is empty. Cannot proceed")

    source_size = dimensions

    for j in range(1, bands_count + 1):
        band = vrt_ds.GetRasterBand(j)
        inBand = ds.GetRasterBand(j)
        myBlockSize = inBand.GetBlockSize()

        myOutNDV = inBand.GetNoDataValue()
        if myOutNDV is not None:
            band.SetNoDataValue(myOutNDV)
        ndv_out = '' if myOutNDV is None else '<NODATA>%i</NODATA>' % myOutNDV

        source_xml = '<SourceFilename relativeToVRT="1">%s</SourceFilename>' % filename + \
                     '<SourceBand>%i</SourceBand>' % j + \
                     '<SourceProperties RasterXSize="%i" RasterYSize="%i" DataType=%s BlockXSize="%i" BlockYSize="%i"/>' % \
                     (source_size[0], source_size[1], dt, myBlockSize[0], myBlockSize[1]) + \
                     '<SrcRect xOff="%i" yOff="%i" xSize="%i" ySize="%i"/>' % \
                     (src_offset[0], src_offset[1], source_size[0], source_size[1]) + \
                     '<DstRect xOff="%i" yOff="%i" xSize="%i" ySize="%i"/>' % \
                     (dst_offset[0], dst_offset[1], source_size[0], source_size[1]) + \
                     ndv_out
        source = '<ComplexSource>' + source_xml + '</ComplexSource>'
        band.SetMetadataItem("source_%i" % j, source, 'new_vrt_sources')
        band = None  # close band
    return vrt_filename, vrt_ds


# def GetExtent(gt,cols,rows):
#     ''' Return list of corner coordinates from a gt
#
#         @type gt:   C{tuple/list}
#         @param gt: gt
#         @type cols:   C{int}
#         @param cols: number of columns in the dataset
#         @type rows:   C{int}
#         @param rows: number of rows in the dataset
#         @rtype:    C{[float,...,float]}
#         @return:   coordinates of each corner
#     '''
#     ext=[]
#     xarr=[0, cols]
#     yarr=[0, rows]
#
#     for px in xarr:
#         for py in yarr:
#             x=gt[0]+(px*gt[1])+(py*gt[2])
#             y=gt[3]+(px*gt[4])+(py*gt[5])
#             ext.append([x, y])
#         yarr.reverse()
#     return ext

