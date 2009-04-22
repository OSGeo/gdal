#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test basic read support for a all datatypes from a TIFF file.
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
import string

sys.path.append( '../pymod' )

import gdaltest
import gdal

###############################################################################
# When imported build a list of units based on the files available.

gdaltest_list = []

init_list = [ \
    ('byte.tif', 1, 4672, None),
    ('int10.tif', 1, 4672, None),
    ('int12.tif', 1, 4672, None),
    ('int16.tif', 1, 4672, None),
    ('uint16.tif', 1, 4672, None),
    ('int24.tif', 1, 4672, None),
    ('int32.tif', 1, 4672, None),
    ('uint32.tif', 1, 4672, None),
    ('float16.tif', 1, 4672, None),
    ('float24.tif', 1, 4672, None),
    ('float32.tif', 1, 4672, None),
    ('float32_minwhite.tif', 1, 1, None),
    ('float64.tif', 1, 4672, None),
    ('cint16.tif', 1, 5028, None),
    ('cint32.tif', 1, 5028, None),
    ('cfloat32.tif', 1, 5028, None),
    ('cfloat64.tif', 1, 5028, None),
# The following four related partial final strip/tiles (#1179)
    ('separate_tiled.tif', 2, 15234, None), 
    ('seperate_strip.tif', 2, 15234, None),
    ('contig_tiled.tif', 2, 15234, None),
    ('contig_strip.tif', 2, 15234, None),
    ('empty1bit.tif', 1, 0, None)]

###############################################################################
# Test absolute/offset && index directory access

def tiff_read_off():

    # Test absolute/offset directory access 
    ds = gdal.Open('GTIFF_DIR:off:408:data/byte.tif')
    if ds.GetRasterBand(1).Checksum() != 4672:
        return 'fail'

    # Test index directory access
    ds = gdal.Open('GTIFF_DIR:1:data/byte.tif')
    if ds.GetRasterBand(1).Checksum() != 4672:
        return 'fail'

    return 'success'


###############################################################################
# Test reading a CMYK tiff as RGBA image

def tiff_read_cmyk_rgba():

    ds = gdal.Open('data/rgbsmall_cmyk.tif')

    md = ds.GetMetadata('IMAGE_STRUCTURE')
    if not md.has_key('SOURCE_COLOR_SPACE') or md['SOURCE_COLOR_SPACE'] != 'CMYK':
        print 'bad value for IMAGE_STRUCTURE[SOURCE_COLOR_SPACE]'
        return 'fail'

    if ds.GetRasterBand(1).GetRasterColorInterpretation()!= gdal.GCI_RedBand:
        gdaltest.post_reason( 'Wrong color interpretation.')
        print ds.GetRasterBand(1).GetRasterColorInterpretation()
        return 'fail'

    if ds.GetRasterBand(1).Checksum() != 23303:
        print 'Expected checksum = %d. Got = %d' % (23303, ds.GetRasterBand(1).Checksum())
        return 'fail'

    return 'success'

###############################################################################
# Test reading a CMYK tiff as a raw image

def tiff_read_cmyk_raw():

    ds = gdal.Open('GTIFF_RAW:data/rgbsmall_cmyk.tif')

    if ds.GetRasterBand(1).GetRasterColorInterpretation()!= gdal.GCI_CyanBand:
        gdaltest.post_reason( 'Wrong color interpretation.')
        print ds.GetRasterBand(1).GetRasterColorInterpretation()
        return 'fail'

    if ds.GetRasterBand(1).Checksum() != 29430:
        print 'Expected checksum = %d. Got = %d' % (29430, ds.GetRasterBand(1).Checksum())
        return 'fail'

    return 'success'


###############################################################################
# Read a .tif.gz file

def tiff_read_gzip():
    import shutil
    shutil.copy ('data/byte.tif.gz', 'tmp/byte.tif.gz')
    ds = gdal.Open('/vsigzip/./tmp/byte.tif.gz')
    if ds.GetRasterBand(1).Checksum() != 4672:
            print 'Expected checksum = %d. Got = %d' % (4672, ds.GetRasterBand(1).Checksum())
            return 'fail'
    ds = None

    os.remove('tmp/byte.tif.gz')
    os.remove('tmp/byte.tif.gz.properties')

    return 'success'

###############################################################################
# Read a .tif.zip file (with explicit filename)

def tiff_read_zip_1():

    ds = gdal.Open('/vsizip/./data/byte.tif.zip/byte.tif')
    if ds.GetRasterBand(1).Checksum() != 4672:
            print 'Expected checksum = %d. Got = %d' % (4672, ds.GetRasterBand(1).Checksum())
            return 'fail'
    ds = None

    return 'success'

###############################################################################
# Read a .tif.zip file (with implicit filename)

def tiff_read_zip_2():

    ds = gdal.Open('/vsizip/./data/byte.tif.zip')
    if ds.GetRasterBand(1).Checksum() != 4672:
            print 'Expected checksum = %d. Got = %d' % (4672, ds.GetRasterBand(1).Checksum())
            return 'fail'
    ds = None

    return 'success'

