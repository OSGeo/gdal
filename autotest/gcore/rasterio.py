#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test default implementation of GDALRasterBand::IRasterIO
# Author:   Even Rouault <even dot rouault at mines dash paris dot org>
# 
###############################################################################
# Copyright (c) 2008, Even Rouault <even dot rouault at mines dash paris dot org>
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

import os
import sys

sys.path.append( '../pymod' )

import gdaltest
import gdal
import gdalconst

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

gdaltest_list = [
    rasterio_1,
    rasterio_2,
    rasterio_3,
    rasterio_4 ]

if __name__ == '__main__':

    gdaltest.setup_run( 'rasterio' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

