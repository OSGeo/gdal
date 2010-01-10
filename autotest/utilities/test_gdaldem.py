#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdaldem testing
# Author:   Even Rouault <even dot rouault @ mines-paris dot org>
# 
###############################################################################
# Copyright (c) 2009, Even Rouault <even dot rouault @ mines-paris dot org>
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
import os

sys.path.append( '../pymod' )

import gdal
import gdaltest
import test_cli_utilities

###############################################################################
# Test gdaldem hillshade

def test_gdaldem_hillshade():
    if test_cli_utilities.get_gdaldem_path() is None:
        return 'skip'

    gdaltest.runexternal(test_cli_utilities.get_gdaldem_path() + ' hillshade -s 111120 -z 30 ../gdrivers/data/n43.dt0 tmp/n43_hillshade.tif')

    src_ds = gdal.Open('../gdrivers/data/n43.dt0')
    ds = gdal.Open('tmp/n43_hillshade.tif')
    if ds is None:
        return 'fail'

    if ds.GetRasterBand(1).Checksum() != 45587:
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

    if ds.GetRasterBand(1).GetNoDataValue() != 0:
        gdaltest.post_reason('Bad nodata value')
        return 'fail'

    src_ds = None
    ds = None

    return 'success'

###############################################################################
# Test gdaldem hillshade to PNG

def test_gdaldem_hillshade_png():
    if test_cli_utilities.get_gdaldem_path() is None:
        return 'skip'

    gdaltest.runexternal(test_cli_utilities.get_gdaldem_path() + ' hillshade -of PNG  -s 111120 -z 30 ../gdrivers/data/n43.dt0 tmp/n43_hillshade.png')

    ds = gdal.Open('tmp/n43_hillshade.png')
    if ds is None:
        return 'fail'
        
    if ds.GetRasterBand(1).Checksum() != 45587:
        gdaltest.post_reason('Bad checksum')
        return 'fail'
        
    ds = None

    return 'success'
    
###############################################################################
# Test gdaldem slope

def test_gdaldem_slope():
    if test_cli_utilities.get_gdaldem_path() is None:
        return 'skip'

    gdaltest.runexternal(test_cli_utilities.get_gdaldem_path() + ' slope -s 111120 ../gdrivers/data/n43.dt0 tmp/n43_slope.tif')

    src_ds = gdal.Open('../gdrivers/data/n43.dt0')
    ds = gdal.Open('tmp/n43_slope.tif')
    if ds is None:
        return 'fail'

    if ds.GetRasterBand(1).Checksum() != 63748:
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

    if ds.GetRasterBand(1).GetNoDataValue() != -9999.0:
        gdaltest.post_reason('Bad nodata value')
        return 'fail'

    src_ds = None
    ds = None

    return 'success'

###############################################################################
# Test gdaldem aspect

def test_gdaldem_aspect():
    if test_cli_utilities.get_gdaldem_path() is None:
        return 'skip'

    gdaltest.runexternal(test_cli_utilities.get_gdaldem_path() + ' aspect ../gdrivers/data/n43.dt0 tmp/n43_aspect.tif')

    src_ds = gdal.Open('../gdrivers/data/n43.dt0')
    ds = gdal.Open('tmp/n43_aspect.tif')
    if ds is None:
        return 'fail'

    if ds.GetRasterBand(1).Checksum() != 54885:
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

    if ds.GetRasterBand(1).GetNoDataValue() != -9999.0:
        gdaltest.post_reason('Bad nodata value')
        return 'fail'

    src_ds = None
    ds = None

    return 'success'

###############################################################################
# Test gdaldem color relief

def test_gdaldem_color_relief():
    if test_cli_utilities.get_gdaldem_path() is None:
        return 'skip'

    gdaltest.runexternal(test_cli_utilities.get_gdaldem_path() + ' color-relief ../gdrivers/data/n43.dt0 data/color_file.txt tmp/n43_colorrelief.tif')
    src_ds = gdal.Open('../gdrivers/data/n43.dt0')
    ds = gdal.Open('tmp/n43_colorrelief.tif')
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
# Test gdaldem color relief to VRT

def test_gdaldem_color_relief_vrt():
    if test_cli_utilities.get_gdaldem_path() is None:
        return 'skip'

    gdaltest.runexternal(test_cli_utilities.get_gdaldem_path() + ' color-relief -of VRT ../gdrivers/data/n43.dt0 data/color_file.txt tmp/n43_colorrelief.vrt')
    src_ds = gdal.Open('../gdrivers/data/n43.dt0')
    ds = gdal.Open('tmp/n43_colorrelief.vrt')
    if ds is None:
        return 'fail'

    ds_ref = gdal.Open('tmp/n43_colorrelief.tif')
    if gdaltest.compare_ds(ds, ds_ref, verbose = 0) > 1:
        gdaltest.post_reason('Bad checksum')
        return 'fail'
    ds_ref = None

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
# Test gdaldem color relief from a Float32 dataset

def test_gdaldem_color_relief_from_float32():
    if test_cli_utilities.get_gdaldem_path() is None:
        return 'skip'
    if test_cli_utilities.get_gdal_translate_path() is None:
        return 'skip'
        
    gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' -ot Float32 ../gdrivers/data/n43.dt0 tmp/n43_float32.tif')
    gdaltest.runexternal(test_cli_utilities.get_gdaldem_path() + ' color-relief tmp/n43_float32.tif data/color_file.txt tmp/n43_colorrelief_from_float32.tif')
    ds = gdal.Open('tmp/n43_colorrelief_from_float32.tif')
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

    ds = None

    return 'success'
    
