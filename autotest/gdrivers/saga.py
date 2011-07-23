#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test SAGA GIS Binary driver
# Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
# 
###############################################################################
# Copyright (c) 2009, Even Rouault, <even dot rouault at mines dash paris dot org>
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
import gdal

sys.path.append( '../pymod' )

import gdaltest

###############################################################################
# Test opening

def saga_1():

    tst = gdaltest.GDALTest( 'SAGA', '4byteFloat.sdat', 1, 108 )
    return tst.testOpen()

###############################################################################
# Test copying a reference sample with CreateCopy()

def saga_2():

    tst = gdaltest.GDALTest( 'SAGA', '4byteFloat.sdat', 1, 108 )
    return tst.testCreateCopy( new_filename = 'tmp/createcopy.sdat' )

###############################################################################
# Test copying a reference sample with Create()

def saga_3():

    tst = gdaltest.GDALTest( 'SAGA', '4byteFloat.sdat', 1, 108 )
    return tst.testCreate( new_filename = 'tmp/copy.sdat', out_bands = 1 )

###############################################################################
# Test CreateCopy() for various data types

def saga_4():

    src_files = [ 'byte.tif',
                  'int16.tif',
                  '../../gcore/data/uint16.tif',
                  '../../gcore/data/int32.tif',
                  '../../gcore/data/uint32.tif',
                  '../../gcore/data/float32.tif',
                  '../../gcore/data/float64.tif' ]

    for src_file in src_files:
        tst = gdaltest.GDALTest( 'SAGA', src_file, 1, 4672 )
        if src_file == 'byte.tif':
            check_minmax = 0
        else:
            check_minmax = 1
        ret = tst.testCreateCopy( new_filename = 'tmp/test4.sdat', check_minmax = check_minmax )
        if ret != 'success':
            return ret
            
    return 'success'

###############################################################################
# Test Create() for various data types

def saga_5():

    src_files = [ 'byte.tif',
                  'int16.tif',
                  '../../gcore/data/uint16.tif',
                  '../../gcore/data/int32.tif',
                  '../../gcore/data/uint32.tif',
                  '../../gcore/data/float32.tif',
                  '../../gcore/data/float64.tif' ]

    for src_file in src_files:
        tst = gdaltest.GDALTest( 'SAGA', src_file, 1, 4672 )
        if src_file == 'byte.tif':
            check_minmax = 0
        else:
            check_minmax = 1
        ret = tst.testCreate( new_filename = 'tmp/test5.sdat', out_bands = 1, check_minmax = check_minmax )
        if ret != 'success':
            return ret
            
    return 'success'
        
###############################################################################
# Test creating empty datasets and check that nodata values are properly written

def saga_6():

    gdal_types = [gdal.GDT_Byte,
                  gdal.GDT_Int16,
                  gdal.GDT_UInt16,
                  gdal.GDT_Int32,
                  gdal.GDT_UInt32,
                  gdal.GDT_Float32,
                  gdal.GDT_Float64 ]
              
    expected_nodata = [ 255, -32767, 65535, -2147483647, 4294967295, -99999.0, -99999.0 ]
                  
    for i in range(len(gdal_types)):
    
        ds = gdal.GetDriverByName('SAGA').Create('tmp/test6.sdat', 2, 2, 1, gdal_types[i])
        ds = None
        
        ds = gdal.Open('tmp/test6.sdat')
        
        data = ds.GetRasterBand(1).ReadRaster(1, 1, 1, 1, buf_type = gdal.GDT_Float64)

        # Read raw data into tuple of float numbers
        import struct
        value = struct.unpack('d' * 1, data)[0]
        if value != expected_nodata[i]:
            print (value)
            gdaltest.post_reason('did not get expected pixel value')
            return 'fail'
            
        nodata = ds.GetRasterBand(1).GetNoDataValue()
        if nodata != expected_nodata[i]:
            print (nodata)
            gdaltest.post_reason('did not get expected nodata value')
            return 'fail'
        
        ds = None

    try:
        os.remove('tmp/test6.sgrd')
        os.remove('tmp/test6.sdat')
    except:
        pass
        
    return 'success'
    
###############################################################################
# Test /vsimem

def saga_7():

    tst = gdaltest.GDALTest( 'SAGA', '4byteFloat.sdat', 1, 108 )
    return tst.testCreateCopy( new_filename = '/vsimem/createcopy.sdat' )
    
gdaltest_list = [
    saga_1,
    saga_2,
    saga_3,
    saga_4,
    saga_5,
    saga_6,
    saga_7 ]

if __name__ == '__main__':

    gdaltest.setup_run( 'saga' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

