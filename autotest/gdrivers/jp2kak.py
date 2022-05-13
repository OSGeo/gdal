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
# Copyright (c) 2009, Even Rouault <even dot rouault at spatialys.com>
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

pytestmark = pytest.mark.require_driver('JP2KAK')

###############################################################################
@pytest.fixture(autouse=True, scope='module')
def startup_and_cleanup():

    gdaltest.jp2kak_drv = gdal.GetDriverByName('JP2KAK')
    assert gdaltest.jp2kak_drv is not None

    gdaltest.deregister_all_jpeg2000_drivers_but('JP2KAK')

    yield

    gdaltest.reregister_all_jpeg2000_drivers()

###############################################################################
# Read test of simple byte reference data.


def test_jp2kak_1():

    tst = gdaltest.GDALTest('JP2KAK', 'jpeg2000/byte.jp2', 1, 50054)
    return tst.testOpen()

###############################################################################
# Read test of simple 16bit reference data.


def test_jp2kak_2():

    tst = gdaltest.GDALTest('JP2KAK', 'jpeg2000/int16.jp2', 1, 4587)
    return tst.testOpen()

###############################################################################
# Test lossless copying.


def test_jp2kak_3():

    tst = gdaltest.GDALTest('JP2KAK', 'jpeg2000/byte.jp2', 1, 50054,
                            options=['QUALITY=100'])

    return tst.testCreateCopy()

###############################################################################
# Test GeoJP2 production with geotransform.


def test_jp2kak_4():

    tst = gdaltest.GDALTest('JP2KAK', 'rgbsmall.tif', 0, 0,
                            options=['GMLJP2=OFF'])

    return tst.testCreateCopy(check_srs=1, check_gt=1)

###############################################################################
# Test GeoJP2 production with gcps.


def test_jp2kak_5():

    tst = gdaltest.GDALTest('JP2KAK', 'rgbsmall.tif', 0, 0,
                            options=['GEOJP2=OFF'])

    return tst.testCreateCopy(check_srs=1, check_gt=1)

###############################################################################
# Test VSI*L support with a JPC rather than jp2 datastream.
#


def test_jp2kak_8():

    tst = gdaltest.GDALTest('JP2KAK', 'jpeg2000/byte.jp2', 1, 50054,
                            options=['QUALITY=100'])

    return tst.testCreateCopy(vsimem=1,
                              new_filename='/vsimem/jp2kak_8.jpc')

###############################################################################
# Test checksum values for a YCbCr color model file.
#


def test_jp2kak_9():

    tst = gdaltest.GDALTest('JP2KAK', 'jpeg2000/rgbwcmyk01_YeGeo_kakadu.jp2', 2, 32141)
    return tst.testOpen()

###############################################################################
# Confirm that we can also read this file using the DirectRasterIO()
# function and get appropriate values.
#


def test_jp2kak_10():

    ds = gdal.Open('data/jpeg2000/rgbwcmyk01_YeGeo_kakadu.jp2')
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

    ds = gdal.Open('data/jpeg2000/gtsmall_11_int16.jp2')
    cs = ds.GetRasterBand(1).Checksum()
    assert cs in (63475, 63472, 63452, 63471)

###############################################################################
# Test handle of 10bit unsigned file.
#


def test_jp2kak_12():

    ds = gdal.Open('data/jpeg2000/gtsmall_10_uint16.jp2')
    cs = ds.GetRasterBand(1).Checksum()
    assert cs in (63360, 63653, 63357, 63358)


###############################################################################
# Test internal overviews.
#

def test_jp2kak_13():

    src_ds = gdal.Open('data/pcidsk/utm.pix')
    jp2_ds = gdaltest.jp2kak_drv.CreateCopy('tmp/jp2kak_13.jp2', src_ds)
    src_ds = None

    jp2_band = jp2_ds.GetRasterBand(1)
    assert jp2_band.GetOverviewCount() == 1, \
        'did not get expected number of overviews on jp2'

    ov_band = jp2_band.GetOverview(0)
    assert ov_band.XSize == 250 and ov_band.YSize == 4, \
        'did not get expected overview size.'
    #
    # Note, due to oddities of rounding related to identifying discard
    # levels the overview is actually generated with no discard levels
    # and in the debug output we see 500x7 -> 500x7 -> 250x4.
    checksum = ov_band.Checksum()
    assert checksum in (11767, 11776, 11736, 11801), 'did not get expected overview checksum'

