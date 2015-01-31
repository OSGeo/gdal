#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read/write functionality for XYZ driver.
# Author:   Even Rouault <even dot rouault at mines dash paris dot org>
# 
###############################################################################
# Copyright (c) 2010-2013, Even Rouault <even dot rouault at mines-paris dot org>
# 
# Permission is hereby granted, free of charge, to any person oxyzaining a
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
import struct
from osgeo import gdal

sys.path.append( '../pymod' )

import gdaltest

###############################################################################
# Test CreateCopy() of byte.tif

def xyz_1():

    tst = gdaltest.GDALTest( 'XYZ', 'byte.tif', 1, 4672 )
    return tst.testCreateCopy( vsimem = 1, check_gt = ( -67.00041667, 0.00083333, 0.0, 50.000416667, 0.0, -0.00083333 ) )

###############################################################################
# Test CreateCopy() of float.img

def xyz_2():

    src_ds = gdal.Open('data/float.img')
    ds = gdal.GetDriverByName('XYZ').CreateCopy('tmp/float.xyz', src_ds, options = ['COLUMN_SEPARATOR=,', 'ADD_HEADER_LINE=YES'] )
    got_cs = ds.GetRasterBand(1).Checksum()
    expected_cs = src_ds.GetRasterBand(1).Checksum()
    ds = None
    gdal.GetDriverByName('XYZ').Delete('tmp/float.xyz')
    if got_cs != expected_cs and got_cs != 24387:
        print(got_cs)
        return 'fail'
    return 'success'

###############################################################################
# Test random access to lines of imagery

def xyz_3():

    content = """Y X Z
0 0 65


0 1 66

1 0 67

1 1 68
2 0 69
2 1 70


"""
    gdal.FileFromMemBuffer('/vsimem/grid.xyz', content)
    ds = gdal.Open('/vsimem/grid.xyz')
    buf = ds.ReadRaster(0,2,2,1)
    bytes = struct.unpack('B' * 2, buf)
    if bytes != (69, 70):
        print(buf)
        return 'fail'
    buf = ds.ReadRaster(0,1,2,1)
    bytes = struct.unpack('B' * 2, buf)
    if bytes != (67, 68):
        print(buf)
        return 'fail'
    buf = ds.ReadRaster(0,0,2,1)
    bytes = struct.unpack('B' * 2, buf)
    if bytes != (65, 66):
        print(buf)
        return 'fail'
    buf = ds.ReadRaster(0,2,2,1)
    bytes = struct.unpack('B' * 2, buf)
    if bytes != (69, 70):
        print(buf)
        return 'fail'
    ds = None
    gdal.Unlink('/vsimem/grid.xyz')
    return 'success'


###############################################################################
# Test regularly spaced XYZ but with missing values at beginning and/or end of lines

def xyz_4_checkline(ds, i, expected_bytes):
    buf = ds.ReadRaster(0,i,ds.RasterXSize,1)
    bytes = struct.unpack('B' * ds.RasterXSize, buf)
    if bytes != expected_bytes:
        return False
    return True

def xyz_4():

    content = """
440750 3751290 1
440810 3751290 2

440690 3751230 3
440750 3751230 4
440810 3751230 5
440870 3751230 6

440810 3751170 7"""
    gdal.FileFromMemBuffer('/vsimem/grid.xyz', content)
    expected = [ ( 0, 1, 2, 0 ), ( 3, 4, 5, 6 ), (0, 0, 7, 0) ]

    ds = gdal.Open('/vsimem/grid.xyz')
    if ds.GetRasterBand(1).GetMinimum() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetRasterBand(1).GetMaximum() != 7:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetRasterBand(1).GetNoDataValue() != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    for i in [0,1,2,1,0,2,0,2,0,1,2]:
        if not xyz_4_checkline(ds, i, expected[i]):
            gdaltest.post_reason('fail')
            return 'fail'
    ds = None
    gdal.Unlink('/vsimem/grid.xyz')
    return 'success'


###############################################################################
# Test XYZ with only integral values and comma field separator

def xyz_5():

    content = """0,1,100
0.5,1,100
1,1,100
0,2,100
0.5,2,100
1,2,100
"""
    gdal.FileFromMemBuffer('/vsimem/grid.xyz', content)

    ds = gdal.Open('/vsimem/grid.xyz')
    if ds.RasterXSize != 3 or ds.RasterYSize != 2:
        gdaltest.post_reason('fail')
        return 'fail'
    got_gt = ds.GetGeoTransform()
    expected_gt = (-0.25, 0.5, 0.0, 0.5, 0.0, 1.0)
    ds = None
    gdal.Unlink('/vsimem/grid.xyz')
    
    for i in range(6):
        if abs(got_gt[i]-expected_gt[i]) > 1e-5:
            gdaltest.post_reason('fail')
            print(got_gt)
            print(expected_gt)
            return 'fail'
    
    return 'success'


###############################################################################
# Test XYZ with comma decimal separator and semi-colon field separator

def xyz_6():

    content = """0;1;100
0,5;1;100
1;1;100
0;2;100
0,5;2;100
1;2;100
"""
    gdal.FileFromMemBuffer('/vsimem/grid.xyz', content)

    ds = gdal.Open('/vsimem/grid.xyz')
    if ds.RasterXSize != 3 or ds.RasterYSize != 2:
        gdaltest.post_reason('fail')
        return 'fail'
    got_gt = ds.GetGeoTransform()
    expected_gt = (-0.25, 0.5, 0.0, 0.5, 0.0, 1.0)
    ds = None
    gdal.Unlink('/vsimem/grid.xyz')
    
    for i in range(6):
        if abs(got_gt[i]-expected_gt[i]) > 1e-5:
            gdaltest.post_reason('fail')
            print(got_gt)
            print(expected_gt)
            return 'fail'
    
    return 'success'

###############################################################################
# Cleanup

def xyz_cleanup():

    return 'success'


gdaltest_list = [
    xyz_1,
    xyz_2,
    xyz_3,
    xyz_4,
    xyz_5,
    xyz_6,
    xyz_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'xyz' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

