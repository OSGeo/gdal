#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read/write functionality for GeoTIFF format.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
# 
###############################################################################
# Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
# 
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
# 
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
# 
# You should have received a copy of the GNU Library General Public
# License along with this library; if not, write to the
# Free Software Foundation, Inc., 59 Temple Place - Suite 330,
# Boston, MA 02111-1307, USA.
###############################################################################

import os
import sys
import gdal
import string
import array

sys.path.append( '../pymod' )

import gdaltest

###############################################################################
# Get the GeoTIFF driver, and verify a few things about it. 

def tiff_write_1():

    gdaltest.tiff_drv = gdal.GetDriverByName( 'GTiff' )
    if gdaltest.tiff_drv is None:
        gdaltest.post_reason( 'GTiff driver not found!' )
        return 'false'
    
    drv_md = gdaltest.tiff_drv.GetMetadata()
    if drv_md['DMD_MIMETYPE'] != 'image/tiff':
        gdaltest.post_reason( 'mime type is wrong' )
        return 'false'

    return 'success'

###############################################################################
# Create a simple file by copying from an existing one.

def tiff_write_2():

    src_ds = gdal.Open( 'data/cfloat64.tif' )

    new_ds = gdaltest.tiff_drv.CreateCopy( 'tmp/test_2.tif', src_ds )

    bnd = new_ds.GetRasterBand(1)
    if bnd.Checksum() != 5028:
        gdaltest.post_reason( 'Didnt get expected checksum on still-open file')
        return 'false'

    bnd = None
    new_ds = None

    # hopefully it's closed now!

    new_ds = gdal.Open( 'tmp/test_2.tif' )
    bnd = new_ds.GetRasterBand(1)
    if bnd.Checksum() != 5028:
        gdaltest.post_reason( 'Didnt get expected checksum on reopened file')
        return 'false'

    if bnd.ComputeRasterMinMax() != (74.0, 255.0):
        gdaltest.post_reason( 'ComputeRasterMinMax() returned wrong value' )
        return 'false'

    bnd = None
    new_ds = None

    gdaltest.tiff_drv.Delete( 'tmp/test_2.tif' )

    return 'success'

###############################################################################
# Create a simple file by copying from an existing one.

def tiff_write_3():

    src_ds = gdal.Open( 'data/utmsmall.tif' )

    options = [ 'TILED=YES', 'BLOCKXSIZE=32', 'BLOCKYSIZE=32' ]
    
    new_ds = gdaltest.tiff_drv.CreateCopy( 'tmp/test_3.tif', src_ds,
                                           options = options )

    bnd = new_ds.GetRasterBand(1)
    if bnd.Checksum() != 50054:
        gdaltest.post_reason( 'Didnt get expected checksum on still-open file')
        return 'false'

    bnd = None
    new_ds = None

    gdaltest.tiff_drv.Delete( 'tmp/test_3.tif' )

    return 'success'

###############################################################################
# Create a tiled file.

