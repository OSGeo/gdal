#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read/write functionality for JP2KAK JPEG2000 driver.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2009, Even Rouault <even dot rouault at mines-paris dot org>
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

from osgeo import gdal


import gdaltest
import pytest

###############################################################################
# Read test of simple byte reference data.


def test_jp2kak_1():

    gdaltest.jp2kak_drv = gdal.GetDriverByName('JP2KAK')
    if gdaltest.jp2kak_drv is None:
        pytest.skip()

    gdaltest.deregister_all_jpeg2000_drivers_but('JP2KAK')

    tst = gdaltest.GDALTest('JP2KAK', 'byte.jp2', 1, 50054)
    return tst.testOpen()

###############################################################################
# Read test of simple 16bit reference data.


def test_jp2kak_2():

    if gdaltest.jp2kak_drv is None:
        pytest.skip()

    tst = gdaltest.GDALTest('JP2KAK', 'int16.jp2', 1, 4587)
    return tst.testOpen()

###############################################################################
# Test lossless copying.


def test_jp2kak_3():

    if gdaltest.jp2kak_drv is None:
        pytest.skip()

    tst = gdaltest.GDALTest('JP2KAK', 'byte.jp2', 1, 50054,
                            options=['QUALITY=100'])

    return tst.testCreateCopy()

###############################################################################
# Test GeoJP2 production with geotransform.


def test_jp2kak_4():

    if gdaltest.jp2kak_drv is None:
        pytest.skip()

    tst = gdaltest.GDALTest('JP2KAK', 'rgbsmall.tif', 0, 0,
                            options=['GMLJP2=OFF'])

    return tst.testCreateCopy(check_srs=1, check_gt=1)

###############################################################################
# Test GeoJP2 production with gcps.


def test_jp2kak_5():

    if gdaltest.jp2kak_drv is None:
        pytest.skip()

    tst = gdaltest.GDALTest('JP2KAK', 'rgbsmall.tif', 0, 0,
                            options=['GEOJP2=OFF'])

    return tst.testCreateCopy(check_srs=1, check_gt=1)

###############################################################################
# Test VSI*L support with a JPC rather than jp2 datastream.
#


def test_jp2kak_8():

    if gdaltest.jp2kak_drv is None:
        pytest.skip()

    tst = gdaltest.GDALTest('JP2KAK', 'byte.jp2', 1, 50054,
                            options=['QUALITY=100'])

    return tst.testCreateCopy(vsimem=1,
                              new_filename='/vsimem/jp2kak_8.jpc')

###############################################################################
# Test checksum values for a YCbCr color model file.
#


def test_jp2kak_9():

    if gdaltest.jp2kak_drv is None:
        pytest.skip()

    tst = gdaltest.GDALTest('JP2KAK', 'rgbwcmyk01_YeGeo_kakadu.jp2', 2, 32141)
    return tst.testOpen()

###############################################################################
# Confirm that we can also read this file using the DirectRasterIO()
# function and get appropriate values.
#


def test_jp2kak_10():

    if gdaltest.jp2kak_drv is None:
        pytest.skip()

    ds = gdal.Open('data/rgbwcmyk01_YeGeo_kakadu.jp2')
    data = ds.ReadRaster(0, 0, 800, 100, band_list=[2, 3]).decode('latin1')
    ds = None

    expected = [(0, 0), (255, 0), (0, 255), (255, 255),
                (255, 255), (0, 255), (255, 0), (0, 0)]
    got = []

    for x in range(8):
        got.append((ord(data[x * 100]), ord(data[80000 + x * 100])))

    assert got == expected, 'did not get expected values.'

###############################################################################
# Test handle of 11bit signed file.
#


def test_jp2kak_11():

    if gdaltest.jp2kak_drv is None:
        pytest.skip()

    ds = gdal.Open('data/gtsmall_11_int16.jp2')
    cs = ds.GetRasterBand(1).Checksum()
    assert cs in (63475, 63472, 63452, 63471)

###############################################################################
# Test handle of 10bit unsigned file.
#


def test_jp2kak_12():

    if gdaltest.jp2kak_drv is None:
        pytest.skip()

    ds = gdal.Open('data/gtsmall_10_uint16.jp2')
    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 63360 or cs == 63357 or cs == 63358


###############################################################################
# Test internal overviews.
#

def test_jp2kak_13():

    if gdaltest.jp2kak_drv is None:
        pytest.skip()

    src_ds = gdal.Open('data/utm.pix')
    jp2_ds = gdaltest.jp2kak_drv.CreateCopy('tmp/jp2kak_13.jp2', src_ds)
    src_ds = None

    jp2_band = jp2_ds.GetRasterBand(1)
    assert jp2_band.GetOverviewCount() == 1, \
        'did not get expected number of overviews on jp2'

    ov_band = jp2_band.GetOverview(0)
    assert ov_band.XSize == 250 and ov_band.YSize == 4, \
        'did not get expected overview size.'

    # Note, due to oddities of rounding related to identifying discard
    # levels the overview is actually generated with no discard levels
    # and in the debug output we see 500x7 -> 500x7 -> 250x4.
    checksum = ov_band.Checksum()
    assert checksum in (11776, 11736, 11801), 'did not get expected overview checksum'

