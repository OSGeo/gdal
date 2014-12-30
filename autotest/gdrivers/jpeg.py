#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test JPEG format driver.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
# 
###############################################################################
# Copyright (c) 2007, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2008-2014, Even Rouault <even dot rouault at mines-paris dot org>
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
import string
import shutil

sys.path.append( '../pymod' )

import gdaltest
from osgeo import gdalconst

###############################################################################
# Perform simple read test.

def jpeg_1():

    ds = gdal.Open( 'data/albania.jpg' )
    cs = ds.GetRasterBand(2).Checksum()
    if cs == 34298:
        gdaltest.jpeg8 = True
    else:
        gdaltest.jpeg8 = False
    ds = None

    if gdaltest.jpeg8:
        tst = gdaltest.GDALTest( 'JPEG', 'albania.jpg', 2, 34298 )
    else:
        tst = gdaltest.GDALTest( 'JPEG', 'albania.jpg', 2, 17016 )
    return tst.testOpen()

###############################################################################
# Verify EXIF metadata, color interpretation and image_structure

def jpeg_2():

    ds = gdal.Open( 'data/albania.jpg' )

    md = ds.GetMetadata()

    ds.GetFileList()

    try:
        if md['EXIF_GPSLatitudeRef'] != 'N' \
           or md['EXIF_GPSLatitude'] != '(41) (1) (22.91)' \
           or md['EXIF_PixelXDimension'] != '361' \
           or (md['EXIF_GPSVersionID'] != '0x2 00 00 00' and  md['EXIF_GPSVersionID'] != '0x2 0x0 0x0 0x0') \
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
               'QUALITY=50',
               'WORLDFILE=YES']
    ds = gdal.GetDriverByName('JPEG').CreateCopy( 'tmp/byte.jpg', ds,
                                                  options = options )

    expected_cs = 4794

    if ds.GetRasterBand(1).Checksum() != expected_cs:
        gdaltest.post_reason( 'Wrong checksum on copied image.')
        print(ds.GetRasterBand(1).Checksum())
        return 'fail'

    if ds.GetRasterBand(1).GetRasterColorInterpretation()!= gdal.GCI_GrayIndex:
        gdaltest.post_reason( 'Wrong color interpretation.')
        print(ds.GetRasterBand(1).GetRasterColorInterpretation())
        return 'fail'

    expected_gt = [440720.0, 60.0, 0.0, 3751320.0, 0.0, -60.0]
    gt = ds.GetGeoTransform()
    for i in range(6):
        if abs(gt[i] - expected_gt[i]) > 1e-6:
            gdaltest.post_reason('did not get expected geotransform from PAM')
            print(gt)
            return 'fail'

    ds = None

    os.unlink( 'tmp/byte.jpg.aux.xml' )

    try:
        os.stat('tmp/byte.wld')
    except:
        gdaltest.post_reason('should have .wld file at that point')
        return 'fail'

    ds = gdal.Open('tmp/byte.jpg')
    expected_gt = [440720.0, 60.0, 0.0, 3751320.0, 0.0, -60.0]
    gt = ds.GetGeoTransform()
    for i in range(6):
        if abs(gt[i] - expected_gt[i]) > 1e-6:
            gdaltest.post_reason('did not get expected geotransform from .wld')
            print(gt)
            return 'fail'
    ds = None

    ds = gdal.Open('tmp/byte.jpg')
    ds.GetFileList()
    ds = None

    gdal.GetDriverByName('JPEG').Delete( 'tmp/byte.jpg' )

    try:
        os.stat('tmp/byte.wld')
        gdaltest.post_reason('did not expect to find .wld file at that point')
        return 'fail'
    except:
        pass

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

    expected_cs = 4794

    if ds.GetRasterBand(1).Checksum() != expected_cs:
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

    expected_cs = 20385

    if ds.GetRasterBand(1).Checksum() != expected_cs:
        gdaltest.post_reason( 'Wrong checksum on copied image.')
        print(ds.GetRasterBand(1).Checksum())
        return 'fail'

    if ds.GetRasterBand(1).GetRasterColorInterpretation()!= gdal.GCI_RedBand:
        gdaltest.post_reason( 'Wrong color interpretation.')
        print(ds.GetRasterBand(1).GetRasterColorInterpretation())
        return 'fail'

    expected_cs = 20865

    if ds.GetRasterBand(2).Checksum() != expected_cs:
        gdaltest.post_reason( 'Wrong checksum on copied image.')
        print(ds.GetRasterBand(2).Checksum())
        return 'fail'

    if ds.GetRasterBand(2).GetRasterColorInterpretation()!= gdal.GCI_GreenBand:
        gdaltest.post_reason( 'Wrong color interpretation.')
        print(ds.GetRasterBand(2).GetRasterColorInterpretation())
        return 'fail'

    expected_cs = 19441

    if ds.GetRasterBand(3).Checksum() != expected_cs:
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

    expected_cs = 21187

    if ds.GetRasterBand(1).Checksum() != expected_cs:
        gdaltest.post_reason( 'Wrong checksum on copied image.')
        print(ds.GetRasterBand(1).Checksum())
        return 'fail'

    if ds.GetRasterBand(1).GetRasterColorInterpretation()!= gdal.GCI_CyanBand:
        gdaltest.post_reason( 'Wrong color interpretation.')
        print(ds.GetRasterBand(1).GetRasterColorInterpretation())
        return 'fail'

    expected_cs = 21054

    if ds.GetRasterBand(2).Checksum() != expected_cs:
        gdaltest.post_reason( 'Wrong checksum on copied image.')
        print(ds.GetRasterBand(2).Checksum())
        return 'fail'

    if ds.GetRasterBand(2).GetRasterColorInterpretation()!= gdal.GCI_MagentaBand:
        gdaltest.post_reason( 'Wrong color interpretation.')
        print(ds.GetRasterBand(2).GetRasterColorInterpretation())
        return 'fail'

    expected_cs = 21499

    if ds.GetRasterBand(3).Checksum() != expected_cs:
        gdaltest.post_reason( 'Wrong checksum on copied image.')
        print(ds.GetRasterBand(3).Checksum())
        return 'fail'

    if ds.GetRasterBand(3).GetRasterColorInterpretation()!= gdal.GCI_YellowBand:
        gdaltest.post_reason( 'Wrong color interpretation.')
        print(ds.GetRasterBand(3).GetRasterColorInterpretation())
        return 'fail'

    expected_cs = 21069

    if ds.GetRasterBand(4).Checksum() != expected_cs:
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

    try:
        os.remove('data/12bit_rose_extract.jpg.aux.xml')
    except:
        pass

    ds = gdal.Open('data/12bit_rose_extract.jpg')
    if ds.GetRasterBand(1).DataType != gdal.GDT_UInt16:
        return 'fail'
    stats = ds.GetRasterBand(1).GetStatistics( 0, 1 )
    if stats[2] < 3613 or stats[2] > 3614:
        print(stats)
        return 'fail'
    ds = None

    try:
        os.remove('data/12bit_rose_extract.jpg.aux.xml')
    except:
        pass

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

