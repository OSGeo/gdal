#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id $
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test KEA driver
# Author:   Even Rouault, <even dot rouault at spatialys dot com>
#
###############################################################################
# Copyright (c) 2014, Even Rouault <even dot rouault at spatialys dot com>
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

from osgeo import gdal
from osgeo import osr

import gdaltest
import pytest

###############################################################################


def test_kea_init():
    gdaltest.kea_driver = gdal.GetDriverByName('KEA')

###############################################################################
# Test copying a reference sample with CreateCopy()


def test_kea_1():
    if gdaltest.kea_driver is None:
        pytest.skip()

    tst = gdaltest.GDALTest('KEA', 'byte.tif', 1, 4672, options=['IMAGEBLOCKSIZE=15', 'THEMATIC=YES'])
    return tst.testCreateCopy(check_srs=True, check_gt=1, new_filename='tmp/byte.kea')

###############################################################################
# Test CreateCopy() for various data types


def test_kea_2():
    if gdaltest.kea_driver is None:
        pytest.skip()

    src_files = ['byte.tif',
                 'int16.tif',
                 '../../gcore/data/uint16.tif',
                 '../../gcore/data/int32.tif',
                 '../../gcore/data/uint32.tif',
                 '../../gcore/data/float32.tif',
                 '../../gcore/data/float64.tif']

    for src_file in src_files:
        tst = gdaltest.GDALTest('KEA', src_file, 1, 4672)
        tst.testCreateCopy(check_minmax=1)


###############################################################################
# Test Create() for various data types


def test_kea_3():
    if gdaltest.kea_driver is None:
        pytest.skip()

    src_files = ['byte.tif',
                 'int16.tif',
                 '../../gcore/data/uint16.tif',
                 '../../gcore/data/int32.tif',
                 '../../gcore/data/uint32.tif',
                 '../../gcore/data/float32.tif',
                 '../../gcore/data/float64.tif']

    for src_file in src_files:
        tst = gdaltest.GDALTest('KEA', src_file, 1, 4672)
        tst.testCreate(out_bands=1, check_minmax=1)


###############################################################################
# Test Create()/CreateCopy() error cases or limit cases


def test_kea_4():
    if gdaltest.kea_driver is None:
        pytest.skip()

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = gdaltest.kea_driver.Create("/non_existing_path/non_existing_path", 1, 1)
    gdal.PopErrorHandler()
    assert ds is None

    src_ds = gdaltest.kea_driver.Create('tmp/src.kea', 1, 1, 0)
    assert src_ds is not None
    ds = gdaltest.kea_driver.CreateCopy("tmp/out.kea", src_ds)
    assert ds is not None
    assert ds.RasterCount == 0
    src_ds = None
    ds = None

    # Test updating a read-only file
    ds = gdaltest.kea_driver.Create('tmp/out.kea', 1, 1)
    ds.GetRasterBand(1).Fill(255)
    ds = None
    ds = gdal.Open('tmp/out.kea')

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ret = ds.SetProjection('a')
    gdal.PopErrorHandler()
    assert ret != 0

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ret = ds.SetGeoTransform([1, 2, 3, 4, 5, 6])
    gdal.PopErrorHandler()
    assert ret != 0

    # Disabled for now since some of them cause memory leaks or
    # crash in the HDF5 library finalizer
    if False:  # pylint: disable=using-constant-test
        gdal.PushErrorHandler('CPLQuietErrorHandler')
        ret = ds.SetMetadataItem('foo', 'bar')
        gdal.PopErrorHandler()
        assert ret != 0

        gdal.PushErrorHandler('CPLQuietErrorHandler')
        ret = ds.SetMetadata({'foo': 'bar'})
        gdal.PopErrorHandler()
        assert ret != 0

        gdal.PushErrorHandler('CPLQuietErrorHandler')
        ret = ds.GetRasterBand(1).SetMetadataItem('foo', 'bar')
        gdal.PopErrorHandler()
        assert ret != 0

        gdal.PushErrorHandler('CPLQuietErrorHandler')
        ret = ds.GetRasterBand(1).SetMetadata({'foo': 'bar'})
        gdal.PopErrorHandler()
        assert ret != 0

        gdal.PushErrorHandler('CPLQuietErrorHandler')
        ret = ds.SetGCPs([], "")
        gdal.PopErrorHandler()
        assert ret != 0

    with gdaltest.error_handler():
        ret = ds.AddBand(gdal.GDT_Byte)
    assert ret != 0

    with gdaltest.error_handler():
        ret = ds.GetRasterBand(1).WriteRaster(0, 0, 1, 1, '\0')
    assert ret != 0
    with gdaltest.error_handler():
        ret = ds.FlushCache()
    assert ret != 0
    assert ds.GetRasterBand(1).Checksum() == 3

    ds = None

    gdaltest.kea_driver.Delete('tmp/src.kea')
    gdaltest.kea_driver.Delete('tmp/out.kea')

