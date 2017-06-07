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
from osgeo import gdal, ogr

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
# Test CreateCopy() on a grayscale uint8

def rl2_6():

    if gdaltest.rl2_drv is None:
        return 'skip'

    tst = gdaltest.GDALTest( 'SQLite', 'byte.tif', 1, 4672 )
    return tst.testCreateCopy( vsimem = 1, check_minmax = False )

###############################################################################
# Test CreateCopy() on a RGB

def rl2_7():

    if gdaltest.rl2_drv is None:
        return 'skip'

    tst = gdaltest.GDALTest( 'SQLite', 'small_world.tif', 1, 30111, options = ['COMPRESS=PNG'] )
    return tst.testCreateCopy( vsimem = 1 )

###############################################################################
# Test CreateCopy() on a paletted dataset

def rl2_8():

    if gdaltest.rl2_drv is None:
        return 'skip'

    tst = gdaltest.GDALTest( 'SQLite', 'small_world_pct.tif', 1, 14890, options = ['COMPRESS=PNG'] )
    return tst.testCreateCopy( vsimem = 1, check_minmax = False )

###############################################################################
# Test CreateCopy() on a DATAGRID uint16

def rl2_9():

    if gdaltest.rl2_drv is None:
        return 'skip'

    tst = gdaltest.GDALTest( 'SQLite', '../../gcore/data/uint16.tif', 1, 4672 )
    return tst.testCreateCopy( vsimem = 1 )

###############################################################################
# Test CreateCopy() on a DATAGRID int16

def rl2_10():

    if gdaltest.rl2_drv is None:
        return 'skip'

    tst = gdaltest.GDALTest( 'SQLite', '../../gcore/data/int16.tif', 1, 4672 )
    return tst.testCreateCopy( vsimem = 1 )

###############################################################################
# Test CreateCopy() on a DATAGRID uint32

def rl2_11():

    if gdaltest.rl2_drv is None:
        return 'skip'

    tst = gdaltest.GDALTest( 'SQLite', '../../gcore/data/uint32.tif', 1, 4672 )
    return tst.testCreateCopy( vsimem = 1 )

###############################################################################
# Test CreateCopy() on a DATAGRID int32

def rl2_12():

    if gdaltest.rl2_drv is None:
        return 'skip'

    tst = gdaltest.GDALTest( 'SQLite', '../../gcore/data/int32.tif', 1, 4672 )
    return tst.testCreateCopy( vsimem = 1 )

###############################################################################
# Test CreateCopy() on a DATAGRID float

def rl2_13():

    if gdaltest.rl2_drv is None:
        return 'skip'

    tst = gdaltest.GDALTest( 'SQLite', '../../gcore/data/float32.tif', 1, 4672 )
    return tst.testCreateCopy( vsimem = 1 )

###############################################################################
# Test CreateCopy() on a DATAGRID double

def rl2_14():

    if gdaltest.rl2_drv is None:
        return 'skip'

    tst = gdaltest.GDALTest( 'SQLite', '../../gcore/data/float64.tif', 1, 4672 )
    return tst.testCreateCopy( vsimem = 1 )

###############################################################################
# Test CreateCopy() on a 1 bit paletted

def rl2_15():

    if gdaltest.rl2_drv is None:
        return 'skip'

    tst = gdaltest.GDALTest( 'SQLite', '../../gcore/data/1bit.bmp', 1, 200 )
    return tst.testCreateCopy( vsimem = 1, check_minmax = False )

###############################################################################
# Test CreateCopy() on a forced 1 bit

def rl2_16():

    if gdaltest.rl2_drv is None:
        return 'skip'

    tst = gdaltest.GDALTest( 'SQLite', 'byte.tif', 1, 400, options = ['NBITS=1', 'COMPRESS=CCITTFAX4'] )
    return tst.testCreateCopy( vsimem = 1, check_minmax = False )

###############################################################################
# Test CreateCopy() on a forced 2 bit

def rl2_17():

    if gdaltest.rl2_drv is None:
        return 'skip'

    tst = gdaltest.GDALTest( 'SQLite', 'byte.tif', 1, 4873, options = ['NBITS=2', 'COMPRESS=DEFLATE'] )
    return tst.testCreateCopy( vsimem = 1, check_minmax = False )

###############################################################################
# Test CreateCopy() on a forced 4 bit

def rl2_18():

    if gdaltest.rl2_drv is None:
        return 'skip'

    tst = gdaltest.GDALTest( 'SQLite', 'byte.tif', 1, 2541, options = ['NBITS=4'] )
    return tst.testCreateCopy( vsimem = 1, check_minmax = False )

###############################################################################
# Test CreateCopy() with forced monochrome

def rl2_19():

    if gdaltest.rl2_drv is None:
        return 'skip'

    tst = gdaltest.GDALTest( 'SQLite', 'byte.tif', 1, 400, options = ['PIXEL_TYPE=MONOCHROME'] )
    return tst.testCreateCopy( vsimem = 1, check_minmax = False )