###############################################################################
# Test reading a stored JPEG in ZIP (#3908)

def jpeg_12():

    ds = gdal.Open('/vsizip/data/byte_jpg.zip')
    if ds is None:
        return 'fail'

    gdal.ErrorReset()
    ds.GetRasterBand(1).Checksum()
    if gdal.GetLastErrorMsg() != '':
        return 'fail'

    gdal.ErrorReset()
    ds.GetRasterBand(1).GetMaskBand().Checksum()
    if gdal.GetLastErrorMsg() != '':
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test outputing to /vsistdout/

def jpeg_13():

    src_ds = gdal.Open('data/byte.tif')
    ds = gdal.GetDriverByName('JPEG').CreateCopy('/vsistdout_redirect//vsimem/tmp.jpg', src_ds)
    if ds.GetRasterBand(1).Checksum() != 0:
        return 'fail'
    ds.ReadRaster(0,0,1,1)
    src_ds = None
    ds = None

    ds = gdal.Open('/vsimem/tmp.jpg')
    if ds is None:
        return 'fail'

    gdal.Unlink('/vsimem/tmp.jpg')

    return 'success'

###############################################################################
# Test outputing to /vsistdout/

def jpeg_14():

    # Check if JPEG driver supports 12bit JPEG reading/writing
    drv = gdal.GetDriverByName('JPEG')
    md = drv.GetMetadata()
    if md[gdal.DMD_CREATIONDATATYPES].find('UInt16') == -1:
        sys.stdout.write('(12bit jpeg not available) ... ')
        return 'skip'

    src_ds = gdal.Open('data/12bit_rose_extract.jpg')
    ds = drv.CreateCopy('/vsistdout_redirect//vsimem/tmp.jpg', src_ds)
    if ds.GetRasterBand(1).Checksum() != 0:
        return 'fail'
    src_ds = None
    ds = None

    ds = gdal.Open('/vsimem/tmp.jpg')
    if ds is None:
        return 'fail'

    gdal.Unlink('/vsimem/tmp.jpg')

    return 'success'