###############################################################################
# Test Create() creation options


def test_kea_5():
    if gdaltest.kea_driver is None:
        pytest.skip()

    options = ['IMAGEBLOCKSIZE=15', 'ATTBLOCKSIZE=100', 'MDC_NELMTS=10',
               'RDCC_NELMTS=256', 'RDCC_NBYTES=500000', 'RDCC_W0=0.5',
               'SIEVE_BUF=32768', 'META_BLOCKSIZE=1024', 'DEFLATE=9', 'THEMATIC=YES']
    ds = gdaltest.kea_driver.Create("tmp/out.kea", 100, 100, 3, options=options)
    ds = None
    ds = gdal.Open('tmp/out.kea')
    assert ds.GetRasterBand(1).GetBlockSize() == [15, 15]
    assert ds.GetRasterBand(1).GetMetadataItem('LAYER_TYPE') == 'thematic', \
        ds.GetRasterBand(1).GetMetadata()
    assert ds.GetRasterBand(1).Checksum() == 0
    assert ds.GetGeoTransform() == (0, 1, 0, 0, 0, -1)
    assert ds.GetProjectionRef() == ''
    ds = None
    gdaltest.kea_driver.Delete('tmp/out.kea')

###############################################################################
# Test metadata


def test_kea_6():
    if gdaltest.kea_driver is None:
        pytest.skip()

    ds = gdaltest.kea_driver.Create("tmp/out.kea", 1, 1, 5)
    ds.SetMetadata({'foo': 'bar'})
    ds.SetMetadataItem('bar', 'baw')
    ds.GetRasterBand(1).SetMetadata({'bar': 'baz'})
    ds.GetRasterBand(1).SetDescription('desc')
    ds.GetRasterBand(2).SetMetadata({'LAYER_TYPE': 'any_string_that_is_not_athematic_is_thematic'})
    ds.GetRasterBand(3).SetMetadata({'LAYER_TYPE': 'athematic'})
    ds.GetRasterBand(4).SetMetadataItem('LAYER_TYPE', 'thematic')
    ds.GetRasterBand(5).SetMetadataItem('LAYER_TYPE', 'athematic')
    assert ds.SetMetadata({'foo': 'bar'}, 'other_domain') != 0
    assert ds.SetMetadataItem('foo', 'bar', 'other_domain') != 0
    assert ds.GetRasterBand(1).SetMetadata({'foo': 'bar'}, 'other_domain') != 0
    assert ds.GetRasterBand(1).SetMetadataItem('foo', 'bar', 'other_domain') != 0
    ds = None

    ds = gdal.Open('tmp/out.kea')
    assert ds.GetMetadata('other_domain') == {}
    assert ds.GetMetadataItem('item', 'other_domain') is None
    assert ds.GetRasterBand(1).GetMetadata('other_domain') == {}
    assert ds.GetRasterBand(1).GetMetadataItem('item', 'other_domain') is None
    md = ds.GetMetadata()
    assert md['foo'] == 'bar'
    assert ds.GetMetadataItem('foo') == 'bar'
    assert ds.GetMetadataItem('bar') == 'baw'
    assert ds.GetRasterBand(1).GetDescription() == 'desc'
    md = ds.GetRasterBand(1).GetMetadata()
    assert md['bar'] == 'baz'
    assert ds.GetRasterBand(1).GetMetadataItem('bar') == 'baz'
    assert ds.GetRasterBand(2).GetMetadataItem('LAYER_TYPE') == 'thematic'
    assert ds.GetRasterBand(3).GetMetadataItem('LAYER_TYPE') == 'athematic'
    assert ds.GetRasterBand(4).GetMetadataItem('LAYER_TYPE') == 'thematic'
    assert ds.GetRasterBand(5).GetMetadataItem('LAYER_TYPE') == 'athematic'
    out2_ds = gdaltest.kea_driver.CreateCopy('tmp/out2.kea', ds)
    ds = None

    assert out2_ds.GetMetadataItem('foo') == 'bar'
    assert out2_ds.GetRasterBand(1).GetMetadataItem('bar') == 'baz'

    out2_ds = None

    gdaltest.kea_driver.Delete('tmp/out.kea')
    gdaltest.kea_driver.Delete('tmp/out2.kea')

