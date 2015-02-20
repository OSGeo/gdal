#!/usr/bin/env python
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

import sys

sys.path.append( '../pymod' )

import gdaltest
from osgeo import gdal

###############################################################################
# Test writing a 1x1 buffer to a 10x6 raster and read it back

def rasterio_1():
    data = 'A'.encode('ascii')

    drv = gdal.GetDriverByName('GTiff')
    ds = drv.Create('tmp/rasterio1.tif', 10, 6, 1)

    ds.GetRasterBand(1).Fill(65)
    checksum = ds.GetRasterBand(1).Checksum()

    ds.GetRasterBand(1).Fill(0)

    ds.WriteRaster(0, 0, ds.RasterXSize, ds.RasterYSize, data, buf_type = gdal.GDT_Byte, buf_xsize=1, buf_ysize=1)
    if checksum != ds.GetRasterBand(1).Checksum():
        gdaltest.post_reason( 'Didnt get expected checksum ')
        return 'fail'

    data2 = ds.ReadRaster(0, 0, ds.RasterXSize, ds.RasterYSize, 1, 1)
    if data2 != data:
        gdaltest.post_reason( 'Didnt get expected buffer ')
        return 'fail'

    ds = None
    drv.Delete('tmp/rasterio1.tif')

    return 'success'

###############################################################################
# Test writing a 5x4 buffer to a 10x6 raster and read it back

def rasterio_2():
    data = 'AAAAAAAAAAAAAAAAAAAA'.encode('ascii')

    drv = gdal.GetDriverByName('GTiff')
    ds = drv.Create('tmp/rasterio2.tif', 10, 6, 1)

    ds.GetRasterBand(1).Fill(65)
    checksum = ds.GetRasterBand(1).Checksum()

    ds.GetRasterBand(1).Fill(0)

    ds.WriteRaster(0, 0, ds.RasterXSize, ds.RasterYSize, data, buf_type = gdal.GDT_Byte, buf_xsize=5, buf_ysize=4)
    if checksum != ds.GetRasterBand(1).Checksum():
        gdaltest.post_reason( 'Didnt get expected checksum ')
        return 'fail'

    data2 = ds.ReadRaster(0, 0, ds.RasterXSize, ds.RasterYSize, 5, 4)
    if data2 != data:
        gdaltest.post_reason( 'Didnt get expected buffer ')
        return 'fail'

    ds = None
    drv.Delete('tmp/rasterio2.tif')

    return 'success'

###############################################################################
# Test extensive read & writes into a non tiled raster

def rasterio_3():

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
                l = 0
                while l < ds.RasterYSize - j:
                    for xsize in range(5):
                        for ysize in range(4):
                            ds.GetRasterBand(1).Fill(0)
                            ds.WriteRaster(i, j, k + 1, l + 1, data[xsize][ysize],
                                           buf_type = gdal.GDT_Byte,
                                           buf_xsize=xsize + 1, buf_ysize=ysize + 1)
                            data2 = ds.ReadRaster(i, j, k + 1, l + 1, xsize + 1, ysize + 1, gdal.GDT_Byte)
                            if data2 != data[xsize][ysize]:
                                gdaltest.post_reason( 'Didnt get expected buffer ')
                                return 'fail'
                    l = l + 1
                k = k + 1
            j = j + 1
        i = i + 1

    ds = None
    drv.Delete('tmp/rasterio3.tif')

    return 'success'

###############################################################################
# Test extensive read & writes into a tiled raster