###############################################################################
# Test CreateCopy() interruption

def jpeg_15():

    tst = gdaltest.GDALTest( 'JPEG', 'albania.jpg', 2, 17016 )

    return tst.testCreateCopy( vsimem = 1, interrupt_during_copy = True )

###############################################################################
# Test overview support

def jpeg_16():

    shutil.copy( 'data/albania.jpg', 'tmp/albania.jpg' )
    try:
        os.unlink( 'tmp/albania.jpg.ovr' )
    except:
        pass

    ds = gdal.Open( 'tmp/albania.jpg' )
    if ds.GetRasterBand(1).GetOverviewCount() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetRasterBand(1).GetOverview(-1) is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetRasterBand(1).GetOverview(1) is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetRasterBand(1).GetOverview(0) is None:
        gdaltest.post_reason('fail')
        return 'fail'
    # "Internal" overview

    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    if gdaltest.jpeg8:
        expected_cs = 34218
    else:
        expected_cs = 31892
    if cs != expected_cs:
        gdaltest.post_reason('fail')
        print(cs)
        return 'fail'

    # Build external overviews
    ds.BuildOverviews('NEAR', [2, 4])
    if ds.GetRasterBand(1).GetOverviewCount() != 2:
        gdaltest.post_reason('fail')
        return 'fail'
    # Check updated checksum
    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    if gdaltest.jpeg8:
        expected_cs = 33698
    else:
        expected_cs = 32460
    if cs != expected_cs:
        gdaltest.post_reason('fail')
        print(cs)
        return 'fail'

    ds = None

    # Check we are using external overviews
    ds = gdal.Open( 'tmp/albania.jpg' )
    if ds.GetRasterBand(1).GetOverviewCount() != 2:
        gdaltest.post_reason('fail')
        return 'fail'
    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    if gdaltest.jpeg8:
        expected_cs = 33698
    else:
        expected_cs = 32460
    if cs != expected_cs:
        gdaltest.post_reason('fail')
        print(cs)
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test bogus files

def jpeg_17():

    gdal.ErrorReset()
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = gdal.Open('data/bogus.jpg')
    gdal.PopErrorHandler()
    if ds is not None or gdal.GetLastErrorType() != gdal.CE_Failure or gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.ErrorReset()
    ds = gdal.Open('data/byte_corrupted.jpg')
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    # ERROR 1: libjpeg: Huffman table 0x00 was not defined
    ds.GetRasterBand(1).Checksum()
    gdal.PopErrorHandler()
    if gdal.GetLastErrorType() != gdal.CE_Failure or gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.ErrorReset()
    ds = gdal.Open('data/byte_corrupted2.jpg')
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    # Warning 1: libjpeg: Corrupt JPEG data: found marker 0x00 instead of RST63
    ds.GetRasterBand(1).Checksum()
    gdal.PopErrorHandler()
    if gdal.GetLastErrorType() != gdal.CE_Warning or gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.ErrorReset()
    ds = gdal.Open('data/byte_corrupted2.jpg')
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    gdal.SetConfigOption('GDAL_ERROR_ON_LIBJPEG_WARNING', 'TRUE')
    # ERROR 1: libjpeg: Corrupt JPEG data: found marker 0x00 instead of RST63
    ds.GetRasterBand(1).Checksum()
    gdal.SetConfigOption('GDAL_ERROR_ON_LIBJPEG_WARNING', None)
    gdal.PopErrorHandler()
    if gdal.GetLastErrorType() != gdal.CE_Failure or gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Test situation where we cause a restart and need to reset scale