###############################################################################
# Test external overviews.
#


def test_jp2kak_14():

    if gdaltest.jp2kak_drv is None:
        pytest.skip()

    jp2_ds = gdal.Open('tmp/jp2kak_13.jp2')

    jp2_ds.BuildOverviews('NEAREST', overviewlist=[2, 4])

    jp2_band = jp2_ds.GetRasterBand(1)
    assert jp2_band.GetOverviewCount() == 2, \
        'did not get expected number of overviews on jp2'

    ov_band = jp2_band.GetOverview(0)
    assert ov_band.XSize == 250 and ov_band.YSize == 4, \
        'did not get expected overview size.'

    checksum = ov_band.Checksum()
    assert checksum in (12288, 12272, 12224), 'did not get expected overview checksum'

    ov_band = jp2_band.GetOverview(1)
    assert ov_band.XSize == 125 and ov_band.YSize == 2, \
        'did not get expected overview size. (2)'

    checksum = ov_band.Checksum()
    assert checksum in (2957, 2980, 2990), 'did not get expected overview checksum (2)'

    jp2_ds = None
    gdaltest.jp2kak_drv.Delete('tmp/jp2kak_13.jp2')

###############################################################################
# Confirm we can read resolution information.
#


def test_jp2kak_15():

    if gdaltest.jp2kak_drv is None:
        pytest.skip()

    jp2_ds = gdal.Open('data/small_200ppcm.jp2')

    md = jp2_ds.GetMetadata()

    assert (not (md['TIFFTAG_RESOLUTIONUNIT'] != '3 (pixels/cm)' or
            md['TIFFTAG_XRESOLUTION'] != '200.012')), \
        'did not get expected resolution metadata'

    jp2_ds = None

###############################################################################
# Confirm we can write and then reread resolution information.
#


def test_jp2kak_16():

    if gdaltest.jp2kak_drv is None:
        pytest.skip()

    jp2_ds = gdal.Open('data/small_200ppcm.jp2')
    out_ds = gdaltest.jp2kak_drv.CreateCopy('tmp/jp2kak_16.jp2', jp2_ds)
    del out_ds
    jp2_ds = None

    jp2_ds = gdal.Open('tmp/jp2kak_16.jp2')
    md = jp2_ds.GetMetadata()

    assert (not (md['TIFFTAG_RESOLUTIONUNIT'] != '3 (pixels/cm)' or
            md['TIFFTAG_XRESOLUTION'] != '200.012')), \
        'did not get expected resolution metadata'

    jp2_ds = None

    gdaltest.jp2kak_drv.Delete('tmp/jp2kak_16.jp2')

###############################################################################
# Test reading a file with axis orientation set properly for an alternate
# axis order coordinate system (urn:...:EPSG::4326).
# In addition, the source .jp2 file's embedded GML has the alternate order
# between the offsetVector tags, and the "GDAL_JP2K_ALT_OFFSETVECTOR_ORDER"
# option is turned on to match that situation.
# This test case was adapted from the "jp2kak_7()" case above.


def test_jp2kak_17():

    if gdaltest.jp2kak_drv is None:
        pytest.skip()

    gdal.SetConfigOption('GDAL_JP2K_ALT_OFFSETVECTOR_ORDER', 'YES')

    ds = gdal.Open('data/gmljp2_dtedsm_epsg_4326_axes_alt_offsetVector.jp2')

    gt = ds.GetGeoTransform()
    gte = (42.999583333333369, 0.008271349862259, 0,
           34.000416666666631, 0, -0.008271349862259)

    if (abs(gt[0] - gte[0]) > 0.0000001 or
        abs(gt[3] - gte[3]) > 0.000001 or
        abs(gt[1] - gte[1]) > 0.000000000005 or
        abs(gt[2] - gte[2]) > 0.000000000005 or
        abs(gt[4] - gte[4]) > 0.000000000005 or
            abs(gt[5] - gte[5]) > 0.000000000005):
        print('got: ', gt)
        gdal.SetConfigOption('GDAL_JP2K_ALT_OFFSETVECTOR_ORDER', 'NO')
        pytest.fail('did not get expected geotransform')

    ds = None

    gdal.SetConfigOption('GDAL_JP2K_ALT_OFFSETVECTOR_ORDER', 'NO')

###############################################################################
# Test lossless copying of Int16


def test_jp2kak_18():

    if gdaltest.jp2kak_drv is None:
        pytest.skip()

    tst = gdaltest.GDALTest('JP2KAK', 'int16.tif', 1, 4672,
                            options=['QUALITY=100'])

    return tst.testCreateCopy()

###############################################################################
# Test lossless copying of UInt16


def test_jp2kak_19():

    if gdaltest.jp2kak_drv is None:
        pytest.skip()

    tst = gdaltest.GDALTest('JP2KAK', '../gcore/data/uint16.tif', 1, 4672,
                            options=['QUALITY=100'], filename_absolute=1)

    return tst.testCreateCopy(vsimem=1)