###############################################################################
# Test external overviews.
#


def test_jp2kak_14():

    jp2_ds = gdal.Open('tmp/jp2kak_13.jp2')

    jp2_ds.BuildOverviews('NEAREST', overviewlist=[2, 4])

    jp2_band = jp2_ds.GetRasterBand(1)
    assert jp2_band.GetOverviewCount() == 2, \
        'did not get expected number of overviews on jp2'

    ov_band = jp2_band.GetOverview(0)
    assert ov_band.XSize == 250 and ov_band.YSize == 4, \
        'did not get expected overview size.'

    checksum = ov_band.Checksum()
    assert checksum in (12224, 12279, 12272, 12288), 'did not get expected overview checksum'

    ov_band = jp2_band.GetOverview(1)
    assert ov_band.XSize == 125 and ov_band.YSize == 2, \
        'did not get expected overview size. (2)'

    checksum = ov_band.Checksum()
    assert checksum in (2918, 2957, 2980, 2990), 'did not get expected overview checksum (2)'

    jp2_ds = None
    gdaltest.jp2kak_drv.Delete('tmp/jp2kak_13.jp2')

#
###############################################################################
# Confirm we can read resolution information.
#


def test_jp2kak_15():

    jp2_ds = gdal.Open('data/jpeg2000/small_200ppcm.jp2')

    md = jp2_ds.GetMetadata()

    assert (not (md['TIFFTAG_RESOLUTIONUNIT'] != '3 (pixels/cm)' or
            md['TIFFTAG_XRESOLUTION'] != '200.012')), \
        'did not get expected resolution metadata'

    jp2_ds = None

###############################################################################
# Confirm we can write and then reread resolution information.
#


def test_jp2kak_16():

    jp2_ds = gdal.Open('data/jpeg2000/small_200ppcm.jp2')
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

    gdal.SetConfigOption('GDAL_JP2K_ALT_OFFSETVECTOR_ORDER', 'YES')

    ds = gdal.Open('data/jpeg2000/gmljp2_dtedsm_epsg_4326_axes_alt_offsetVector.jp2')

    gt = ds.GetGeoTransform()
    gte = (42.999583333333369, 0.008271349862259, 0,
           34.000416666666631, 0, -0.008271349862259)

    if (gt[0] != pytest.approx(gte[0], abs=0.0000001) or
        gt[3] != pytest.approx(gte[3], abs=0.000001) or
        gt[1] != pytest.approx(gte[1], abs=0.000000000005) or
        gt[2] != pytest.approx(gte[2], abs=0.000000000005) or
        gt[4] != pytest.approx(gte[4], abs=0.000000000005) or
            gt[5] != pytest.approx(gte[5], abs=0.000000000005)):
        print('got: ', gt)
        gdal.SetConfigOption('GDAL_JP2K_ALT_OFFSETVECTOR_ORDER', 'NO')
        pytest.fail('did not get expected geotransform')

    ds = None

    gdal.SetConfigOption('GDAL_JP2K_ALT_OFFSETVECTOR_ORDER', 'NO')

###############################################################################
# Test lossless copying of Int16


def test_jp2kak_lossless_int16():

    tst = gdaltest.GDALTest('JP2KAK', 'int16.tif', 1, 4672,
                            options=['QUALITY=100'])

    return tst.testCreateCopy()

###############################################################################
# Test lossless copying of UInt16


def test_jp2kak_lossless_uint16():

    tst = gdaltest.GDALTest('JP2KAK', '../gcore/data/uint16.tif', 1, 4672,
                            options=['QUALITY=100'], filename_absolute=1)

    return tst.testCreateCopy(vsimem=1)

###############################################################################
# Test lossless copying of Int32


def test_jp2kak_lossless_int32():

    tst = gdaltest.GDALTest('JP2KAK', '../gcore/data/int32.tif', 1, 4672,
                            options=['QUALITY=100'], filename_absolute=1)

    return tst.testCreateCopy()

###############################################################################
# Test lossless copying of UInt32


