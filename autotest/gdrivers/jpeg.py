#!/usr/bin/env pytest
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
import shutil
import struct

import gdaltest

from osgeo import gdal
from osgeo import gdalconst
import pytest

###############################################################################
# Perform simple read test.


def test_jpeg_1():

    ds = gdal.Open('data/albania.jpg')
    cs = ds.GetRasterBand(2).Checksum()
    if cs == 34296:
        gdaltest.jpeg_version = '9b'
    elif cs == 34298:
        gdaltest.jpeg_version = '8'
    else:
        gdaltest.jpeg_version = 'pre8'
    ds = None

    if gdaltest.jpeg_version == '9b':
        tst = gdaltest.GDALTest('JPEG', 'albania.jpg', 2, 34296)
    elif gdaltest.jpeg_version == '8':
        tst = gdaltest.GDALTest('JPEG', 'albania.jpg', 2, 34298)
    else:
        tst = gdaltest.GDALTest('JPEG', 'albania.jpg', 2, 17016)
    return tst.testOpen()

###############################################################################
# Verify EXIF metadata, color interpretation and image_structure


def test_jpeg_2():

    ds = gdal.Open('data/albania.jpg')

    md = ds.GetMetadata()

    ds.GetFileList()

    try:
        assert (not (md['EXIF_GPSLatitudeRef'] != 'N' or
                md['EXIF_GPSLatitude'] != '(41) (1) (22.91)' or
                md['EXIF_PixelXDimension'] != '361' or
                md['EXIF_GPSVersionID'] != '0x02 0x00 0x00 0x00' or
                md['EXIF_ExifVersion'] != '0210' or
                md['EXIF_XResolution'] != '(96)')), 'Exif metadata wrong.'
    except KeyError:
        print(md)
        pytest.fail('Exit metadata apparently missing.')

    assert ds.GetRasterBand(3).GetRasterColorInterpretation() == gdal.GCI_BlueBand, \
        'Did not get expected color interpretation.'

    md = ds.GetMetadata('IMAGE_STRUCTURE')
    assert 'INTERLEAVE' in md and md['INTERLEAVE'] == 'PIXEL', \
        'missing INTERLEAVE metadata'
    assert 'COMPRESSION' in md and md['COMPRESSION'] == 'JPEG', \
        'missing INTERLEAVE metadata'

    md = ds.GetRasterBand(3).GetMetadata('IMAGE_STRUCTURE')
    assert 'COMPRESSION' in md and md['COMPRESSION'] == 'JPEG', \
        'missing INTERLEAVE metadata'

###############################################################################
# Create simple copy and check (greyscale) using progressive option.


def test_jpeg_3():

    ds = gdal.Open('data/byte.tif')

    options = ['PROGRESSIVE=YES',
               'QUALITY=50',
               'WORLDFILE=YES']
    ds = gdal.GetDriverByName('JPEG').CreateCopy('tmp/byte.jpg', ds,
                                                 options=options)

    # IJG, MozJPEG
    expected_cs = [4794, 4787]

    assert ds.GetRasterBand(1).Checksum() in expected_cs, \
        'Wrong checksum on copied image.'

    assert ds.GetRasterBand(1).GetRasterColorInterpretation() == gdal.GCI_GrayIndex, \
        'Wrong color interpretation.'

    expected_gt = [440720.0, 60.0, 0.0, 3751320.0, 0.0, -60.0]
    gt = ds.GetGeoTransform()
    for i in range(6):
        assert abs(gt[i] - expected_gt[i]) <= 1e-6, 'did not get expected geotransform from PAM'

    ds = None

    os.unlink('tmp/byte.jpg.aux.xml')

    try:
        os.stat('tmp/byte.wld')
    except OSError:
        pytest.fail('should have .wld file at that point')

    ds = gdal.Open('tmp/byte.jpg')
    expected_gt = [440720.0, 60.0, 0.0, 3751320.0, 0.0, -60.0]
    gt = ds.GetGeoTransform()
    for i in range(6):
        assert abs(gt[i] - expected_gt[i]) <= 1e-6, \
            'did not get expected geotransform from .wld'
    ds = None

    ds = gdal.Open('tmp/byte.jpg')
    ds.GetFileList()
    ds = None

    gdal.GetDriverByName('JPEG').Delete('tmp/byte.jpg')

    assert not os.path.exists('tmp/byte.wld')

    
###############################################################################
# Verify masked jpeg.


def test_jpeg_4():

    try:
        gdalconst.GMF_ALL_VALID
    except AttributeError:
        pytest.skip()

    ds = gdal.Open('data/masked.jpg')

    refband = ds.GetRasterBand(1)

    assert refband.GetMaskFlags() == gdalconst.GMF_PER_DATASET, 'wrong mask flags'

    cs = refband.GetMaskBand().Checksum()
    assert cs == 770, 'Wrong mask checksum'

###############################################################################
# Verify CreateCopy() of masked jpeg.


def test_jpeg_5():

    try:
        gdalconst.GMF_ALL_VALID
    except AttributeError:
        pytest.skip()

    ds = gdal.Open('data/masked.jpg')

    ds2 = gdal.GetDriverByName('JPEG').CreateCopy('tmp/masked.jpg', ds)

    refband = ds2.GetRasterBand(1)

    assert refband.GetMaskFlags() == gdalconst.GMF_PER_DATASET, 'wrong mask flags'

    cs = refband.GetMaskBand().Checksum()
    assert cs == 770, 'Wrong checksum on copied images mask.'

    refband = None
    ds2 = None
    gdal.GetDriverByName('JPEG').Delete('tmp/masked.jpg')

