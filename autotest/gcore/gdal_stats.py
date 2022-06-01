#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test core numeric operations and statistics calculations
# Author:   Mateusz Loskot <mateusz@loskot.net>
#
###############################################################################
# Copyright (c) 2007, Mateusz Loskot <mateusz@loskot.net>
# Copyright (c) 2009-2012, Even Rouault <even dot rouault at spatialys.com>
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Library General Public
# License along with this library; if not, write to the
# Free Software Foundation, Inc., 59 Temple Place - Suite 330,
# Boston, MA 02111-1307, USA.
###############################################################################

import os
import struct
import shutil


import gdaltest
from osgeo import gdal
import pytest

###############################################################################
# Test handling NaN with GDT_Float32 data


def test_stats_nan_1():

    gdaltest.gtiff_drv = gdal.GetDriverByName('GTiff')
    if gdaltest.gtiff_drv is None:
        pytest.skip()

    stats = (50.0, 58.0, 54.0, 2.5819888974716)

    shutil.copyfile('data/nan32.tif', 'tmp/nan32.tif')

    t = gdaltest.GDALTest('GTiff', 'tmp/nan32.tif', 1, 874, filename_absolute=1)
    ret = t.testOpen(check_approx_stat=stats, check_stat=stats)

    gdal.GetDriverByName('GTiff').Delete('tmp/nan32.tif')

    return ret

###############################################################################
# Test handling NaN with GDT_Float64 data


def test_stats_nan_2():

    if gdaltest.gtiff_drv is None:
        pytest.skip()

    stats = (50.0, 58.0, 54.0, 2.5819888974716)

    shutil.copyfile('data/nan64.tif', 'tmp/nan64.tif')

    t = gdaltest.GDALTest('GTiff', 'tmp/nan64.tif', 1, 4414, filename_absolute=1)
    ret = t.testOpen(check_approx_stat=stats, check_stat=stats)

    gdal.GetDriverByName('GTiff').Delete('tmp/nan64.tif')

    return ret

###############################################################################
# Test stats on signed byte (#3151)


def test_stats_signedbyte():

    if gdaltest.gtiff_drv is None:
        pytest.skip()

    stats = (-128.0, 127.0, -0.2, 80.64)

    shutil.copyfile('data/stats_signed_byte.img', 'tmp/stats_signed_byte.img')

    t = gdaltest.GDALTest('HFA', 'tmp/stats_signed_byte.img', 1, 11, filename_absolute=1)
    ret = t.testOpen(check_approx_stat=stats, check_stat=stats, skip_checksum=1)

    gdal.GetDriverByName('HFA').Delete('tmp/stats_signed_byte.img')

    return ret


###############################################################################
# Test return of GetStatistics() when we don't have stats and don't
# force their computation (#3572)

def test_stats_dont_force():

    gdal.Unlink('data/byte.tif.aux.xml')
    ds = gdal.Open('data/byte.tif')
    stats = ds.GetRasterBand(1).GetStatistics(0, 0)
    assert stats == [0, 0, 0, -1], 'did not get expected stats'


###############################################################################
# Test statistics when stored nodata value doesn't accurately match the nodata
# value used in the imagery (#3573)

def test_stats_approx_nodata():

    shutil.copyfile('data/minfloat.tif', 'tmp/minfloat.tif')
    try:
        os.remove('tmp/minfloat.tif.aux.xml')
    except OSError:
        pass

    ds = gdal.Open('tmp/minfloat.tif')
    stats = ds.GetRasterBand(1).GetStatistics(0, 1)
    md = ds.GetRasterBand(1).GetMetadata()
    nodata = ds.GetRasterBand(1).GetNoDataValue()
    ds = None

    os.remove('tmp/minfloat.tif.aux.xml')

    ds = gdal.Open('tmp/minfloat.tif')
    minmax = ds.GetRasterBand(1).ComputeRasterMinMax()
    ds = None

    os.remove('tmp/minfloat.tif')

    if nodata != -3.4028234663852886e+38:
        print("%.18g" % nodata)
        pytest.fail('did not get expected nodata')

    assert stats == [-3.0, 5.0, 1.0, 4.0], 'did not get expected stats'

    assert md == {'STATISTICS_MEAN': '1', 'STATISTICS_MAXIMUM': '5', 'STATISTICS_MINIMUM': '-3', 'STATISTICS_STDDEV': '4', 'STATISTICS_VALID_PERCENT': '50'}, \
        'did not get expected metadata'

    assert minmax == (-3.0, 5.0), 'did not get expected minmax'


