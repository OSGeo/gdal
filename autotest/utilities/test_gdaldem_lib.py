#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdaldem testing
# Author:   Even Rouault <even dot rouault @ mines-paris dot org>
#
###############################################################################
# Copyright (c) 2015, Even Rouault <even dot rouault at mines-paris dot org>
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.
###############################################################################

import struct
import sys

sys.path.append( '../pymod' )

from osgeo import gdal
from osgeo import osr
import gdaltest

###############################################################################
# Test gdaldem hillshade

def test_gdaldem_lib_hillshade():

    src_ds = gdal.Open('../gdrivers/data/n43.dt0')
    ds = gdal.DEMProcessing('', src_ds, 'hillshade', format = 'MEM', scale = 111120, zFactor = 30)
    if ds is None:
        return 'fail'

    cs = ds.GetRasterBand(1).Checksum()
    if cs != 45587:
        gdaltest.post_reason('Bad checksum')
        print(cs)
        return 'fail'

    src_gt = src_ds.GetGeoTransform()
    dst_gt = ds.GetGeoTransform()
    for i in range(6):
        if abs(src_gt[i] - dst_gt[i]) > 1e-10:
            gdaltest.post_reason('Bad geotransform')
            return 'fail'

    dst_wkt = ds.GetProjectionRef()
    if dst_wkt.find('AUTHORITY["EPSG","4326"]') == -1:
        gdaltest.post_reason('Bad projection')
        return 'fail'

    if ds.GetRasterBand(1).GetNoDataValue() != 0:
        gdaltest.post_reason('Bad nodata value')
        return 'fail'

    src_ds = None
    ds = None

    return 'success'

###############################################################################
# Test gdaldem hillshade with source being floating point

def test_gdaldem_lib_hillshade_float():

    src_ds = gdal.Translate('', gdal.Open('../gdrivers/data/n43.dt0'), format = 'MEM', outputType = gdal.GDT_Float32)
    ds = gdal.DEMProcessing('', src_ds, 'hillshade', format = 'MEM', scale = 111120, zFactor = 30)
    if ds is None:
        return 'fail'

    cs = ds.GetRasterBand(1).Checksum()
    if cs != 45587:
        gdaltest.post_reason('Bad checksum')
        print(cs)
        return 'fail'

    src_gt = src_ds.GetGeoTransform()
    dst_gt = ds.GetGeoTransform()
    for i in range(6):
        if abs(src_gt[i] - dst_gt[i]) > 1e-10:
            gdaltest.post_reason('Bad geotransform')
            return 'fail'

    dst_wkt = ds.GetProjectionRef()
    if dst_wkt.find('AUTHORITY["EPSG","4326"]') == -1:
        gdaltest.post_reason('Bad projection')
        return 'fail'

    if ds.GetRasterBand(1).GetNoDataValue() != 0:
        gdaltest.post_reason('Bad nodata value')
        return 'fail'

    src_ds = None
    ds = None

    return 'success'

###############################################################################
# Test gdaldem hillshade with source being floating point

def test_gdaldem_lib_hillshade_float_png():

    src_ds = gdal.Translate('', gdal.Open('../gdrivers/data/n43.dt0'), format = 'MEM', outputType = gdal.GDT_Float32)
    ds = gdal.DEMProcessing('/vsimem/test_gdaldem_lib_hillshade_float_png.png', src_ds, 'hillshade', format = 'PNG', scale = 111120, zFactor = 30)
    if ds is None:
        return 'fail'

    cs = ds.GetRasterBand(1).Checksum()
    if cs != 45587:
        gdaltest.post_reason('Bad checksum')
        print(cs)
        return 'fail'

    src_gt = src_ds.GetGeoTransform()
    dst_gt = ds.GetGeoTransform()
    for i in range(6):
        if abs(src_gt[i] - dst_gt[i]) > 1e-10:
            gdaltest.post_reason('Bad geotransform')
            return 'fail'

    dst_wkt = ds.GetProjectionRef()
    if dst_wkt.find('AUTHORITY["EPSG","4326"]') == -1:
        gdaltest.post_reason('Bad projection')
        return 'fail'

    if ds.GetRasterBand(1).GetNoDataValue() != 0:
        gdaltest.post_reason('Bad nodata value')
        return 'fail'

    src_ds = None
    ds = None

    gdal.GetDriverByName('PNG').Delete('/vsimem/test_gdaldem_lib_hillshade_float_png.png')

    return 'success'