###############################################################################
# Verify ability to open file with corrupt metadata (#1904).  Note the file
# data/vophead.jpg is truncated to keep the size small, but this should
# not affect opening the file which just reads the header.


def test_jpeg_6():

    ds = gdal.Open('data/vophead.jpg')

    # Because of the optimization in r17446, we shouldn't yet get this error.
    assert (not (gdal.GetLastErrorType() == 2 and
            gdal.GetLastErrorMsg().find('Ignoring EXIF') != -1)), 'got error too soon.'

    with gdaltest.error_handler('CPLQuietErrorHandler'):
        # Get this warning:
        #   Ignoring EXIF directory with unlikely entry count (65499).
        md = ds.GetMetadata()

    # Did we get an exif related warning?
    assert (not (gdal.GetLastErrorType() != 2 or
            gdal.GetLastErrorMsg().find('Ignoring EXIF') == -1)), \
        'did not get expected error.'

    assert len(md) == 1 and md['EXIF_Software'] == 'IrfanView', \
        'did not get expected metadata.'

    ds = None


###############################################################################
# Test creating an in memory copy.

def test_jpeg_7():

    ds = gdal.Open('data/byte.tif')

    options = ['PROGRESSIVE=YES',
               'QUALITY=50']
    ds = gdal.GetDriverByName('JPEG').CreateCopy('/vsimem/byte.jpg', ds,
                                                 options=options)

    # IJG, MozJPEG
    expected_cs = [4794, 4787]

    assert ds.GetRasterBand(1).Checksum() in expected_cs, \
        'Wrong checksum on copied image.'

    ds = None
    gdal.GetDriverByName('JPEG').Delete('/vsimem/byte.jpg')

###############################################################################
# Read a CMYK image as a RGB image


def test_jpeg_8():

    ds = gdal.Open('data/rgb_ntf_cmyk.jpg')

    expected_cs = 20385

    assert ds.GetRasterBand(1).Checksum() == expected_cs, \
        'Wrong checksum on copied image.'

    assert ds.GetRasterBand(1).GetRasterColorInterpretation() == gdal.GCI_RedBand, \
        'Wrong color interpretation.'

    expected_cs = 20865

    assert ds.GetRasterBand(2).Checksum() == expected_cs, \
        'Wrong checksum on copied image.'

    assert ds.GetRasterBand(2).GetRasterColorInterpretation() == gdal.GCI_GreenBand, \
        'Wrong color interpretation.'

    expected_cs = 19441

    assert ds.GetRasterBand(3).Checksum() == expected_cs, \
        'Wrong checksum on copied image.'

    assert ds.GetRasterBand(3).GetRasterColorInterpretation() == gdal.GCI_BlueBand, \
        'Wrong color interpretation.'

    md = ds.GetMetadata('IMAGE_STRUCTURE')

    assert 'SOURCE_COLOR_SPACE' in md and md['SOURCE_COLOR_SPACE'] == 'CMYK', \
        'missing SOURCE_COLOR_SPACE metadata'

###############################################################################
# Read a CMYK image as a CMYK image


def test_jpeg_9():

    gdal.SetConfigOption('GDAL_JPEG_TO_RGB', 'NO')
    ds = gdal.Open('data/rgb_ntf_cmyk.jpg')
    gdal.SetConfigOption('GDAL_JPEG_TO_RGB', 'YES')

    expected_cs = 21187

    assert ds.GetRasterBand(1).Checksum() == expected_cs, \
        'Wrong checksum on copied image.'

    assert ds.GetRasterBand(1).GetRasterColorInterpretation() == gdal.GCI_CyanBand, \
        'Wrong color interpretation.'

    expected_cs = 21054

    assert ds.GetRasterBand(2).Checksum() == expected_cs, \
        'Wrong checksum on copied image.'

    assert ds.GetRasterBand(2).GetRasterColorInterpretation() == gdal.GCI_MagentaBand, \
        'Wrong color interpretation.'

    expected_cs = 21499

    assert ds.GetRasterBand(3).Checksum() == expected_cs, \
        'Wrong checksum on copied image.'

    assert ds.GetRasterBand(3).GetRasterColorInterpretation() == gdal.GCI_YellowBand, \
        'Wrong color interpretation.'

    expected_cs = 21069

    assert ds.GetRasterBand(4).Checksum() == expected_cs, \
        'Wrong checksum on copied image.'

    assert ds.GetRasterBand(4).GetRasterColorInterpretation() == gdal.GCI_BlackBand, \
        'Wrong color interpretation.'

###############################################################################
# Check reading a 12-bit JPEG