def rasterio_4():

    data = [ '' for i in range(5 * 4)]
    for size in range(5 * 4):
        for k in range(size+1):
            data[size] = data[size] + 'A'
        data[size] = data[size].encode('ascii')

    drv = gdal.GetDriverByName('GTiff')
    ds = drv.Create('tmp/rasterio4.tif', 20, 20, 1, options = [ 'TILED=YES', 'BLOCKXSIZE=16', 'BLOCKYSIZE=16' ])

    i = 0
    while i < ds.RasterXSize:
        j = 0
        while j < ds.RasterYSize:
            k = 0
            while k < ds.RasterXSize - i:
                l = 0
                while l < ds.RasterYSize - j:
                    for xsize in range(5):
                        for ysize in range(4):
                            ds.GetRasterBand(1).Fill(0)
                            ds.WriteRaster(i, j, k + 1, l + 1, data[(xsize + 1) * (ysize + 1) - 1],
                                           buf_type = gdal.GDT_Byte,
                                           buf_xsize=xsize + 1, buf_ysize=ysize + 1)
                            data2 = ds.ReadRaster(i, j, k + 1, l + 1, xsize + 1, ysize + 1, gdal.GDT_Byte)
                            if data2 != data[(xsize + 1) * (ysize + 1) - 1]:
                                gdaltest.post_reason( 'Didnt get expected buffer ')
                                print(i,j,k,l,xsize,ysize)
                                print(data2)
                                print(data[(xsize + 1) * (ysize + 1) - 1])
                                return 'fail'
                    l = l + 1
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

    return 'success'

###############################################################################
# Test error cases of ReadRaster()

def rasterio_5():

    ds = gdal.Open('data/byte.tif')

    for obj in [ds, ds.GetRasterBand(1)]:
        obj.ReadRaster(0,0,-2000000000,1,1,1)
        obj.ReadRaster(0,0,1,-2000000000,1,1)

    for band_number in [-1,0,2]:
        gdal.ErrorReset()
        gdal.PushErrorHandler('CPLQuietErrorHandler')
        res = ds.ReadRaster(0,0,1,1,band_list=[band_number])
        gdal.PopErrorHandler()
        error_msg = gdal.GetLastErrorMsg()
        if res is not None:
            gdaltest.post_reason('expected None')
            return 'fail'
        if error_msg.find('this band does not exist on dataset') == -1:
            gdaltest.post_reason('did not get expected error msg')
            print(error_msg)
            return 'fail'

    res = ds.ReadRaster(0,0,1,1,band_list=[1,1])
    if res is None:
        gdaltest.post_reason('expected non None')
        return 'fail'

    for obj in [ds, ds.GetRasterBand(1)]:
        gdal.ErrorReset()
        gdal.PushErrorHandler('CPLQuietErrorHandler')
        res = obj.ReadRaster(0,0,21,21)
        gdal.PopErrorHandler()
        error_msg = gdal.GetLastErrorMsg()
        if res is not None:
            gdaltest.post_reason('expected None')
            return 'fail'
        if error_msg.find('Access window out of range in RasterIO()') == -1:
            gdaltest.post_reason('did not get expected error msg (1)')
            print(error_msg)
            return 'fail'

        # This should only fail on a 32bit build
        try:
            maxsize = sys.maxint
        except:
            maxsize = sys.maxsize

        # On win64, maxsize == 2147483647 and ReadRaster()
        # fails because of out of memory condition, not
        # because of integer overflow. I'm not sure on how
        # to detect win64 better.
        if maxsize == 2147483647 and sys.platform != 'win32':
            gdal.ErrorReset()
            gdal.PushErrorHandler('CPLQuietErrorHandler')
            res = obj.ReadRaster(0,0,1,1,1000000,1000000)
            gdal.PopErrorHandler()
            error_msg = gdal.GetLastErrorMsg()
            if res is not None:
                gdaltest.post_reason('expected None')
                return 'fail'
            if error_msg.find('Integer overflow') == -1:
                gdaltest.post_reason('did not get expected error msg (2)')
                print(error_msg)
                return 'fail'

        gdal.ErrorReset()
        gdal.PushErrorHandler('CPLQuietErrorHandler')
        res = obj.ReadRaster(0,0,0,1)
        gdal.PopErrorHandler()
        error_msg = gdal.GetLastErrorMsg()
        if res is not None:
            gdaltest.post_reason('expected None')
            return 'fail'
        if error_msg.find('Illegal values for buffer size') == -1:
            gdaltest.post_reason('did not get expected error msg (3)')
            print(error_msg)
            return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test error cases of WriteRaster()