###############################################################################
# Test gdaldem hillshade -combined

def test_gdaldem_lib_hillshade_combined():

    src_ds = gdal.Open('../gdrivers/data/n43.dt0')
    ds = gdal.DEMProcessing('', src_ds, 'hillshade', format = 'MEM', combined = True, scale = 111120, zFactor = 30)
    if ds is None:
        return 'fail'

    cs = ds.GetRasterBand(1).Checksum()
    if cs != 43876:
        gdaltest.post_reason('Bad checksum')
        print(cs)
        return 'fail'

    src_gt = src_ds.GetGeoTransform()
    dst_gt = ds.GetGeoTransform()
    for i in range(6):
        if abs(src_gt[i] - dst_gt[i]) > 1e-10:
            gdaltest.post_reason('Bad geotransform')
            return 'fail'

    dst_wkt = ds.GetProjectionRef()
    if dst_wkt.find('AUTHORITY["EPSG","4326"]') == -1:
        gdaltest.post_reason('Bad projection')
        return 'fail'

    if ds.GetRasterBand(1).GetNoDataValue() != 0:
        gdaltest.post_reason('Bad nodata value')
        return 'fail'

    src_ds = None
    ds = None

    return 'success'

###############################################################################
# Test gdaldem hillshade -alg ZevenbergenThorne

def test_gdaldem_lib_hillshade_ZevenbergenThorne():

    src_ds = gdal.Open('../gdrivers/data/n43.dt0')
    ds = gdal.DEMProcessing('', src_ds, 'hillshade', format = 'MEM', alg = 'ZevenbergenThorne', scale = 111120, zFactor = 30)
    if ds is None:
        return 'fail'

    cs = ds.GetRasterBand(1).Checksum()
    if cs != 46544:
        gdaltest.post_reason('Bad checksum')
        print(cs)
        return 'fail'

    src_gt = src_ds.GetGeoTransform()
    dst_gt = ds.GetGeoTransform()
    for i in range(6):
        if abs(src_gt[i] - dst_gt[i]) > 1e-10:
            gdaltest.post_reason('Bad geotransform')
            return 'fail'

    dst_wkt = ds.GetProjectionRef()
    if dst_wkt.find('AUTHORITY["EPSG","4326"]') == -1:
        gdaltest.post_reason('Bad projection')
        return 'fail'

    if ds.GetRasterBand(1).GetNoDataValue() != 0:
        gdaltest.post_reason('Bad nodata value')
        return 'fail'

    src_ds = None
    ds = None

    return 'success'

###############################################################################
# Test gdaldem hillshade -alg ZevenbergenThorne -combined

def test_gdaldem_lib_hillshade_ZevenbergenThorne_combined():

    src_ds = gdal.Open('../gdrivers/data/n43.dt0')
    ds = gdal.DEMProcessing('', src_ds, 'hillshade', format = 'MEM', alg = 'ZevenbergenThorne', combined = True, scale = 111120, zFactor = 30)
    if ds is None:
        return 'fail'

    cs = ds.GetRasterBand(1).Checksum()
    if cs != 43112:
        gdaltest.post_reason('Bad checksum')
        print(cs)
        return 'fail'

    src_gt = src_ds.GetGeoTransform()
    dst_gt = ds.GetGeoTransform()
    for i in range(6):
        if abs(src_gt[i] - dst_gt[i]) > 1e-10:
            gdaltest.post_reason('Bad geotransform')
            return 'fail'

    dst_wkt = ds.GetProjectionRef()
    if dst_wkt.find('AUTHORITY["EPSG","4326"]') == -1:
        gdaltest.post_reason('Bad projection')
        return 'fail'

    if ds.GetRasterBand(1).GetNoDataValue() != 0:
        gdaltest.post_reason('Bad nodata value')
        return 'fail'

    src_ds = None
    ds = None

    return 'success'