def jpeg_18():

    import struct

    height = 1024
    width = 1024
    src_ds = gdal.GetDriverByName('GTiff').Create('/vsimem/jpeg_18.tif', width, height, 1)
    for i in range(height):
        data = struct.pack('B' * 1, int(i / (height / 256)))
        src_ds.WriteRaster(0,i,width,1,data,1,1)

    ds = gdal.GetDriverByName('JPEG').CreateCopy('/vsimem/jpeg_18.jpg', src_ds, options = ['QUALITY=99'])
    src_ds = None
    gdal.Unlink('/vsimem/jpeg_18.tif')

    oldSize = gdal.GetCacheMax()
    gdal.SetCacheMax(0)

    line0 = ds.GetRasterBand(1).ReadRaster(0,0,width,1)
    data = struct.unpack('B' * width, line0)
    if abs(data[0] - 0) > 10:
        return 'fail'
    line1023 = ds.GetRasterBand(1).ReadRaster(0,height-1,width,1)
    data = struct.unpack('B' * width, line1023)
    if abs(data[0] - 255) > 10:
        return 'fail'
    line0_ovr1 = ds.GetRasterBand(1).GetOverview(1).ReadRaster(0,0,int(width / 4),1)
    data = struct.unpack('B' * (int(width / 4)), line0_ovr1)
    if abs(data[0] - 0) > 10:
        return 'fail'
    line1023_bis = ds.GetRasterBand(1).ReadRaster(0,height-1,width,1)
    if line1023_bis == line0 or line1023 != line1023_bis:
        gdaltest.post_reason('fail')
        return 'fail'
    line0_bis = ds.GetRasterBand(1).ReadRaster(0,0,width,1)
    if line0 != line0_bis:
        gdaltest.post_reason('fail')
        return 'fail'
    line255_ovr1 = ds.GetRasterBand(1).GetOverview(1).ReadRaster(0,int(height / 4) - 1,int(width / 4),1)
    data = struct.unpack('B' * (int(width / 4)), line255_ovr1)
    if abs(data[0] - 255) > 10:
        return 'fail'
    line0_bis = ds.GetRasterBand(1).ReadRaster(0,0,width,1)
    if line0 != line0_bis:
        gdaltest.post_reason('fail')
        return 'fail'
    line0_ovr1_bis = ds.GetRasterBand(1).GetOverview(1).ReadRaster(0,0,int(width / 4),1)
    if line0_ovr1 != line0_ovr1_bis:
        gdaltest.post_reason('fail')
        return 'fail'
    line255_ovr1_bis = ds.GetRasterBand(1).GetOverview(1).ReadRaster(0,int(height / 4) - 1,int(width / 4),1)
    if line255_ovr1 != line255_ovr1_bis:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.SetCacheMax(oldSize)

    ds = None
    gdal.Unlink('/vsimem/jpeg_18.jpg')

    return 'success'

###############################################################################
# Test MSB ordering of bits in mask (#5102)

