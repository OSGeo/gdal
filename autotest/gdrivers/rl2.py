#!/usr/bin/env pytest
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

from osgeo import gdal, ogr


import gdaltest
import pytest

###############################################################################
# Get the rl2 driver


def test_rl2_1():

    gdaltest.rl2_drv = gdal.GetDriverByName('SQLite')
    if gdaltest.rl2_drv is None:
        pytest.skip()
    if gdaltest.rl2_drv.GetMetadataItem('DCAP_RASTER') is None:
        gdaltest.rl2_drv = None
        pytest.skip()

    
###############################################################################
# Test opening a rl2 DB gray level


def test_rl2_2():

    if gdaltest.rl2_drv is None:
        pytest.skip()

    ds = gdal.Open('data/rasterlite2/byte.rl2')

    assert ds.RasterCount == 1, 'expected 1 band'

    assert ds.GetRasterBand(1).GetOverviewCount() == 0, 'did not expect overview'

    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 4672

    gt = ds.GetGeoTransform()
    expected_gt = (440720.0, 60.0, 0.0, 3751320.0, 0.0, -60.0)
    for i in range(6):
        assert gt[i] == pytest.approx(expected_gt[i], abs=1e-15)

    wkt = ds.GetProjectionRef()
    assert '26711' in wkt

    assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_GrayIndex

    assert ds.GetRasterBand(1).GetMinimum() == 74

    assert ds.GetRasterBand(1).GetOverview(-1) is None

    assert ds.GetRasterBand(1).GetOverview(0) is None

    subds = ds.GetSubDatasets()
    expected_subds = []
    assert subds == expected_subds

    gdal.SetConfigOption('RL2_SHOW_ALL_PYRAMID_LEVELS', 'YES')
    ds = gdal.Open('data/rasterlite2/byte.rl2')
    gdal.SetConfigOption('RL2_SHOW_ALL_PYRAMID_LEVELS', None)

    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    assert cs == 1087

###############################################################################
# Test opening a rl2 DB gray level


def test_rl2_3():

    if gdaltest.rl2_drv is None:
        pytest.skip()

    ds = gdal.Open('data/rasterlite2/small_world.rl2')

    assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_RedBand

    ds.GetRasterBand(1).GetNoDataValue()

    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 25550

    cs = ds.GetRasterBand(2).Checksum()
    assert cs == 28146

    assert ds.GetRasterBand(1).GetOverviewCount() == 2

    cs = ds.GetRasterBand(1).GetOverview(1).Checksum()
    assert cs == 51412

    subds = ds.GetSubDatasets()
    expected_subds = [('RASTERLITE2:data/rasterlite2/small_world.rl2:small_world:1:world_west', 'Coverage small_world, section world_west / 1'), ('RASTERLITE2:data/rasterlite2/small_world.rl2:small_world:2:world_east', 'Coverage small_world, section world_east / 2')]
    assert subds == expected_subds

    ds = gdal.Open('RASTERLITE2:data/rasterlite2/small_world.rl2:small_world:1:world_west')

    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 3721

    assert ds.GetRasterBand(1).GetOverviewCount() == 1

    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    assert cs == 35686

###############################################################################
# Test opening a rl2 DB paletted


def test_rl2_4():

    if gdaltest.rl2_drv is None:
        pytest.skip()

    ds = gdal.Open('data/rasterlite2/small_world_pct.rl2')

    assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_PaletteIndex

    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 14890

    pct = ds.GetRasterBand(1).GetColorTable()
    assert pct.GetCount() == 256
    assert pct.GetColorEntry(1) == (176, 184, 176, 255)

    pct = ds.GetRasterBand(1).GetColorTable()
    assert pct.GetCount() == 256

    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    assert cs == 35614

###############################################################################
# Test opening a rl2 DB with various data types


