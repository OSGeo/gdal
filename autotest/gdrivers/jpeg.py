#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test JPEG format driver.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
# 
###############################################################################
# Copyright (c) 2007, Frank Warmerdam <warmerdam@pobox.com>
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
import string

sys.path.append( '../pymod' )

import gdaltest
import gdalconst

###############################################################################
# Perform simple read test.

def jpeg_1():

    tst = gdaltest.GDALTest( 'JPEG', 'albania.jpg', 2, 17016 )
    return tst.testOpen()

###############################################################################
# Verify EXIF metadata, color interpretation and image_structure

def jpeg_2():

    ds = gdal.Open( 'data/albania.jpg' )

    md = ds.GetMetadata()

    try:
        if md['EXIF_GPSLatitudeRef'] != 'N' \
           or md['EXIF_GPSLatitude'] != '(41) (1) (22.91)' \
           or md['EXIF_PixelXDimension'] != '361' \
           or md['EXIF_GPSVersionID'] != '0x2 00 00 00' \
           or md['EXIF_ExifVersion'] != '0210' \
           or md['EXIF_XResolution'] != '(96)':
            print(md)
            gdaltest.post_reason( 'Exif metadata wrong.' )
            return 'fail'
    except:
        print(md)
        gdaltest.post_reason( 'Exit metadata apparently missing.' )
        return 'fail'

    if ds.GetRasterBand(3).GetRasterColorInterpretation() != gdal.GCI_BlueBand:
        gdaltest.post_reason( 'Did not get expected color interpretation.' )
        return 'fail'

    md = ds.GetMetadata('IMAGE_STRUCTURE')
    if 'INTERLEAVE' not in md or md['INTERLEAVE'] != 'PIXEL':
        gdaltest.post_reason( 'missing INTERLEAVE metadata' )
        return 'fail'
    if 'COMPRESSION' not in md or md['COMPRESSION'] != 'JPEG':
        gdaltest.post_reason( 'missing INTERLEAVE metadata' )
        return 'fail'

    md = ds.GetRasterBand(3).GetMetadata('IMAGE_STRUCTURE')
    if 'COMPRESSION' not in md or md['COMPRESSION'] != 'JPEG':
        gdaltest.post_reason( 'missing INTERLEAVE metadata' )
        return 'fail'

    return 'success'

###############################################################################
# Create simple copy and check (greyscale) using progressive option.

def jpeg_3():

    ds = gdal.Open('data/byte.tif')

    options = ['PROGRESSIVE=YES',
               'QUALITY=50']
    ds = gdal.GetDriverByName('JPEG').CreateCopy( 'tmp/byte.jpg', ds,
                                                  options = options )
                                                  
    if ds.GetRasterBand(1).Checksum() != 4794:
        gdaltest.post_reason( 'Wrong checksum on copied image.')
        print(ds.GetRasterBand(1).Checksum())
        return 'fail'

    if ds.GetRasterBand(1).GetRasterColorInterpretation()!= gdal.GCI_GrayIndex:
        gdaltest.post_reason( 'Wrong color interpretation.')
        print(ds.GetRasterBand(1).GetRasterColorInterpretation())
        return 'fail'

    ds = None
    gdal.GetDriverByName('JPEG').Delete( 'tmp/byte.jpg' )
    
    return 'success'
    
###############################################################################
# Verify masked jpeg. 

def jpeg_4():

    try:
        gdalconst.GMF_ALL_VALID
    except:
        return 'skip'
    
    ds = gdal.Open('data/masked.jpg')

    refband = ds.GetRasterBand(1)

    if refband.GetMaskFlags() != gdalconst.GMF_PER_DATASET:
        gdaltest.post_reason( 'wrong mask flags' )
        return 'fail'

    cs = refband.GetMaskBand().Checksum()
    if cs != 770:
        gdaltest.post_reason( 'Wrong mask checksum' )
        print(cs)
        return 'fail'

    return 'success'
    
###############################################################################
# Verify CreateCopy() of masked jpeg.

