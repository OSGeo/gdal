#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for SQLite RasterLite2 driver.
# Author:   Even Rouault, <even dot rouault at spatialys dot com>
#
###############################################################################
# Copyright (c) 2016, Even Rouault <even dot rouault at spatialys dot com>
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
from osgeo import gdal

sys.path.append( '../pymod' )

import gdaltest

###############################################################################
# Get the rl2 driver

def rl2_1():

    gdaltest.rl2_drv = gdal.GetDriverByName( 'SQLite' )
    if gdaltest.rl2_drv is None:
        return 'skip'
    if gdaltest.rl2_drv.GetMetadataItem('DCAP_RASTER') is None:
        gdaltest.rl2_drv = None
        return 'skip'

    return 'success'

###############################################################################
# Test opening a rl2 DB gray level

def rl2_2():

    if gdaltest.rl2_drv is None:
        return 'skip'

    ds = gdal.Open('data/byte.rl2')

    if ds.RasterCount != 1:
        gdaltest.post_reason('expected 1 band')
        return 'fail'

    if ds.GetRasterBand(1).GetOverviewCount() != 0:
        gdaltest.post_reason('did not expect overview')
        return 'fail'

    cs = ds.GetRasterBand(1).Checksum()
    if cs != 4672:
        gdaltest.post_reason('fail')
        print(cs)
        return 'fail'

    gt = ds.GetGeoTransform()
    expected_gt = ( 440720.0, 60.0, 0.0, 3751320.0, 0.0, -60.0 )
    for i in range(6):
        if abs(gt[i] - expected_gt[i]) > 1e-15:
            print(gt)
            print(expected_gt)
            return 'fail'

    wkt = ds.GetProjectionRef()
    if wkt.find('26711') < 0:
        gdaltest.post_reason('fail')
        return 'fail'

    if ds.GetRasterBand(1).GetColorInterpretation() != gdal.GCI_GrayIndex:
        gdaltest.post_reason('fail')
        return 'fail'

    if ds.GetRasterBand(1).GetMinimum() != 74:
        gdaltest.post_reason('fail')
        return 'fail'

    if ds.GetRasterBand(1).GetOverview(-1) is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    if ds.GetRasterBand(1).GetOverview(0) is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    subds = ds.GetSubDatasets()
    expected_subds = []
    if subds != expected_subds:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.SetConfigOption('RL2_SHOW_ALL_PYRAMID_LEVELS', 'YES')
    ds = gdal.Open('data/byte.rl2')
    gdal.SetConfigOption('RL2_SHOW_ALL_PYRAMID_LEVELS', None)

    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    if cs != 1087:
        gdaltest.post_reason('fail')
        print(cs)
        return 'fail'


    return 'success'

###############################################################################
# Test opening a rl2 DB gray level

def rl2_3():

    if gdaltest.rl2_drv is None:
        return 'skip'

    ds = gdal.Open( 'data/small_world.rl2' )

    if ds.GetRasterBand(1).GetColorInterpretation() != gdal.GCI_RedBand:
        gdaltest.post_reason('fail')
        return 'fail'

    ds.GetRasterBand(1).GetNoDataValue()

    cs = ds.GetRasterBand(1).Checksum()
    if cs != 25550:
        gdaltest.post_reason('fail')
        print(cs)
        return 'fail'

    cs = ds.GetRasterBand(2).Checksum()
    if cs != 28146:
        gdaltest.post_reason('fail')
        print(cs)
        return 'fail'

    if ds.GetRasterBand(1).GetOverviewCount() != 2:
        gdaltest.post_reason('fail')
        return 'fail'

    cs = ds.GetRasterBand(1).GetOverview(1).Checksum()
    if cs != 51412:
        gdaltest.post_reason('fail')
        print(cs)
        return 'fail'

    subds = ds.GetSubDatasets()
    expected_subds = [('RASTERLITE2:data/small_world.rl2:small_world:1:world_west', 'Coverage small_world, section world_west / 1'), ('RASTERLITE2:data/small_world.rl2:small_world:2:world_east', 'Coverage small_world, section world_east / 2')]
    if subds != expected_subds:
        gdaltest.post_reason('fail')
        print(subds)
        return 'fail'

    ds = gdal.Open('RASTERLITE2:data/small_world.rl2:small_world:1:world_west')

    cs = ds.GetRasterBand(1).Checksum()
    if cs != 3721:
        gdaltest.post_reason('fail')
        print(cs)
        return 'fail'

    if ds.GetRasterBand(1).GetOverviewCount() != 1:
        gdaltest.post_reason('fail')
        return 'fail'

    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    if cs != 35686:
        gdaltest.post_reason('fail')
        print(cs)
        return 'fail'

    return 'success'

###############################################################################
# Test opening a rl2 DB paletted

