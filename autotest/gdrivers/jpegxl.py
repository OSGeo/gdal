#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read/write functionality for JPEGXL driver.
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2022, Even Rouault <even dot rouault at spatialys.com>
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

import base64

from osgeo import gdal
import pytest
import gdaltest

pytestmark = pytest.mark.require_driver('JPEGXL')


def test_jpegxl_read():
    tst = gdaltest.GDALTest('JPEGXL', 'jpegxl/byte.jxl', 1, 4672)
    return tst.testOpen(check_gt=(440720, 60, 0, 3751320, 0, -60))


def test_jpegxl_byte():
    tst = gdaltest.GDALTest('JPEGXL', 'byte.tif', 1, 4672)
    return tst.testCreateCopy(vsimem=1)


def test_jpegxl_uint16():
    tst = gdaltest.GDALTest('JPEGXL', '../../gcore/data/uint16.tif', 1, 4672)
    return tst.testCreateCopy(vsimem=1)


def test_jpegxl_float32():
    tst = gdaltest.GDALTest('JPEGXL', 'float32.tif', 1, 4672)
    return tst.testCreateCopy(vsimem=1)


def test_jpegxl_grey_alpha():
    tst = gdaltest.GDALTest('JPEGXL', '../../gcore/data/stefan_full_greyalpha.tif', 1, 1970)
    return tst.testCreateCopy(vsimem=1)


def test_jpegxl_rgb():
    tst = gdaltest.GDALTest('JPEGXL', 'rgbsmall.tif', 1, 21212)
    return tst.testCreateCopy(vsimem=1)


def test_jpegxl_rgba():
    tst = gdaltest.GDALTest('JPEGXL', '../../gcore/data/stefan_full_rgba.tif', 1, 12603)
    return tst.testCreateCopy(vsimem=1)


def test_jpegxl_rgba_lossless_no():

    src_ds = gdal.Open('../gcore/data/stefan_full_rgba.tif')
    outfilename = '/vsimem/out.jxl'
    gdal.GetDriverByName('JPEGXL').CreateCopy(outfilename, src_ds, options = ['LOSSLESS=NO'])
    ds = gdal.Open(outfilename)
    cs = ds.GetRasterBand(1).Checksum()
    assert cs != 0 and cs != src_ds.GetRasterBand(1).Checksum()

    ds = None
    gdal.GetDriverByName('JPEGXL').Delete(outfilename)


def test_jpegxl_rgba_distance():

    src_ds = gdal.Open('../gcore/data/stefan_full_rgba.tif')
    outfilename = '/vsimem/out.jxl'
    gdal.GetDriverByName('JPEGXL').CreateCopy(outfilename, src_ds, options = ['DISTANCE=2'])
    ds = gdal.Open(outfilename)
    cs = ds.GetRasterBand(1).Checksum()
    assert cs != 0 and cs != src_ds.GetRasterBand(1).Checksum()

    ds = None
    gdal.GetDriverByName('JPEGXL').Delete(outfilename)


@pytest.mark.parametrize('quality,equivalent_distance', [(100,0),
                                                         (90,1),
                                                         (10,12.65)])
def test_jpegxl_rgba_quality(quality, equivalent_distance):

    src_ds = gdal.Open('../gcore/data/stefan_full_rgba.tif')
    outfilename = '/vsimem/out.jxl'

    gdal.GetDriverByName('JPEGXL').CreateCopy(outfilename, src_ds, options = ['QUALITY=' + str(quality)])
    ds = gdal.Open(outfilename)
    cs = ds.GetRasterBand(1).Checksum()
    assert cs != 0 and cs != src_ds.GetRasterBand(1).Checksum()

    with gdaltest.error_handler():
        gdal.GetDriverByName('JPEGXL').CreateCopy(outfilename, src_ds, options = ['DISTANCE=' + str(equivalent_distance)])
    ds = gdal.Open(outfilename)
    assert ds.GetRasterBand(1).Checksum() == cs

    ds = None
    gdal.GetDriverByName('JPEGXL').Delete(outfilename)