def test_jp2kak_lossless_uint32():

    tst = gdaltest.GDALTest('JP2KAK', '../gcore/data/uint32.tif', 1, 4672,
                            options=['QUALITY=100'], filename_absolute=1)

    return tst.testCreateCopy(vsimem=1)

###############################################################################
# Test lossless copying of Int32


def test_jp2kak_lossless_int32_nbits_20():

    src_ds = gdal.Translate('', '../gcore/data/int32.tif', options='-of MEM -scale 74 255 -524288 524287')
    tmpfilename = '/vsimem/test_jp2kak_lossless_int32_nbits_20.j2k'
    gdal.GetDriverByName('JP2KAK').CreateCopy(tmpfilename, src_ds, options = ['QUALITY=100', 'NBITS=20'])
    ds = gdal.Open(tmpfilename)
    assert ds.GetRasterBand(1).ReadRaster() == src_ds.GetRasterBand(1).ReadRaster()
    gdal.GetDriverByName('JP2KAK').Delete(tmpfilename)

###############################################################################
# Test lossless copying of UInt32


def test_jp2kak_lossless_uint32_nbits_20():

    src_ds = gdal.Translate('', '../gcore/data/uint32.tif', options='-of MEM -scale 74 255 0 1048575')
    tmpfilename = '/vsimem/test_jp2kak_lossless_uint32_nbits_20.j2k'
    gdal.GetDriverByName('JP2KAK').CreateCopy(tmpfilename, src_ds, options = ['QUALITY=100', 'NBITS=20'])
    ds = gdal.Open(tmpfilename)
    assert ds.GetRasterBand(1).ReadRaster() == src_ds.GetRasterBand(1).ReadRaster()
    gdal.GetDriverByName('JP2KAK').Delete(tmpfilename)

###############################################################################
# Test lossy copying of Int32


def test_jp2kak_lossy_int32_nbits_20():

    src_ds = gdal.Translate('', '../gcore/data/int32.tif', options='-of MEM -scale 74 255 -524288 524287')
    tmpfilename = '/vsimem/test_jp2kak_lossy_int32_nbits_20.j2k'
    gdal.GetDriverByName('JP2KAK').CreateCopy(tmpfilename, src_ds, options = ['QUALITY=80', 'NBITS=20'])
    ds = gdal.Open(tmpfilename)
    assert src_ds.GetRasterBand(1).ComputeStatistics(False) == pytest.approx(ds.GetRasterBand(1).ComputeStatistics(False), rel=1e-2)
    gdal.GetDriverByName('JP2KAK').Delete(tmpfilename)

###############################################################################
# Test lossy copying of UInt32


def test_jp2kak_lossy_uint32_nbits_20():

    src_ds = gdal.Translate('', '../gcore/data/uint32.tif', options='-of MEM -scale 74 255 0 1048575')
    tmpfilename = '/vsimem/test_jp2kak_lossy_uint32_nbits_20.j2k'
    gdal.GetDriverByName('JP2KAK').CreateCopy(tmpfilename, src_ds, options = ['QUALITY=80', 'NBITS=20'])
    ds = gdal.Open(tmpfilename)
    assert src_ds.GetRasterBand(1).ComputeStatistics(False) == pytest.approx(ds.GetRasterBand(1).ComputeStatistics(False), rel=1e-2)
    gdal.GetDriverByName('JP2KAK').Delete(tmpfilename)

###############################################################################
# Test auto-promotion of 1bit alpha band to 8bit


def test_jp2kak_20():

    ds = gdal.Open('data/jpeg2000/stefan_full_rgba_alpha_1bit.jp2')
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

    ds = gdal.OpenEx('data/jpeg2000/stefan_full_rgba_alpha_1bit.jp2',
                     open_options=['1BIT_ALPHA_PROMOTION=NO'])
    fourth_band = ds.GetRasterBand(4)
    assert fourth_band.GetMetadataItem('NBITS', 'IMAGE_STRUCTURE') == '1'

###############################################################################
# Test non nearest upsampling


def test_jp2kak_21():

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
# Test accessing overview levels when the dimensions of the full resolution
# image are not a multiple of 2^numresolutions


def test_jp2kak_odd_dimensions():

    ds = gdal.Open('data/jpeg2000/513x513.jp2')
    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    ds = None

    assert cs == 29642

###############################################################################
# Test reading an image whose origin is not (0,0)