def rl2_4():

    if gdaltest.rl2_drv is None:
        return 'skip'

    ds = gdal.Open( 'data/small_world_pct.rl2' )

    if ds.GetRasterBand(1).GetColorInterpretation() != gdal.GCI_PaletteIndex:
        gdaltest.post_reason('fail')
        return 'fail'

    cs = ds.GetRasterBand(1).Checksum()
    if cs != 14890:
        gdaltest.post_reason('fail')
        print(cs)
        return 'fail'

    pct = ds.GetRasterBand(1).GetColorTable()
    if pct.GetCount() != 256:
        gdaltest.post_reason('fail')
        return 'fail'
    if pct.GetColorEntry(1) != (176, 184, 176, 255):
        gdaltest.post_reason('fail')
        print(pct.GetColorEntry(1))
        return 'fail'

    pct = ds.GetRasterBand(1).GetColorTable()
    if pct.GetCount() != 256:
        gdaltest.post_reason('fail')
        return 'fail'

    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    if cs != 35614:
        gdaltest.post_reason('fail')
        print(cs)
        return 'fail'

    return 'success'

###############################################################################
# Test opening a rl2 DB with various data types

def rl2_5():

    if gdaltest.rl2_drv is None:
        return 'skip'

    ds = gdal.Open( 'data/multi_type.rl2' )

    subds = ds.GetSubDatasets()
    expected_subds = [('RASTERLITE2:data/multi_type.rl2:uint8', 'Coverage uint8'), ('RASTERLITE2:data/multi_type.rl2:int8', 'Coverage int8'), ('RASTERLITE2:data/multi_type.rl2:uint16', 'Coverage uint16'), ('RASTERLITE2:data/multi_type.rl2:int16', 'Coverage int16'), ('RASTERLITE2:data/multi_type.rl2:uint32', 'Coverage uint32'), ('RASTERLITE2:data/multi_type.rl2:int32', 'Coverage int32'), ('RASTERLITE2:data/multi_type.rl2:float', 'Coverage float'), ('RASTERLITE2:data/multi_type.rl2:double', 'Coverage double'), ('RASTERLITE2:data/multi_type.rl2:1bit', 'Coverage 1bit'), ('RASTERLITE2:data/multi_type.rl2:2bit', 'Coverage 2bit'), ('RASTERLITE2:data/multi_type.rl2:4bit', 'Coverage 4bit')]
    if subds != expected_subds:
        gdaltest.post_reason('fail')
        print(subds)
        return 'fail'

    tests = [ ('RASTERLITE2:data/multi_type.rl2:uint8', gdal.GDT_Byte, 4672),
              ('RASTERLITE2:data/multi_type.rl2:int8', gdal.GDT_Byte, 4575),
              ('RASTERLITE2:data/multi_type.rl2:uint16', gdal.GDT_UInt16, 4457),
              ('RASTERLITE2:data/multi_type.rl2:int16', gdal.GDT_Int16, 4457),
              ('RASTERLITE2:data/multi_type.rl2:uint32', gdal.GDT_UInt32, 4457),
              ('RASTERLITE2:data/multi_type.rl2:int32', gdal.GDT_Int32, 4457),
              ('RASTERLITE2:data/multi_type.rl2:float', gdal.GDT_Float32, 4457),
              ('RASTERLITE2:data/multi_type.rl2:double', gdal.GDT_Float64, 4457),
              ('RASTERLITE2:data/multi_type.rl2:1bit', gdal.GDT_Byte, 4873)]
    for (subds_name, dt, expected_cs) in tests:
        ds = gdal.Open( subds_name )
        if ds.GetRasterBand(1).DataType != dt:
            gdaltest.post_reason('fail')
            print(subds_name)
            return 'fail'
        cs = ds.GetRasterBand(1).Checksum()
        if cs != expected_cs:
            gdaltest.post_reason('fail')
            print(cs)
            return 'fail'
        if subds_name == 'RASTERLITE2:data/multi_type.rl2:int8':
            if ds.GetRasterBand(1).GetMetadataItem('PIXELTYPE', 'IMAGE_STRUCTURE') != 'SIGNEDBYTE':
                gdaltest.post_reason('fail')
                print(ds.GetRasterBand(1).GetMetadataItem('PIXELTYPE', 'IMAGE_STRUCTURE'))
                return 'fail'

    return 'success'

###############################################################################
# Test CreateCopy()

def rl2_6():

    if gdaltest.rl2_drv is None:
        return 'skip'

    tst = gdaltest.GDALTest( 'SQLite', 'byte.tif', 1, 4672 )
    return tst.testCreateCopy( vsimem = 1 )

###############################################################################
# Test CreateCopy()

def rl2_7():

    if gdaltest.rl2_drv is None:
        return 'skip'

    tst = gdaltest.GDALTest( 'SQLite', 'small_world.tif', 1, 30111, options = ['COMPRESS=PNG'] )
    return tst.testCreateCopy( vsimem = 1 )

gdaltest_list = [
    rl2_1,
    rl2_2,
    rl2_3,
    rl2_4,
    rl2_5,
    rl2_6,
    rl2_7
]

if __name__ == '__main__':

    gdaltest.setup_run( 'rl2' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()
