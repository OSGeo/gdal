#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test MEM format driver.
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
import string

sys.path.append( '../pymod' )

import gdaltest

#
# TODO:
# o Add testing of add band.

###############################################################################
# Create a MEM dataset, and set some data, then test it.

def mem_1():

    #######################################################
    # Setup dataset
    drv = gdal.GetDriverByName('MEM')
    gdaltest.mem_ds = drv.Create( 'mem_1.mem', 50, 3 )
    ds = gdaltest.mem_ds

    raw_data = array.array('f',list(range(150))).tostring()
    ds.WriteRaster( 0, 0, 50, 3, raw_data,
                    buf_type = gdal.GDT_Float32,
                    band_list = [1] )

    wkt = gdaltest.user_srs_to_wkt( 'EPSG:26711' )
    ds.SetProjection( wkt )

    gt = ( 440720, 5, 0, 3751320, 0, -5 )
    ds.SetGeoTransform( gt )

    band = ds.GetRasterBand(1)
    band.SetNoDataValue( -1.0 )

    #######################################################
    # Verify dataset.

    if band.GetNoDataValue() != -1.0:
        gdaltest.post_reason( 'no data is wrong' )
        return 'fail'

    if ds.GetProjection() != wkt:
        gdaltest.post_reason( 'projection wrong' )
        return 'fail'

    if ds.GetGeoTransform() != gt:
        gdaltest.post_reason( 'geotransform wrong' )
        return 'fail'

    if band.Checksum() != 1531:
        gdaltest.post_reason( 'checksum wrong' )
        print(band.Checksum())
        return 'fail'

    gdaltest.mem_ds = None

    return 'success'

###############################################################################
# Open an in-memory array. 

def mem_2():

    # allocate band data array.

    try:
        p = gdal.ptrcreate( 'float', 5, 150 )
    except:
        return 'skip'
    
    tokens = string.split(p,'_')

    # build ds name.
    dsname = 'MEM:::DATAPOINTER=0x'+tokens[1]+',PIXELS=50,LINES=3,BANDS=1,' \
             + 'DATATYPE=Float32,PIXELOFFSET=4,LINEOFFSET=200,BANDOFFSET=0'

    ds = gdal.Open( dsname )
    if ds is None:
        gdaltest.post_reason( 'opening MEM dataset failed.' )
        return 'fail'

    chksum = ds.GetRasterBand(1).Checksum()
    if chksum != 750:
        gdaltest.post_reason( 'checksum failed.' )
        print(chksum)
        return 'fail'

    ds.GetRasterBand(1).Fill( 100.0 )
    ds.FlushCache()

    if gdal.ptrvalue(p,5) != 100.0:
        print(gdal.ptrvalue(p,5))
        gdaltest.post_reason( 'fill seems to have failed.' )
        return 'fail'

    ds = None

    gdal.ptrfree( p )

    return 'success'

###############################################################################
# Test creating a MEM dataset with the "MEM:::" name

def mem_3():

    drv = gdal.GetDriverByName('MEM')
    ds = drv.Create( 'MEM:::', 1, 1, 1 )
    if ds is None:
        return 'fail'
    ds = None

    return 'success'

###############################################################################
# Test creating a band interleaved multi-band MEM dataset

def mem_4():

    drv = gdal.GetDriverByName('MEM')

    ds = drv.Create( '', 100, 100, 3 )
    expected_cs = [ 0, 0, 0 ]
    for i in range(3):
        cs = ds.GetRasterBand(i+1).Checksum()
        if cs != expected_cs[i]:
            gdaltest.post_reason('did not get expected checksum for band %d' % (i+1))
            print(cs)
            return 'fail'

    ds.GetRasterBand(1).Fill(255)
    expected_cs = [ 57182, 0, 0 ]
    for i in range(3):
        cs = ds.GetRasterBand(i+1).Checksum()
        if cs != expected_cs[i]:
            gdaltest.post_reason('did not get expected checksum for band %d after fill' % (i+1))
            print(cs)
            return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test creating a pixel interleaved multi-band MEM dataset

def mem_5():

    drv = gdal.GetDriverByName('MEM')

    ds = drv.Create( '', 100, 100, 3, options = ['INTERLEAVE=PIXEL'] )
    expected_cs = [ 0, 0, 0 ]
    for i in range(3):
        cs = ds.GetRasterBand(i+1).Checksum()
        if cs != expected_cs[i]:
            gdaltest.post_reason('did not get expected checksum for band %d' % (i+1))
            print(cs)
            return 'fail'

    ds.GetRasterBand(1).Fill(255)
    expected_cs = [ 57182, 0, 0 ]
    for i in range(3):
        cs = ds.GetRasterBand(i+1).Checksum()
        if cs != expected_cs[i]:
            gdaltest.post_reason('did not get expected checksum for band %d after fill' % (i+1))
            print(cs)
            return 'fail'

    if ds.GetMetadataItem('INTERLEAVE', 'IMAGE_STRUCTURE') != 'PIXEL':
        gdaltest.post_reason('did not get expected INTERLEAVE value')
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# cleanup

def mem_cleanup():
    gdaltest.mem_ds = None
    return 'success'


gdaltest_list = [
    mem_1,
    mem_2,
    mem_3,
    mem_4,
    mem_5,
    mem_cleanup ]
  


if __name__ == '__main__':

    gdaltest.setup_run( 'mem' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