def test_jpegxl_xmp():

    if 'COMPRESS_BOX' not in gdal.GetDriverByName('JPEGXL').GetMetadataItem('DMD_CREATIONOPTIONLIST'):
        pytest.skip()

    src_ds = gdal.Open('data/gtiff/byte_with_xmp.tif')
    outfilename = '/vsimem/out.jxl'
    gdal.GetDriverByName('JPEGXL').CreateCopy(outfilename, src_ds)
    assert gdal.VSIStatL(outfilename + '.aux.xml') is None
    ds = gdal.Open(outfilename)
    assert set(ds.GetMetadataDomainList()) == set(['DERIVED_SUBDATASETS', 'xml:XMP'])
    assert ds.GetMetadata('xml:XMP')[0].startswith('<?xpacket')

    ds = None
    gdal.GetDriverByName('JPEGXL').Delete(outfilename)


def test_jpegxl_exif():

    if 'COMPRESS_BOX' not in gdal.GetDriverByName('JPEGXL').GetMetadataItem('DMD_CREATIONOPTIONLIST'):
        pytest.skip()

    src_ds = gdal.Open('../gcore/data/exif_and_gps.tif')
    outfilename = '/vsimem/out.jxl'
    gdal.GetDriverByName('JPEGXL').CreateCopy(outfilename, src_ds)
    gdal.Unlink(outfilename + '.aux.xml')
    ds = gdal.Open(outfilename)
    assert set(ds.GetMetadataDomainList()) == set(['DERIVED_SUBDATASETS', 'IMAGE_STRUCTURE', 'EXIF'])
    assert src_ds.GetMetadata('EXIF') == ds.GetMetadata('EXIF')

    ds = None
    gdal.GetDriverByName('JPEGXL').Delete(outfilename)


def test_jpegxl_read_huge_xmp_compressed_box():

    if 'COMPRESS_BOX' not in gdal.GetDriverByName('JPEGXL').GetMetadataItem('DMD_CREATIONOPTIONLIST'):
        pytest.skip()

    with gdaltest.error_handler():
        gdal.ErrorReset()
        ds = gdal.Open('data/jpegxl/huge_xmp_compressed_box.jxl')
        assert ds is not None
        assert gdal.GetLastErrorMsg() != ''


def test_jpegxl_uint8_7_bits():

    src_ds = gdal.Open('data/byte.tif')
    rescaled_ds = gdal.Translate('', src_ds, options = '-of MEM -scale 0 255 0 127')
    outfilename = '/vsimem/out.jxl'
    gdal.GetDriverByName('JPEGXL').CreateCopy(outfilename, rescaled_ds, options = ['NBITS=7'])
    ds = gdal.Open(outfilename)
    assert ds.GetRasterBand(1).Checksum() == rescaled_ds.GetRasterBand(1).Checksum()
    assert ds.GetRasterBand(1).GetMetadataItem('NBITS', 'IMAGE_STRUCTURE') == '7'

    ds = None
    gdal.GetDriverByName('JPEGXL').Delete(outfilename)


def test_jpegxl_uint16_12_bits():

    src_ds = gdal.Open('../gcore/data/uint16.tif')
    outfilename = '/vsimem/out.jxl'
    gdal.GetDriverByName('JPEGXL').CreateCopy(outfilename, src_ds, options = ['NBITS=12'])
    ds = gdal.Open(outfilename)
    assert ds.GetRasterBand(1).Checksum() == 4672
    assert ds.GetRasterBand(1).GetMetadataItem('NBITS', 'IMAGE_STRUCTURE') == '12'

    ds = None
    gdal.GetDriverByName('JPEGXL').Delete(outfilename)