def rasterio_6():

    ds = gdal.GetDriverByName('MEM').Create('', 2, 2)

    for obj in [ds, ds.GetRasterBand(1)]:
        try:
            obj.WriteRaster(0,0,2,2,None)
            gdaltest.post_reason('expected exception')
            return 'fail'
        except:
            pass

        gdal.ErrorReset()
        gdal.PushErrorHandler('CPLQuietErrorHandler')
        obj.WriteRaster(0,0,2,2,' ')
        gdal.PopErrorHandler()
        error_msg = gdal.GetLastErrorMsg()
        if error_msg.find('Buffer too small') == -1:
            gdaltest.post_reason('did not get expected error msg (1)')
            print(error_msg)
            return 'fail'

        gdal.ErrorReset()
        gdal.PushErrorHandler('CPLQuietErrorHandler')
        obj.WriteRaster(-1,0,1,1,' ')
        gdal.PopErrorHandler()
        error_msg = gdal.GetLastErrorMsg()
        if error_msg.find('Access window out of range in RasterIO()') == -1:
            gdaltest.post_reason('did not get expected error msg (2)')
            print(error_msg)
            return 'fail'

        gdal.ErrorReset()
        gdal.PushErrorHandler('CPLQuietErrorHandler')
        obj.WriteRaster(0,0,0,1,' ')
        gdal.PopErrorHandler()
        error_msg = gdal.GetLastErrorMsg()
        if error_msg.find('Illegal values for buffer size') == -1:
            gdaltest.post_reason('did not get expected error msg (3)')
            print(error_msg)
            return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test that default window reading works via ReadRaster()

def rasterio_7():

    ds = gdal.Open('data/byte.tif')
    
    data = ds.GetRasterBand(1).ReadRaster()
    l = len(data)
    if l != 400:
        gdaltest.post_reason('did not read expected band data via ReadRaster()')
        return 'fail'

    data = ds.ReadRaster()
    l = len(data)
    if l != 400:
        gdaltest.post_reason('did not read expected dataset data via ReadRaster()')
        return 'fail'

    return 'success'

###############################################################################
# Test callback of ReadRaster()

def rasterio_8_progress_callback(pct, message, user_data):
    if abs(pct - (user_data[0] + 0.05)) > 1e-5:
        print('Expected %f, got %f' % (user_data[0] + 0.05, pct))
        user_data[1] = False
    user_data[0] = pct
    return 1 # 1 to continue, 0 to stop

def rasterio_8_progress_interrupt_callback(pct, message, user_data):
    user_data[0] = pct
    if pct >= 0.5:
        return 0
    return 1 # 1 to continue, 0 to stop

def rasterio_8_progress_callback_2(pct, message, user_data):
    if pct < user_data[0]:
        print('Got %f, last pct was %f' % (pct, user_data[0]))
        return 0
    user_data[0] = pct
    return 1 # 1 to continue, 0 to stop

