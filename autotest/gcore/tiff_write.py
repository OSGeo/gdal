#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read/write functionality for GeoTIFF format.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2008-2013, Even Rouault <even dot rouault at mines-paris dot org>
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
from osgeo import gdal
import string
import array
import shutil
from osgeo import osr
import sys

sys.path.append( '../pymod' )

import gdaltest

run_tiff_write_api_proxy = True

###############################################################################
# Get the GeoTIFF driver, and verify a few things about it.

def tiff_write_1():

    gdaltest.oldCacheSize = gdal.GetCacheMax()
    gdaltest.tiff_drv = gdal.GetDriverByName( 'GTiff' )
    if gdaltest.tiff_drv is None:
        gdaltest.post_reason( 'GTiff driver not found!' )
        return 'fail'

    drv_md = gdaltest.tiff_drv.GetMetadata()
    if drv_md['DMD_MIMETYPE'] != 'image/tiff':
        gdaltest.post_reason( 'mime type is wrong' )
        return 'fail'

    return 'success'

###############################################################################
# Create a simple file by copying from an existing one.

def tiff_write_2():

    src_ds = gdal.Open( 'data/cfloat64.tif' )

    new_ds = gdaltest.tiff_drv.CreateCopy( 'tmp/test_2.tif', src_ds )

    bnd = new_ds.GetRasterBand(1)
    if bnd.Checksum() != 5028:
        gdaltest.post_reason( 'Didnt get expected checksum on still-open file')
        return 'fail'

    bnd = None
    new_ds = None

    # hopefully it's closed now!

    new_ds = gdal.Open( 'tmp/test_2.tif' )
    bnd = new_ds.GetRasterBand(1)
    if bnd.Checksum() != 5028:
        gdaltest.post_reason( 'Didnt get expected checksum on reopened file')
        return 'fail'

    if bnd.ComputeRasterMinMax() != (74.0, 255.0):
        gdaltest.post_reason( 'ComputeRasterMinMax() returned wrong value' )
        return 'fail'

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
        return 'fail'

    bnd = None
    new_ds = None

    gdaltest.tiff_drv.Delete( 'tmp/test_3.tif' )

    return 'success'

###############################################################################
# Create a tiled file.

def tiff_write_4():

    try:
        from osgeo import gdalnumeric
    except:
        return 'skip'

    options = [ 'TILED=YES', 'BLOCKXSIZE=32', 'BLOCKYSIZE=32' ]

    new_ds = gdaltest.tiff_drv.Create( 'tmp/test_4.tif', 40, 50, 3,
                                       gdal.GDT_Byte, options )

    try:
        data_red = gdalnumeric.zeros( (50, 40) )
        data_green = gdalnumeric.zeros( (50, 40) )
        data_blue = gdalnumeric.zeros( (50, 40) )
    except:
        import numpy
        data_red = numpy.zeros( (50, 40) )
        data_green = numpy.zeros( (50, 40) )
        data_blue = numpy.zeros( (50, 40) )

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
        try:
            data_red   = data_red.astype(gdalnumeric.uint8)
            data_green = data_green.astype(gdalnumeric.uint8)
            data_blue  = data_blue.astype(gdalnumeric.uint8)
        except:
            pass

    new_ds.GetRasterBand(1).WriteArray( data_red )
    new_ds.GetRasterBand(2).WriteArray( data_green )
    new_ds.GetRasterBand(3).WriteArray( data_blue )

    gt = (0.0, 1.0, 0.0, 50.0, 0.0, -1.0 )
    new_ds.SetGeoTransform( gt )

    if new_ds.GetRasterBand(1).Checksum() != 21577 \
       or new_ds.GetRasterBand(2).Checksum() != 20950 \
       or new_ds.GetRasterBand(3).Checksum() != 23730:
        gdaltest.post_reason( 'Wrong checksum.' )
        return 'fail'

    if gt != new_ds.GetGeoTransform():
        gdaltest.post_reason( 'Wrong geotransform.' )
        return 'fail'

    dict = {}
    dict['TEST_KEY'] = 'TestValue'
    new_ds.SetMetadata( dict )

    new_ds = None

    new_ds = gdal.Open( 'tmp/test_4.tif' )

    if new_ds.GetRasterBand(1).Checksum() != 21577 \
       or new_ds.GetRasterBand(2).Checksum() != 20950 \
       or new_ds.GetRasterBand(3).Checksum() != 23730:
        gdaltest.post_reason( 'Wrong checksum (2).' )
        return 'fail'

    if gt != new_ds.GetGeoTransform():
        gdaltest.post_reason( 'Wrong geotransform(2).' )
        return 'fail'

    nd = new_ds.GetRasterBand(1).GetNoDataValue()
    if nd is not None:
        gdaltest.post_reason( 'Got unexpected nodata value.' )
        return 'fail'

    md_dict = new_ds.GetMetadata()
    if md_dict['TEST_KEY'] != 'TestValue':
        gdaltest.post_reason( 'Missing metadata' )
        return 'fail'

    new_ds = None

    gdaltest.tiff_drv.Delete( 'tmp/test_4.tif' )

    return 'success'

###############################################################################
# Write a file with GCPs.

def tiff_write_5():

    src_ds = gdal.Open( 'data/gcps.vrt' )

    new_ds = gdaltest.tiff_drv.CreateCopy( 'tmp/test_5.tif', src_ds )

    if new_ds.GetGCPProjection().find(
                   'AUTHORITY["EPSG","26711"]') == -1:
        gdaltest.post_reason( 'GCP Projection not set properly.' )
        return 'fail'

    gcps = new_ds.GetGCPs()
    if len(gcps) != 4:
        gdaltest.post_reason( 'GCP count wrong.' )
        return 'fail'

    new_ds = None

    gdaltest.tiff_drv.Delete( 'tmp/test_5.tif' )

    # Test SetGCPs on a new GTiff
    new_ds = gdaltest.tiff_drv.Create(  'tmp/test_5.tif', 10, 10, 1)
    new_ds.SetGCPs(gcps, src_ds.GetGCPProjection() )
    new_ds = None

    new_ds = gdal.Open('tmp/test_5.tif')
    gcps = new_ds.GetGCPs()
    if len(gcps) != 4:
        gdaltest.post_reason( 'GCP count wrong.' )
        return 'fail'
    new_ds = None

    gdaltest.tiff_drv.Delete( 'tmp/test_5.tif' )

    return 'success'

###############################################################################
# Test a mixture of reading and writing on a DEFLATE compressed file.
# May crash with libtiff <= 3.8.2, so skip it if BigTIFF is not supported
# (this is a sign of an older libtiff...)

def tiff_write_6():

    md = gdaltest.tiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('BigTIFF') == -1:
        return 'skip'

    options= [ 'TILED=YES', 'BLOCKXSIZE=32', 'BLOCKYSIZE=32',
               'COMPRESS=DEFLATE', 'PREDICTOR=2' ]
    ds = gdaltest.tiff_drv.Create( 'tmp/test_6.tif', 200, 200, 1,
                                   gdal.GDT_Byte, options )

    # make a 32x32 byte buffer
    buf = array.array('B',list(range(32))).tostring()
    for i in range(5):
        buf = buf + buf

    ds.WriteRaster( 0, 0, 32, 32, buf, buf_type = gdal.GDT_Byte )
    ds.FlushCache()
    ds.WriteRaster( 32, 0, 32, 32, buf, buf_type = gdal.GDT_Byte )
    ds.FlushCache()
    buf_read = ds.ReadRaster( 0, 0, 32, 32, buf_type = gdal.GDT_Byte )

    if buf_read != buf:
        gdaltest.tiff_write_6_failed = True
        gdaltest.post_reason( 'did not get back expected data.' )
        return 'fail'

    ds = None

    gdaltest.tiff_write_6_failed = False
    gdaltest.tiff_drv.Delete( 'tmp/test_6.tif' )

    return 'success'

###############################################################################
# Test a mixture of reading and writing on a LZW compressed file.
# Will cause older libtiff versions (<=3.8.2 for sure) to crash, so skip it
# if BigTIFF is not supported (this is a sign of an older libtiff...)

def tiff_write_7():

    md = gdaltest.tiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('BigTIFF') == -1:
        return 'skip'

    options= [ 'TILED=YES', 'COMPRESS=LZW', 'PREDICTOR=2' ]
    ds = gdaltest.tiff_drv.Create( 'tmp/test_7.tif', 200, 200, 1,
                                   gdal.GDT_Byte, options )

    # make a 32x32 byte buffer
    buf = array.array('B',list(range(32))).tostring()
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

    gdaltest.tiff_drv.Delete( 'tmp/test_7.tif' )

    return 'success'

###############################################################################
# Test a mixture of reading and writing on a PACKBITS compressed file.

def tiff_write_8():

    options= [ 'TILED=YES', 'BLOCKXSIZE=32', 'BLOCKYSIZE=32', 'COMPRESS=PACKBITS' ]
    ds = gdaltest.tiff_drv.Create( 'tmp/test_8.tif', 200, 200, 1,
                                   gdal.GDT_Byte, options )

    # make a 32x32 byte buffer
    buf = array.array('B',list(range(32))).tostring()
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

    gdaltest.tiff_drv.Delete( 'tmp/test_8.tif' )

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
    if bnd.Checksum() != 5287:
        gdaltest.post_reason( 'Didnt get expected checksum on reopened file')
        return 'fail'

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

###############################################################################
# Simple 1 bit file, treated through the GTiffBitmapBand class.

def tiff_write_11():

    ut = gdaltest.GDALTest( 'GTiff', 'oddsize1bit.tif', 1, 5918,
                            options = [ 'NBITS=1', 'COMPRESS=CCITTFAX4' ] )
    return ut.testCreateCopy()

###############################################################################
# Read JPEG Compressed YCbCr subsampled image.

def tiff_write_12():

    md = gdaltest.tiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('JPEG') == -1:
        return 'skip'

    ds = gdal.Open('data/sasha.tif')
    cs = ds.GetRasterBand(3).Checksum()
    if cs != 31952 and cs != 30145:
        return 'fail'

    return 'success'

###############################################################################
# Write JPEG Compressed YCbCr subsampled image.

def tiff_write_13():

    md = gdaltest.tiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('JPEG') == -1:
        return 'skip'

    src_ds = gdal.Open('data/sasha.tif')
    ds = gdaltest.tiff_drv.CreateCopy('tmp/sasha.tif', src_ds, options = [ 'TILED=YES',
                                                                           'COMPRESS=JPEG',
                                                                           'PHOTOMETRIC=YCBCR',
                                                                           'JPEG_QUALITY=31' ])
    ds = None

    ds = gdal.Open('tmp/sasha.tif')
    cs = ds.GetRasterBand(3).Checksum()
    ds = None

    os.unlink('tmp/sasha.tif')
    if cs != 17347 and cs != 14445:
        print(cs)
        return 'fail'

    return 'success'

###############################################################################
# Test creating an in memory copy.

def tiff_write_14():

    tst = gdaltest.GDALTest( 'GTiff', 'byte.tif', 1, 4672 )

    return tst.testCreateCopy( vsimem = 1 )


###############################################################################
# Test that we can restrict metadata and georeferencing in the output
# file using the PROFILE creation option with CreateCopy()

def tiff_write_15():

    drv = gdal.GetDriverByName( 'GTiff' )

    ds_in = gdal.Open('data/byte.vrt')

    ds = gdaltest.tiff_drv.CreateCopy( 'tmp/tw_15.tif', ds_in, options=['PROFILE=BASELINE'] )

    ds_in = None
    ds = None

    ds = gdal.Open( 'tmp/tw_15.tif' )
    if ds.GetGeoTransform() != (0.0,1.0,0.0,0.0,0.0,1.0):
        gdaltest.post_reason( 'Got wrong geotransform, profile ignored?' )
        return 'fail'

    md = ds.GetMetadata()
    if 'test' not in md:
        gdaltest.post_reason( 'Metadata absent from .aux.xml file.' )
        return 'fail'

    md = ds.GetRasterBand(1).GetMetadata()
    if 'testBand' not in md:
        gdaltest.post_reason( 'Metadata absent from .aux.xml file.' )
        return 'fail'

    ds = None

    try:
        os.remove( 'tmp/tw_15.tif.aux.xml' )
    except:
        try:
            os.stat( 'tmp/tw_15.tif.aux.xml' )
        except:
            gdaltest.post_reason( 'No .aux.xml file.' )
            return 'fail'
            pass

    ds = gdal.Open( 'tmp/tw_15.tif' )

    md = ds.GetMetadata()
    if 'test' in md:
        gdaltest.post_reason( 'Metadata written to BASELINE file.' )
        return 'fail'

    md = ds.GetRasterBand(1).GetMetadata()
    if 'testBand' in md:
        gdaltest.post_reason( 'Metadata written to BASELINE file.' )
        return 'fail'

    ds = None

    gdaltest.tiff_drv.Delete( 'tmp/tw_15.tif' )

    return 'success'

###############################################################################
# Test that we can restrict metadata and georeferencing in the output
# file using the PROFILE creation option with Create()

def tiff_write_16():

    ds_in = gdal.Open('data/byte.vrt')

    ds = gdaltest.tiff_drv.Create( 'tmp/tw_16.tif', 20, 20, gdal.GDT_Byte,
                     options=['PROFILE=BASELINE'] )

    ds.SetMetadata( {'test':'testvalue'} )
    ds.GetRasterBand(1).SetMetadata( {'testBand':'testvalueBand'} )

    ds.SetGeoTransform( (10,5,0,30,0,-5) )

    data = ds_in.ReadRaster( 0, 0, 20, 20 )
    ds.WriteRaster( 0, 0, 20, 20, data )

    ds_in = None
    ds = None

    ds = gdal.Open( 'tmp/tw_16.tif' )
    if ds.GetGeoTransform() != (0.0,1.0,0.0,0.0,0.0,1.0):
        gdaltest.post_reason( 'Got wrong geotransform, profile ignored?' )
        return 'fail'

    md = ds.GetMetadata()
    if 'test' not in md:
        gdaltest.post_reason( 'Metadata absent from .aux.xml file.' )
        return 'fail'

    md = ds.GetRasterBand(1).GetMetadata()
    if 'testBand' not in md:
        gdaltest.post_reason( 'Metadata absent from .aux.xml file.' )
        return 'fail'

    ds = None

    try:
        os.remove( 'tmp/tw_16.tif.aux.xml' )
    except:
        try:
            os.stat( 'tmp/tw_16.tif.aux.xml' )
        except:
            gdaltest.post_reason( 'No .aux.xml file.' )
            return 'fail'
            pass

    ds = gdal.Open( 'tmp/tw_16.tif' )

    md = ds.GetMetadata()
    if 'test' in md:
        gdaltest.post_reason( 'Metadata written to BASELINE file.' )
        return 'fail'

    md = ds.GetRasterBand(1).GetMetadata()
    if 'testBand' in md:
        gdaltest.post_reason( 'Metadata written to BASELINE file.' )
        return 'fail'

    ds = None

    gdaltest.tiff_drv.Delete( 'tmp/tw_16.tif' )

    return 'success'

###############################################################################
# Test writing a TIFF with an RPC tag.

def tiff_write_17():

    # Translate RPC controlled data to GeoTIFF.

    ds_in = gdal.Open('data/rpc.vrt')
    rpc_md = ds_in.GetMetadata('RPC')

    ds = gdaltest.tiff_drv.CreateCopy( 'tmp/tw_17.tif', ds_in )

    ds_in = None
    ds = None

    # Ensure there is no .aux.xml file which might hold the RPC.
    try:
        os.remove( 'tmp/tm_17.tif.aux.xml' )
    except:
        pass

    # confirm there is no .rpb file created by default.
    try:
        open('tmp/tm_17.RPB').read()
        gdaltest.post_reason( 'unexpectedly found .RPB file' )
        return 'fail'
    except:
        pass

    # Open the dataset, and confirm the RPC data is still intact.
    ds = gdal.Open( 'tmp/tw_17.tif' )
    if not gdaltest.rpcs_equal(ds.GetMetadata('RPC'),rpc_md):
        return 'fail'

    ds = None

    gdaltest.tiff_drv.Delete( 'tmp/tw_17.tif' )

    return 'success'

###############################################################################
# Test that above test still work with the optimization in the GDAL_DISABLE_READDIR_ON_OPEN
# case (#3996)

def tiff_write_17_disable_readdir():
    oldval = gdal.GetConfigOption('GDAL_DISABLE_READDIR_ON_OPEN')
    gdal.SetConfigOption('GDAL_DISABLE_READDIR_ON_OPEN', 'TRUE')
    ret = tiff_write_17()
    gdal.SetConfigOption('GDAL_DISABLE_READDIR_ON_OPEN', oldval)
    return ret

###############################################################################
# Test writing a TIFF with an RPB file and IMD file.

def tiff_write_18():

    # Translate RPC controlled data to GeoTIFF.

    ds_in = gdal.Open('data/rpc.vrt')
    rpc_md = ds_in.GetMetadata('RPC')

    ds = gdaltest.tiff_drv.CreateCopy( 'tmp/tw_18.tif', ds_in,
                         options = [ 'PROFILE=BASELINE' ] )

    ds_in = None
    ds = None

    # Ensure there is no .aux.xml file which might hold the RPC.
    try:
        os.remove( 'tmp/tm_18.tif.aux.xml' )
    except:
        pass

    # confirm there is an .rpb and .imd file.
    try:
        open('tmp/tw_18.RPB').read()
        open('tmp/tw_18.IMD').read()
    except:
        gdaltest.post_reason( 'missing .RPB or .IMD file.' )
        return 'fail'

    # Open the dataset, and confirm the RPC/IMD data is still intact.
    ds = gdal.Open( 'tmp/tw_18.tif' )

    if not gdaltest.rpcs_equal(ds.GetMetadata('RPC'),rpc_md):
        return 'fail'

    imd_md = ds.GetMetadata('IMD')
    if imd_md['version'] != '"R"' \
       or imd_md['numColumns'] != '30324' \
       or imd_md['IMAGE_1.sunEl'] != '39.7':
        gdaltest.post_reason( 'IMD contents wrong?' )
        print(imd_md)
        return 'fail'

    ds = None

    # Test differed loading with GetMetadataItem()
    ds = gdal.Open( 'tmp/tw_18.tif' )
    if ds.GetMetadataItem('LINE_OFF', 'RPC') != '16201':
        gdaltest.post_reason("wrong value for GetMetadataItem('LINE_OFF', 'RPC')")
        return 'fail'
    if ds.GetMetadataItem('version', 'IMD') != '"R"':
        gdaltest.post_reason("wrong value for GetMetadataItem('version', 'IMD')")
        return 'fail'
    ds = None

    gdaltest.tiff_drv.Delete( 'tmp/tw_18.tif' )

    # Confirm IMD and RPC files are cleaned up.  If not likely the
    # file list functionality is not working properly.
    try:
        open('tmp/tw_18.RPB').read()
        gdaltest.post_reason( 'RPB did not get cleaned up.' )
        return 'fail'
    except:
        pass

    try:
        open('tmp/tw_18.IMD').read()
        gdaltest.post_reason( 'IMD did not get cleaned up.' )
        return 'fail'
    except:
        pass

    return 'success'

###############################################################################
# Test that above test still work with the optimization in the GDAL_DISABLE_READDIR_ON_OPEN
# case (#3996)

def tiff_write_18_disable_readdir():
    oldval = gdal.GetConfigOption('GDAL_DISABLE_READDIR_ON_OPEN')
    gdal.SetConfigOption('GDAL_DISABLE_READDIR_ON_OPEN', 'TRUE')
    ret = tiff_write_18()
    gdal.SetConfigOption('GDAL_DISABLE_READDIR_ON_OPEN', oldval)
    return ret

###############################################################################
# Test the write of a pixel-interleaved image with NBITS = 7

def tiff_write_19():

    src_ds = gdal.Open( 'data/contig_strip.tif' )

    new_ds = gdaltest.tiff_drv.CreateCopy( 'tmp/contig_strip_7.tif', src_ds,
                                           options = [ 'NBITS=7', 'INTERLEAVE=PIXEL' ] )

    new_ds = None

    # hopefully it's closed now!

    new_ds = gdal.Open( 'tmp/contig_strip_7.tif' )
    if new_ds.GetRasterBand(1).Checksum() != src_ds.GetRasterBand(1).Checksum() or \
       new_ds.GetRasterBand(2).Checksum() != src_ds.GetRasterBand(2).Checksum() or \
       new_ds.GetRasterBand(3).Checksum() != src_ds.GetRasterBand(3).Checksum() :
        gdaltest.post_reason( 'Didnt get expected checksum on reopened file')
        return 'fail'

    new_ds = None
    src_ds = None

    gdaltest.tiff_drv.Delete( 'tmp/contig_strip_7.tif' )

    return 'success'

###############################################################################
# Test write and read of some TIFF tags
# Expected to fail (properly) with older libtiff versions (<=3.8.2 for sure)

def tiff_write_20():

    new_ds = gdaltest.tiff_drv.Create( 'tmp/tags.tif', 1, 1, 1)

    values = [ ('TIFFTAG_DOCUMENTNAME'    , 'document_name'),
               ('TIFFTAG_IMAGEDESCRIPTION', 'image_description'),
               ('TIFFTAG_SOFTWARE'        , 'software'),
               ('TIFFTAG_DATETIME'        , '2009/01/01 13:01:08'),
               ('TIFFTAG_ARTIST'          , 'artitst'),
               ('TIFFTAG_HOSTCOMPUTER'    , 'host_computer'),
               ('TIFFTAG_COPYRIGHT'       , 'copyright'),
               ('TIFFTAG_XRESOLUTION'     , '100'),
               ('TIFFTAG_YRESOLUTION'     , '101'),
               ('TIFFTAG_RESOLUTIONUNIT'  , '2 (pixels/inch)'),
             ]

    dict = {}
    for item in values:
        dict[item[0]] = item[1]
    new_ds.SetMetadata(dict)

    new_ds = None

    # hopefully it's closed now!

    new_ds = gdal.Open( 'tmp/tags.tif' )
    md = new_ds.GetMetadata()
    for item in values:
        if item[0] not in md:
            gdaltest.post_reason( 'Couldnt find tag %s' %(item[0]))
            return 'fail'

        if md[item[0]] != item[1]:
            gdaltest.post_reason( 'For tag %s, got %s, expected %s' %(item[0], md[item[0]], item[1]))
            return 'fail'

    new_ds = None
    src_ds = None

    gdaltest.tiff_drv.Delete( 'tmp/tags.tif' )

    return 'success'

###############################################################################
# Test RGBA images with TIFFTAG_EXTRASAMPLES=EXTRASAMPLE_ASSOCALPHA