def test_jp2kak_image_origin_not_zero():

    ds = gdal.Open('data/jpeg2000/byte_image_origin_not_zero.jp2')
    assert ds.GetRasterBand(1).Checksum() == 4672
    assert ds.GetRasterBand(1).ReadRaster(0,0,20,20,10,10) is not None

###############################################################################
# Test multiple RATE parameters

def test_jp2openjpeg_test_multi_rate():
    src_ds = gdal.Open('../gcore/data/stefan_full_rgba.tif')
    gdaltest.jp2kak_drv.CreateCopy('/vsimem/jp2kak_22.jp2', src_ds, options=['RATE=1.5,2,3,4', 'LAYERS=4'])
    node = gdal.GetJPEG2000Structure('/vsimem/jp2kak_22.jp2', ['ALL=YES'])

    arr = []
    find_elements_with_name(node, "Field", "COM", arr)

    # Approximation of [4556, 6075, 9112, 12150] = floor(dfPixelsTotal * currentBitRate * 0.125F)
    # dfPixelsTotal = 24300
    # 4.6e+03 -> 4556 (24300 * 1.5 * 0.125F)
    # 6.1e+03 -> 6075 (24300 * 2 * 0.125F)
    # 9.1e+03 -> 9112 (24300 * 3 * 0.125F)
    # 1.2e+04 -> 12150 (24300 * 4 * 0.125F)

    split = arr[4][5][1].split(',')
    # 4 layers + 2 blocs for extra information
    assert len(split) == 6

    assert split[2].split('\n')[0].strip() == '4.6e+03'
    assert split[3].split('\n')[0].strip() == '6.1e+03'
    assert split[4].split('\n')[0].strip() == '9.1e+03'
    assert split[5].split('\n')[0].strip() == '1.2e+04'

    gdal.Unlink('/vsimem/jp2kak_22.jp2')

###############################################################################
# Test multiple RATE parameters  using dash as first value

def test_jp2openjpeg_test_multi_rate_dash():
    src_ds = gdal.Open('../gcore/data/stefan_full_rgba.tif')
    gdaltest.jp2kak_drv.CreateCopy('/vsimem/jp2kak_22.jp2', src_ds, options=['RATE=-,4', 'LAYERS=4'])
    node = gdal.GetJPEG2000Structure('/vsimem/jp2kak_22.jp2', ['ALL=YES'])

    arr = []
    find_elements_with_name(node, "Field", "COM", arr)

    # Approximation of [12150, 0, 0, 0] = floor(dfPixelsTotal * currentBitRate * 0.125F)
    # dfPixelsTotal = 24300
    # dfPixelsTotal = 24300
    # 1.2e+04 -> 12150 (24300 * 4 * 0.125F)
    # 0
    # 0
    # 0

    split = arr[4][5][1].split(',')

    # 4 layers + 2 blocs for extra information
    assert len(split) == 6

    assert split[2].split('\n')[0].strip() == '1.2e+04'

    gdal.Unlink('/vsimem/jp2kak_22.jp2')

###############################################################################
# Test single RATE parameter

def test_jp2openjpeg_test_single_rate():
    src_ds = gdal.Open('../gcore/data/stefan_full_rgba.tif')
    gdaltest.jp2kak_drv.CreateCopy('/vsimem/jp2kak_22.jp2', src_ds, options=['RATE=1.5', 'LAYERS=4'])
    node = gdal.GetJPEG2000Structure('/vsimem/jp2kak_22.jp2', ['ALL=YES'])

    arr = []
    find_elements_with_name(node, "Field", "COM", arr)

    # Approximation of [0,0,0,4556] = floor(dfPixelsTotal * currentBitRate * 0.125F)
    # dfPixelsTotal = 24300
    # First layers are 0; only the last one has a defined value
    # 4.6e+03 -> 4556 (24300 * 1.5 * 0.125F)

    split = arr[4][5][1].split(',')

    # 4 layers + 2 blocs for extra information
    assert len(split) == 6

    assert split[5].split('\n')[0].strip() == '4.6e+03'

    gdal.Unlink('/vsimem/jp2kak_22.jp2')

###############################################################################
# Test single RATE parameter with defined quality < 99.5

