#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test default implementation of GDALRasterBand::IRasterIO
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2008-2013, Even Rouault <even dot rouault at spatialys.com>
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

from osgeo import gdal
import gdaltest
import pytest

###############################################################################
# Test writing a 1x1 buffer to a 10x6 raster and read it back


def test_rasterio_1():
    data = 'A'.encode('ascii')

    drv = gdal.GetDriverByName('GTiff')
    ds = drv.Create('tmp/rasterio1.tif', 10, 6, 1)

    ds.GetRasterBand(1).Fill(65)
    checksum = ds.GetRasterBand(1).Checksum()

    ds.GetRasterBand(1).Fill(0)

    ds.WriteRaster(0, 0, ds.RasterXSize, ds.RasterYSize, data, buf_type=gdal.GDT_Byte, buf_xsize=1, buf_ysize=1)
    assert checksum == ds.GetRasterBand(1).Checksum(), 'Didnt get expected checksum '

    data2 = ds.ReadRaster(0, 0, ds.RasterXSize, ds.RasterYSize, 1, 1)
    assert data2 == data, 'Didnt get expected buffer '

    ds = None
    drv.Delete('tmp/rasterio1.tif')

###############################################################################
# Test writing a 5x4 buffer to a 10x6 raster and read it back


def test_rasterio_2():
    data = 'AAAAAAAAAAAAAAAAAAAA'.encode('ascii')

    drv = gdal.GetDriverByName('GTiff')
    ds = drv.Create('tmp/rasterio2.tif', 10, 6, 1)

    ds.GetRasterBand(1).Fill(65)
    checksum = ds.GetRasterBand(1).Checksum()

    ds.GetRasterBand(1).Fill(0)

    ds.WriteRaster(0, 0, ds.RasterXSize, ds.RasterYSize, data, buf_type=gdal.GDT_Byte, buf_xsize=5, buf_ysize=4)
    assert checksum == ds.GetRasterBand(1).Checksum(), 'Didnt get expected checksum '

    data2 = ds.ReadRaster(0, 0, ds.RasterXSize, ds.RasterYSize, 5, 4)
    assert data2 == data, 'Didnt get expected buffer '

    ds = None
    drv.Delete('tmp/rasterio2.tif')

###############################################################################
# Test extensive read & writes into a non tiled raster


def test_rasterio_3():

    data = [['' for i in range(4)] for i in range(5)]
    for xsize in range(5):
        for ysize in range(4):
            for m in range((xsize + 1) * (ysize + 1)):
                data[xsize][ysize] = data[xsize][ysize] + 'A'
            data[xsize][ysize] = data[xsize][ysize].encode('ascii')

    drv = gdal.GetDriverByName('GTiff')
    ds = drv.Create('tmp/rasterio3.tif', 10, 6, 1)

    i = 0
    while i < ds.RasterXSize:
        j = 0
        while j < ds.RasterYSize:
            k = 0
            while k < ds.RasterXSize - i:
                m = 0
                while m < ds.RasterYSize - j:
                    for xsize in range(5):
                        for ysize in range(4):
                            ds.GetRasterBand(1).Fill(0)
                            ds.WriteRaster(i, j, k + 1, m + 1, data[xsize][ysize],
                                           buf_type=gdal.GDT_Byte,
                                           buf_xsize=xsize + 1, buf_ysize=ysize + 1)
                            data2 = ds.ReadRaster(i, j, k + 1, m + 1, xsize + 1, ysize + 1, gdal.GDT_Byte)
                            assert data2 == data[xsize][ysize], \
                                'Didnt get expected buffer '
                    m = m + 1
                k = k + 1
            j = j + 1
        i = i + 1

    ds = None
    drv.Delete('tmp/rasterio3.tif')

###############################################################################
# Test extensive read & writes into a tiled raster


def test_rasterio_4():

    data = ['' for i in range(5 * 4)]
    for size in range(5 * 4):
        for k in range(size + 1):
            data[size] = data[size] + 'A'
        data[size] = data[size].encode('ascii')

    drv = gdal.GetDriverByName('GTiff')
    ds = drv.Create('tmp/rasterio4.tif', 20, 20, 1, options=['TILED=YES', 'BLOCKXSIZE=16', 'BLOCKYSIZE=16'])

    i = 0
    while i < ds.RasterXSize:
        j = 0
        while j < ds.RasterYSize:
            k = 0
            while k < ds.RasterXSize - i:
                m = 0
                while m < ds.RasterYSize - j:
                    for xsize in range(5):
                        for ysize in range(4):
                            ds.GetRasterBand(1).Fill(0)
                            ds.WriteRaster(i, j, k + 1, m + 1, data[(xsize + 1) * (ysize + 1) - 1],
                                           buf_type=gdal.GDT_Byte,
                                           buf_xsize=xsize + 1, buf_ysize=ysize + 1)
                            data2 = ds.ReadRaster(i, j, k + 1, m + 1, xsize + 1, ysize + 1, gdal.GDT_Byte)
                            if data2 != data[(xsize + 1) * (ysize + 1) - 1]:
                                print(i, j, k, m, xsize, ysize)
                                pytest.fail('Didnt get expected buffer ')
                    m = m + 1
                k = k + 1
            if j >= 15:
                j = j + 1
            else:
                j = j + 3
        if i >= 15:
            i = i + 1
        else:
            i = i + 3

    ds = None
    drv.Delete('tmp/rasterio4.tif')

###############################################################################
# Test error cases of ReadRaster()


def test_rasterio_5():

    ds = gdal.Open('data/byte.tif')

    for obj in [ds, ds.GetRasterBand(1)]:
        obj.ReadRaster(0, 0, -2000000000, 1, 1, 1)
        obj.ReadRaster(0, 0, 1, -2000000000, 1, 1)

    for band_number in [-1, 0, 2]:
        gdal.ErrorReset()
        gdal.PushErrorHandler('CPLQuietErrorHandler')
        res = ds.ReadRaster(0, 0, 1, 1, band_list=[band_number])
        gdal.PopErrorHandler()
        error_msg = gdal.GetLastErrorMsg()
        assert res is None, 'expected None'
        assert error_msg.find('this band does not exist on dataset') != -1, \
            'did not get expected error msg'

    res = ds.ReadRaster(0, 0, 1, 1, band_list=[1, 1])
    assert res is not None, 'expected non None'

    for obj in [ds, ds.GetRasterBand(1)]:
        gdal.ErrorReset()
        gdal.PushErrorHandler('CPLQuietErrorHandler')
        res = obj.ReadRaster(0, 0, 21, 21)
        gdal.PopErrorHandler()
        error_msg = gdal.GetLastErrorMsg()
        assert res is None, 'expected None'
        assert error_msg.find('Access window out of range in RasterIO()') != -1, \
            'did not get expected error msg (1)'

        # This should only fail on a 32bit build
        try:
            maxsize = sys.maxint
        except AttributeError:
            maxsize = sys.maxsize

        # On win64, maxsize == 2147483647 and ReadRaster()
        # fails because of out of memory condition, not
        # because of integer overflow. I'm not sure on how
        # to detect win64 better.
        if maxsize == 2147483647 and sys.platform != 'win32':
            gdal.ErrorReset()
            gdal.PushErrorHandler('CPLQuietErrorHandler')
            res = obj.ReadRaster(0, 0, 1, 1, 1000000, 1000000)
            gdal.PopErrorHandler()
            error_msg = gdal.GetLastErrorMsg()
            assert res is None, 'expected None'
            assert error_msg.find('Integer overflow') != -1, \
                'did not get expected error msg (2)'

        gdal.ErrorReset()
        gdal.PushErrorHandler('CPLQuietErrorHandler')
        res = obj.ReadRaster(0, 0, 0, 1)
        gdal.PopErrorHandler()
        error_msg = gdal.GetLastErrorMsg()
        assert res is None, 'expected None'
        assert error_msg.find('Illegal values for buffer size') != -1, \
            'did not get expected error msg (3)'

    ds = None

###############################################################################
# Test error cases of WriteRaster()