def test_jpeg_10():

    if gdaltest.jpeg_version == '9b':  # Fails for some reason
        pytest.skip()

    # Check if JPEG driver supports 12bit JPEG reading/writing
    drv = gdal.GetDriverByName('JPEG')
    md = drv.GetMetadata()
    if md[gdal.DMD_CREATIONDATATYPES].find('UInt16') == -1:
        sys.stdout.write('(12bit jpeg not available) ... ')
        pytest.skip()

    try:
        os.remove('data/12bit_rose_extract.jpg.aux.xml')
    except OSError:
        pass

    ds = gdal.Open('data/12bit_rose_extract.jpg')
    assert ds.GetRasterBand(1).DataType == gdal.GDT_UInt16
    stats = ds.GetRasterBand(1).GetStatistics(0, 1)
    assert stats[2] >= 3613 and stats[2] <= 3614
    ds = None

    try:
        os.remove('data/12bit_rose_extract.jpg.aux.xml')
    except OSError:
        pass

    
###############################################################################
# Check creating a 12-bit JPEG


def test_jpeg_11():

    if gdaltest.jpeg_version == '9b':  # Fails for some reason
        pytest.skip()

    # Check if JPEG driver supports 12bit JPEG reading/writing
    drv = gdal.GetDriverByName('JPEG')
    md = drv.GetMetadata()
    if md[gdal.DMD_CREATIONDATATYPES].find('UInt16') == -1:
        sys.stdout.write('(12bit jpeg not available) ... ')
        pytest.skip()

    ds = gdal.Open('data/12bit_rose_extract.jpg')
    out_ds = gdal.GetDriverByName('JPEG').CreateCopy('tmp/jpeg11.jpg', ds)
    del out_ds

    ds = gdal.Open('tmp/jpeg11.jpg')
    assert ds.GetRasterBand(1).DataType == gdal.GDT_UInt16
    stats = ds.GetRasterBand(1).GetStatistics(0, 1)
    assert stats[2] >= 3613 and stats[2] <= 3614
    ds = None

    gdal.GetDriverByName('JPEG').Delete('tmp/jpeg11.jpg')

###############################################################################
# Test reading a stored JPEG in ZIP (#3908)


def test_jpeg_12():

    ds = gdal.Open('/vsizip/data/byte_jpg.zip')
    assert ds is not None

    gdal.ErrorReset()
    ds.GetRasterBand(1).Checksum()
    assert gdal.GetLastErrorMsg() == ''

    gdal.ErrorReset()
    ds.GetRasterBand(1).GetMaskBand().Checksum()
    assert gdal.GetLastErrorMsg() == ''

    ds = None

###############################################################################
# Test writing to /vsistdout/


def test_jpeg_13():

    src_ds = gdal.Open('data/byte.tif')
    ds = gdal.GetDriverByName('JPEG').CreateCopy(
        '/vsistdout_redirect//vsimem/tmp.jpg', src_ds)
    assert ds.GetRasterBand(1).Checksum() == 0
    ds.ReadRaster(0, 0, 1, 1)
    src_ds = None
    ds = None

    ds = gdal.Open('/vsimem/tmp.jpg')
    assert ds is not None

    gdal.Unlink('/vsimem/tmp.jpg')

###############################################################################
# Test writing to /vsistdout/


def test_jpeg_14():

    if gdaltest.jpeg_version == '9b':  # Fails for some reason
        pytest.skip()

    # Check if JPEG driver supports 12bit JPEG reading/writing
    drv = gdal.GetDriverByName('JPEG')
    md = drv.GetMetadata()
    if md[gdal.DMD_CREATIONDATATYPES].find('UInt16') == -1:
        sys.stdout.write('(12bit jpeg not available) ... ')
        pytest.skip()

    src_ds = gdal.Open('data/12bit_rose_extract.jpg')
    ds = drv.CreateCopy('/vsistdout_redirect//vsimem/tmp.jpg', src_ds)
    assert ds.GetRasterBand(1).Checksum() == 0
    src_ds = None
    ds = None

    ds = gdal.Open('/vsimem/tmp.jpg')
    assert ds is not None

    gdal.Unlink('/vsimem/tmp.jpg')

###############################################################################
# Test CreateCopy() interruption


def test_jpeg_15():

    tst = gdaltest.GDALTest('JPEG', 'albania.jpg', 2, 17016)

    return tst.testCreateCopy(vsimem=1, interrupt_during_copy=True)

###############################################################################
# Test overview support


def test_jpeg_16():

    shutil.copy('data/albania.jpg', 'tmp/albania.jpg')
    gdal.Unlink('tmp/albania.jpg.ovr')

    ds = gdal.Open('tmp/albania.jpg')
    assert ds.GetRasterBand(1).GetOverviewCount() == 1
    assert ds.GetRasterBand(1).GetOverview(-1) is None
    assert ds.GetRasterBand(1).GetOverview(1) is None
    assert ds.GetRasterBand(1).GetOverview(0) is not None
    # "Internal" overview

    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    if gdaltest.jpeg_version in ('8', '9b'):
        expected_cs = 34218
    else:
        expected_cs = 31892
    assert cs == expected_cs

    # Build external overviews
    ds.BuildOverviews('NEAR', [2, 4])
    assert ds.GetRasterBand(1).GetOverviewCount() == 2
    # Check updated checksum
    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    if gdaltest.jpeg_version in ('8', '9b'):
        expected_cs = 33698
    else:
        expected_cs = 32460
    assert cs == expected_cs

    ds = None

    # Check we are using external overviews
    ds = gdal.Open('tmp/albania.jpg')
    assert ds.GetRasterBand(1).GetOverviewCount() == 2
    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    if gdaltest.jpeg_version in ('8', '9b'):
        expected_cs = 33698
    else:
        expected_cs = 32460
    assert cs == expected_cs

    ds = None