def rasterio_8():

    ds = gdal.Open('data/byte.tif')

    # Progress not implemented yet
    if gdal.GetConfigOption('GTIFF_DIRECT_IO') == 'YES' or \
       gdal.GetConfigOption('GTIFF_VIRTUAL_MEM_IO') == 'YES':
        return 'skip'
    
    # Test RasterBand.ReadRaster
    tab = [ 0, True ]
    data = ds.GetRasterBand(1).ReadRaster(resample_alg = gdal.GRIORA_NearestNeighbour,
                                          callback = rasterio_8_progress_callback,
                                          callback_data = tab)
    l = len(data)
    if l != 400:
        gdaltest.post_reason('did not read expected band data via ReadRaster()')
        return 'fail'
    if abs(tab[0] - 1) > 1e-5 or not tab[1]:
        gdaltest.post_reason('failure')
        return 'fail'

    # Test interruption
    tab = [ 0 ]
    data = ds.GetRasterBand(1).ReadRaster(resample_alg = gdal.GRIORA_NearestNeighbour,
                                          callback = rasterio_8_progress_interrupt_callback,
                                          callback_data = tab)
    if data is not None:
        gdaltest.post_reason('failure')
        return 'fail'
    if tab[0] < 0.50:
        gdaltest.post_reason('failure')
        return 'fail'
    
    # Test RasterBand.ReadRaster with type change
    tab = [ 0, True ]
    data = ds.GetRasterBand(1).ReadRaster(buf_type = gdal.GDT_Int16,
                                          callback = rasterio_8_progress_callback,
                                          callback_data = tab)
    if data is None:
        gdaltest.post_reason('did not read expected band data via ReadRaster()')
        return 'fail'
    if abs(tab[0] - 1) > 1e-5 or not tab[1]:
        gdaltest.post_reason('failure')
        return 'fail'

    # Same with interruption
    tab = [ 0 ]
    data = ds.GetRasterBand(1).ReadRaster(buf_type = gdal.GDT_Int16,
                                          callback = rasterio_8_progress_interrupt_callback,
                                          callback_data = tab)
    if data is not None or tab[0] < 0.50:
        gdaltest.post_reason('failure')
        return 'fail'

    # Test RasterBand.ReadRaster with resampling
    tab = [ 0, True ]
    data = ds.GetRasterBand(1).ReadRaster(buf_xsize = 40,
                                          callback = rasterio_8_progress_callback,
                                          callback_data = tab)
    if data is None:
        gdaltest.post_reason('did not read expected band data via ReadRaster()')
        return 'fail'
    if abs(tab[0] - 1) > 1e-5 or not tab[1]:
        gdaltest.post_reason('failure')
        return 'fail'

    # Same with interruption
    tab = [ 0 ]
    data = ds.GetRasterBand(1).ReadRaster(buf_xsize = 40,
                                          callback = rasterio_8_progress_interrupt_callback,
                                          callback_data = tab)
    if data is not None or tab[0] < 0.50:
        gdaltest.post_reason('failure')
        return 'fail'

    # Test Dataset.ReadRaster
    tab = [ 0, True ]
    data = ds.ReadRaster(resample_alg = gdal.GRIORA_NearestNeighbour,
                         callback = rasterio_8_progress_callback,
                         callback_data = tab)
    l = len(data)
    if l != 400:
        gdaltest.post_reason('did not read expected dataset data via ReadRaster()')
        return 'fail'
    if abs(tab[0] - 1) > 1e-5 or not tab[1]:
        gdaltest.post_reason('failure')
        return 'fail'

    ds = None

    # Test Dataset.ReadRaster on a multi band file, with INTERLEAVE=BAND
    ds = gdal.Open('data/rgbsmall.tif')
    last_pct = [ 0 ]
    data = ds.ReadRaster(resample_alg = gdal.GRIORA_NearestNeighbour,
                  callback = rasterio_8_progress_callback_2,
                  callback_data = last_pct)
    if data is None or abs(last_pct[0] - 1.0) > 1e-5:
        gdaltest.post_reason('failure')
        return 'fail'

    # Same with interruption
    tab = [ 0 ]
    data = ds.ReadRaster(callback = rasterio_8_progress_interrupt_callback,
                         callback_data = tab)
    if data is not None or tab[0] < 0.50:
        gdaltest.post_reason('failure')
        return 'fail'

    ds = None

    # Test Dataset.ReadRaster on a multi band file, with INTERLEAVE=PIXEL
    ds = gdal.Open('data/rgbsmall_cmyk.tif')
    last_pct = [ 0 ]
    data = ds.ReadRaster(resample_alg = gdal.GRIORA_NearestNeighbour,
                  callback = rasterio_8_progress_callback_2,
                  callback_data = last_pct)
    if data is None or abs(last_pct[0] - 1.0) > 1e-5:
        gdaltest.post_reason('failure')
        return 'fail'

    # Same with interruption
    tab = [ 0 ]
    data = ds.ReadRaster(callback = rasterio_8_progress_interrupt_callback,
                         callback_data = tab)
    if data is not None or tab[0] < 0.50:
        gdaltest.post_reason('failure')
        return 'fail'

    return 'success'