def test_rl2_5():

    if gdaltest.rl2_drv is None:
        pytest.skip()

    ds = gdal.Open('data/rasterlite2/multi_type.rl2')

    subds = ds.GetSubDatasets()
    expected_subds = [('RASTERLITE2:data/rasterlite2/multi_type.rl2:uint8', 'Coverage uint8'), ('RASTERLITE2:data/rasterlite2/multi_type.rl2:int8', 'Coverage int8'), ('RASTERLITE2:data/rasterlite2/multi_type.rl2:uint16', 'Coverage uint16'), ('RASTERLITE2:data/rasterlite2/multi_type.rl2:int16', 'Coverage int16'), ('RASTERLITE2:data/rasterlite2/multi_type.rl2:uint32', 'Coverage uint32'), ('RASTERLITE2:data/rasterlite2/multi_type.rl2:int32', 'Coverage int32'), ('RASTERLITE2:data/rasterlite2/multi_type.rl2:float', 'Coverage float'), ('RASTERLITE2:data/rasterlite2/multi_type.rl2:double', 'Coverage double'), ('RASTERLITE2:data/rasterlite2/multi_type.rl2:1bit', 'Coverage 1bit'), ('RASTERLITE2:data/rasterlite2/multi_type.rl2:2bit', 'Coverage 2bit'), ('RASTERLITE2:data/rasterlite2/multi_type.rl2:4bit', 'Coverage 4bit')]
    assert subds == expected_subds

    tests = [('RASTERLITE2:data/rasterlite2/multi_type.rl2:uint8', gdal.GDT_Byte, 4672),
             ('RASTERLITE2:data/rasterlite2/multi_type.rl2:int8', gdal.GDT_Byte, 4575),
             ('RASTERLITE2:data/rasterlite2/multi_type.rl2:uint16', gdal.GDT_UInt16, 4457),
             ('RASTERLITE2:data/rasterlite2/multi_type.rl2:int16', gdal.GDT_Int16, 4457),
             ('RASTERLITE2:data/rasterlite2/multi_type.rl2:uint32', gdal.GDT_UInt32, 4457),
             ('RASTERLITE2:data/rasterlite2/multi_type.rl2:int32', gdal.GDT_Int32, 4457),
             ('RASTERLITE2:data/rasterlite2/multi_type.rl2:float', gdal.GDT_Float32, 4457),
             ('RASTERLITE2:data/rasterlite2/multi_type.rl2:double', gdal.GDT_Float64, 4457),
             ('RASTERLITE2:data/rasterlite2/multi_type.rl2:1bit', gdal.GDT_Byte, 4873)]
    for (subds_name, dt, expected_cs) in tests:
        ds = gdal.Open(subds_name)
        assert ds.GetRasterBand(1).DataType == dt, subds_name
        cs = ds.GetRasterBand(1).Checksum()
        assert cs == expected_cs
        if subds_name == 'RASTERLITE2:data/rasterlite2/multi_type.rl2:int8':
            assert ds.GetRasterBand(1).GetMetadataItem('PIXELTYPE', 'IMAGE_STRUCTURE') == 'SIGNEDBYTE'

    
###############################################################################
# Test CreateCopy() on a grayscale uint8


def test_rl2_6():

    if gdaltest.rl2_drv is None:
        pytest.skip()

    tst = gdaltest.GDALTest('SQLite', 'byte.tif', 1, 4672)
    return tst.testCreateCopy(vsimem=1, check_minmax=False)

###############################################################################
# Test CreateCopy() on a RGB


def test_rl2_7():

    if gdaltest.rl2_drv is None:
        pytest.skip()

    tst = gdaltest.GDALTest('SQLite', 'small_world.tif', 1, 30111, options=['COMPRESS=PNG'])
    return tst.testCreateCopy(vsimem=1)

###############################################################################
# Test CreateCopy() on a paletted dataset


def test_rl2_8():

    if gdaltest.rl2_drv is None:
        pytest.skip()

    tst = gdaltest.GDALTest('SQLite', 'small_world_pct.tif', 1, 14890, options=['COMPRESS=PNG'])
    return tst.testCreateCopy(vsimem=1, check_minmax=False)

###############################################################################
# Test CreateCopy() on a DATAGRID uint16


