#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test SieveFilter() algorithm.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
# 
###############################################################################
# Copyright (c) 2008, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2009-2010, Even Rouault <even dot rouault at mines-paris dot org>
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
# Test a fairly default case.

def sieve_1():

    drv = gdal.GetDriverByName( 'GTiff' )
    src_ds = gdal.Open('data/sieve_src.grd')
    src_band = src_ds.GetRasterBand(1)
    
    dst_ds = drv.Create('tmp/sieve_1.tif', 5, 7, 1, gdal.GDT_Byte )
    dst_band = dst_ds.GetRasterBand(1)
    
    gdal.SieveFilter( src_band, None, dst_band, 2, 4 )

    cs_expected = 364
    cs = dst_band.Checksum()
    
    dst_band = None
    dst_ds = None

    if cs == cs_expected \
       or gdal.GetConfigOption( 'CPL_DEBUG', 'OFF' ) != 'ON':
        drv.Delete( 'tmp/sieve_1.tif' )
    
    if cs != cs_expected:
        print('Got: ', cs)
        gdaltest.post_reason( 'got wrong checksum' )
        return 'fail'
    else:
        return 'success' 

###############################################################################
# Try eight connected.

def sieve_2():

    drv = gdal.GetDriverByName( 'GTiff' )
    src_ds = gdal.Open('data/sieve_src.grd')
    src_band = src_ds.GetRasterBand(1)
    
    dst_ds = drv.Create('tmp/sieve_2.tif', 5, 7, 1, gdal.GDT_Byte )
    dst_band = dst_ds.GetRasterBand(1)
    
    gdal.SieveFilter( src_band, None, dst_band, 2, 8 )

    cs_expected = 370
    cs = dst_band.Checksum()
    
    dst_band = None
    dst_ds = None

    if cs == cs_expected \
       or gdal.GetConfigOption( 'CPL_DEBUG', 'OFF' ) != 'ON':
        drv.Delete( 'tmp/sieve_2.tif' )
    
    if cs != cs_expected:
        print('Got: ', cs)
        gdaltest.post_reason( 'got wrong checksum' )
        return 'fail'
    else:
        return 'success' 

###############################################################################
# Do a sieve resulting in unmergable polygons.

def sieve_3():

    drv = gdal.GetDriverByName( 'GTiff' )
    src_ds = gdal.Open('data/unmergable.grd')
    src_band = src_ds.GetRasterBand(1)
    
    dst_ds = drv.Create('tmp/sieve_3.tif', 5, 7, 1, gdal.GDT_Byte )
    dst_band = dst_ds.GetRasterBand(1)
    
    gdal.SieveFilter( src_band, None, dst_band, 2, 8 )

    cs_expected = 472
    cs = dst_band.Checksum()
    
    dst_band = None
    dst_ds = None

    if cs == cs_expected \
       or gdal.GetConfigOption( 'CPL_DEBUG', 'OFF' ) != 'ON':
        drv.Delete( 'tmp/sieve_3.tif' )
    
    if cs != cs_expected:
        print('Got: ', cs)
        gdaltest.post_reason( 'got wrong checksum' )
        return 'fail'
    else:
        return 'success' 

###############################################################################
# Try the bug 2634 simplified data.

def sieve_4():

    drv = gdal.GetDriverByName( 'GTiff' )
    src_ds = gdal.Open('data/sieve_2634.grd')
    src_band = src_ds.GetRasterBand(1)
    
    dst_ds = drv.Create('tmp/sieve_4.tif', 10, 8, 1, gdal.GDT_Byte )
    dst_band = dst_ds.GetRasterBand(1)
    
    gdal.SieveFilter( src_band, None, dst_band, 2, 4 )

    cs_expected = 98
    cs = dst_band.Checksum()
    
    dst_band = None
    dst_ds = None

    if cs == cs_expected \
       or gdal.GetConfigOption( 'CPL_DEBUG', 'OFF' ) != 'ON':
        drv.Delete( 'tmp/sieve_4.tif' )
    
    if cs != cs_expected:
        print('Got: ', cs)
        gdaltest.post_reason( 'got wrong checksum' )
        return 'fail'
    else:
        return 'success' 


###############################################################################
# Same as sieve_1, but we provide a mask band
# This should yield the same result as we use an opaque band

