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
import shutil
import osr
import sys

sys.path.append( '../pymod' )

import gdaltest

###############################################################################
# Get the GeoTIFF driver, and verify a few things about it. 

def tiff_write_1():

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
        import gdalnumeric
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
    if bnd.Checksum() != 4084:
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
    
    ut = gdaltest.GDALTest( 'GTiff', 'sasha.tif', 3, 31952 )
                           
    return ut.testOpen()

###############################################################################
# Write JPEG Compressed YCbCr subsampled image. 

def tiff_write_13():

    md = gdaltest.tiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('JPEG') == -1:
        return 'skip'

    ut = gdaltest.GDALTest( 'GTiff', 'sasha.tif', 3, 17255,
                            options = [ 'TILED=YES',
                                        'COMPRESS=JPEG',
                                        'PHOTOMETRIC=YCBCR',
                                        'JPEG_QUALITY=31' ] )
                           
    return ut.testCreateCopy( skip_preclose_test=1 )

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
    if md['NBITS'] != '9':
        gdaltest.post_reason( 'Didnt get expected NBITS value')
        return 'fail'

    ds2 = gdaltest.tiff_drv.CreateCopy( 'tmp/tw_44_copy.tif', ds )
    ds2 = None

    ds2 = gdal.Open('tmp/tw_44_copy.tif')
    bnd = ds.GetRasterBand(1)
    md = bnd.GetMetadata('IMAGE_STRUCTURE')
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
    if md['NBITS'] != '17':
        gdaltest.post_reason( 'Didnt get expected NBITS value')
        return 'fail'

    ds2 = gdaltest.tiff_drv.CreateCopy( 'tmp/tw_45_copy.tif', ds )
    ds2 = None

    ds2 = gdal.Open('tmp/tw_45_copy.tif')
    bnd = ds.GetRasterBand(1)
    md = bnd.GetMetadata('IMAGE_STRUCTURE')
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

    ds = gdaltest.tiff_drv.Create('tmp/tiff_write_54.tif',
                                              256, 256, 3,
                                              options=['TILED=YES', 'COMPRESS=JPEG', 'PHOTOMETRIC=YCBCR'] )
    ds.GetRasterBand(1).Fill(0)
    ds.FlushCache()
    ds = None

    gdaltest.tiff_drv.Delete( 'tmp/tiff_write_54.tif' )

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
            os.remove( tuple[1] )
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
            os.remove( tuple[1] )
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

    drv = gdal.GetDriverByName('GTiff')
    dst_ds = drv.CreateCopy( 'tmp/test_74.tif', ds,
                             options = ['COMPRESS=JPEG', 'NBITS=12',
                                        'JPEG_QUALITY=95',
                                        'PHOTOMETRIC=YCBCR'] )

    ds = None
    dst_ds = None

    dst_ds = gdal.Open( 'tmp/test_74.tif' )
    stats = dst_ds.GetRasterBand(1).GetStatistics( 0, 1 )

    if stats[2] < 2150 or stats[2] > 2180:
        gdaltest.post_reason( 'did not get expected mean for band1.')
        print(stats)
        return 'fail'

    try:
        compression = dst_ds.GetMetadataItem('COMPRESSION','IMAGE_STRUCTURE')
    except:
        md = dst_ds.GetMetadata('IMAGE_STRUCTURE')
        compression = md['COMPRESSION']

    if compression != 'YCbCr JPEG':
        gdaltest.post_reason( 'did not get expected COMPRESSION value' )
        print(('COMPRESSION="%s"' % compression))
        return 'fail'

    try:
        nbits = dst_ds.GetRasterBand(3).GetMetadataItem('NBITS','IMAGE_STRUCTURE')
    except:
        md = dst_ds.GetRasterBand(3).GetMetadata('IMAGE_STRUCTURE')
        nbits = md['NBITS']

    if nbits != '12':
        gdaltest.post_reason( 'did not get expected NBITS value' )
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

    src_ds = gdaltest.tiff_drv.Create( 'tmp/tiff_write_78_src.tif', 16, 2048, 3 )
    src_ds.GetRasterBand(2).Fill(255)

    new_ds = gdaltest.tiff_drv.CreateCopy( 'tmp/tiff_write_78.tif', src_ds,
                                            options = ['BLOCKYSIZE=%d' % src_ds.RasterYSize,
                                                       'COMPRESS=JPEG',
                                                       'PHOTOMETRIC=YCBCR'] )
                             
    # Make sure the file is flushed so that we re-read from it rather from cached blocks
    new_ds.FlushCache()
    
    if 'GetBlockSize' in dir(gdal.Band):
        (blockx, blocky) = new_ds.GetRasterBand(1).GetBlockSize()
        if blocky == 1:
            print('using SplitBand')
        else:
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
            print(attempt)
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
        gdaltest.post_reason('did not get expected values in internal case')
        print(scale)
        print(value)
        return 'fail'
    ds = None

    gdaltest.tiff_drv.Delete( 'tmp/tiff_write_80.tif' )


    # Second part : test storing and retrieving scale & offsets from PAM metadata
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
        gdaltest.post_reason('did not get expected values in PAM case')
        print(scale)
        print(value)
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
def tiff_write_cleanup():
    gdaltest.tiff_drv = None

    if 0 == gdal.GetCacheMax():
        gdal.SetCacheMax(oldCacheSize)

    return 'success'

oldCacheSize = gdal.GetCacheMax()

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
    tiff_write_18,
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
    tiff_write_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'tiff_write' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