def test_rasterio_6():

    ds = gdal.GetDriverByName('MEM').Create('', 2, 2)

    for obj in [ds, ds.GetRasterBand(1)]:
        with pytest.raises(Exception):
            obj.WriteRaster(0, 0, 2, 2, None)

        gdal.ErrorReset()
        gdal.PushErrorHandler('CPLQuietErrorHandler')
        obj.WriteRaster(0, 0, 2, 2, ' ')
        gdal.PopErrorHandler()
        error_msg = gdal.GetLastErrorMsg()
        assert error_msg.find('Buffer too small') != -1, \
            'did not get expected error msg (1)'

        gdal.ErrorReset()
        gdal.PushErrorHandler('CPLQuietErrorHandler')
        obj.WriteRaster(-1, 0, 1, 1, ' ')
        gdal.PopErrorHandler()
        error_msg = gdal.GetLastErrorMsg()
        assert error_msg.find('Access window out of range in RasterIO()') != -1, \
            'did not get expected error msg (2)'

        gdal.ErrorReset()
        gdal.PushErrorHandler('CPLQuietErrorHandler')
        obj.WriteRaster(0, 0, 0, 1, ' ')
        gdal.PopErrorHandler()
        error_msg = gdal.GetLastErrorMsg()
        assert error_msg.find('Illegal values for buffer size') != -1, \
            'did not get expected error msg (3)'

    ds = None

###############################################################################
# Test that default window reading works via ReadRaster()


def test_rasterio_7():

    ds = gdal.Open('data/byte.tif')

    data = ds.GetRasterBand(1).ReadRaster()
    assert len(data) == 400, 'did not read expected band data via ReadRaster()'

    data = ds.ReadRaster()
    assert len(data) == 400, 'did not read expected dataset data via ReadRaster()'

###############################################################################
# Test callback of ReadRaster()


def rasterio_8_progress_callback(pct, message, user_data):
    # pylint: disable=unused-argument
    if pct != pytest.approx((user_data[0] + 0.05), abs=1e-5):
        print('Expected %f, got %f' % (user_data[0] + 0.05, pct))
        user_data[1] = False
    user_data[0] = pct
    return 1  # 1 to continue, 0 to stop


def rasterio_8_progress_interrupt_callback(pct, message, user_data):
    # pylint: disable=unused-argument
    user_data[0] = pct
    if pct >= 0.5:
        return 0
    return 1  # 1 to continue, 0 to stop


def rasterio_8_progress_callback_2(pct, message, user_data):
    # pylint: disable=unused-argument
    if pct < user_data[0]:
        print('Got %f, last pct was %f' % (pct, user_data[0]))
        return 0
    user_data[0] = pct
    return 1  # 1 to continue, 0 to stop


def test_rasterio_8():

    ds = gdal.Open('data/byte.tif')

    # Progress not implemented yet
    if gdal.GetConfigOption('GTIFF_DIRECT_IO') == 'YES' or \
       gdal.GetConfigOption('GTIFF_VIRTUAL_MEM_IO') == 'YES':
        pytest.skip()

    # Test RasterBand.ReadRaster
    tab = [0, True]
    data = ds.GetRasterBand(1).ReadRaster(resample_alg=gdal.GRIORA_NearestNeighbour,
                                          callback=rasterio_8_progress_callback,
                                          callback_data=tab)
    assert len(data) == 400, 'did not read expected band data via ReadRaster()'
    assert tab[0] == pytest.approx(1, abs=1e-5) and tab[1]

    # Test interruption
    tab = [0]
    data = ds.GetRasterBand(1).ReadRaster(resample_alg=gdal.GRIORA_NearestNeighbour,
                                          callback=rasterio_8_progress_interrupt_callback,
                                          callback_data=tab)
    assert data is None
    assert tab[0] >= 0.50

    # Test RasterBand.ReadRaster with type change
    tab = [0, True]
    data = ds.GetRasterBand(1).ReadRaster(buf_type=gdal.GDT_Int16,
                                          callback=rasterio_8_progress_callback,
                                          callback_data=tab)
    assert data is not None, 'did not read expected band data via ReadRaster()'
    assert tab[0] == pytest.approx(1, abs=1e-5) and tab[1]

    # Same with interruption
    tab = [0]
    data = ds.GetRasterBand(1).ReadRaster(buf_type=gdal.GDT_Int16,
                                          callback=rasterio_8_progress_interrupt_callback,
                                          callback_data=tab)
    assert data is None and tab[0] >= 0.50

    # Test RasterBand.ReadRaster with resampling
    tab = [0, True]
    data = ds.GetRasterBand(1).ReadRaster(buf_xsize=40,
                                          callback=rasterio_8_progress_callback,
                                          callback_data=tab)
    assert data is not None, 'did not read expected band data via ReadRaster()'
    assert tab[0] == pytest.approx(1, abs=1e-5) and tab[1]

    # Same with interruption
    tab = [0]
    data = ds.GetRasterBand(1).ReadRaster(buf_xsize=40,
                                          callback=rasterio_8_progress_interrupt_callback,
                                          callback_data=tab)
    assert data is None and tab[0] >= 0.50

    # Test Dataset.ReadRaster
    tab = [0, True]
    data = ds.ReadRaster(resample_alg=gdal.GRIORA_NearestNeighbour,
                         callback=rasterio_8_progress_callback,
                         callback_data=tab)
    assert len(data) == 400, 'did not read expected dataset data via ReadRaster()'
    assert tab[0] == pytest.approx(1, abs=1e-5) and tab[1]

    ds = None

    # Test Dataset.ReadRaster on a multi band file, with INTERLEAVE=BAND
    ds = gdal.Open('data/rgbsmall.tif')
    last_pct = [0]
    data = ds.ReadRaster(resample_alg=gdal.GRIORA_NearestNeighbour,
                         callback=rasterio_8_progress_callback_2,
                         callback_data=last_pct)
    assert not (data is None or last_pct[0] != pytest.approx(1.0, abs=1e-5))

    # Same with interruption
    tab = [0]
    data = ds.ReadRaster(callback=rasterio_8_progress_interrupt_callback,
                         callback_data=tab)
    assert data is None and tab[0] >= 0.50

    ds = None

    # Test Dataset.ReadRaster on a multi band file, with INTERLEAVE=PIXEL
    ds = gdal.Open('data/rgbsmall_cmyk.tif')
    last_pct = [0]
    data = ds.ReadRaster(resample_alg=gdal.GRIORA_NearestNeighbour,
                         callback=rasterio_8_progress_callback_2,
                         callback_data=last_pct)
    assert not (data is None or last_pct[0] != pytest.approx(1.0, abs=1e-5))

    # Same with interruption
    tab = [0]
    data = ds.ReadRaster(callback=rasterio_8_progress_interrupt_callback,
                         callback_data=tab)
    assert data is None and tab[0] >= 0.50

###############################################################################
# Test resampling algorithm of ReadRaster()


def rasterio_9_progress_callback(pct, message, user_data):
    # pylint: disable=unused-argument
    if pct < user_data[0]:
        print('Got %f, last pct was %f' % (pct, user_data[0]))
        return 0
    user_data[0] = pct
    if user_data[1] is not None and pct >= user_data[1]:
        return 0
    return 1  # 1 to continue, 0 to stop


def rasterio_9_checksum(data, buf_xsize, buf_ysize, data_type=gdal.GDT_Byte):
    ds = gdal.GetDriverByName('MEM').Create('', buf_xsize, buf_ysize, 1)
    ds.GetRasterBand(1).WriteRaster(0, 0, buf_xsize, buf_ysize, data, buf_type=data_type)
    cs = ds.GetRasterBand(1).Checksum()
    return cs