def tiff_write_21():

    src_ds = gdal.Open( 'data/stefan_full_rgba.tif' )

    new_ds = gdaltest.tiff_drv.CreateCopy( 'tmp/stefan_full_rgba.tif', src_ds )

    new_ds = None

    new_ds = gdal.Open( 'tmp/stefan_full_rgba.tif' )
    if new_ds.RasterCount != 4:
        return 'fail'
    for i in range(4):
        if new_ds.GetRasterBand(i+1).GetRasterColorInterpretation() != src_ds.GetRasterBand(i+1).GetRasterColorInterpretation():
            return 'fail'
        if new_ds.GetRasterBand(i+1).Checksum() != src_ds.GetRasterBand(i+1).Checksum():
            return 'fail'

    new_ds = None
    src_ds = None

    gdaltest.tiff_drv.Delete( 'tmp/stefan_full_rgba.tif' )

    return 'success'

###############################################################################
# Test RGBA images with TIFFTAG_EXTRASAMPLES=EXTRASAMPLE_UNSPECIFIED

def tiff_write_22():

    src_ds = gdal.Open( 'data/stefan_full_rgba_photometric_rgb.tif' )

    new_ds = gdaltest.tiff_drv.CreateCopy( 'tmp/stefan_full_rgba_photometric_rgb.tif', src_ds, options = [ 'PHOTOMETRIC=RGB' ] )

    new_ds = None

    new_ds = gdal.Open( 'tmp/stefan_full_rgba_photometric_rgb.tif' )
    if new_ds.RasterCount != 4:
        return 'fail'
    for i in range(4):
        if new_ds.GetRasterBand(i+1).GetRasterColorInterpretation() != src_ds.GetRasterBand(i+1).GetRasterColorInterpretation():
            return 'fail'
        if new_ds.GetRasterBand(i+1).Checksum() != src_ds.GetRasterBand(i+1).Checksum():
            return 'fail'

    new_ds = None
    src_ds = None

    gdaltest.tiff_drv.Delete( 'tmp/stefan_full_rgba_photometric_rgb.tif' )

    return 'success'

###############################################################################
# Test grey+alpha images with ALPHA=YES

def tiff_write_23():

    src_ds = gdal.Open( 'data/stefan_full_greyalpha.tif' )

    new_ds = gdaltest.tiff_drv.CreateCopy( 'tmp/stefan_full_greyalpha.tif', src_ds, options = [ 'ALPHA=YES' ] )

    new_ds = None

    new_ds = gdal.Open( 'tmp/stefan_full_greyalpha.tif' )
    if new_ds.RasterCount != 2:
        return 'fail'
    for i in range(2):
        if new_ds.GetRasterBand(i+1).GetRasterColorInterpretation() != src_ds.GetRasterBand(i+1).GetRasterColorInterpretation():
            return 'fail'
        if new_ds.GetRasterBand(i+1).Checksum() != src_ds.GetRasterBand(i+1).Checksum():
            return 'fail'

    new_ds = None
    src_ds = None

    gdaltest.tiff_drv.Delete( 'tmp/stefan_full_greyalpha.tif' )

    return 'success'

###############################################################################
# Test grey+alpha images without ALPHA=YES

def tiff_write_24():

    src_ds = gdal.Open( 'data/stefan_full_greyalpha.tif' )

    new_ds = gdaltest.tiff_drv.CreateCopy( 'tmp/stefan_full_greyunspecified.tif', src_ds )

    new_ds = None

    os.unlink( 'tmp/stefan_full_greyunspecified.tif.aux.xml' )

    new_ds = gdal.Open( 'tmp/stefan_full_greyunspecified.tif' )
    if new_ds.GetRasterBand(2).GetRasterColorInterpretation() != gdal.GCI_Undefined:
        return 'fail'

    new_ds = None
    src_ds = None

    gdaltest.tiff_drv.Delete( 'tmp/stefan_full_greyunspecified.tif' )

    return 'success'

###############################################################################
# Read a CIELAB image to test the RGBA image TIFF interface

def tiff_write_25():

    src_ds = gdal.Open( 'data/cielab.tif' )
    if src_ds.RasterCount != 4:
        return 'fail'
    if src_ds.GetRasterBand(1).Checksum() != 6:
        return 'fail'
    if src_ds.GetRasterBand(2).Checksum() != 3:
        return 'fail'
    if src_ds.GetRasterBand(3).Checksum() != 0:
        return 'fail'
    if src_ds.GetRasterBand(4).Checksum() != 3:
        return 'fail'
    if src_ds.GetRasterBand(1).GetRasterColorInterpretation() != gdal.GCI_RedBand:
        return 'fail'
    if src_ds.GetRasterBand(2).GetRasterColorInterpretation() != gdal.GCI_GreenBand:
        return 'fail'
    if src_ds.GetRasterBand(3).GetRasterColorInterpretation() != gdal.GCI_BlueBand:
        return 'fail'
    if src_ds.GetRasterBand(4).GetRasterColorInterpretation() != gdal.GCI_AlphaBand:
        return 'fail'
    src_ds = None

    return 'success'


###############################################################################
# Test color table in a 8 bit image

def tiff_write_26():

    ds = gdaltest.tiff_drv.Create( 'tmp/ct8.tif', 1, 1, 1, gdal.GDT_Byte)

    ct = gdal.ColorTable()
    ct.SetColorEntry( 0, (255,255,255,255) )
    ct.SetColorEntry( 1, (255,255,0,255) )
    ct.SetColorEntry( 2, (255,0,255,255) )
    ct.SetColorEntry( 3, (0,255,255,255) )

    ds.GetRasterBand( 1 ).SetRasterColorTable( ct )

    ct = None
    ds = None

    ds = gdal.Open( 'tmp/ct8.tif' )

    ct = ds.GetRasterBand( 1 ).GetRasterColorTable()
    if ct.GetCount() != 256 or \
       ct.GetColorEntry(0) != (255,255,255,255) or \
       ct.GetColorEntry(1) != (255,255,0,255) or \
       ct.GetColorEntry(2) != (255,0,255,255) or \
       ct.GetColorEntry(3) != (0,255,255,255):
        gdaltest.post_reason( 'Wrong color table entry.' )
        return 'fail'

    ct = None
    ds = None

    gdaltest.tiff_drv.Delete( 'tmp/ct8.tif' )

    return 'success'

###############################################################################
# Test color table in a 16 bit image

def tiff_write_27():

    ds = gdaltest.tiff_drv.Create( 'tmp/ct16.tif', 1, 1, 1, gdal.GDT_UInt16)

    ct = gdal.ColorTable()
    ct.SetColorEntry( 0, (255,255,255,255) )
    ct.SetColorEntry( 1, (255,255,0,255) )
    ct.SetColorEntry( 2, (255,0,255,255) )
    ct.SetColorEntry( 3, (0,255,255,255) )

    ds.GetRasterBand( 1 ).SetRasterColorTable( ct )

    ct = None
    ds = None

    ds = gdal.Open( 'tmp/ct16.tif' )
    new_ds = gdaltest.tiff_drv.CreateCopy( 'tmp/ct16_copy.tif', ds )
    new_ds = None
    ds = None

    ds = gdal.Open( 'tmp/ct16_copy.tif' )

    ct = ds.GetRasterBand( 1 ).GetRasterColorTable()
    if ct.GetCount() != 65536 or \
       ct.GetColorEntry(0) != (255,255,255,255) or \
       ct.GetColorEntry(1) != (255,255,0,255) or \
       ct.GetColorEntry(2) != (255,0,255,255) or \
       ct.GetColorEntry(3) != (0,255,255,255):
        gdaltest.post_reason( 'Wrong color table entry.' )
        return 'fail'

    ct = None
    ds = None

    gdaltest.tiff_drv.Delete( 'tmp/ct16.tif' )
    gdaltest.tiff_drv.Delete( 'tmp/ct16_copy.tif' )

    return 'success'

###############################################################################
# Test SetRasterColorInterpretation on a 2 channel image

def tiff_write_28():

    ds = gdaltest.tiff_drv.Create( 'tmp/greyalpha.tif', 1, 1, 2)

    if ds.GetRasterBand(2).GetRasterColorInterpretation() != gdal.GCI_Undefined:
        return 'fail'

    ds.GetRasterBand(2).SetRasterColorInterpretation(gdal.GCI_AlphaBand)

    if ds.GetRasterBand(2).GetRasterColorInterpretation() != gdal.GCI_AlphaBand:
        return 'fail'

    ds = None

    ds = gdal.Open( 'tmp/greyalpha.tif' )

    if ds.GetRasterBand(2).GetRasterColorInterpretation() != gdal.GCI_AlphaBand:
        return 'fail'
    ds = None

    gdaltest.tiff_drv.Delete( 'tmp/greyalpha.tif' )

    return 'success'

###############################################################################
# Test SetRasterColorInterpretation on a 4 channel image

def tiff_write_29():

    # When creating a 4 channel image with PHOTOMETRIC=RGB,
    # TIFFTAG_EXTRASAMPLES=EXTRASAMPLE_UNSPECIFIED
    ds = gdaltest.tiff_drv.Create( 'tmp/rgba.tif', 1, 1, 4, options = ['PHOTOMETRIC=RGB'])

    if ds.GetRasterBand(4).GetRasterColorInterpretation() != gdal.GCI_Undefined:
        return 'fail'

    ds.GetRasterBand(4).SetRasterColorInterpretation(gdal.GCI_AlphaBand)

    if ds.GetRasterBand(4).GetRasterColorInterpretation() != gdal.GCI_AlphaBand:
        return 'fail'

    ds = None

    ds = gdal.Open( 'tmp/rgba.tif' )

    if ds.GetRasterBand(4).GetRasterColorInterpretation() != gdal.GCI_AlphaBand:
        return 'fail'
    ds = None

    gdaltest.tiff_drv.Delete( 'tmp/rgba.tif' )

    return 'success'


###############################################################################
# Create a BigTIFF image with BigTIFF=YES

def tiff_write_30():

    md = gdaltest.tiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('BigTIFF') == -1:
        return 'skip'

    ds = gdaltest.tiff_drv.Create( 'tmp/bigtiff.tif', 1, 1, 1, options = ['BigTIFF=YES'] )
    ds = None

    ds = gdal.Open( 'tmp/bigtiff.tif' )
    if ds is None:
        return 'fail'
    ds = None

    fileobj = open( 'tmp/bigtiff.tif', mode='rb')
    binvalues = array.array('b')
    binvalues.fromfile(fileobj, 4)
    fileobj.close()

    gdaltest.tiff_drv.Delete( 'tmp/bigtiff.tif' )

    # Check BigTIFF signature
    if ((binvalues[2] != 0x2B or binvalues[3] != 0) \
        and (binvalues[3] != 0x2B or binvalues[2] != 0)):
        return 'fail'

    return 'success'

###############################################################################
# Create a BigTIFF image implicitely (more than 4Gb)

def tiff_write_31():

    md = gdaltest.tiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('BigTIFF') == -1:
        return 'skip'

    ds = gdaltest.tiff_drv.Create( 'tmp/bigtiff.tif', 100000, 100000, 1,
                                   options = ['SPARSE_OK=TRUE'] )
    ds = None

    ds = gdal.Open( 'tmp/bigtiff.tif' )
    if ds is None:
        return 'fail'
    ds = None

    fileobj = open( 'tmp/bigtiff.tif', mode='rb')
    binvalues = array.array('b')
    binvalues.fromfile(fileobj, 4)
    fileobj.close()

    gdaltest.tiff_drv.Delete( 'tmp/bigtiff.tif' )

    # Check BigTIFF signature
    if ((binvalues[2] != 0x2B or binvalues[3] != 0) \
        and (binvalues[3] != 0x2B or binvalues[2] != 0)):
        return 'fail'

    return 'success'

###############################################################################
# Create a rotated image

def tiff_write_32():

    ds_in = gdal.Open('data/byte.vrt')

    # Test creation
    ds = gdaltest.tiff_drv.Create( 'tmp/byte_rotated.tif', 20, 20, gdal.GDT_Byte )

    gt = (10,3.53553390593,3.53553390593,30,3.53553390593,-3.53553390593)
    ds.SetGeoTransform( gt )

    data = ds_in.ReadRaster( 0, 0, 20, 20 )
    ds.WriteRaster( 0, 0, 20, 20, data )

    ds_in = None

    # Test copy
    new_ds = gdaltest.tiff_drv.CreateCopy( 'tmp/byte_rotated_copy.tif', ds )
    new_ds = None

    # Check copy
    ds = gdal.Open( 'tmp/byte_rotated_copy.tif' )
    new_gt = ds.GetGeoTransform()
    for i in range(6):
        if abs(new_gt[i]-gt[i]) > 1e-5:
            print('')
            print(('old = ', gt))
            print(('new = ', new_gt))
            gdaltest.post_reason( 'Geotransform differs.' )
            return 'fail'

    ds = None
    new_ds = None

    gdaltest.tiff_drv.Delete( 'tmp/byte_rotated.tif' )
    gdaltest.tiff_drv.Delete( 'tmp/byte_rotated_copy.tif' )

    return 'success'

###############################################################################
# Test that metadata is written in .aux.xml file in GeoTIFF profile with CreateCopy
# (BASELINE is tested by tiff_write_15)

def tiff_write_33():

    ds_in = gdal.Open('data/byte.vrt')

    ds = gdaltest.tiff_drv.CreateCopy( 'tmp/tw_33.tif', ds_in, options=['PROFILE=GeoTIFF'] )

    ds_in = None

    ds = None

    ds = gdal.Open( 'tmp/tw_33.tif' )

    md = ds.GetMetadata()
    if 'test' not in md:
        gdaltest.post_reason( 'Metadata absent from .aux.xml file.' )
        return 'fail'

    md = ds.GetRasterBand(1).GetMetadata()
    if 'testBand' not in md:
        gdaltest.post_reason( 'Metadata absent from .aux.xml file.' )
        return 'fail'

    ds = None

    try:
        os.remove( 'tmp/tw_33.tif.aux.xml' )
    except:
        try:
            os.stat( 'tmp/tw_33.tif.aux.xml' )
        except:
            gdaltest.post_reason( 'No .aux.xml file.' )
            return 'fail'
            pass

    ds = gdal.Open( 'tmp/tw_33.tif' )

    md = ds.GetMetadata()
    if 'test' in md:
        gdaltest.post_reason( 'Metadata written to GeoTIFF file.' )
        return 'fail'

    md = ds.GetRasterBand(1).GetMetadata()
    if 'testBand' in md:
        gdaltest.post_reason( 'Metadata written to GeoTIFF file.' )
        return 'fail'

    ds = None

    gdaltest.tiff_drv.Delete( 'tmp/tw_33.tif' )

    return 'success'

###############################################################################
# Test that metadata is written in .aux.xml file in GeoTIFF profile with Create
# (BASELINE is tested by tiff_write_16)

def tiff_write_34():

    ds = gdaltest.tiff_drv.Create( 'tmp/tw_34.tif', 1, 1, gdal.GDT_Byte,
                     options=['PROFILE=GeoTIFF'] )
    ds.SetMetadata( {'test':'testvalue'} )
    ds.GetRasterBand(1).SetMetadata( {'testBand':'testvalueBand'} )

    ds = None

    ds = gdal.Open( 'tmp/tw_34.tif' )

    md = ds.GetMetadata()
    if 'test' not in md:
        gdaltest.post_reason( 'Metadata absent from .aux.xml file.' )
        return 'fail'

    md = ds.GetRasterBand(1).GetMetadata()
    if 'testBand' not in md:
        gdaltest.post_reason( 'Metadata absent from .aux.xml file.' )
        return 'fail'

    ds = None

    try:
        os.remove( 'tmp/tw_34.tif.aux.xml' )
    except:
        try:
            os.stat( 'tmp/tw_34.tif.aux.xml' )
        except:
            gdaltest.post_reason( 'No .aux.xml file.' )
            return 'fail'
            pass

    ds = gdal.Open( 'tmp/tw_34.tif' )

    md = ds.GetMetadata()
    if 'test' in md:
        gdaltest.post_reason( 'Metadata written to GeoTIFF file.' )
        return 'fail'

    md = ds.GetRasterBand(1).GetMetadata()
    if 'testBand' in md:
        gdaltest.post_reason( 'Metadata written to GeoTIFF file.' )
        return 'fail'

    ds = None

    gdaltest.tiff_drv.Delete( 'tmp/tw_34.tif' )

    return 'success'

###############################################################################
# Test fallback from internal storage of Geotiff metadata to PAM storage
# when metadata is too big to fit into the GDALGeotiff tag

def tiff_write_35():

    # I've no idea why this works, and why this rolled in a
    # loop doesn't work... Python gurus please fix that !
    big_string = 'a'
    big_string = big_string + big_string
    big_string = big_string + big_string
    big_string = big_string + big_string
    big_string = big_string + big_string
    big_string = big_string + big_string
    big_string = big_string + big_string
    big_string = big_string + big_string
    big_string = big_string + big_string
    big_string = big_string + big_string
    big_string = big_string + big_string
    big_string = big_string + big_string
    big_string = big_string + big_string
    big_string = big_string + big_string
    big_string = big_string + big_string
    big_string = big_string + big_string

    ds = gdaltest.tiff_drv.Create( 'tmp/tw_35.tif', 1, 1, gdal.GDT_Byte )

    md = {}
    md['test'] = big_string
    ds.SetMetadata( md )

    md = ds.GetMetadata()

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = None
    gdal.PopErrorHandler()

    try:
        os.stat( 'tmp/tw_35.tif.aux.xml' )
    except:
        gdaltest.post_reason( 'No .aux.xml file.' )
        return 'fail'
        pass

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = gdal.Open( 'tmp/tw_35.tif' )
    gdal.PopErrorHandler()

    md = ds.GetMetadata()
    if 'test' not in md or len(md['test']) != 32768:
        gdaltest.post_reason( 'Did not get expected metadata.' )
        return 'fail'

    ds = None

    gdaltest.tiff_drv.Delete( 'tmp/tw_35.tif' )

    return 'success'

###############################################################################
# Generic functions for the 8 following tests

def tiff_write_big_odd_bits(vrtfilename, tmpfilename, nbits, interleaving):
    ds_in = gdal.Open(vrtfilename)

    ds = gdaltest.tiff_drv.CreateCopy( tmpfilename, ds_in, options = [ 'NBITS=' + str(nbits), 'INTERLEAVE='+ interleaving ] )

    ds_in = None

    ds = None

    ds = gdal.Open( tmpfilename )
    bnd = ds.GetRasterBand(1)
    if bnd.Checksum() != 4672:
        gdaltest.post_reason( 'Didnt get expected checksum on band 1')
        return 'fail'
    md = bnd.GetMetadata('IMAGE_STRUCTURE')
    if md['NBITS'] != str(nbits):
        gdaltest.post_reason( 'Didnt get expected NBITS value')
        return 'fail'

    bnd = ds.GetRasterBand(2)
    if bnd.Checksum() != 4672:
        gdaltest.post_reason( 'Didnt get expected checksum on band 2')
        return 'fail'
    bnd = ds.GetRasterBand(3)
    if bnd.Checksum() != 4672:
        gdaltest.post_reason( 'Didnt get expected checksum on band 3')
        return 'fail'
    bnd = None

    md = ds.GetMetadata('IMAGE_STRUCTURE');
    if md['INTERLEAVE'] != interleaving:
        gdaltest.post_reason( 'Didnt get expected interleaving')
        return 'fail'

    ds = None

    gdaltest.tiff_drv.Delete( tmpfilename )

    return 'success'


###############################################################################
# Test copy with NBITS=9, INTERLEAVE=PIXEL

def tiff_write_36():
    return tiff_write_big_odd_bits('data/uint16_3band.vrt', 'tmp/tw_36.tif', 9, 'PIXEL' )


###############################################################################
# Test copy with NBITS=9, INTERLEAVE=BAND

def tiff_write_37():
    return tiff_write_big_odd_bits('data/uint16_3band.vrt', 'tmp/tw_37.tif', 9, 'BAND' )

###############################################################################
# Test copy with NBITS=12, INTERLEAVE=PIXEL

def tiff_write_38():
    return tiff_write_big_odd_bits('data/uint16_3band.vrt', 'tmp/tw_38.tif', 12, 'PIXEL' )

###############################################################################
# Test copy with NBITS=12, INTERLEAVE=BAND

def tiff_write_39():
    return tiff_write_big_odd_bits('data/uint16_3band.vrt', 'tmp/tw_39.tif', 12, 'BAND' )

###############################################################################
# Test copy with NBITS=17, INTERLEAVE=PIXEL

def tiff_write_40():
    return tiff_write_big_odd_bits('data/uint32_3band.vrt', 'tmp/tw_40tif', 17, 'PIXEL' )

###############################################################################
# Test copy with NBITS=17, INTERLEAVE=BAND

def tiff_write_41():
    return tiff_write_big_odd_bits('data/uint32_3band.vrt', 'tmp/tw_41.tif', 17, 'BAND' )

###############################################################################
# Test copy with NBITS=24, INTERLEAVE=PIXEL

def tiff_write_42():
    return tiff_write_big_odd_bits('data/uint32_3band.vrt', 'tmp/tw_42.tif', 24, 'PIXEL' )

###############################################################################
# Test copy with NBITS=24, INTERLEAVE=BAND

def tiff_write_43():
    return tiff_write_big_odd_bits('data/uint32_3band.vrt', 'tmp/tw_43.tif', 24, 'BAND' )


###############################################################################
# Test create with NBITS=9 and preservation through CreateCopy of NBITS

def tiff_write_44():

    ds = gdaltest.tiff_drv.Create( 'tmp/tw_44.tif', 1, 1, 1, gdal.GDT_UInt16, options = [ 'NBITS=9' ] )
    ds = None
    ds = gdal.Open( 'tmp/tw_44.tif' )
    bnd = ds.GetRasterBand(1)
    md = bnd.GetMetadata('IMAGE_STRUCTURE')
    bnd = None
    if md['NBITS'] != '9':
        gdaltest.post_reason( 'Didnt get expected NBITS value')
        return 'fail'

    ds2 = gdaltest.tiff_drv.CreateCopy( 'tmp/tw_44_copy.tif', ds )
    ds2 = None

    ds2 = gdal.Open('tmp/tw_44_copy.tif')
    bnd = ds.GetRasterBand(1)
    md = bnd.GetMetadata('IMAGE_STRUCTURE')
    bnd = None
    if md['NBITS'] != '9':
        gdaltest.post_reason( 'Didnt get expected NBITS value')
        return 'fail'

    ds = None
    ds2 = None

    gdaltest.tiff_drv.Delete( 'tmp/tw_44.tif' )
    gdaltest.tiff_drv.Delete( 'tmp/tw_44_copy.tif' )

    return 'success'