###############################################################################
# Test gdaldem hillshade with -compute_edges

def test_gdaldem_lib_hillshade_compute_edges():

    src_ds = gdal.Open('../gdrivers/data/n43.dt0')
    ds = gdal.DEMProcessing('', src_ds, 'hillshade', format = 'MEM', computeEdges = True, scale = 111120, zFactor = 30)
    if ds is None:
        return 'fail'

    cs = ds.GetRasterBand(1).Checksum()
    if cs != 50239:
        gdaltest.post_reason('Bad checksum')
        print(cs)
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test gdaldem hillshade with -compute_edges with floating point

def test_gdaldem_lib_hillshade_compute_edges_float():

    src_ds = gdal.Translate('', gdal.Open('../gdrivers/data/n43.dt0'), format = 'MEM', outputType = gdal.GDT_Float32)
    ds = gdal.DEMProcessing('', src_ds, 'hillshade', format = 'MEM', computeEdges = True, scale = 111120, zFactor = 30)
    if ds is None:
        return 'fail'

    cs = ds.GetRasterBand(1).Checksum()
    if cs != 50239:
        gdaltest.post_reason('Bad checksum')
        print(cs)
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test gdaldem hillshade with -az parameter

def test_gdaldem_lib_hillshade_azimuth():

    from sys import version_info
    src_ds = gdal.GetDriverByName('MEM').Create('', 100, 100, 1)
    src_ds.SetGeoTransform([2,0.01,0,49,0,-0.01])
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(4326)
    src_ds.SetProjection(sr.ExportToWkt())
    for j in range(100):
        data = ''
        for i in range(100):
            val = 255 - 5 * max(abs(50-i),abs(50-j))
            data = data + ('%c' % (val))
        if version_info >= (3,0,0):
            data = bytes(data, 'ISO-8859-1')
        src_ds.GetRasterBand(1).WriteRaster(0,j,100,1,data)

    # Light from the east
    ds = gdal.DEMProcessing('', src_ds, 'hillshade', format = 'MEM', azimuth = 90, scale = 111120, zFactor = 100)
    if ds is None:
        return 'fail'
    ds_ref = gdal.Open('data/pyramid_shaded_ref.tif')
    if gdaltest.compare_ds(ds, ds_ref, verbose = 1) > 1:
        gdaltest.post_reason('Bad checksum')
        return 'fail'
    ds = None
    ds_ref = None

    return 'success'

###############################################################################
# Test gdaldem hillshade -multidirectional

def test_gdaldem_lib_hillshade_multidirectional():

    src_ds = gdal.Open('../gdrivers/data/n43.dt0')
    ds = gdal.DEMProcessing('', src_ds, 'hillshade', format = 'MEM', multiDirectional = True, computeEdges = True, scale = 111120, zFactor = 30)
    if ds is None:
        return 'fail'

    cs = ds.GetRasterBand(1).Checksum()
    if cs != 51784:
        gdaltest.post_reason('Bad checksum')
        print(cs)
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test gdaldem hillshade -multidirectional

def test_gdaldem_lib_hillshade_multidirectional_ZevenbergenThorne():

    src_ds = gdal.Open('../gdrivers/data/n43.dt0')
    ds = gdal.DEMProcessing('', src_ds, 'hillshade', format = 'MEM', alg = 'ZevenbergenThorne', multiDirectional = True, computeEdges = True, scale = 111120, zFactor = 30)
    if ds is None:
        return 'fail'

    cs = ds.GetRasterBand(1).Checksum()
    if cs != 50860:
        gdaltest.post_reason('Bad checksum')
        print(cs)
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test gdaldem color relief