###############################################################################
# Test gdaldem color relief to PNG

def test_gdaldem_color_relief_png():
    if test_cli_utilities.get_gdaldem_path() is None:
        return 'skip'

    gdaltest.runexternal(test_cli_utilities.get_gdaldem_path() + ' color-relief -of PNG ../gdrivers/data/n43.dt0 data/color_file.txt tmp/n43_colorrelief.png')
    ds = gdal.Open('tmp/n43_colorrelief.png')
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

    ds = None

    return 'success'
    
###############################################################################
# Test gdaldem color relief from a Float32 to PNG

def test_gdaldem_color_relief_from_float32_to_png():
    if test_cli_utilities.get_gdaldem_path() is None:
        return 'skip'
    if test_cli_utilities.get_gdal_translate_path() is None:
        return 'skip'
        
    gdaltest.runexternal(test_cli_utilities.get_gdaldem_path() + ' color-relief -of PNG tmp/n43_float32.tif data/color_file.txt tmp/n43_colorrelief_from_float32.png')
    ds = gdal.Open('tmp/n43_colorrelief_from_float32.png')
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

    ds = None

    return 'success'
    
###############################################################################
# Test gdaldem color relief with -nearest_color_entry

def test_gdaldem_color_relief_nearest_color_entry():
    if test_cli_utilities.get_gdaldem_path() is None:
        return 'skip'

    gdaltest.runexternal(test_cli_utilities.get_gdaldem_path() + ' color-relief -nearest_color_entry ../gdrivers/data/n43.dt0 data/color_file.txt tmp/n43_colorrelief_nearest.tif')
    ds = gdal.Open('tmp/n43_colorrelief_nearest.tif')
    if ds is None:
        return 'fail'

    if ds.GetRasterBand(1).Checksum() != 57296:
        print(ds.GetRasterBand(1).Checksum())
        gdaltest.post_reason('Bad checksum')
        return 'fail'

    if ds.GetRasterBand(2).Checksum() != 42926:
        print(ds.GetRasterBand(2).Checksum())
        gdaltest.post_reason('Bad checksum')
        return 'fail'

    if ds.GetRasterBand(3).Checksum() != 47181:
        print(ds.GetRasterBand(3).Checksum())
        gdaltest.post_reason('Bad checksum')
        return 'fail'

    ds = None

    return 'success'
    
###############################################################################
# Test gdaldem color relief with -nearest_color_entry and -of VRT

def test_gdaldem_color_relief_nearest_color_entry_vrt():
    if test_cli_utilities.get_gdaldem_path() is None:
        return 'skip'

    gdaltest.runexternal(test_cli_utilities.get_gdaldem_path() + ' color-relief -of VRT -nearest_color_entry ../gdrivers/data/n43.dt0 data/color_file.txt tmp/n43_colorrelief_nearest.vrt')
    ds = gdal.Open('tmp/n43_colorrelief_nearest.vrt')
    if ds is None:
        return 'fail'

    if ds.GetRasterBand(1).Checksum() != 57296:
        print(ds.GetRasterBand(1).Checksum())
        gdaltest.post_reason('Bad checksum')
        return 'fail'

    if ds.GetRasterBand(2).Checksum() != 42926:
        print(ds.GetRasterBand(2).Checksum())
        gdaltest.post_reason('Bad checksum')
        return 'fail'

    if ds.GetRasterBand(3).Checksum() != 47181:
        print(ds.GetRasterBand(3).Checksum())
        gdaltest.post_reason('Bad checksum')
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Cleanup

def test_gdaldem_cleanup():
    try:
        os.remove('tmp/n43_hillshade.tif')
    except:
        pass
    try:
        os.remove('tmp/n43_hillshade.png')
        os.remove('tmp/n43_hillshade.png.aux.xml')
    except:
        pass
    try:
        os.remove('tmp/n43_slope.tif')
    except:
        pass
    try:
        os.remove('tmp/n43_aspect.tif')
    except:
        pass
    try:
        os.remove('tmp/n43_colorrelief.tif')
    except:
        pass
    try:
        os.remove('tmp/n43_colorrelief.vrt')
    except:
        pass
    try:
        os.remove('tmp/n43_float32.tif')
        os.remove('tmp/n43_colorrelief_from_float32.tif')
    except:
        pass
    try:
        os.remove('tmp/n43_colorrelief.png')
        os.remove('tmp/n43_colorrelief.png.aux.xml')
    except:
        pass
    try:
        os.remove('tmp/n43_colorrelief_from_float32.png')
        os.remove('tmp/n43_colorrelief_from_float32.png.aux.xml')
    except:
        pass
    try:
        os.remove('tmp/n43_colorrelief_nearest.tif')
    except:
        pass
    try:
        os.remove('tmp/n43_colorrelief_nearest.vrt')
    except:
        pass
    return 'success'

gdaltest_list = [
    test_gdaldem_hillshade,
    test_gdaldem_hillshade_png,
    test_gdaldem_slope,
    test_gdaldem_aspect,
    test_gdaldem_color_relief,
    test_gdaldem_color_relief_vrt,
    test_gdaldem_color_relief_from_float32,
    test_gdaldem_color_relief_png,
    test_gdaldem_color_relief_from_float32_to_png,
    test_gdaldem_color_relief_nearest_color_entry,
    test_gdaldem_color_relief_nearest_color_entry_vrt,
    test_gdaldem_cleanup
    ]


if __name__ == '__main__':

    gdaltest.setup_run( 'test_gdaldem' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()