###############################################################################
# Test create with NBITS=17 and preservation through CreateCopy of NBITS

def tiff_write_45():

    ds = gdaltest.tiff_drv.Create( 'tmp/tw_45.tif', 1, 1, 1, gdal.GDT_UInt32, options = [ 'NBITS=17' ] )
    ds = None
    ds = gdal.Open( 'tmp/tw_45.tif' )
    bnd = ds.GetRasterBand(1)
    md = bnd.GetMetadata('IMAGE_STRUCTURE')
    bnd = None
    if md['NBITS'] != '17':
        gdaltest.post_reason( 'Didnt get expected NBITS value')
        return 'fail'

    ds2 = gdaltest.tiff_drv.CreateCopy( 'tmp/tw_45_copy.tif', ds )
    ds2 = None

    ds2 = gdal.Open('tmp/tw_45_copy.tif')
    bnd = ds.GetRasterBand(1)
    md = bnd.GetMetadata('IMAGE_STRUCTURE')
    bnd = None
    if md['NBITS'] != '17':
        gdaltest.post_reason( 'Didnt get expected NBITS value')
        return 'fail'

    ds = None
    ds2 = None

    gdaltest.tiff_drv.Delete( 'tmp/tw_45.tif' )
    gdaltest.tiff_drv.Delete( 'tmp/tw_45_copy.tif' )

    return 'success'


###############################################################################
# Test correct round-tripping of ReadBlock/WriteBlock

def tiff_write_46():
    import struct

    oldSize = gdal.GetCacheMax()
    gdal.SetCacheMax(0)

    ds = gdaltest.tiff_drv.Create("tmp/tiff_write_46_1.tif", 10, 10, 1, options = [ 'NBITS=1' ])
    ds.GetRasterBand(1).Fill(0)

    ds2 = gdaltest.tiff_drv.Create("tmp/tiff_write_46_2.tif", 10, 10, 1, options = [ 'NBITS=1' ])
    ds2.GetRasterBand(1).Fill(1)
    ones = ds2.ReadRaster(0, 0, 10, 1)

    # Load the working block
    data = ds.ReadRaster(0, 0, 10, 1)

    # Write the working bloc
    ds.WriteRaster(0, 0, 10, 1, ones)

    # This will discard the cached block for ds
    ds3 = gdaltest.tiff_drv.Create("tmp/tiff_write_46_3.tif", 10, 10, 1)
    ds3.GetRasterBand(1).Fill(1)

    # Load the working block again
    data = ds.ReadRaster(0, 0, 10, 1)

    # We expect (1, 1, 1, 1, 1, 1, 1, 1, 1, 1)
    got = struct.unpack('B' * 10, data)
    for i in range(len(got)):
        if got[i] != 1:
            print(got)
            gdal.SetCacheMax(oldSize)
            return 'fail'

    gdal.SetCacheMax(oldSize)

    ds = None
    ds2 = None
    ds3 = None
    gdaltest.tiff_drv.Delete( 'tmp/tiff_write_46_1.tif' )
    gdaltest.tiff_drv.Delete( 'tmp/tiff_write_46_2.tif' )
    gdaltest.tiff_drv.Delete( 'tmp/tiff_write_46_3.tif' )

    return 'success'

###############################################################################
# Test #2457

def tiff_write_47():

    oldSize = gdal.GetCacheMax()
    gdal.SetCacheMax(0)

    ret = tiff_write_3()

    gdal.SetCacheMax(oldSize)
    return ret


###############################################################################
# Test #2457 with nYOff of RasterIO not aligned on the block height

def tiff_write_48():

    oldSize = gdal.GetCacheMax()
    gdal.SetCacheMax(0)

    src_ds = gdal.Open( 'data/utmsmall.tif' )
    new_ds = gdal.GetDriverByName("GTiff").Create('tmp/tiff_write_48.tif', 100, 100, 1, options = [ 'TILED=YES', 'BLOCKXSIZE=96', 'BLOCKYSIZE=96' ])
    data = src_ds.ReadRaster(0, 0, 100, 1)
    data2 = src_ds.ReadRaster(0, 1, 100, 99)
    new_ds.WriteRaster(0, 1, 100, 99, data2)
    new_ds.WriteRaster(0, 0, 100, 1, data)
    new_ds = None

    gdal.SetCacheMax(oldSize)

    new_ds = None
    new_ds = gdal.Open('tmp/tiff_write_48.tif')
    if new_ds.GetRasterBand(1).Checksum() != 50054:
        gdaltest.post_reason( 'Didnt get expected checksum ')
        return 'fail'

    new_ds = None

    gdaltest.tiff_drv.Delete( 'tmp/tiff_write_48.tif' )

    return 'success'


###############################################################################
# Test copying a CMYK TIFF into another CMYK TIFF

def tiff_write_49():

    # We open the source as RAW to get the CMYK bands
    src_ds = gdal.Open( 'GTIFF_RAW:data/rgbsmall_cmyk.tif' )

    new_ds = gdal.GetDriverByName("GTiff").CreateCopy('tmp/tiff_write_49.tif', src_ds, options = [ 'PHOTOMETRIC=CMYK' ])

    # At this point, for the purpose of the copy, the dataset will have been opened as RAW
    if new_ds.GetRasterBand(1).GetRasterColorInterpretation()!= gdal.GCI_CyanBand:
        gdaltest.post_reason( 'Wrong color interpretation.')
        print((ds.GetRasterBand(1).GetRasterColorInterpretation()))
        return 'fail'

    new_ds = None

    new_ds = gdal.Open('GTIFF_RAW:tmp/tiff_write_49.tif')

    for i in range(4):
        if new_ds.GetRasterBand(i + 1).Checksum() != src_ds.GetRasterBand(i + 1).Checksum():
            gdaltest.post_reason( 'Didnt get expected checksum ')
            return 'fail'

    src_ds = None
    new_ds = None

    gdaltest.tiff_drv.Delete( 'tmp/tiff_write_49.tif' )

    return 'success'


###############################################################################
# Test creating a CMYK TIFF from another CMYK TIFF

def tiff_write_50():

    # We open the source as RAW to get the CMYK bands
    src_ds = gdal.Open( 'GTIFF_RAW:data/rgbsmall_cmyk.tif' )

    new_ds = gdal.GetDriverByName("GTiff").Create('tmp/tiff_write_50.tif', src_ds.RasterXSize, src_ds.RasterYSize, 4, options = [ 'PHOTOMETRIC=CMYK' ])
    for i in range(4):
        data = src_ds.GetRasterBand(i+1).ReadRaster(0, 0, src_ds.RasterXSize, src_ds.RasterYSize)
        new_ds.GetRasterBand(i+1).WriteRaster(0, 0, src_ds.RasterXSize, src_ds.RasterYSize, data)

    if new_ds.GetRasterBand(1).GetRasterColorInterpretation()!= gdal.GCI_CyanBand:
        gdaltest.post_reason( 'Wrong color interpretation.')
        print((ds.GetRasterBand(1).GetRasterColorInterpretation()))
        return 'fail'

    new_ds = None

    new_ds = gdal.Open('GTIFF_RAW:tmp/tiff_write_50.tif')

    for i in range(4):
        if new_ds.GetRasterBand(i + 1).Checksum() != src_ds.GetRasterBand(i + 1).Checksum():
            gdaltest.post_reason( 'Didnt get expected checksum ')
            return 'fail'

    src_ds = None
    new_ds = None

    gdaltest.tiff_drv.Delete( 'tmp/tiff_write_50.tif' )

    return 'success'


###############################################################################
# Test proper clearing of existing GeoTIFF tags when updating the projection.
# http://trac.osgeo.org/gdal/ticket/2546

def tiff_write_51():
    shutil.copyfile( 'data/utmsmall.tif', 'tmp/tiff_write_51.tif' )

    ds = gdal.Open( 'tmp/tiff_write_51.tif', gdal.GA_Update )

    srs = osr.SpatialReference()
    srs.SetFromUserInput('EPSG:32601')
    ds.SetProjection(srs.ExportToWkt())
    ds = None

    ds = gdal.Open( 'tmp/tiff_write_51.tif' )
    wkt = ds.GetProjection()
    ds = None

    # Create a new GeoTIFF file with same projection
    ds = gdaltest.tiff_drv.Create( 'tmp/tiff_write_51_ref.tif', 1, 1, 1 )
    ds.SetProjection(srs.ExportToWkt())
    ds = None

    # Read it back as the reference WKT
    ds = gdal.Open( 'tmp/tiff_write_51_ref.tif' )
    expected_wkt = ds.GetProjection()
    ds = None

    if wkt.find('NAD') != -1 or wkt.find('North Am') != -1:
        gdaltest.post_reason( 'It appears the NAD27 datum was not properly cleared.' )
        return 'fail'

    if wkt != expected_wkt or wkt.find( 'WGS 84 / UTM zone 1N') == -1:
        print(wkt)
        print(expected_wkt)
        gdaltest.post_reason( 'coordinate system does not exactly match.' )
        return 'fail'

    ds = None

    gdaltest.tiff_drv.Delete( 'tmp/tiff_write_51.tif' )
    gdaltest.tiff_drv.Delete( 'tmp/tiff_write_51_ref.tif' )

    return 'success'

###############################################################################
# Test the ability to update a paletted TIFF files color table.

def tiff_write_52():
    shutil.copyfile( 'data/test_average_palette.tif', 'tmp/tiff_write_52.tif' )

    test_ct_data = [ (255,0,0), (0,255,0), (0,0,255), (255,255,255,0)]

    test_ct = gdal.ColorTable()
    for i in range(len(test_ct_data)):
        test_ct.SetColorEntry( i, test_ct_data[i] )

    ds = gdal.Open( 'tmp/tiff_write_52.tif', gdal.GA_Update )
    ds.GetRasterBand(1).SetRasterColorTable( test_ct )
    ds = None

    ds = gdal.Open( 'tmp/tiff_write_52.tif' )
    ct = ds.GetRasterBand(1).GetRasterColorTable()

    if ct.GetColorEntry(0) != (255,0,0,255):
        print((ct.GetColorEntry(0)))
        gdaltest.post_reason( 'Did not get expected color 0.' )
        return 'fail'

    ct = None
    ds = None

    gdaltest.tiff_drv.Delete( 'tmp/tiff_write_52.tif' )

    return 'success'

###############################################################################
# Test the ability to create a paletted image and then update later.

def tiff_write_53():
    test_ct_data = [ (255,0,0), (0,255,0), (0,0,255), (255,255,255,0)]

    test_ct = gdal.ColorTable()
    for i in range(len(test_ct_data)):
        test_ct.SetColorEntry( i, test_ct_data[i] )

    ds = gdaltest.tiff_drv.Create('tmp/tiff_write_53.tif',
                                              30, 50, 1,
                                              options=['PHOTOMETRIC=PALETTE'] )
    ds.GetRasterBand(1).Fill(10)
    ds = None

    ds = gdal.Open( 'tmp/tiff_write_53.tif', gdal.GA_Update )
    ds.GetRasterBand(1).SetRasterColorTable( test_ct )
    ds = None

    ds = gdal.Open( 'tmp/tiff_write_53.tif' )
    ct = ds.GetRasterBand(1).GetRasterColorTable()

    if ct.GetColorEntry(0) != (255,0,0,255):
        print((ct.GetColorEntry(0)))
        gdaltest.post_reason( 'Did not get expected color 0.' )
        return 'fail'

    ct = None
    ds = None

    gdaltest.tiff_drv.Delete( 'tmp/tiff_write_53.tif' )

    return 'success'


###############################################################################
# Same as before except we create an overview before reopening the file and
# adding the color table

def tiff_write_53_bis():
    test_ct_data = [ (255,0,0), (0,255,0), (0,0,255), (255,255,255,0)]

    test_ct = gdal.ColorTable()
    for i in range(len(test_ct_data)):
        test_ct.SetColorEntry( i, test_ct_data[i] )

    ds = gdaltest.tiff_drv.Create('tmp/tiff_write_53_bis.tif',
                                              30, 50, 1,
                                              options=['PHOTOMETRIC=PALETTE'] )
    ds.GetRasterBand(1).Fill(10)
    ds.BuildOverviews( 'NONE', overviewlist = [2] )
    ds = None

    ds = gdal.Open( 'tmp/tiff_write_53_bis.tif', gdal.GA_Update )
    ds.GetRasterBand(1).SetRasterColorTable( test_ct )
    ds = None

    ds = gdal.Open( 'tmp/tiff_write_53_bis.tif' )
    ct = ds.GetRasterBand(1).GetRasterColorTable()

    if ct.GetColorEntry(0) != (255,0,0,255):
        print((ct.GetColorEntry(0)))
        gdaltest.post_reason( 'Did not get expected color 0.' )
        return 'fail'

    ct = None
    ds = None

    gdaltest.tiff_drv.Delete( 'tmp/tiff_write_53_bis.tif' )

    return 'success'

###############################################################################
# Test the ability to create a JPEG compressed TIFF, with PHOTOMETRIC=YCBCR
# and write data into it without closing it and re-opening it (#2645)

def tiff_write_54():

    md = gdaltest.tiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('JPEG') == -1:
        return 'skip'

    ds = gdaltest.tiff_drv.Create('tmp/tiff_write_54.tif',
                                              256, 256, 3,
                                              options=['TILED=YES', 'COMPRESS=JPEG', 'PHOTOMETRIC=YCBCR'] )
    ds.GetRasterBand(1).Fill(255)
    ds.FlushCache()
    ds = None

    ds = gdal.Open('tmp/tiff_write_54.tif')
    cs = ds.GetRasterBand(1).Checksum()
    ds = None

    gdaltest.tiff_drv.Delete( 'tmp/tiff_write_54.tif' )

    if cs == 0:
        gdaltest.post_reason('did not get expected checksum')
        print(cs)
        return 'fail'

    return 'success'


###############################################################################
# Test creating and reading an equirectangular file with all parameters (#2706)

def tiff_write_55():

    ds = gdaltest.tiff_drv.Create('tmp/tiff_write_55.tif',
                                              256, 256, 1 )
    srs_expected = 'PROJCS["Equirectangular Mars",GEOGCS["GCS_Mars",DATUM["unknown",SPHEROID["unnamed",3394813.857975945,0]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433]],PROJECTION["Equirectangular"],PARAMETER["latitude_of_origin",-2],PARAMETER["central_meridian",184.4129943847656],PARAMETER["standard_parallel_1",-15],PARAMETER["false_easting",0],PARAMETER["false_northing",0],UNIT["metre",1,AUTHORITY["EPSG","9001"]]]'

    ds.SetProjection( srs_expected )

    ds.SetGeoTransform( (100,1,0,200,0,-1) )
    ds = None

    ds = gdal.Open( 'tmp/tiff_write_55.tif' )
    srs = ds.GetProjectionRef()
    ds = None

    if srs != srs_expected:
        print(srs)
        gdaltest.post_reason( 'failed to preserve Equirectangular projection as expected, old libgeotiff?' )
        return 'fail'

    gdaltest.tiff_drv.Delete( 'tmp/tiff_write_55.tif' )

    return 'success'

###############################################################################
# Test clearing the colormap from an existing paletted TIFF file.

def tiff_write_56():

    md = gdaltest.tiff_drv.GetMetadata()
    # Expected to fail with libtiff < 4.0 as it needs TIFFUnsetField, so skip it
    if md['DMD_CREATIONOPTIONLIST'].find('BigTIFF') == -1:
        return 'skip'

    test_ct_data = [ (255,0,0), (0,255,0), (0,0,255), (255,255,255,0)]

    test_ct = gdal.ColorTable()
    for i in range(len(test_ct_data)):
        test_ct.SetColorEntry( i, test_ct_data[i] )

    ds = gdaltest.tiff_drv.Create('tmp/tiff_write_56.tif',
                                              30, 50, 1,
                                              options=['PHOTOMETRIC=PALETTE'] )
    ds.GetRasterBand(1).Fill(10)
    ds = None

    test_ct = gdal.ColorTable()

    ds = gdal.Open( 'tmp/tiff_write_56.tif', gdal.GA_Update )
    ds.GetRasterBand(1).SetRasterColorTable( test_ct )
    ds = None

    ds = gdal.Open( 'tmp/tiff_write_56.tif' )
    ct = ds.GetRasterBand(1).GetRasterColorTable()

    if ct != None:
        gdaltest.post_reason( 'color table seemingly not cleared.' )
        return 'fail'

    ct = None
    ds = None

    gdaltest.tiff_drv.Delete( 'tmp/tiff_write_56.tif' )

    return 'success'

###############################################################################
# Test replacing normal norm up georef with rotated georef (#2625)

def tiff_write_57():

    md = gdaltest.tiff_drv.GetMetadata()
    # Expected to fail with libtiff < 4.0 as it needs TIFFUnsetField, so skip it
    if md['DMD_CREATIONOPTIONLIST'].find('BigTIFF') == -1:
        return 'skip'

    # copy a file to tmp dir to modify.
    open('tmp/tiff57.tif','wb').write(open('data/byte.tif', 'rb').read())

    # open and set a non-northup geotransform.

    ds = gdal.Open('tmp/tiff57.tif',gdal.GA_Update)
    ds.SetGeoTransform([100,1,3,200,3,1])
    ds = None

    ds = gdal.Open('tmp/tiff57.tif')
    gt = ds.GetGeoTransform()
    ds = None

    if gt != (100,1,3,200,3,1):
        print(gt)
        gdaltest.post_reason( 'did not get expected geotransform, perhaps unset is not working?' )
        return 'fail'

    gdaltest.tiff_drv.Delete('tmp/tiff57.tif')

    return 'success'

###############################################################################
# Test writing partial end strips (#2748)

def tiff_write_58():

    md = gdaltest.tiff_drv.GetMetadata()

    for compression in ('NONE', 'JPEG', 'LZW', 'DEFLATE', 'PACKBITS'):

        if md['DMD_CREATIONOPTIONLIST'].find(compression) != -1:
            ds = gdaltest.tiff_drv.Create('tmp/tiff_write_58.tif', 4, 4000, 1, options = ['COMPRESS=' + compression] )
            ds.GetRasterBand(1).Fill(255)
            ds = None

            ds = gdal.Open('tmp/tiff_write_58.tif')
            if ds.GetRasterBand(1).Checksum() != 65241:
                gdaltest.post_reason( 'wrong checksum' )
                return 'fail'
            ds = None

            gdaltest.tiff_drv.Delete('tmp/tiff_write_58.tif')
        else:
            print(('Skipping compression method %s' % compression))


    return 'success'

###############################################################################
# Test fix for #2759

def tiff_write_59():
    import struct

    ret = 'success'

    for nbands in (1,2):
        for nbits in (1,8,9,12,16,17,24,32):

            if nbits <= 8:
                gdal_type = gdal.GDT_Byte
                ctype = 'B'
            elif nbits <= 16:
                gdal_type = gdal.GDT_UInt16
                ctype = 'h'
            else:
                gdal_type = gdal.GDT_UInt32
                ctype = 'i'

            ds = gdaltest.tiff_drv.Create("tmp/tiff_write_59.tif", 10, 10, nbands, gdal_type, options = [ 'NBITS=%d' % nbits ])
            ds.GetRasterBand(1).Fill(1)

            ds = None
            ds = gdal.Open("tmp/tiff_write_59.tif", gdal.GA_Update);

            data = struct.pack(ctype * 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)
            ds.GetRasterBand(1).WriteRaster(0, 0, 10, 1, data)

            ds = None
            ds = gdal.Open("tmp/tiff_write_59.tif");

            data = ds.GetRasterBand(1).ReadRaster(0, 0, 10, 1)

            # We expect zeros
            got = struct.unpack(ctype * 10, data)
            for i in range(len(got)):
                if got[i] != 0:
                    print(('nbands=%d, NBITS=%d' % (nbands, nbits)))
                    print(got)
                    ret = 'fail'
                    break

            ds = None
            gdaltest.tiff_drv.Delete( 'tmp/tiff_write_59.tif' )

    return ret

###############################################################################
# Test fix for #2760

def tiff_write_60():

    tuples = [ ('TFW=YES', 'tmp/tiff_write_60.tfw'),
               ('WORLDFILE=YES', 'tmp/tiff_write_60.wld') ]

    for tuple in tuples:
        # Create case
        ds = gdaltest.tiff_drv.Create('tmp/tiff_write_60.tif', 10, 10, options = [ tuple[0], 'PROFILE=BASELINE' ])
        gt = (0.0, 1.0, 0.0, 50.0, 0.0, -1.0 )
        ds.SetGeoTransform(gt)
        ds = None

        ds = gdal.Open('tmp/tiff_write_60.tif')
        if ds.GetGeoTransform() != gt:
            print('case1')
            print((ds.GetGeoTransform()))
            return 'fail'

        ds = None
        gdaltest.tiff_drv.Delete( 'tmp/tiff_write_60.tif' )

        try:
            os.stat( tuple[1] )
            gdaltest.post_reason( '%s should have been deleted' % tuple[1])
            return 'fail'
        except:
            pass

        # CreateCopy case
        src_ds = gdal.Open('data/byte.tif')
        ds = gdaltest.tiff_drv.CreateCopy('tmp/tiff_write_60.tif', src_ds, options = [ tuple[0], 'PROFILE=BASELINE' ])
        gt = (0.0, 1.0, 0.0, 50.0, 0.0, -1.0 )
        ds.SetGeoTransform(gt)
        ds = None

        ds = gdal.Open('tmp/tiff_write_60.tif')
        if ds.GetGeoTransform() != gt:
            print('case2')
            print((ds.GetGeoTransform()))
            return 'fail'

        ds = None
        gdaltest.tiff_drv.Delete( 'tmp/tiff_write_60.tif' )

        try:
            os.stat( tuple[1] )
            gdaltest.post_reason( '%s should have been deleted' % tuple[1])
            return 'fail'
        except:
            pass

    return 'success'

###############################################################################
# Test BigTIFF=IF_NEEDED creation option

def tiff_write_61():

    md = gdaltest.tiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('BigTIFF') == -1:
        return 'skip'

    ds = gdaltest.tiff_drv.Create( 'tmp/bigtiff.tif', 50000, 50000, 1,
                                   options = ['BIGTIFF=IF_NEEDED', 'SPARSE_OK=TRUE'] )
    ds = None

    ds = gdal.Open( 'tmp/bigtiff.tif' )
    if ds is None:
        return 'fail'
    ds = None

    fileobj = open( 'tmp/bigtiff.tif', mode='rb')
    binvalues = array.array('b')
    binvalues.fromfile(fileobj, 4)
    fileobj.close()

    gdaltest.tiff_drv.Delete( 'tmp/bigtiff.tif' )

    # Check classical TIFF signature
    if ((binvalues[2] != 0x2A or binvalues[3] != 0) \
        and (binvalues[3] != 0x2A or binvalues[2] != 0)):
        return 'fail'

    return 'success'