def test_jp2openjpeg_test_multi_rate_quality_50():
    src_ds = gdal.Open('../gcore/data/stefan_full_rgba.tif')
    gdaltest.jp2kak_drv.CreateCopy('/vsimem/jp2kak_22.jp2', src_ds, options=['RATE=1.5', 'LAYERS=4', 'QUALITY=50'])
    node = gdal.GetJPEG2000Structure('/vsimem/jp2kak_22.jp2', ['ALL=YES'])

    arr = []
    find_elements_with_name(node, "Field", "COM", arr)

    # Approximation of [0,0,0,4556] = floor(dfPixelsTotal * currentBitRate * 0.125F)
    # dfPixelsTotal = 24300
    # First layers are 0; only the last one has a defined value
    # 4.6e+03 -> 4556 (24300 * 1.5 * 0.125F)

    split = arr[4][5][1].split(',')

    # 4 layers + 2 blocs for extra information
    assert len(split) == 6
    assert split[5].split('\n')[0].strip() == '4.6e+03'

    gdal.Unlink('/vsimem/jp2kak_22.jp2')

###############################################################################
# Test single RATE parameter with defined quality > 99.5

def test_jp2openjpeg_test_multi_rate_quality_100():
    src_ds = gdal.Open('../gcore/data/stefan_full_rgba.tif')
    gdaltest.jp2kak_drv.CreateCopy('/vsimem/jp2kak_22.jp2', src_ds, options=['RATE=1.5', 'LAYERS=4', 'QUALITY=100'])
    node = gdal.GetJPEG2000Structure('/vsimem/jp2kak_22.jp2', ['ALL=YES'])

    arr = []
    find_elements_with_name(node, "Field", "COM", arr)

    # Approximation of [0,0,0,4556] = floor(dfPixelsTotal * currentBitRate * 0.125F)
    # dfPixelsTotal = 24300
    # First layers are 0; only the last one has a defined value
    # 4.6e+03 -> 4556 (24300 * 1.5 * 0.125F)
    split = arr[4][5][1].split(',')

    # 4 layers + 2 blocs for extra information
    assert len(split) == 6

    assert split[5].split('\n')[0].strip() == '4.6e+03'

    gdal.Unlink('/vsimem/jp2kak_22.jp2')

###############################################################################
# Test single RATE parameter with defined quality > 99.5 and the Creversible option

def test_jp2openjpeg_test_multi_rate_quality_100_reversible():
    src_ds = gdal.Open('../gcore/data/stefan_full_rgba.tif')
    gdaltest.jp2kak_drv.CreateCopy('/vsimem/jp2kak_22.jp2', src_ds, options=['RATE=1.5', 'LAYERS=4', 'QUALITY=100',
                                                                             'Creversible=yes'])
    node = gdal.GetJPEG2000Structure('/vsimem/jp2kak_22.jp2', ['ALL=YES'])

    arr = []
    find_elements_with_name(node, "Field", "COM", arr)

    # Approximation of [0,0,0,4556] = floor(dfPixelsTotal * currentBitRate * 0.125F)
    # dfPixelsTotal = 24300
    # First layers are 0; only the last one has a defined value
    # 4.6e+03 -> 4556 (24300 * 1.5 * 0.125F)
    split = arr[4][5][1].split(',')

    # 4 layers + 2 blocs for extra information
    assert len(split) == 6

    assert split[5].split('\n')[0].strip() == '4.6e+03'

    gdal.Unlink('/vsimem/jp2kak_22.jp2')

###############################################################################
# Test single RATE parameter with defined quality > 99.5 and the Creversible option

def test_jp2openjpeg_test_multi_rate_quality_100_no_reversible():
    src_ds = gdal.Open('../gcore/data/stefan_full_rgba.tif')
    gdaltest.jp2kak_drv.CreateCopy('/vsimem/jp2kak_22.jp2', src_ds, options=['RATE=1.5', 'LAYERS=4', 'QUALITY=100',
                                                                             'Creversible=no'])
    node = gdal.GetJPEG2000Structure('/vsimem/jp2kak_22.jp2', ['ALL=YES'])

    arr = []
    find_elements_with_name(node, "Field", "COM", arr)

    # Approximation of [0,0,0,4556] = floor(dfPixelsTotal * currentBitRate * 0.125F)
    # dfPixelsTotal = 24300
    # First layers are 0; only the last one has a defined value
    # 4.6e+03 -> 4556 (24300 * 1.5 * 0.125F)
    split = arr[4][5][1].split(',')

    # 4 layers + 2 blocs for extra information
    assert len(split) == 6

    assert split[5].split('\n')[0].strip() == '4.6e+03'

    gdal.Unlink('/vsimem/jp2kak_22.jp2')