def test_jpegxl_rasterio():

    src_ds = gdal.Open('data/rgbsmall.tif')
    outfilename = '/vsimem/out.jxl'
    gdal.GetDriverByName('JPEGXL').CreateCopy(outfilename, src_ds)
    ds = gdal.Open(outfilename)

    # Optimized code path: read directly in target buffer
    for i in range(2):
        got_data = ds.ReadRaster(
            buf_pixel_space = 3,
            buf_line_space = 3 * src_ds.RasterXSize,
            buf_band_space = 1)
        expected_data = src_ds.ReadRaster(
            buf_pixel_space = 3,
            buf_line_space = 3 * src_ds.RasterXSize,
            buf_band_space = 1)
        assert got_data == expected_data

    # Optimized code path: do not use block cache
    got_data = ds.ReadRaster(
        buf_type = gdal.GDT_UInt16,
        buf_pixel_space = 2 * 3,
        buf_line_space = 2 * 3 * src_ds.RasterXSize,
        buf_band_space = 2)
    expected_data = src_ds.ReadRaster(
        buf_type = gdal.GDT_UInt16,
        buf_pixel_space = 2 * 3,
        buf_line_space = 2 * 3 * src_ds.RasterXSize,
        buf_band_space = 2)
    assert got_data == expected_data

    got_data = ds.ReadRaster(
        band_list = [1, 2],
        buf_type = gdal.GDT_UInt16,
        buf_pixel_space = 2 * 2,
        buf_line_space = 2 * 2 * src_ds.RasterXSize,
        buf_band_space = 2)
    expected_data = src_ds.ReadRaster(
        band_list = [1, 2],
        buf_type = gdal.GDT_UInt16,
        buf_pixel_space = 2 * 2,
        buf_line_space = 2 * 2 * src_ds.RasterXSize,
        buf_band_space = 2)
    assert got_data == expected_data

    # Optimized code path: band interleaved spacing
    assert ds.ReadRaster(buf_type = gdal.GDT_UInt16) == src_ds.ReadRaster(buf_type = gdal.GDT_UInt16)
    assert ds.ReadRaster(band_list = [2, 1]) == src_ds.ReadRaster(band_list = [2, 1])

    # Regular code path
    assert ds.ReadRaster(0, 0, 10, 10) == src_ds.ReadRaster(0, 0, 10, 10)

    ds = None
    gdal.GetDriverByName('JPEGXL').Delete(outfilename)


def test_jpegxl_icc_profile():

    f = open('data/sRGB.icc', 'rb')
    data = f.read()
    icc = base64.b64encode(data).decode('ascii')
    f.close()

    src_ds = gdal.GetDriverByName('MEM').Create('', 1, 1, 3)
    src_ds.GetRasterBand(1).SetColorInterpretation(gdal.GCI_RedBand)
    src_ds.GetRasterBand(2).SetColorInterpretation(gdal.GCI_GreenBand)
    src_ds.GetRasterBand(3).SetColorInterpretation(gdal.GCI_BlueBand)
    src_ds.SetMetadataItem('SOURCE_ICC_PROFILE', icc, 'COLOR_PROFILE')
    outfilename = '/vsimem/out.jxl'
    gdal.GetDriverByName('JPEGXL').CreateCopy(outfilename, src_ds)
    ds = gdal.Open(outfilename)
    assert ds.GetMetadataItem('SOURCE_ICC_PROFILE', 'COLOR_PROFILE') == icc

    ds = None
    gdal.GetDriverByName('JPEGXL').Delete(outfilename)