###############################################################################
# Test read and copy of dataset with nan as nodata value (#3576)

def test_stats_nan_3():

    src_ds = gdal.Open('data/nan32_nodata.tif')
    nodata = src_ds.GetRasterBand(1).GetNoDataValue()
    assert gdaltest.isnan(nodata), ('expected nan, got %f' % nodata)

    out_ds = gdaltest.gtiff_drv.CreateCopy('tmp/nan32_nodata.tif', src_ds)
    del out_ds

    src_ds = None

    try:
        os.remove('tmp/nan32_nodata.tif.aux.xml')
    except OSError:
        pass

    ds = gdal.Open('tmp/nan32_nodata.tif')
    nodata = ds.GetRasterBand(1).GetNoDataValue()
    ds = None

    gdaltest.gtiff_drv.Delete('tmp/nan32_nodata.tif')
    assert gdaltest.isnan(nodata), ('expected nan, got %f' % nodata)

###############################################################################
# Test reading a VRT with a complex source that define nan as band nodata
# and complex source nodata (#3576)


def test_stats_nan_4():

    ds = gdal.Open('data/nan32_nodata.vrt')
    cs = ds.GetRasterBand(1).Checksum()
    nodata = ds.GetRasterBand(1).GetNoDataValue()
    ds = None

    assert cs == 874, 'did not get expected checksum'

    assert gdaltest.isnan(nodata), ('expected nan, got %f' % nodata)

###############################################################################
# Test reading a VRT with a complex source that define 0 as band nodata
# and complex source nodata (nan must be translated to 0 then) (#3576)


def test_stats_nan_5():

    ds = gdal.Open('data/nan32_nodata_nan_to_zero.vrt')
    cs = ds.GetRasterBand(1).Checksum()
    nodata = ds.GetRasterBand(1).GetNoDataValue()
    ds = None

    assert cs == 978, 'did not get expected checksum'

    assert nodata == 0, ('expected nan, got %f' % nodata)


###############################################################################
# Test reading a warped VRT with nan as src nodata and dest nodata (#3576)

def test_stats_nan_6():

    ds = gdal.Open('data/nan32_nodata_warp.vrt')
    cs = ds.GetRasterBand(1).Checksum()
    nodata = ds.GetRasterBand(1).GetNoDataValue()
    ds = None

    assert cs == 874, 'did not get expected checksum'

    assert gdaltest.isnan(nodata), ('expected nan, got %f' % nodata)

###############################################################################
# Test reading a warped VRT with nan as src nodata and 0 as dest nodata (#3576)


def test_stats_nan_7():

    ds = gdal.Open('data/nan32_nodata_warp_nan_to_zero.vrt')
    cs = ds.GetRasterBand(1).Checksum()
    nodata = ds.GetRasterBand(1).GetNoDataValue()
    ds = None

    assert cs == 978, 'did not get expected checksum'

    assert nodata == 0, ('expected nan, got %f' % nodata)


###############################################################################
# Test reading a warped VRT with zero as src nodata and nan as dest nodata (#3576)

def test_stats_nan_8():

    ds = gdal.Open('data/nan32_nodata_warp_zero_to_nan.vrt')
    cs = ds.GetRasterBand(1).Checksum()
    nodata = ds.GetRasterBand(1).GetNoDataValue()
    ds = None

    assert cs == 874, 'did not get expected checksum'

    assert gdaltest.isnan(nodata), ('expected nan, got %f' % nodata)

###############################################################################
# Test statistics computation when nodata = +/- inf


def stats_nodata_inf_progress_cbk(value, string, extra):
    # pylint: disable=unused-argument
    extra[0] = value