###############################################################################
# Test BigTIFF=IF_SAFER creation option

def tiff_write_62():

    md = gdaltest.tiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('BigTIFF') == -1:
        return 'skip'

    ds = gdaltest.tiff_drv.Create( 'tmp/bigtiff.tif', 50000, 50000, 1,
                                   options = ['BIGTIFF=IF_SAFER', 'SPARSE_OK=TRUE'] )
    ds = None

    ds = gdal.Open( 'tmp/bigtiff.tif' )
    if ds is None:
        return 'fail'
    ds = None

    fileobj = open( 'tmp/bigtiff.tif', mode='rb')
    binvalues = array.array('b')
    binvalues.fromfile(fileobj, 4)
    fileobj.close()

    gdaltest.tiff_drv.Delete( 'tmp/bigtiff.tif' )

    # Check BigTIFF signature
    if ((binvalues[2] != 0x2B or binvalues[3] != 0) \
        and (binvalues[3] != 0x2B or binvalues[2] != 0)):
        return 'fail'

    return 'success'

###############################################################################
# Test BigTIFF=NO creation option when creating a BigTIFF file would be required

def tiff_write_63():

    md = gdaltest.tiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('BigTIFF') == -1:
        return 'skip'
    try:
        if int(gdal.VersionInfo('VERSION_NUM')) < 1700:
            return 'skip'
    except:
    # OG-python bindings don't have gdal.VersionInfo. Too bad, but let's hope that GDAL's version isn't too old !
        pass

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = gdaltest.tiff_drv.Create( 'tmp/bigtiff.tif', 150000, 150000, 1,
                                   options = ['BIGTIFF=NO'] )
    gdal.PopErrorHandler()

    if ds is None:
        return 'success'

    return 'fail'

###############################################################################
# Test returned projection in WKT format for a WGS84 GeoTIFF (#2787)

def tiff_write_64():

    ds = gdaltest.tiff_drv.Create( 'tmp/tiff_write_64.tif', 1, 1, 1 )
    srs = osr.SpatialReference()
    srs.SetFromUserInput('WGS84')
    ds.SetProjection(srs.ExportToWkt())
    ds = None

    ds = gdal.Open( 'tmp/tiff_write_64.tif' )
    wkt = ds.GetProjection()
    ds = None

    expected_wkt = """GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433],AUTHORITY["EPSG","4326"]]"""

    if wkt != expected_wkt:
        print(wkt)
        print(expected_wkt)
        gdaltest.post_reason( 'coordinate system does not exactly match.' )
        return 'fail'

    gdaltest.tiff_drv.Delete( 'tmp/tiff_write_64.tif' )

    return 'success'

###############################################################################
# Verify that we can write XML metadata.

def tiff_write_65():

    ds = gdaltest.tiff_drv.Create( 'tmp/tiff_write_65.tif', 10, 10 )

    doc = '<doc><test xml:attr="abc"/></doc>'
    ds.SetMetadata( [ doc ], 'xml:test' )

    ds = None

    ds = gdal.Open( 'tmp/tiff_write_65.tif' )
    md = ds.GetMetadata( 'xml:test' )
    ds = None

    if len(md) != 1 or md[0] != doc:
        gdaltest.post_reason( 'did not get xml back clean' )
        return 'fail'

    gdaltest.tiff_drv.Delete( 'tmp/tiff_write_65.tif' )

    return 'success'


###############################################################################
# Verify that we can write and read a band-interleaved GeoTIFF with 65535 bands (#2838)

def tiff_write_66():

    ds = gdaltest.tiff_drv.Create('tmp/tiff_write_66.tif',1,1,65535, options = ['INTERLEAVE=BAND'])
    ds = None

    ds = gdal.Open('tmp/tiff_write_66.tif')
    if ds.RasterCount != 65535:
        return 'fail'

    if ds.GetRasterBand(1).Checksum() != 0:
        return 'fail'

    if ds.GetRasterBand(65535).Checksum() != 0:
        return 'fail'

    ds = None

    gdaltest.tiff_drv.Delete( 'tmp/tiff_write_66.tif' )

    return 'success'


###############################################################################
# Verify that we can write and read a pixel-interleaved GeoTIFF with 65535 bands (#2838)

def tiff_write_67():

    ds = gdaltest.tiff_drv.Create('tmp/tiff_write_67.tif',1,1,65535, options = ['INTERLEAVE=PIXEL'])
    ds = None

    ds = gdal.Open('tmp/tiff_write_67.tif')
    if ds.RasterCount != 65535:
        return 'fail'

    if ds.GetRasterBand(1).Checksum() != 0:
        return 'fail'

    if ds.GetRasterBand(65535).Checksum() != 0:
        return 'fail'

    ds = None

    gdaltest.tiff_drv.Delete( 'tmp/tiff_write_67.tif' )

    return 'success'

###############################################################################
# Verify that we can set the color table after a Create() (scenario hit by map.tif in #2820)

def tiff_write_68():

    ds = gdaltest.tiff_drv.Create('tmp/tiff_write_68.tif',151,161,options=['COMPRESS=LZW'])
    ct = gdal.ColorTable()
    ct.SetColorEntry( 0, (255,255,255,255) )
    ct.SetColorEntry( 1, (255,255,0,255) )
    ct.SetColorEntry( 2, (255,0,255,255) )
    ct.SetColorEntry( 3, (0,255,255,255) )
    ds.GetRasterBand(1).SetRasterColorTable(ct)
    ds.GetRasterBand(1).Fill(255)
    ds = None

    ds = gdal.Open('tmp/tiff_write_68.tif')
    if ds.GetRasterBand(1).Checksum() == 0:
        return 'fail'
    ds = None

    gdaltest.tiff_drv.Delete( 'tmp/tiff_write_68.tif' )

    return 'success'

###############################################################################
# Verify GTiffRasterBand::NullBlock() when reading empty block without any nodata value set

def tiff_write_69():

    ds = gdaltest.tiff_drv.Create('tmp/tiff_write_69.tif', 32, 32, 1, gdal.GDT_Int16, options = ['SPARSE_OK=YES'] )
    ds = None

    ds = gdal.Open('tmp/tiff_write_69.tif')
    if ds.GetRasterBand(1).Checksum() != 0:
        print((ds.GetRasterBand(1).Checksum()))
        return 'fail'
    ds = None

    gdaltest.tiff_drv.Delete( 'tmp/tiff_write_69.tif' )

    return 'success'

###############################################################################
# Verify GTiffRasterBand::NullBlock() when reading empty block with nodata value set

def tiff_write_70():

    ref_ds = gdaltest.tiff_drv.Create('tmp/tiff_write_70_ref.tif', 32, 32, 1, gdal.GDT_Int16 )
    ref_ds.GetRasterBand(1).Fill(-32768)
    ref_ds = None

    ref_ds = gdal.Open('tmp/tiff_write_70_ref.tif')
    expected_cs = ref_ds.GetRasterBand(1).Checksum()
    ref_ds = None

    ds = gdaltest.tiff_drv.Create('tmp/tiff_write_70.tif', 32, 32, 1, gdal.GDT_Int16, options = ['SPARSE_OK=YES'] )
    ds.GetRasterBand(1).SetNoDataValue(-32768)
    ds = None

    ds = gdal.Open('tmp/tiff_write_70.tif')
    if ds.GetRasterBand(1).Checksum() != expected_cs:
        print((ds.GetRasterBand(1).Checksum()))
        return 'fail'
    ds = None

    gdaltest.tiff_drv.Delete( 'tmp/tiff_write_70.tif' )
    gdaltest.tiff_drv.Delete( 'tmp/tiff_write_70_ref.tif' )

    return 'success'


###############################################################################
# Test reading in a real BigTIFF file (on filesystems supporting sparse files)

def tiff_write_71():

    import struct

    md = gdaltest.tiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('BigTIFF') == -1:
        return 'skip'

    # Determine if the filesystem supports sparse files (we don't want to create a real 10 GB
    # file !
    if (gdaltest.filesystem_supports_sparse_files('tmp') == False):
        return 'skip'

    header = open('data/bigtiff_header_extract.tif', 'rb').read()

    f = open('tmp/tiff_write_71.tif', 'wb')
    f.write(header)

    from sys import version_info

    # Write StripByteCounts tag
    # 100,000 in little endian
    if version_info >= (3,0,0):
        for i in range(100000):
            exec("f.write(b'\\xa0\\x86\\x01\\x00\\x00\\x00\\x00\\x00')")
    else:
        for i in range(100000):
            f.write('\xa0\x86\x01\x00\x00\x00\x00\x00')

    # Write StripOffsets tag
    offset = 1600252
    for i in range(100000):
        f.write(struct.pack('<Q', offset))
        offset = offset + 100000

    # Write 0x78 as value of pixel (99999, 99999)
    f.seek(10001600252-1, 0)
    if version_info >= (3,0,0):
        exec("f.write(b'\\x78')")
    else:
        f.write('\x78')
    f.close()

    ds = gdal.Open('tmp/tiff_write_71.tif')
    data = ds.GetRasterBand(1).ReadRaster(99999, 99999, 1, 1)
    if struct.unpack('b', data)[0] != 0x78:
        return 'fail'
    ds = None

    gdaltest.tiff_drv.Delete( 'tmp/tiff_write_71.tif' )

    return 'success'

###############################################################################
# With CreateCopy(), check that TIFF directory is in the first bytes of the file
# and has not been rewritten later (#3021)

def tiff_write_72():

    shutil.copyfile( 'data/byte.tif', 'tmp/byte.tif' )
    ds = gdal.Open('tmp/byte.tif', gdal.GA_Update)
    dict = {}
    dict['TEST_KEY'] = 'TestValue'
    ds.SetMetadata( dict )
    ds = None

    for profile in ('GDALGeotiff', 'GEOTIFF', 'BASELINE'):
        src_ds = gdal.Open('tmp/byte.tif')
        out_ds = gdaltest.tiff_drv.CreateCopy('tmp/tiff_write_72.tif', src_ds, options = ['ENDIANNESS=LITTLE', 'PROFILE=' + profile])
        src_ds = None
        out_ds = None

        fileobj = open( 'tmp/tiff_write_72.tif', mode='rb')
        binvalues = array.array('b')
        fileobj.seek(4)
        try:
            binvalues.fromfile(fileobj, 4)
        except:
            binvalues.fromfile(fileobj, 4)
        fileobj.close()

        # Directory should be at offset 8 of the file
        if not (binvalues[0] == 0x08 and binvalues[1] == 0x00 and binvalues[2] == 0x00 and binvalues[3] == 0x00):
            gdaltest.post_reason('Failed with profile %s' % profile)
            return 'fail'

    gdaltest.tiff_drv.Delete( 'tmp/byte.tif' )
    gdaltest.tiff_drv.Delete( 'tmp/tiff_write_72.tif' )

    return 'success'

###############################################################################
# With Create(), check that TIFF directory is in the first bytes of the file
# and has not been rewritten later (#3021)

def tiff_write_73():

    out_ds = gdaltest.tiff_drv.Create('tmp/tiff_write_73.tif', 10, 10, options = ['ENDIANNESS=LITTLE'])
    out_ds.SetGeoTransform([1,0.01,0,1,0,-0.01])
    srs = osr.SpatialReference()
    srs.SetFromUserInput('EPSG:32601')
    out_ds.SetProjection(srs.ExportToWkt())
    dict = {}
    dict['TEST_KEY'] = 'TestValue'
    out_ds.SetMetadata( dict )
    out_ds.BuildOverviews('NONE', [2])
    out_ds.GetRasterBand(1).Fill(255)
    out_ds = None

    fileobj = open( 'tmp/tiff_write_73.tif', mode='rb')
    binvalues = array.array('b')
    fileobj.seek(4)
    try:
        binvalues.fromfile(fileobj, 4)
    except:
        binvalues.fromfile(fileobj, 4)
    fileobj.close()

    # Directory should be at offset 8 of the file
    if not (binvalues[0] == 0x08 and binvalues[1] == 0x00 and binvalues[2] == 0x00 and binvalues[3] == 0x00):
        return 'fail'

    # Re-open the file and modify the pixel content
    out_ds = gdal.Open('tmp/tiff_write_73.tif', gdal.GA_Update)
    out_ds.GetRasterBand(1).Fill(0)
    out_ds = None

    fileobj = open( 'tmp/tiff_write_73.tif', mode='rb')
    binvalues = array.array('b')
    fileobj.seek(4)
    try:
        binvalues.fromfile(fileobj, 4)
    except:
        binvalues.fromfile(fileobj, 4)
    fileobj.close()

    # Directory should be at offset 8 of the file
    if not (binvalues[0] == 0x08 and binvalues[1] == 0x00 and binvalues[2] == 0x00 and binvalues[3] == 0x00):
        return 'fail'

    gdaltest.tiff_drv.Delete( 'tmp/tiff_write_73.tif' )

    return 'success'

###############################################################################
# Verify we can write 12bit jpeg encoded tiff.

def tiff_write_74():

    md = gdaltest.tiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('JPEG') == -1:
        return 'skip'

    old_accum = gdal.GetConfigOption( 'CPL_ACCUM_ERROR_MSG', 'OFF' )
    gdal.SetConfigOption( 'CPL_ACCUM_ERROR_MSG', 'ON' )
    gdal.ErrorReset()
    gdal.PushErrorHandler( 'CPLQuietErrorHandler' )

    try:
        ds = gdal.Open('data/mandrilmini_12bitjpeg.tif')
        ds.GetRasterBand(1).ReadRaster(0,0,1,1)
    except:
        ds = None

    gdal.PopErrorHandler()
    gdal.SetConfigOption( 'CPL_ACCUM_ERROR_MSG', old_accum )

    if gdal.GetLastErrorMsg().find(
                   'Unsupported JPEG data precision 12') != -1:
        sys.stdout.write('(12bit jpeg not available) ... ')
        return 'skip'

    for photometric in ('YCBCR', 'RGB') :

        drv = gdal.GetDriverByName('GTiff')
        dst_ds = drv.CreateCopy( 'tmp/test_74.tif', ds,
                                options = ['COMPRESS=JPEG', 'NBITS=12',
                                            'JPEG_QUALITY=95',
                                            'PHOTOMETRIC=' + photometric] )
        dst_ds = None

        dst_ds = gdal.Open( 'tmp/test_74.tif' )
        stats = dst_ds.GetRasterBand(1).GetStatistics( 0, 1 )

        if stats[2] < 2150 or stats[2] > 2180:
            gdaltest.post_reason( 'did not get expected mean for band1.')
            print(stats)
            print(photometric)
            return 'fail'

        try:
            compression = dst_ds.GetMetadataItem('COMPRESSION','IMAGE_STRUCTURE')
        except:
            md = dst_ds.GetMetadata('IMAGE_STRUCTURE')
            compression = md['COMPRESSION']

        if (photometric == 'YCBCR' and compression != 'YCbCr JPEG') or \
           (photometric == 'RGB' and compression != 'JPEG'):
            gdaltest.post_reason( 'did not get expected COMPRESSION value' )
            print(('COMPRESSION="%s"' % compression))
            print(photometric)
            return 'fail'

        try:
            nbits = dst_ds.GetRasterBand(3).GetMetadataItem('NBITS','IMAGE_STRUCTURE')
        except:
            md = dst_ds.GetRasterBand(3).GetMetadata('IMAGE_STRUCTURE')
            nbits = md['NBITS']

        if nbits != '12':
            gdaltest.post_reason( 'did not get expected NBITS value' )
            print(photometric)
            return 'fail'

        dst_ds = None

        gdaltest.tiff_drv.Delete( 'tmp/test_74.tif' )

    return 'success'

###############################################################################
# Verify that FlushCache() alone doesn't cause crash (#3067 )

def tiff_write_75():

    ds = gdaltest.tiff_drv.Create('tmp/tiff_write_75.tif', 1, 1, 1)
    ds.FlushCache()
    ds = None

    gdaltest.tiff_drv.Delete( 'tmp/tiff_write_75.tif' )

    return 'success'

###############################################################################
# Test generating a G4 band to use the TIFFWriteScanline()

def tiff_write_76():

    src_ds = gdal.Open('data/slim_g4.tif')
    compression = src_ds.GetMetadata('IMAGE_STRUCTURE')['COMPRESSION']
    new_ds = gdaltest.tiff_drv.CreateCopy( 'tmp/tiff_write_76.tif', src_ds, options = ['BLOCKYSIZE=%d' % src_ds.RasterYSize, 'COMPRESS=' + compression ] )
    new_ds = None
    new_ds = gdal.Open( 'tmp/tiff_write_76.tif' )

    cs = new_ds.GetRasterBand(1).Checksum()
    if cs != 3322:
        print(cs)
        gdaltest.post_reason( 'Got wrong checksum' )
        return 'fail'

    src_ds = None
    new_ds = None

    gdaltest.tiff_drv.Delete( 'tmp/tiff_write_76.tif' )

    return 'success'

###############################################################################
# Test generating & reading a 8bit all-in-one-strip multiband TIFF (#3904)

def tiff_write_77():

    src_ds = gdaltest.tiff_drv.Create( 'tmp/tiff_write_77_src.tif', 1, 5000, 3 )
    src_ds.GetRasterBand(2).Fill(255)

    for interleaving in ('PIXEL', 'BAND'):
        new_ds = gdaltest.tiff_drv.CreateCopy( 'tmp/tiff_write_77.tif', src_ds,
                                                options = ['BLOCKYSIZE=%d' % src_ds.RasterYSize,
                                                           'COMPRESS=LZW',
                                                           'INTERLEAVE=' + interleaving] )

        for attempt in range(2):

            # Test reading a few samples to check that random reading works
            band_lines = [ (1,0), (1,5), (1,3), (2,10), (1,100), (2,1000), (2,500),
                            (1,500), (2,500), (2,4999), (2,4999), (3,4999), (1,4999) ]
            for band_line in band_lines:
                cs = new_ds.GetRasterBand(band_line[0]).Checksum(0,band_line[1],1,1)
                if band_line[0] == 2:
                    expected_cs = 255 % 7;
                else:
                    expected_cs = 0 % 7;
                if cs != expected_cs:
                    print(cs)
                    gdaltest.post_reason( 'Got wrong checksum' )
                    return 'fail'

            # Test whole bands
            for i in range(3):
                cs = new_ds.GetRasterBand(i+1).Checksum()
                expected_cs = src_ds.GetRasterBand(i+1).Checksum()
                if cs != expected_cs:
                    print(cs)
                    gdaltest.post_reason( 'Got wrong checksum' )
                    return 'fail'

            if attempt == 0:
                new_ds = None
                new_ds = gdal.Open( 'tmp/tiff_write_77.tif' )

        new_ds = None

        gdaltest.tiff_drv.Delete( 'tmp/tiff_write_77.tif' )

    src_ds = None
    gdaltest.tiff_drv.Delete( 'tmp/tiff_write_77_src.tif' )

    return 'success'

###############################################################################
# Test generating & reading a YCbCr JPEG all-in-one-strip multiband TIFF (#3259)

def tiff_write_78():

    md = gdaltest.tiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('JPEG') == -1:
        return 'skip'

    src_ds = gdaltest.tiff_drv.Create( 'tmp/tiff_write_78_src.tif', 16, 2048, 3 )
    src_ds.GetRasterBand(2).Fill(255)

    new_ds = gdaltest.tiff_drv.CreateCopy( 'tmp/tiff_write_78.tif', src_ds,
                                            options = ['BLOCKYSIZE=%d' % src_ds.RasterYSize,
                                                       'COMPRESS=JPEG',
                                                       'PHOTOMETRIC=YCBCR'] )

    # Make sure the file is flushed so that we re-read from it rather from cached blocks
    new_ds.FlushCache()
    #new_ds = None
    #new_ds = gdal.Open('tmp/tiff_write_78.tif')

    if 'GetBlockSize' in dir(gdal.Band):
        (blockx, blocky) = new_ds.GetRasterBand(1).GetBlockSize()
        if blocky != 1:
            print('')
            print('using regular band (libtiff <= 3.9.2 or <= 4.0.0beta5, or SplitBand disabled by config option)')

    # Test reading a few samples to check that random reading works
    band_lines = [ (1,0), (1,5), (1,3), (2,10), (1,100), (2,1000), (2,500),
                    (1,500), (2,500), (2,2047), (2,2047), (3,2047), (1,2047) ]
    for band_line in band_lines:
        cs = new_ds.GetRasterBand(band_line[0]).Checksum(0,band_line[1],1,1)
        if band_line[0] == 1:
            expected_cs = 0 % 7;
        elif band_line[0] == 2:
            expected_cs = 255 % 7;
        else:
            # We should expect 0, but due to JPEG YCbCr compression & decompression,
            # this ends up being 1
            expected_cs = 1 % 7;
        if cs != expected_cs:
            print(cs)
            print(expected_cs)
            print(band_line)
            gdaltest.post_reason( 'Got wrong checksum' )
            return 'fail'

    # Test whole bands
    for i in range(3):
        cs = new_ds.GetRasterBand(i+1).Checksum()
        expected_cs = src_ds.GetRasterBand(i+1).Checksum()
        if i == 2:
            # We should expect 0, but due to JPEG YCbCr compression & decompression,
            # this ends up being 32768
            expected_cs = 32768
        if cs != expected_cs:
            print(cs)
            gdaltest.post_reason( 'Got wrong checksum' )
            return 'fail'

    new_ds = None
    gdaltest.tiff_drv.Delete( 'tmp/tiff_write_78.tif' )

    src_ds = None
    gdaltest.tiff_drv.Delete( 'tmp/tiff_write_78_src.tif' )

    return 'success'

###############################################################################
# Test reading & updating GDALMD_AREA_OR_POINT (#3522)

