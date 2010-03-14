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
        gdalnumeric.zeros
    except:
        return 'skip'

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

    if string.find(new_ds.GetGCPProjection(),
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

def tiff_write_6():

    options= [ 'TILED=YES', 'BLOCKXSIZE=32', 'BLOCKYSIZE=32',
               'COMPRESS=DEFLATE', 'PREDICTOR=2' ]
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

    drv = gdal.GetDriverByName( 'GTiff' )
    md = drv.GetMetadata()
    if string.find(md['DMD_CREATIONOPTIONLIST'],'BigTIFF') == -1:
        return 'skip'

    options= [ 'TILED=YES', 'COMPRESS=LZW', 'PREDICTOR=2' ]
    ds = gdaltest.tiff_drv.Create( 'tmp/test_7.tif', 200, 200, 1,
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

    gdaltest.tiff_drv.Delete( 'tmp/test_7.tif' )

    return 'success'

###############################################################################
# Test a mixture of reading and writing on a PACKBITS compressed file.

def tiff_write_8():

    options= [ 'TILED=YES', 'BLOCKXSIZE=32', 'BLOCKYSIZE=32', 'COMPRESS=PACKBITS' ]
    ds = gdaltest.tiff_drv.Create( 'tmp/test_8.tif', 200, 200, 1,
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

    drv = gdal.GetDriverByName( 'GTiff' )
    md = drv.GetMetadata()
    if string.find(md['DMD_CREATIONOPTIONLIST'],'JPEG') == -1:
        return 'skip'
    
    ut = gdaltest.GDALTest( 'GTiff', 'sasha.tif', 3, 31952 )
                           
    return ut.testOpen()

###############################################################################
# Write JPEG Compressed YCbCr subsampled image. 

def tiff_write_13():

    drv = gdal.GetDriverByName( 'GTiff' )
    md = drv.GetMetadata()
    if string.find(md['DMD_CREATIONOPTIONLIST'],'JPEG') == -1:
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
    
    ds = drv.CreateCopy( 'tmp/tw_15.tif', ds_in, options=['PROFILE=BASELINE'] )

    ds_in = None
    ds = None

    ds = gdal.Open( 'tmp/tw_15.tif' )
    if ds.GetGeoTransform() != (0.0,1.0,0.0,0.0,0.0,1.0):
        gdaltest.post_reason( 'Got wrong geotransform, profile ignored?' )
        return 'fail'

    md = ds.GetMetadata()
    if not md.has_key('test'):
        gdaltest.post_reason( 'Metadata absent from .aux.xml file.' )
        return 'fail'

    md = ds.GetRasterBand(1).GetMetadata()
    if not md.has_key('testBand'):
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
    if md.has_key('test'):
        gdaltest.post_reason( 'Metadata written to BASELINE file.' )
        return 'fail'

    md = ds.GetRasterBand(1).GetMetadata()
    if md.has_key('testBand'):
        gdaltest.post_reason( 'Metadata written to BASELINE file.' )
        return 'fail'

    ds = None

    drv.Delete( 'tmp/tw_15.tif' )

    return 'success'

###############################################################################
# Test that we can restrict metadata and georeferencing in the output
# file using the PROFILE creation option with Create()

def tiff_write_16():

    drv = gdal.GetDriverByName( 'GTiff' )

    ds_in = gdal.Open('data/byte.vrt')
    
    ds = drv.Create( 'tmp/tw_16.tif', 20, 20, gdal.GDT_Byte,
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
    if not md.has_key('test'):
        gdaltest.post_reason( 'Metadata absent from .aux.xml file.' )
        return 'fail'

    md = ds.GetRasterBand(1).GetMetadata()
    if not md.has_key('testBand'):
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
    if md.has_key('test'):
        gdaltest.post_reason( 'Metadata written to BASELINE file.' )
        return 'fail'

    md = ds.GetRasterBand(1).GetMetadata()
    if md.has_key('testBand'):
        gdaltest.post_reason( 'Metadata written to BASELINE file.' )
        return 'fail'

    ds = None

    drv.Delete( 'tmp/tw_16.tif' )

    return 'success'

###############################################################################
# Test writing a TIFF with an RPC tag. 

def tiff_write_17():

    # Translate RPC controlled data to GeoTIFF.

    drv = gdal.GetDriverByName( 'GTiff' )

    ds_in = gdal.Open('data/rpc.vrt')
    rpc_md = ds_in.GetMetadata('RPC')
    
    ds = drv.CreateCopy( 'tmp/tw_17.tif', ds_in )
    
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

    drv.Delete( 'tmp/tw_17.tif' )

    return 'success'

###############################################################################
# Test writing a TIFF with an RPB file and IMD file.

def tiff_write_18():

    # Translate RPC controlled data to GeoTIFF.

    drv = gdal.GetDriverByName( 'GTiff' )

    ds_in = gdal.Open('data/rpc.vrt')
    rpc_md = ds_in.GetMetadata('RPC')
    
    ds = drv.CreateCopy( 'tmp/tw_18.tif', ds_in,
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
        print imd_md
        return 'fail'

    ds = None

    drv.Delete( 'tmp/tw_18.tif' )

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
        if not md.has_key(item[0]):
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

    drv = gdal.GetDriverByName( 'GTiff' )
    md = drv.GetMetadata()
    if string.find(md['DMD_CREATIONOPTIONLIST'],'BigTIFF') == -1:
        return 'skip'

    ds = gdaltest.tiff_drv.Create( 'tmp/bigtiff.tif', 1, 1, 1, options = ['BigTIFF=YES'] )
    ds = None

    ds = gdal.Open( 'tmp/bigtiff.tif' )
    if ds is None:
        return 'fail'
    ds = None

    fileobj = open( 'tmp/bigtiff.tif', mode='rb')
    binvalues = array.array('b')
    binvalues.read(fileobj, 4)
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

    drv = gdal.GetDriverByName( 'GTiff' )
    md = drv.GetMetadata()
    if string.find(md['DMD_CREATIONOPTIONLIST'],'BigTIFF') == -1:
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
    binvalues.read(fileobj, 4)
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

    drv = gdal.GetDriverByName( 'GTiff' )

    ds_in = gdal.Open('data/byte.vrt')

    # Test creation
    ds = drv.Create( 'tmp/byte_rotated.tif', 20, 20, gdal.GDT_Byte )

    gt = (10,3.53553390593,3.53553390593,30,3.53553390593,-3.53553390593)
    ds.SetGeoTransform( gt )

    data = ds_in.ReadRaster( 0, 0, 20, 20 )
    ds.WriteRaster( 0, 0, 20, 20, data )

    ds_in = None

    # Test copy
    new_ds = drv.CreateCopy( 'tmp/byte_rotated_copy.tif', ds )
    new_ds = None

    # Check copy
    ds = gdal.Open( 'tmp/byte_rotated_copy.tif' )
    new_gt = ds.GetGeoTransform()
    for i in range(6):
        if abs(new_gt[i]-gt[i]) > 1e-5:
            print
            print 'old = ', gt
            print 'new = ', new_gt
            gdaltest.post_reason( 'Geotransform differs.' )
            return 'fail'

    ds = None
    new_ds = None

    drv.Delete( 'tmp/byte_rotated.tif' )
    drv.Delete( 'tmp/byte_rotated_copy.tif' )

    return 'success'

###############################################################################
# Test that metadata is written in .aux.xml file in GeoTIFF profile with CreateCopy
# (BASELINE is tested by tiff_write_15)

def tiff_write_33():

    drv = gdal.GetDriverByName( 'GTiff' )

    ds_in = gdal.Open('data/byte.vrt')

    ds = drv.CreateCopy( 'tmp/tw_33.tif', ds_in, options=['PROFILE=GeoTIFF'] )

    ds_in = None

    ds = None

    ds = gdal.Open( 'tmp/tw_33.tif' )

    md = ds.GetMetadata()
    if not md.has_key('test'):
        gdaltest.post_reason( 'Metadata absent from .aux.xml file.' )
        return 'fail'

    md = ds.GetRasterBand(1).GetMetadata()
    if not md.has_key('testBand'):
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
    if md.has_key('test'):
        gdaltest.post_reason( 'Metadata written to GeoTIFF file.' )
        return 'fail'

    md = ds.GetRasterBand(1).GetMetadata()
    if md.has_key('testBand'):
        gdaltest.post_reason( 'Metadata written to GeoTIFF file.' )
        return 'fail'

    ds = None

    drv.Delete( 'tmp/tw_33.tif' )

    return 'success'

###############################################################################
# Test that metadata is written in .aux.xml file in GeoTIFF profile with Create
# (BASELINE is tested by tiff_write_16)

def tiff_write_34():

    drv = gdal.GetDriverByName( 'GTiff' )

    ds = drv.Create( 'tmp/tw_34.tif', 1, 1, gdal.GDT_Byte,
                     options=['PROFILE=GeoTIFF'] )
    ds.SetMetadata( {'test':'testvalue'} )
    ds.GetRasterBand(1).SetMetadata( {'testBand':'testvalueBand'} )

    ds = None

    ds = gdal.Open( 'tmp/tw_34.tif' )

    md = ds.GetMetadata()
    if not md.has_key('test'):
        gdaltest.post_reason( 'Metadata absent from .aux.xml file.' )
        return 'fail'

    md = ds.GetRasterBand(1).GetMetadata()
    if not md.has_key('testBand'):
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
    if md.has_key('test'):
        gdaltest.post_reason( 'Metadata written to GeoTIFF file.' )
        return 'fail'

    md = ds.GetRasterBand(1).GetMetadata()
    if md.has_key('testBand'):
        gdaltest.post_reason( 'Metadata written to GeoTIFF file.' )
        return 'fail'

    ds = None

    drv.Delete( 'tmp/tw_34.tif' )

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

    drv = gdal.GetDriverByName( 'GTiff' )

    ds = drv.Create( 'tmp/tw_35.tif', 1, 1, gdal.GDT_Byte )

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
    if not md.has_key('test') or len(md['test']) != 32768:
        gdaltest.post_reason( 'Did not get expected metadata.' )
        return 'fail'

    ds = None

    drv.Delete( 'tmp/tw_35.tif' )

    return 'success'

###############################################################################
# Generic functions for the 8 following tests

def tiff_write_big_odd_bits(vrtfilename, tmpfilename, nbits, interleaving):
    drv = gdal.GetDriverByName( 'GTiff' )

    ds_in = gdal.Open(vrtfilename)

    ds = drv.CreateCopy( tmpfilename, ds_in, options = [ 'NBITS=' + str(nbits), 'INTERLEAVE='+ interleaving ] )

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

    drv.Delete( tmpfilename )

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
    drv = gdal.GetDriverByName( 'GTiff' )
    ds = drv.Create( 'tmp/tw_44.tif', 1, 1, 1, gdal.GDT_UInt16, options = [ 'NBITS=9' ] )
    ds = None
    ds = gdal.Open( 'tmp/tw_44.tif' )
    bnd = ds.GetRasterBand(1)
    md = bnd.GetMetadata('IMAGE_STRUCTURE')
    if md['NBITS'] != '9':
        gdaltest.post_reason( 'Didnt get expected NBITS value')
        return 'fail'

    ds2 = drv.CreateCopy( 'tmp/tw_44_copy.tif', ds )
    ds2 = None

    ds2 = gdal.Open('tmp/tw_44_copy.tif')
    bnd = ds.GetRasterBand(1)
    md = bnd.GetMetadata('IMAGE_STRUCTURE')
    if md['NBITS'] != '9':
        gdaltest.post_reason( 'Didnt get expected NBITS value')
        return 'fail'

    ds = None
    ds2 = None

    drv.Delete( 'tmp/tw_44.tif' )
    drv.Delete( 'tmp/tw_44_copy.tif' )

    return 'success'

###############################################################################
# Test create with NBITS=17 and preservation through CreateCopy of NBITS

def tiff_write_45():
    drv = gdal.GetDriverByName( 'GTiff' )
    ds = drv.Create( 'tmp/tw_45.tif', 1, 1, 1, gdal.GDT_UInt32, options = [ 'NBITS=17' ] )
    ds = None
    ds = gdal.Open( 'tmp/tw_45.tif' )
    bnd = ds.GetRasterBand(1)
    md = bnd.GetMetadata('IMAGE_STRUCTURE')
    if md['NBITS'] != '17':
        gdaltest.post_reason( 'Didnt get expected NBITS value')
        return 'fail'

    ds2 = drv.CreateCopy( 'tmp/tw_45_copy.tif', ds )
    ds2 = None

    ds2 = gdal.Open('tmp/tw_45_copy.tif')
    bnd = ds.GetRasterBand(1)
    md = bnd.GetMetadata('IMAGE_STRUCTURE')
    if md['NBITS'] != '17':
        gdaltest.post_reason( 'Didnt get expected NBITS value')
        return 'fail'

    ds = None
    ds2 = None

    drv.Delete( 'tmp/tw_45.tif' )
    drv.Delete( 'tmp/tw_45_copy.tif' )

    return 'success'


###############################################################################
# Test correct round-tripping of ReadBlock/WriteBlock

def tiff_write_46():
    import struct

    drv = gdal.GetDriverByName( 'GTiff' )

    oldSize = gdal.GetCacheMax()
    gdal.SetCacheMax(0)

    ds = drv.Create("tmp/tiff_write_46_1.tif", 10, 10, 1, options = [ 'NBITS=1' ])
    ds.GetRasterBand(1).Fill(0)

    ds2 = drv.Create("tmp/tiff_write_46_2.tif", 10, 10, 1, options = [ 'NBITS=1' ])
    ds2.GetRasterBand(1).Fill(1)
    ones = ds2.ReadRaster(0, 0, 10, 1)

    # Load the working block
    data = ds.ReadRaster(0, 0, 10, 1)

    # Write the working bloc
    ds.WriteRaster(0, 0, 10, 1, ones)

    # This will discard the cached block for ds
    ds3 = drv.Create("tmp/tiff_write_46_3.tif", 10, 10, 1)
    ds3.GetRasterBand(1).Fill(1)

    # Load the working block again
    data = ds.ReadRaster(0, 0, 10, 1)

    # We expect (1, 1, 1, 1, 1, 1, 1, 1, 1, 1)
    got = struct.unpack('B' * 10, data)
    for i in range(len(got)):
        if got[i] != 1:
            print got
            gdal.SetCacheMax(oldSize)
            return 'fail'

    gdal.SetCacheMax(oldSize)

    ds = None
    ds2 = None
    ds3 = None
    drv.Delete( 'tmp/tiff_write_46_1.tif' )
    drv.Delete( 'tmp/tiff_write_46_2.tif' )
    drv.Delete( 'tmp/tiff_write_46_3.tif' )

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

    drv = gdal.GetDriverByName( 'GTiff' )

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

    drv.Delete( 'tmp/tiff_write_48.tif' )

    return 'success'


###############################################################################
# Test copying a CMYK TIFF into another CMYK TIFF

def tiff_write_49():

    drv = gdal.GetDriverByName( 'GTiff' )

    # We open the source as RAW to get the CMYK bands
    src_ds = gdal.Open( 'GTIFF_RAW:data/rgbsmall_cmyk.tif' )

    new_ds = gdal.GetDriverByName("GTiff").CreateCopy('tmp/tiff_write_49.tif', src_ds, options = [ 'PHOTOMETRIC=CMYK' ])

    # At this point, for the purpose of the copy, the dataset will have been opened as RAW
    if new_ds.GetRasterBand(1).GetRasterColorInterpretation()!= gdal.GCI_CyanBand:
        gdaltest.post_reason( 'Wrong color interpretation.')
        print ds.GetRasterBand(1).GetRasterColorInterpretation()
        return 'fail'

    new_ds = None

    new_ds = gdal.Open('GTIFF_RAW:tmp/tiff_write_49.tif')

    for i in range(4):
        if new_ds.GetRasterBand(i + 1).Checksum() != src_ds.GetRasterBand(i + 1).Checksum():
            gdaltest.post_reason( 'Didnt get expected checksum ')
            return 'fail'

    src_ds = None
    new_ds = None

    drv.Delete( 'tmp/tiff_write_49.tif' )

    return 'success'


###############################################################################
# Test creating a CMYK TIFF from another CMYK TIFF

def tiff_write_50():

    drv = gdal.GetDriverByName( 'GTiff' )

    # We open the source as RAW to get the CMYK bands
    src_ds = gdal.Open( 'GTIFF_RAW:data/rgbsmall_cmyk.tif' )

    new_ds = gdal.GetDriverByName("GTiff").Create('tmp/tiff_write_50.tif', src_ds.RasterXSize, src_ds.RasterYSize, 4, options = [ 'PHOTOMETRIC=CMYK' ])
    for i in range(4):
        data = src_ds.GetRasterBand(i+1).ReadRaster(0, 0, src_ds.RasterXSize, src_ds.RasterYSize)
        new_ds.GetRasterBand(i+1).WriteRaster(0, 0, src_ds.RasterXSize, src_ds.RasterYSize, data)

    if new_ds.GetRasterBand(1).GetRasterColorInterpretation()!= gdal.GCI_CyanBand:
        gdaltest.post_reason( 'Wrong color interpretation.')
        print ds.GetRasterBand(1).GetRasterColorInterpretation()
        return 'fail'

    new_ds = None

    new_ds = gdal.Open('GTIFF_RAW:tmp/tiff_write_50.tif')

    for i in range(4):
        if new_ds.GetRasterBand(i + 1).Checksum() != src_ds.GetRasterBand(i + 1).Checksum():
            gdaltest.post_reason( 'Didnt get expected checksum ')
            return 'fail'

    src_ds = None
    new_ds = None

    drv.Delete( 'tmp/tiff_write_50.tif' )

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
    ds = gdal.GetDriverByName('GTiff').Create( 'tmp/tiff_write_51_ref.tif', 1, 1, 1 )
    ds.SetProjection(srs.ExportToWkt())
    ds = None

    # Read it back as the reference WKT
    ds = gdal.Open( 'tmp/tiff_write_51_ref.tif' )
    expected_wkt = ds.GetProjection()
    ds = None

    if string.find(wkt,'NAD') != -1 or string.find(wkt,'North Am') != -1:
        gdaltest.post_reason( 'It appears the NAD27 datum was not properly cleared.' )
        return 'fail'
    
    if wkt != expected_wkt or string.find(wkt, 'WGS 84 / UTM zone 1N') == -1:
        print wkt
        print expected_wkt
        gdaltest.post_reason( 'coordinate system does not exactly match.' )
        return 'fail'

    ds = None

    gdal.GetDriverByName('GTiff').Delete( 'tmp/tiff_write_51.tif' )
    gdal.GetDriverByName('GTiff').Delete( 'tmp/tiff_write_51_ref.tif' )
    
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
        print ct.GetColorEntry(0)
        gdaltest.post_reason( 'Did not get expected color 0.' )
        return 'fail'

    ct = None
    ds = None

    gdal.GetDriverByName('GTiff').Delete( 'tmp/tiff_write_52.tif' )
    
    return 'success'

###############################################################################
# Test the ability to create a paletted image and then update later.

def tiff_write_53():
    test_ct_data = [ (255,0,0), (0,255,0), (0,0,255), (255,255,255,0)]

    test_ct = gdal.ColorTable()
    for i in range(len(test_ct_data)):
        test_ct.SetColorEntry( i, test_ct_data[i] )
        
    ds = gdal.GetDriverByName('GTiff').Create('tmp/tiff_write_53.tif',
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
        print ct.GetColorEntry(0)
        gdaltest.post_reason( 'Did not get expected color 0.' )
        return 'fail'

    ct = None
    ds = None

    gdal.GetDriverByName('GTiff').Delete( 'tmp/tiff_write_53.tif' )
    
    return 'success'


###############################################################################
# Test the ability to create a JPEG compressed TIFF, with PHOTOMETRIC=YCBCR
# and write data into it without closing it and re-opening it (#2645)

def tiff_write_54():

    ds = gdal.GetDriverByName('GTiff').Create('tmp/tiff_write_54.tif',
                                              256, 256, 3,
                                              options=['TILED=YES', 'COMPRESS=JPEG', 'PHOTOMETRIC=YCBCR'] )
    ds.GetRasterBand(1).Fill(0)
    ds.FlushCache()
    ds = None

    gdal.GetDriverByName('GTiff').Delete( 'tmp/tiff_write_54.tif' )

    return 'success'


###############################################################################
# Test creating and reading an equirectangular file with all parameters (#2706)

def tiff_write_55():

    ds = gdal.GetDriverByName('GTiff').Create('tmp/tiff_write_55.tif',
                                              256, 256, 1 )
    srs_expected = 'PROJCS["Equirectangular Mars",GEOGCS["GCS_Mars",DATUM["unknown",SPHEROID["unnamed",3394813.857975945,0]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433]],PROJECTION["Equirectangular"],PARAMETER["latitude_of_origin",-2],PARAMETER["central_meridian",184.4129943847656],PARAMETER["standard_parallel_1",-15],PARAMETER["false_easting",0],PARAMETER["false_northing",0],UNIT["metre",1,AUTHORITY["EPSG","9001"]]]'
    
    ds.SetProjection( srs_expected )

    ds.SetGeoTransform( (100,1,0,200,0,-1) )
    ds = None

    ds = gdal.Open( 'tmp/tiff_write_55.tif' )
    srs = ds.GetProjectionRef()
    ds = None

    if srs != srs_expected:
        print srs
        gdaltest.post_reason( 'failed to preserve Equirectangular projection as expected, old libgeotiff?' )
        return 'fail'

    gdal.GetDriverByName('GTiff').Delete( 'tmp/tiff_write_55.tif' )

    return 'success'


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
    tiff_write_54,
    tiff_write_55,
    tiff_write_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'tiff_write' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