def test_stats_nodata_inf():

    ds = gdal.GetDriverByName('HFA').Create('/vsimem/stats_nodata_inf.img', 3, 1, 1, gdal.GDT_Float32)
    ds.GetRasterBand(1).SetNoDataValue(gdaltest.neginf())
    ds.GetRasterBand(1).WriteRaster(0, 0, 1, 1, struct.pack('f', gdaltest.neginf()), buf_type=gdal.GDT_Float32)
    ds.GetRasterBand(1).WriteRaster(1, 0, 1, 1, struct.pack('f', 1), buf_type=gdal.GDT_Float32)
    ds.GetRasterBand(1).WriteRaster(2, 0, 1, 1, struct.pack('f', -2), buf_type=gdal.GDT_Float32)

    ds.GetRasterBand(1).Checksum()
    user_data = [0]
    stats = ds.GetRasterBand(1).ComputeStatistics(False, stats_nodata_inf_progress_cbk, user_data)
    assert user_data[0] == 1.0, 'did not get expected pct'
    ds = None

    gdal.GetDriverByName('HFA').Delete('/vsimem/stats_nodata_inf.img')

    assert stats == [-2.0, 1.0, -0.5, 1.5], 'did not get expected stats'


###############################################################################
# Test deserialization of +inf/-inf values written by Linux and Windows

def stats_nodata_check(filename, expected_nodata):
    ds = gdal.Open(filename)
    nodata = ds.GetRasterBand(1).GetNoDataValue()
    ds = None

    assert nodata == expected_nodata, 'did not get expected nodata value'


def test_stats_nodata_neginf_linux():
    return stats_nodata_check('data/stats_nodata_neginf.tif', gdaltest.neginf())


def test_stats_nodata_neginf_msvc():
    return stats_nodata_check('data/stats_nodata_neginf_msvc.tif', gdaltest.neginf())


def test_stats_nodata_posinf_linux():
    return stats_nodata_check('data/stats_nodata_posinf.tif', gdaltest.posinf())


def test_stats_nodata_posinf_msvc():
    return stats_nodata_check('data/stats_nodata_posinf_msvc.tif', gdaltest.posinf())

###############################################################################
# Test standard deviation computation on huge values


def test_stats_stddev_huge_values():

    gdal.FileFromMemBuffer('/vsimem/stats_stddev_huge_values.asc',
                           """ncols        4
nrows        4
xllcorner    0
yllcorner    0
cellsize     1
 100000000 100000002 100000000 100000002
 100000000 100000002 100000000 100000002
 100000000 100000002 100000000 100000002
 100000000 100000002 100000000 100000002""")
    ds = gdal.Open('/vsimem/stats_stddev_huge_values.asc')
    stats = ds.GetRasterBand(1).ComputeStatistics(0)
    assert stats == [100000000.0, 100000002.0, 100000001.0, 1.0], \
        'did not get expected stats'
    ds = None
    gdal.GetDriverByName('AAIGRID').Delete('/vsimem/stats_stddev_huge_values.asc')

###############################################################################
# Test approximate statistics computation on a square shaped raster whose first column
# of blocks is nodata only


def test_stats_square_shape():

    ds = gdal.GetDriverByName('GTiff').Create('/vsimem/stats_square_shape.tif', 32, 32, options=['TILED=YES', 'BLOCKXSIZE=16', 'BLOCKYSIZE=16'])
    ds.GetRasterBand(1).SetNoDataValue(0)
    ds.GetRasterBand(1).WriteRaster(16, 0, 16, 32, struct.pack('B' * 1, 255), buf_xsize=1, buf_ysize=1)
    stats = ds.GetRasterBand(1).ComputeStatistics(True)
    hist = ds.GetRasterBand(1).GetHistogram(approx_ok=1)
    minmax = ds.GetRasterBand(1).ComputeRasterMinMax(1)
    ds = None

    gdal.GetDriverByName('GTiff').Delete('/vsimem/stats_square_shape.tif')

    assert stats == [255, 255, 255, 0], 'did not get expected stats'
    assert hist[255] == 16 * 16, 'did not get expected histogram'
    if minmax != (255, 255):
        print(hist)
        pytest.fail('did not get expected minmax')


###############################################################################
# Test when nodata = FLT_MIN (#6578)