def tiff_write_79():

    ds = gdaltest.tiff_drv.Create( 'tmp/tiff_write_79.tif', 1, 1)
    srs = osr.SpatialReference()
    srs.SetFromUserInput('EPSG:32601')
    ds.SetProjection(srs.ExportToWkt())
    ds = None

    for do_projection_ref in [False, True]:
        for check_just_after in [False, True]:

            ds = gdal.Open( 'tmp/tiff_write_79.tif')
            if do_projection_ref:
                ds.GetProjectionRef()
            mdi = ds.GetMetadataItem('AREA_OR_POINT')
            if mdi != 'Area':
                gdaltest.post_reason('(1) did not get expected value. do_projection_ref = %d, check_just_after = %d' % (do_projection_ref, check_just_after))
                print(mdi)
                return 'fail'
            ds = None

            # Still read-only.
            ds = gdal.Open( 'tmp/tiff_write_79.tif')
            if do_projection_ref:
                ds.GetProjectionRef()
            ds.SetMetadataItem('AREA_OR_POINT', 'Point')
            ds = None
            try:
                # check that it doesn't go to PAM
                os.stat('tmp/tiff_write_79.tif.aux.xml')
                gdaltest.post_reason('got to PAM')
                return 'fail'
            except:
                pass

            # So should get 'Area'
            ds = gdal.Open( 'tmp/tiff_write_79.tif')
            if do_projection_ref:
                ds.GetProjectionRef()
            mdi = ds.GetMetadataItem('AREA_OR_POINT')
            if mdi != 'Area':
                gdaltest.post_reason('(2) did not get expected value. do_projection_ref = %d, check_just_after = %d' % (do_projection_ref, check_just_after))
                print(mdi)
                return 'fail'
            ds = None

            # Now update to 'Point'
            ds = gdal.Open( 'tmp/tiff_write_79.tif', gdal.GA_Update)
            if do_projection_ref:
                ds.GetProjectionRef()
            ds.SetMetadataItem('AREA_OR_POINT', 'Point')
            if check_just_after:
                mdi = ds.GetMetadataItem('AREA_OR_POINT')
                if mdi != 'Point':
                    gdaltest.post_reason('(3) did not get expected value. do_projection_ref = %d, check_just_after = %d' % (do_projection_ref, check_just_after))
                    print(mdi)
                    return 'fail'
            ds = None
            try:
                # check that it doesn't go to PAM
                os.stat('tmp/tiff_write_79.tif.aux.xml')
                gdaltest.post_reason('got to PAM')
                return 'fail'
            except:
                pass

            # Now should get 'Point'
            ds = gdal.Open( 'tmp/tiff_write_79.tif')
            if do_projection_ref:
                ds.GetProjectionRef()
            mdi = ds.GetMetadataItem('AREA_OR_POINT')
            if mdi != 'Point':
                gdaltest.post_reason('(4) did not get expected value. do_projection_ref = %d, check_just_after = %d' % (do_projection_ref, check_just_after))
                print(mdi)
                return 'fail'
            ds = None

            # Now update back to 'Area' through SetMetadata()
            ds = gdal.Open( 'tmp/tiff_write_79.tif', gdal.GA_Update)
            if do_projection_ref:
                ds.GetProjectionRef()
            md = {}
            md['AREA_OR_POINT'] = 'Area'
            ds.SetMetadata(md)
            if check_just_after:
                mdi = ds.GetMetadataItem('AREA_OR_POINT')
                if mdi != 'Area':
                    gdaltest.post_reason('(5) did not get expected value. do_projection_ref = %d, check_just_after = %d' % (do_projection_ref, check_just_after))
                    print(mdi)
                    return 'fail'
            ds = None

            # Now should get 'Area'
            ds = gdal.Open( 'tmp/tiff_write_79.tif')
            if do_projection_ref:
                ds.GetProjectionRef()
            mdi = ds.GetMetadataItem('AREA_OR_POINT')
            if mdi != 'Area':
                gdaltest.post_reason('(6) did not get expected value')
                print(mdi)
                return 'fail'
            ds = None

    gdaltest.tiff_drv.Delete( 'tmp/tiff_write_79.tif' )

    return 'success'

###############################################################################
# Test SetOffset() & SetScale()

def tiff_write_80():

    # First part : test storing and retrieving scale & offsets from internal metadata
    ds = gdaltest.tiff_drv.Create( 'tmp/tiff_write_80.tif', 1, 1)
    ds.GetRasterBand(1).SetScale(100)
    ds.GetRasterBand(1).SetOffset(1000)
    ds = None

    try:
        # check that it doesn't go to PAM
        os.stat('tmp/tiff_write_80.tif.aux.xml')
        gdaltest.post_reason('got to PAM, but not expected...')
        return 'fail'
    except:
        pass

    ds = gdal.Open('tmp/tiff_write_80.tif')
    scale = ds.GetRasterBand(1).GetScale()
    offset = ds.GetRasterBand(1).GetOffset()
    if scale != 100 or offset != 1000:
        gdaltest.post_reason('did not get expected values in internal case (1)')
        print(scale)
        print(offset)
        return 'fail'
    ds = None

    # Test CreateCopy()
    src_ds = gdal.Open('tmp/tiff_write_80.tif')
    ds = gdaltest.tiff_drv.CreateCopy( 'tmp/tiff_write_80_copy.tif', src_ds )
    src_ds = None
    ds = None
    ds = gdal.Open( 'tmp/tiff_write_80_copy.tif')
    scale = ds.GetRasterBand(1).GetScale()
    offset = ds.GetRasterBand(1).GetOffset()
    if scale != 100 or offset != 1000:
        gdaltest.post_reason('did not get expected values in copy')
        print(scale)
        print(offset)
        return 'fail'
    ds = None
    gdaltest.tiff_drv.Delete( 'tmp/tiff_write_80_copy.tif' )

    # Second part : test unsetting scale & offsets from internal metadata
    ds = gdal.Open('tmp/tiff_write_80.tif', gdal.GA_Update)
    ds.GetRasterBand(1).SetScale(1)
    ds.GetRasterBand(1).SetOffset(0)
    ds = None

    ds = gdal.Open('tmp/tiff_write_80.tif')
    scale = ds.GetRasterBand(1).GetScale()
    offset = ds.GetRasterBand(1).GetOffset()
    if scale != 1 or offset != 0:
        gdaltest.post_reason('did not get expected values in internal case (2)')
        print(scale)
        print(offset)
        return 'fail'
    ds = None

    gdaltest.tiff_drv.Delete( 'tmp/tiff_write_80.tif' )


    # Third part : test storing and retrieving scale & offsets from PAM metadata
    ds = gdaltest.tiff_drv.Create( 'tmp/tiff_write_80_bis.tif', 1, 1)
    if ds.GetRasterBand(1).GetScale() != None or ds.GetRasterBand(1).GetOffset() != None:
        gdaltest.post_reason('expected None values')
        return 'fail'
    ds = None

    ds = gdal.Open('tmp/tiff_write_80_bis.tif')
    ds.GetRasterBand(1).SetScale(-100)
    ds.GetRasterBand(1).SetOffset(-1000)
    ds = None

    try:
        # check that it *goes* to PAM
        os.stat('tmp/tiff_write_80_bis.tif.aux.xml')
    except:
        gdaltest.post_reason('did not go to PAM as expected')
        return 'fail'

    ds = gdal.Open('tmp/tiff_write_80_bis.tif')
    scale = ds.GetRasterBand(1).GetScale()
    offset = ds.GetRasterBand(1).GetOffset()
    if scale != -100 or offset != -1000:
        gdaltest.post_reason('did not get expected values in PAM case (1)')
        print(scale)
        print(offset)
        return 'fail'
    ds = None


    # Fourth part : test unsetting scale & offsets from PAM metadata
    ds = gdal.Open('tmp/tiff_write_80_bis.tif')
    ds.GetRasterBand(1).SetScale(1)
    ds.GetRasterBand(1).SetOffset(0)
    ds = None

    try:
        # check that there is no more any PAM file
        os.stat('tmp/tiff_write_80_bis.tif.aux.xml')
        gdaltest.post_reason('PAM file should be deleted')
        return 'fail'
    except:
        pass

    ds = gdal.Open('tmp/tiff_write_80_bis.tif')
    scale = ds.GetRasterBand(1).GetScale()
    offset = ds.GetRasterBand(1).GetOffset()
    if scale != 1 or offset != 0:
        gdaltest.post_reason('did not get expected values in PAM case (2)')
        print(scale)
        print(offset)
        return 'fail'
    ds = None

    gdaltest.tiff_drv.Delete( 'tmp/tiff_write_80_bis.tif' )

    return 'success'

###############################################################################
# Test retrieving GCP from PAM

def tiff_write_81():

    shutil.copyfile('data/byte.tif', 'tmp/tiff_write_81.tif')
    f = open('tmp/tiff_write_81.tif.aux.xml', 'wt')
    f.write("""
<PAMDataset>
  <GCPList Projection="PROJCS[&quot;NAD27 / UTM zone 11N&quot;,GEOGCS[&quot;NAD27&quot;,DATUM[&quot;North_American_Datum_1927&quot;,SPHEROID[&quot;Clarke 1866&quot;,6378206.4,294.9786982139006,AUTHORITY[&quot;EPSG&quot;,&quot;7008&quot;]],AUTHORITY[&quot;EPSG&quot;,&quot;6267&quot;]],PRIMEM[&quot;Greenwich&quot;,0],UNIT[&quot;degree&quot;,0.0174532925199433],AUTHORITY[&quot;EPSG&quot;,&quot;4267&quot;]],PROJECTION[&quot;Transverse_Mercator&quot;],PARAMETER[&quot;latitude_of_origin&quot;,0],PARAMETER[&quot;central_meridian&quot;,-117],PARAMETER[&quot;scale_factor&quot;,0.9996],PARAMETER[&quot;false_easting&quot;,500000],PARAMETER[&quot;false_northing&quot;,0],UNIT[&quot;metre&quot;,1,AUTHORITY[&quot;EPSG&quot;,&quot;9001&quot;]],AUTHORITY[&quot;EPSG&quot;,&quot;26711&quot;]]">
    <GCP Id="" Pixel="0.0000" Line="0.0000" X="4.407200000000E+05" Y="3.751320000000E+06"/>
    <GCP Id="" Pixel="100.0000" Line="0.0000" X="4.467200000000E+05" Y="3.751320000000E+06"/>
    <GCP Id="" Pixel="0.0000" Line="100.0000" X="4.407200000000E+05" Y="3.745320000000E+06"/>
    <GCP Id="" Pixel="100.0000" Line="100.0000" X="4.467200000000E+05" Y="3.745320000000E+06"/>
  </GCPList>
</PAMDataset>""")
    f.close()

    ds = gdal.Open('tmp/tiff_write_81.tif')

    if ds.GetGCPProjection().find(
                   'AUTHORITY["EPSG","26711"]') == -1:
        gdaltest.post_reason( 'GCP Projection not set properly.' )
        return 'fail'

    gcps = ds.GetGCPs()
    if len(gcps) != 4:
        gdaltest.post_reason( 'GCP count wrong.' )
        return 'fail'

    ds = None

    gdaltest.tiff_drv.Delete( 'tmp/tiff_write_81.tif' )

    return 'success'

###############################################################################
# Test writing & reading a signedbyte 8 bit geotiff

def tiff_write_82():

    src_ds = gdal.Open('data/byte.tif')
    ds = gdaltest.tiff_drv.CreateCopy('tmp/tiff_write_82.tif', src_ds, options = ['PIXELTYPE=SIGNEDBYTE'])
    src_ds = None
    ds = None

    ds = gdal.Open('tmp/tiff_write_82.tif')
    md = ds.GetRasterBand(1).GetMetadata('IMAGE_STRUCTURE')
    if md['PIXELTYPE'] != 'SIGNEDBYTE':
        gdaltest.post_reason('did not get SIGNEDBYTE')
        return 'fail'
    ds = None

    gdaltest.tiff_drv.Delete( 'tmp/tiff_write_82.tif' )

    return 'success'


###############################################################################
# Test writing & reading an indexed GeoTIFF with an extra transparency band (#3547)

def tiff_write_83():

    # Test Create() method
    ds = gdaltest.tiff_drv.Create('tmp/tiff_write_83.tif',1,1,2)
    ct = gdal.ColorTable()
    ct.SetColorEntry( 127, (255,255,255,255) )
    ds.GetRasterBand( 1 ).SetRasterColorTable( ct )
    ds.GetRasterBand( 1 ).Fill(127)
    ds.GetRasterBand( 2 ).Fill(255)
    ds = None

    # Test CreateCopy() method
    src_ds = gdal.Open('tmp/tiff_write_83.tif')
    ds = gdaltest.tiff_drv.CreateCopy('tmp/tiff_write_83_2.tif', src_ds)
    src_ds = None
    ds = None

    ds = gdal.Open('tmp/tiff_write_83_2.tif')
    ct2 = ds.GetRasterBand(1).GetRasterColorTable()
    if ct2.GetColorEntry(127) != (255,255,255,255):
        gdaltest.post_reason('did not get expected color table')
        return 'fail'
    ct2 = None
    cs1 = ds.GetRasterBand(1).Checksum()
    if cs1 != 127 % 7:
        gdaltest.post_reason('did not get expected checksum for band 1')
        return 'fail'
    cs2 = ds.GetRasterBand(2).Checksum()
    if cs2 != 255 % 7:
        gdaltest.post_reason('did not get expected checksum for band 2')
        return 'fail'
    ds = None

    gdaltest.tiff_drv.Delete( 'tmp/tiff_write_83.tif' )
    gdaltest.tiff_drv.Delete( 'tmp/tiff_write_83_2.tif' )

    return 'success'

###############################################################################
# Test propagation of non-standard JPEG quality when the current directory
# changes in the midst of encoding of tiles (#3539)

def tiff_write_84():

    md = gdaltest.tiff_drv.GetMetadata()

    # Crashes with libtiff < 4.0
    if md['DMD_CREATIONOPTIONLIST'].find('BigTIFF') == -1:
        return 'skip'

    if md['DMD_CREATIONOPTIONLIST'].find('JPEG') == -1:
        return 'skip'

    oldSize = gdal.GetCacheMax()
    gdal.SetCacheMax(0)

    ds = gdal.GetDriverByName('GTiff').Create('tmp/tiff_write_84.tif', 128, 128, 3)
    ds = None

    try:
        os.remove('tmp/tiff_write_84.tif.ovr')
    except:
        pass

    ds = gdal.Open('tmp/tiff_write_84.tif')
    gdal.SetConfigOption('COMPRESS_OVERVIEW','JPEG')
    gdal.SetConfigOption('JPEG_QUALITY_OVERVIEW','90')
    ds.BuildOverviews('NEAREST', overviewlist = [2])
    cs = ds.GetRasterBand(2).GetOverview(0).Checksum()
    ds = None
    gdal.SetConfigOption('COMPRESS_OVERVIEW',None)
    gdal.SetConfigOption('JPEG_QUALITY_OVERVIEW',None)

    gdal.SetCacheMax(oldSize)

    gdaltest.tiff_drv.Delete( 'tmp/tiff_write_84.tif' )

    if cs != 0:
        print(cs)
        gdaltest.post_reason('did not get expected checksum')
        return 'fail'

    return 'success'

###############################################################################
# Test SetUnitType()

def tiff_write_85():

    # First part : test storing and retrieving unittype from internal metadata
    ds = gdaltest.tiff_drv.Create( 'tmp/tiff_write_85.tif', 1, 1)
    ds.GetRasterBand(1).SetUnitType('ft')
    ds = None

    try:
        # check that it doesn't go to PAM
        os.stat('tmp/tiff_write_85.tif.aux.xml')
        gdaltest.post_reason('got to PAM, but not expected...')
        return 'fail'
    except:
        pass

    ds = gdal.Open('tmp/tiff_write_85.tif')
    unittype = ds.GetRasterBand(1).GetUnitType()
    if unittype != 'ft':
        gdaltest.post_reason('did not get expected values in internal case (1)')
        print(unittype)
        return 'fail'
    ds = None

    # Test CreateCopy()
    src_ds = gdal.Open('tmp/tiff_write_85.tif')
    ds = gdaltest.tiff_drv.CreateCopy( 'tmp/tiff_write_85_copy.tif', src_ds )
    src_ds = None
    ds = None
    ds = gdal.Open( 'tmp/tiff_write_85_copy.tif')
    unittype = ds.GetRasterBand(1).GetUnitType()
    if unittype != 'ft':
        gdaltest.post_reason('did not get expected values in copy')
        print(unittype)
        return 'fail'
    ds = None
    gdaltest.tiff_drv.Delete( 'tmp/tiff_write_85_copy.tif' )

    # Second part : test unsetting unittype from internal metadata
    ds = gdal.Open('tmp/tiff_write_85.tif', gdal.GA_Update)
    ds.GetRasterBand(1).SetUnitType(None)
    ds = None

    ds = gdal.Open('tmp/tiff_write_85.tif')
    unittype = ds.GetRasterBand(1).GetUnitType()
    if unittype != '':
        gdaltest.post_reason('did not get expected values in internal case (2)')
        print(unittype)
        return 'fail'
    ds = None

    gdaltest.tiff_drv.Delete( 'tmp/tiff_write_85.tif' )


    # Third part : test storing and retrieving unittype from PAM metadata
    ds = gdaltest.tiff_drv.Create( 'tmp/tiff_write_85_bis.tif', 1, 1)
    if len(ds.GetRasterBand(1).GetUnitType()) != 0:
        gdaltest.post_reason('expected None values')
        return 'fail'
    ds = None

    ds = gdal.Open('tmp/tiff_write_85_bis.tif')
    ds.GetRasterBand(1).SetUnitType('ft')
    ds = None

    try:
        # check that it *goes* to PAM
        os.stat('tmp/tiff_write_85_bis.tif.aux.xml')
    except:
        gdaltest.post_reason('did not go to PAM as expected')
        return 'fail'

    ds = gdal.Open('tmp/tiff_write_85_bis.tif')
    unittype = ds.GetRasterBand(1).GetUnitType()
    if unittype != 'ft':
        gdaltest.post_reason('did not get expected values in PAM case (1)')
        print(unittype)
        return 'fail'
    ds = None

    # Fourth part : test unsetting unittype from PAM metadata
    ds = gdal.Open('tmp/tiff_write_85_bis.tif')
    ds.GetRasterBand(1).SetUnitType(None)
    ds = None

    try:
        # check that there is no more any PAM file
        os.stat('tmp/tiff_write_85_bis.tif.aux.xml')
        gdaltest.post_reason('PAM file should be deleted')
        return 'fail'
    except:
        pass

    ds = gdal.Open('tmp/tiff_write_85_bis.tif')
    unittype = ds.GetRasterBand(1).GetUnitType()
    if unittype != '':
        gdaltest.post_reason('did not get expected values in PAM case (2)')
        print(unittype)
        return 'fail'
    ds = None

    gdaltest.tiff_drv.Delete( 'tmp/tiff_write_85_bis.tif' )

    return 'success'

###############################################################################
# Test special handling of xml:ESRI domain.  When the ESRI_XML_PAM config
# option is set we want to write this to PAM, not into the geotiff itself.
# This is a special option so that ArcGIS 10 written geotiffs will still work
# properly with earlier versions of ArcGIS, requested by ESRI.

def tiff_write_86():

    gdal.SetConfigOption( 'ESRI_XML_PAM', 'YES' )

    ds = gdaltest.tiff_drv.Create( 'tmp/tiff_write_86.tif', 100, 100,
                                   1, gdal.GDT_Byte )
    ds.SetMetadata( ['<abc></abc>'], 'xml:ESRI' )
    ds.SetMetadataItem( 'BaseTest', 'Value' )
    ds = None

    # Is the xml:ESRI data available?
    ds = gdal.Open( 'tmp/tiff_write_86.tif' )
    if ds.GetMetadata( 'xml:ESRI' ) != [ '<abc />\n' ]:
        print(ds.GetMetadata( 'xml:ESRI' ))
        gdaltest.post_reason( 'did not get expected xml:ESRI metadata.' )
        return 'fail'

    if ds.GetMetadataItem('BaseTest') != 'Value':
        gdaltest.post_value( 'missing metadata(1)' )
        return 'fail'
    ds = None

    # After removing the pam file is it gone, but the conventional
    # metadata still available?

    os.rename( 'tmp/tiff_write_86.tif.aux.xml',
               'tmp/tiff_write_86.tif.aux.xml.hidden' )

    ds = gdal.Open( 'tmp/tiff_write_86.tif' )
    if ds.GetMetadata( 'xml:ESRI' ) != None:
        print(ds.GetMetadata( 'xml:ESRI' ))
        gdaltest.post_reason( 'unexpectedly got xml:ESRI metadata' )
        return 'fail'

    if ds.GetMetadataItem('BaseTest') != 'Value':
        gdaltest.post_value( 'missing metadata(2)' )
        return 'fail'

    ds = None

    # now confirm that CreateCopy also preserves things similarly.

    os.rename( 'tmp/tiff_write_86.tif.aux.xml.hidden',
               'tmp/tiff_write_86.tif.aux.xml' )

    ds_src = gdal.Open( 'tmp/tiff_write_86.tif' )
    ds = gdaltest.tiff_drv.CreateCopy( 'tmp/tiff_write_86_cc.tif', ds_src )
    ds_src = None
    ds = None

    # Is the xml:ESRI data available?
    ds = gdal.Open( 'tmp/tiff_write_86_cc.tif' )
    if ds.GetMetadata( 'xml:ESRI' ) != [ '<abc />\n' ]:
        print(ds.GetMetadata( 'xml:ESRI' ))
        gdaltest.post_reason( 'did not get expected xml:ESRI metadata (cc).' )
        return 'fail'

    if ds.GetMetadataItem('BaseTest') != 'Value':
        gdaltest.post_value( 'missing metadata(1cc)' )
        return 'fail'
    ds = None

    # After removing the pam file is it gone, but the conventional
    # metadata still available?

    os.remove( 'tmp/tiff_write_86_cc.tif.aux.xml' )

    ds = gdal.Open( 'tmp/tiff_write_86_cc.tif' )
    if ds.GetMetadata( 'xml:ESRI' ) != None:
        print(ds.GetMetadata( 'xml:ESRI' ))
        gdaltest.post_reason( 'unexpectedly got xml:ESRI metadata(2)' )
        return 'fail'

    if ds.GetMetadataItem('BaseTest') != 'Value':
        gdaltest.post_value( 'missing metadata(2cc)' )
        return 'fail'

    ds = None

    # Cleanup

    gdal.SetConfigOption( 'ESRI_XML_PAM', 'NO' )

    gdaltest.tiff_drv.Delete( 'tmp/tiff_write_86.tif' )
    gdaltest.tiff_drv.Delete( 'tmp/tiff_write_86_cc.tif' )

    return 'success'


###############################################################################
# Test COPY_SRC_OVERVIEWS creation option