def test_rasterio_9():
    ds = gdal.Open('data/byte.tif')

    # Test RasterBand.ReadRaster, with Bilinear
    tab = [0, None]
    data = ds.GetRasterBand(1).ReadRaster(buf_type=gdal.GDT_Int16,
                                          buf_xsize=10,
                                          buf_ysize=10,
                                          resample_alg=gdal.GRIORA_Bilinear,
                                          callback=rasterio_9_progress_callback,
                                          callback_data=tab)
    assert data is not None
    data_ar = struct.unpack('h' * 10 * 10, data)
    cs = rasterio_9_checksum(data, 10, 10, data_type=gdal.GDT_Int16)
    assert cs == 1211

    assert tab[0] == pytest.approx(1.0, abs=1e-5)

    # Same but query with GDT_Float32. Check that we do not get floating-point
    # values, since the band type is Byte
    data = ds.GetRasterBand(1).ReadRaster(buf_type=gdal.GDT_Float32,
                                          buf_xsize=10,
                                          buf_ysize=10,
                                          resample_alg=gdal.GRIORA_Bilinear)

    data_float32_ar = struct.unpack('f' * 10 * 10, data)
    assert data_ar == data_float32_ar

    # Test RasterBand.ReadRaster, with Lanczos
    tab = [0, None]
    data = ds.GetRasterBand(1).ReadRaster(buf_xsize=10,
                                          buf_ysize=10,
                                          resample_alg=gdal.GRIORA_Lanczos,
                                          callback=rasterio_9_progress_callback,
                                          callback_data=tab)
    assert data is not None
    cs = rasterio_9_checksum(data, 10, 10)
    assert cs == 1154

    assert tab[0] == pytest.approx(1.0, abs=1e-5)

    # Test RasterBand.ReadRaster, with Bilinear and UInt16 data type
    src_ds_uint16 = gdal.Open('data/uint16.tif')
    tab = [0, None]
    data = src_ds_uint16.GetRasterBand(1).ReadRaster(buf_type=gdal.GDT_UInt16,
                                                     buf_xsize=10,
                                                     buf_ysize=10,
                                                     resample_alg=gdal.GRIORA_Bilinear,
                                                     callback=rasterio_9_progress_callback,
                                                     callback_data=tab)
    assert data is not None
    cs = rasterio_9_checksum(data, 10, 10, data_type=gdal.GDT_UInt16)
    assert cs == 1211

    assert tab[0] == pytest.approx(1.0, abs=1e-5)

    # Test RasterBand.ReadRaster, with Bilinear on Complex, thus using warp API
    tab = [0, None]
    complex_ds = gdal.GetDriverByName('MEM').Create('', 20, 20, 1, gdal.GDT_CInt16)
    complex_ds.GetRasterBand(1).WriteRaster(0, 0, 20, 20, ds.GetRasterBand(1).ReadRaster(), buf_type=gdal.GDT_Byte)
    data = complex_ds.GetRasterBand(1).ReadRaster(buf_xsize=10,
                                                  buf_ysize=10,
                                                  resample_alg=gdal.GRIORA_Bilinear,
                                                  callback=rasterio_9_progress_callback,
                                                  callback_data=tab)
    assert data is not None
    cs = rasterio_9_checksum(data, 10, 10, data_type=gdal.GDT_CInt16)
    assert cs == 1211

    assert tab[0] == pytest.approx(1.0, abs=1e-5)

    # Test interruption
    tab = [0, 0.5]
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    data = ds.GetRasterBand(1).ReadRaster(buf_xsize=10,
                                          buf_ysize=10,
                                          resample_alg=gdal.GRIORA_Bilinear,
                                          callback=rasterio_9_progress_callback,
                                          callback_data=tab)
    gdal.PopErrorHandler()
    assert data is None
    assert tab[0] >= 0.50

    # Test RasterBand.ReadRaster, with Gauss, and downsampling
    tab = [0, None]
    data = ds.GetRasterBand(1).ReadRaster(buf_xsize=10,
                                          buf_ysize=10,
                                          resample_alg=gdal.GRIORA_Gauss,
                                          callback=rasterio_9_progress_callback,
                                          callback_data=tab)
    assert data is not None
    cs = rasterio_9_checksum(data, 10, 10)
    assert cs == 1089

    assert tab[0] == pytest.approx(1.0, abs=1e-5)

    # Test RasterBand.ReadRaster, with Cubic, and downsampling
    tab = [0, None]
    data = ds.GetRasterBand(1).ReadRaster(buf_xsize=10,
                                          buf_ysize=10,
                                          resample_alg=gdal.GRIORA_Cubic,
                                          callback=rasterio_9_progress_callback,
                                          callback_data=tab)
    assert data is not None
    cs = rasterio_9_checksum(data, 10, 10)
    assert cs == 1059

    assert tab[0] == pytest.approx(1.0, abs=1e-5)

    # Test RasterBand.ReadRaster, with Cubic, and downsampling with >=8x8 source samples used for a dest sample
    data = ds.GetRasterBand(1).ReadRaster(buf_xsize=5,
                                          buf_ysize=5,
                                          resample_alg=gdal.GRIORA_Cubic)
    assert data is not None
    cs = rasterio_9_checksum(data, 5, 5)
    assert cs == 214

    # Same with UInt16
    data = src_ds_uint16.GetRasterBand(1).ReadRaster(buf_xsize=5,
                                                     buf_ysize=5,
                                                     resample_alg=gdal.GRIORA_Cubic)
    assert data is not None
    cs = rasterio_9_checksum(data, 5, 5, data_type=gdal.GDT_UInt16)
    assert cs == 214

    # Test RasterBand.ReadRaster, with Cubic and supersampling
    tab = [0, None]
    data = ds.GetRasterBand(1).ReadRaster(buf_xsize=40,
                                          buf_ysize=40,
                                          resample_alg=gdal.GRIORA_Cubic,
                                          callback=rasterio_9_progress_callback,
                                          callback_data=tab)
    assert data is not None
    cs = rasterio_9_checksum(data, 40, 40)
    assert cs == 19556

    assert tab[0] == pytest.approx(1.0, abs=1e-5)

    # Test Dataset.ReadRaster, with Cubic and supersampling
    tab = [0, None]
    data = ds.ReadRaster(buf_xsize=40,
                         buf_ysize=40,
                         resample_alg=gdal.GRIORA_CubicSpline,
                         callback=rasterio_9_progress_callback,
                         callback_data=tab)
    assert data is not None
    cs = rasterio_9_checksum(data, 40, 40)
    assert cs == 19041

    assert tab[0] == pytest.approx(1.0, abs=1e-5)

    # Test Dataset.ReadRaster on a multi band file, with INTERLEAVE=PIXEL
    ds = gdal.Open('data/rgbsmall_cmyk.tif')
    tab = [0, None]
    data = ds.ReadRaster(buf_xsize=25,
                         buf_ysize=25,
                         resample_alg=gdal.GRIORA_Cubic,
                         callback=rasterio_9_progress_callback,
                         callback_data=tab)
    assert data is not None
    cs = rasterio_9_checksum(data[0:25 * 25], 25, 25)
    assert cs == 5975
    cs = rasterio_9_checksum(data[25 * 25:2 * 25 * 25], 25, 25)
    assert cs == 6248

    assert tab[0] == pytest.approx(1.0, abs=1e-5)
    ds = None

    # Test Band.ReadRaster on a RGBA with parts fully opaque, and fully transparent and with huge upscaling
    ds = gdal.Open('data/stefan_full_rgba.png')
    tab = [0, None]
    data = ds.GetRasterBand(1).ReadRaster(buf_xsize=162 * 16,
                                          buf_ysize=150 * 16,
                                          resample_alg=gdal.GRIORA_Cubic,
                                          callback=rasterio_9_progress_callback,
                                          callback_data=tab)
    assert data is not None
    cs = rasterio_9_checksum(data, 162 * 16, 150 * 16)
    assert cs == 30836
    assert tab[0] == pytest.approx(1.0, abs=1e-5)

###############################################################################
# Test error when getting a block


def test_rasterio_10():
    ds = gdal.Open('data/byte_truncated.tif')

    gdal.PushErrorHandler()
    data = ds.GetRasterBand(1).ReadRaster()
    gdal.PopErrorHandler()
    assert data is None

    # Change buffer type
    gdal.PushErrorHandler()
    data = ds.GetRasterBand(1).ReadRaster(buf_type=gdal.GDT_Int16)
    gdal.PopErrorHandler()
    assert data is None

    # Resampling case
    gdal.PushErrorHandler()
    data = ds.GetRasterBand(1).ReadRaster(buf_xsize=10,
                                          buf_ysize=10)
    gdal.PopErrorHandler()
    assert data is None