###############################################################################
# Test bogus files


def test_jpeg_17():
    gdal.ErrorReset()
    with gdaltest.error_handler('CPLQuietErrorHandler'):
        ds = gdal.Open('data/bogus.jpg')
        assert (not (ds is not None or
            gdal.GetLastErrorType() != gdal.CE_Failure or
                gdal.GetLastErrorMsg() == ''))

    gdal.ErrorReset()
    ds = gdal.Open('data/byte_corrupted.jpg')
    with gdaltest.error_handler('CPLQuietErrorHandler'):
            # ERROR 1: libjpeg: Huffman table 0x00 was not defined
        cs = ds.GetRasterBand(1).Checksum()
    if (gdal.GetLastErrorType() != gdal.CE_Failure or
            gdal.GetLastErrorMsg() == ''):
        # libjpeg-turbo 1.4.0 doesn't emit errors...
        assert cs == 4925

    gdal.ErrorReset()
    ds = gdal.Open('data/byte_corrupted2.jpg')
    with gdaltest.error_handler('CPLQuietErrorHandler'):
        # Get this warning:
        #   libjpeg: Corrupt JPEG data: found marker 0x00 instead of RST63
        ds.GetRasterBand(1).Checksum()

    assert (not (gdal.GetLastErrorType() != gdal.CE_Warning or
            gdal.GetLastErrorMsg() == ''))

    gdal.ErrorReset()
    ds = gdal.Open('data/byte_corrupted2.jpg')
    with gdaltest.error_handler('CPLQuietErrorHandler'):
        gdal.SetConfigOption('GDAL_ERROR_ON_LIBJPEG_WARNING', 'TRUE')
        # Get this ERROR 1:
        #   libjpeg: Corrupt JPEG data: found marker 0x00 instead of RST63
        ds.GetRasterBand(1).Checksum()
        gdal.SetConfigOption('GDAL_ERROR_ON_LIBJPEG_WARNING', None)

    assert (not (gdal.GetLastErrorType() != gdal.CE_Failure or
            gdal.GetLastErrorMsg() == ''))

###############################################################################
# Test situation where we cause a restart and need to reset scale


def test_jpeg_18():
    height = 1024
    width = 1024
    src_ds = gdal.GetDriverByName('GTiff').Create('/vsimem/jpeg_18.tif',
                                                  width, height, 1)
    for i in range(height):
        data = struct.pack('B' * 1, int(i / (height / 256)))
        src_ds.WriteRaster(0, i, width, 1, data, 1, 1)

    ds = gdal.GetDriverByName('JPEG').CreateCopy('/vsimem/jpeg_18.jpg', src_ds,
                                                 options=['QUALITY=99'])
    src_ds = None
    gdal.Unlink('/vsimem/jpeg_18.tif')

    oldSize = gdal.GetCacheMax()
    gdal.SetCacheMax(0)

    line0 = ds.GetRasterBand(1).ReadRaster(0, 0, width, 1)
    data = struct.unpack('B' * width, line0)
    assert abs(data[0] - 0) <= 10
    line1023 = ds.GetRasterBand(1).ReadRaster(0, height - 1, width, 1)
    data = struct.unpack('B' * width, line1023)
    assert abs(data[0] - 255) <= 10
    line0_ovr1 = ds.GetRasterBand(1).GetOverview(1).ReadRaster(0, 0,
                                                               int(width / 4),
                                                               1)
    data = struct.unpack('B' * (int(width / 4)), line0_ovr1)
    assert abs(data[0] - 0) <= 10
    line1023_bis = ds.GetRasterBand(1).ReadRaster(0, height - 1, width, 1)
    assert line1023_bis != line0 and line1023 == line1023_bis
    line0_bis = ds.GetRasterBand(1).ReadRaster(0, 0, width, 1)
    assert line0 == line0_bis
    line255_ovr1 = ds.GetRasterBand(1).GetOverview(1).ReadRaster(
        0, int(height / 4) - 1, int(width / 4), 1)
    data = struct.unpack('B' * int(width / 4), line255_ovr1)
    assert abs(data[0] - 255) <= 10
    line0_bis = ds.GetRasterBand(1).ReadRaster(0, 0, width, 1)
    assert line0 == line0_bis
    line0_ovr1_bis = ds.GetRasterBand(1).GetOverview(1).ReadRaster(
        0, 0, int(width / 4), 1)
    assert line0_ovr1 == line0_ovr1_bis
    line255_ovr1_bis = ds.GetRasterBand(1).GetOverview(1).ReadRaster(
        0, int(height / 4) - 1, int(width / 4), 1)
    assert line255_ovr1 == line255_ovr1_bis

    gdal.SetCacheMax(oldSize)

    ds = None
    gdal.Unlink('/vsimem/jpeg_18.jpg')

###############################################################################
# Test MSB ordering of bits in mask (#5102)