def tiff_write_87():

    shutil.copy('data/utmsmall.tif', 'tmp/tiff_write_87_src.tif')

    src_ds = gdal.Open('tmp/tiff_write_87_src.tif', gdal.GA_Update)
    src_ds.BuildOverviews( 'NEAR', overviewlist = [2, 4] )
    ds = gdaltest.tiff_drv.CreateCopy('tmp/tiff_write_87_dst.tif', src_ds, options = ['COPY_SRC_OVERVIEWS=YES', 'ENDIANNESS=LITTLE'])
    ds = None
    src_ds = None

    ds = gdal.Open('tmp/tiff_write_87_dst.tif')
    cs1 = ds.GetRasterBand(1).GetOverview(0).Checksum()
    cs2 = ds.GetRasterBand(1).GetOverview(1).Checksum()
    ds = None

    # We should also check that the IFDs are at the beginning of the file,
    # that the smallest overview data is before the larger one which is
    # before the full res data... but not possible to do that with GDAL API
    # We'll just check the first IFD (whose position is at offset 4) is at offset 8
    f = open('tmp/tiff_write_87_dst.tif', 'rb')
    data = f.read(8)
    f.close()

    gdaltest.tiff_drv.Delete( 'tmp/tiff_write_87_src.tif' )
    gdaltest.tiff_drv.Delete( 'tmp/tiff_write_87_dst.tif' )

    import struct
    ar = struct.unpack('B' * 8, data)
    if ar[4] != 8 or ar[5] != 0 or ar[6] != 0 or ar[7] != 0:
        gdaltest.post_reason('first IFD is not at offset 8')
        print(ar)
        return 'fail'

    # Check checksums
    if cs1 != 28926 or cs2 != 7332:
        gdaltest.post_reason('did not get expected checksums')
        print(cs1)
        print(cs2)
        return 'fail'

    return 'success'

###############################################################################
# Test that COPY_SRC_OVERVIEWS creation option has an influence
# on BIGTIFF creation

def tiff_write_88():
    md = gdaltest.tiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('BigTIFF') == -1:
        return 'skip'

    # The file would be > 4.2 GB without SPARSE_OK
    src_ds = gdaltest.tiff_drv.Create('tmp/tiff_write_88_src.tif', 60000, 60000, 1,
            options = ['TILED=YES', 'SPARSE_OK=YES'])
    src_ds.BuildOverviews( 'NONE', overviewlist = [2, 4] )
    # Just write one data block so that we can truncate it
    data = src_ds.GetRasterBand(1).GetOverview(1).ReadRaster(0, 0, 128, 128)
    src_ds.GetRasterBand(1).GetOverview(1).WriteRaster(0, 0, 128, 128, data)
    src_ds = None

    # Truncate the file to cause an I/O error on reading
    # so that the CreateCopy() aborts quickly
    f = open('tmp/tiff_write_88_src.tif', 'rb')
    f.seek(0, 2)
    len = f.tell()
    f.seek(0, 0)
    data = f.read(len-1)
    f.close()
    f = open('tmp/tiff_write_88_src.tif', 'wb')
    f.write(data)
    f.close()

    src_ds = gdal.Open('tmp/tiff_write_88_src.tif')
    # for testing only. We need to keep the file to check it was a bigtiff
    gdal.SetConfigOption('GTIFF_DELETE_ON_ERROR', 'NO')
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = gdaltest.tiff_drv.CreateCopy('tmp/tiff_write_88_dst.tif', src_ds,
            options = ['TILED=YES', 'COPY_SRC_OVERVIEWS=YES', 'ENDIANNESS=LITTLE'])
    gdal.PopErrorHandler()
    gdal.SetConfigOption('GTIFF_DELETE_ON_ERROR', None)
    ds = None
    src_ds = None

    f = open('tmp/tiff_write_88_dst.tif', 'rb')
    data = f.read(8)
    f.close()

    os.remove( 'tmp/tiff_write_88_src.tif' )
    os.remove( 'tmp/tiff_write_88_dst.tif' )

    import struct
    ar = struct.unpack('B' * 8, data)
    if ar[2] != 43:
        gdaltest.post_reason('not a BIGTIFF file')
        print(ar)
        return 'fail'
    if ar[4] != 8 or ar[5] != 0 or ar[6] != 0 or ar[7] != 0:
        gdaltest.post_reason('first IFD is not at offset 8')
        print(ar)
        return 'fail'

    return 'success'

###############################################################################
# Test JPEG_QUALITY propagation while creating a (defalte compressed) mask band

def tiff_write_89():
    md = gdaltest.tiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('BigTIFF') == -1:
        return 'skip'

    if md['DMD_CREATIONOPTIONLIST'].find('JPEG') == -1:
        return 'skip'


    last_size = 0
    for quality in [90, 75, 30]:
        src_ds = gdal.Open('../gdrivers/data/utm.tif')

        ds = gdal.GetDriverByName('GTiff').Create('tmp/tiff_write_89.tif', 1024, 1024, 3, \
            options = [ 'COMPRESS=JPEG', 'PHOTOMETRIC=YCBCR', 'JPEG_QUALITY=%d' % quality ])

        gdal.SetConfigOption( 'GDAL_TIFF_INTERNAL_MASK', 'YES' )
        ds.CreateMaskBand(gdal.GMF_PER_DATASET)
        gdal.SetConfigOption( 'GDAL_TIFF_INTERNAL_MASK', None )

        data = src_ds.GetRasterBand(1).ReadRaster(0, 0, 512, 512, 1024, 1024)
        ds.GetRasterBand(1).WriteRaster(0, 0, 1024, 1024, data)
        ds.GetRasterBand(2).WriteRaster(0, 0, 1024, 1024, data)
        ds.GetRasterBand(3).WriteRaster(0, 0, 1024, 1024, data)
        ds.GetRasterBand(1).GetMaskBand().Fill(255)

        src_ds = None
        ds = None

        # older versions of python don't have SEEK_END, add if missing.
        try:
            os.SEEK_END
        except:
            os.SEEK_END = 2

        f = open('tmp/tiff_write_89.tif', 'rb')
        f.seek(0, os.SEEK_END)
        size = f.tell()
        f.close()

        #print('quality = %d, size = %d' % (quality, size))

        if quality != 90:
            if size >= last_size:
                gdaltest.post_reason('did not get decreasing file sizes')
                print(size)
                print(last_size)
                return 'fail'

        last_size = size

    gdaltest.tiff_drv.Delete( 'tmp/tiff_write_89.tif' )

    return 'success'

###############################################################################
# Test JPEG_QUALITY propagation while creating (internal) overviews

def tiff_write_90():
    md = gdaltest.tiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('BigTIFF') == -1:
        return 'skip'

    if md['DMD_CREATIONOPTIONLIST'].find('JPEG') == -1:
        return 'skip'


    last_size = 0
    for quality in [90, 75, 30]:
        src_ds = gdal.Open('../gdrivers/data/utm.tif')

        ds = gdal.GetDriverByName('GTiff').Create('tmp/tiff_write_90.tif', 1024, 1024, 3, \
            options = [ 'COMPRESS=JPEG', 'PHOTOMETRIC=YCBCR', 'JPEG_QUALITY=%d' % quality ])

        data = src_ds.GetRasterBand(1).ReadRaster(0, 0, 512, 512, 1024, 1024)
        ds.GetRasterBand(1).WriteRaster(0, 0, 1024, 1024, data)
        ds.GetRasterBand(2).WriteRaster(0, 0, 1024, 1024, data)
        ds.GetRasterBand(3).WriteRaster(0, 0, 1024, 1024, data)
        ds.BuildOverviews( 'NEAR', overviewlist = [2, 4])

        src_ds = None
        ds = None

        f = open('tmp/tiff_write_90.tif', 'rb')
        f.seek(0, os.SEEK_END)
        size = f.tell()
        f.close()

        #print('quality = %d, size = %d' % (quality, size))

        if quality != 90:
            if size >= last_size:
                gdaltest.post_reason('did not get decreasing file sizes')
                print(size)
                print(last_size)
                return 'fail'

        last_size = size

    gdaltest.tiff_drv.Delete( 'tmp/tiff_write_90.tif' )

    return 'success'


###############################################################################
# Test JPEG_QUALITY propagation while creating (internal) overviews after re-opening

def tiff_write_91():
    md = gdaltest.tiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('BigTIFF') == -1:
        return 'skip'

    if md['DMD_CREATIONOPTIONLIST'].find('JPEG') == -1:
        return 'skip'


    last_size = 0
    for quality in [90, 75, 30]:
        src_ds = gdal.Open('../gdrivers/data/utm.tif')

        ds = gdal.GetDriverByName('GTiff').Create('tmp/tiff_write_91.tif', 1024, 1024, 3, \
            options = [ 'COMPRESS=JPEG', 'PHOTOMETRIC=YCBCR', 'JPEG_QUALITY=%d' % quality ])

        data = src_ds.GetRasterBand(1).ReadRaster(0, 0, 512, 512, 1024, 1024)
        ds.GetRasterBand(1).WriteRaster(0, 0, 1024, 1024, data)
        ds.GetRasterBand(2).WriteRaster(0, 0, 1024, 1024, data)
        ds.GetRasterBand(3).WriteRaster(0, 0, 1024, 1024, data)
        ds = None

        ds = gdal.Open('tmp/tiff_write_91.tif', gdal.GA_Update)
        gdal.SetConfigOption('JPEG_QUALITY_OVERVIEW', '%d' % quality)
        ds.BuildOverviews( 'NEAR', overviewlist = [2, 4])
        gdal.SetConfigOption('JPEG_QUALITY_OVERVIEW', None)

        src_ds = None
        ds = None

        f = open('tmp/tiff_write_91.tif', 'rb')
        f.seek(0, os.SEEK_END)
        size = f.tell()
        f.close()

        #print('quality = %d, size = %d' % (quality, size))

        if quality != 90:
            if size >= last_size:
                gdaltest.post_reason('did not get decreasing file sizes')
                print(size)
                print(last_size)
                return 'fail'

        last_size = size

    gdaltest.tiff_drv.Delete( 'tmp/tiff_write_91.tif' )

    return 'success'


###############################################################################
# Test the effect of JPEG_QUALITY_OVERVIEW while creating (internal) overviews after re-opening

def tiff_write_92():
    md = gdaltest.tiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('BigTIFF') == -1:
        return 'skip'

    if md['DMD_CREATIONOPTIONLIST'].find('JPEG') == -1:
        return 'skip'


    last_size = 0
    quality = 30
    for use_jpeg_quality_overview in [False, True]:
        src_ds = gdal.Open('../gdrivers/data/utm.tif')

        ds = gdal.GetDriverByName('GTiff').Create('tmp/tiff_write_92.tif', 1024, 1024, 3, \
            options = [ 'COMPRESS=JPEG', 'PHOTOMETRIC=YCBCR', 'JPEG_QUALITY=%d' % quality ])

        data = src_ds.GetRasterBand(1).ReadRaster(0, 0, 512, 512, 1024, 1024)
        ds.GetRasterBand(1).WriteRaster(0, 0, 1024, 1024, data)
        ds.GetRasterBand(2).WriteRaster(0, 0, 1024, 1024, data)
        ds.GetRasterBand(3).WriteRaster(0, 0, 1024, 1024, data)
        ds = None

        ds = gdal.Open('tmp/tiff_write_92.tif', gdal.GA_Update)
        if use_jpeg_quality_overview:
            gdal.SetConfigOption('JPEG_QUALITY_OVERVIEW', '%d' % quality)
        ds.BuildOverviews( 'NEAR', overviewlist = [2, 4])
        gdal.SetConfigOption('JPEG_QUALITY_OVERVIEW', None)

        src_ds = None
        ds = None

        f = open('tmp/tiff_write_92.tif', 'rb')
        f.seek(0, os.SEEK_END)
        size = f.tell()
        f.close()

        #print('quality = %d, size = %d' % (quality, size))

        if use_jpeg_quality_overview:
            if size >= last_size:
                gdaltest.post_reason('did not get decreasing file sizes')
                print(size)
                print(last_size)
                return 'fail'

        last_size = size

    gdaltest.tiff_drv.Delete( 'tmp/tiff_write_92.tif' )

    return 'success'

###############################################################################
# Test JPEG_QUALITY_OVERVIEW propagation while creating external overviews

def tiff_write_93():
    md = gdaltest.tiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('BigTIFF') == -1:
        return 'skip'

    if md['DMD_CREATIONOPTIONLIST'].find('JPEG') == -1:
        return 'skip'

    src_ds = gdal.Open('../gdrivers/data/utm.tif')
    ds = gdal.GetDriverByName('GTiff').Create('tmp/tiff_write_93.tif', 1024, 1024, 3, \
        options = [ 'COMPRESS=JPEG', 'PHOTOMETRIC=YCBCR' ])

    data = src_ds.GetRasterBand(1).ReadRaster(0, 0, 512, 512, 1024, 1024)
    ds.GetRasterBand(1).WriteRaster(0, 0, 1024, 1024, data)
    ds.GetRasterBand(2).WriteRaster(0, 0, 1024, 1024, data)
    ds.GetRasterBand(3).WriteRaster(0, 0, 1024, 1024, data)
    ds = None

    src_ds = None

    last_size = 0
    for quality in [90, 75, 30]:

        try:
            os.remove('tmp/tiff_write_93.tif.ovr')
        except:
            pass

        ds = gdal.Open('tmp/tiff_write_93.tif')
        gdal.SetConfigOption('COMPRESS_OVERVIEW', 'JPEG')
        gdal.SetConfigOption('JPEG_QUALITY_OVERVIEW', '%d' % quality)
        gdal.SetConfigOption('PHOTOMETRIC_OVERVIEW', 'YCBCR')
        ds.BuildOverviews( 'NEAR', overviewlist = [2, 4])
        gdal.SetConfigOption('COMPRESS_OVERVIEW', None)
        gdal.SetConfigOption('JPEG_QUALITY_OVERVIEW', None)
        gdal.SetConfigOption('PHOTOMETRIC_OVERVIEW', None)
        ds = None

        f = open('tmp/tiff_write_93.tif.ovr', 'rb')
        f.seek(0, os.SEEK_END)
        size = f.tell()
        f.close()

        #print('quality = %d, size = %d' % (quality, size))

        if quality != 90:
            if size >= last_size:
                gdaltest.post_reason('did not get decreasing file sizes')
                print(size)
                print(last_size)
                return 'fail'

            if quality == 30 and size >= 83000:
                gdaltest.post_reason('file larger than expected. should be about 69100. perhaps jpeg quality is not well propagated')
                print(size)
                return 'fail'

        last_size = size

    gdaltest.tiff_drv.Delete( 'tmp/tiff_write_93.tif' )

    return 'success'


###############################################################################
# Test CreateCopy() of a dataset with a mask into a JPEG compressed dataset
# and check JPEG_QUALITY propagation without warning

def tiff_write_94():
    md = gdaltest.tiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('BigTIFF') == -1:
        return 'skip'

    if md['DMD_CREATIONOPTIONLIST'].find('JPEG') == -1:
        return 'skip'

    src_ds = gdal.GetDriverByName('GTiff').Create('tmp/tiff_write_94_src.tif', 1024, 1024, 3)
    gdal.SetConfigOption( 'GDAL_TIFF_INTERNAL_MASK', 'YES' )
    src_ds.CreateMaskBand(gdal.GMF_PER_DATASET)
    gdal.SetConfigOption( 'GDAL_TIFF_INTERNAL_MASK', None )
    src_ds.GetRasterBand(1).GetMaskBand().WriteRaster(0,0,1,1,'\xff',1,1)

    gdal.SetConfigOption( 'GDAL_TIFF_INTERNAL_MASK', 'YES' )
    ds = gdal.GetDriverByName('GTiff').CreateCopy('tmp/tiff_write_94_dst.tif', src_ds,
        options = [ 'COMPRESS=JPEG', 'PHOTOMETRIC=YCBCR', 'JPEG_QUALITY=30' ])
    gdal.SetConfigOption( 'GDAL_TIFF_INTERNAL_MASK', None )

    src_ds = None
    ds = None

    ds = gdal.Open('tmp/tiff_write_94_dst.tif')
    cs = ds.GetRasterBand(1).GetMaskBand().Checksum()
    ds = None

    gdaltest.tiff_drv.Delete( 'tmp/tiff_write_94_src.tif' )
    gdaltest.tiff_drv.Delete( 'tmp/tiff_write_94_dst.tif' )

    if cs != 3:
        print(cs)
        gdaltest.post_reason( 'wrong checksum' )
        return 'fail'

    return 'success'

###############################################################################
# Test that COPY_SRC_OVERVIEWS deal well with rounding issues when computing
# overview levels from the overview size

def tiff_write_95():

    src_ds = gdaltest.tiff_drv.Create('tmp/tiff_write_95_src.tif', 7171, 6083, options = ['SPARSE_OK=YES'])
    src_ds.BuildOverviews( 'NONE', overviewlist = [ 2, 4, 8, 16, 32, 64 ])
    gdal.SetConfigOption('GTIFF_DONT_WRITE_BLOCKS', 'YES')
    ds = gdaltest.tiff_drv.CreateCopy('tmp/tiff_write_95_dst.tif', src_ds, options = ['COPY_SRC_OVERVIEWS=YES'])
    gdal.SetConfigOption('GTIFF_DONT_WRITE_BLOCKS', None)
    ok = ds != None
    ds = None
    src_ds = None

    gdaltest.tiff_drv.Delete( 'tmp/tiff_write_95_src.tif' )
    gdaltest.tiff_drv.Delete( 'tmp/tiff_write_95_dst.tif' )

    if not ok:
        return 'fail'

    return 'success'

###############################################################################
# Test that COPY_SRC_OVERVIEWS combined with GDAL_TIFF_INTERNAL_MASK=YES work well

def tiff_write_96():

    gdal.SetConfigOption('GDAL_TIFF_INTERNAL_MASK', 'YES')
    src_ds = gdaltest.tiff_drv.Create('tmp/tiff_write_96_src.tif', 100, 100)
    src_ds.GetRasterBand(1).Fill(255)
    src_ds.CreateMaskBand(gdal.GMF_PER_DATASET)
    from sys import version_info
    if version_info >= (3,0,0):
        exec("src_ds.GetRasterBand(1).GetMaskBand().WriteRaster(25,25,50,50,b'\\xff',1,1)")
    else:
        src_ds.GetRasterBand(1).GetMaskBand().WriteRaster(25,25,50,50,'\xff',1,1)
    src_ds.BuildOverviews( 'NEAR', overviewlist = [ 2, 4 ])
    expected_cs = src_ds.GetRasterBand(1).Checksum()
    expected_cs_mask = src_ds.GetRasterBand(1).GetMaskBand().Checksum()
    expected_cs_ovr_1 = src_ds.GetRasterBand(1).GetOverview(0).Checksum()
    expected_cs_ovr_mask_1 = src_ds.GetRasterBand(1).GetOverview(0).GetMaskBand().Checksum()
    expected_cs_ovr_2 = src_ds.GetRasterBand(1).GetOverview(1).Checksum()
    expected_cs_ovr_mask_2 = src_ds.GetRasterBand(1).GetOverview(1).GetMaskBand().Checksum()

    ds = gdaltest.tiff_drv.CreateCopy('tmp/tiff_write_96_dst.tif', src_ds, options = ['COPY_SRC_OVERVIEWS=YES'])
    ds = None
    gdal.SetConfigOption('GDAL_TIFF_INTERNAL_MASK', None)

    ds = gdal.Open('tmp/tiff_write_96_dst.tif')
    cs = ds.GetRasterBand(1).Checksum()
    cs_mask = ds.GetRasterBand(1).GetMaskBand().Checksum()
    cs_ovr_1 = ds.GetRasterBand(1).GetOverview(0).Checksum()
    cs_ovr_mask_1 = ds.GetRasterBand(1).GetOverview(0).GetMaskBand().Checksum()
    cs_ovr_2 = ds.GetRasterBand(1).GetOverview(1).Checksum()
    cs_ovr_mask_2 = ds.GetRasterBand(1).GetOverview(1).GetMaskBand().Checksum()

    ds = None
    src_ds = None

    gdaltest.tiff_drv.Delete( 'tmp/tiff_write_96_src.tif' )
    gdaltest.tiff_drv.Delete( 'tmp/tiff_write_96_dst.tif' )

    if [expected_cs,expected_cs_mask,expected_cs_ovr_1,expected_cs_ovr_mask_1,expected_cs_ovr_2,expected_cs_ovr_mask_2] != \
       [cs,cs_mask,cs_ovr_1,cs_ovr_mask_1,cs_ovr_2,cs_ovr_mask_2]:
        gdaltest.post_reason('did not get expected checksums')
        print(expected_cs,expected_cs_mask,expected_cs_ovr_1,expected_cs_ovr_mask_1,expected_cs_ovr_2,expected_cs_ovr_mask_2)
        print(cs,cs_mask,cs_ovr_1,cs_ovr_mask_1,cs_ovr_2,cs_ovr_mask_2)
        return 'fail'

    return 'success'

###############################################################################
# Create a simple file by copying from an existing one - PixelIsPoint

def tiff_write_97():

    gdal.SetConfigOption( 'GTIFF_POINT_GEO_IGNORE', 'FALSE' )

    src_ds = gdal.Open( 'data/byte_point.tif' )

    new_ds = gdaltest.tiff_drv.CreateCopy( 'tmp/test_97.tif', src_ds )

    gt = new_ds.GetGeoTransform()
    md = new_ds.GetMetadataItem( 'AREA_OR_POINT' )
    new_ds = None

    gt_expected = (440690.0, 60.0, 0.0, 3751350.0, 0.0, -60.0)

    if gt != gt_expected:
        print(gt)
        gdaltest.post_reason( 'did not get expected geotransform' )
        return 'fail'

    if md != 'Point':
        gdaltest.post_reason( 'did not get expected AREA_OR_POINT value' )
        return 'fail'

    gdaltest.tiff_drv.Delete( 'tmp/test_97.tif' )

    # Again, but ignoring PixelIsPoint

    gdal.SetConfigOption( 'GTIFF_POINT_GEO_IGNORE', 'TRUE' )

    new_ds = gdaltest.tiff_drv.CreateCopy( 'tmp/test_97_2.tif', src_ds )

    gt = new_ds.GetGeoTransform()
    md = new_ds.GetMetadataItem( 'AREA_OR_POINT' )
    new_ds = None
    src_ds = None

    gt_expected = (440690.0, 60.0, 0.0, 3751350.0, 0.0, -60.0)

    if gt != gt_expected:
        print(gt)
        gdaltest.post_reason( 'did not get expected geotransform when ignoring PixelIsPoint' )
        return 'fail'

    if md != 'Point':
        gdaltest.post_reason( 'did not get expected AREA_OR_POINT value' )
        return 'fail'

    gdal.SetConfigOption( 'GTIFF_POINT_GEO_IGNORE', 'FALSE' )

    # read back this file with pixelispoint behavior enabled.

    new_ds = gdal.Open( 'tmp/test_97_2.tif' )

    gt = new_ds.GetGeoTransform()
    md = new_ds.GetMetadataItem( 'AREA_OR_POINT' )
    new_ds = None

    gt_expected = (440660.0, 60.0, 0.0, 3751380.0, 0.0, -60.0)

    if gt != gt_expected:
        print(gt)
        gdaltest.post_reason( 'did not get expected geotransform when ignoring PixelIsPoint (2)' )
        return 'fail'

    if md != 'Point':
        gdaltest.post_reason( 'did not get expected AREA_OR_POINT value' )
        return 'fail'

    gdaltest.tiff_drv.Delete( 'tmp/test_97_2.tif' )

    return 'success'