def test_gdaldem_lib_color_relief():

    src_ds = gdal.Open('../gdrivers/data/n43.dt0')
    ds = gdal.DEMProcessing('', src_ds, 'color-relief', format = 'MEM', colorFilename = 'data/color_file.txt')
    if ds is None:
        return 'fail'

    if ds.GetRasterBand(1).Checksum() != 55009:
        print(ds.GetRasterBand(1).Checksum())
        gdaltest.post_reason('Bad checksum')
        return 'fail'

    if ds.GetRasterBand(2).Checksum() != 37543:
        print(ds.GetRasterBand(2).Checksum())
        gdaltest.post_reason('Bad checksum')
        return 'fail'

    if ds.GetRasterBand(3).Checksum() != 47711:
        print(ds.GetRasterBand(3).Checksum())
        gdaltest.post_reason('Bad checksum')
        return 'fail'

    src_gt = src_ds.GetGeoTransform()
    dst_gt = ds.GetGeoTransform()
    for i in range(6):
        if abs(src_gt[i] - dst_gt[i]) > 1e-10:
            gdaltest.post_reason('Bad geotransform')
            return 'fail'

    dst_wkt = ds.GetProjectionRef()
    if dst_wkt.find('AUTHORITY["EPSG","4326"]') == -1:
        gdaltest.post_reason('Bad projection')
        return 'fail'

    src_ds = None
    ds = None

    return 'success'

###############################################################################
# Test gdaldem tpi

def test_gdaldem_lib_tpi():

    src_ds = gdal.Open('../gdrivers/data/n43.dt0')
    ds = gdal.DEMProcessing('', src_ds, 'tpi', format = 'MEM')
    if ds is None:
        return 'fail'

    cs = ds.GetRasterBand(1).Checksum()
    if cs != 60504:
        gdaltest.post_reason('Bad checksum')
        print(cs)
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test gdaldem tri

def test_gdaldem_lib_tri():

    src_ds = gdal.Open('../gdrivers/data/n43.dt0')
    ds = gdal.DEMProcessing('', src_ds, 'tri', format = 'MEM')
    if ds is None:
        return 'fail'

    cs = ds.GetRasterBand(1).Checksum()
    if cs != 61143:
        gdaltest.post_reason('Bad checksum')
        print(cs)
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test gdaldem roughness

def test_gdaldem_lib_roughness():

    src_ds = gdal.Open('../gdrivers/data/n43.dt0')
    ds = gdal.DEMProcessing('', src_ds, 'roughness', format = 'MEM')
    if ds is None:
        return 'fail'

    cs = ds.GetRasterBand(1).Checksum()
    if cs != 38624:
        gdaltest.post_reason('Bad checksum')
        print(cs)
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test gdaldem slope -alg ZevenbergenThorne

def test_gdaldem_lib_slope_ZevenbergenThorne():

    src_ds = gdal.Open('../gdrivers/data/n43.dt0')
    ds = gdal.DEMProcessing('', src_ds, 'slope', format = 'MEM', alg = 'ZevenbergenThorne', scale = 111120, zFactor = 30)
    if ds is None:
        return 'fail'

    cs = ds.GetRasterBand(1).Checksum()
    if cs != 64393:
        gdaltest.post_reason('Bad checksum')
        print(cs)
        return 'fail'

    return 'success'

###############################################################################
# Test gdaldem aspect -alg ZevenbergenThorne

def test_gdaldem_lib_aspect_ZevenbergenThorne():

    src_ds = gdal.Open('../gdrivers/data/n43.dt0')
    ds = gdal.DEMProcessing('', src_ds, 'aspect', format = 'MEM', alg = 'ZevenbergenThorne', scale = 111120, zFactor = 30)
    if ds is None:
        return 'fail'

    cs = ds.GetRasterBand(1).Checksum()
    if cs != 50539:
        gdaltest.post_reason('Bad checksum')
        print(cs)
        return 'fail'

    return 'success'

###############################################################################
# Test gdaldem hillshade with nodata values