def jpeg_19():

    import struct

    for (width, height, iX) in [ (32, 32, 12), (25, 25, 8), (24, 25, 8) ]:
        src_ds = gdal.GetDriverByName('GTiff').Create('/vsimem/jpeg_19.tif', width, height, 1)
        src_ds.CreateMaskBand(gdal.GMF_PER_DATASET)
        src_ds.GetRasterBand(1).GetMaskBand().WriteRaster(0,0,iX,height,struct.pack('B' * 1, 255),1,1)
        src_ds.GetRasterBand(1).GetMaskBand().WriteRaster(iX,0,width-iX,height,struct.pack('B' * 1, 0),1,1)
        tiff_mask_data = src_ds.GetRasterBand(1).GetMaskBand().ReadRaster(0, 0, width, height)

        # Generate a JPEG file with a (default) LSB bit mask order
        out_ds = gdal.GetDriverByName('JPEG').CreateCopy('/vsimem/jpeg_19.jpg', src_ds)
        out_ds = None

        # Generate a JPEG file with a MSB bit mask order
        gdal.SetConfigOption('JPEG_WRITE_MASK_BIT_ORDER', 'MSB')
        out_ds = gdal.GetDriverByName('JPEG').CreateCopy('/vsimem/jpeg_19_msb.jpg', src_ds)
        out_ds = None
        gdal.SetConfigOption('JPEG_WRITE_MASK_BIT_ORDER', None)

        src_ds = None

        # Check that the file are indeed different
        statBuf = gdal.VSIStatL('/vsimem/jpeg_19.jpg')
        f = gdal.VSIFOpenL('/vsimem/jpeg_19.jpg', 'rb')
        data1 = gdal.VSIFReadL(1, statBuf.size, f)
        gdal.VSIFCloseL(f)

        statBuf = gdal.VSIStatL('/vsimem/jpeg_19_msb.jpg')
        f = gdal.VSIFOpenL('/vsimem/jpeg_19_msb.jpg', 'rb')
        data2 = gdal.VSIFReadL(1, statBuf.size, f)
        gdal.VSIFCloseL(f)

        if (width, height, iX) == (24, 25, 8):
            if data1 != data2:
                gdaltest.post_reason('fail')
                return 'fail'
        else:
            if data1 == data2:
                gdaltest.post_reason('fail')
                return 'fail'

        # Check the file with the LSB bit mask order
        ds = gdal.Open('/vsimem/jpeg_19.jpg')
        jpg_mask_data = ds.GetRasterBand(1).GetMaskBand().ReadRaster(0, 0, width, height)
        ds = None
        if tiff_mask_data != jpg_mask_data:
            gdaltest.post_reason('fail')
            return 'fail'

        # Check the file with the MSB bit mask order
        ds = gdal.Open('/vsimem/jpeg_19_msb.jpg')
        jpg_mask_data = ds.GetRasterBand(1).GetMaskBand().ReadRaster(0, 0, width, height)
        ds = None
        if tiff_mask_data != jpg_mask_data:
            gdaltest.post_reason('fail')
            return 'fail'

        gdal.GetDriverByName('GTiff').Delete('/vsimem/jpeg_19.tif')
        gdal.GetDriverByName('JPEG').Delete('/vsimem/jpeg_19.jpg')
        gdal.GetDriverByName('JPEG').Delete('/vsimem/jpeg_19_msb.jpg')

    return 'success'

###############################################################################
# Test correct GCP reading with PAM (#5352)

def jpeg_20():

    src_ds = gdal.Open('data/rgb_gcp.vrt')
    ds = gdal.GetDriverByName('JPEG').CreateCopy('/vsimem/jpeg_20.jpg', src_ds)
    ds = None

    ds = gdal.Open('/vsimem/jpeg_20.jpg')
    if ds.GetGCPProjection().find('GEOGCS["WGS 84"') != 0:
        gdaltest.post_reason('failure')
        print(ds.GetGCPProjection())
        return 'fail'
    if ds.GetGCPCount() != 4:
        gdaltest.post_reason('failure')
        return 'fail'
    if len(ds.GetGCPs()) != 4:
        gdaltest.post_reason('failure')
        return 'fail'
    ds = None
    
    gdal.GetDriverByName('JPEG').Delete('/vsimem/jpeg_20.jpg')

    return 'success'

###############################################################################
# Test implicit and EXIF overviews