###############################################################################
# Test georef


def test_kea_7():
    if gdaltest.kea_driver is None:
        pytest.skip()

    sr = osr.SpatialReference()
    sr.ImportFromEPSG(4326)

    # Geotransform
    ds = gdaltest.kea_driver.Create("tmp/out.kea", 1, 1)
    assert ds.GetGCPCount() == 0
    assert ds.SetGeoTransform([1, 2, 3, 4, 5, 6]) == 0
    assert ds.SetProjection(sr.ExportToWkt()) == 0
    ds = None

    ds = gdal.Open('tmp/out.kea')
    out2_ds = gdaltest.kea_driver.CreateCopy('tmp/out2.kea', ds)
    ds = None
    assert out2_ds.GetGCPCount() == 0
    assert out2_ds.GetGeoTransform() == (1, 2, 3, 4, 5, 6)
    assert out2_ds.GetProjectionRef() != ''
    out2_ds = None

    gdaltest.kea_driver.Delete('tmp/out.kea')
    gdaltest.kea_driver.Delete('tmp/out2.kea')

    # GCP
    ds = gdaltest.kea_driver.Create("tmp/out.kea", 1, 1)
    gcp1 = gdal.GCP(0, 1, 2, 3, 4)
    gcp1.Id = "id"
    gcp1.Info = "info"
    gcp2 = gdal.GCP(0, 1, 2, 3, 4)
    gcps = [gcp1, gcp2]
    ds.SetGCPs(gcps, sr.ExportToWkt())
    ds = None

    ds = gdal.Open('tmp/out.kea')
    out2_ds = gdaltest.kea_driver.CreateCopy('tmp/out2.kea', ds)
    ds = None

    assert out2_ds.GetGCPCount() == 2
    assert out2_ds.GetGCPProjection() != ''
    got_gcps = out2_ds.GetGCPs()
    for i in range(2):
        assert (got_gcps[i].GCPX == gcps[i].GCPX and got_gcps[i].GCPY == gcps[i].GCPY and \
           got_gcps[i].GCPZ == gcps[i].GCPZ and got_gcps[i].GCPPixel == gcps[i].GCPPixel and \
           got_gcps[i].GCPLine == gcps[i].GCPLine and got_gcps[i].Id == gcps[i].Id and \
           got_gcps[i].Info == gcps[i].Info)
    out2_ds = None

    gdaltest.kea_driver.Delete('tmp/out.kea')
    gdaltest.kea_driver.Delete('tmp/out2.kea')

###############################################################################
# Test colortable


def test_kea_8():
    if gdaltest.kea_driver is None:
        pytest.skip()

    for i in range(2):
        ds = gdaltest.kea_driver.Create("tmp/out.kea", 1, 1)
        assert ds.GetRasterBand(1).GetColorTable() is None
        assert ds.GetRasterBand(1).SetColorTable(None) != 0
        ct = gdal.ColorTable()
        ct.SetColorEntry(0, (0, 255, 0, 255))
        ct.SetColorEntry(1, (255, 0, 255, 255))
        ct.SetColorEntry(2, (0, 0, 255, 255))
        assert ds.GetRasterBand(1).SetColorTable(ct) == 0
        if i == 1:
            # And again
            assert ds.GetRasterBand(1).SetColorTable(ct) == 0
        ds = None

        ds = gdal.Open('tmp/out.kea')
        out2_ds = gdaltest.kea_driver.CreateCopy('tmp/out2.kea', ds)
        ds = None
        got_ct = out2_ds.GetRasterBand(1).GetColorTable()
        assert got_ct.GetCount() == 3, 'Got wrong color table entry count.'
        assert got_ct.GetColorEntry(1) == (255, 0, 255, 255), \
            'Got wrong color table entry.'

        out2_ds = None

        gdaltest.kea_driver.Delete('tmp/out.kea')
        gdaltest.kea_driver.Delete('tmp/out2.kea')


###############################################################################
# Test color interpretation