def test_gdaldem_lib_nodata():

    for ( value, type ) in [ ( 0, gdal.GDT_Byte ),
                             ( 1, gdal.GDT_Byte ),
                             ( 255, gdal.GDT_Byte ),
                             ( 0, gdal.GDT_UInt16 ),
                             ( 1, gdal.GDT_UInt16 ),
                             ( 65535, gdal.GDT_UInt16 ),
                             ( 0, gdal.GDT_Int16 ),
                             ( -32678, gdal.GDT_Int16 ),
                             ( 32767, gdal.GDT_Int16 ) ]:
        src_ds = gdal.GetDriverByName('MEM').Create('', 10, 10, 1, type)
        src_ds.GetRasterBand(1).SetNoDataValue(value)
        src_ds.GetRasterBand(1).Fill(value)

        ds = gdal.DEMProcessing('', src_ds, 'hillshade', format = 'MEM')
        if ds is None:
            return 'fail'

        cs = ds.GetRasterBand(1).Checksum()
        if cs != 0:
            gdaltest.post_reason('Bad checksum')
            print(cs)
            return 'fail'

        src_ds = None
        ds = None

    src_ds = gdal.GetDriverByName('MEM').Create('', 3, 3, 1)
    src_ds.GetRasterBand(1).SetNoDataValue(0)
    src_ds.GetRasterBand(1).WriteRaster(1,1,1,1, struct.pack('B', 255))

    ds = gdal.DEMProcessing('', src_ds, 'hillshade', format = 'MEM')
    cs = ds.GetRasterBand(1).Checksum()
    if cs != 0:
        gdaltest.post_reason('Bad checksum')
        print(cs)
        print(ds.ReadAsArray())
        return 'fail'

    ds = gdal.DEMProcessing('', src_ds, 'hillshade', format = 'MEM', computeEdges = True)
    cs = ds.GetRasterBand(1).Checksum()
    if cs != 10:
        gdaltest.post_reason('Bad checksum')
        print(cs)
        print(ds.ReadAsArray()) # Should be 0 0 0 0 181 0 0 0 0
        return 'fail'

    # Same with floating point
    src_ds = gdal.GetDriverByName('MEM').Create('', 3, 3, 1, gdal.GDT_Float32)
    src_ds.GetRasterBand(1).SetNoDataValue(0)
    src_ds.GetRasterBand(1).WriteRaster(1,1,1,1, struct.pack('f', 255))

    ds = gdal.DEMProcessing('', src_ds, 'hillshade', format = 'MEM')
    cs = ds.GetRasterBand(1).Checksum()
    if cs != 0:
        gdaltest.post_reason('Bad checksum')
        print(cs)
        print(ds.ReadAsArray())
        return 'fail'

    ds = gdal.DEMProcessing('', src_ds, 'hillshade', format = 'MEM', computeEdges = True)
    cs = ds.GetRasterBand(1).Checksum()
    if cs != 10:
        gdaltest.post_reason('Bad checksum')
        print(cs)
        print(ds.ReadAsArray()) # Should be 0 0 0 0 181 0 0 0 0
        return 'fail'

    return 'success'

gdaltest_list = [
    test_gdaldem_lib_hillshade,
    test_gdaldem_lib_hillshade_float,
    test_gdaldem_lib_hillshade_float_png,
    test_gdaldem_lib_hillshade_combined,
    test_gdaldem_lib_hillshade_ZevenbergenThorne,
    test_gdaldem_lib_hillshade_ZevenbergenThorne_combined,
    test_gdaldem_lib_hillshade_compute_edges,
    test_gdaldem_lib_hillshade_compute_edges_float,
    test_gdaldem_lib_hillshade_azimuth,
    test_gdaldem_lib_hillshade_multidirectional,
    test_gdaldem_lib_hillshade_multidirectional_ZevenbergenThorne,
    test_gdaldem_lib_color_relief,
    test_gdaldem_lib_tpi,
    test_gdaldem_lib_tri,
    test_gdaldem_lib_roughness,
    test_gdaldem_lib_slope_ZevenbergenThorne,
    test_gdaldem_lib_aspect_ZevenbergenThorne,
    test_gdaldem_lib_nodata
    ]


if __name__ == '__main__':

    gdaltest.setup_run( 'test_gdaldem_lib' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()