def test_rl2_9():

    if gdaltest.rl2_drv is None:
        pytest.skip()

    tst = gdaltest.GDALTest('SQLite', '../../gcore/data/uint16.tif', 1, 4672)
    return tst.testCreateCopy(vsimem=1)

###############################################################################
# Test CreateCopy() on a DATAGRID int16


def test_rl2_10():

    if gdaltest.rl2_drv is None:
        pytest.skip()

    tst = gdaltest.GDALTest('SQLite', '../../gcore/data/int16.tif', 1, 4672)
    return tst.testCreateCopy(vsimem=1)

###############################################################################
# Test CreateCopy() on a DATAGRID uint32


def test_rl2_11():

    if gdaltest.rl2_drv is None:
        pytest.skip()

    tst = gdaltest.GDALTest('SQLite', '../../gcore/data/uint32.tif', 1, 4672)
    return tst.testCreateCopy(vsimem=1)

###############################################################################
# Test CreateCopy() on a DATAGRID int32


def test_rl2_12():

    if gdaltest.rl2_drv is None:
        pytest.skip()

    tst = gdaltest.GDALTest('SQLite', '../../gcore/data/int32.tif', 1, 4672)
    return tst.testCreateCopy(vsimem=1)

###############################################################################
# Test CreateCopy() on a DATAGRID float


def test_rl2_13():

    if gdaltest.rl2_drv is None:
        pytest.skip()

    tst = gdaltest.GDALTest('SQLite', '../../gcore/data/float32.tif', 1, 4672)
    return tst.testCreateCopy(vsimem=1)

###############################################################################
# Test CreateCopy() on a DATAGRID double


def test_rl2_14():

    if gdaltest.rl2_drv is None:
        pytest.skip()

    tst = gdaltest.GDALTest('SQLite', '../../gcore/data/float64.tif', 1, 4672)
    return tst.testCreateCopy(vsimem=1)

###############################################################################
# Test CreateCopy() on a 1 bit paletted


def test_rl2_15():

    if gdaltest.rl2_drv is None:
        pytest.skip()

    tst = gdaltest.GDALTest('SQLite', '../../gcore/data/1bit.bmp', 1, 200)
    return tst.testCreateCopy(vsimem=1, check_minmax=False)

###############################################################################
# Test CreateCopy() on a forced 1 bit


def test_rl2_16():

    if gdaltest.rl2_drv is None:
        pytest.skip()

    tst = gdaltest.GDALTest('SQLite', 'byte.tif', 1, 4873, options=['NBITS=1', 'COMPRESS=CCITTFAX4'])
    return tst.testCreateCopy(vsimem=1, check_minmax=False)

###############################################################################
# Test CreateCopy() on a forced 2 bit


def test_rl2_17():

    if gdaltest.rl2_drv is None:
        pytest.skip()

    tst = gdaltest.GDALTest('SQLite', 'byte.tif', 1, 4873, options=['NBITS=2', 'COMPRESS=DEFLATE'])
    return tst.testCreateCopy(vsimem=1, check_minmax=False)

###############################################################################
# Test CreateCopy() on a forced 4 bit


def test_rl2_18():

    if gdaltest.rl2_drv is None:
        pytest.skip()

    tst = gdaltest.GDALTest('SQLite', 'byte.tif', 1, 2541, options=['NBITS=4'])
    return tst.testCreateCopy(vsimem=1, check_minmax=False)

###############################################################################
# Test CreateCopy() with forced monochrome


def test_rl2_19():

    if gdaltest.rl2_drv is None:
        pytest.skip()

    tst = gdaltest.GDALTest('SQLite', 'byte.tif', 1, 4873, options=['PIXEL_TYPE=MONOCHROME'])
    return tst.testCreateCopy(vsimem=1, check_minmax=False)

###############################################################################
# Test incompatibilities on CreateCopy()
# Se https://www.gaia-gis.it/fossil/librasterlite2/wiki?name=reference_table