def test_jpegxl_lossless_copy_of_jpeg():

    has_box_api = 'COMPRESS_BOX' in gdal.GetDriverByName('JPEGXL').GetMetadataItem('DMD_CREATIONOPTIONLIST')

    src_ds = gdal.Open('data/jpeg/albania.jpg')
    outfilename = '/vsimem/out.jxl'
    gdal.GetDriverByName('JPEGXL').CreateCopy(outfilename, src_ds)
    if has_box_api:
        assert gdal.VSIStatL(outfilename + '.aux.xml') is None
    ds = gdal.Open(outfilename)
    assert ds is not None
    if has_box_api:
        assert set(ds.GetMetadataDomainList()) == set(['DERIVED_SUBDATASETS', 'EXIF', 'IMAGE_STRUCTURE'])
        assert ds.GetMetadataItem('HAS_JPEG_RECONSTRUCTION_DATA', '_DEBUG_') == 'YES'

    ds = None
    gdal.GetDriverByName('JPEGXL').Delete(outfilename)

    # Test failure in JxlEncoderAddJPEGFrame() by adding a truncated JPEG file
    data = open('data/jpeg/albania.jpg', 'rb').read()
    data = data[0:len(data)//2]
    with gdaltest.tempfile('/vsimem/truncated.jpg', data):
        src_ds = gdal.Open('/vsimem/truncated.jpg')
        with gdaltest.error_handler():
            assert gdal.GetDriverByName('JPEGXL').CreateCopy(outfilename, src_ds) is None


def test_jpegxl_read_extra_channels():

    src_ds = gdal.Open('data/rgbsmall.tif')
    ds = gdal.Open('data/jpegxl/threeband_non_rgb.jxl')

    assert [ds.GetRasterBand(i+1).Checksum() for i in range(src_ds.RasterCount)] == [src_ds.GetRasterBand(i+1).Checksum() for i in range(src_ds.RasterCount)]
    assert ds.ReadRaster() == src_ds.ReadRaster()


def test_jpegxl_write_extra_channels():

    outfilename = '/vsimem/out.jxl'
    src_ds = gdal.Open('../gcore/data/stefan_full_rgba.tif')
    mem_ds = gdal.GetDriverByName('MEM').Create('', src_ds.RasterXSize, src_ds.RasterYSize, src_ds.RasterCount)
    mem_ds.WriteRaster(0, 0, src_ds.RasterXSize, src_ds.RasterYSize,
                       src_ds.ReadRaster())
    mem_ds.GetRasterBand(3).SetDescription('third channel')
    outfilename = '/vsimem/out.jxl'

    drv = gdal.GetDriverByName('JPEGXL')
    if drv.GetMetadataItem('JXL_ENCODER_SUPPORT_EXTRA_CHANNELS') is not None:
        assert drv.CreateCopy(outfilename, mem_ds) is not None
        assert gdal.VSIStatL(outfilename + '.aux.xml') is None
        ds = gdal.Open(outfilename)
        assert [ds.GetRasterBand(i+1).Checksum() for i in range(src_ds.RasterCount)] == [mem_ds.GetRasterBand(i+1).Checksum() for i in range(src_ds.RasterCount)]
        assert ds.ReadRaster() == mem_ds.ReadRaster()
        assert ds.GetRasterBand(1).GetDescription() == ''
        assert ds.GetRasterBand(2).GetDescription() == '' # 'Band 2' encoded in .jxl file, but hidden when reading back
        assert ds.GetRasterBand(3).GetDescription() == 'third channel'
    else:
        with gdaltest.error_handler():
            assert drv.CreateCopy(outfilename, mem_ds) is None
            assert gdal.GetLastErrorMsg() == 'This version of libjxl does not support creating non-alpha extra channels.'

    ds = None
    gdal.GetDriverByName('JPEGXL').Delete(outfilename)


def test_jpegxl_read_five_bands():

    ds = gdal.Open('data/jpegxl/five_bands.jxl')
    assert [ds.GetRasterBand(i+1).Checksum() for i in range(5)] == [3741, 5281, 6003, 5095, 4318]
    mem_ds = gdal.GetDriverByName('MEM').CreateCopy('', ds)
    assert [mem_ds.GetRasterBand(i+1).Checksum() for i in range(5)] == [3741, 5281, 6003, 5095, 4318]
    assert ds.ReadRaster() == mem_ds.ReadRaster()
    assert ds.ReadRaster(band_list = [1]) == mem_ds.ReadRaster(band_list = [1])
    assert ds.ReadRaster(buf_pixel_space = ds.RasterCount,
                         buf_line_space = ds.RasterCount * ds.RasterXSize,
                         buf_band_space = 1) == \
           mem_ds.ReadRaster(buf_pixel_space = ds.RasterCount,
                             buf_line_space = ds.RasterCount * ds.RasterXSize,
                             buf_band_space = 1)


def test_jpegxl_write_five_bands():

    drv = gdal.GetDriverByName('JPEGXL')
    if drv.GetMetadataItem('JXL_ENCODER_SUPPORT_EXTRA_CHANNELS') is None:
        pytest.skip()

    src_ds = gdal.Open('data/jpegxl/five_bands.jxl')
    outfilename = '/vsimem/out.jxl'
    assert drv.CreateCopy(outfilename, src_ds) is not None
    ds = gdal.Open(outfilename)
    assert [ds.GetRasterBand(i+1).Checksum() for i in range(5)] == [3741, 5281, 6003, 5095, 4318]
    ds = None
    gdal.GetDriverByName('JPEGXL').Delete(outfilename)


def test_jpegxl_createcopy_errors():

    outfilename = '/vsimem/out.jxl'

    # band count = 0
    src_ds = gdal.GetDriverByName('MEM').Create('', 1, 1, 0)
    with gdaltest.error_handler():
        gdal.ErrorReset()
        assert gdal.GetDriverByName('JPEGXL').CreateCopy(outfilename, src_ds) is None
        assert gdal.GetLastErrorMsg() != ''

    # unsupported data type
    src_ds = gdal.GetDriverByName('MEM').Create('', 1, 1, 1, gdal.GDT_Int16)
    with gdaltest.error_handler():
        gdal.ErrorReset()
        assert gdal.GetDriverByName('JPEGXL').CreateCopy(outfilename, src_ds) is None
        assert gdal.GetLastErrorMsg() != ''

    # wrong out file name
    src_ds = gdal.GetDriverByName('MEM').Create('', 1, 1)
    with gdaltest.error_handler():
        gdal.ErrorReset()
        assert gdal.GetDriverByName('JPEGXL').CreateCopy('/i_do/not/exist.jxl', src_ds) is None
        assert gdal.GetLastErrorMsg() != ''

    # mutually exclusive options
    src_ds = gdal.GetDriverByName('MEM').Create('', 1, 1)
    with gdaltest.error_handler():
        gdal.ErrorReset()
        assert gdal.GetDriverByName('JPEGXL').CreateCopy(outfilename, src_ds,
                                                         options=['LOSSLESS=YES', 'DISTANCE=1']) is None
        assert gdal.GetLastErrorMsg() != ''

    # mutually exclusive options
    src_ds = gdal.GetDriverByName('MEM').Create('', 1, 1)
    with gdaltest.error_handler():
        gdal.ErrorReset()
        assert gdal.GetDriverByName('JPEGXL').CreateCopy(outfilename, src_ds,
                                                         options=['LOSSLESS=YES', 'QUALITY=90']) is None
        assert gdal.GetLastErrorMsg() != ''

    # mutually exclusive options
    src_ds = gdal.GetDriverByName('MEM').Create('', 1, 1)
    with gdaltest.error_handler():
        gdal.ErrorReset()
        assert gdal.GetDriverByName('JPEGXL').CreateCopy(outfilename, src_ds,
                                                         options=['DISTANCE=1', 'QUALITY=90']) is None
        assert gdal.GetLastErrorMsg() != ''

    # wrong value for DISTANCE
    src_ds = gdal.GetDriverByName('MEM').Create('', 1, 1)
    with gdaltest.error_handler():
        gdal.ErrorReset()
        assert gdal.GetDriverByName('JPEGXL').CreateCopy(outfilename, src_ds,
                                                         options=['DISTANCE=-1']) is None
        assert gdal.GetLastErrorMsg() != ''

    # wrong value for EFFORT
    src_ds = gdal.GetDriverByName('MEM').Create('', 1, 1)
    with gdaltest.error_handler():
        gdal.ErrorReset()
        assert gdal.GetDriverByName('JPEGXL').CreateCopy(outfilename, src_ds,
                                                         options=['EFFORT=-1']) is None
        assert gdal.GetLastErrorMsg() != ''