def sieve_5():

    drv = gdal.GetDriverByName( 'GTiff' )
    src_ds = gdal.Open('data/sieve_src.grd')
    src_band = src_ds.GetRasterBand(1)
    
    dst_ds = drv.Create('tmp/sieve_1.tif', 5, 7, 1, gdal.GDT_Byte )
    dst_band = dst_ds.GetRasterBand(1)
    
    gdal.SieveFilter( src_band, dst_band.GetMaskBand(), dst_band, 2, 4 )

    cs_expected = 364
    cs = dst_band.Checksum()
    
    dst_band = None
    dst_ds = None

    if cs == cs_expected \
       or gdal.GetConfigOption( 'CPL_DEBUG', 'OFF' ) != 'ON':
        drv.Delete( 'tmp/sieve_1.tif' )
    
    if cs != cs_expected:
        print('Got: ', cs)
        gdaltest.post_reason( 'got wrong checksum' )
        return 'fail'
    else:
        return 'success' 

###############################################################################
# Peformance test. When increasing the 'size' parameter, performance
# should stay roughly linear with the number of pixels (ie size^2)

def sieve_6():

    try:
        import numpy
    except:
        return 'skip'

    # Try 3002. Should run in less than 10 seconds
    #size = 3002
    size = 102
    ds = gdal.GetDriverByName('MEM').Create('', size+1, size)
    ar = numpy.zeros((size,size+1), dtype = numpy.uint8)
    for i in range(size):
        for j in range(int(size/3)):
            ar[i][size+1-1-i-1-3*j] = 255
            ar[i][size+1-1-i-3*j] = 255
        ar[i][0] = 255
    ar[size-1] = 255
    ds.GetRasterBand(1).WriteArray(ar)
    
    band = ds.GetRasterBand(1)

    gdal.SieveFilter( band, None, band, 2, 4 )
    
    #ar = band.ReadAsArray()
    #print(ar)
    
    cs = band.Checksum()
    if (size == 102 and cs != 60955) or (size == 3002 and cs != 63178):
        print('Got: ', cs)
        gdaltest.post_reason( 'got wrong checksum' )
        return 'fail'
    
    return 'success'

###############################################################################
# Test with nodata

def sieve_7():

    gdal.FileFromMemBuffer('/vsimem/sieve_7.asc', """ncols        7
nrows        7
xllcorner    440720.000000000000
yllcorner    3750120.000000000000
cellsize     60.000000000000
NODATA_value 0
 0 0 0 0 0 0 0
 0 1 1 1 1 1 1
 0 1 0 0 1 1 1
 0 1 0 2 2 2 1
 0 1 1 2 1 2 1
 0 1 1 2 2 2 1
 0 1 1 1 1 1 1
 """)


    drv = gdal.GetDriverByName( 'GTiff' )
    src_ds = gdal.Open('/vsimem/sieve_7.asc')
    src_band = src_ds.GetRasterBand(1)
    
    dst_ds = drv.Create('/vsimem/sieve_7.tif', 7, 7, 1, gdal.GDT_Byte )
    dst_band = dst_ds.GetRasterBand(1)
    
    gdal.SieveFilter( src_band, src_band.GetMaskBand(), dst_band, 4, 4 )

    cs_expected = 42
    cs = dst_band.Checksum()

    dst_band = None
    dst_ds = None
    
    gdal.Unlink('/vsimem/sieve_7.asc')

    if cs == cs_expected \
       or gdal.GetConfigOption( 'CPL_DEBUG', 'OFF' ) != 'ON':
        drv.Delete( '/vsimem/sieve_7.tif' )
    
    # Expeced:
    #[[0 0 0 0 0 0 0]
    # [0 1 1 1 1 1 1]
    # [0 1 0 0 1 1 1]
    # [0 1 0 2 2 2 1]
    # [0 1 1 2 2 2 1]
    # [0 1 1 2 2 2 1]
    # [0 1 1 1 1 1 1]]

    if cs != cs_expected:
        print('Got: ', cs)
        gdaltest.post_reason( 'got wrong checksum' )
        return 'fail'
    else:
        return 'success' 


gdaltest_list = [
    sieve_1,
    sieve_2,
    sieve_3,
    sieve_4,
    sieve_5,
    sieve_6,
    sieve_7
    ]

if __name__ == '__main__':

    gdaltest.setup_run( 'sieve' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