def test_kea_9():
    if gdaltest.kea_driver is None:
        pytest.skip()

    ds = gdaltest.kea_driver.Create("tmp/out.kea", 1, 1, gdal.GCI_YCbCr_CrBand - gdal.GCI_GrayIndex + 1)
    assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_GrayIndex
    for i in range(gdal.GCI_GrayIndex, gdal.GCI_YCbCr_CrBand + 1):
        ds.GetRasterBand(i).SetColorInterpretation(i)
    ds = None

    ds = gdal.Open('tmp/out.kea')
    out2_ds = gdaltest.kea_driver.CreateCopy('tmp/out2.kea', ds)
    ds = None
    for i in range(gdal.GCI_GrayIndex, gdal.GCI_YCbCr_CrBand + 1):
        assert out2_ds.GetRasterBand(i).GetColorInterpretation() == i, \
            'Got wrong color interpretation.'

    out2_ds = None

    gdaltest.kea_driver.Delete('tmp/out.kea')
    gdaltest.kea_driver.Delete('tmp/out2.kea')

###############################################################################
# Test nodata


def test_kea_10():
    if gdaltest.kea_driver is None:
        pytest.skip()

    for (dt, nd, expected_nd) in [(gdal.GDT_Byte, 0, 0),
                                  (gdal.GDT_Byte, 1.1, 1.0),
                                  (gdal.GDT_Byte, 255, 255),
                                  (gdal.GDT_Byte, -1, None),
                                  (gdal.GDT_Byte, 256, None),
                                  (gdal.GDT_UInt16, 0, 0),
                                  (gdal.GDT_UInt16, 65535, 65535),
                                  (gdal.GDT_UInt16, -1, None),
                                  (gdal.GDT_UInt16, 65536, None),
                                  (gdal.GDT_Int16, -32768, -32768),
                                  (gdal.GDT_Int16, 32767, 32767),
                                  (gdal.GDT_Int16, -32769, None),
                                  (gdal.GDT_Int16, 32768, None),
                                  (gdal.GDT_UInt32, 0, 0),
                                  (gdal.GDT_UInt32, 0xFFFFFFFF, 0xFFFFFFFF),
                                  (gdal.GDT_UInt32, -1, None),
                                  (gdal.GDT_UInt32, 0xFFFFFFFF + 1, None),
                                  (gdal.GDT_Int32, -2147483648, -2147483648),
                                  (gdal.GDT_Int32, 2147483647, 2147483647),
                                  (gdal.GDT_Int32, -2147483649, None),
                                  (gdal.GDT_Int32, 2147483648, None),
                                  (gdal.GDT_Float32, 0.5, 0.5),
                                  ]:
        ds = gdaltest.kea_driver.Create("tmp/out.kea", 1, 1, 1, dt)
        assert ds.GetRasterBand(1).GetNoDataValue() is None
        ds.GetRasterBand(1).SetNoDataValue(nd)
        if ds.GetRasterBand(1).GetNoDataValue() != expected_nd:
            print(dt)
            pytest.fail('Got wrong nodata.')
        ds = None

        ds = gdal.Open('tmp/out.kea')
        out2_ds = gdaltest.kea_driver.CreateCopy('tmp/out2.kea', ds)
        ds = None
        if out2_ds.GetRasterBand(1).GetNoDataValue() != expected_nd:
            print(dt)
            pytest.fail('Got wrong nodata.')
        out2_ds.GetRasterBand(1).DeleteNoDataValue()
        out2_ds = None

        ds = gdal.Open('tmp/out2.kea')
        assert ds.GetRasterBand(1).GetNoDataValue() is None
        ds = None

        gdaltest.kea_driver.Delete('tmp/out.kea')
        gdaltest.kea_driver.Delete('tmp/out2.kea')


###############################################################################
# Test AddBand


def test_kea_11():
    if gdaltest.kea_driver is None:
        pytest.skip()

    ds = gdaltest.kea_driver.Create("tmp/out.kea", 1, 1, 1, gdal.GDT_Byte)
    ds = None

    ds = gdal.Open('tmp/out.kea', gdal.GA_Update)
    assert ds.AddBand(gdal.GDT_Byte) == 0
    assert ds.AddBand(gdal.GDT_Int16, options=['DEFLATE=9']) == 0
    ds = None

    ds = gdal.Open('tmp/out.kea')
    assert ds.RasterCount == 3
    assert ds.GetRasterBand(2).DataType == gdal.GDT_Byte
    assert ds.GetRasterBand(3).DataType == gdal.GDT_Int16
    ds = None

    gdaltest.kea_driver.Delete('tmp/out.kea')

###############################################################################
# Test RAT