###############################################################################
# Create a rotated geotiff file (uses a geomatrix) with - PixelIsPoint

def tiff_write_98():

    gdal.SetConfigOption( 'GTIFF_POINT_GEO_IGNORE', 'FALSE' )

    src_ds = gdal.Open( 'data/geomatrix.tif' )

    gdal.SetConfigOption( 'GTIFF_POINT_GEO_IGNORE', 'TRUE' )

    new_ds = gdaltest.tiff_drv.CreateCopy( 'tmp/test_98.tif', src_ds )

    gt = new_ds.GetGeoTransform()
    md = new_ds.GetMetadataItem( 'AREA_OR_POINT' )
    new_ds = None
    src_ds = None

    gt_expected = (1841001.75, 1.5, -5.0, 1144003.25, -5.0, -1.5)

    if gt != gt_expected:
        print(gt)
        gdaltest.post_reason( 'did not get expected geotransform' )
        return 'fail'

    if md != 'Point':
        gdaltest.post_reason( 'did not get expected AREA_OR_POINT value' )
        return 'fail'

    gdal.SetConfigOption( 'GTIFF_POINT_GEO_IGNORE', 'FALSE' )

    new_ds = gdal.Open( 'tmp/test_98.tif' )

    gt = new_ds.GetGeoTransform()
    md = new_ds.GetMetadataItem( 'AREA_OR_POINT' )
    new_ds = None
    src_ds = None

    gt_expected = (1841003.5, 1.5, -5.0, 1144006.5, -5.0, -1.5)

    if gt != gt_expected:
        print(gt)
        gdaltest.post_reason( 'did not get expected geotransform (2)' )
        return 'fail'

    if md != 'Point':
        gdaltest.post_reason( 'did not get expected AREA_OR_POINT value' )
        return 'fail'

    gdaltest.tiff_drv.Delete( 'tmp/test_98.tif' )

    return 'success'

###############################################################################
# Create copy into a RGB JPEG-IN-TIFF (#3887)

def tiff_write_99():

    src_ds = gdal.Open('data/rgbsmall.tif')
    new_ds = gdaltest.tiff_drv.CreateCopy( 'tmp/test_99.tif', src_ds, options = ['COMPRESS=JPEG'] )
    new_ds = None
    src_ds = None

    ds = gdal.Open('tmp/test_99.tif')
    cs1 = ds.GetRasterBand(1).Checksum()
    cs2 = ds.GetRasterBand(2).Checksum()
    cs3 = ds.GetRasterBand(3).Checksum()
    ds = None

    gdaltest.tiff_drv.Delete( 'tmp/test_99.tif' )

    if (cs1, cs2, cs3) != (21629,21651,21371):
        print('%d,%d,%d' % (cs1, cs2, cs3))
        return 'fail'

    return 'success'

###############################################################################
# Create copy into a 2 band JPEG-IN-TIFF (#3887)

def tiff_write_100():

    src_ds = gdaltest.tiff_drv.Create( '/vsimem/test_100_src.tif', 16, 16, 2 )
    src_ds.GetRasterBand(1).Fill(255)
    new_ds = gdaltest.tiff_drv.CreateCopy( '/vsimem/test_100_dst.tif', src_ds, options = ['COMPRESS=JPEG'] )
    new_ds = None
    src_ds = None

    ds = gdal.Open('/vsimem/test_100_dst.tif')
    cs1 = ds.GetRasterBand(1).Checksum()
    cs2 = ds.GetRasterBand(2).Checksum()
    ds = None

    gdaltest.tiff_drv.Delete( '/vsimem/test_100_src.tif' )
    gdaltest.tiff_drv.Delete( '/vsimem/test_100_dst.tif' )

    if (cs1, cs2) != (3118,0):
        print('%d,%d' % (cs1, cs2))
        return 'fail'

    return 'success'

###############################################################################
# Test CHUNKY_STRIP_READ_SUPPORT (#3894)
# We use random data so the compressed files are big enough to need partial
# reloading. tiff_write_78 doesn't produce enough big data to trigger this...

def tiff_write_101():

    if not gdaltest.run_slow_tests():
        return 'skip'

    md = gdaltest.tiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('BigTIFF') == -1:
        return 'skip'

    if sys.platform.startswith('linux'):
        # Much faster to use /dev/urandom than python random generator !
        f = open('/dev/urandom', 'rb')
        rand_array = f.read(10 * 1024 * 1024)
        f.close()
    else:
        import random
        import array
        rand_array = array.array('B')
        for i in range(10 * 1024 * 1024):
            rand_array.append(random.randint(0,255))

    f = open('tmp/tiff_write_101.bin', 'wb')
    f.write(rand_array)
    f.close()

    f = open('tmp/tiff_write_101.hdr', 'wb')
    f.write("""ENVI
samples = 2500
lines   = 4000
bands   = 1
header offset = 0
file type = ENVI Standard
data type = 1
interleave = bsq
byte order = 0
map info = {UTM, 1, 1, 440720.000000, 3751320.000000, 60.000000, 60.000000, 11, North}
band names = {
Band 1}""".encode('ascii'))
    f.close()

    src_ds = gdal.Open('tmp/tiff_write_101.bin')
    expected_cs = src_ds.GetRasterBand(1).Checksum()

    for compression_method in ['DEFLATE', 'LZW', 'JPEG', 'PACKBITS', 'LZMA' ]:
        if md['DMD_CREATIONOPTIONLIST'].find(compression_method) == -1:
            continue

        ds = gdaltest.tiff_drv.CreateCopy('tmp/tiff_write_101.tif', src_ds, \
            options = ['COMPRESS=' + compression_method, 'BLOCKXSIZE=2500', 'BLOCKYSIZE=4000'])
        ds = None

        ds = gdal.Open('tmp/tiff_write_101.tif')
        gdal.ErrorReset()
        cs = ds.GetRasterBand(1).Checksum()
        error_msg = gdal.GetLastErrorMsg()
        ds = None

        gdaltest.tiff_drv.Delete( 'tmp/tiff_write_101.tif' )

        if error_msg != '':
            src_ds = None
            gdaltest.tiff_drv.Delete( 'tmp/tiff_write_101.bin' )
            return 'fail'

        if compression_method != 'JPEG' and cs != expected_cs:
            gdaltest.post_reason('for compression method %s, got %d instead of %d' % (compression_method, cs, expected_cs))
            src_ds = None
            gdaltest.tiff_drv.Delete( 'tmp/tiff_write_101.bin' )
            return 'fail'

    src_ds = None
    gdaltest.tiff_drv.Delete( 'tmp/tiff_write_101.bin' )

    return 'success'

###############################################################################
# Test writing and reading back COMPD_CS

def tiff_write_102():

    ds = gdaltest.tiff_drv.Create('/vsimem/tiff_write_102.tif',1,1)
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(7401)
    wkt = sr.ExportToWkt()
    ds.SetProjection(wkt)
    ds = None

    gdal.SetConfigOption('GTIFF_REPORT_COMPD_CS', 'YES')
    ds = gdal.Open('/vsimem/tiff_write_102.tif')
    wkt1 = ds.GetProjectionRef()
    ds = None

    gdal.SetConfigOption('GTIFF_REPORT_COMPD_CS', 'NO')
    ds = gdal.Open('/vsimem/tiff_write_102.tif')
    wkt2 = ds.GetProjectionRef()
    ds = None

    gdaltest.tiff_drv.Delete( '/vsimem/tiff_write_102.tif' )

    if wkt1.find('COMPD_CS') != 0:
        gdaltest.post_reason('expected COMPD_CS, but got something else')
        print(wkt1)
        return 'fail'

    if wkt2.find('COMPD_CS') == 0:
        gdaltest.post_reason('got COMPD_CS, but did not expected it')
        print(wkt2)
        return 'fail'

    return 'success'

###############################################################################
# Test -co COPY_SRC_OVERVIEWS=YES on a multiband source with external overviews (#3938)

def tiff_write_103():
    import test_cli_utilities
    if test_cli_utilities.get_gdal_translate_path() is None:
        return 'skip'
    if test_cli_utilities.get_gdaladdo_path() is None:
        return 'skip'

    gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' data/rgbsmall.tif tmp/tiff_write_103_src.tif -outsize 260 260')
    gdaltest.runexternal(test_cli_utilities.get_gdaladdo_path() + ' -ro tmp/tiff_write_103_src.tif 2')
    gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' tmp/tiff_write_103_src.tif tmp/tiff_write_103_dst.tif -co COPY_SRC_OVERVIEWS=YES')

    src_ds = gdal.Open('tmp/tiff_write_103_src.tif')
    dst_ds = gdal.Open('tmp/tiff_write_103_dst.tif')
    src_cs = src_ds.GetRasterBand(1).GetOverview(0).Checksum()
    dst_cs = dst_ds.GetRasterBand(1).GetOverview(0).Checksum()
    src_ds = None
    dst_ds = None

    gdaltest.tiff_drv.Delete( 'tmp/tiff_write_103_src.tif' )
    gdaltest.tiff_drv.Delete( 'tmp/tiff_write_103_dst.tif' )

    if src_cs != dst_cs:
        gdaltest.post_reason('did not get expected checksum')
        print(src_cs)
        print(dst_cs)
        return 'fail'

    return 'success'


###############################################################################
# Confirm as best we can that we can write geotiff files with detailed
# projection parameters with the correct linear units set.  (#3901)

def tiff_write_104():

    src_ds = gdal.Open( 'data/spaf27_correct.tif' )
    dst_ds = gdaltest.tiff_drv.CreateCopy( 'tmp/test_104.tif', src_ds )

    src_ds = None
    dst_ds = None

    ds = gdal.Open( 'tmp/test_104.tif' )
    wkt = ds.GetProjectionRef()
    ds = None

    srs = osr.SpatialReference( wkt )
    fe = srs.GetProjParm(osr.SRS_PP_FALSE_EASTING)
    if abs(fe-2000000.0) > 0.001:
        gdaltest.post_reason( 'did not get expected false easting' )
        return 'fail'

    gdaltest.tiff_drv.Delete( 'tmp/test_104.tif' )

    return 'success'

###############################################################################
# Confirm as best we can that we can write geotiff files with detailed
# projection parameters with the correct linear units set.  (#3901)

def tiff_write_105():

    # This hangs forever with libtiff 3.8.2, so skip it
    md = gdaltest.tiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('BigTIFF') == -1:
        return 'skip'

    shutil.copyfile( 'data/bug4468.tif', 'tmp/bug4468.tif' )

    # Update a pixel and close again.
    ds = gdal.Open( 'tmp/bug4468.tif', gdal.GA_Update )
    data = ds.ReadRaster(0,0,1,1)
    ds.WriteRaster(0,0,1,1,data)
    ds = None

    # Now check if the image is still intact.
    ds = gdal.Open( 'tmp/bug4468.tif' )
    cs = ds.GetRasterBand(1).Checksum()

    if cs != 2923:
        gdaltest.post_reason( 'Did not get expected checksum, got %d.' % cs )
        return 'fail'

    ds = None

    gdaltest.tiff_drv.Delete( 'tmp/bug4468.tif' )

    return 'success'

###############################################################################
# Test the direct copy mechanism of JPEG source

def tiff_write_106(filename = '../gdrivers/data/byte_with_xmp.jpg', options = ['COMPRESS=JPEG'], check_cs = True):

    md = gdaltest.tiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('JPEG') == -1:
        return 'skip'

    src_ds = gdal.Open(filename)
    nbands = src_ds.RasterCount
    src_cs = []
    for i in range(nbands):
        src_cs.append(src_ds.GetRasterBand(i+1).Checksum())

    out_ds = gdaltest.tiff_drv.CreateCopy('/vsimem/tiff_write_106.tif', src_ds, options = options)
    out_ds = None

    out_ds = gdal.Open('/vsimem/tiff_write_106.tif')
    cs = []
    for i in range(nbands):
        cs.append(out_ds.GetRasterBand(i+1).Checksum())
    out_ds = None

    gdal.Unlink('/vsimem/tiff_write_106.tif')

    if check_cs:
        for i in range(nbands):
            if cs[i] != src_cs[i]:
                gdaltest.post_reason('did not get expected checksum')
                print(cs[i])
                print(src_cs[i])
                return 'fail'
    else:
        for i in range(nbands):
            if cs[i] == 0:
                gdaltest.post_reason('did not get expected checksum')
                return 'fail'

    return 'success'

def tiff_write_107():
    return tiff_write_106(options = ['COMPRESS=JPEG','BLOCKYSIZE=8'])

def tiff_write_108():
    return tiff_write_106(options = ['COMPRESS=JPEG','BLOCKYSIZE=20'])

def tiff_write_109():
    return tiff_write_106(options = ['COMPRESS=JPEG','TILED=YES','BLOCKYSIZE=16','BLOCKXSIZE=16'])

# Strip organization of YCbCr does *NOT* give exact pixels w.r.t. original image
def tiff_write_110():
    return tiff_write_106(filename = '../gdrivers/data/albania.jpg' , check_cs = False)

# Whole copy of YCbCr *DOES* give exact pixels w.r.t. original image
def tiff_write_111():
    return tiff_write_106(filename = '../gdrivers/data/albania.jpg' , options = ['COMPRESS=JPEG', 'BLOCKYSIZE=260'])

def tiff_write_111_bis():
    return tiff_write_106(filename = '../gdrivers/data/albania.jpg' , options = ['COMPRESS=JPEG', 'BLOCKYSIZE=260', 'INTERLEAVE=PIXEL'])

def tiff_write_111_ter():
    return tiff_write_106(filename = '../gdrivers/data/albania.jpg' , options = ['COMPRESS=JPEG', 'BLOCKYSIZE=260', 'INTERLEAVE=BAND'], check_cs = False)

# Tiled organization of YCbCr does *NOT* give exact pixels w.r.t. original image
def tiff_write_112():
    return tiff_write_106(filename = '../gdrivers/data/albania.jpg' , options = ['COMPRESS=JPEG', 'TILED=YES'], check_cs = False)

# The source is a JPEG in RGB colorspace (usually it is YCbCr).
def tiff_write_113():
    return tiff_write_106(filename = '../gdrivers/data/rgbsmall_rgb.jpg' , options = ['COMPRESS=JPEG', 'BLOCKYSIZE=8'])

###############################################################################
# Test CreateCopy() interruption

def tiff_write_114():

    tst = gdaltest.GDALTest( 'GTiff', 'byte.tif', 1, 4672 )

    return tst.testCreateCopy( vsimem = 1, interrupt_during_copy = True )

###############################################################################
# Test writing a pixel interleaved RGBA JPEG-compressed TIFF

def tiff_write_115():
    md = gdaltest.tiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('JPEG') == -1:
        return 'skip'

    tmpfilename = '/vsimem/tiff_write_115.tif'

    src_ds = gdal.Open('data/stefan_full_rgba.tif')
    ds = gdaltest.tiff_drv.CreateCopy(tmpfilename, src_ds, options = ['COMPRESS=JPEG'])
    if ds is None:
        return 'fail'
    ds = None
    src_ds = None

    f = gdal.VSIFOpenL(tmpfilename + '.aux.xml', 'rb')
    if f is not None:
        gdal.VSIFCloseL(f)
        gdal.Unlink(tmpfilename)
        return 'fail'

    ds = gdal.Open(tmpfilename)
    md = ds.GetMetadata('IMAGE_STRUCTURE')
    if md['INTERLEAVE'] != 'PIXEL':
        gdaltest.post_reason('failed')
        ds = None
        gdal.Unlink(tmpfilename)
        return 'fail'

    expected_cs = [16404, 62700, 37913, 14174]
    for i in range(4):
        cs = ds.GetRasterBand(i+1).Checksum()
        if cs != expected_cs[i]:
            gdaltest.post_reason('failed')
            ds = None
            gdal.Unlink(tmpfilename)
            return 'fail'

        if ds.GetRasterBand(i+1).GetRasterColorInterpretation() != gdal.GCI_RedBand + i:
            gdaltest.post_reason('failed')
            ds = None
            gdal.Unlink(tmpfilename)
            return 'fail'

    ds = None
    gdal.Unlink(tmpfilename)

    return 'success'

###############################################################################
# Test writing a band interleaved RGBA JPEG-compressed TIFF

def tiff_write_116():
    md = gdaltest.tiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('JPEG') == -1:
        return 'skip'

    tmpfilename = '/vsimem/tiff_write_116.tif'

    src_ds = gdal.Open('data/stefan_full_rgba.tif')
    ds = gdaltest.tiff_drv.CreateCopy(tmpfilename, src_ds, options = ['COMPRESS=JPEG', 'INTERLEAVE=BAND'])
    if ds is None:
        return 'fail'
    ds = None
    src_ds = None

    f = gdal.VSIFOpenL(tmpfilename + '.aux.xml', 'rb')
    if f is not None:
        gdal.VSIFCloseL(f)
        gdal.Unlink(tmpfilename)
        return 'fail'

    ds = gdal.Open(tmpfilename)
    md = ds.GetMetadata('IMAGE_STRUCTURE')
    if md['INTERLEAVE'] != 'BAND':
        gdaltest.post_reason('failed')
        ds = None
        gdal.Unlink(tmpfilename)
        return 'fail'

    expected_cs = [16404, 62700, 37913, 14174]
    for i in range(4):
        cs = ds.GetRasterBand(i+1).Checksum()
        if cs != expected_cs[i]:
            gdaltest.post_reason('failed')
            ds = None
            gdal.Unlink(tmpfilename)
            return 'fail'

        if ds.GetRasterBand(i+1).GetRasterColorInterpretation() != gdal.GCI_RedBand + i:
            gdaltest.post_reason('failed')
            ds = None
            gdal.Unlink(tmpfilename)
            return 'fail'

    ds = None
    gdal.Unlink(tmpfilename)

    return 'success'

###############################################################################
# Test bugfix for ticket #4771 (rewriting of a deflate compressed tile, libtiff bug)

def tiff_write_117():
    # This will also fail with a libtiff 4.x older than 2012-08-13
    # Might be good to be able to test internal libtiff presence
    md = gdaltest.tiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('BigTIFF') == -1:
        return 'skip'

    import random
    from osgeo import gdal

    # so that we have always the same random :-)
    random.seed(0)

    ds = gdal.GetDriverByName('GTiff').Create('/vsimem/tiff_write_117.tif', 512, 256, 2, options = ['COMPRESS=DEFLATE', 'TILED=YES'])

    # Write first tile so that its byte count of that tile is 2048 (a multiple of 1024)
    adjust = 1254
    data = ''.join(['0' for i in range(65536 - adjust)]) + ''.join([('%c' % random.randint(0,255)) for i in range(adjust)])
    ds.GetRasterBand(1).WriteRaster(0, 0, 256, 256, data)

    # Second tile will be implicitely written at closing, or we could write any content

    ds = None

    ds = gdal.Open('/vsimem/tiff_write_117.tif', gdal.GA_Update)

    # Will adjust tif_rawdatasize to TIFFroundup_64((uint64)size, 1024) = TIFFroundup_64(2048, 1024) = 2048
    ds.GetRasterBand(1).ReadRaster(0, 0, 256, 256)

    # The new bytecount will be greater than 2048
    data = ''.join([('%c' % random.randint(0,255)) for i in range(256 * 256)])
    ds.GetRasterBand(1).WriteRaster(0, 0, 256, 256, data)

    # Make sure that data is written now
    ds.FlushCache()

    # Oops, without fix, the second tile will have been overwritten and an error will be emitted
    data = ds.GetRasterBand(1).ReadRaster(256, 0, 256, 256)

    ds = None

    gdal.Unlink('/vsimem/tiff_write_117.tif')

    if data is None:
        gdaltest.post_reason('if GDAL is configured with external libtiff 4.x, it can fail if it is older than 4.0.3. With internal libtiff, should not fail')
        return 'fail'

    return 'success'

###############################################################################
# Test bugfix for ticket #4816

def tiff_write_118():

    ds = gdal.GetDriverByName('GTiff').Create('/vsimem/tiff_write_118.tif', 1, 1)
    # Should be rejected in a non-XML domain
    ds.SetMetadata('bla', 'foo')
    ds = None

    ds = gdal.Open('/vsimem/tiff_write_118.tif')
    md = ds.GetMetadata('foo')
    ds = None

    gdal.Unlink('/vsimem/tiff_write_118.tif')

    if len(md) != 0:
        print(md)
        return 'fail'

    return 'success'

###############################################################################
# Test bugfix for ticket #4816

def tiff_write_119():

    ds = gdal.GetDriverByName('GTiff').Create('/vsimem/tiff_write_119.tif', 1, 1)
    ds.SetMetadata('foo=bar', 'foo')
    ds = None

    ds = gdal.Open('/vsimem/tiff_write_119.tif')
    md = ds.GetMetadata('foo')
    ds = None

    gdal.Unlink('/vsimem/tiff_write_119.tif')

    if md['foo'] != 'bar':
        print(md)
        return 'fail'

    return 'success'

###############################################################################
# Test bugfix for ticket #4816

def tiff_write_120():

    ds = gdal.GetDriverByName('GTiff').Create('/vsimem/tiff_write_120.tif', 1, 1)
    ds.SetMetadata('<foo/>', 'xml:foo')
    ds = None

    ds = gdal.Open('/vsimem/tiff_write_120.tif')
    md = ds.GetMetadata('xml:foo')
    ds = None

    gdal.Unlink('/vsimem/tiff_write_120.tif')

    if len(md) != 1:
        print(md)
        return 'fail'
    if md[0] != '<foo/>':
        print(md)
        return 'fail'

    return 'success'

###############################################################################
# Test error cases of COPY_SRC_OVERVIEWS creation option