###############################################################################
# Test quality > 99.5 and the Creversible option

def test_jp2openjpeg_test_quality_100_no_reversible():
    src_ds = gdal.Open('../gcore/data/stefan_full_rgba.tif')
    gdaltest.jp2kak_drv.CreateCopy('/vsimem/jp2kak_22.jp2', src_ds, options=['QUALITY=100', 'Creversible=no'])
    node = gdal.GetJPEG2000Structure('/vsimem/jp2kak_22.jp2', ['ALL=YES'])

    arr = []
    find_elements_with_name(node, "Field", "COM", arr)

    split = arr[4][5][1].split(',')

    # 12 layers + 2 blocs for extra information
    assert len(split) == 14

    gdal.Unlink('/vsimem/jp2kak_22.jp2')

###############################################################################
# Test quality > 99.5 and the Creversible option

def test_jp2openjpeg_test_quality_100_reversible():
    src_ds = gdal.Open('../gcore/data/stefan_full_rgba.tif')
    gdaltest.jp2kak_drv.CreateCopy('/vsimem/jp2kak_22.jp2', src_ds, options=['QUALITY=100', 'Creversible=yes'])
    node = gdal.GetJPEG2000Structure('/vsimem/jp2kak_22.jp2', ['ALL=YES'])

    arr = []
    find_elements_with_name(node, "Field", "COM", arr)

    split = arr[4][5][1].split(',')

    # 12 layers + 2 blocs for extra information
    assert len(split) == 14
    gdal.Unlink('/vsimem/jp2kak_22.jp2')
    return False

XML_TYPE_IDX = 0
XML_VALUE_IDX = 1
XML_FIRST_CHILD_IDX = 2

def get_element_val(node):
    if node is None:
        return None
    for child_idx in range(XML_FIRST_CHILD_IDX, len(node)):
        child = node[child_idx]
        if child[XML_TYPE_IDX] == gdal.CXT_Text:
            return child[XML_VALUE_IDX]
    return None

def find_element_with_name(ar, element_name, name):
    typ = ar[XML_TYPE_IDX]
    value = ar[XML_VALUE_IDX]
    if typ == gdal.CXT_Element and value == element_name and get_attribute_val(ar, 'name') == name:
        return ar
    for child_idx in range(XML_FIRST_CHILD_IDX, len(ar)):
        child = ar[child_idx]
        found = find_element_with_name(child, element_name, name)
        if found:
            return found
    return None

def find_elements_with_name(ar, element_name, name, arr):
    typ = ar[XML_TYPE_IDX]
    value = ar[XML_VALUE_IDX]
    if typ == gdal.CXT_Element and value == element_name and get_attribute_val(ar, 'name') == name:
        return ar
    for child_idx in range(XML_FIRST_CHILD_IDX, len(ar)):
        child = ar[child_idx]
        found = find_element_with_name(child, element_name, name)
        if found:
            arr.append(found)

        find_elements_with_name(child, element_name, name, arr)
    return None

def get_attribute_val(ar, attr_name):
    node = find_xml_node(ar, attr_name, True)
    if node is None or node[XML_TYPE_IDX] != gdal.CXT_Attribute:
        return None
    if len(ar) > XML_FIRST_CHILD_IDX and \
            node[XML_FIRST_CHILD_IDX][XML_TYPE_IDX] == gdal.CXT_Text:
        return node[XML_FIRST_CHILD_IDX][XML_VALUE_IDX]
    return None

def find_xml_node(ar, element_name, only_attributes=False):
    # type = ar[XML_TYPE_IDX]
    value = ar[XML_VALUE_IDX]
    if value == element_name:
        return ar
    for child_idx in range(XML_FIRST_CHILD_IDX, len(ar)):
        child = ar[child_idx]
        if only_attributes and child[XML_TYPE_IDX] != gdal.CXT_Attribute:
            continue
        found = find_xml_node(child, element_name)
        if found is not None:
            return found
    return None