def test_stats_flt_min():

    shutil.copyfile('data/flt_min.tif', 'tmp/flt_min.tif')
    try:
        os.remove('tmp/flt_min.tif.aux.xml')
    except OSError:
        pass

    ds = gdal.Open('tmp/flt_min.tif')
    stats = ds.GetRasterBand(1).GetStatistics(0, 1)
    nodata = ds.GetRasterBand(1).GetNoDataValue()
    ds = None

    os.remove('tmp/flt_min.tif.aux.xml')

    ds = gdal.Open('tmp/flt_min.tif')
    minmax = ds.GetRasterBand(1).ComputeRasterMinMax()
    ds = None

    os.remove('tmp/flt_min.tif')

    if nodata != 1.17549435082228751e-38:
        print("%.18g" % nodata)
        pytest.fail('did not get expected nodata')

    assert (stats == [0.0, 1.0, 0.33333333333333337, 0.47140452079103168] or \
       stats == [0.0, 1.0, 0.33333333333333331, 0.47140452079103168]), \
        'did not get expected stats'

    assert minmax == (0.0, 1.0), 'did not get expected minmax'

###############################################################################
# Test when nodata = DBL_MIN (#6578)


def test_stats_dbl_min():

    shutil.copyfile('data/dbl_min.tif', 'tmp/dbl_min.tif')
    try:
        os.remove('tmp/dbl_min.tif.aux.xml')
    except OSError:
        pass

    ds = gdal.Open('tmp/dbl_min.tif')
    stats = ds.GetRasterBand(1).GetStatistics(0, 1)
    nodata = ds.GetRasterBand(1).GetNoDataValue()
    ds = None

    os.remove('tmp/dbl_min.tif.aux.xml')

    ds = gdal.Open('tmp/dbl_min.tif')
    minmax = ds.GetRasterBand(1).ComputeRasterMinMax()
    ds = None

    os.remove('tmp/dbl_min.tif')

    if nodata != 2.22507385850720138e-308:
        print("%.18g" % nodata)
        pytest.fail('did not get expected nodata')

    assert (stats == [0.0, 1.0, 0.33333333333333337, 0.47140452079103168] or \
       stats == [0.0, 1.0, 0.33333333333333331, 0.47140452079103168]), \
        'did not get expected stats'

    assert minmax == (0.0, 1.0), 'did not get expected minmax'

###############################################################################
# Test stats on a tiled Byte with partial tiles