def test_kea_12():
    if gdaltest.kea_driver is None:
        pytest.skip()

    ds = gdaltest.kea_driver.Create("tmp/out.kea", 1, 1, 1, gdal.GDT_Byte)
    assert ds.GetRasterBand(1).GetDefaultRAT().GetColumnCount() == 0
    assert ds.GetRasterBand(1).SetDefaultRAT(None) != 0
    rat = ds.GetRasterBand(1).GetDefaultRAT()
    rat.CreateColumn('col_real_generic', gdal.GFT_Real, gdal.GFU_Generic)
    assert ds.GetRasterBand(1).SetDefaultRAT(rat) == 0
    rat = ds.GetRasterBand(1).GetDefaultRAT()
    rat.CreateColumn('col_integer_pixelcount', gdal.GFT_Real, gdal.GFU_PixelCount)
    rat.CreateColumn('col_string_name', gdal.GFT_String, gdal.GFU_Name)
    rat.CreateColumn('col_integer_red', gdal.GFT_Integer, gdal.GFU_Red)
    rat.CreateColumn('col_integer_green', gdal.GFT_Integer, gdal.GFU_Green)
    rat.CreateColumn('col_integer_blue', gdal.GFT_Integer, gdal.GFU_Blue)
    rat.CreateColumn('col_integer_alpha', gdal.GFT_Integer, gdal.GFU_Alpha)
    rat.SetRowCount(1)

    rat.SetValueAsString(0, 0, "1.23")
    rat.SetValueAsInt(0, 0, 1)
    rat.SetValueAsDouble(0, 0, 1.23)

    rat.SetValueAsInt(0, 2, 0)
    rat.SetValueAsDouble(0, 2, 0)
    rat.SetValueAsString(0, 2, 'foo')

    rat.SetValueAsString(0, 3, "123")
    rat.SetValueAsDouble(0, 3, 123)
    rat.SetValueAsInt(0, 3, 123)

    cloned_rat = rat.Clone()
    assert ds.GetRasterBand(1).SetDefaultRAT(rat) == 0
    ds = None

    ds = gdal.Open('tmp/out.kea')
    out2_ds = gdaltest.kea_driver.CreateCopy('tmp/out2.kea', ds)
    rat = out2_ds.GetRasterBand(1).GetDefaultRAT()

    for i in range(7):
        assert rat.GetColOfUsage(rat.GetUsageOfCol(i)) == i

    assert cloned_rat.GetNameOfCol(0) == 'col_real_generic'
    assert cloned_rat.GetTypeOfCol(0) == gdal.GFT_Real
    assert cloned_rat.GetUsageOfCol(0) == gdal.GFU_Generic
    assert cloned_rat.GetUsageOfCol(1) == gdal.GFU_PixelCount
    assert cloned_rat.GetTypeOfCol(2) == gdal.GFT_String
    assert cloned_rat.GetTypeOfCol(3) == gdal.GFT_Integer

    assert rat.GetColumnCount() == cloned_rat.GetColumnCount()
    assert rat.GetRowCount() == cloned_rat.GetRowCount()
    for i in range(rat.GetColumnCount()):
        assert rat.GetNameOfCol(i) == cloned_rat.GetNameOfCol(i)
        assert rat.GetTypeOfCol(i) == cloned_rat.GetTypeOfCol(i)
        assert rat.GetUsageOfCol(i) == cloned_rat.GetUsageOfCol(i)

    gdal.PushErrorHandler('CPLQuietErrorHandler')

    rat.GetNameOfCol(-1)
    rat.GetTypeOfCol(-1)
    rat.GetUsageOfCol(-1)

    rat.GetNameOfCol(rat.GetColumnCount())
    rat.GetTypeOfCol(rat.GetColumnCount())
    rat.GetUsageOfCol(rat.GetColumnCount())

    rat.GetValueAsDouble(-1, 0)
    rat.GetValueAsInt(-1, 0)
    rat.GetValueAsString(-1, 0)

    rat.GetValueAsDouble(rat.GetColumnCount(), 0)
    rat.GetValueAsInt(rat.GetColumnCount(), 0)
    rat.GetValueAsString(rat.GetColumnCount(), 0)

    rat.GetValueAsDouble(0, -1)
    rat.GetValueAsInt(0, -1)
    rat.GetValueAsString(0, -1)

    rat.GetValueAsDouble(0, rat.GetRowCount())
    rat.GetValueAsInt(0, rat.GetRowCount())
    rat.GetValueAsString(0, rat.GetRowCount())

    gdal.PopErrorHandler()

    assert rat.GetValueAsDouble(0, 0) == 1.23
    assert rat.GetValueAsInt(0, 0) == 1
    assert rat.GetValueAsString(0, 0) == '1.23'

    assert rat.GetValueAsInt(0, 3) == 123
    assert rat.GetValueAsDouble(0, 3) == 123
    assert rat.GetValueAsString(0, 3) == '123'

    assert rat.GetValueAsString(0, 2) == 'foo'
    assert rat.GetValueAsInt(0, 2) == 0
    assert rat.GetValueAsDouble(0, 2) == 0

    ds = None
    out2_ds = None

    gdaltest.kea_driver.Delete('tmp/out.kea')
    gdaltest.kea_driver.Delete('tmp/out2.kea')