def tiff_write_121():

    # Test when the overview band is NULL
    src_ds = gdal.Open("""<VRTDataset rasterXSize="20" rasterYSize="20">
  <VRTRasterBand dataType="Byte" band="1">
    <SimpleSource>
      <SourceFilename relativeToVRT="1">data/byte.tif</SourceFilename>
      <SourceBand>1</SourceBand>
    </SimpleSource>
    <Overview>
      <SourceFilename relativeToVRT="0">non_existing</SourceFilename>
      <SourceBand>1</SourceBand>
    </Overview>
  </VRTRasterBand>
</VRTDataset>""")
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = gdaltest.tiff_drv.CreateCopy('/vsimem/tiff_write_121.tif', src_ds, options = ['COPY_SRC_OVERVIEWS=YES'])
    gdal.PopErrorHandler()
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    src_ds = None

    # Test when the overview count isn't the same on all base bands
    src_ds = gdal.Open("""<VRTDataset rasterXSize="20" rasterYSize="20">
  <VRTRasterBand dataType="Byte" band="1">
    <SimpleSource>
      <SourceFilename relativeToVRT="1">data/byte.tif</SourceFilename>
      <SourceBand>1</SourceBand>
    </SimpleSource>
    <Overview>
      <SourceFilename relativeToVRT="1">data/byte.tif</SourceFilename>
      <SourceBand>1</SourceBand>
    </Overview>
  </VRTRasterBand>
  <VRTRasterBand dataType="Byte" band="2">
    <SimpleSource>
      <SourceFilename relativeToVRT="1">data/byte.tif</SourceFilename>
      <SourceBand>1</SourceBand>
    </SimpleSource>
  </VRTRasterBand>
</VRTDataset>""")
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = gdaltest.tiff_drv.CreateCopy('/vsimem/tiff_write_121.tif', src_ds, options = ['COPY_SRC_OVERVIEWS=YES'])
    gdal.PopErrorHandler()
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    src_ds = None

    # Test when the overview bands of same level have not the same dimensions
    src_ds = gdal.Open("""<VRTDataset rasterXSize="20" rasterYSize="20">
  <VRTRasterBand dataType="Byte" band="1">
    <SimpleSource>
      <SourceFilename relativeToVRT="1">data/byte.tif</SourceFilename>
      <SourceBand>1</SourceBand>
    </SimpleSource>
    <Overview>
      <SourceFilename relativeToVRT="1">data/byte.tif</SourceFilename>
      <SourceBand>1</SourceBand>
    </Overview>
  </VRTRasterBand>
  <VRTRasterBand dataType="Byte" band="2">
    <SimpleSource>
      <SourceFilename relativeToVRT="1">data/byte.tif</SourceFilename>
      <SourceBand>1</SourceBand>
    </SimpleSource>
    <Overview>
      <SourceFilename relativeToVRT="0">data/rgbsmall.tif</SourceFilename>
      <SourceBand>1</SourceBand>
    </Overview>
  </VRTRasterBand>
</VRTDataset>""")
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = gdaltest.tiff_drv.CreateCopy('/vsimem/tiff_write_121.tif', src_ds, options = ['COPY_SRC_OVERVIEWS=YES'])
    gdal.PopErrorHandler()
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    src_ds = None

    return 'success'

###############################################################################
# Test write and read of some TIFFTAG_RESOLUTIONUNIT tags where '*'/'' is
# specified (gdalwarp conflicts)
# Expected to fail (properly) with older libtiff versions (<=3.8.2 for sure)

def tiff_write_122():
    new_ds = gdaltest.tiff_drv.Create('tmp/tags122.tif', 1, 1, 1)

    new_ds.SetMetadata({
        'TIFFTAG_RESOLUTIONUNIT': '*',
    })

    new_ds = None
    # hopefully it's closed now!

    new_ds = gdal.Open('tmp/tags122.tif')
    md = new_ds.GetMetadata()

    if 'TIFFTAG_RESOLUTIONUNIT' not in md:
        gdaltest.post_reason('Couldnt find tag TIFFTAG_RESOLUTIONUNIT')
        return 'fail'

    elif md['TIFFTAG_RESOLUTIONUNIT'] != '1 (unitless)':
        gdaltest.post_reason("Got unexpected tag TIFFTAG_RESOLUTIONUNIT='%s' (expected ='1 (unitless)')" % md['TIFFTAG_RESOLUTIONUNIT'])
        return 'fail'

    new_ds = None

    gdaltest.tiff_drv.Delete('tmp/tags122.tif')

    return 'success'

###############################################################################
# Test implicit photometric interpretation

def tiff_write_123():

    src_ds = gdaltest.tiff_drv.Create('/vsimem/tiff_write_123_src.tif', 1,1,3,gdal.GDT_Int16)
    src_ds.GetRasterBand(1).SetColorInterpretation(gdal.GCI_RedBand)
    src_ds.GetRasterBand(2).SetColorInterpretation(gdal.GCI_GreenBand)
    src_ds.GetRasterBand(3).SetColorInterpretation(gdal.GCI_BlueBand)
    src_ds = None
    statBuf = gdal.VSIStatL('/vsimem/tiff_write_123_src.tif.aux.xml', gdal.VSI_STAT_EXISTS_FLAG | gdal.VSI_STAT_NATURE_FLAG | gdal.VSI_STAT_SIZE_FLAG)
    if statBuf is not None:
        gdaltest.post_reason('did not expect PAM file')
        return 'fail'
    src_ds = gdal.Open('/vsimem/tiff_write_123_src.tif')
    if src_ds.GetRasterBand(1).GetColorInterpretation() != gdal.GCI_RedBand:
        gdaltest.post_reason('fail')
        return 'fail'

    new_ds = gdaltest.tiff_drv.CreateCopy('/vsimem/tiff_write_123.tif', src_ds)
    new_ds = None
    statBuf = gdal.VSIStatL('/vsimem/tiff_write_123.tif.aux.xml', gdal.VSI_STAT_EXISTS_FLAG | gdal.VSI_STAT_NATURE_FLAG | gdal.VSI_STAT_SIZE_FLAG)
    if statBuf is not None:
        gdaltest.post_reason('did not expect PAM file')
        return 'fail'
    ds = gdal.Open('/vsimem/tiff_write_123.tif')
    if ds.GetRasterBand(1).GetColorInterpretation() != gdal.GCI_RedBand:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    gdaltest.tiff_drv.Delete('/vsimem/tiff_write_123_src.tif')
    gdaltest.tiff_drv.Delete('/vsimem/tiff_write_123.tif')
    
    ds = gdaltest.tiff_drv.Create('/vsimem/tiff_write_123_bgr.tif', 1,1,3,gdal.GDT_Byte)
    ds.GetRasterBand(1).SetColorInterpretation(gdal.GCI_BlueBand)
    if ds.GetRasterBand(1).GetColorInterpretation() != gdal.GCI_BlueBand:
        gdaltest.post_reason('fail')
        return 'fail'
    ds.GetRasterBand(2).SetColorInterpretation(gdal.GCI_GreenBand)
    ds.GetRasterBand(3).SetColorInterpretation(gdal.GCI_RedBand)
    ds = None
    statBuf = gdal.VSIStatL('/vsimem/tiff_write_123_bgr.tif.aux.xml', gdal.VSI_STAT_EXISTS_FLAG | gdal.VSI_STAT_NATURE_FLAG | gdal.VSI_STAT_SIZE_FLAG)
    if statBuf is None:
        gdaltest.post_reason('expected PAM file')
        return 'fail'
    ds = gdal.Open('/vsimem/tiff_write_123_bgr.tif')
    if ds.GetRasterBand(1).GetColorInterpretation() != gdal.GCI_BlueBand:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None
    gdaltest.tiff_drv.Delete('/vsimem/tiff_write_123_bgr.tif')

    return 'success'

###############################################################################
# Test error cases with palette creation

def tiff_write_124():

    ds = gdaltest.tiff_drv.Create('/vsimem/tiff_write_124.tif', 1,1,3,gdal.GDT_Byte)

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    # Test "SetColorTable() can only be called on band 1"
    ret = ds.GetRasterBand(2).SetColorTable(gdal.ColorTable())
    gdal.PopErrorHandler()
    if ret == 0:
        return 'fail'

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    # Test "SetColorTable() not supported for multi-sample TIFF files"
    ret = ds.GetRasterBand(1).SetColorTable(gdal.ColorTable())
    gdal.PopErrorHandler()
    if ret == 0:
        return 'fail'

    ds = None

    ds = gdaltest.tiff_drv.Create('/vsimem/tiff_write_124.tif', 1,1,1, gdal.GDT_UInt32)
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    # Test "SetColorTable() only supported for Byte or UInt16 bands in TIFF format."
    ret = ds.GetRasterBand(1).SetColorTable(gdal.ColorTable())
    gdal.PopErrorHandler()
    if ret == 0:
        return 'fail'
    ds = None

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    # Test "SetColorTable() only supported for Byte or UInt16 bands in TIFF format."
    ds = gdaltest.tiff_drv.Create('/vsimem/tiff_write_124.tif', 1,1,1, gdal.GDT_UInt32, options = ['PHOTOMETRIC=PALETTE'])
    gdal.PopErrorHandler()
    ds = None

    gdaltest.tiff_drv.Delete('/vsimem/tiff_write_124.tif')

    return 'success'

###############################################################################
# Test out-of-memory conditions with SplitBand and SplitBitmapBand

def tiff_write_125():

    ds = gdaltest.tiff_drv.Create('/vsimem/tiff_write_125.tif', 2147000000, 5000, 65535, options = ['SPARSE_OK=YES', 'BLOCKYSIZE=5000', 'COMPRESS=LZW', 'BIGTIFF=NO'])
    ds = None

    ds = gdal.Open('/vsimem/tiff_write_125.tif')
    # Will not open on 32-bit due to overflow
    if ds is not None:
        gdal.PushErrorHandler('CPLQuietErrorHandler')
        ds.GetRasterBand(1).ReadBlock(0,0)
        gdal.PopErrorHandler()


    ds = gdal.GetDriverByName('GTiff').Create('/vsimem/tiff_write_125.tif', 2147000000, 5000, 1, options = ['NBITS=1', 'SPARSE_OK=YES', 'BLOCKYSIZE=5000', 'COMPRESS=LZW', 'BIGTIFF=NO'])
    ds = None

    ds = gdal.Open('/vsimem/tiff_write_125.tif')
    # Will not open on 32-bit due to overflow
    if ds is not None:
        gdal.PushErrorHandler('CPLQuietErrorHandler')
        ds.GetRasterBand(1).ReadBlock(0,0)
        gdal.PopErrorHandler()

    gdaltest.tiff_drv.Delete('/vsimem/tiff_write_125.tif')

    return 'success'

###############################################################################
# Test implicit JPEG-in-TIFF overviews

def tiff_write_126():

    md = gdaltest.tiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('JPEG') == -1:
        return 'skip'

    src_ds = gdal.Open('../gdrivers/data/small_world_400pct.vrt')

    options_list = [ (['COMPRESS=JPEG', 'PHOTOMETRIC=YCBCR'], [48788,56561], [61397,2463], [29605,33654], [10904,10453]),
                     (['COMPRESS=JPEG', 'PHOTOMETRIC=YCBCR', 'TILED=YES'], [48788,56561], [61397,2463], [29605,33654], [10904,10453]),
                     (['COMPRESS=JPEG', 'PHOTOMETRIC=YCBCR', 'BLOCKYSIZE=800'], [48788,56561], [61397,2463], [29605,33654], [10904,10453]),
                     (['COMPRESS=JPEG', 'PHOTOMETRIC=YCBCR', 'BLOCKYSIZE=64'], [48788,56561], [61397,2463], [29605,33654], [10904,10453]),
                     (['COMPRESS=JPEG'], [49887,58937], [59311,2826], [30829,34806], [11664,58937]),
                     (['COMPRESS=JPEG', 'INTERLEAVE=BAND'], [49887,58937], [59311,2826], [30829,34806], [11664,58937]),
                     (['COMPRESS=JPEG', 'INTERLEAVE=BAND', 'TILED=YES'], [49887,58937], [59311,2826], [30829,34806], [11664,58937]),
                     (['COMPRESS=JPEG', 'INTERLEAVE=BAND', 'BLOCKYSIZE=800'], [49887,58937], [59311,2826], [30829,34806], [11664,58937]),
                     (['COMPRESS=JPEG', 'INTERLEAVE=BAND', 'BLOCKYSIZE=32'], [49887,58937], [59311,2826], [30829,34806], [11664,58937]),
                   ]

    for (options, cs1, cs2, cs3, cs4) in options_list:
        ds = gdaltest.tiff_drv.CreateCopy('/vsimem/tiff_write_126.tif', src_ds, options = options)
        ds = None

        ds = gdal.Open('/vsimem/tiff_write_126.tif')
        # Officially we have 0 public overviews...
        if ds.GetRasterBand(1).GetOverviewCount() != 0:
            print(options)
            print(ds.GetRasterBand(1).GetOverviewCount())
            gdaltest.post_reason('fail')
            return 'fail'
        # But they do exist...
        cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
        if not(cs in cs1):
            print(options)
            print(cs)
            gdaltest.post_reason('fail')
            return 'fail'
        cs = ds.GetRasterBand(2).GetOverview(0).Checksum()
        if not(cs in cs2):
            print(options)
            print(cs)
            gdaltest.post_reason('fail')
            return 'fail'
        cs = ds.GetRasterBand(1).GetOverview(1).Checksum()
        if not(cs in cs3):
            print(options)
            print(cs)
            gdaltest.post_reason('fail')
            return 'fail'
        cs = ds.GetRasterBand(1).GetOverview(2).Checksum()
        if not(cs in cs4):
            print(options)
            print(cs)
            gdaltest.post_reason('fail')
            return 'fail'
        if ds.GetRasterBand(1).GetOverview(-1) is not None:
            print(options)
            gdaltest.post_reason('fail')
            return 'fail'
        if ds.GetRasterBand(1).GetOverview(3) is not None:
            print(options)
            gdaltest.post_reason('fail')
            return 'fail'
        ovr_1_data = ds.GetRasterBand(1).GetOverview(1).GetDataset().ReadRaster(0,0,400,200)
        subsampled_data = ds.ReadRaster(0,0,1600,800,400,200)
        if ovr_1_data != subsampled_data:
            print(options)
            gdaltest.post_reason('fail')
            return 'fail'
        ds = None

        gdaltest.tiff_drv.Delete('/vsimem/tiff_write_126.tif')

    src_ds = gdal.Open('../gdrivers/data/small_world_400pct_1band.vrt')

    options_list = [ (['COMPRESS=JPEG'], [49887,58937], [30829,34806], [11664,58937]),
                     (['COMPRESS=JPEG', 'TILED=YES'], [49887,58937], [30829,34806], [11664,58937]),
                     (['COMPRESS=JPEG', 'BLOCKYSIZE=800'], [49887,58937], [30829,34806], [11664,58937]),
                     (['COMPRESS=JPEG', 'BLOCKYSIZE=32'], [49887,58937], [30829,34806], [11664,58937]),
                   ]

    for (options, cs1, cs3, cs4) in options_list:
        ds = gdaltest.tiff_drv.CreateCopy('/vsimem/tiff_write_126.tif', src_ds, options = options)
        ds = None

        ds = gdal.Open('/vsimem/tiff_write_126.tif')
        # Officially we have 0 public overviews...
        if ds.GetRasterBand(1).GetOverviewCount() != 0:
            print(options)
            print(ds.GetRasterBand(1).GetOverviewCount())
            gdaltest.post_reason('fail')
            return 'fail'
        # But they do exist...
        cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
        if not(cs in cs1):
            print(options)
            print(cs)
            gdaltest.post_reason('fail')
            return 'fail'
        cs = ds.GetRasterBand(1).GetOverview(1).Checksum()
        if not(cs in cs3):
            print(options)
            print(cs)
            gdaltest.post_reason('fail')
            return 'fail'
        cs = ds.GetRasterBand(1).GetOverview(2).Checksum()
        if not(cs in cs4):
            print(options)
            print(cs)
            gdaltest.post_reason('fail')
            return 'fail'
        ovr_1_data = ds.GetRasterBand(1).GetOverview(1).GetDataset().ReadRaster(0,0,400,200)
        subsampled_data = ds.ReadRaster(0,0,1600,800,400,200)
        if ovr_1_data != subsampled_data:
            print(options)
            gdaltest.post_reason('fail')
            return 'fail'
        ds = None

        gdaltest.tiff_drv.Delete('/vsimem/tiff_write_126.tif')

    # Test single-strip, opened as split band
    src_ds = gdaltest.tiff_drv.Create('/vsimem/tiff_write_126_src.tif', 8, 2001)
    src_ds.GetRasterBand(1).Fill(255)
    ds = gdaltest.tiff_drv.CreateCopy('/vsimem/tiff_write_126.tif', src_ds, options = ['COMPRESS=JPEG', 'BLOCKYSIZE=2001'])
    src_ds = None
    gdaltest.tiff_drv.Delete('/vsimem/tiff_write_126_src.tif')
    ds = None

    ds = gdal.Open('/vsimem/tiff_write_126.tif')
    if ds.GetRasterBand(1).GetBlockSize() != [8,1]:
        print(ds.GetRasterBand(1).GetBlockSize())
        gdaltest.post_reason('fail')
        return 'fail'
    ovr_ds = ds.GetRasterBand(1).GetOverview(1).GetDataset()
    ovr_1_data = ovr_ds.ReadRaster(0,0,ovr_ds.RasterXSize,ovr_ds.RasterYSize,1,1)
    subsampled_data = ds.ReadRaster(0,0,ds.RasterXSize,ds.RasterYSize,1,1)
    if ovr_1_data != subsampled_data:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    gdaltest.tiff_drv.Delete('/vsimem/tiff_write_126.tif')

    # Test with completely sparse file
    ds = gdaltest.tiff_drv.Create('/vsimem/tiff_write_126.tif', 1024, 1024, options = ['COMPRESS=JPEG', 'SPARSE_OK=YES'])
    ds = None

    ds = gdal.Open('/vsimem/tiff_write_126.tif')
    # We don't even have JPEGTABLES !
    if ds.GetRasterBand(1).GetOverview(0) is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetRasterBand(1).GetMetadataItem('JPEGTABLES', 'TIFF') is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetRasterBand(1).GetMetadataItem('BLOCK_OFFSET_0_0', 'TIFF') is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetRasterBand(1).GetMetadataItem('BLOCK_SIZE_0_0', 'TIFF') is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None
    gdaltest.tiff_drv.Delete('/vsimem/tiff_write_126.tif')

    # Test with partially sparse file
    ds = gdaltest.tiff_drv.Create('/vsimem/tiff_write_126.tif', 1024, 1024, 3, options = ['COMPRESS=JPEG', 'SPARSE_OK=YES', 'INTERLEAVE=BAND'])
    # Fill band 3, but let blocks of band 1 unwritten.
    ds.GetRasterBand(3).Fill(0)
    ds = None

    ds = gdal.Open('/vsimem/tiff_write_126.tif')
    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    if cs != 0:
        print(cs)
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None
    gdaltest.tiff_drv.Delete('/vsimem/tiff_write_126.tif')

    return 'success'

###############################################################################
# Ask to run again tests with GDAL_API_PROXY=YES

def tiff_write_api_proxy():

    if not run_tiff_write_api_proxy:
        return 'skip'

    import test_py_scripts
    ret = test_py_scripts.run_py_script_as_external_script('.', 'tiff_write', ' -api_proxy', display_live_on_parent_stdout = True)

    if ret.find('Failed:    0') == -1:
        return 'fail'

    return 'success'

###############################################################################
def tiff_write_cleanup():
    gdaltest.tiff_drv = None

    if 0 == gdal.GetCacheMax():
        gdal.SetCacheMax(gdaltest.oldCacheSize)

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
    tiff_write_11,
    tiff_write_12,
    tiff_write_13,
    tiff_write_14,
    tiff_write_15,
    tiff_write_16,
    tiff_write_17,
    tiff_write_17_disable_readdir,
    tiff_write_18,
    tiff_write_18_disable_readdir,
    tiff_write_19,
    tiff_write_20,
    tiff_write_21,
    tiff_write_22,
    tiff_write_23,
    tiff_write_24,
    tiff_write_25,
    tiff_write_26,
    tiff_write_27,
    tiff_write_28,
    tiff_write_29,
    tiff_write_30,
    tiff_write_31,
    tiff_write_32,
    tiff_write_33,
    tiff_write_34,
    tiff_write_35,
    tiff_write_36,
    tiff_write_37,
    tiff_write_38,
    tiff_write_39,
    tiff_write_40,
    tiff_write_41,
    tiff_write_42,
    tiff_write_43,
    tiff_write_44,
    tiff_write_45,
    tiff_write_46,
    tiff_write_47,
    tiff_write_48,
    tiff_write_49,
    tiff_write_50,
    tiff_write_51,
    tiff_write_52,
    tiff_write_53,
    tiff_write_53_bis,
    tiff_write_54,
    tiff_write_55,
    tiff_write_56,
    tiff_write_57,
    tiff_write_58,
    tiff_write_59,
    tiff_write_60,
    tiff_write_61,
    tiff_write_62,
    tiff_write_63,
    tiff_write_64,
    tiff_write_65,
    tiff_write_66,
    tiff_write_67,
    tiff_write_68,
    tiff_write_69,
    tiff_write_70,
    tiff_write_71,
    tiff_write_72,
    tiff_write_73,
    tiff_write_74,
    tiff_write_75,
    tiff_write_76,
    tiff_write_77,
    tiff_write_78,
    tiff_write_79,
    tiff_write_80,
    tiff_write_81,
    tiff_write_82,
    tiff_write_83,
    tiff_write_84,
    tiff_write_85,
    tiff_write_86,
    tiff_write_87,
    tiff_write_88,
    tiff_write_89, # leaks mem
    tiff_write_90, # leaks mem
    tiff_write_91, # leaks mem
    tiff_write_92, # leaks mem
    tiff_write_93, # leaks mem
    tiff_write_94,  # leaks mem
    tiff_write_95,
    tiff_write_96,  # leaks mem
    tiff_write_97,
    tiff_write_98,
    tiff_write_99,
    tiff_write_100,
    tiff_write_101,
    tiff_write_102,
    tiff_write_103,
    tiff_write_104,
    tiff_write_105,
    tiff_write_106,
    tiff_write_107,
    tiff_write_108,
    tiff_write_109,
    tiff_write_110,
    tiff_write_111,
    tiff_write_111_bis,
    tiff_write_111_ter,
    tiff_write_112,
    tiff_write_113,
    tiff_write_114,
    tiff_write_115,
    tiff_write_116,
    tiff_write_117,
    tiff_write_118,
    tiff_write_119,
    tiff_write_120,
    tiff_write_121,
    tiff_write_122,
    tiff_write_123,
    tiff_write_124,
    tiff_write_125,
    tiff_write_126,
    #tiff_write_api_proxy,
    tiff_write_cleanup ]

if __name__ == '__main__':

    if len(sys.argv) >= 2 and sys.argv[1] == '-api_proxy':
        run_tiff_write_api_proxy = False
        gdal.SetConfigOption('GDAL_API_PROXY', 'YES')

    gdaltest.setup_run( 'tiff_write' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