def test_stats_byte_partial_tiles():

    ds = gdal.Translate('/vsimem/stats_byte_tiled.tif', '../gdrivers/data/small_world.tif',
                        creationOptions=['TILED=YES', 'BLOCKXSIZE=64', 'BLOCKYSIZE=64'])
    stats = ds.GetRasterBand(1).GetStatistics(0, 1)
    ds = None

    gdal.GetDriverByName('GTiff').Delete('/vsimem/stats_byte_tiled.tif')

    expected_stats = [0.0, 255.0, 50.22115, 67.119029288849973]
    assert stats == expected_stats, 'did not get expected stats'

    # Same but with nodata set
    ds = gdal.Translate('/vsimem/stats_byte_tiled.tif', '../gdrivers/data/small_world.tif',
                        creationOptions=['TILED=YES', 'BLOCKXSIZE=64', 'BLOCKYSIZE=64'])
    ds.GetRasterBand(1).SetNoDataValue(0)
    stats = ds.GetRasterBand(1).GetStatistics(0, 1)
    ds = None

    gdal.GetDriverByName('GTiff').Delete('/vsimem/stats_byte_tiled.tif')

    expected_stats = [1.0, 255.0, 50.311081057390084, 67.14541389488096]
    expected_stats_32bit = [1.0, 255.0, 50.311081057390084, 67.145413894880946]
    assert stats == expected_stats or stats == expected_stats_32bit, \
        'did not get expected stats'

    # Same but with nodata set but untiled and with non power of 16 block size
    ds = gdal.Translate('/vsimem/stats_byte_untiled.tif', '../gdrivers/data/small_world.tif',
                        options='-srcwin 0 0 399 200')
    ds.GetRasterBand(1).SetNoDataValue(0)
    stats = ds.GetRasterBand(1).GetStatistics(0, 1)
    ds = None

    gdal.GetDriverByName('GTiff').Delete('/vsimem/stats_byte_untiled.tif')

    expected_stats = [1.0, 255.0, 50.378183963744554, 67.184793517649453]
    assert stats == expected_stats, 'did not get expected stats'

    ds = gdal.GetDriverByName('GTiff').Create('/vsimem/stats_byte_tiled.tif', 1000, 512,
                                              options=['TILED=YES', 'BLOCKXSIZE=512', 'BLOCKYSIZE=512'])
    ds.GetRasterBand(1).Fill(255)
    stats = ds.GetRasterBand(1).GetStatistics(0, 1)
    ds = None
    gdal.Unlink('/vsimem/stats_byte_tiled.tif')

    expected_stats = [255.0, 255.0, 255.0, 0.0]
    assert max([abs(stats[i] - expected_stats[i]) for i in range(4)]) <= 1e-15, \
        'did not get expected stats'

    # Non optimized code path
    ds = gdal.GetDriverByName('MEM').Create('', 1, 1)
    ds.GetRasterBand(1).WriteRaster(0, 0, 1, 1, struct.pack('B' * 1, 1))
    stats = ds.GetRasterBand(1).GetStatistics(0, 1)
    ds = None

    expected_stats = [1.0, 1.0, 1.0, 0.0]
    assert max([abs(stats[i] - expected_stats[i]) for i in range(4)]) <= 1e-15, \
        'did not get expected stats'

    ds = gdal.GetDriverByName('MEM').Create('', 3, 5)
    ds.GetRasterBand(1).WriteRaster(0, 0, 3, 1, struct.pack('B' * 3, 20, 30, 50))
    ds.GetRasterBand(1).WriteRaster(0, 1, 3, 1, struct.pack('B' * 3, 60, 10, 5))
    ds.GetRasterBand(1).WriteRaster(0, 2, 3, 1, struct.pack('B' * 3, 10, 20, 0))
    ds.GetRasterBand(1).WriteRaster(0, 3, 3, 1, struct.pack('B' * 3, 10, 20, 255))
    ds.GetRasterBand(1).WriteRaster(0, 4, 3, 1, struct.pack('B' * 3, 10, 20, 10))
    stats = ds.GetRasterBand(1).GetStatistics(0, 1)
    ds = None

    expected_stats = [0.0, 255.0, 35.333333333333336, 60.785597709398971]
    assert max([abs(stats[i] - expected_stats[i]) for i in range(4)]) <= 1e-15, \
        'did not get expected stats'

    ds = gdal.GetDriverByName('MEM').Create('', 32 + 2, 2)
    ds.GetRasterBand(1).Fill(1)
    ds.GetRasterBand(1).WriteRaster(32, 1, 2, 1, struct.pack('B' * 2, 0, 255))
    stats = ds.GetRasterBand(1).GetStatistics(0, 1)
    ds = None

    expected_stats = [0.0, 255.0, 4.7205882352941178, 30.576733555893391]
    assert max([abs(stats[i] - expected_stats[i]) for i in range(4)]) <= 1e-15, \
        'did not get expected stats'

    ds = gdal.GetDriverByName('MEM').Create('', 32 + 2, 2)
    ds.GetRasterBand(1).Fill(1)
    ds.GetRasterBand(1).SetNoDataValue(2)
    ds.GetRasterBand(1).WriteRaster(32, 1, 2, 1, struct.pack('B' * 2, 0, 255))
    stats = ds.GetRasterBand(1).GetStatistics(0, 1)
    ds = None

    expected_stats = [0.0, 255.0, 4.7205882352941178, 30.576733555893391]
    assert max([abs(stats[i] - expected_stats[i]) for i in range(4)]) <= 1e-15, \
        'did not get expected stats'

###############################################################################
# Test stats on uint16