###############################################################################
# Check handling of non-degree angular units (#601)

def tiff_grads():

    ds = gdal.Open('data/test_gf.tif')
    srs = ds.GetProjectionRef()

    if string.find(srs,'PARAMETER["latitude_of_origin",46.8]') == -1:
        print srs
        gdaltest.post_reason( 'Did not get expected latitude of origin.' )
        return 'fail'

    return 'success'

###############################################################################
# Test GTiffSplitBitmapBand to treat one row 1bit files as scanline blocks (#2622)

def tiff_g4_split():

    if not 'GetBlockSize' in dir(gdal.Band):
        return 'skip'
    
    ds = gdal.Open('data/slim_g4.tif')

    (blockx, blocky) = ds.GetRasterBand(1).GetBlockSize()
    
    if blocky != 1:
        gdaltest.post_reason( 'Did not get scanline sized blocks.' )
        return 'fail'

    cs = ds.GetRasterBand(1).Checksum()
    if cs != 3322:
        print cs
        gdaltest.post_reason( 'Got wrong checksum' )
        return 'fail'
    
    return 'success'

###############################################################################
# Test reading a tiff with multiple images in it

def tiff_multi_images():

    # Implicitely get the content of the first image (backward compatibility)
    ds = gdal.Open('data/twoimages.tif')
    if ds.GetRasterBand(1).Checksum() != 4672:
            print 'Expected checksum = %d. Got = %d' % (4672, ds.GetRasterBand(1).Checksum())
            return 'fail'
    ds = None

    # Explicitely get the content of the first image
    ds = gdal.Open('GTIFF_DIR:1:data/twoimages.tif')
    if ds.GetRasterBand(1).Checksum() != 4672:
            print 'Expected checksum = %d. Got = %d' % (4672, ds.GetRasterBand(1).Checksum())
            return 'fail'
    ds = None

    # Explicitely get the content of the second image
    ds = gdal.Open('GTIFF_DIR:2:data/twoimages.tif')
    if ds.GetRasterBand(1).Checksum() != 4672:
            print 'Expected checksum = %d. Got = %d' % (4672, ds.GetRasterBand(1).Checksum())
            return 'fail'
    ds = None

    return 'success'

###############################################################################
# Test reading a tiff from a memory buffer (#2931)

def tiff_vsimem():

    try:
        gdal.FileFromMemBuffer
    except:
        return 'skip'

    content = open('data/byte.tif').read()

    # Create in-memory file
    gdal.FileFromMemBuffer('/vsimem/tiffinmem', content)

    ds = gdal.Open('/vsimem/tiffinmem', gdal.GA_Update)
    if ds.GetRasterBand(1).Checksum() != 4672:
            print 'Expected checksum = %d. Got = %d' % (4672, ds.GetRasterBand(1).Checksum())
            return 'fail'
    ds.GetRasterBand(1).Fill(0)
    ds = None

    ds = gdal.Open('/vsimem/tiffinmem')
    if ds.GetRasterBand(1).Checksum() != 0:
            print 'Expected checksum = %d. Got = %d' % (0, ds.GetRasterBand(1).Checksum())
            return 'fail'
    ds = None

    # Release memory associated to the in-memory file
    gdal.Unlink('/vsimem/tiffinmem')

    return 'success'

###############################################################################
# Test reading a tiff from inside a zip in a memory buffer !

def tiff_vsizip_and_mem():

    try:
        gdal.FileFromMemBuffer
    except:
        return 'skip'

    content = open('./data/byte.tif.zip').read()

    # Create in-memory file
    gdal.FileFromMemBuffer('/vsimem/tiffinmem.zip', content)

    ds = gdal.Open('/vsizip/vsimem/tiffinmem.zip/byte.tif')
    if ds.GetRasterBand(1).Checksum() != 4672:
            print 'Expected checksum = %d. Got = %d' % (4672, ds.GetRasterBand(1).Checksum())
            return 'fail'

    # Release memory associated to the in-memory file
    gdal.Unlink('/vsimem/tiffinmem.zip')

    return 'success'


for item in init_list:
    ut = gdaltest.GDALTest( 'GTiff', item[0], item[1], item[2] )
    if ut is None:
	print( 'GTiff tests skipped' )
	sys.exit()
    gdaltest_list.append( (ut.testOpen, item[0]) )
gdaltest_list.append( (tiff_read_off) )
gdaltest_list.append( (tiff_read_cmyk_rgba) )
gdaltest_list.append( (tiff_read_cmyk_raw) )
gdaltest_list.append( (tiff_read_gzip) )
gdaltest_list.append( (tiff_read_zip_1) )
gdaltest_list.append( (tiff_read_zip_2) )
gdaltest_list.append( (tiff_grads) )
gdaltest_list.append( (tiff_g4_split) )
gdaltest_list.append( (tiff_multi_images) )
gdaltest_list.append( (tiff_vsimem) )
gdaltest_list.append( (tiff_vsizip_and_mem) )

if __name__ == '__main__':

    gdaltest.setup_run( 'tiff_read' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