def test_rl2_20():

    if gdaltest.rl2_drv is None:
        pytest.skip()

    tests = [('MONOCHROME', 2, gdal.GDT_Byte, 'NONE', None, None),
             ('MONOCHROME', 1, gdal.GDT_UInt16, 'NONE', None, None),
             ('PALETTE', 1, gdal.GDT_Byte, 'NONE', None, None),
             ('PALETTE', 1, gdal.GDT_UInt16, 'NONE', None, gdal.ColorTable()),
             ('GRAYSCALE', 2, gdal.GDT_Byte, 'NONE', None, None),
             ('GRAYSCALE', 1, gdal.GDT_UInt16, 'NONE', None, None),
             ('RGB', 1, gdal.GDT_Byte, 'NONE', None, None),
             ('RGB', 3, gdal.GDT_Int16, 'NONE', None, None),
             ('MULTIBAND', 1, gdal.GDT_Byte, 'NONE', None, None),
             ('MULTIBAND', 256, gdal.GDT_Byte, 'NONE', None, None),
             ('MULTIBAND', 2, gdal.GDT_Int16, 'NONE', None, None),
             ('DATAGRID', 2, gdal.GDT_Byte, 'NONE', None, None),
             ('DATAGRID', 1, gdal.GDT_CFloat32, 'NONE', None, None),
             ('MONOCHROME', 1, gdal.GDT_Byte, 'JPEG', None, None),
             ('PALETTE', 1, gdal.GDT_Byte, 'JPEG', None, gdal.ColorTable()),
             ('GRAYSCALE', 1, gdal.GDT_Byte, 'CCITTFAX4', None, None),
             ('RGB', 3, gdal.GDT_Byte, 'CCITTFAX4', None, None),
             ('RGB', 3, gdal.GDT_UInt16, 'JPEG', None, None),
             ('MULTIBAND', 3, gdal.GDT_Byte, 'CCITTFAX4', None, None),
             ('MULTIBAND', 3, gdal.GDT_UInt16, 'CCITTFAX4', None, None),
             ('MULTIBAND', 2, gdal.GDT_Byte, 'CCITTFAX4', None, None),
             ('DATAGRID', 1, gdal.GDT_Byte, 'CCITTFAX4', None, None),
             ('DATAGRID', 1, gdal.GDT_Int16, 'CCITTFAX4', None, None), ]

    for (pixel_type, band_count, dt, compress, nbits, pct) in tests:
        src_ds = gdal.GetDriverByName('MEM').Create('', 1, 1, band_count, dt)
        if pct is not None:
            src_ds.GetRasterBand(1).SetColorTable(pct)
        if nbits is not None:
            src_ds.GetRasterBand(1).SetMetadataItem('NBITS', nbits, 'IMAGE_STRUCTURE')
        options = ['PIXEL_TYPE=' + pixel_type, 'COMPRESS=' + compress]
        with gdaltest.error_handler():
            out_ds = gdaltest.rl2_drv.CreateCopy('/vsimem/rl2_20.rl2', src_ds, options=options)
        assert out_ds is None, \
            ('Expected error for %s, band=%d, dt=%d, %s, nbits=%s' % (pixel_type, band_count, dt, compress, nbits))

    gdal.Unlink('/vsimem/rl2_20.rl2')

###############################################################################
# Test compression methods


def test_rl2_21():

    if gdaltest.rl2_drv is None:
        pytest.skip()

    tests = [('DEFLATE', None),
             ('LZMA', None),
             ('PNG', None),
             ('JPEG', None),
             ('JPEG', 50),
             ('JPEG', 100),
             ('WEBP', None),
             ('WEBP', 50),
             ('WEBP', 100),
             ('JPEG2000', None),
             ('JPEG2000', 50),
             ('JPEG2000', 100)]

    src_ds = gdal.Open('data/byte.tif')
    for (compress, quality) in tests:

        if gdaltest.rl2_drv.GetMetadataItem('DMD_CREATIONOPTIONLIST').find(compress) < 0:
            print('Skipping test of %s, since it is not available in the run-time librasterlite2' % compress)
            continue

        options = ['COMPRESS=' + compress]
        if quality is not None:
            options += ['QUALITY=' + str(quality)]
        out_ds = gdaltest.rl2_drv.CreateCopy('/vsimem/rl2_21.rl2', src_ds, options=options)
        assert out_ds is not None, \
            ('Got error with %s, quality=%d' % (compress, quality))
        assert out_ds.GetMetadataItem('COMPRESSION', 'IMAGE_STRUCTURE').find(compress) >= 0, \
            ('Compression %s does not seem to have been applied' % compress)

    gdal.Unlink('/vsimem/rl2_21.rl2')