def tiff_write_4():

    try:
        import gdalnumeric
        gdalnumeric.zeros
    except:
        return 'skip'

    src_ds = gdal.Open( 'data/utmsmall.tif' )

    options = [ 'TILED=YES', 'BLOCKXSIZE=32', 'BLOCKYSIZE=32' ]
    
    new_ds = gdaltest.tiff_drv.Create( 'tmp/test_4.tif', 40, 50, 3,
                                       gdal.GDT_Byte, options )

    data_red = gdalnumeric.zeros( (50, 40) )
    data_green = gdalnumeric.zeros( (50, 40) )
    data_blue = gdalnumeric.zeros( (50, 40) )

    for y in range(50):
        for x in range(40):
            data_red[y][x] = x
            data_green[y][x] = y
            data_blue[y][x] = x+y

    try:
        data_red   = data_red.astype(gdalnumeric.UnsignedInt8)
        data_green = data_green.astype(gdalnumeric.UnsignedInt8)
        data_blue  = data_blue.astype(gdalnumeric.UnsignedInt8)
    except AttributeError:
        data_red   = data_red.astype(gdalnumeric.uint8)
        data_green = data_green.astype(gdalnumeric.uint8)
        data_blue  = data_blue.astype(gdalnumeric.uint8)
        
    new_ds.GetRasterBand(1).WriteArray( data_red )
    new_ds.GetRasterBand(2).WriteArray( data_green )
    new_ds.GetRasterBand(3).WriteArray( data_blue )

    gt = (0.0, 1.0, 0.0, 50.0, 0.0, -1.0 )
    new_ds.SetGeoTransform( gt )

    if new_ds.GetRasterBand(1).Checksum() != 21577 \
       or new_ds.GetRasterBand(2).Checksum() != 20950 \
       or new_ds.GetRasterBand(3).Checksum() != 23730:
        gdaltest.post_reason( 'Wrong checksum.' )
        return 'false'

    if gt != new_ds.GetGeoTransform():
        gdaltest.post_reason( 'Wrong geotransform.' )
        return 'false'

    dict = {}
    dict['TEST_KEY'] = 'TestValue'
    new_ds.SetMetadata( dict )
    
    new_ds = None

    new_ds = gdal.Open( 'tmp/test_4.tif' )

    if new_ds.GetRasterBand(1).Checksum() != 21577 \
       or new_ds.GetRasterBand(2).Checksum() != 20950 \
       or new_ds.GetRasterBand(3).Checksum() != 23730:
        gdaltest.post_reason( 'Wrong checksum (2).' )
        return 'false'

    if gt != new_ds.GetGeoTransform():
        gdaltest.post_reason( 'Wrong geotransform(2).' )
        return 'false'

    nd = new_ds.GetRasterBand(1).GetNoDataValue()
    if nd is not None:
        gdaltest.post_reason( 'Got unexpected nodata value.' )
        return 'false'

    md_dict = new_ds.GetMetadata()
    if md_dict['TEST_KEY'] != 'TestValue':
        gdaltest.post_reason( 'Missing metadata' )
        return 'false'
                              
    new_ds = None
    
    gdaltest.tiff_drv.Delete( 'tmp/test_4.tif' )

    return 'success'

###############################################################################
# Write a file with GCPs.

def tiff_write_5():

    src_ds = gdal.Open( 'data/gcps.vrt' )

    new_ds = gdaltest.tiff_drv.CreateCopy( 'tmp/test_5.tif', src_ds )

    if string.find(new_ds.GetGCPProjection(),
                   'AUTHORITY["EPSG","26711"]') == -1:
        gdaltest.post_reason( 'GCP Projection not set properly.' )
        return 'false'
                   
    gcps = new_ds.GetGCPs()
    if len(gcps) != 4:
        gdaltest.post_reason( 'GCP count wrong.' )
        return 'false'

    new_ds = None

    gdaltest.tiff_drv.Delete( 'tmp/test_5.tif' )

    return 'success'

###############################################################################
# Test a mixture of reading and writing on a DEFLATE compressed file.

def tiff_write_6():

    options= [ 'TILED=YES', 'BLOCKSIZE=32', 'COMPRESS=DEFLATE' ]
    ds = gdaltest.tiff_drv.Create( 'tmp/test_6.tif', 200, 200, 1,
                                   gdal.GDT_Byte, options )

    # make a 32x32 byte buffer 
    buf = array.array('B',range(32)).tostring()
    for i in range(5):
        buf = buf + buf

    ds.WriteRaster( 0, 0, 32, 32, buf, buf_type = gdal.GDT_Byte )
    ds.FlushCache()
    ds.WriteRaster( 32, 0, 32, 32, buf, buf_type = gdal.GDT_Byte )
    ds.FlushCache()
    buf_read = ds.ReadRaster( 0, 0, 32, 32, buf_type = gdal.GDT_Byte )

    if buf_read != buf:
        gdaltest.post_reason( 'did not get back expected data.' )
        return 'fail'

    ds = None

    gdaltest.tiff_drv.Delete( 'tmp/test_6.tif' )

    return 'success'