###############################################################################
# Test cubic resampling and nbits


def test_rasterio_11():
    numpy = pytest.importorskip('numpy')

    mem_ds = gdal.GetDriverByName('MEM').Create('', 4, 3)
    mem_ds.GetRasterBand(1).WriteArray(numpy.array([[80, 125, 125, 80], [80, 125, 125, 80], [80, 125, 125, 80]]))

    # A bit dummy
    mem_ds.GetRasterBand(1).SetMetadataItem('NBITS', '8', 'IMAGE_STRUCTURE')
    ar = mem_ds.GetRasterBand(1).ReadAsArray(0, 0, 4, 3, 8, 3, resample_alg=gdal.GRIORA_Cubic)
    assert ar.max() == 129

    # NBITS=7
    mem_ds.GetRasterBand(1).SetMetadataItem('NBITS', '7', 'IMAGE_STRUCTURE')
    ar = mem_ds.GetRasterBand(1).ReadAsArray(0, 0, 4, 3, 8, 3, resample_alg=gdal.GRIORA_Cubic)
    # Would overshoot to 129 if NBITS was ignored
    assert ar.max() == 127

###############################################################################
# Test cubic resampling on dataset RasterIO with an alpha channel


def rasterio_12_progress_callback(pct, message, user_data):
    if pct < user_data[0]:
        print('Got %f, last pct was %f' % (pct, user_data[0]))
        return 0
    user_data[0] = pct
    return 1  # 1 to continue, 0 to stop


def test_rasterio_12():
    numpy = pytest.importorskip('numpy')

    mem_ds = gdal.GetDriverByName('MEM').Create('', 4, 3, 4)
    for i in range(3):
        mem_ds.GetRasterBand(i + 1).SetColorInterpretation(gdal.GCI_GrayIndex)
    mem_ds.GetRasterBand(4).SetColorInterpretation(gdal.GCI_AlphaBand)
    for i in range(4):
        mem_ds.GetRasterBand(i + 1).WriteArray(numpy.array([[0, 0, 0, 0], [0, 255, 0, 0], [0, 0, 0, 0]]))

    tab = [0]
    ar_ds = mem_ds.ReadAsArray(0, 0, 4, 3, buf_xsize=8, buf_ysize=3, resample_alg=gdal.GRIORA_Cubic,
                               callback=rasterio_12_progress_callback,
                               callback_data=tab)
    assert tab[0] == 1.0

    ar_ds2 = mem_ds.ReadAsArray(0, 0, 4, 3, buf_xsize=8, buf_ysize=3, resample_alg=gdal.GRIORA_Cubic)
    assert numpy.array_equal(ar_ds, ar_ds2)

    ar_bands = [mem_ds.GetRasterBand(i + 1).ReadAsArray(0, 0, 4, 3, buf_xsize=8, buf_ysize=3, resample_alg=gdal.GRIORA_Cubic) for i in range(4)]

    # Results of band or dataset RasterIO should be the same
    for i in range(4):
        assert numpy.array_equal(ar_ds[i], ar_bands[i])

    # First, second and third band should have identical content
    assert numpy.array_equal(ar_ds[0], ar_ds[1])

    # Alpha band should be different
    assert not numpy.array_equal(ar_ds[0], ar_ds[3])

###############################################################################
# Test cubic resampling with masking


def test_rasterio_13():
    numpy = pytest.importorskip('numpy')

    for dt in [gdal.GDT_Byte, gdal.GDT_UInt16, gdal.GDT_UInt32]:

        mem_ds = gdal.GetDriverByName('MEM').Create('', 4, 3, 1, dt)
        mem_ds.GetRasterBand(1).SetNoDataValue(0)
        mem_ds.GetRasterBand(1).WriteArray(numpy.array([[0, 0, 0, 0], [0, 255, 0, 0], [0, 0, 0, 0]]))

        ar_ds = mem_ds.ReadAsArray(0, 0, 4, 3, buf_xsize=8, buf_ysize=3, resample_alg=gdal.GRIORA_Cubic)

        expected_ar = numpy.array([[0, 0, 0, 0, 0, 0, 0, 0], [0, 255, 255, 255, 255, 0, 0, 0], [0, 0, 0, 0, 0, 0, 0, 0]])
        assert numpy.array_equal(ar_ds, expected_ar), dt


###############################################################################
# Test average downsampling by a factor of 2 on exact boundaries


def test_rasterio_14():

    gdal.FileFromMemBuffer('/vsimem/rasterio_14.asc',
                           """ncols        6
nrows        6
xllcorner    0
yllcorner    0
cellsize     0
  0   0   100 0   0   0
  0   100 0   0   0 100
  0   0   0   0 100   0
100   0 100   0   0   0
  0 100   0 100   0   0
  0   0   0   0   0 100""")

    ds = gdal.Translate('/vsimem/rasterio_14_out.asc', '/vsimem/rasterio_14.asc', options='-of AAIGRID -r average -outsize 50% 50%')
    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 110, ds.ReadAsArray()

    gdal.Unlink('/vsimem/rasterio_14.asc')
    gdal.Unlink('/vsimem/rasterio_14_out.asc')

    ds = gdal.GetDriverByName('MEM').Create('', 1000000, 1)
    ds.GetRasterBand(1).WriteRaster(ds.RasterXSize - 1, 0, 1, 1, struct.pack('B' * 1, 100))
    data = ds.ReadRaster(buf_xsize=int(ds.RasterXSize / 2), buf_ysize=1, resample_alg=gdal.GRIORA_Average)
    data = struct.unpack('B' * int(ds.RasterXSize / 2), data)
    assert data[-1:][0] == 50

    data = ds.ReadRaster(ds.RasterXSize - 2, 0, 2, 1, buf_xsize=1, buf_ysize=1, resample_alg=gdal.GRIORA_Average)
    data = struct.unpack('B' * 1, data)
    assert data[0] == 50

    ds = gdal.GetDriverByName('MEM').Create('', 1, 1000000)
    ds.GetRasterBand(1).WriteRaster(0, ds.RasterYSize - 1, 1, 1, struct.pack('B' * 1, 100))
    data = ds.ReadRaster(buf_xsize=1, buf_ysize=int(ds.RasterYSize / 2), resample_alg=gdal.GRIORA_Average)
    data = struct.unpack('B' * int(ds.RasterYSize / 2), data)
    assert data[-1:][0] == 50

    data = ds.ReadRaster(0, ds.RasterYSize - 2, 1, 2, buf_xsize=1, buf_ysize=1, resample_alg=gdal.GRIORA_Average)
    data = struct.unpack('B' * 1, data)
    assert data[0] == 50


###############################################################################
# Test average downsampling by a non-integer factor


def test_rasterio_average_4by4_to_3by3():

    gdal.FileFromMemBuffer('/vsimem/test_rasterio_average_4by4_to_3by3.asc',
                           """ncols        4
nrows        4
xllcorner    0
yllcorner    0
cellsize     1
 1.0 5 9 13
 2 6 10 14
 3 7 11 15
 4 8 12 16""")

    ds = gdal.Translate('', '/vsimem/test_rasterio_average_4by4_to_3by3.asc', options='-ot Float32 -f MEM -r average -outsize 3 3')
    data = ds.GetRasterBand(1).ReadRaster()
    assert struct.unpack('f' * 9, data) == (2.25, 7.25, 12.25,
                                            3.5, 8.5, 13.5,
                                            4.75, 9.75, 14.75)

    gdal.Unlink('/vsimem/test_rasterio_average_4by4_to_3by3.asc')

###############################################################################
# Test average oversampling by an integer factor (should behave like nearest)