def test_jpeg_19():

    for width, height, iX in [(32, 32, 12), (25, 25, 8), (24, 25, 8)]:
        src_ds = gdal.GetDriverByName('GTiff').Create('/vsimem/jpeg_19.tif',
                                                      width, height, 1)
        src_ds.CreateMaskBand(gdal.GMF_PER_DATASET)
        src_ds.GetRasterBand(1).GetMaskBand().WriteRaster(
            0, 0, iX, height, struct.pack('B' * 1, 255), 1, 1)
        src_ds.GetRasterBand(1).GetMaskBand().WriteRaster(
            iX, 0, width - iX, height, struct.pack('B' * 1, 0), 1, 1)
        tiff_mask_data = src_ds.GetRasterBand(1).GetMaskBand().ReadRaster(
            0, 0, width, height)

        # Generate a JPEG file with a (default) LSB bit mask order
        out_ds = gdal.GetDriverByName('JPEG').CreateCopy('/vsimem/jpeg_19.jpg',
                                                         src_ds)
        out_ds = None

        # Generate a JPEG file with a MSB bit mask order
        gdal.SetConfigOption('JPEG_WRITE_MASK_BIT_ORDER', 'MSB')
        out_ds = gdal.GetDriverByName('JPEG').CreateCopy(
            '/vsimem/jpeg_19_msb.jpg', src_ds)
        del out_ds
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
            assert data1 == data2
        else:
            assert data1 != data2

        # Check the file with the LSB bit mask order
        ds = gdal.Open('/vsimem/jpeg_19.jpg')
        jpg_mask_data = ds.GetRasterBand(1).GetMaskBand().ReadRaster(
            0, 0, width, height)
        ds = None
        assert tiff_mask_data == jpg_mask_data

        # Check the file with the MSB bit mask order
        ds = gdal.Open('/vsimem/jpeg_19_msb.jpg')
        jpg_mask_data = ds.GetRasterBand(1).GetMaskBand().ReadRaster(
            0, 0, width, height)
        ds = None
        assert tiff_mask_data == jpg_mask_data

        gdal.GetDriverByName('GTiff').Delete('/vsimem/jpeg_19.tif')
        gdal.GetDriverByName('JPEG').Delete('/vsimem/jpeg_19.jpg')
        gdal.GetDriverByName('JPEG').Delete('/vsimem/jpeg_19_msb.jpg')

    
###############################################################################
# Test correct GCP reading with PAM (#5352)


def test_jpeg_20():

    src_ds = gdal.Open('data/rgb_gcp.vrt')
    ds = gdal.GetDriverByName('JPEG').CreateCopy('/vsimem/jpeg_20.jpg', src_ds)
    ds = None

    ds = gdal.Open('/vsimem/jpeg_20.jpg')
    assert ds.GetGCPProjection().find('GEOGCS["WGS 84"') == 0
    assert ds.GetGCPCount() == 4
    assert len(ds.GetGCPs()) == 4
    ds = None

    gdal.GetDriverByName('JPEG').Delete('/vsimem/jpeg_20.jpg')

###############################################################################
# Test implicit and EXIF overviews


def test_jpeg_21():

    ds = gdal.Open('data/black_with_white_exif_ovr.jpg')
    assert ds.GetRasterBand(1).GetOverviewCount() == 3
    expected_dim_cs = [[512, 512, 0], [256, 256, 0], [196, 196, 12681]]
    i = 0
    for expected_w, expected_h, expected_cs in expected_dim_cs:
        ovr = ds.GetRasterBand(1).GetOverview(i)
        cs = ovr.Checksum()
        assert (not (ovr.XSize != expected_w or
            ovr.YSize != expected_h or
                cs != expected_cs))
        i = i + 1
    ds = None

###############################################################################
# Test generation of EXIF overviews


def test_jpeg_22():

    src_ds = gdal.GetDriverByName('Mem').Create('', 4096, 2048)
    src_ds.GetRasterBand(1).Fill(255)
    ds = gdal.GetDriverByName('JPEG').CreateCopy(
        '/vsimem/jpeg_22.jpg', src_ds, options=['EXIF_THUMBNAIL=YES'])
    src_ds = None
    assert ds.GetRasterBand(1).GetOverviewCount() == 4
    ovr = ds.GetRasterBand(1).GetOverview(3)
    cs = ovr.Checksum()
    assert ovr.XSize == 128 and ovr.YSize == 64 and cs == 34957
    ds = None

    # With 3 bands
    src_ds = gdal.GetDriverByName('Mem').Create('', 2048, 4096, 3)
    src_ds.GetRasterBand(1).Fill(255)
    ds = gdal.GetDriverByName('JPEG').CreateCopy(
        '/vsimem/jpeg_22.jpg', src_ds, options=['EXIF_THUMBNAIL=YES'])
    src_ds = None
    ovr = ds.GetRasterBand(1).GetOverview(3)
    assert ovr.XSize == 64 and ovr.YSize == 128
    ds = None

    # With comment
    src_ds = gdal.GetDriverByName('Mem').Create('', 2048, 4096)
    src_ds.GetRasterBand(1).Fill(255)
    ds = gdal.GetDriverByName('JPEG').CreateCopy(
        '/vsimem/jpeg_22.jpg', src_ds,
        options=['COMMENT=foo', 'EXIF_THUMBNAIL=YES', 'THUMBNAIL_WIDTH=40'])
    src_ds = None
    ovr = ds.GetRasterBand(1).GetOverview(3)
    assert ds.GetMetadataItem('COMMENT') == 'foo'
    assert ovr.XSize == 40 and ovr.YSize == 80
    ds = None

    src_ds = gdal.GetDriverByName('Mem').Create('', 2048, 4096)
    src_ds.GetRasterBand(1).Fill(255)
    ds = gdal.GetDriverByName('JPEG').CreateCopy(
        '/vsimem/jpeg_22.jpg', src_ds,
        options=['EXIF_THUMBNAIL=YES', 'THUMBNAIL_HEIGHT=60'])
    src_ds = None
    ovr = ds.GetRasterBand(1).GetOverview(3)
    assert ovr.XSize == 30 and ovr.YSize == 60
    ds = None

    src_ds = gdal.GetDriverByName('Mem').Create('', 2048, 4096)
    src_ds.GetRasterBand(1).Fill(255)
    ds = gdal.GetDriverByName('JPEG').CreateCopy(
        '/vsimem/jpeg_22.jpg', src_ds,
        options=['EXIF_THUMBNAIL=YES',
                 'THUMBNAIL_WIDTH=50',
                 'THUMBNAIL_HEIGHT=40'])
    src_ds = None
    ovr = ds.GetRasterBand(1).GetOverview(3)
    assert ovr.XSize == 50 and ovr.YSize == 40
    ds = None

    gdal.Unlink('/vsimem/jpeg_22.jpg')