###############################################################################
# Test a mixture of reading and writing on a LZW compressed file.

def tiff_write_7():

    options= [ 'TILED=YES', 'BLOCKSIZE=32', 'COMPRESS=LZW', 'PREDICTOR=2' ]
    ds = gdaltest.tiff_drv.Create( 'tmp/test_6.tif', 200, 200, 1,
                                   gdal.GDT_Byte, options )

    # make a 32x32 byte buffer 
    buf = array.array('B',range(32)).tostring()
    for i in range(5):
        buf = buf + buf

    ds.WriteRaster( 0, 0, 32, 32, buf, buf_type = gdal.GDT_Byte )
    ds.FlushCache()
    ds.WriteRaster( 32, 0, 32, 32, buf, buf_type = gdal.GDT_Byte )
    ds.FlushCache()
    buf_read = ds.ReadRaster( 0, 0, 32, 32, buf_type = gdal.GDT_Byte )

    if buf_read != buf:
        gdaltest.post_reason( 'did not get back expected data.' )
        return 'fail'

    ds = None

    gdaltest.tiff_drv.Delete( 'tmp/test_6.tif' )

    return 'success'

###############################################################################
# Test a mixture of reading and writing on a PACKBITS compressed file.

def tiff_write_8():

    options= [ 'TILED=YES', 'BLOCKSIZE=32', 'COMPRESS=PACKBITS' ]
    ds = gdaltest.tiff_drv.Create( 'tmp/test_6.tif', 200, 200, 1,
                                   gdal.GDT_Byte, options )

    # make a 32x32 byte buffer 
    buf = array.array('B',range(32)).tostring()
    for i in range(5):
        buf = buf + buf

    ds.WriteRaster( 0, 0, 32, 32, buf, buf_type = gdal.GDT_Byte )
    ds.FlushCache()
    ds.WriteRaster( 32, 0, 32, 32, buf, buf_type = gdal.GDT_Byte )
    ds.FlushCache()
    
    buf_read = ds.ReadRaster( 0, 0, 32, 32, buf_type = gdal.GDT_Byte )

    if buf_read != buf:
        gdaltest.post_reason( 'did not get back expected data.' )
        return 'fail'

    ds = None

    gdaltest.tiff_drv.Delete( 'tmp/test_6.tif' )

    return 'success'

###############################################################################
# Create a simple file by copying from an existing one.

def tiff_write_9():

    src_ds = gdal.Open( 'data/byte.tif' )

    new_ds = gdaltest.tiff_drv.CreateCopy( 'tmp/test_9.tif', src_ds,
                                           options = [ 'NBITS=5' ] )

    new_ds = None
    src_ds = None

    # hopefully it's closed now!

    new_ds = gdal.Open( 'tmp/test_9.tif' )
    bnd = new_ds.GetRasterBand(1)
    if bnd.Checksum() != 4084:
        gdaltest.post_reason( 'Didnt get expected checksum on reopened file')
        return 'false'

    bnd = None
    new_ds = None

    gdaltest.tiff_drv.Delete( 'tmp/test_9.tif' )

    return 'success'

###############################################################################
# 1bit file but with band interleaving, and odd size (not multiple of 8) #1957

def tiff_write_10():

    ut = gdaltest.GDALTest( 'GTiff', 'oddsize_1bit2b.tif', 2, 5918,
                            options = [ 'NBITS=1', 'INTERLEAVE=BAND' ] )
    return ut.testCreate( out_bands = 2 )

def tiff_write_cleanup():
    gdaltest.tiff_drv = None

    return 'success'

gdaltest_list = [
    tiff_write_1,
    tiff_write_2,
    tiff_write_3,
    tiff_write_4,
    tiff_write_5,
    tiff_write_6,
    tiff_write_7,
    tiff_write_8,
    tiff_write_9,
    tiff_write_10,
    tiff_write_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'tiff_write' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