def test_rasterio_15():

    gdal.FileFromMemBuffer('/vsimem/rasterio_15.asc',
                           """ncols        2
nrows        2
xllcorner    0
yllcorner    0
cellsize     0
  0   100
100   100""")

    ds = gdal.Translate('/vsimem/rasterio_15_out.asc', '/vsimem/rasterio_15.asc', options='-of AAIGRID -outsize 200% 200%')
    data_ref = ds.GetRasterBand(1).ReadRaster()
    ds = None
    ds = gdal.Translate('/vsimem/rasterio_15_out.asc', '/vsimem/rasterio_15.asc', options='-of AAIGRID -r average -outsize 200% 200%')
    data = ds.GetRasterBand(1).ReadRaster()
    cs = ds.GetRasterBand(1).Checksum()
    assert data == data_ref and cs == 134, ds.ReadAsArray()

    gdal.Unlink('/vsimem/rasterio_15.asc')
    gdal.Unlink('/vsimem/rasterio_15_out.asc')

###############################################################################
# Test mode downsampling by a factor of 2 on exact boundaries


def test_rasterio_16():

    gdal.FileFromMemBuffer('/vsimem/rasterio_16.asc',
                           """ncols        6
nrows        6
xllcorner    0
yllcorner    0
cellsize     0
  0   0   0   0   0   0
  2   100 0   0   0   0
100   100 0   0   0   0
  0   100 0   0   0   0
  0   0   0   0   0   0
  0   0   0   0   0  0""")

    ds = gdal.Translate('/vsimem/rasterio_16_out.asc', '/vsimem/rasterio_16.asc', options='-of AAIGRID -r mode -outsize 50% 50%')
    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 15, ds.ReadAsArray()

    gdal.Unlink('/vsimem/rasterio_16.asc')
    gdal.Unlink('/vsimem/rasterio_16_out.asc')

###############################################################################


def test_rasterio_nodata():
    pytest.importorskip('numpy')

    ndv = 123
    btype = [gdal.GDT_Byte, gdal.GDT_Int16, gdal.GDT_Int32, gdal.GDT_Float32, gdal.GDT_Float64]

    ### create a MEM dataset
    for src_type in btype:
        mem_ds = gdal.GetDriverByName('MEM').Create('', 10, 9, 1, src_type)
        mem_ds.GetRasterBand(1).SetNoDataValue(ndv)
        mem_ds.GetRasterBand(1).Fill(ndv)

        for dst_type in btype:
            if ( dst_type > src_type ):
                ### read to a buffer of a wider type (and resample)
                data = mem_ds.GetRasterBand(1).ReadAsArray(0, 0, 10, 9, 4, 3, resample_alg=gdal.GRIORA_Bilinear, buf_type=dst_type)
                assert int(data[0,0]) == ndv, 'did not read expected band data via ReadAsArray() - src type -> dst type: ' + str( src_type ) + ' -> ' + str( dst_type )


###############################################################################

def test_rasterio_lanczos_nodata():

    ds = gdal.Open('data/rasterio_lanczos_nodata.tif')

    data = ds.GetRasterBand(1).ReadRaster(buf_xsize=9,
                                          buf_ysize=9,
                                          resample_alg=gdal.GRIORA_Lanczos)
    data_ar = struct.unpack('H' * 9 * 9, data)
    expected_ar = (0, 0, 0, 22380, 22417, 22509, 22525, 22505, 22518,
                   0, 0, 0, 22415, 22432, 22433, 22541, 22541, 22568,
                   0, 0, 0, 22355, 22378, 22429, 22468, 22562, 22591,
                   0, 0, 0, 22271, 22343, 22384, 22526, 22565, 22699,
                   0, 0, 0, 22404, 22345, 22537, 22590, 22582, 22645,
                   0, 0, 0, 22461, 22484, 22464, 22495, 22633, 22638,
                   0, 0, 0, 22481, 22466, 22500, 22534, 22536, 22571,
                   0, 0, 0, 22460, 22460, 22547, 22538, 22456, 22572,
                   0, 0, 0, 0, 22504, 22496, 22564, 22563, 22610)
    assert data_ar == expected_ar

###############################################################################


def test_rasterio_resampled_value_is_nodata():

    gdal.FileFromMemBuffer('/vsimem/in.asc',
"""ncols        4
nrows        4
xllcorner    440720.000000000000
yllcorner    3750120.000000000000
cellsize     60.000000000000
nodata_value 0
 -1.1 -1.1 1.1 1.1
 -1.1 -1.1 1.1 1.1
 -1.1 -1.1 1.1 1.1
 -1.1 -1.1 1.1 1.1""")

    ds = gdal.Open('/vsimem/in.asc')

    data = ds.GetRasterBand(1).ReadRaster(buf_xsize=1,
                                          buf_ysize=1,
                                          resample_alg=gdal.GRIORA_Lanczos)
    data_ar = struct.unpack('f' * 1, data)
    expected_ar = (1.1754943508222875e-38, )
    assert data_ar == expected_ar

    data = ds.GetRasterBand(1).ReadRaster(buf_xsize=1,
                                          buf_ysize=1,
                                          resample_alg=gdal.GRIORA_Average)
    data_ar = struct.unpack('f' * 1, data)
    expected_ar = (1.1754943508222875e-38, )
    assert data_ar == expected_ar

    gdal.Unlink('/vsimem/in.asc')


    gdal.FileFromMemBuffer('/vsimem/in.asc',
"""ncols        4
nrows        4
xllcorner    440720.000000000000
yllcorner    3750120.000000000000
cellsize     60.000000000000
nodata_value 0
 -1 -1 1 1
 -1 -1 1 1
 -1 -1 1 1
 -1 -1 1 1""")

    ds = gdal.Open('/vsimem/in.asc')

    data = ds.GetRasterBand(1).ReadRaster(buf_xsize=1,
                                          buf_ysize=1,
                                          resample_alg=gdal.GRIORA_Lanczos)
    data_ar = struct.unpack('I' * 1, data)
    expected_ar = (1, )
    assert data_ar == expected_ar

    data = ds.GetRasterBand(1).ReadRaster(buf_xsize=1,
                                          buf_ysize=1,
                                          resample_alg=gdal.GRIORA_Average)
    data_ar = struct.unpack('I' * 1, data)
    expected_ar = (1, )
    assert data_ar == expected_ar

    gdal.Unlink('/vsimem/in.asc')



def test_rasterio_dataset_readarray_cint16():
    numpy = pytest.importorskip('numpy')

    mem_ds = gdal.GetDriverByName('MEM').Create('', 1, 1, 2, gdal.GDT_CInt16)
    mem_ds.GetRasterBand(1).WriteArray(numpy.array([[1 + 2j]]))
    mem_ds.GetRasterBand(2).WriteArray(numpy.array([[3 + 4j]]))
    got = mem_ds.GetRasterBand(1).ReadAsArray()
    assert got == numpy.array([[1 + 2j]])
    got = mem_ds.ReadAsArray()
    assert got[0] == numpy.array([[1 + 2j]])
    assert got[1] == numpy.array([[3 + 4j]])


def test_rasterio_rasterband_write_on_readonly():

    ds = gdal.Open('data/byte.tif')
    band = ds.GetRasterBand(1)
    with gdaltest.error_handler():
        err = band.WriteRaster(0, 0, 20, 20, band.ReadRaster())
    assert err != 0


def test_rasterio_dataset_write_on_readonly():

    ds = gdal.Open('data/byte.tif')
    with gdaltest.error_handler():
        err = ds.WriteRaster(0, 0, 20, 20, ds.ReadRaster())
    assert err != 0


@pytest.mark.parametrize('resample_alg', [-1, 8, "foo"])
def test_rasterio_dataset_invalid_resample_alg(resample_alg):

    mem_ds = gdal.GetDriverByName('MEM').Create('', 2, 2)
    with gdaltest.error_handler():
        with pytest.raises(Exception):
            assert mem_ds.ReadRaster(buf_xsize=1, buf_ysize=1, resample_alg=resample_alg) is None
        with pytest.raises(Exception):
            assert mem_ds.GetRasterBand(1).ReadRaster(buf_xsize=1, buf_ysize=1, resample_alg=resample_alg) is None
        with pytest.raises(Exception):
            assert mem_ds.ReadAsArray(buf_xsize=1, buf_ysize=1, resample_alg=resample_alg) is None
        with pytest.raises(Exception):
            assert mem_ds.GetRasterBand(1).ReadAsArray(buf_xsize=1, buf_ysize=1, resample_alg=resample_alg) is None