def jpeg_5():

    try:
        gdalconst.GMF_ALL_VALID
    except:
        return 'skip'
    
    ds = gdal.Open('data/masked.jpg')

    ds2 = gdal.GetDriverByName('JPEG').CreateCopy( 'tmp/masked.jpg', ds )

    refband = ds2.GetRasterBand(1)

    if refband.GetMaskFlags() != gdalconst.GMF_PER_DATASET:
        gdaltest.post_reason( 'wrong mask flags' )
        return 'fail'

    cs = refband.GetMaskBand().Checksum()
    if cs != 770:
        gdaltest.post_reason( 'Wrong checksum on copied images mask.')
        print(cs)
        return 'fail'

    refband = None
    ds2 = None
    gdal.GetDriverByName('JPEG').Delete( 'tmp/masked.jpg' )
    
    return 'success'
    
###############################################################################
# Verify ability to open file with corrupt metadata (#1904).  Note the file
# data/vophead.jpg is truncated to keep the size small, but this should
# not affect opening the file which just reads the header.

def jpeg_6():

    ds = gdal.Open('data/vophead.jpg')

    # Bacause of the optimization done in r17446, we should'nt yet get this error
    if gdal.GetLastErrorType() == 2 \
       and gdal.GetLastErrorMsg().find('Ignoring EXIF') != -1:
        gdaltest.post_reason( 'got error too soon.')
        return 'fail'

    md = ds.GetMetadata()

    # Did we get an exif related warning?
    if gdal.GetLastErrorType() != 2 \
       or gdal.GetLastErrorMsg().find('Ignoring EXIF') == -1:
        gdaltest.post_reason( 'we did not get expected error.')
        return 'fail'

    if len(md) != 1 or md['EXIF_Software'] != 'IrfanView':
        gdaltest.post_reason( 'did not get expected metadata.' )
        print(md)
        return 'fail'

    ds = None

    return 'success'


###############################################################################
# Test creating an in memory copy.

def jpeg_7():

    ds = gdal.Open('data/byte.tif')

    options = ['PROGRESSIVE=YES',
               'QUALITY=50']
    ds = gdal.GetDriverByName('JPEG').CreateCopy( '/vsimem/byte.jpg', ds,
                                                  options = options )

    if ds.GetRasterBand(1).Checksum() != 4794:
        gdaltest.post_reason( 'Wrong checksum on copied image.')
        print(ds.GetRasterBand(1).Checksum())
        return 'fail'

    ds = None
    gdal.GetDriverByName('JPEG').Delete( '/vsimem/byte.jpg' )

    return 'success'

###############################################################################
# Read a CMYK image as a RGB image

def jpeg_8():

    ds = gdal.Open( 'data/rgb_ntf_cmyk.jpg' )

    if ds.GetRasterBand(1).Checksum() != 20385:
        gdaltest.post_reason( 'Wrong checksum on copied image.')
        print(ds.GetRasterBand(1).Checksum())
        return 'fail'

    if ds.GetRasterBand(1).GetRasterColorInterpretation()!= gdal.GCI_RedBand:
        gdaltest.post_reason( 'Wrong color interpretation.')
        print(ds.GetRasterBand(1).GetRasterColorInterpretation())
        return 'fail'

    if ds.GetRasterBand(2).Checksum() != 20865:
        gdaltest.post_reason( 'Wrong checksum on copied image.')
        print(ds.GetRasterBand(2).Checksum())
        return 'fail'

    if ds.GetRasterBand(2).GetRasterColorInterpretation()!= gdal.GCI_GreenBand:
        gdaltest.post_reason( 'Wrong color interpretation.')
        print(ds.GetRasterBand(2).GetRasterColorInterpretation())
        return 'fail'

    if ds.GetRasterBand(3).Checksum() != 19441:
        gdaltest.post_reason( 'Wrong checksum on copied image.')
        print(ds.GetRasterBand(3).Checksum())
        return 'fail'

    if ds.GetRasterBand(3).GetRasterColorInterpretation()!= gdal.GCI_BlueBand:
        gdaltest.post_reason( 'Wrong color interpretation.')
        print(ds.GetRasterBand(3).GetRasterColorInterpretation())
        return 'fail'

    md = ds.GetMetadata('IMAGE_STRUCTURE')

    if 'SOURCE_COLOR_SPACE' not in md or md['SOURCE_COLOR_SPACE'] != 'CMYK':
        gdaltest.post_reason( 'missing SOURCE_COLOR_SPACE metadata' )
        return 'fail'

    return 'success'

###############################################################################
# Read a CMYK image as a CMYK image

