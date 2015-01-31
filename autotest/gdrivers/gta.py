#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test GTA driver
# Author:   Even Rouault <even dot rouault at mines dash paris dot org>
#
###############################################################################
# Copyright (c) 2011, Even Rouault <even dot rouault at mines dash paris dot org>
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

import gdaltest
from osgeo import gdal


gdaltest_list = []

init_list = [ \
    ('byte.tif', 1, 4672, []),
    ('byte_signed.tif', 1, 4672, []),
    ('int16.tif', 1, 4672, []),
    ('uint16.tif', 1, 4672, []),
    ('int32.tif', 1, 4672, []),
    ('uint32.tif', 1, 4672, []),
    ('float32.tif', 1, 4672, []),
    ('float64.tif', 1, 4672, []),
    ('cint16.tif', 1, 5028, []),
    ('cint32.tif', 1, 5028, []),
    ('cfloat32.tif', 1, 5028, []),
    ('cfloat64.tif', 1, 5028, []),
    ('rgbsmall.tif', 1, 21212, []) ]

###############################################################################
# Verify we have the driver.

def gta_1():

    try:
        gdaltest.gta_drv = gdal.GetDriverByName( 'GTA' )
    except:
        gdaltest.gta_drv = None
        return 'skip'

    return 'success'

###############################################################################
# Test updating existing dataset, check srs, check gt

def gta_2():

    if gdaltest.gta_drv is None:
        return 'skip'

    src_ds = gdal.Open('data/byte.tif')
    out_ds = gdaltest.gta_drv.CreateCopy('/vsimem/byte.gta', src_ds)
    out_ds = None

    out_ds = gdal.Open('/vsimem/byte.gta', gdal.GA_Update)
    out_ds.GetRasterBand(1).Fill(0)
    out_ds = None

    out_ds = gdal.Open('/vsimem/byte.gta')
    cs = out_ds.GetRasterBand(1).Checksum()
    if cs != 0:
        gdaltest.post_reason('did not get expected checksum')
        return 'fail'
    out_ds = None

    out_ds = gdal.Open('/vsimem/byte.gta', gdal.GA_Update)
    out_ds.WriteRaster(0,0,20,20,src_ds.ReadRaster(0,0,20,20))
    out_ds = None

    out_ds = gdal.Open('/vsimem/byte.gta')
    cs = out_ds.GetRasterBand(1).Checksum()
    if cs != src_ds.GetRasterBand(1).Checksum():
        gdaltest.post_reason('did not get expected checksum')
        return 'fail'

    gt = out_ds.GetGeoTransform()
    wkt = out_ds.GetProjectionRef()
    out_ds = None

    expected_gt = src_ds.GetGeoTransform()
    for i in range(6):
        if abs(gt[i] - expected_gt[i]) > 1e-6:
            gdaltest.post_reason('did not get expected wkt')
            return 'fail'

    if wkt != src_ds.GetProjectionRef():
        gdaltest.post_reason('did not get expected wkt')
        return 'fail'

    gdaltest.gta_drv.Delete('/vsimem/byte.gta')

    return 'success'

###############################################################################
# Test writing and readings GCPs

def gta_3():

    if gdaltest.gta_drv is None:
        return 'skip'

    src_ds = gdal.Open( '../gcore/data/gcps.vrt' )

    new_ds = gdaltest.gta_drv.CreateCopy( '/vsimem/gta_3.gta', src_ds )
    new_ds = None

    new_ds = gdal.Open( '/vsimem/gta_3.gta' )

    if new_ds.GetGeoTransform() != (0.0, 1.0, 0.0, 0.0, 0.0, 1.0):
        gdaltest.post_reason( 'GeoTransform not set properly.' )
        return 'fail'

    if new_ds.GetProjectionRef() != '':
        gdaltest.post_reason( 'Projection not set properly.' )
        return 'fail'

    if new_ds.GetGCPProjection() != src_ds.GetGCPProjection():
        gdaltest.post_reason( 'GCP Projection not set properly.' )
        return 'fail'

    gcps = new_ds.GetGCPs()
    expected_gcps = src_ds.GetGCPs()
    if len(gcps) != len(expected_gcps):
        gdaltest.post_reason( 'GCP count wrong.' )
        return 'fail'

    new_ds = None

    gdaltest.gta_drv.Delete( '/vsimem/gta_3.gta' )

    return 'success'