def test_rasterio_floating_point_window_no_resampling():
    """ Test fix for #3101 """

    ds = gdal.Translate('/vsimem/test.tif', gdal.Open('data/rgbsmall.tif'))
    assert ds.GetMetadataItem('INTERLEAVE', 'IMAGE_STRUCTURE') == 'PIXEL'

    # Check that GDALDataset::IRasterIO() in block-based strategy behaves the
    # same as GDALRasterBand::IRasterIO() generic case (ie the one dealing
    # with floating-point window coordinates)
    data_per_band = b''.join( ds.GetRasterBand(i+1).ReadRaster(0.1,0.2,10.4,11.4,10,11) for i in range(3) )
    data_per_dataset = ds.ReadRaster(0.1,0.2,10.4,11.4,10,11)
    ds = None
    gdal.Unlink('/vsimem/test.tif')
    assert data_per_band == data_per_dataset


def test_rasterio_floating_point_window_no_resampling_numpy():
    # Same as above but using ReadAsArray() instead of ReadRaster()
    numpy = pytest.importorskip('numpy')

    ds = gdal.Translate('/vsimem/test.tif', gdal.Open('data/rgbsmall.tif'))
    assert ds.GetMetadataItem('INTERLEAVE', 'IMAGE_STRUCTURE') == 'PIXEL'

    data_per_band = numpy.stack([ds.GetRasterBand(i+1).ReadAsArray(0.1,0.2,10.4,11.4,buf_xsize=10,buf_ysize=11) for i in range(3)])
    data_per_dataset = ds.ReadAsArray(0.1,0.2,10.4,11.4,buf_xsize=10,buf_ysize=11)
    ds = None
    gdal.Unlink('/vsimem/test.tif')
    assert numpy.array_equal(data_per_band, data_per_dataset)


###############################################################################
# Test average downsampling by a factor of 2 on exact boundaries, with uint16 data type


def test_rasterio_average_halfsize_downsampling_uint16():

    v1 = 65535
    v2 = 65535
    v3 = 65535
    v4 = 65535
    m1 = (v1 + v2 + v3 + v4 + 2) >> 2

    v5 = 65535
    v6 = 2
    v7 = 0
    v8 = 0
    m2 = (v5 + v6 + v7 + v8 + 2) >> 2

    v9 = 32767
    v10 = 32767
    v11 = 32767
    v12 = 32767
    m3 = (v9 + v10 + v11 + v12 + 2) >> 2

    v13 = 1
    v14 = 0
    v15 = 1
    v16 = 1
    m4 = (v13 + v14 + v15 + v16 + 2) >> 2
    ds = gdal.GetDriverByName('MEM').Create('', 18, 2, 1, gdal.GDT_UInt16)
    ds.WriteRaster(0, 0, 18, 2,
                   struct.pack('H' * 18 * 2,
                               v1, v2, v5, v6, v9,  v10, v13, v14, v5, v6, v9,  v10, v13, v14, v1, v2, v5, v6,
                               v3, v4, v7, v8, v11, v12, v15, v16, v7, v8, v11, v12, v15, v16, v3, v4, v7, v8))
    # Ask for at least 8 output pixels in width to trigger SSE2 optim
    data = ds.GetRasterBand(1).ReadRaster(0, 0, 18, 2, 9, 1, resample_alg = gdal.GRIORA_Average)
    assert struct.unpack('H' * 9, data) == (m1, m2, m3, m4, m2, m3, m4, m1, m2)

    ds.BuildOverviews('AVERAGE', [2])
    ovr_data = ds.GetRasterBand(1).GetOverview(0).ReadRaster()
    assert ovr_data == data

###############################################################################
# Test average downsampling by a factor of 2 on exact boundaries, with float32 data type


def test_rasterio_average_halfsize_downsampling_float32():

    ds = gdal.GetDriverByName('MEM').Create('', 18, 4, 1, gdal.GDT_Float32)
    ds.WriteRaster(0, 0, 18, 4,
                   struct.pack('f' * 18 * 4,
                               0,     0,     0, 0, 65535, 65535, 0,     0,     0, 0, 65535, 65535, 0,     0,     0, 0, 65535, 65535,
                               2,     65535, 0, 0, 65535, 65535, 2,     65535, 0, 0, 65535, 65535, 2,     65535, 0, 0, 65535, 65535,
                               65535, 65535, 0, 0, 0,     0,     65535, 65535, 0, 0, 0,     0,     65535, 65535, 0, 0, 0,     0,
                               0,     65535, 0, 0, 0,     0,     0,     65535, 0, 0, 0,     0,     0,     65535, 0, 0, 0,     0))
    # Ask for at least 8 output pixels in width to trigger SSE2 optim
    data = ds.GetRasterBand(1).ReadRaster(0, 0, 18, 4, 9, 2, resample_alg = gdal.GRIORA_Average)
    assert struct.unpack('f' * 18, data) == pytest.approx(
                                           (16384.25, 0.0, 65535.0, 16384.25, 0.0, 65535.0, 16384.25, 0.0, 65535.0,
                                            49151.25, 0.0, 0.0,     49151.25, 0.0, 0.0,     49151.25, 0.0, 0.0), rel=1e-10)

    ds.BuildOverviews('AVERAGE', [2])
    ovr_data = ds.GetRasterBand(1).GetOverview(0).ReadRaster()
    assert ovr_data == data

###############################################################################
# Test rms downsampling by a factor of 2 on exact boundaries, with float data type


def test_rasterio_rms_halfsize_downsampling_float32():

    ds = gdal.GetDriverByName('MEM').Create('', 18, 4, 1, gdal.GDT_Float32)
    ds.WriteRaster(0, 0, 18, 4,
                   struct.pack('f' * 18 * 4,
                               0,     0,     0, 0, 65535, 65535, 0,     0,     0, 0, 65535, 65535, 0,     0,     0, 0, 65535, 65535,
                               2,     65535, 0, 0, 65535, 65535, 2,     65535, 0, 0, 65535, 65535, 2,     65535, 0, 0, 65535, 65535,
                               65535, 65535, 0, 0, 0,     0,     65535, 65535, 0, 0, 0,     0,     65535, 65535, 0, 0, 0,     0,
                               0,     65535, 0, 0, 0,     0,     0,     65535, 0, 0, 0,     0,     0,     65535, 0, 0, 0,     0))
    # Ask for at least 8 output pixels in width to trigger SSE2 optim
    data = ds.GetRasterBand(1).ReadRaster(0, 0, 18, 4, 9, 2, resample_alg = gdal.GRIORA_RMS)
    assert struct.unpack('f' * 18, data) == pytest.approx(
                                           (32767.5,       0, 65535, 32767.5,       0, 65535, 32767.5,       0, 65535,
                                            56754.9765625, 0, 0,     56754.9765625, 0, 0,     56754.9765625, 0, 0), rel=1e-10)

    ds.BuildOverviews('RMS', [2])
    ovr_data = ds.GetRasterBand(1).GetOverview(0).ReadRaster()
    assert ovr_data == data

###############################################################################
# Test rms downsampling by a factor of 2 on exact boundaries, with byte data type