###############################################################################
# Test optimized JPEG IRasterIO


def test_jpeg_23():
    ds = gdal.Open('data/albania.jpg')
    cs = [ds.GetRasterBand(i + 1).Checksum() for i in range(3)]

    # Band interleaved
    data = ds.ReadRaster(0, 0, ds.RasterXSize, ds.RasterYSize)
    tmp_ds = gdal.GetDriverByName('Mem').Create(
        '', ds.RasterXSize, ds.RasterYSize, 3)
    tmp_ds.WriteRaster(0, 0, ds.RasterXSize, ds.RasterYSize, data)
    got_cs = [tmp_ds.GetRasterBand(i + 1).Checksum() for i in range(3)]
    assert cs == got_cs

    # Pixel interleaved
    data = ds.ReadRaster(0, 0, ds.RasterXSize, ds.RasterYSize,
                         buf_pixel_space=3, buf_band_space=1)
    tmp_ds = gdal.GetDriverByName('Mem').Create(
        '', ds.RasterXSize, ds.RasterYSize, 3)
    tmp_ds.WriteRaster(0, 0, ds.RasterXSize, ds.RasterYSize, data,
                       buf_pixel_space=3, buf_band_space=1)
    got_cs = [tmp_ds.GetRasterBand(i + 1).Checksum() for i in range(3)]
    assert cs == got_cs

    # Pixel interleaved with padding
    data = ds.ReadRaster(0, 0, ds.RasterXSize, ds.RasterYSize,
                         buf_pixel_space=4, buf_band_space=1)
    tmp_ds = gdal.GetDriverByName('Mem').Create(
        '', ds.RasterXSize, ds.RasterYSize, 3)
    tmp_ds.WriteRaster(0, 0, ds.RasterXSize, ds.RasterYSize, data,
                       buf_pixel_space=4, buf_band_space=1)
    got_cs = [tmp_ds.GetRasterBand(i + 1).Checksum() for i in range(3)]
    assert cs == got_cs

###############################################################################
# Test Arithmetic coding (and if not enabled, will trigger error code handling
# in CreateCopy())


def test_jpeg_24():

    has_arithmetic = bool(gdal.GetDriverByName('JPEG').GetMetadataItem(
        'DMD_CREATIONOPTIONLIST').find('ARITHMETIC') >= 0)

    src_ds = gdal.Open('data/byte.tif')
    if not has_arithmetic:
        gdal.PushErrorHandler()
    ds = gdal.GetDriverByName('JPEG').CreateCopy('/vsimem/byte.jpg', src_ds,
                                                 options=['ARITHMETIC=YES'])
    if not has_arithmetic:
        gdal.PopErrorHandler()
    else:
        if gdal.GetLastErrorMsg().find('Requested feature was omitted at compile time') >= 0:
            ds = None
            gdal.Unlink('/vsimem/byte.jpg')
            pytest.skip()

        expected_cs = 4743

        assert ds.GetRasterBand(1).Checksum() == expected_cs, \
            'Wrong checksum on copied image.'

        ds = None
        gdal.GetDriverByName('JPEG').Delete('/vsimem/byte.jpg')

    
###############################################################################
# Test COMMENT


def test_jpeg_25():

    src_ds = gdal.Open('data/byte.tif')
    ds = gdal.GetDriverByName('JPEG').CreateCopy(
        '/vsimem/byte.jpg', src_ds, options=['COMMENT=my comment'])
    ds = None
    ds = gdal.Open('/vsimem/byte.jpg')
    if ds.GetMetadataItem('COMMENT') != 'my comment':
        print(ds.GetMetadata())
        pytest.fail('Wrong comment.')

    ds = None
    gdal.GetDriverByName('JPEG').Delete('/vsimem/byte.jpg')

###############################################################################
# Test creation error


def test_jpeg_26():

    src_ds = gdal.GetDriverByName('Mem').Create('', 70000, 1)
    with gdaltest.error_handler():
        ds = gdal.GetDriverByName('JPEG').CreateCopy(
            '/vsimem/jpeg_26.jpg', src_ds)
    assert ds is None
    gdal.Unlink('/vsimem/jpeg_26.jpg')

