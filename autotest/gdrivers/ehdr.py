#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test EHdr format driver.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
# 
###############################################################################
# Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
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
import array

sys.path.append( '../pymod' )

import gdaltest

###############################################################################
# 16bit image.

def ehdr_1():

    tst = gdaltest.GDALTest( 'EHDR', 'rgba16.png', 2, 2042 )

    return tst.testCreate()

###############################################################################
# 8bit with geotransform and projection check.

def ehdr_2():

    tst = gdaltest.GDALTest( 'EHDR', 'byte.tif', 1, 4672 )

    return tst.testCreateCopy( check_gt = 1, check_srs = 1 )

###############################################################################
# 32bit floating point (read, and createcopy).

def ehdr_3():

    tst = gdaltest.GDALTest( 'EHDR', 'float32.bil', 1, 27 )

    return tst.testCreateCopy()

###############################################################################
# create dataset with a nodata value and a color table.

def ehdr_4():

    drv = gdal.GetDriverByName( 'EHdr' )
    ds = drv.Create( 'tmp/test_4.bil', 200, 100, 1, gdal.GDT_Byte )

    raw_data = array.array('h',list(range(200))).tostring()

    for line in range(100):
        ds.WriteRaster( 0, line, 200, 1, raw_data,
                        buf_type = gdal.GDT_Int16 )

    ct = gdal.ColorTable()
    ct.SetColorEntry( 0, (255,255,255,255) )
    ct.SetColorEntry( 1, (255,255,0,255) )
    ct.SetColorEntry( 2, (255,0,255,255) )
    ct.SetColorEntry( 3, (0,255,255,255) )
    
    ds.GetRasterBand( 1 ).SetRasterColorTable( ct )
    
    ds.GetRasterBand( 1 ).SetNoDataValue( 17 )

    ds = None

    return 'success'

###############################################################################
# verify last dataset's colortable and nodata value.

def ehdr_5():
    ds = gdal.Open( 'tmp/test_4.bil' )
    band = ds.GetRasterBand(1)

    if band.GetNoDataValue() != 17:
        gdaltest.post_reason( 'failed to preserve nodata value.' )
        return 'fail'

    ct = band.GetRasterColorTable()
    if ct is None or ct.GetCount() != 4 \
       or ct.GetColorEntry( 2 ) != (255,0,255,255):
        gdaltest.post_reason( 'color table not persisted properly.' )
        return 'fail'

    band = None
    ct = None
    ds = None

    gdal.GetDriverByName('EHdr').Delete( 'tmp/test_4.bil' )
    
    return 'success'

###############################################################################
# Test creating an in memory copy.

def ehdr_6():

    tst = gdaltest.GDALTest( 'EHDR', 'float32.bil', 1, 27 )

    return tst.testCreateCopy( vsimem = 1 )

###############################################################################
# 32bit integer (read, and createcopy).

def ehdr_7():

    tst = gdaltest.GDALTest( 'EHDR', 'int32.tif', 1, 4672 )

    return tst.testCreateCopy()

###############################################################################
# Test signed 8bit integer support. (#2717)

def ehdr_8():

    drv = gdal.GetDriverByName('EHDR')
    src_ds = gdal.Open('data/8s.vrt')
    ds = drv.CreateCopy( 'tmp/ehdr_8.bil', src_ds )
    src_ds = None

    md = ds.GetRasterBand(1).GetMetadata('IMAGE_STRUCTURE')
    if 'PIXELTYPE' not in md or md['PIXELTYPE'] != 'SIGNEDBYTE':
        gdaltest.post_reason( 'Failed to detect SIGNEDBYTE' )
        return 'fail'

    cs = ds.GetRasterBand(1).Checksum()
    expected = 4672
    if cs != expected:
        print(cs)
        gdaltest.post_reason( 'Did not get expected image checksum.' )
        return 'fail'

    ds = None

    drv.Delete( 'tmp/ehdr_8.bil' )
    
    return 'success'

gdaltest_list = [
    ehdr_1,
    ehdr_2,
    ehdr_3,
    ehdr_4,
    ehdr_5,
    ehdr_6,
    ehdr_7,
    ehdr_8,
    ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ehdr' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

