#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test ComputeProximity() algorithm.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
# 
###############################################################################
# Copyright (c) 2008, Frank Warmerdam <warmerdam@pobox.com>
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

def proximity_1():

    drv = gdal.GetDriverByName( 'GTiff' )
    src_ds = gdal.Open('data/pat.tif')
    src_band = src_ds.GetRasterBand(1)
    
    dst_ds = drv.Create('tmp/proximity_1.tif', 25, 25, 1, gdal.GDT_Byte )
    dst_band = dst_ds.GetRasterBand(1)
    
    gdal.ComputeProximity( src_band, dst_band )

    cs_expected = 1941
    cs = dst_band.Checksum()
    
    dst_band = None
    dst_ds = None

    if cs == cs_expected \
       or gdal.GetConfigOption( 'CPL_DEBUG', 'OFF' ) != 'ON':
        drv.Delete( 'tmp/proximity_1.tif' )
    
    if cs != cs_expected:
        print('Got: ', cs)
        gdaltest.post_reason( 'got wrong checksum' )
        return 'fail'
    else:
        return 'success' 

###############################################################################
# Try several options

def proximity_2():

    drv = gdal.GetDriverByName( 'GTiff' )
    src_ds = gdal.Open('data/pat.tif')
    src_band = src_ds.GetRasterBand(1)
    
    dst_ds = drv.Create('tmp/proximity_2.tif', 25, 25, 1, gdal.GDT_Float32 )
    dst_band = dst_ds.GetRasterBand(1)
    
    gdal.ComputeProximity( src_band, dst_band,
                           options = [ 'VALUES=65,64',
                                       'MAXDIST=12',
                                       'NODATA=-1',
                                       'FIXED_BUF_VAL=255' ] )

    cs_expected = 3256
    cs = dst_band.Checksum()
    
    dst_band = None
    dst_ds = None

    if cs == cs_expected \
       or gdal.GetConfigOption( 'CPL_DEBUG', 'OFF' ) != 'ON':
        drv.Delete( 'tmp/proximity_2.tif' )
    
    if cs != cs_expected:
        print('Got: ', cs)
        gdaltest.post_reason( 'got wrong checksum' )
        return 'fail'
    else:
        return 'success'

###############################################################################
# Try input nodata option

def proximity_3():

    drv = gdal.GetDriverByName( 'GTiff' )
    src_ds = gdal.Open('data/pat.tif')
    src_band = src_ds.GetRasterBand(1)
    
    dst_ds = drv.Create('tmp/proximity_3.tif', 25, 25, 1, gdal.GDT_Byte )
    dst_band = dst_ds.GetRasterBand(1)
    
    gdal.ComputeProximity( src_band, dst_band,
                          options = [ 'VALUES=65,64',
                                      'MAXDIST=12',
                                      'USE_INPUT_NODATA=YES',
                                      'NODATA=0' ] )

    cs_expected = 1465
    cs = dst_band.Checksum()
    
    dst_band = None
    dst_ds = None

    if cs == cs_expected \
       or gdal.GetConfigOption( 'CPL_DEBUG', 'OFF' ) != 'ON':
        drv.Delete( 'tmp/proximity_3.tif' )
    
    if cs != cs_expected:
        print('Got: ', cs)
        gdaltest.post_reason( 'got wrong checksum' )
        return 'fail'
    else:
        return 'success'

gdaltest_list = [
    proximity_1,
    proximity_2,
    proximity_3
    ]

if __name__ == '__main__':

    gdaltest.setup_run( 'proximity' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