###############################################################################
# Test overviews


def test_kea_13():
    if gdaltest.kea_driver is None:
        pytest.skip()

    src_ds = gdal.Open('data/byte.tif')
    ds = gdaltest.kea_driver.CreateCopy("tmp/out.kea", src_ds)
    src_ds = None
    ds.BuildOverviews('NEAR', [2])
    ds = None
    ds = gdal.Open('tmp/out.kea')
    out2_ds = gdaltest.kea_driver.CreateCopy('tmp/out2.kea', ds)  # yes CreateCopy() of KEA copies overviews
    assert out2_ds.GetRasterBand(1).GetOverviewCount() == 1
    assert out2_ds.GetRasterBand(1).GetOverview(0).Checksum() == 1087
    assert out2_ds.GetRasterBand(1).GetOverview(0).GetDefaultRAT() is None
    assert out2_ds.GetRasterBand(1).GetOverview(0).SetDefaultRAT(None) != 0
    assert out2_ds.GetRasterBand(1).GetOverview(-1) is None
    assert out2_ds.GetRasterBand(1).GetOverview(1) is None
    out2_ds = None
    ds = None

    gdaltest.kea_driver.Delete('tmp/out.kea')
    gdaltest.kea_driver.Delete('tmp/out2.kea')

###############################################################################
# Test mask bands


def test_kea_14():
    if gdaltest.kea_driver is None:
        pytest.skip()

    ds = gdaltest.kea_driver.Create("tmp/out.kea", 1, 1, 1, gdal.GDT_Byte)
    assert ds.GetRasterBand(1).GetMaskFlags() == gdal.GMF_ALL_VALID
    assert ds.GetRasterBand(1).GetMaskBand().Checksum() == 3
    ds.GetRasterBand(1).CreateMaskBand(0)
    assert ds.GetRasterBand(1).GetMaskFlags() == 0
    assert ds.GetRasterBand(1).GetMaskBand().IsMaskBand()
    assert ds.GetRasterBand(1).GetMaskBand().Checksum() == 3
    ds.GetRasterBand(1).GetMaskBand().Fill(0)
    assert ds.GetRasterBand(1).GetMaskBand().Checksum() == 0
    ds = None

    ds = gdal.Open('tmp/out.kea')
    out2_ds = gdaltest.kea_driver.CreateCopy('tmp/out2.kea', ds)  # yes CreateCopy() of KEA copies overviews
    assert out2_ds.GetRasterBand(1).GetMaskFlags() == 0
    assert out2_ds.GetRasterBand(1).GetMaskBand().Checksum() == 0
    out2_ds = None
    ds = None

    gdaltest.kea_driver.Delete('tmp/out.kea')
    gdaltest.kea_driver.Delete('tmp/out2.kea')


###############################################################################
# Test /vsi functionality


def test_kea_15():
    if gdaltest.kea_driver is None:
        pytest.skip()

    # create an temp image
    ds = gdaltest.kea_driver.Create('tmp/vsitest.kea', 1, 1)
    ds.GetRasterBand(1).Fill(255)
    ds = None

    # load it into /vsimem and try and open it
    gdal.FileFromMemBuffer('/vsimem/foo.kea', open('tmp/vsitest.kea', 'rb').read())
    ds = gdal.Open('/vsimem/foo.kea')
    assert ds.GetDriver().ShortName == "KEA"
    ds = None

    gdal.Unlink('/vsimem/foo.kea')
    gdaltest.kea_driver.Delete('tmp/vsitest.kea')

def test_kea_destroy():
    # there is always a 'tmp/out.kea.aux.xml' left behind by
    # a few of the tests
    gdal.Unlink('tmp/out.kea.aux.xml')