###############################################################################
# Test APPEND_SUBDATASET


def test_rl2_22():

    if gdaltest.rl2_drv is None:
        pytest.skip()

    src_ds = gdal.Open('data/byte.tif')

    ds = ogr.GetDriverByName('SQLite').CreateDataSource('/vsimem/rl2_22.rl2', options=['SPATIALITE=YES'])
    ds.CreateLayer('foo', None, ogr.wkbPoint)
    ds = None
    ds = gdaltest.rl2_drv.CreateCopy('/vsimem/rl2_22.rl2', src_ds, options=['APPEND_SUBDATASET=YES', 'COVERAGE=byte'])
    assert ds.GetRasterBand(1).Checksum() == 4672
    ds = None
    ds = gdal.OpenEx('/vsimem/rl2_22.rl2')
    assert ds.RasterXSize == 20
    assert ds.GetLayerCount() == 1

    left_ds = gdal.Translate('left', src_ds, srcWin=[0, 0, 10, 20], format='MEM')
    right_ds = gdal.Translate('', src_ds, srcWin=[10, 0, 10, 20], format='MEM')

    gdaltest.rl2_drv.CreateCopy('/vsimem/rl2_22.rl2', left_ds, options=['COVERAGE=left_right'])
    ds = gdaltest.rl2_drv.CreateCopy('/vsimem/rl2_22.rl2', right_ds, options=['APPEND_SUBDATASET=YES', 'COVERAGE=left_right', 'SECTION=right'])
    assert ds.GetRasterBand(1).Checksum() == 4672

    src_ds = gdal.Open('data/rgbsmall.tif')
    ds = gdaltest.rl2_drv.CreateCopy('/vsimem/rl2_22.rl2', src_ds, options=['APPEND_SUBDATASET=YES', 'COVERAGE=rgbsmall'])
    assert ds.GetRasterBand(1).Checksum() == src_ds.GetRasterBand(1).Checksum()
    ds = None

    gdal.Unlink('/vsimem/rl2_22.rl2')

###############################################################################
# Test BuildOverviews


def test_rl2_23():

    if gdaltest.rl2_drv is None:
        pytest.skip()

    src_ds = gdal.Open('data/byte.tif')
    src_ds = gdal.Translate('', src_ds, format='MEM', width=2048, height=2048)
    ds = gdaltest.rl2_drv.CreateCopy('/vsimem/rl2_23.rl2', src_ds)
    ret = ds.BuildOverviews('NEAR', [2])
    assert ret == 0
    assert ds.GetRasterBand(1).GetOverviewCount() == 5
    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    assert cs != 0
    ret = ds.BuildOverviews('NONE', [])
    assert ret == 0
    ds = gdal.Open('/vsimem/rl2_23.rl2')
    assert ds.GetRasterBand(1).GetOverviewCount() != 5
    ds = None

    gdal.Unlink('/vsimem/rl2_23.rl2')

###############################################################################
# Test opening a .rl2.sql file


def test_rl2_24():

    if gdaltest.rl2_drv is None:
        pytest.skip()

    if gdal.GetDriverByName('SQLite').GetMetadataItem("ENABLE_SQL_SQLITE_FORMAT") != 'YES':
        pytest.skip()

    ds = gdal.Open('data/rasterlite2/byte.rl2.sql')
    assert ds.GetRasterBand(1).Checksum() == 4672, 'validation failed'

###############################################################################
# Test Create()


def test_rl2_error_create():

    if gdaltest.rl2_drv is None:
        pytest.skip()

    with gdaltest.error_handler():
        assert gdaltest.rl2_drv.Create('/vsimem/out.db', 1, 1) is None
    