###############################################################################
# Test reading a file that contains the 2 denial of service
# vulnerabilities listed in
# http://www.libjpeg-jpeg_26.org/pmwiki/uploads/About/TwoIssueswiththeJPEGStandard.pdf


def test_jpeg_27():

    # Should error out with 'Reading this strip would require
    # libjpeg to allocate at least...'
    gdal.ErrorReset()
    ds = gdal.Open('/vsisubfile/146,/vsizip/../gcore/data/eofloop_valid_huff.tif.zip')
    with gdaltest.error_handler():
        cs = ds.GetRasterBand(1).Checksum()
        assert cs == 0 and gdal.GetLastErrorMsg() != ''

    # Should error out with 'Scan number...
    gdal.ErrorReset()
    ds = gdal.Open('/vsisubfile/146,/vsizip/../gcore/data/eofloop_valid_huff.tif.zip')
    with gdaltest.error_handler():
        gdal.SetConfigOption('GDAL_ALLOW_LARGE_LIBJPEG_MEM_ALLOC', 'YES')
        gdal.SetConfigOption('GDAL_JPEG_MAX_ALLOWED_SCAN_NUMBER', '10')
        cs = ds.GetRasterBand(1).Checksum()
        gdal.SetConfigOption('GDAL_ALLOW_LARGE_LIBJPEG_MEM_ALLOC', None)
        gdal.SetConfigOption('GDAL_JPEG_MAX_ALLOWED_SCAN_NUMBER', None)
        assert gdal.GetLastErrorMsg() != ''

    
###############################################################################
# Test writing of EXIF and GPS tags