def internal_test_rasterio_rms_halfsize_downsampling_byte_content(gdal_dt, struct_type):

    ds = gdal.GetDriverByName('MEM').Create('', 38, 6, 1, gdal_dt)
    ds.WriteRaster(0, 0, 38, 6,
        struct.pack(struct_type * 38 * 6,
        0,   0,   1, 0, 1, 1,     0, 0,   0, 0,    1, 0, 1, 1,     0, 0,   0, 0,   0,   0,   1, 0, 1, 1,     0, 0,   0, 0,    1, 0, 1, 1,     0, 0,   0, 0,   0,   0,
        2,   100, 0, 0, 0, 0,     2, 100, 2, 100,  0, 0, 0, 0,     2, 100, 2, 100, 2,   100, 0, 0, 0, 0,     2, 100, 2, 100,  0, 0, 0, 0,     2, 100, 2, 100, 2,   100,
        100, 100, 1, 1, 1, 1,     0, 0,   0, 0,    1, 1, 1, 1,     0, 0,   0, 0,   100, 100, 1, 1, 1, 1,     0, 0,   0, 0,    1, 1, 1, 1,     0, 0,   0, 0,   100, 100,
        0,   100, 1, 0, 1, 1,     0, 0,   0, 0,    1, 0, 1, 1,     0, 0,   0, 0,   0,   100, 1, 0, 1, 1,     0, 0,   0, 0,    1, 0, 1, 1,     0, 0,   0, 0,   0,   100,
        188, 229, 0, 0, 255, 255, 0, 0,   0, 0,    0, 0, 255, 255, 0, 0,   0, 0,   0,   0,   0, 0, 255, 255, 0, 0,   0, 0,    0, 0, 255, 255, 0, 0,   0, 0,   188, 229,
        233, 233, 0, 0, 255, 255, 0, 0,   0, 0,    0, 0, 255, 255, 0, 0,   0, 0,   0,   0,   0, 0, 255, 255, 0, 0,   0, 0,    0, 0, 255, 255, 0, 0,   0, 0,   233, 233 ))
    # Make sure to request at least 16 pixels in width to test the SSE2 implementation
    data = ds.GetRasterBand(1).ReadRaster(0, 0, 38, 6, 19, 3, resample_alg = gdal.GRIORA_RMS)
    assert struct.unpack(struct_type * 19 * 3, data) == \
        (50, 0, 0,   50, 50, 0, 0,   50, 50, 50, 0, 0,   50, 50, 0, 0,   50, 50, 50,
         87, 1, 1,   0,  0,  1, 1,   0,  0,  87, 1, 1,   0,  0,  1, 1,   0,  0,  87,
         222,0, 255, 0,  0,  0, 255, 0,  0,  0,  0, 255, 0,  0,  0, 255, 0,  0,  222)

    ds.BuildOverviews('RMS', [2])
    ovr_data = ds.GetRasterBand(1).GetOverview(0).ReadRaster()
    assert ovr_data == data

def test_rasterio_rms_halfsize_downsampling_byte():

    internal_test_rasterio_rms_halfsize_downsampling_byte_content(gdal.GDT_Byte, 'B')

###############################################################################
# Test rms downsampling by a factor of 2 on exact boundaries, with byte data type
# and nodata never hit

def test_rasterio_rms_halfsize_downsampling_byte_nodata_not_hit():

    ds = gdal.GetDriverByName('MEM').Create('', 20, 6, 1, gdal.GDT_Byte)
    ds.GetRasterBand(1).SetNoDataValue(180)
    ds.WriteRaster(0, 0, 20, 6,
                   struct.pack('B' * 20 * 6,
                               0,   0,   1, 0, 1, 1,     0, 0,   0, 0,    1, 0, 1, 1,     0, 0,   0, 0,   0,   0,
                               2,   100, 0, 0, 0, 0,     2, 100, 2, 100,  0, 0, 0, 0,     2, 100, 2, 100, 2,   100,
                               100, 100, 1, 1, 1, 1,     0, 0,   0, 0,    1, 1, 1, 1,     0, 0,   0, 0,   100, 100,
                               0,   100, 1, 0, 1, 1,     0, 0,   0, 0,    1, 0, 1, 1,     0, 0,   0, 0,   0,   100,
                               0,   0,   0, 0, 255, 255, 0, 0,   0, 0,    0, 0, 255, 255, 0, 0,   0, 0,   0,   0,
                               0,   0,   0, 0, 255, 255, 0, 0,   0, 0,    0, 0, 255, 255, 0, 0,   0, 0,   0,   0))
    # Make sure to request at least 8 pixels in width to test the SSE2 implementation
    data = ds.GetRasterBand(1).ReadRaster(0, 0, 20, 6, 10, 3, resample_alg = gdal.GRIORA_RMS)
    assert struct.unpack('B' * 30, data) == (50, 0, 0,   50, 50, 0, 0,   50, 50, 50,
                                             87, 1, 1,   0,  0,  1, 1,   0,  0,  87,
                                             0,  0, 255, 0,  0,  0, 255, 0,  0,  0)

    ds.BuildOverviews('RMS', [2])
    ovr_data = ds.GetRasterBand(1).GetOverview(0).ReadRaster()
    assert ovr_data == data

###############################################################################
# Test rms downsampling by a factor of 2 on exact boundaries, with uint16 data type


def test_rasterio_rms_halfsize_downsampling_uint16():

    ds = gdal.GetDriverByName('MEM').Create('', 18, 4, 1, gdal.GDT_UInt16)
    ds.WriteRaster(0, 0, 18, 4,
                   struct.pack('H' * 18 * 4,
                               0,     0,     0, 0, 65535, 65535, 0,     0,     0, 0, 65535, 65535, 0,     0,     0, 0, 65535, 65535,
                               2,     65535, 0, 0, 65535, 65535, 2,     65535, 0, 0, 65535, 65535, 2,     65535, 0, 0, 65535, 65535,
                               65535, 65535, 0, 0, 0,     0,     65535, 65535, 0, 0, 0,     0,     65535, 65535, 0, 0, 0,     0,
                               0,     65535, 0, 0, 0,     0,     0,     65535, 0, 0, 0,     0,     0,     65535, 0, 0, 0,     0))
    # Ask for at least 4 output pixels in width to trigger SSE2 optim
    data = ds.GetRasterBand(1).ReadRaster(0, 0, 18, 4, 9, 2, resample_alg = gdal.GRIORA_RMS)
    assert struct.unpack('H' * 18, data) == (32768, 0, 65535, 32768, 0, 65535, 32768, 0, 65535,
                                             56755, 0, 0,     56755, 0, 0,     56755, 0, 0)


###############################################################################
# Test rms downsampling by a factor of 2 on exact boundaries, with uint16 data type
# and all values fitting on 14 bits


def test_rasterio_rms_halfsize_downsampling_uint16_fits_in_14bits():

    ds = gdal.GetDriverByName('MEM').Create('', 8, 2, 1, gdal.GDT_UInt16)
    ds.WriteRaster(0, 0, 8, 2,
                   struct.pack('H' * 8 * 2,
                               10, 9,  16383, 16383, 1, 0, 16380, 16380,
                               10, 10, 16383, 16383, 1, 1, 16378, 16380))
    # Ask for at least 4 output pixels in width to trigger SSE2 optim
    data = ds.GetRasterBand(1).ReadRaster(0, 0, 8, 2, 4, 1, resample_alg = gdal.GRIORA_RMS)
    assert struct.unpack('H' * 4, data) == (10, 16383, 1, 16380)

###############################################################################
# Test rms downsampling by a factor of 2 on exact boundaries, with uint16 data type
# but with content as in test_rasterio_rms_halfsize_downsampling_byte to
# check UInt16 resampling is consistent with Byte one

def test_rasterio_rms_halfsize_downsampling_uint16_with_byte_content():

    internal_test_rasterio_rms_halfsize_downsampling_byte_content(gdal.GDT_UInt16, 'H')


###############################################################################
# Test rms downsampling by a factor of 2. / 3, with float data type


def test_rasterio_rms_two_third_downsampling_float32():

    ds = gdal.GetDriverByName('MEM').Create('', 6, 6, 1, gdal.GDT_Float32)
    ds.WriteRaster(0, 0, 6, 6,
                   struct.pack('f' * 6 * 6,
                               0,   0,   0, 0, 0, 0,
                               2,   100, 0, 0, 0, 0,
                               100, 100, 0, 0, 0, 0,
                               0,   100, 0, 0, 0, 0,
                               0,   0,   0, 0, 0, 0,
                               0,   0,   0, 0, 0, 0))
    data = ds.GetRasterBand(1).ReadRaster(0, 0, 6, 6, 4, 4, resample_alg = gdal.GRIORA_RMS)
    assert struct.unpack('f' * 16, data) == pytest.approx(
                                            (33.34666442871094, 33.33333206176758, 0.0, 0.0,
                                             88.19674682617188, 57.73502731323242, 0.0, 0.0,
                                             47.14045333862305, 47.14045333862305, 0.0, 0.0,
                                             0.0, 0.0, 0.0, 0.0), rel=1e-10)