###############################################################################
# Test incompatibilities on CreateCopy()
# Se https://www.gaia-gis.it/fossil/librasterlite2/wiki?name=reference_table

def rl2_20():

    if gdaltest.rl2_drv is None:
        return 'skip'

    tests = [ ( 'MONOCHROME', 2, gdal.GDT_Byte, 'NONE', None, None ),
              ( 'MONOCHROME', 1, gdal.GDT_UInt16, 'NONE', None, None ),
              ( 'PALETTE', 1, gdal.GDT_Byte, 'NONE', None, None ),
              ( 'PALETTE', 1, gdal.GDT_UInt16, 'NONE', None, gdal.ColorTable() ),
              ( 'GRAYSCALE', 2, gdal.GDT_Byte, 'NONE', None, None ),
              ( 'GRAYSCALE', 1, gdal.GDT_UInt16, 'NONE', None, None ),
              ( 'RGB', 1, gdal.GDT_Byte, 'NONE', None, None ),
              ( 'RGB', 3, gdal.GDT_Int16, 'NONE', None, None ),
              ( 'MULTIBAND', 1, gdal.GDT_Byte, 'NONE', None, None ),
              ( 'MULTIBAND', 256, gdal.GDT_Byte, 'NONE', None, None ),
              ( 'MULTIBAND', 2, gdal.GDT_Int16, 'NONE', None, None ),
              ( 'DATAGRID', 2, gdal.GDT_Byte, 'NONE', None, None ),
              ( 'DATAGRID', 1, gdal.GDT_CFloat32, 'NONE', None, None ),
              ( 'MONOCHROME', 1, gdal.GDT_Byte, 'JPEG', None, None),
              ( 'PALETTE', 1, gdal.GDT_Byte, 'JPEG', None, gdal.ColorTable()),
              ( 'GRAYSCALE', 1, gdal.GDT_Byte, 'CCITTFAX4', None, None),
              ( 'RGB', 3, gdal.GDT_Byte, 'CCITTFAX4', None, None),
              ( 'RGB', 3, gdal.GDT_UInt16, 'JPEG', None, None),
              ( 'MULTIBAND', 3, gdal.GDT_Byte, 'CCITTFAX4', None, None),
              ( 'MULTIBAND', 3, gdal.GDT_UInt16, 'CCITTFAX4', None, None),
              ( 'MULTIBAND', 2, gdal.GDT_Byte, 'CCITTFAX4', None, None),
              ( 'DATAGRID', 1, gdal.GDT_Byte, 'CCITTFAX4', None, None),
              ( 'DATAGRID', 1, gdal.GDT_Int16, 'CCITTFAX4', None, None), ]

    for (pixel_type, band_count, dt, compress, nbits, pct) in tests:
        src_ds = gdal.GetDriverByName('MEM').Create('', 1, 1, band_count, dt)
        if pct is not None:
            src_ds.GetRasterBand(1).SetColorTable(pct)
        if nbits is not None:
            src_ds.GetRasterBand(1).SetMetadataItem('NBITS', nbits, 'IMAGE_STRUCTURE')
        options = ['PIXEL_TYPE=' + pixel_type, 'COMPRESS=' + compress]
        with gdaltest.error_handler():
            out_ds = gdaltest.rl2_drv.CreateCopy('/vsimem/rl2_20.rl2', src_ds, options = options)
        if out_ds is not None:
            gdaltest.post_reason('Expected error for %s, band=%d, dt=%d, %s, nbits=%s' % (pixel_type, band_count, dt, compress, nbits))
            return 'fail'

    gdal.Unlink('/vsimem/rl2_20.rl2')

    return 'success'

###############################################################################
# Test compression methods

def rl2_21():

    if gdaltest.rl2_drv is None:
        return 'skip'

    tests = [ ('DEFLATE', None),
              ('LZMA', None),
              ('PNG', None),
              ('JPEG', None),
              ('JPEG', 50),
              ('JPEG', 100),
              ('WEBP', None),
              ('WEBP', 50),
              ('WEBP', 100),
              ('CHARLS', None),
              ('JPEG2000', None),
              ('JPEG2000', 50),
              ('JPEG2000', 100) ]

    src_ds = gdal.Open('data/byte.tif')
    for (compress, quality) in tests:

        if gdaltest.rl2_drv.GetMetadataItem('DMD_CREATIONOPTIONLIST').find(compress) < 0:
            print('Skipping test of %s, since it is not available in the run-time librasterlite2' % compress)
            continue

        options = [ 'COMPRESS=' + compress ]
        if quality is not None:
            options += [ 'QUALITY=' + str(quality) ]
        out_ds = gdaltest.rl2_drv.CreateCopy('/vsimem/rl2_21.rl2', src_ds, options = options)
        if out_ds is None:
            gdaltest.post_reason('Got error with %s, quality=%d' % (compress, quality))
            return 'fail'
        if out_ds.GetMetadataItem('COMPRESSION', 'IMAGE_STRUCTURE').find(compress) < 0:
            gdaltest.post_reason('Compression %s does not seem to have been applied' % compress)
            return 'fail'

    gdal.Unlink('/vsimem/rl2_21.rl2')

    return 'success'

