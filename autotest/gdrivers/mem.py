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
from osgeo import gdal
import array
import string

sys.path.append( '../pymod' )

import gdaltest

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

    # Set GCPs()
    wkt_gcp = gdaltest.user_srs_to_wkt( 'EPSG:4326' )
    gcps = [ gdal.GCP(0,1,2,3,4) ]
    ds.SetGCPs([], "")
    ds.SetGCPs(gcps, wkt_gcp)
    ds.SetGCPs([], "")
    ds.SetGCPs(gcps, wkt_gcp)
    ds.SetGCPs(gcps, wkt_gcp)

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

    if ds.GetGCPCount() != 1:
        gdaltest.post_reason( 'GetGCPCount wrong' )
        return 'fail'

    if len(ds.GetGCPs()) != 1:
        gdaltest.post_reason( 'GetGCPs wrong' )
        return 'fail'

    if ds.GetGCPProjection() != wkt_gcp:
        gdaltest.post_reason( 'GetGCPProjection wrong' )
        return 'fail'

    gdaltest.mem_ds = None

    return 'success'

###############################################################################
# Open an in-memory array. 

def mem_2():

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = gdal.Open( 'MEM:::' )
    gdal.PopErrorHandler()
    if ds is not None:
        gdaltest.post_reason( 'opening MEM dataset should have failed.' )
        return 'fail'

    try:
        import ctypes
    except:
        return 'skip'

    for libname in ['msvcrt', 'libc.so.6']:
        try:
            crt = ctypes.CDLL(libname)
        except:
            crt = None
        if crt is not None:
            break

    if crt is None:
        return 'skip'

    malloc = crt.malloc
    malloc.argtypes = [ctypes.c_size_t]
    malloc.restype = ctypes.c_void_p

    free = crt.free
    free.argtypes = [ctypes.c_void_p]
    free.restype = None

    # allocate band data array.
    width = 50
    height = 3
    p = malloc(width*height*4)
    if p is None:
        return 'skip'
    float_p = ctypes.cast(p,ctypes.POINTER(ctypes.c_float))

    # build ds name.
    dsnames = ['MEM:::DATAPOINTER=0x%X,PIXELS=%d,LINES=%d,BANDS=1,DATATYPE=Float32,PIXELOFFSET=4,LINEOFFSET=%d,BANDOFFSET=0' % (p, width, height, width * 4),
               'MEM:::DATAPOINTER=0x%X,PIXELS=%d,LINES=%d,DATATYPE=Float32' % (p, width, height) ]

    for dsname in dsnames:

        for i in range(width*height):
            float_p[i] = 5.0

        ds = gdal.Open( dsname )
        if ds is None:
            gdaltest.post_reason( 'opening MEM dataset failed.' )
            free(p)
            return 'fail'

        chksum = ds.GetRasterBand(1).Checksum()
        if chksum != 750:
            gdaltest.post_reason( 'checksum failed.' )
            print(chksum)
            free(p)
            return 'fail'

        ds.GetRasterBand(1).Fill( 100.0 )
        ds.FlushCache()

        if float_p[0] != 100.0:
            print(float_p[0])
            gdaltest.post_reason( 'fill seems to have failed.' )
            free(p)
            return 'fail'

        ds = None

    free(p)

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
# Test out-of-memory situations (simulated by multiplication overflows)

def mem_6():

    drv = gdal.GetDriverByName('MEM')

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = drv.Create( '', 0x7FFFFFFF, 0x7FFFFFFF, 16, options = ['INTERLEAVE=PIXEL'] )
    gdal.PopErrorHandler()
    if ds is not None:
        return 'fail'
    ds = None

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = drv.Create( '', 0x7FFFFFFF, 0x7FFFFFFF, 16, gdal.GDT_Float64 )
    gdal.PopErrorHandler()
    if ds is not None:
        return 'fail'
    ds = None

    return 'success'

###############################################################################
# Test AddBand()

def mem_7():

    drv = gdal.GetDriverByName('MEM')
    ds = drv.Create( 'MEM:::', 1, 1, 1 )
    ds.AddBand(gdal.GDT_Byte, [])
    if ds.RasterCount != 2:
        return 'fail'
    ds = None

    return 'success'

###############################################################################
# Test SetDefaultHistogram() / GetDefaultHistogram()

def mem_8():

    drv = gdal.GetDriverByName('MEM')
    ds = drv.Create( 'MEM:::', 1, 1, 1 )
    ds.GetRasterBand(1).SetDefaultHistogram(0,255,[])
    ds.GetRasterBand(1).SetDefaultHistogram(1,2,[5,6])
    ds.GetRasterBand(1).SetDefaultHistogram(1,2,[3,4])
    hist = ds.GetRasterBand(1).GetDefaultHistogram(force=0)
    ds = None

    if hist != (1.0, 2.0, 2, [3, 4]):
        print(hist)
        return 'fail'

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
    mem_6,
    mem_7,
    mem_8,
    mem_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'mem' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