def jpeg_21():

    ds = gdal.Open('data/black_with_white_exif_ovr.jpg')
    if ds.GetRasterBand(1).GetOverviewCount() != 3:
        gdaltest.post_reason('failure')
        print(ds.GetRasterBand(1).GetOverviewCount())
        return 'fail'
    expected_dim_cs = [ [512,512,0], [256,256,0], [196,196,12681] ]
    i = 0
    for (expected_w, expected_h, expected_cs) in expected_dim_cs:
        ovr = ds.GetRasterBand(1).GetOverview(i)
        cs = ovr.Checksum()
        if ovr.XSize != expected_w or ovr.YSize != expected_h or cs != expected_cs:
            gdaltest.post_reason('failure')
            print(ovr.XSize)
            print(ovr.YSize)
            print(cs)
            return 'fail'
        i = i + 1
    ds = None

    return 'success'

###############################################################################
# Test generation of EXIF overviews

def jpeg_22():

    src_ds = gdal.GetDriverByName('Mem').Create('', 4096, 2048)
    src_ds.GetRasterBand(1).Fill(255)
    ds = gdal.GetDriverByName('JPEG').CreateCopy('/vsimem/jpeg_22.jpg', src_ds, options = ['EXIF_THUMBNAIL=YES'])
    src_ds = None
    if ds.GetRasterBand(1).GetOverviewCount() != 4:
        gdaltest.post_reason('failure')
        print(ds.GetRasterBand(1).GetOverviewCount())
        return 'fail'
    ovr = ds.GetRasterBand(1).GetOverview(3)
    cs = ovr.Checksum()
    if ovr.XSize != 128 or ovr.YSize != 64 or cs != 34957:
        gdaltest.post_reason('failure')
        print(ovr.XSize)
        print(ovr.YSize)
        print(cs)
        return 'fail'
    ds = None

    src_ds = gdal.GetDriverByName('Mem').Create('', 2048, 4096)
    src_ds.GetRasterBand(1).Fill(255)
    ds = gdal.GetDriverByName('JPEG').CreateCopy('/vsimem/jpeg_22.jpg', src_ds, options = ['EXIF_THUMBNAIL=YES'])
    src_ds = None
    ovr = ds.GetRasterBand(1).GetOverview(3)
    if ovr.XSize != 64 or ovr.YSize != 128:
        gdaltest.post_reason('failure')
        print(ovr.XSize)
        print(ovr.YSize)
        return 'fail'
    ds = None

    src_ds = gdal.GetDriverByName('Mem').Create('', 2048, 4096)
    src_ds.GetRasterBand(1).Fill(255)
    ds = gdal.GetDriverByName('JPEG').CreateCopy('/vsimem/jpeg_22.jpg', src_ds, options = ['EXIF_THUMBNAIL=YES', 'THUMBNAIL_WIDTH=40'])
    src_ds = None
    ovr = ds.GetRasterBand(1).GetOverview(3)
    if ovr.XSize != 40 or ovr.YSize != 80:
        gdaltest.post_reason('failure')
        print(ovr.XSize)
        print(ovr.YSize)
        return 'fail'
    ds = None

    src_ds = gdal.GetDriverByName('Mem').Create('', 2048, 4096)
    src_ds.GetRasterBand(1).Fill(255)
    ds = gdal.GetDriverByName('JPEG').CreateCopy('/vsimem/jpeg_22.jpg', src_ds, options = ['EXIF_THUMBNAIL=YES', 'THUMBNAIL_HEIGHT=60'])
    src_ds = None
    ovr = ds.GetRasterBand(1).GetOverview(3)
    if ovr.XSize != 30 or ovr.YSize != 60:
        gdaltest.post_reason('failure')
        print(ovr.XSize)
        print(ovr.YSize)
        return 'fail'
    ds = None

    src_ds = gdal.GetDriverByName('Mem').Create('', 2048, 4096)
    src_ds.GetRasterBand(1).Fill(255)
    ds = gdal.GetDriverByName('JPEG').CreateCopy('/vsimem/jpeg_22.jpg', src_ds, options = ['EXIF_THUMBNAIL=YES', 'THUMBNAIL_WIDTH=50', 'THUMBNAIL_HEIGHT=40'])
    src_ds = None
    ovr = ds.GetRasterBand(1).GetOverview(3)
    if ovr.XSize != 50 or ovr.YSize != 40:
        gdaltest.post_reason('failure')
        print(ovr.XSize)
        print(ovr.YSize)
        return 'fail'
    ds = None

    gdal.Unlink('/vsimem/jpeg_22.jpg')

    return 'success'