def test_stats_uint16():

    ds = gdal.Translate('/vsimem/stats_uint16_tiled.tif', '../gdrivers/data/small_world.tif',
                        outputType=gdal.GDT_UInt16,
                        scaleParams=[[0, 255, 0, 65535]],
                        creationOptions=['TILED=YES', 'BLOCKXSIZE=64', 'BLOCKYSIZE=64'])
    stats = ds.GetRasterBand(1).GetStatistics(0, 1)
    ds = None

    gdal.GetDriverByName('GTiff').Delete('/vsimem/stats_uint16_tiled.tif')

    expected_stats = [0.0, 65535.0, 50.22115 * 65535 / 255, 67.119029288849973 * 65535 / 255]
    assert stats == expected_stats, 'did not get expected stats'

    ds = gdal.Translate('/vsimem/stats_uint16_untiled.tif', '../gdrivers/data/small_world.tif',
                        options='-srcwin 0 0 399 200 -scale 0 255 0 65535 -ot UInt16')
    stats = ds.GetRasterBand(1).GetStatistics(0, 1)
    ds = None

    gdal.GetDriverByName('GTiff').Delete('/vsimem/stats_uint16_untiled.tif')

    expected_stats = [0.0, 65535.0, 12923.9921679198, 17259.703026841547]
    assert stats == expected_stats, 'did not get expected stats'

    # Same but with nodata set but untiled and with non power of 16 block size
    ds = gdal.Translate('/vsimem/stats_uint16_untiled.tif', '../gdrivers/data/small_world.tif',
                        options='-srcwin 0 0 399 200 -scale 0 255 0 65535 -ot UInt16')
    ds.GetRasterBand(1).SetNoDataValue(0)
    stats = ds.GetRasterBand(1).GetStatistics(0, 1)
    ds = None

    gdal.GetDriverByName('GTiff').Delete('/vsimem/stats_uint16_untiled.tif')

    expected_stats = [257.0, 65535.0, 50.378183963744554 * 65535 / 255, 67.184793517649453 * 65535 / 255]
    assert stats == expected_stats, 'did not get expected stats'

    for fill_val in [0, 1, 32767, 32768, 65535]:
        ds = gdal.GetDriverByName('GTiff').Create('/vsimem/stats_uint16_tiled.tif', 1000, 512, 1, gdal.GDT_UInt16,
                                                  options=['TILED=YES', 'BLOCKXSIZE=512', 'BLOCKYSIZE=512'])
        ds.GetRasterBand(1).Fill(fill_val)
        stats = ds.GetRasterBand(1).GetStatistics(0, 1)
        ds = None
        gdal.Unlink('/vsimem/stats_uint16_tiled.tif')

        expected_stats = [fill_val, fill_val, fill_val, 0.0]
        if max([abs(stats[i] - expected_stats[i]) for i in range(4)]) > 1e-15:
            print(fill_val)
            pytest.fail('did not get expected stats')

    # Test remaining pixels after multiple of 32
    ds = gdal.GetDriverByName('MEM').Create('', 32 + 2, 1, 1, gdal.GDT_UInt16)
    ds.GetRasterBand(1).Fill(1)
    ds.GetRasterBand(1).WriteRaster(32, 0, 2, 1, struct.pack('H' * 2, 0, 65535))
    stats = ds.GetRasterBand(1).GetStatistics(0, 1)
    ds = None

    expected_stats = [0.0, 65535.0, 1928.4411764705883, 11072.48066469611]
    assert max([abs(stats[i] - expected_stats[i]) for i in range(4)]) <= 1e-15, \
        'did not get expected stats'

    # Non optimized code path
    for fill_val in [0, 1, 32767, 32768, 65535]:
        ds = gdal.GetDriverByName('MEM').Create('', 1, 1, 1, gdal.GDT_UInt16)
        ds.GetRasterBand(1).WriteRaster(0, 0, 1, 1, struct.pack('H' * 1, fill_val))
        stats = ds.GetRasterBand(1).GetStatistics(0, 1)
        ds = None

        expected_stats = [fill_val, fill_val, fill_val, 0.0]
        if max([abs(stats[i] - expected_stats[i]) for i in range(4)]) > 1e-15:
            print(fill_val)
            pytest.fail('did not get expected stats')

    ds = gdal.GetDriverByName('MEM').Create('', 3, 5, 1, gdal.GDT_UInt16)
    ds.GetRasterBand(1).WriteRaster(0, 0, 3, 1, struct.pack('H' * 3, 20, 30, 50))
    ds.GetRasterBand(1).WriteRaster(0, 1, 3, 1, struct.pack('H' * 3, 60, 10, 5))
    ds.GetRasterBand(1).WriteRaster(0, 2, 3, 1, struct.pack('H' * 3, 10, 20, 0))
    ds.GetRasterBand(1).WriteRaster(0, 3, 3, 1, struct.pack('H' * 3, 10, 20, 65535))
    ds.GetRasterBand(1).WriteRaster(0, 4, 3, 1, struct.pack('H' * 3, 10, 20, 10))
    stats = ds.GetRasterBand(1).GetStatistics(0, 1)
    ds = None

    expected_stats = [0.0, 65535.0, 4387.333333333333, 16342.408927558861]
    assert max([abs(stats[i] - expected_stats[i]) for i in range(4)]) <= 1e-15, \
        'did not get expected stats'

    ds = gdal.GetDriverByName('MEM').Create('', 2, 2, 1, gdal.GDT_UInt16)
    ds.GetRasterBand(1).WriteRaster(0, 0, 2, 1, struct.pack('H' * 2, 0, 65535))
    ds.GetRasterBand(1).WriteRaster(0, 1, 2, 1, struct.pack('H' * 2, 1, 65534))
    stats = ds.GetRasterBand(1).GetStatistics(0, 1)
    ds = None

    expected_stats = [0.0, 65535.0, 32767.5, 32767.000003814814]
    assert max([abs(stats[i] - expected_stats[i]) for i in range(4)]) <= 1e-15, \
        'did not get expected stats'

