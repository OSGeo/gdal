#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test default implementation of GDALRasterBand::IRasterIO
# Author:   Even Rouault <even dot rouault at mines dash paris dot org>
#
###############################################################################
# Copyright (c) 2008-2013, Even Rouault <even dot rouault at mines-paris dot org>
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
    if abs(pct - (user_data[0] + 0.05)) > 1e-5:
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
    assert abs(tab[0] - 1) <= 1e-5 and tab[1]

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
    assert abs(tab[0] - 1) <= 1e-5 and tab[1]

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
    assert abs(tab[0] - 1) <= 1e-5 and tab[1]

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
    assert abs(tab[0] - 1) <= 1e-5 and tab[1]

    ds = None

    # Test Dataset.ReadRaster on a multi band file, with INTERLEAVE=BAND
    ds = gdal.Open('data/rgbsmall.tif')
    last_pct = [0]
    data = ds.ReadRaster(resample_alg=gdal.GRIORA_NearestNeighbour,
                         callback=rasterio_8_progress_callback_2,
                         callback_data=last_pct)
    assert not (data is None or abs(last_pct[0] - 1.0) > 1e-5)

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
    assert not (data is None or abs(last_pct[0] - 1.0) > 1e-5)

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

    assert abs(tab[0] - 1.0) <= 1e-5

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

    assert abs(tab[0] - 1.0) <= 1e-5

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

    assert abs(tab[0] - 1.0) <= 1e-5

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

    assert abs(tab[0] - 1.0) <= 1e-5

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

    assert abs(tab[0] - 1.0) <= 1e-5

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

    assert abs(tab[0] - 1.0) <= 1e-5

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

    assert abs(tab[0] - 1.0) <= 1e-5

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

    assert abs(tab[0] - 1.0) <= 1e-5

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

    assert abs(tab[0] - 1.0) <= 1e-5
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
    assert abs(tab[0] - 1.0) <= 1e-5

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

    try:
        from osgeo import gdalnumeric
        gdalnumeric.zeros
        import numpy
    except (ImportError, AttributeError):
        pytest.skip()

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

    try:
        from osgeo import gdalnumeric
        gdalnumeric.zeros
        import numpy
    except (ImportError, AttributeError):
        pytest.skip()

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

    try:
        from osgeo import gdalnumeric
        gdalnumeric.zeros
        import numpy
    except (ImportError, AttributeError):
        pytest.skip()

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

    try:
        from osgeo import gdalnumeric
        gdalnumeric.zeros
        import numpy
    except (ImportError, AttributeError):
        pytest.skip()

    mem_ds = gdal.GetDriverByName('MEM').Create('', 1, 1, 2, gdal.GDT_CInt16)
    mem_ds.GetRasterBand(1).WriteArray(numpy.array([[1 + 2j]]))
    mem_ds.GetRasterBand(2).WriteArray(numpy.array([[3 + 4j]]))
    got = mem_ds.GetRasterBand(1).ReadAsArray()
    assert got == numpy.array([[1 + 2j]])
    got = mem_ds.ReadAsArray()
    assert got[0] == numpy.array([[1 + 2j]])
    assert got[1] == numpy.array([[3 + 4j]])