###############################################################################
# Test APPEND_SUBDATASET

def rl2_22():

    if gdaltest.rl2_drv is None:
        return 'skip'

    src_ds = gdal.Open('data/byte.tif')

    ds = ogr.GetDriverByName('SQLite').CreateDataSource('/vsimem/rl2_22.rl2', options = ['SPATIALITE=YES'])
    ds.CreateLayer('foo', None, ogr.wkbPoint)
    ds = None
    ds = gdaltest.rl2_drv.CreateCopy('/vsimem/rl2_22.rl2', src_ds, options = ['APPEND_SUBDATASET=YES', 'COVERAGE=byte'])
    if ds.GetRasterBand(1).Checksum() != 4672:
        gdaltest.post_reason('fail')
        print(ds.GetRasterBand(1).Checksum())
        return 'fail'
    ds = None
    ds = gdal.OpenEx('/vsimem/rl2_22.rl2')
    if ds.RasterXSize != 20:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetLayerCount() != 1:
        gdaltest.post_reason('fail')
        return 'fail'

    left_ds = gdal.Translate('left', src_ds, srcWin = [0,0,10,20], format = 'MEM')
    right_ds = gdal.Translate('', src_ds, srcWin = [10,0,10,20], format = 'MEM')

    gdaltest.rl2_drv.CreateCopy('/vsimem/rl2_22.rl2', left_ds, options = ['COVERAGE=left_right'])
    ds = gdaltest.rl2_drv.CreateCopy('/vsimem/rl2_22.rl2', right_ds, options = ['APPEND_SUBDATASET=YES', 'COVERAGE=left_right', 'SECTION=right'])
    if ds.GetRasterBand(1).Checksum() != 4672:
        gdaltest.post_reason('fail')
        print(ds.GetRasterBand(1).Checksum())
        return 'fail'

    src_ds = gdal.Open('data/rgbsmall.tif')
    ds = gdaltest.rl2_drv.CreateCopy('/vsimem/rl2_22.rl2', src_ds, options = ['APPEND_SUBDATASET=YES', 'COVERAGE=rgbsmall'])
    if ds.GetRasterBand(1).Checksum() != src_ds.GetRasterBand(1).Checksum():
        gdaltest.post_reason('fail')
        print(ds.GetRasterBand(1).Checksum())
        return 'fail'
    ds = None

    gdal.Unlink('/vsimem/rl2_22.rl2')

    return 'success'

###############################################################################
# Test BuildOverviews

def rl2_23():

    if gdaltest.rl2_drv is None:
        return 'skip'

    src_ds = gdal.Open('data/byte.tif')
    src_ds = gdal.Translate('', src_ds, format = 'MEM', width = 2048, height = 2048)
    ds = gdaltest.rl2_drv.CreateCopy('/vsimem/rl2_23.rl2', src_ds)
    ret = ds.BuildOverviews('NEAR', [2])
    if ret != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetRasterBand(1).GetOverviewCount() != 5:
        gdaltest.post_reason('fail')
        print(ds.GetRasterBand(1).GetOverviewCount())
        return 'fail'
    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    if cs == 0:
        gdaltest.post_reason('fail')
        print(cs)
        return 'fail'
    ret = ds.BuildOverviews('NONE', [])
    if ret != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = gdal.Open('/vsimem/rl2_23.rl2')
    if ds.GetRasterBand(1).GetOverviewCount() == 5:
        gdaltest.post_reason('fail')
        print(ds.GetRasterBand(1).GetOverviewCount())
        return 'fail'
    ds = None

    gdal.Unlink('/vsimem/rl2_23.rl2')

    return 'success'

###############################################################################
# Test opening a .rl2.sql file

def rl2_24():

    if gdaltest.rl2_drv is None:
        return 'skip'

    if gdal.GetDriverByName('SQLite').GetMetadataItem("ENABLE_SQL_SQLITE_FORMAT") != 'YES':
        return 'skip'

    ds = gdal.Open('data/byte.rl2.sql')
    if ds.GetRasterBand(1).Checksum() != 4672:
        gdaltest.post_reason('validation failed')
        return 'fail'
    return 'success'

gdaltest_list = [
    rl2_1,
    rl2_2,
    rl2_3,
    rl2_4,
    rl2_5,
    rl2_6,
    rl2_7,
    rl2_8,
    rl2_9,
    rl2_10,
    rl2_11,
    rl2_12,
    rl2_13,
    rl2_14,
    rl2_15,
    rl2_16,
    rl2_17,
    rl2_18,
    rl2_19,
    rl2_20,
    rl2_21,
    rl2_22,
    rl2_23,
    rl2_24
]

if __name__ == '__main__':

    gdaltest.setup_run( 'rl2' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()