###############################################################################
# Test a case where the nodata value is almost the maximum value of float32


def test_stats_nodata_almost_max_float32():

    gdal.FileFromMemBuffer('/vsimem/float32_almost_nodata_max_float32.tif',
                           open('data/float32_almost_nodata_max_float32.tif', 'rb').read())

    ds = gdal.Open('/vsimem/float32_almost_nodata_max_float32.tif')
    minmax = ds.GetRasterBand(1).ComputeRasterMinMax()
    assert minmax == (0, 0), 'did not get expected minmax'
    stats = ds.GetRasterBand(1).ComputeStatistics(False)
    assert stats == [0, 0, 0, 0], 'did not get expected stats'
    hist = ds.GetRasterBand(1).GetHistogram(approx_ok=0)
    assert hist[0] == 3, 'did not get expected hist'
    ds = None

    gdal.GetDriverByName('GTiff').Delete('/vsimem/float32_almost_nodata_max_float32.tif')


###############################################################################
# Test STATISTICS_APPROXIMATE


def test_stats_approx_stats_flag(dt=gdal.GDT_Byte, struct_frmt='B'):

    ds = gdal.GetDriverByName('MEM').Create('', 2000, 2000, 1, dt)
    ds.GetRasterBand(1).WriteRaster(1000, 1000, 1, 1, struct.pack(struct_frmt * 1, 20))

    approx_ok = 1
    force = 1
    stats = ds.GetRasterBand(1).GetStatistics(approx_ok, force)
    assert stats == [0.0, 0.0, 0.0, 0.0], 'did not get expected stats'
    md = ds.GetRasterBand(1).GetMetadata()
    assert md == {'STATISTICS_MEAN': '0', 'STATISTICS_MAXIMUM': '0', 'STATISTICS_MINIMUM': '0', 'STATISTICS_APPROXIMATE': 'YES', 'STATISTICS_STDDEV': '0', 'STATISTICS_VALID_PERCENT': '100'}, \
        'did not get expected metadata'

    approx_ok = 0
    force = 0
    stats = ds.GetRasterBand(1).GetStatistics(approx_ok, force)
    assert stats == [0.0, 0.0, 0.0, -1.0], 'did not get expected stats'

    approx_ok = 0
    force = 1
    stats = ds.GetRasterBand(1).GetStatistics(approx_ok, force)
    assert stats[1] == 20.0, 'did not get expected stats'
    md = ds.GetRasterBand(1).GetMetadata()
    assert 'STATISTICS_APPROXIMATE' not in md, 'did not get expected metadata'


def test_stats_approx_stats_flag_float():
    return test_stats_approx_stats_flag(dt=gdal.GDT_Float32, struct_frmt='f')