###############################################################################
# Test auto-promotion of 1bit alpha band to 8bit


def test_jp2kak_20():

    if gdaltest.jp2kak_drv is None:
        pytest.skip()

    ds = gdal.Open('data/stefan_full_rgba_alpha_1bit.jp2')
    fourth_band = ds.GetRasterBand(4)
    assert fourth_band.GetMetadataItem('NBITS', 'IMAGE_STRUCTURE') is None
    got_cs = fourth_band.Checksum()
    assert got_cs == 8527
    jp2_bands_data = ds.ReadRaster(0, 0, ds.RasterXSize, ds.RasterYSize)
    # jp2_fourth_band_data = fourth_band.ReadRaster(
    #     0, 0, ds.RasterXSize, ds.RasterYSize)
    fourth_band.ReadRaster(0, 0, ds.RasterXSize, ds.RasterYSize,
                           int(ds.RasterXSize / 16), int(ds.RasterYSize / 16))

    tiff_drv = gdal.GetDriverByName('GTiff')
    tmp_ds = tiff_drv.CreateCopy('/vsimem/jp2kak_20.tif', ds)
    fourth_band = tmp_ds.GetRasterBand(4)
    got_cs = fourth_band.Checksum()
    gtiff_bands_data = tmp_ds.ReadRaster(0, 0, ds.RasterXSize, ds.RasterYSize)
    # gtiff_fourth_band_data = fourth_band.ReadRaster(0, 0, ds.RasterXSize,
    #                                                 ds.RasterYSize)
    # gtiff_fourth_band_subsampled_data = fourth_band.ReadRaster(
    #     0, 0, ds.RasterXSize, ds.RasterYSize,
    #     ds.RasterXSize/16, ds.RasterYSize/16)
    tmp_ds = None
    tiff_drv.Delete('/vsimem/jp2kak_20.tif')
    assert got_cs == 8527

    assert jp2_bands_data == gtiff_bands_data

    # if jp2_fourth_band_data != gtiff_fourth_band_data:
    #    gdaltest.post_reason('fail')
    #    return 'fail'

    ds = gdal.OpenEx('data/stefan_full_rgba_alpha_1bit.jp2',
                     open_options=['1BIT_ALPHA_PROMOTION=NO'])
    fourth_band = ds.GetRasterBand(4)
    assert fourth_band.GetMetadataItem('NBITS', 'IMAGE_STRUCTURE') == '1'

###############################################################################
# Test non nearest upsampling


def test_jp2kak_21():

    if gdaltest.jp2kak_drv is None:
        pytest.skip()

    tmp_ds = gdaltest.jp2kak_drv.CreateCopy(
        '/vsimem/jp2kak_21.jp2',
        gdal.Open('data/int16.tif'), options=['QUALITY=100'])
    tmp_ds = None
    tmp_ds = gdal.Open('/vsimem/jp2kak_21.jp2')
    full_res_data = tmp_ds.ReadRaster(0, 0, 20, 20)
    upsampled_data = tmp_ds.ReadRaster(0, 0, 20, 20, 40, 40,
                                       resample_alg=gdal.GRIORA_Cubic)
    tmp_ds = None
    gdal.Unlink('/vsimem/jp2kak_21.jp2')

    tmp_ds = gdal.GetDriverByName('MEM').Create('', 20, 20, 1, gdal.GDT_Int16)
    tmp_ds.GetRasterBand(1).WriteRaster(0, 0, 20, 20, full_res_data)
    ref_upsampled_data = tmp_ds.ReadRaster(0, 0, 20, 20, 40, 40,
                                           resample_alg=gdal.GRIORA_Cubic)

    mem_ds = gdal.GetDriverByName('MEM').Create('', 40, 40, 1, gdal.GDT_Int16)
    mem_ds.GetRasterBand(1).WriteRaster(0, 0, 40, 40, ref_upsampled_data)
    ref_cs = mem_ds.GetRasterBand(1).Checksum()
    mem_ds.GetRasterBand(1).WriteRaster(0, 0, 40, 40, upsampled_data)
    cs = mem_ds.GetRasterBand(1).Checksum()
    assert cs == ref_cs

###############################################################################
# Test RGBA datasets


def test_jp2kak_22():

    if gdaltest.jp2kak_drv is None:
        pytest.skip()

    src_ds = gdal.Open('../gcore/data/stefan_full_rgba.tif')
    gdaltest.jp2kak_drv.CreateCopy('/vsimem/jp2kak_22.jp2', src_ds, options=['QUALITY=100'])
    ds = gdal.Open('/vsimem/jp2kak_22.jp2')
    for i in range(4):
        ref_cs = src_ds.GetRasterBand(1).Checksum()
        cs = ds.GetRasterBand(1).Checksum()
        assert ref_cs == cs, i
        assert src_ds.GetRasterBand(1).GetColorInterpretation() == ds.GetRasterBand(1).GetColorInterpretation(), \
            i
    ds = None

    gdal.Unlink('/vsimem/jp2kak_22.jp2')


###############################################################################
# Cleanup.

def test_jp2kak_cleanup():

    gdaltest.reregister_all_jpeg2000_drivers()