###############################################################################
# Test rms downsampling by a factor of 2 on exact boundaries, with CFloat32


def test_rasterio_rms_halfsize_downsampling_cfloat32():

    # Test real part
    ds = gdal.GetDriverByName('MEM').Create('', 4, 6, 1, gdal.GDT_CFloat32)
    ds.WriteRaster(0, 0, 4, 6,
                   struct.pack('f' * 2 * 4 * 6,
                               0,   0,  0,   0, 0, 0, 0, 0,
                               2,   0,  100, 0, 0, 0, 0, 0,
                               100, 0,  100, 0, 0, 0, 0, 0,
                               0,   0,  100, 0, 0, 0, 0, 0,
                               0,   0,  0,   0, 0, 0, 0, 0,
                               0,   0,  0,   0, 0, 0, 0, 0))
    # This will go through the warping code internally
    data = ds.GetRasterBand(1).ReadRaster(0, 0, 4, 6, 2, 3, resample_alg = gdal.GRIORA_RMS)
    assert struct.unpack('f' * 2 * 2 * 3, data) == pytest.approx(
                                           (50.0099983215332, 0., 0., 0.,
                                            86.6025390625,    0., 0., 0.,
                                            0.,               0., 0., 0.), rel = 1e-10)

    # This will use the overview code
    ds.BuildOverviews('RMS', [2])
    ovr_data = ds.GetRasterBand(1).GetOverview(0).ReadRaster()
    assert ovr_data == data


    # Test imaginary part
    ds = gdal.GetDriverByName('MEM').Create('', 4, 6, 1, gdal.GDT_CFloat32)
    ds.WriteRaster(0, 0, 4, 6,
                   struct.pack('f' * 2 * 4 * 6,
                               0, 0,   0,  0,   0, 0, 0, 0,
                               0, 2,   0,  100, 0, 0, 0, 0,
                               0, 100, 0,  100, 0, 0, 0, 0,
                               0, 0,   0,  100, 0, 0, 0, 0,
                               0, 0,   0,  0,   0, 0, 0, 0,
                               0, 0,   0,  0,   0, 0, 0, 0))
    # This will go through the warping code internally
    data = ds.GetRasterBand(1).ReadRaster(0, 0, 4, 6, 2, 3, resample_alg = gdal.GRIORA_RMS)
    assert struct.unpack('f' * 2 * 2 * 3, data) == pytest.approx(
                                           (0.0, 50.0099983215332, 0., 0.,
                                            0.0, 86.6025390625,    0., 0.,
                                            0.0, 0.,               0., 0.), rel = 1e-10)

    # This will use the overview code
    ds.BuildOverviews('RMS', [2])
    ovr_data = ds.GetRasterBand(1).GetOverview(0).ReadRaster()
    assert ovr_data == data

###############################################################################
# Test WriteRaster() on a bytearray


def test_rasterio_writeraster_from_bytearray():

    ds = gdal.GetDriverByName('MEM').Create('', 1, 2)
    ar = bytearray([1, 2])
    ds.WriteRaster(0, 0, 1, 2, ar)
    assert ds.ReadRaster() == ar

###############################################################################
# Test WriteRaster() on a memoryview


def test_rasterio_writeraster_from_memoryview():

    ds = gdal.GetDriverByName('MEM').Create('', 1, 2)
    ar = memoryview(bytearray([1, 2, 3]))[1:]
    ds.WriteRaster(0, 0, 1, 2, ar)
    assert ds.ReadRaster() == ar

###############################################################################
# Test ReadRaster() in an existing buffer


def test_rasterio_readraster_in_existing_buffer():

    ds = gdal.GetDriverByName('MEM').Create('', 2, 1)
    ar = bytearray([1, 2])
    ds.WriteRaster(0, 0, 2, 1, ar)
    band = ds.GetRasterBand(1)

    # buf_obj is of expected size
    assert ds.ReadRaster(buf_obj = bytearray([0, 0])) == ar
    # buf_obj is larger than expected
    assert ds.ReadRaster(buf_obj = bytearray([0, 0, 10])) == bytearray([1, 2, 10])
    with gdaltest.error_handler():
        # buf_obj is a wrong object type
        assert ds.ReadRaster(buf_obj = 123) is None
        # buf_obj is not large enough
        assert ds.ReadRaster(buf_obj = bytearray([0])) is None
        # buf_obj is read-only
        assert ds.ReadRaster(buf_obj = bytes(bytearray([0, 0]))) is None


    # buf_obj is of expected size
    assert band.ReadRaster(buf_obj = bytearray([0, 0])) == ar
    # buf_obj is larger than expected
    assert band.ReadRaster(buf_obj = bytearray([0, 0, 10])) == bytearray([1, 2, 10])
    with gdaltest.error_handler():
        # buf_obj is a wrong object type
        assert band.ReadRaster(buf_obj = 123) is None
        # buf_obj is not large enough
        assert band.ReadRaster(buf_obj = bytearray([0])) is None
        # buf_obj is read-only
        assert band.ReadRaster(buf_obj = bytes(bytearray([0, 0]))) is None

###############################################################################
# Test ReadBlock() in an existing buffer


def test_rasterio_readblock_in_existing_buffer():

    ds = gdal.GetDriverByName('MEM').Create('', 2, 1)
    ar = bytearray([1, 2])
    ds.WriteRaster(0, 0, 2, 1, ar)
    band = ds.GetRasterBand(1)

    assert band.ReadBlock(0, 0) == ar

    # buf_obj is of expected size
    assert band.ReadBlock(0, 0, buf_obj = bytearray([0, 0])) == ar
    # buf_obj is larger than expected
    assert band.ReadBlock(0, 0, buf_obj = bytearray([0, 0, 10])) == bytearray([1, 2, 10])
    with gdaltest.error_handler():
        # buf_obj is a wrong object type
        assert band.ReadBlock(0, 0, buf_obj = 123) is None
        # buf_obj is not large enough
        assert band.ReadBlock(0, 0, buf_obj = bytearray([0])) is None
        # buf_obj is read-only
        assert band.ReadBlock(0, 0, buf_obj = bytes(bytearray([0, 0]))) is None

###############################################################################
# Test ReadRaster() in an existing buffer and alignment issues


@pytest.mark.parametrize('datatype', [gdal.GDT_Int16,
                                      gdal.GDT_UInt16,
                                      gdal.GDT_Int32,
                                      gdal.GDT_UInt32,
                                      gdal.GDT_Float32,
                                      gdal.GDT_Float64,
                                      gdal.GDT_CInt16,
                                      gdal.GDT_CInt32,
                                      gdal.GDT_CFloat32,
                                      gdal.GDT_CFloat64],
                          ids=gdal.GetDataTypeName)
def test_rasterio_readraster_in_existing_buffer_alignment_issues(datatype):

    ds = gdal.GetDriverByName('MEM').Create('', 2, 1, 1, datatype)
    band = ds.GetRasterBand(1)
    band.Fill(1)
    ar = band.ReadRaster()
    buffer_size = 2 * 1 * (gdal.GetDataTypeSize(datatype) // 8)

    # buf_obj has appropriate alignment
    assert ds.ReadRaster(buf_obj = bytearray([0] * buffer_size)) == ar

    with gdaltest.error_handler():
        # buf_obj has not appropriate alignment
        assert ds.ReadRaster(buf_obj = memoryview(bytearray([0] * (buffer_size + 1)))[1:]) is None

    # buf_obj has appropriate alignment
    assert band.ReadRaster(buf_obj = bytearray([0] * buffer_size)) == ar

    with gdaltest.error_handler():
        # buf_obj has not appropriate alignment
        assert band.ReadRaster(buf_obj = memoryview(bytearray([0] * (buffer_size + 1)))[1:]) is None

    # buf_obj has appropriate alignment
    assert band.ReadBlock(0, 0, buf_obj = bytearray([0] * buffer_size)) == ar

    with gdaltest.error_handler():
        # buf_obj has not appropriate alignment
        assert band.ReadBlock(0, 0, buf_obj = memoryview(bytearray([0] * (2 * 8 + 1)))[1:]) is None