###############################################################################
# Test resampling algorithm of ReadRaster()

def rasterio_9_progress_callback(pct, message, user_data):
    if pct < user_data[0]:
        print('Got %f, last pct was %f' % (pct, user_data[0]))
        return 0
    user_data[0] = pct
    if user_data[1] is not None and pct >= user_data[1]:
        return 0
    return 1 # 1 to continue, 0 to stop

def rasterio_9_checksum(data, buf_xsize, buf_ysize, data_type = gdal.GDT_Byte):
    ds = gdal.GetDriverByName('MEM').Create('', buf_xsize, buf_ysize, 1)
    ds.GetRasterBand(1).WriteRaster(0,0,buf_xsize,buf_ysize,data, buf_type = data_type)
    cs = ds.GetRasterBand(1).Checksum()
    return cs

def rasterio_9():
    ds = gdal.Open('data/byte.tif')

    # Test RasterBand.ReadRaster, with Bilinear
    tab = [ 0, None ]
    data = ds.GetRasterBand(1).ReadRaster(buf_type = gdal.GDT_Int16,
                                          buf_xsize = 10,
                                          buf_ysize = 10,
                                          resample_alg = gdal.GRIORA_Bilinear,
                                          callback = rasterio_9_progress_callback,
                                          callback_data = tab)
    if data is None:
        gdaltest.post_reason('failure')
        return 'fail'
    cs = rasterio_9_checksum(data, 10, 10, data_type = gdal.GDT_Int16)
    if cs != 1211: # checksum of gdal_translate data/byte.tif out.tif -outsize 10 10 -r BILINEAR
        gdaltest.post_reason('failure')
        print(cs)
        return 'fail'

    if abs(tab[0] - 1.0) > 1e-5:
        gdaltest.post_reason('failure')
        return 'fail'

    # Test RasterBand.ReadRaster, with Bilinear and UInt16 data type
    src_ds_uint16 = gdal.Open('data/uint16.tif')
    tab = [ 0, None ]
    data = src_ds_uint16.GetRasterBand(1).ReadRaster(buf_type = gdal.GDT_UInt16,
                                          buf_xsize = 10,
                                          buf_ysize = 10,
                                          resample_alg = gdal.GRIORA_Bilinear,
                                          callback = rasterio_9_progress_callback,
                                          callback_data = tab)
    if data is None:
        gdaltest.post_reason('failure')
        return 'fail'
    cs = rasterio_9_checksum(data, 10, 10, data_type = gdal.GDT_UInt16)
    if cs != 1211: # checksum of gdal_translate data/byte.tif out.tif -outsize 10 10 -r BILINEAR
        gdaltest.post_reason('failure')
        print(cs)
        return 'fail'

    if abs(tab[0] - 1.0) > 1e-5:
        gdaltest.post_reason('failure')
        return 'fail'


    # Test RasterBand.ReadRaster, with Bilinear on Complex, thus using warp API
    tab = [ 0, None ]
    complex_ds = gdal.GetDriverByName('MEM').Create('', 20, 20, 1, gdal.GDT_CInt16)
    complex_ds.GetRasterBand(1).WriteRaster(0,0,20,20, ds.GetRasterBand(1).ReadRaster(), buf_type = gdal.GDT_Byte)
    data = complex_ds.GetRasterBand(1).ReadRaster(buf_xsize = 10,
                                                  buf_ysize = 10,
                                                  resample_alg = gdal.GRIORA_Bilinear,
                                                  callback = rasterio_9_progress_callback,
                                                  callback_data = tab)
    if data is None:
        gdaltest.post_reason('failure')
        return 'fail'
    cs = rasterio_9_checksum(data, 10, 10, data_type = gdal.GDT_CInt16)
    if cs != 1211: # checksum of gdal_translate data/byte.tif out.tif -outsize 10 10 -r BILINEAR
        gdaltest.post_reason('failure')
        print(cs)
        return 'fail'

    if abs(tab[0] - 1.0) > 1e-5:
        gdaltest.post_reason('failure')
        return 'fail'



    # Test interruption
    tab = [ 0, 0.5 ]
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    data = ds.GetRasterBand(1).ReadRaster(buf_xsize = 10,
                                          buf_ysize = 10,
                                          resample_alg = gdal.GRIORA_Bilinear,
                                          callback = rasterio_9_progress_callback,
                                          callback_data = tab)
    gdal.PopErrorHandler()
    if data is not None:
        gdaltest.post_reason('failure')
        return 'fail'
    if tab[0] < 0.50:
        gdaltest.post_reason('failure')
        return 'fail'

    # Test RasterBand.ReadRaster, with Gauss, and downsampling
    tab = [ 0, None ]
    data = ds.GetRasterBand(1).ReadRaster(buf_xsize = 10,
                                          buf_ysize = 10,
                                          resample_alg = gdal.GRIORA_Gauss,
                                          callback = rasterio_9_progress_callback,
                                          callback_data = tab)
    if data is None:
        gdaltest.post_reason('failure')
        return 'fail'
    cs = rasterio_9_checksum(data, 10, 10)
    if cs != 1089: # checksum of gdal_translate data/byte.tif out.tif -outsize 10 10 -r GAUSS
        gdaltest.post_reason('failure')
        print(cs)
        return 'fail'

    if abs(tab[0] - 1.0) > 1e-5:
        gdaltest.post_reason('failure')
        return 'fail'

    # Test RasterBand.ReadRaster, with Cubic, and downsampling
    tab = [ 0, None ]
    data = ds.GetRasterBand(1).ReadRaster(buf_xsize = 10,
                                          buf_ysize = 10,
                                          resample_alg = gdal.GRIORA_Cubic,
                                          callback = rasterio_9_progress_callback,
                                          callback_data = tab)
    if data is None:
        gdaltest.post_reason('failure')
        return 'fail'
    cs = rasterio_9_checksum(data, 10, 10)
    if cs != 1059: # checksum of gdal_translate data/byte.tif out.tif -outsize 10 10 -r CUBIC
        gdaltest.post_reason('failure')
        print(cs)
        return 'fail'

    if abs(tab[0] - 1.0) > 1e-5:
        gdaltest.post_reason('failure')
        return 'fail'

    # Test RasterBand.ReadRaster, with Cubic and supersampling
    tab = [ 0, None ]
    data = ds.GetRasterBand(1).ReadRaster(buf_xsize = 40,
                                          buf_ysize = 40,
                                          resample_alg = gdal.GRIORA_Cubic,
                                          callback = rasterio_9_progress_callback,
                                          callback_data = tab)
    if data is None:
        gdaltest.post_reason('failure')
        return 'fail'
    cs = rasterio_9_checksum(data, 40, 40)
    if cs != 19556: # checksum of gdal_translate data/byte.tif out.tif -outsize 40 40 -r CUBIC
        gdaltest.post_reason('failure')
        print(cs)
        return 'fail'

    if abs(tab[0] - 1.0) > 1e-5:
        gdaltest.post_reason('failure')
        return 'fail'

    # Test Dataset.ReadRaster, with Cubic and supersampling
    tab = [ 0, None ]
    data = ds.ReadRaster(buf_xsize = 40,
                         buf_ysize = 40,
                         resample_alg = gdal.GRIORA_CubicSpline,
                         callback = rasterio_9_progress_callback,
                         callback_data = tab)
    if data is None:
        gdaltest.post_reason('failure')
        return 'fail'
    cs = rasterio_9_checksum(data, 40, 40)
    if cs != 19041: # checksum of gdal_translate data/byte.tif out.tif -outsize 40 40 -r CUBICSPLINE
        gdaltest.post_reason('failure')
        print(cs)
        return 'fail'

    if abs(tab[0] - 1.0) > 1e-5:
        gdaltest.post_reason('failure')
        return 'fail'

    # Test Dataset.ReadRaster on a multi band file, with INTERLEAVE=PIXEL
    ds = gdal.Open('data/rgbsmall_cmyk.tif')
    tab = [ 0, None ]
    data = ds.ReadRaster(buf_xsize = 25,
                         buf_ysize = 25,
                         resample_alg = gdal.GRIORA_Cubic,
                         callback = rasterio_9_progress_callback,
                         callback_data = tab)
    if data is None:
        gdaltest.post_reason('failure')
        return 'fail'
    cs = rasterio_9_checksum(data[0:25*25], 25, 25)
    if cs != 5975: # checksum of gdal_translate data/rgbsmall_cmyk.tif out.tif -outsize 25 25 -r CUBIC
        gdaltest.post_reason('failure')
        print(cs)
        return 'fail'
    cs = rasterio_9_checksum(data[25*25:2*25*25], 25, 25)
    if cs != 6248: # checksum of gdal_translate data/rgbsmall_cmyk.tif out.tif -outsize 25 25 -r CUBIC
        gdaltest.post_reason('failure')
        print(cs)
        return 'fail'

    if abs(tab[0] - 1.0) > 1e-5:
        gdaltest.post_reason('failure')
        return 'fail'
    ds = None

    # Test Band.ReadRaster on a RGBA with parts fully opaque, and fully transparent and with huge upscaling
    ds = gdal.Open('data/stefan_full_rgba.png')
    tab = [ 0, None ]
    data = ds.GetRasterBand(1).ReadRaster(buf_xsize = 162 * 16,
                         buf_ysize = 150 * 16,
                         resample_alg = gdal.GRIORA_Cubic,
                         callback = rasterio_9_progress_callback,
                         callback_data = tab)
    if data is None:
        gdaltest.post_reason('failure')
        return 'fail'
    cs = rasterio_9_checksum(data, 162 * 16, 150 * 16)
    if cs != 30836: # checksum of gdal_translate data/stefan_full_rgba.png out.tif -outsize 1600% 1600% -r CUBIC
        gdaltest.post_reason('failure')
        print(cs)
        return 'fail'
    if abs(tab[0] - 1.0) > 1e-5:
        gdaltest.post_reason('failure')
        return 'fail'

    return 'success'

###############################################################################
# Test error when getting a block

def rasterio_10():
    ds = gdal.Open('data/byte_truncated.tif')

    gdal.PushErrorHandler()
    data = ds.GetRasterBand(1).ReadRaster()
    gdal.PopErrorHandler()
    if data is not None:
        gdaltest.post_reason('failure')
        return 'fail'

    # Change buffer type
    gdal.PushErrorHandler()
    data = ds.GetRasterBand(1).ReadRaster(buf_type = gdal.GDT_Int16)
    gdal.PopErrorHandler()
    if data is not None:
        gdaltest.post_reason('failure')
        return 'fail'

    # Resampling case
    gdal.PushErrorHandler()
    data = ds.GetRasterBand(1).ReadRaster(buf_xsize = 10,
                                          buf_ysize = 10)
    gdal.PopErrorHandler()
    if data is not None:
        gdaltest.post_reason('failure')
        return 'fail'

    return 'success'

gdaltest_list = [
    rasterio_1,
    rasterio_2,
    rasterio_3,
    rasterio_4,
    rasterio_5,
    rasterio_6,
    rasterio_7,
    rasterio_8,
    rasterio_9,
    rasterio_10,
    ]

if __name__ == '__main__':

    gdaltest.setup_run( 'rasterio' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

