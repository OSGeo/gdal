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

gdaltest_list = [
    test_gdaldem_lib_hillshade,
    test_gdaldem_lib_hillshade_combined,
    test_gdaldem_lib_hillshade_compute_edges,
    test_gdaldem_lib_hillshade_azimuth,
    test_gdaldem_lib_color_relief
    ]


if __name__ == '__main__':

    gdaltest.setup_run( 'test_gdaldem_lib' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()