def jpeg_9():

    gdal.SetConfigOption('GDAL_JPEG_TO_RGB', 'NO')
    ds = gdal.Open( 'data/rgb_ntf_cmyk.jpg' )
    gdal.SetConfigOption('GDAL_JPEG_TO_RGB', 'YES')

    if ds.GetRasterBand(1).Checksum() != 21187:
        gdaltest.post_reason( 'Wrong checksum on copied image.')
        print(ds.GetRasterBand(1).Checksum())
        return 'fail'

    if ds.GetRasterBand(1).GetRasterColorInterpretation()!= gdal.GCI_CyanBand:
        gdaltest.post_reason( 'Wrong color interpretation.')
        print(ds.GetRasterBand(1).GetRasterColorInterpretation())
        return 'fail'

    if ds.GetRasterBand(2).Checksum() != 21054:
        gdaltest.post_reason( 'Wrong checksum on copied image.')
        print(ds.GetRasterBand(2).Checksum())
        return 'fail'

    if ds.GetRasterBand(2).GetRasterColorInterpretation()!= gdal.GCI_MagentaBand:
        gdaltest.post_reason( 'Wrong color interpretation.')
        print(ds.GetRasterBand(2).GetRasterColorInterpretation())
        return 'fail'

    if ds.GetRasterBand(3).Checksum() != 21499:
        gdaltest.post_reason( 'Wrong checksum on copied image.')
        print(ds.GetRasterBand(3).Checksum())
        return 'fail'

    if ds.GetRasterBand(3).GetRasterColorInterpretation()!= gdal.GCI_YellowBand:
        gdaltest.post_reason( 'Wrong color interpretation.')
        print(ds.GetRasterBand(3).GetRasterColorInterpretation())
        return 'fail'

    if ds.GetRasterBand(4).Checksum() != 21069:
        gdaltest.post_reason( 'Wrong checksum on copied image.')
        print(ds.GetRasterBand(4).Checksum())
        return 'fail'

    if ds.GetRasterBand(4).GetRasterColorInterpretation()!= gdal.GCI_BlackBand:
        gdaltest.post_reason( 'Wrong color interpretation.')
        print(ds.GetRasterBand(4).GetRasterColorInterpretation())
        return 'fail'

    return 'success'

###############################################################################
# Check reading a 12-bit JPEG

def jpeg_10():

    # Check if JPEG driver supports 12bit JPEG reading/writing
    drv = gdal.GetDriverByName('JPEG')
    md = drv.GetMetadata()
    if md[gdal.DMD_CREATIONDATATYPES].find('UInt16') == -1:
        sys.stdout.write('(12bit jpeg not available) ... ')
        return 'skip'

    ds = gdal.Open('data/12bit_rose_extract.jpg')
    if ds.GetRasterBand(1).DataType != gdal.GDT_UInt16:
        return 'fail'
    stats = ds.GetRasterBand(1).GetStatistics( 0, 1 )
    if stats[2] < 3613 or stats[2] > 3614:
        print(stats)
        return 'fail'
    ds = None

    return 'success'

###############################################################################
# Check creating a 12-bit JPEG

def jpeg_11():

    # Check if JPEG driver supports 12bit JPEG reading/writing
    drv = gdal.GetDriverByName('JPEG')
    md = drv.GetMetadata()
    if md[gdal.DMD_CREATIONDATATYPES].find('UInt16') == -1:
        sys.stdout.write('(12bit jpeg not available) ... ')
        return 'skip'

    ds = gdal.Open('data/12bit_rose_extract.jpg')
    out_ds = gdal.GetDriverByName('JPEG').CreateCopy('tmp/jpeg11.jpg', ds)
    out_ds = None

    ds = gdal.Open('tmp/jpeg11.jpg')
    if ds.GetRasterBand(1).DataType != gdal.GDT_UInt16:
        return 'fail'
    stats = ds.GetRasterBand(1).GetStatistics( 0, 1 )
    if stats[2] < 3613 or stats[2] > 3614:
        print(stats)
        return 'fail'
    ds = None

    gdal.GetDriverByName('JPEG').Delete('tmp/jpeg11.jpg');

    return 'success'

gdaltest_list = [
    jpeg_1,
    jpeg_2,
    jpeg_3,
    jpeg_4,
    jpeg_5,
    jpeg_6,
    jpeg_7,
    jpeg_8,
    jpeg_9,
    jpeg_10,
    jpeg_11 ]

if __name__ == '__main__':

    gdaltest.setup_run( 'jpeg' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