def test_jpeg_28():

    tmpfilename = '/vsimem/jpeg_28.jpg'

    # Nothing
    src_ds = gdal.GetDriverByName('MEM').Create('', 1, 1)
    ds = gdal.GetDriverByName('JPEG').CreateCopy(tmpfilename, src_ds)
    src_ds = None
    ds = gdal.Open(tmpfilename)
    assert not ds.GetMetadata()

    # EXIF tags only
    src_ds = gdal.GetDriverByName('MEM').Create('', 1, 1)

    src_ds.SetMetadataItem('EXIF_DateTime', 'dt')  # not enough values ASCII
    src_ds.SetMetadataItem('EXIF_DateTimeOriginal', '01234567890123456789')  # truncated ASCII
    src_ds.SetMetadataItem('EXIF_DateTimeDigitized', '0123456789012345678')  # right number of items ASCII
    src_ds.SetMetadataItem('EXIF_Make', 'make')  # variable ASCII

    src_ds.SetMetadataItem('EXIF_ExifVersion', '01234')  # truncated UNDEFINED
    src_ds.SetMetadataItem('EXIF_ComponentsConfiguration', '0x1F')  # not enough values UNDEFINED
    src_ds.SetMetadataItem('EXIF_FlashpixVersion', 'ABCD')  # right number of items UNDEFINED
    src_ds.SetMetadataItem('EXIF_SpatialFrequencyResponse', '0xab 0xCD')  # variable UNDEFINED

    src_ds.SetMetadataItem('EXIF_Orientation', '10')  # right number of items SHORT
    src_ds.SetMetadataItem('EXIF_ResolutionUnit', '2 4')  # truncated SHORT
    src_ds.SetMetadataItem('EXIF_TransferFunction', '0 1')  # not enough values SHORT
    src_ds.SetMetadataItem('EXIF_ISOSpeedRatings', '1 2 3')  # variable SHORT

    src_ds.SetMetadataItem('EXIF_StandardOutputSensitivity', '123456789')  # right number of items LONG

    src_ds.SetMetadataItem('EXIF_XResolution', '96')  # right number of items RATIONAL
    src_ds.SetMetadataItem('EXIF_YResolution', '96 0')  # truncated RATIONAL
    src_ds.SetMetadataItem('EXIF_CompressedBitsPerPixel', 'nan')  # invalid RATIONAL
    src_ds.SetMetadataItem('EXIF_ApertureValue', '-1')  # invalid RATIONAL

    with gdaltest.error_handler():
        gdal.GetDriverByName('JPEG').CreateCopy(tmpfilename, src_ds)
    src_ds = None
    assert gdal.VSIStatL(tmpfilename + '.aux.xml') is None
    ds = gdal.Open(tmpfilename)
    got_md = ds.GetMetadata()
    expected_md = {'EXIF_DateTimeDigitized': '0123456789012345678', 'EXIF_DateTimeOriginal': '0123456789012345678', 'EXIF_Orientation': '10', 'EXIF_ApertureValue': '(0)', 'EXIF_YResolution': '(96)', 'EXIF_XResolution': '(96)', 'EXIF_TransferFunction': '0 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0', 'EXIF_ExifVersion': '0123', 'EXIF_DateTime': 'dt                 ', 'EXIF_FlashpixVersion': 'ABCD', 'EXIF_ComponentsConfiguration': '0x1f 0x00 0x00 0x00', 'EXIF_Make': 'make', 'EXIF_StandardOutputSensitivity': '123456789', 'EXIF_ResolutionUnit': '2', 'EXIF_CompressedBitsPerPixel': '(0)', 'EXIF_SpatialFrequencyResponse': '0xab 0xcd', 'EXIF_ISOSpeedRatings': '1 2 3'}
    assert got_md == expected_md

    # Test SRATIONAL
    for val in (-1.5, -1, -0.5, 0, 0.5, 1, 1.5):
        src_ds = gdal.GetDriverByName('MEM').Create('', 1, 1)
        src_ds.SetMetadataItem('EXIF_ShutterSpeedValue', str(val))
        gdal.GetDriverByName('JPEG').CreateCopy(tmpfilename, src_ds)
        src_ds = None
        assert gdal.VSIStatL(tmpfilename + '.aux.xml') is None
        ds = gdal.Open(tmpfilename)
        got_val = ds.GetMetadataItem('EXIF_ShutterSpeedValue')
        got_val = got_val.replace('(', '').replace(')', '')
        assert float(got_val) == val, ds.GetMetadataItem('EXIF_ShutterSpeedValue')

    # Test RATIONAL
    for val in (0, 0.5, 1, 1.5):
        src_ds = gdal.GetDriverByName('MEM').Create('', 1, 1)
        src_ds.SetMetadataItem('EXIF_ApertureValue', str(val))
        gdal.GetDriverByName('JPEG').CreateCopy(tmpfilename, src_ds)
        src_ds = None
        assert gdal.VSIStatL(tmpfilename + '.aux.xml') is None
        ds = gdal.Open(tmpfilename)
        got_val = ds.GetMetadataItem('EXIF_ApertureValue')
        got_val = got_val.replace('(', '').replace(')', '')
        assert float(got_val) == val, ds.GetMetadataItem('EXIF_ApertureValue')

    # GPS tags only
    src_ds = gdal.GetDriverByName('MEM').Create('', 1, 1)
    src_ds.SetMetadataItem('EXIF_GPSLatitudeRef', 'N')
    src_ds.SetMetadataItem('EXIF_GPSLatitude', '49 34 56.5')
    gdal.GetDriverByName('JPEG').CreateCopy(tmpfilename, src_ds)
    src_ds = None
    assert gdal.VSIStatL(tmpfilename + '.aux.xml') is None
    ds = gdal.Open(tmpfilename)
    got_md = ds.GetMetadata()
    assert got_md == {'EXIF_GPSLatitudeRef': 'N', 'EXIF_GPSLatitude': '(49) (34) (56.5)'}
    ds = None

    # EXIF and GPS tags
    src_ds = gdal.GetDriverByName('MEM').Create('', 1, 1)
    src_ds.SetMetadataItem('EXIF_ExifVersion', '0231')
    src_ds.SetMetadataItem('EXIF_GPSLatitudeRef', 'N')
    gdal.GetDriverByName('JPEG').CreateCopy(tmpfilename, src_ds)
    src_ds = None
    assert gdal.VSIStatL(tmpfilename + '.aux.xml') is None
    ds = gdal.Open(tmpfilename)
    got_md = ds.GetMetadata()
    assert got_md == {'EXIF_ExifVersion': '0231', 'EXIF_GPSLatitudeRef': 'N'}
    ds = None

    # EXIF and other metadata
    src_ds = gdal.GetDriverByName('MEM').Create('', 1, 1)
    src_ds.SetMetadataItem('EXIF_ExifVersion', '0231')
    src_ds.SetMetadataItem('EXIF_invalid', 'foo')
    src_ds.SetMetadataItem('FOO', 'BAR')
    with gdaltest.error_handler():
        gdal.GetDriverByName('JPEG').CreateCopy(tmpfilename, src_ds)
    src_ds = None
    assert gdal.VSIStatL(tmpfilename + '.aux.xml') is not None
    ds = gdal.Open(tmpfilename)
    got_md = ds.GetMetadata()
    assert got_md == {'EXIF_ExifVersion': '0231', 'EXIF_invalid': 'foo', 'FOO': 'BAR'}
    ds = None

    # Too much content for EXIF
    src_ds = gdal.GetDriverByName('MEM').Create('', 1, 1)
    src_ds.SetMetadataItem('EXIF_UserComment', 'x' * 65535)
    with gdaltest.error_handler():
        gdal.GetDriverByName('JPEG').CreateCopy(tmpfilename, src_ds)
    src_ds = None
    ds = None

    # EXIF and GPS tags and EXIF overview
    src_ds = gdal.GetDriverByName('MEM').Create('', 1024, 1024)
    src_ds.SetMetadataItem('EXIF_ExifVersion', '0231')
    src_ds.SetMetadataItem('EXIF_GPSLatitudeRef', 'N')
    gdal.GetDriverByName('JPEG').CreateCopy(tmpfilename, src_ds,
                                            options=['EXIF_THUMBNAIL=YES', 'THUMBNAIL_WIDTH=32', 'THUMBNAIL_HEIGHT=32'])
    src_ds = None
    assert gdal.VSIStatL(tmpfilename + '.aux.xml') is None
    ds = gdal.Open(tmpfilename)
    got_md = ds.GetMetadata()
    assert got_md == {'EXIF_ExifVersion': '0231', 'EXIF_GPSLatitudeRef': 'N'}
    assert ds.GetRasterBand(1).GetOverview(ds.GetRasterBand(1).GetOverviewCount() - 1).XSize == 32
    ds = None

    gdal.Unlink(tmpfilename)

###############################################################################
# Cleanup


def test_jpeg_cleanup():
    gdal.Unlink('tmp/albania.jpg')
    gdal.Unlink('tmp/albania.jpg.ovr')