def test_stats_all_nodata():

    ds = gdal.GetDriverByName('MEM').Create('', 2000, 2000)
    ds.GetRasterBand(1).SetNoDataValue(0)
    approx_ok = 1
    force = 1
    with gdaltest.error_handler():
        stats = ds.GetRasterBand(1).GetStatistics(approx_ok, force)
    assert stats == [0.0, 0.0, 0.0, 0.0], 'did not get expected stats'

    ds = gdal.GetDriverByName('MEM').Create('', 2000, 2000, 1,
                                            gdal.GDT_Float32)
    ds.GetRasterBand(1).SetNoDataValue(0)
    approx_ok = 1
    force = 1
    with gdaltest.error_handler():
        stats = ds.GetRasterBand(1).GetStatistics(approx_ok, force)
    assert stats == [0.0, 0.0, 0.0, 0.0], 'did not get expected stats'


def test_stats_float32_with_nodata_slightly_above_float_max():

    ds = gdal.Open('data/float32_with_nodata_slightly_above_float_max.tif')
    my_min, my_max = ds.GetRasterBand(1).ComputeRasterMinMax()
    assert (my_min, my_max) == (-1.0989999771118164, 0.703338623046875), \
        'did not get expected stats'


def test_stats_clear():

    filename = '/vsimem/out.tif'
    gdal.Translate(filename, 'data/byte.tif')
    ds = gdal.Open(filename)
    assert ds.GetRasterBand(1).GetStatistics(False, False) == [0,0,0,-1]
    assert ds.GetRasterBand(1).ComputeStatistics(False) != [0,0,0,-1]

    ds = gdal.Open(filename)
    assert ds.GetRasterBand(1).GetStatistics(False, False) != [0,0,0,-1]
    ds.ClearStatistics()

    ds = gdal.Open(filename)
    assert ds.GetRasterBand(1).GetStatistics(False, False) == [0,0,0,-1]

    gdal.GetDriverByName('GTiff').Delete(filename)


@pytest.mark.parametrize('datatype,minval,maxval',
                         [(gdal.GDT_Byte, 1, 254),
                          (gdal.GDT_Byte, -127, 127),
                          (gdal.GDT_UInt16, 1, 65535),
                          (gdal.GDT_Int16, -32767, 32766),
                          (gdal.GDT_UInt32, 1, (1 << 32) - 2),
                          (gdal.GDT_Int32, -(1 << 31) + 1, (1 << 31) - 2),
                          (gdal.GDT_UInt64, 1, (1 << 53) - 2),
                          (gdal.GDT_Int64, -(1 << 53) + 2, (1 << 53) - 2),
                          (gdal.GDT_Float32, -struct.unpack('f', struct.pack('f', 1e20))[0],
                                             struct.unpack('f', struct.pack('f', 1e20))[0]),
                          (gdal.GDT_Float64, -1e100, 1e100),
                          (gdal.GDT_CInt16, -32767, 32766),
                          (gdal.GDT_CInt32, -(1 << 31) + 1, (1 << 31) - 2),
                          (gdal.GDT_CFloat32, -struct.unpack('f', struct.pack('f', 1e20))[0],
                                             struct.unpack('f', struct.pack('f', 1e20))[0]),
                          (gdal.GDT_CFloat64, -1e100, 1e100),])
@pytest.mark.parametrize('nodata', [None, 0, 1])
def test_stats_computeminmax(datatype, minval, maxval, nodata):
    ds = gdal.GetDriverByName('MEM').Create('', 64, 1, 1, datatype)
    minval_mod = minval
    expected_minval = minval
    if datatype == gdal.GDT_Byte and minval < 0:
        ds.GetRasterBand(1).SetMetadataItem('PIXELTYPE', 'SIGNEDBYTE', 'IMAGE_STRUCTURE')
        minval_mod = 256 + minval
    if nodata:
        ds.GetRasterBand(1).SetNoDataValue(nodata)
        if nodata == 1 and minval == 1:
            expected_minval = maxval
    ds.GetRasterBand(1).WriteRaster(0, 0, 64, 1,
                                    struct.pack('d' * 2, minval_mod, maxval),
                                    buf_type = gdal.GDT_Float64,
                                    buf_xsize = 2,
                                    buf_ysize = 1)
    assert ds.GetRasterBand(1).ComputeRasterMinMax(0) == (expected_minval, maxval)