###############################################################################
# Test optimized JPEG IRasterIO

def jpeg_23():
    ds = gdal.Open( 'data/albania.jpg' )
    cs = [ ds.GetRasterBand(i+1).Checksum() for i in range(3)]
    
    # Band interleaved
    data = ds.ReadRaster(0,0,ds.RasterXSize, ds.RasterYSize)
    tmp_ds = gdal.GetDriverByName('Mem').Create('', ds.RasterXSize, ds.RasterYSize, 3)
    tmp_ds.WriteRaster(0,0,ds.RasterXSize, ds.RasterYSize,data)
    got_cs = [ tmp_ds.GetRasterBand(i+1).Checksum() for i in range(3)]
    if cs != got_cs:
        gdaltest.post_reason('failure')
        return 'fail'    
        
    # Pixel interleaved
    data = ds.ReadRaster(0,0,ds.RasterXSize, ds.RasterYSize, buf_pixel_space = 3, buf_band_space = 1)
    tmp_ds = gdal.GetDriverByName('Mem').Create('', ds.RasterXSize, ds.RasterYSize, 3)
    tmp_ds.WriteRaster(0,0,ds.RasterXSize, ds.RasterYSize,data, buf_pixel_space = 3, buf_band_space = 1)
    got_cs = [ tmp_ds.GetRasterBand(i+1).Checksum() for i in range(3)]
    if cs != got_cs:
        gdaltest.post_reason('failure')
        return 'fail'    

    # Pixel interleaved with padding
    data = ds.ReadRaster(0,0,ds.RasterXSize, ds.RasterYSize, buf_pixel_space = 4, buf_band_space = 1)
    tmp_ds = gdal.GetDriverByName('Mem').Create('', ds.RasterXSize, ds.RasterYSize, 3)
    tmp_ds.WriteRaster(0,0,ds.RasterXSize, ds.RasterYSize,data, buf_pixel_space = 4, buf_band_space = 1)
    got_cs = [ tmp_ds.GetRasterBand(i+1).Checksum() for i in range(3)]
    if cs != got_cs:
        gdaltest.post_reason('failure')
        return 'fail'    
    
    return 'success'

###############################################################################
# Test Arithmetic coding (and if not enabled, will trigger error code handling
# in CreateCopy())

def jpeg_24():

    if gdal.GetDriverByName('JPEG').GetMetadataItem('DMD_CREATIONOPTIONLIST').find('ARITHMETIC') >= 0:
        has_arithmetic = True
    else:
        has_arithmetic = False

    src_ds = gdal.Open('data/byte.tif')
    if not has_arithmetic:
        gdal.PushErrorHandler()
    ds = gdal.GetDriverByName('JPEG').CreateCopy( '/vsimem/byte.jpg', src_ds,
                                                  options = ['ARITHMETIC=YES'] )
    if not has_arithmetic:
        gdal.PopErrorHandler()
    else:
        expected_cs = 4743

        if ds.GetRasterBand(1).Checksum() != expected_cs:
            gdaltest.post_reason( 'Wrong checksum on copied image.')
            print(ds.GetRasterBand(1).Checksum())
            return 'fail'

        ds = None
        gdal.GetDriverByName('JPEG').Delete( '/vsimem/byte.jpg' )

    return 'success'

###############################################################################
# Cleanup

def jpeg_cleanup():

    try:
        os.unlink( 'tmp/albania.jpg' )
    except:
        pass
    try:
        os.unlink( 'tmp/albania.jpg.ovr' )
    except:
        pass

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
    jpeg_11,
    jpeg_12,
    jpeg_13,
    jpeg_14,
    jpeg_15,
    jpeg_16,
    jpeg_17,
    jpeg_18,
    jpeg_19,
    jpeg_20,
    jpeg_21,
    jpeg_22,
    jpeg_23,
    jpeg_24,
    jpeg_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'jpeg' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