###############################################################################
# Test band metadata

def gta_4():

    if gdaltest.gta_drv is None:
        return 'skip'

    src_ds = gdal.GetDriverByName('MEM').Create('',1,1,17)
    src_ds.GetRasterBand(1).Fill(255)
    src_ds.GetRasterBand(1).ComputeStatistics(False)
    src_ds.GetRasterBand(1).SetNoDataValue(123)
    src_ds.GetRasterBand(1).SetCategoryNames(['a', 'b'])
    src_ds.GetRasterBand(1).SetOffset(2)
    src_ds.GetRasterBand(1).SetScale(3)
    src_ds.GetRasterBand(1).SetUnitType('custom')
    src_ds.GetRasterBand(1).SetDescription('description')
    for i in range(17):
        if i != gdal.GCI_PaletteIndex:
            src_ds.GetRasterBand(i + 1).SetColorInterpretation(i)

    new_ds = gdaltest.gta_drv.CreateCopy( '/vsimem/gta_4.gta', src_ds )
    new_ds = None

    new_ds = gdal.Open( '/vsimem/gta_4.gta' )
    band = new_ds.GetRasterBand(1)
    if band.GetNoDataValue() != 123:
        gdaltest.post_reason('did not get expected nodata value')
        print(band.GetNoDataValue())
        return 'fail'
    if band.GetMinimum() != 255:
        gdaltest.post_reason('did not get expected minimum value')
        print(band.GetMinimum())
        return 'fail'
    if band.GetMaximum() != 255:
        gdaltest.post_reason('did not get expected maximum value')
        print(band.GetMaximum())
        return 'fail'
    if band.GetCategoryNames() != ['a', 'b']:
        gdaltest.post_reason('did not get expected category names')
        print(band.GetCategoryNames())
        return 'fail'
    if band.GetOffset() != 2:
        gdaltest.post_reason('did not get expected offset value')
        print(band.GetOffset())
        return 'fail'
    if band.GetScale() != 3:
        gdaltest.post_reason('did not get expected scale value')
        print(band.GetScale())
        return 'fail'
    if band.GetUnitType() != 'custom':
        gdaltest.post_reason('did not get expected unit value')
        print(band.GetUnitType())
        return 'fail'
    if band.GetDescription() != 'description':
        gdaltest.post_reason('did not get expected description')
        print(band.GetDescription())
        return 'fail'
    for i in range(17):
        if i != gdal.GCI_PaletteIndex:
            if new_ds.GetRasterBand(i+1).GetColorInterpretation() != i:
                gdaltest.post_reason('did not get expected color interpreation for band %d' % (i+1))
                print(new_ds.GetRasterBand(i+1).GetColorInterpretation())
                return 'fail'

    new_ds = None

    gdaltest.gta_drv.Delete( '/vsimem/gta_4.gta' )

    return 'success'

###############################################################################
# Test compression algorithms

def gta_5():

    if gdaltest.gta_drv is None:
        return 'skip'

    src_ds = gdal.Open('data/byte.tif')

    compress_list =['NONE',
                    'BZIP2',
                    "XZ",
                    "ZLIB",
                    "ZLIB1",
                    "ZLIB2",
                    "ZLIB3",
                    "ZLIB4",
                    "ZLIB5",
                    "ZLIB6",
                    "ZLIB7",
                    "ZLIB8",
                    "ZLIB9"]

    for compress in compress_list:
        out_ds = gdaltest.gta_drv.CreateCopy('/vsimem/gta_5.gta', src_ds, options = ['COMPRESS=' + compress])
        del out_ds

    gdaltest.gta_drv.Delete( '/vsimem/gta_5.gta' )

    return 'success'

for item in init_list:
    if item[0] == 'byte_signed.tif':
        filename = item[0]
    else:
        filename = '../../gcore/data/' + item[0]
    ut = gdaltest.GDALTest( 'GTA', filename, item[1], item[2], options = item[3] )
    if ut is None:
        print( 'GTA tests skipped' )
        sys.exit()
    gdaltest_list.append( (ut.testCreateCopy, item[0]) )

gdaltest_list.append( gta_1 )
gdaltest_list.append( gta_2 )
gdaltest_list.append( gta_3 )
gdaltest_list.append( gta_4 )
gdaltest_list.append( gta_5 )

if __name__ == '__main__':

    gdaltest.setup_run( 'gta' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

