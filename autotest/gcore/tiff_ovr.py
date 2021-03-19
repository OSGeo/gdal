#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test Overview Support (mostly a GeoTIFF issue).
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2008-2013, Even Rouault <even dot rouault at spatialys.com>
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
import shutil
import array
import stat
from osgeo import osr
from osgeo import gdal

import pytest

import gdaltest


@pytest.fixture(params=['invert', 'dont-invert'])
def both_endian(request):
    """
    Runs tests with both values of GDAL_TIFF_ENDIANNESS
    """
    if request.param == 'invert':
        with gdaltest.config_option('GDAL_TIFF_ENDIANNESS', 'INVERTED'):
            yield
    else:
        yield


###############################################################################
# Check the overviews

def tiff_ovr_check(src_ds):
    for i in (1, 2, 3):
        assert src_ds.GetRasterBand(i).GetOverviewCount() == 2, 'overviews missing'

        ovr_band = src_ds.GetRasterBand(i).GetOverview(0)
        if ovr_band.XSize != 10 or ovr_band.YSize != 10:
            msg = 'overview wrong size: band %d, overview 0, size = %d * %d,' % (i, ovr_band.XSize, ovr_band.YSize)
            pytest.fail(msg)

        if ovr_band.Checksum() != 1087:
            msg = 'overview wrong checksum: band %d, overview 0, checksum = %d,' % (i, ovr_band.Checksum())
            pytest.fail(msg)

        ovr_band = src_ds.GetRasterBand(i).GetOverview(1)
        if ovr_band.XSize != 5 or ovr_band.YSize != 5:
            msg = 'overview wrong size: band %d, overview 1, size = %d * %d,' % (i, ovr_band.XSize, ovr_band.YSize)
            pytest.fail(msg)

        if ovr_band.Checksum() != 328:
            msg = 'overview wrong checksum: band %d, overview 1, checksum = %d,' % (i, ovr_band.Checksum())
            pytest.fail(msg)

###############################################################################
# Create a 3 band floating point GeoTIFF file so we can build overviews on it
# later.  Build overviews on it.


def test_tiff_ovr_1(both_endian):

    gdaltest.tiff_drv = gdal.GetDriverByName('GTiff')

    src_ds = gdal.Open('data/mfloat32.vrt')

    assert src_ds is not None, 'Failed to open test dataset.'

    gdaltest.tiff_drv.CreateCopy('tmp/mfloat32.tif', src_ds,
                                 options=['INTERLEAVE=PIXEL'])
    src_ds = None

    ds = gdal.Open('tmp/mfloat32.tif')

    assert ds is not None, 'Failed to open test dataset.'

    err = ds.BuildOverviews(overviewlist=[2, 4])

    assert err == 0, 'BuildOverviews reports an error'

    ret = tiff_ovr_check(ds)

    ds = None

    return ret


###############################################################################
# Open file and verify some characteristics of the overviews.

def test_tiff_ovr_2(both_endian):

    src_ds = gdal.Open('tmp/mfloat32.tif')

    assert src_ds is not None, 'Failed to open test dataset.'

    ret = tiff_ovr_check(src_ds)

    src_ds = None

    return ret

###############################################################################
# Open target file in update mode, and create internal overviews.


def test_tiff_ovr_3(both_endian):

    try:
        os.unlink('tmp/mfloat32.tif.ovr')
    except OSError:
        pass

    src_ds = gdal.Open('tmp/mfloat32.tif', gdal.GA_Update)

    assert src_ds is not None, 'Failed to open test dataset.'

    err = src_ds.BuildOverviews(overviewlist=[2, 4])
    assert err == 0, 'BuildOverviews reports an error'

    ret = tiff_ovr_check(src_ds)

    src_ds = None

    return ret

###############################################################################
# Re-open target file and check overviews


def test_tiff_ovr_3bis(both_endian):
    return test_tiff_ovr_2(both_endian)

###############################################################################
# Test generation


def test_tiff_ovr_4(both_endian):

    shutil.copyfile('data/oddsize_1bit2b.tif', 'tmp/ovr4.tif')

    wrk_ds = gdal.Open('tmp/ovr4.tif', gdal.GA_Update)

    assert wrk_ds is not None, 'Failed to open test dataset.'

    wrk_ds.BuildOverviews('AVERAGE_BIT2GRAYSCALE', overviewlist=[2, 4])
    wrk_ds = None

    wrk_ds = gdal.Open('tmp/ovr4.tif')

    ovband = wrk_ds.GetRasterBand(1).GetOverview(1)
    md = ovband.GetMetadata()
    assert 'RESAMPLING' in md and md['RESAMPLING'] == 'AVERAGE_BIT2GRAYSCALE', \
        'Did not get expected RESAMPLING metadata.'

    # compute average value of overview band image data.
    ovimage = ovband.ReadRaster(0, 0, ovband.XSize, ovband.YSize)

    pix_count = ovband.XSize * ovband.YSize
    total = float(sum(ovimage))

    average = total / pix_count
    exp_average = 154.0992
    assert average == pytest.approx(exp_average, abs=0.1), 'got wrong average for overview image'

    # Read base band as overview resolution and verify we aren't getting
    # the grayscale image.

    frband = wrk_ds.GetRasterBand(1)
    ovimage = frband.ReadRaster(0, 0, frband.XSize, frband.YSize,
                                ovband.XSize, ovband.YSize)

    pix_count = ovband.XSize * ovband.YSize
    total = float(sum(ovimage))
    average = total / pix_count
    exp_average = 0.6096

    assert average == pytest.approx(exp_average, abs=0.01), 'got wrong average for downsampled image'

    wrk_ds = None


###############################################################################
# Test average overview generation with nodata.

def test_tiff_ovr_5(both_endian):

    shutil.copyfile('data/nodata_byte.tif', 'tmp/ovr5.tif')

    wrk_ds = gdal.Open('tmp/ovr5.tif', gdal.GA_ReadOnly)

    assert wrk_ds is not None, 'Failed to open test dataset.'

    wrk_ds.BuildOverviews('AVERAGE', overviewlist=[2])

    cs = wrk_ds.GetRasterBand(1).GetOverview(0).Checksum()
    exp_cs = 1130

    assert cs == exp_cs, 'got wrong overview checksum.'

###############################################################################
# Same as tiff_ovr_5 but with USE_RDD=YES to force external overview


def test_tiff_ovr_6(both_endian):

    shutil.copyfile('data/nodata_byte.tif', 'tmp/ovr6.tif')

    with gdaltest.config_option('USE_RRD', 'YES'):
        wrk_ds = gdal.Open('tmp/ovr6.tif', gdal.GA_Update)

        assert wrk_ds is not None, 'Failed to open test dataset.'

        def cbk(pct, _, user_data):
            if user_data[0] < 0:
                assert pct == 0
            assert pct >= user_data[0]
            user_data[0] = pct
            return 1

        tab = [-1]
        wrk_ds.BuildOverviews('AVERAGE', overviewlist=[2], callback=cbk, callback_data=tab)
        assert tab[0] == 1.0

    try:
        os.stat('tmp/ovr6.aux')
    except OSError:
        pytest.fail('no external overview.')

    cs = wrk_ds.GetRasterBand(1).GetOverview(0).Checksum()
    exp_cs = 1130

    assert cs == exp_cs, 'got wrong overview checksum.'


###############################################################################
# Check nearest resampling on a dataset with a raster band that has a color table

def test_tiff_ovr_7(both_endian):

    shutil.copyfile('data/test_average_palette.tif', 'tmp/test_average_palette.tif')

    # This dataset is a black&white chessboard, index 0 is black, index 1 is white.
    # In nearest resampling, we are expecting a uniform black image.
    ds = gdal.Open('tmp/test_average_palette.tif', gdal.GA_Update)

    assert ds is not None, 'Failed to open test dataset.'

    ds.BuildOverviews('NEAREST', overviewlist=[2])

    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    exp_cs = 0

    ds = None

    assert cs == exp_cs, 'got wrong overview checksum.'

###############################################################################
# Check average resampling on a dataset with a raster band that has a color table


def test_tiff_ovr_8(both_endian):

    shutil.copyfile('data/test_average_palette.tif', 'tmp/test_average_palette.tif')

    # This dataset is a black&white chessboard, index 0 is black, index 1 is white.
    # So the result of averaging (0,0,0) and (255,255,255) is (127,127,127), which is
    # index 2. So the result of the averaging is a uniform grey image.
    ds = gdal.Open('tmp/test_average_palette.tif', gdal.GA_Update)

    assert ds is not None, 'Failed to open test dataset.'

    ds.BuildOverviews('AVERAGE', overviewlist=[2])

    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    exp_cs = 200

    ds = None

    assert cs == exp_cs, 'got wrong overview checksum.'

###############################################################################
# Check RMS resampling on a dataset with a raster band that has a color table


def test_tiff_ovr_rms_palette(both_endian):

    shutil.copyfile('data/test_average_palette.tif', 'tmp/test_average_palette.tif')

    # This dataset is a black&white chessboard, index 0 is black, index 1 is white.
    # So the result of averaging (0,0,0) and (255,255,255) is (180.3,180.3,180.3),
    # and the closest color is (127,127,127) at index 2.
    # So the result of the averaging is a uniform grey image.
    ds = gdal.Open('tmp/test_average_palette.tif', gdal.GA_Update)
    ds.BuildOverviews('RMS', overviewlist=[2])

    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    exp_cs = 200

    ds = None

    assert cs == exp_cs, 'got wrong overview checksum.'

###############################################################################
# Test --config COMPRESS_OVERVIEW JPEG --config PHOTOMETRIC_OVERVIEW YCBCR -ro
# Will also check that pixel interleaving is automatically selected (#3064)


def test_tiff_ovr_9(both_endian):
    gdaltest.tiff_drv.Delete('tmp/ovr9.tif')

    shutil.copyfile('data/rgbsmall.tif', 'tmp/ovr9.tif')

    with gdaltest.config_options({'COMPRESS_OVERVIEW': 'JPEG',
                                  'PHOTOMETRIC_OVERVIEW': 'YCBCR'}):
        ds = gdal.Open('tmp/ovr9.tif', gdal.GA_ReadOnly)

        assert ds is not None, 'Failed to open test dataset.'

        ds.BuildOverviews('AVERAGE', overviewlist=[2])

    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    exp_cs = 5562

    ds = None

    assert cs == exp_cs or cs == 5635, 'got wrong overview checksum.'

    # Re-check after dataset reopening
    ds = gdal.Open('tmp/ovr9.tif', gdal.GA_ReadOnly)

    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    exp_cs = 5562

    ds = None

    assert cs == exp_cs or cs == 5635, 'got wrong overview checksum.'

###############################################################################
# Similar to tiff_ovr_9 but with internal overviews.


def test_tiff_ovr_10(both_endian):

    src_ds = gdal.Open('data/rgbsmall.tif', gdal.GA_ReadOnly)

    assert src_ds is not None, 'Failed to open test dataset.'

    ds = gdaltest.tiff_drv.CreateCopy('tmp/ovr10.tif', src_ds, options=['COMPRESS=JPEG', 'PHOTOMETRIC=YCBCR'])
    src_ds = None

    assert ds is not None, 'Failed to apply JPEG compression.'

    ds.BuildOverviews('AVERAGE', overviewlist=[2])

    ds = None
    ds = gdal.Open('tmp/ovr10.tif', gdal.GA_ReadOnly)

    assert ds is not None, 'Failed to open copy of test dataset.'

    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    exp_cs = 5562

    ds = None

    assert cs == exp_cs or cs == 5635, 'got wrong overview checksum.'

###############################################################################
# Overview on a dataset with NODATA_VALUES


def test_tiff_ovr_11(both_endian):

    src_ds = gdal.Open('data/test_nodatavalues.tif', gdal.GA_ReadOnly)

    assert src_ds is not None, 'Failed to open test dataset.'

    ds = gdaltest.tiff_drv.CreateCopy('tmp/ovr11.tif', src_ds)
    src_ds = None

    ds.BuildOverviews('AVERAGE', overviewlist=[2])

    ds = None
    ds = gdal.Open('tmp/ovr11.tif', gdal.GA_ReadOnly)

    assert ds is not None, 'Failed to open copy of test dataset.'

    cs = ds.GetRasterBand(2).GetOverview(0).Checksum()
    # If NODATA_VALUES was ignored, we would get 2766
    exp_cs = 2792

    ds = None

    assert cs == exp_cs, 'got wrong overview checksum.'

###############################################################################
# Same as tiff_ovr_11 but with compression to trigger the multiband overview
# code


def test_tiff_ovr_12(both_endian):

    src_ds = gdal.Open('data/test_nodatavalues.tif', gdal.GA_ReadOnly)

    assert src_ds is not None, 'Failed to open test dataset.'

    ds = gdaltest.tiff_drv.CreateCopy('tmp/ovr12.tif', src_ds, options=['COMPRESS=DEFLATE'])
    src_ds = None

    ds.BuildOverviews('AVERAGE', overviewlist=[2])

    ds = None
    ds = gdal.Open('tmp/ovr12.tif', gdal.GA_ReadOnly)

    assert ds is not None, 'Failed to open copy of test dataset.'

    cs = ds.GetRasterBand(2).GetOverview(0).Checksum()
    # If NODATA_VALUES was ignored, we would get 2766
    exp_cs = 2792

    ds = None

    assert cs == exp_cs, 'got wrong overview checksum.'


###############################################################################
# Test gaussian resampling

def test_tiff_ovr_13(both_endian):

    gdaltest.tiff_drv = gdal.GetDriverByName('GTiff')

    src_ds = gdal.Open('data/mfloat32.vrt')

    assert src_ds is not None, 'Failed to open test dataset.'

    gdaltest.tiff_drv.CreateCopy('tmp/mfloat32.tif', src_ds,
                                 options=['INTERLEAVE=PIXEL'])
    src_ds = None

    ds = gdal.Open('tmp/mfloat32.tif')

    assert ds is not None, 'Failed to open test dataset.'

    err = ds.BuildOverviews('GAUSS', overviewlist=[2, 4])

    assert err == 0, 'BuildOverviews reports an error'

    # if ds.GetRasterBand(1).GetOverview(0).Checksum() != 1225:
    #    gdaltest.post_reason( 'bad checksum' )
    #    return 'fail'

    ds = None

###############################################################################
# Check gauss resampling on a dataset with a raster band that has a color table


def test_tiff_ovr_14(both_endian):

    shutil.copyfile('data/test_average_palette.tif', 'tmp/test_gauss_palette.tif')

    ds = gdal.Open('tmp/test_gauss_palette.tif', gdal.GA_Update)

    assert ds is not None, 'Failed to open test dataset.'

    ds.BuildOverviews('GAUSS', overviewlist=[2])

    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    exp_cs = 200

    ds = None

    assert cs == exp_cs, 'got wrong overview checksum.'

###############################################################################
# Same as tiff_ovr_11 but with gauss, and compression to trigger the multiband overview
# code


def test_tiff_ovr_15(both_endian):

    src_ds = gdal.Open('data/test_nodatavalues.tif', gdal.GA_ReadOnly)

    assert src_ds is not None, 'Failed to open test dataset.'

    ds = gdaltest.tiff_drv.CreateCopy('tmp/ovr15.tif', src_ds, options=['COMPRESS=DEFLATE'])
    src_ds = None

    ds.BuildOverviews('GAUSS', overviewlist=[2])

    ds = None
    ds = gdal.Open('tmp/ovr15.tif', gdal.GA_ReadOnly)

    assert ds is not None, 'Failed to open copy of test dataset.'

    cs = ds.GetRasterBand(2).GetOverview(0).Checksum()
    # If NODATA_VALUES was ignored, we would get 2954
    exp_cs = 2987

    ds = None

    assert cs == exp_cs, 'got wrong overview checksum.'


###############################################################################
# Test mode resampling on non-byte dataset

def test_tiff_ovr_16(both_endian):

    gdaltest.tiff_drv = gdal.GetDriverByName('GTiff')

    src_ds = gdal.Open('data/mfloat32.vrt')

    assert src_ds is not None, 'Failed to open test dataset.'

    gdaltest.tiff_drv.CreateCopy('tmp/ovr16.tif', src_ds,
                                 options=['INTERLEAVE=PIXEL'])
    src_ds = None

    ds = gdal.Open('tmp/ovr16.tif')

    assert ds is not None, 'Failed to open test dataset.'

    err = ds.BuildOverviews('MODE', overviewlist=[2, 4])

    assert err == 0, 'BuildOverviews reports an error'

    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    exp_cs = 1122
    assert cs == exp_cs, 'bad checksum'

    ds = None


###############################################################################
# Test mode resampling on a byte dataset

def test_tiff_ovr_17(both_endian):

    shutil.copyfile('data/byte.tif', 'tmp/ovr17.tif')

    ds = gdal.Open('tmp/ovr17.tif')

    assert ds is not None, 'Failed to open test dataset.'

    err = ds.BuildOverviews('MODE', overviewlist=[2, 4])

    assert err == 0, 'BuildOverviews reports an error'

    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    exp_cs = 1122
    assert cs == exp_cs, 'bad checksum'

    ds = None

###############################################################################
# Check mode resampling on a dataset with a raster band that has a color table


def test_tiff_ovr_18(both_endian):

    shutil.copyfile('data/test_average_palette.tif', 'tmp/ovr18.tif')

    ds = gdal.Open('tmp/ovr18.tif', gdal.GA_Update)

    assert ds is not None, 'Failed to open test dataset.'

    ds.BuildOverviews('MODE', overviewlist=[2])

    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    exp_cs = 100

    ds = None

    assert cs == exp_cs, 'got wrong overview checksum.'

###############################################################################
# Check that we can create overviews on a newly create file (#2621)


def test_tiff_ovr_19(both_endian):

    ds = gdal.GetDriverByName('GTiff').Create('tmp/ovr19.tif', 100, 100, 1)
    ds.GetRasterBand(1).Fill(1)

    # The flush is important to simulate the behaviour that wash it by #2621
    ds.FlushCache()
    ds.BuildOverviews('NEAR', overviewlist=[2])
    ds.FlushCache()
    ds.BuildOverviews('NEAR', overviewlist=[2, 4])

    assert ds.GetRasterBand(1).GetOverviewCount() == 2, \
        'Overview could not be generated'

    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    assert cs == 2500

    cs = ds.GetRasterBand(1).GetOverview(1).Checksum()
    assert cs == 625

    ds = None


###############################################################################
# Test BIGTIFF_OVERVIEW=YES option

def test_tiff_ovr_20(both_endian):

    ds = gdaltest.tiff_drv.Create('tmp/ovr20.tif', 100, 100, 1)
    ds = None

    ds = gdal.Open('tmp/ovr20.tif')

    assert ds is not None, 'Failed to open test dataset.'

    with gdaltest.config_option('BIGTIFF_OVERVIEW', 'YES'):
        ds.BuildOverviews('NEAREST', overviewlist=[2, 4])

    ds = None

    fileobj = open('tmp/ovr20.tif.ovr', mode='rb')
    binvalues = array.array('b')
    binvalues.fromfile(fileobj, 4)
    fileobj.close()

    # Check BigTIFF signature
    assert (not ((binvalues[2] != 0x2B or binvalues[3] != 0) and
            (binvalues[3] != 0x2B or binvalues[2] != 0)))


###############################################################################
# Test BIGTIFF_OVERVIEW=IF_NEEDED option

def test_tiff_ovr_21(both_endian):

    ds = gdaltest.tiff_drv.Create('tmp/ovr21.tif', 170000, 100000, 1, options=['SPARSE_OK=YES'])
    ds = None

    ds = gdal.Open('tmp/ovr21.tif')

    assert ds is not None, 'Failed to open test dataset.'

    # 170 k * 100 k = 17 GB. 17 GB / (2^2) = 4.25 GB > 4.2 GB
    # so BigTIFF is needed
    ds.BuildOverviews('NONE', overviewlist=[2])

    ds = None

    fileobj = open('tmp/ovr21.tif.ovr', mode='rb')
    binvalues = array.array('b')
    binvalues.fromfile(fileobj, 4)
    fileobj.close()

    # Check BigTIFF signature
    assert (not ((binvalues[2] != 0x2B or binvalues[3] != 0) and
            (binvalues[3] != 0x2B or binvalues[2] != 0)))

###############################################################################
# Test BIGTIFF_OVERVIEW=NO option when BigTIFF is really needed


def test_tiff_ovr_22(both_endian):

    ds = gdaltest.tiff_drv.Create('tmp/ovr22.tif', 170000, 100000, 1, options=['SPARSE_OK=YES'])
    ds = None

    ds = gdal.Open('tmp/ovr22.tif')

    assert ds is not None, 'Failed to open test dataset.'

    # 170 k * 100 k = 17 GB. 17 GB / (2^2) = 4.25 GB > 4.2 GB
    # so BigTIFF is needed
    with gdaltest.config_option('BIGTIFF_OVERVIEW', 'NO'):
        with gdaltest.error_handler():
            err = ds.BuildOverviews('NONE', overviewlist=[2])

    ds = None

    if err != 0:
        return
    pytest.fail()

###############################################################################
# Same as before, but BigTIFF might be not needed as we use a compression
# method for the overviews.


def test_tiff_ovr_23(both_endian):

    ds = gdaltest.tiff_drv.Create('tmp/ovr23.tif', 170000, 100000, 1, options=['SPARSE_OK=YES'])
    ds = None

    ds = gdal.Open('tmp/ovr23.tif')

    assert ds is not None, 'Failed to open test dataset.'

    with gdaltest.config_option('BIGTIFF_OVERVIEW', 'NO'):
        with gdaltest.config_option('COMPRESS_OVERVIEW', 'DEFLATE'):
            ds.BuildOverviews('NONE', overviewlist=[2])

    ds = None

    fileobj = open('tmp/ovr23.tif.ovr', mode='rb')
    binvalues = array.array('b')
    binvalues.fromfile(fileobj, 4)
    fileobj.close()

    # Check Classical TIFF signature
    assert (not ((binvalues[2] != 0x2A or binvalues[3] != 0) and
            (binvalues[3] != 0x2A or binvalues[2] != 0)))

###############################################################################
# Test BIGTIFF_OVERVIEW=IF_SAFER option


def test_tiff_ovr_24(both_endian):

    ds = gdaltest.tiff_drv.Create('tmp/ovr24.tif', 85000, 100000, 1, options=['SPARSE_OK=YES'])
    ds = None

    ds = gdal.Open('tmp/ovr24.tif')

    assert ds is not None, 'Failed to open test dataset.'

    # 85 k * 100 k = 8.5 GB, so BigTIFF might be needed as
    # 8.5 GB / 2 > 4.2 GB
    with gdaltest.config_option('BIGTIFF_OVERVIEW', 'IF_SAFER'):
        ds.BuildOverviews('NONE', overviewlist=[16])

    ds = None

    fileobj = open('tmp/ovr24.tif.ovr', mode='rb')
    binvalues = array.array('b')
    binvalues.fromfile(fileobj, 4)
    fileobj.close()

    # Check BigTIFF signature
    assert (not ((binvalues[2] != 0x2B or binvalues[3] != 0) and
            (binvalues[3] != 0x2B or binvalues[2] != 0)))

###############################################################################
# Test creating overviews after some blocks have been written in the main
# band and actually flushed


def test_tiff_ovr_25(both_endian):

    ds = gdaltest.tiff_drv.Create('tmp/ovr25.tif', 100, 100, 1)
    ds.GetRasterBand(1).Fill(1)
    ds.GetRasterBand(1).FlushCache()
    ds.BuildOverviews('NEAR', overviewlist=[2])
    ds = None

    ds = gdal.Open('tmp/ovr25.tif')
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 10000

    assert ds.GetRasterBand(1).GetOverviewCount() != 0

    assert ds.GetRasterBand(1).GetOverview(0).Checksum() == 2500

###############################################################################
# Test gdal.RegenerateOverview()


def test_tiff_ovr_26(both_endian):

    ds = gdaltest.tiff_drv.Create('tmp/ovr26.tif', 100, 100, 1)
    ds.GetRasterBand(1).Fill(1)
    ds.GetRasterBand(1).FlushCache()
    ds.BuildOverviews('NEAR', overviewlist=[2])
    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    ds.GetRasterBand(1).GetOverview(0).Fill(0)
    cs_new = ds.GetRasterBand(1).GetOverview(0).Checksum()
    assert cs_new == 0
    gdal.RegenerateOverview(ds.GetRasterBand(1), ds.GetRasterBand(1).GetOverview(0), 'NEAR')
    cs_new = ds.GetRasterBand(1).GetOverview(0).Checksum()
    assert cs == cs_new
    ds = None

###############################################################################
# Test gdal.RegenerateOverviews()


def test_tiff_ovr_27(both_endian):

    ds = gdaltest.tiff_drv.Create('tmp/ovr27.tif', 100, 100, 1)
    ds.GetRasterBand(1).Fill(1)
    ds.GetRasterBand(1).FlushCache()
    ds.BuildOverviews('NEAR', overviewlist=[2, 4])
    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    cs2 = ds.GetRasterBand(1).GetOverview(1).Checksum()
    ds.GetRasterBand(1).GetOverview(0).Fill(0)
    ds.GetRasterBand(1).GetOverview(1).Fill(0)
    cs_new = ds.GetRasterBand(1).GetOverview(0).Checksum()
    cs2_new = ds.GetRasterBand(1).GetOverview(1).Checksum()
    assert cs_new == 0 and cs2_new == 0
    gdal.RegenerateOverviews(ds.GetRasterBand(1), [ds.GetRasterBand(1).GetOverview(0), ds.GetRasterBand(1).GetOverview(1)], 'NEAR')
    cs_new = ds.GetRasterBand(1).GetOverview(0).Checksum()
    cs2_new = ds.GetRasterBand(1).GetOverview(1).Checksum()
    assert cs == cs_new
    assert cs2 == cs2_new
    ds = None

###############################################################################
# Test cleaning overviews.


def test_tiff_ovr_28(both_endian):

    ds = gdal.Open('tmp/ovr25.tif', gdal.GA_Update)
    assert ds.BuildOverviews(overviewlist=[]) == 0, \
        'BuildOverviews() returned error code.'

    assert ds.GetRasterBand(1).GetOverviewCount() == 0, \
        'Overview(s) appear to still exist.'

    # Close and reopen to confirm they are really gone.
    ds = None
    ds = gdal.Open('tmp/ovr25.tif')
    assert ds.GetRasterBand(1).GetOverviewCount() == 0, \
        'Overview(s) appear to still exist after reopen.'

###############################################################################
# Test cleaning external overviews (ovr) on a non-TIFF format.


def test_tiff_ovr_29(both_endian):

    src_ds = gdal.Open('data/byte.tif')
    png_ds = gdal.GetDriverByName('PNG').CreateCopy('tmp/ovr29.png', src_ds)
    src_ds = None

    png_ds.BuildOverviews(overviewlist=[2])
    png_ds = None

    assert open('tmp/ovr29.png.ovr') is not None, 'Did not expected .ovr file.'

    png_ds = gdal.Open('tmp/ovr29.png')

    assert png_ds.GetRasterBand(1).GetOverviewCount() == 1, 'did not find overview'

    png_ds.BuildOverviews(overviewlist=[])
    assert png_ds.GetRasterBand(1).GetOverviewCount() == 0, 'delete overview failed.'

    png_ds = None
    png_ds = gdal.Open('tmp/ovr29.png')

    assert png_ds.GetRasterBand(1).GetOverviewCount() == 0, 'delete overview failed.'

    png_ds = None

    assert not os.path.exists('tmp/ovr29.png.ovr')

    gdal.GetDriverByName('PNG').Delete('tmp/ovr29.png')

###############################################################################
# Test fix for #2988.


def test_tiff_ovr_30(both_endian):

    ds = gdaltest.tiff_drv.Create('tmp/ovr30.tif', 20, 20, 1)
    ds.BuildOverviews(overviewlist=[2])
    ds = None

    ds = gdal.Open('tmp/ovr30.tif', gdal.GA_Update)
    ds.SetMetadata({'TEST_KEY': 'TestValue'})
    ds = None

    ds = gdaltest.tiff_drv.Create('tmp/ovr30.tif', 20, 20, 1)
    ds.BuildOverviews(overviewlist=[2])
    ds = None

    ds = gdal.Open('tmp/ovr30.tif', gdal.GA_Update)
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(4326)
    ds.SetProjection(sr.ExportToWkt())
    ds = None

    ds = gdal.Open('tmp/ovr30.tif')
    assert ds.GetProjectionRef().find('4326') != -1

###############################################################################
# Test fix for #3033


def test_tiff_ovr_31(both_endian):

    ds = gdaltest.tiff_drv.Create('tmp/ovr31.tif', 100, 100, 4)
    ds.GetRasterBand(1).Fill(255)
    ds.GetRasterBand(2).Fill(255)
    ds.GetRasterBand(3).Fill(255)
    ds.GetRasterBand(4).Fill(255)
    ds.BuildOverviews('average', overviewlist=[2, 4])
    cs = ds.GetRasterBand(1).GetOverview(1).Checksum()
    expected_cs = 7646
    assert cs == expected_cs, \
        ('Checksum is %d. Expected checksum is %d' % (cs, expected_cs))
    ds = None

###############################################################################
# Test Cubic sampling.


def test_tiff_ovr_32(both_endian):

    # 4 regular band
    shutil.copyfile('data/stefan_full_rgba_photometric_rgb.tif', 'tmp/ovr32.tif')

    ds = gdal.Open('tmp/ovr32.tif', gdal.GA_Update)
    ds.BuildOverviews('cubic', overviewlist=[2, 5])

    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    expected_cs = 21168
    assert cs == expected_cs, \
        ('Checksum is %d. Expected checksum is %d for overview 0.' % (cs, expected_cs))

    cs = ds.GetRasterBand(3).GetOverview(1).Checksum()
    expected_cs = 1851
    assert cs == expected_cs, \
        ('Checksum is %d. Expected checksum is %d for overview 1.' % (cs, expected_cs))

    ds = None

    gdaltest.tiff_drv.Delete('tmp/ovr32.tif')

    # Same, but with non-byte data type (help testing the non-SSE2 code path)
    src_ds = gdal.Open('data/stefan_full_rgba_photometric_rgb.tif')

    tmp_ds = gdal.GetDriverByName('GTiff').Create('/vsimem/ovr32_float.tif', src_ds.RasterXSize, src_ds.RasterYSize, src_ds.RasterCount, gdal.GDT_Float32)
    src_data = src_ds.ReadRaster(0, 0, src_ds.RasterXSize, src_ds.RasterYSize)
    tmp_ds.WriteRaster(0, 0, src_ds.RasterXSize, src_ds.RasterYSize, src_data, buf_type=gdal.GDT_Byte)
    tmp_ds.BuildOverviews('cubic', overviewlist=[2])

    tmp2_ds = gdal.GetDriverByName('GTiff').Create('/vsimem/ovr32_byte.tif', tmp_ds.RasterXSize, tmp_ds.RasterYSize, tmp_ds.RasterCount)
    tmp2_ds.BuildOverviews('NONE', overviewlist=[2])
    tmp2_ovr_ds = tmp2_ds.GetRasterBand(1).GetOverview(0).GetDataset()
    tmp_ovr_ds = tmp_ds.GetRasterBand(1).GetOverview(0).GetDataset()
    src_data = tmp_ovr_ds.ReadRaster(0, 0, tmp_ovr_ds.RasterXSize, tmp_ovr_ds.RasterYSize, buf_type=gdal.GDT_Byte)
    tmp2_ovr_ds.WriteRaster(0, 0, tmp_ovr_ds.RasterXSize, tmp_ovr_ds.RasterYSize, src_data)

    cs = tmp2_ds.GetRasterBand(1).GetOverview(0).Checksum()
    expected_cs = 21168
    assert cs == expected_cs, \
        ('Checksum is %d. Expected checksum is %d for overview 0.' % (cs, expected_cs))

    src_ds = None
    tmp_ds = None
    tmp2_ds = None
    gdaltest.tiff_drv.Delete('/vsimem/ovr32_float.tif')
    gdaltest.tiff_drv.Delete('/vsimem/ovr32_byte.tif')

    # Test GDALRegenerateOverviewsMultiBand
    shutil.copyfile('data/stefan_full_rgba_photometric_rgb.tif', 'tmp/ovr32.tif')

    ds = gdal.Open('tmp/ovr32.tif')
    with gdaltest.config_option('COMPRESS_OVERVIEW', 'DEFLATE'):
        with gdaltest.config_option('INTERLEAVE_OVERVIEW', 'PIXEL'):
            ds.BuildOverviews('cubic', overviewlist=[2, 5])

    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    expected_cs = 21168
    assert cs == expected_cs, \
        ('Checksum is %d. Expected checksum is %d for overview 0.' % (cs, expected_cs))

    cs = ds.GetRasterBand(3).GetOverview(1).Checksum()
    expected_cs = 1851
    assert cs == expected_cs, \
        ('Checksum is %d. Expected checksum is %d for overview 1.' % (cs, expected_cs))

    ds = None

    gdaltest.tiff_drv.Delete('tmp/ovr32.tif')

    # 3 bands + alpha
    shutil.copyfile('data/stefan_full_rgba.tif', 'tmp/ovr32.tif')

    ds = gdal.Open('tmp/ovr32.tif', gdal.GA_Update)
    ds.BuildOverviews('cubic', overviewlist=[2, 5])

    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    expected_cs = 21656
    assert cs == expected_cs, \
        ('Checksum is %d. Expected checksum is %d for overview 0.' % (cs, expected_cs))

    cs = ds.GetRasterBand(3).GetOverview(1).Checksum()
    expected_cs = 2132
    assert cs == expected_cs, \
        ('Checksum is %d. Expected checksum is %d for overview 1.' % (cs, expected_cs))

    ds = None

    gdaltest.tiff_drv.Delete('tmp/ovr32.tif')

    # Same, but with non-byte data type (help testing the non-SSE2 code path)
    src_ds = gdal.Open('data/stefan_full_rgba.tif')

    tmp_ds = gdal.GetDriverByName('GTiff').Create('/vsimem/ovr32_float.tif', src_ds.RasterXSize, src_ds.RasterYSize, src_ds.RasterCount, gdal.GDT_Float32)
    src_data = src_ds.ReadRaster(0, 0, src_ds.RasterXSize, src_ds.RasterYSize)
    tmp_ds.WriteRaster(0, 0, src_ds.RasterXSize, src_ds.RasterYSize, src_data, buf_type=gdal.GDT_Byte)
    tmp_ds.BuildOverviews('cubic', overviewlist=[2])

    tmp2_ds = gdal.GetDriverByName('GTiff').Create('/vsimem/ovr32_byte.tif', tmp_ds.RasterXSize, tmp_ds.RasterYSize, tmp_ds.RasterCount)
    tmp2_ds.BuildOverviews('NONE', overviewlist=[2])
    tmp2_ovr_ds = tmp2_ds.GetRasterBand(1).GetOverview(0).GetDataset()
    tmp_ovr_ds = tmp_ds.GetRasterBand(1).GetOverview(0).GetDataset()
    src_data = tmp_ovr_ds.ReadRaster(0, 0, tmp_ovr_ds.RasterXSize, tmp_ovr_ds.RasterYSize, buf_type=gdal.GDT_Byte)
    tmp2_ovr_ds.WriteRaster(0, 0, tmp_ovr_ds.RasterXSize, tmp_ovr_ds.RasterYSize, src_data)

    cs = tmp2_ds.GetRasterBand(1).GetOverview(0).Checksum()
    # expected_cs = 21656
    expected_cs = 21168
    assert cs == expected_cs, \
        ('Checksum is %d. Expected checksum is %d for overview 0.' % (cs, expected_cs))

    src_ds = None
    tmp_ds = None
    tmp2_ds = None
    gdaltest.tiff_drv.Delete('/vsimem/ovr32_float.tif')
    gdaltest.tiff_drv.Delete('/vsimem/ovr32_byte.tif')

    # Same test with a compressed dataset
    src_ds = gdal.Open('data/stefan_full_rgba.tif')
    ds = gdal.GetDriverByName('GTiff').CreateCopy('tmp/ovr32.tif', src_ds, options=['COMPRESS=DEFLATE'])
    ds.BuildOverviews('cubic', overviewlist=[2, 5])

    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    expected_cs = 21656
    assert cs == expected_cs, \
        ('Checksum is %d. Expected checksum is %d for overview 0.' % (cs, expected_cs))

    cs = ds.GetRasterBand(3).GetOverview(1).Checksum()
    expected_cs = 2132
    assert cs == expected_cs, \
        ('Checksum is %d. Expected checksum is %d for overview 1.' % (cs, expected_cs))

    ds = None

    gdaltest.tiff_drv.Delete('tmp/ovr32.tif')


###############################################################################
# Test creation of overviews on a 1x1 dataset (fix for #3069)

def test_tiff_ovr_33(both_endian):

    try:
        os.remove('tmp/ovr33.tif.ovr')
    except OSError:
        pass

    ds = gdaltest.tiff_drv.Create('tmp/ovr33.tif', 1, 1, 1)
    ds = None
    ds = gdal.Open('tmp/ovr33.tif')
    ds.BuildOverviews('NEAREST', overviewlist=[2, 4])
    ds = None

    gdaltest.tiff_drv.Delete('tmp/ovr33.tif')


###############################################################################
# Confirm that overviews are used on a Band.RasterIO().

def test_tiff_ovr_34(both_endian):

    ds_in = gdal.Open('data/byte.tif')
    ds = gdaltest.tiff_drv.CreateCopy('tmp/ovr34.tif', ds_in)
    ds.BuildOverviews('NEAREST', overviewlist=[2])
    ds.GetRasterBand(1).GetOverview(0).Fill(32.0)
    ds = None
    ds_in = None

    ds = gdal.Open('tmp/ovr34.tif')
    data = ds.GetRasterBand(1).ReadRaster(0, 0, 20, 20, buf_xsize=5, buf_ysize=5)
    ds = None

    if data != '                         '.encode('ascii'):
        print('[%s]' % data)
        pytest.fail('did not get expected cleared overview.')

    gdaltest.tiff_drv.Delete('tmp/ovr34.tif')

###############################################################################
# Confirm that overviews are used on a Band.RasterIO().


def test_tiff_ovr_35(both_endian):

    ds_in = gdal.Open('data/byte.tif')
    ds = gdaltest.tiff_drv.CreateCopy('tmp/ovr35.tif', ds_in)
    ds.BuildOverviews('NEAREST', overviewlist=[2])
    ds.GetRasterBand(1).GetOverview(0).Fill(32.0)
    ds = None
    ds_in = None

    ds = gdal.Open('tmp/ovr35.tif')
    data = ds.ReadRaster(0, 0, 20, 20, buf_xsize=5, buf_ysize=5, band_list=[1])
    ds = None

    if data != '                         '.encode('ascii'):
        print('[%s]' % data)
        pytest.fail('did not get expected cleared overview.')

    gdaltest.tiff_drv.Delete('tmp/ovr35.tif')

###############################################################################
# Confirm that overviews are used on a Band.RasterIO() when using BlockBasedRasterIO() (#3124)


def test_tiff_ovr_36(both_endian):

    with gdaltest.config_option('GDAL_FORCE_CACHING', 'YES'):
        ret = test_tiff_ovr_35(both_endian)
    return ret

###############################################################################
# Test PREDICTOR_OVERVIEW=2 option. (#3414)


def test_tiff_ovr_37(both_endian):

    shutil.copy('../gdrivers/data/n43.dt0', 'tmp/ovr37.dt0')

    ds = gdal.Open('tmp/ovr37.dt0')

    assert ds is not None, 'Failed to open test dataset.'

    with gdaltest.config_option('PREDICTOR_OVERVIEW', '2'):
        with gdaltest.config_option('COMPRESS_OVERVIEW', 'LZW'):
            ds.BuildOverviews('NEAR', overviewlist=[2])

    ds = None

    ds = gdal.Open('tmp/ovr37.dt0')
    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    assert cs == 45378, 'got wrong overview checksum.'
    ds = None

    predictor2_size = os.stat('tmp/ovr37.dt0.ovr')[stat.ST_SIZE]
    # 3789 : on little-endian host
    # 3738 : on big-endian host
    assert predictor2_size in (3789, 3738), 'did not get expected file size.'

###############################################################################
# Test that the predictor flag gets well propagated to internal overviews


def test_tiff_ovr_38(both_endian):

    src_ds = gdal.Open('../gdrivers/data/n43.dt0')
    ds = gdaltest.tiff_drv.CreateCopy('tmp/ovr38.tif', src_ds, options=['COMPRESS=LZW', 'PREDICTOR=2'])
    ds.BuildOverviews(overviewlist=[2, 4])
    ds = None

    file_size = os.stat('tmp/ovr38.tif')[stat.ST_SIZE]

    assert file_size <= 21000, 'did not get expected file size.'

###############################################################################
# Test external overviews on all datatypes


def test_tiff_ovr_39(both_endian):

    for datatype in [gdal.GDT_Byte,
                     gdal.GDT_Int16,
                     gdal.GDT_UInt16,
                     gdal.GDT_Int32,
                     gdal.GDT_UInt32,
                     gdal.GDT_Float32,
                     gdal.GDT_Float64,
                     gdal.GDT_CInt16,
                     gdal.GDT_CInt32,
                     gdal.GDT_CFloat32,
                     gdal.GDT_CFloat64]:

        gdal.Translate('tmp/ovr39.tif', 'data/byte.tif', options='-ot ' + gdal.GetDataTypeName(datatype))
        try:
            os.remove('tmp/ovr39.tif.ovr')
        except OSError:
            pass

        ds = gdal.Open('tmp/ovr39.tif')
        ds.BuildOverviews('NEAREST', overviewlist=[2])
        ds = None

        ds = gdal.Open('tmp/ovr39.tif.ovr')
        ovr_datatype = ds.GetRasterBand(1).DataType
        ds = None

        assert datatype == ovr_datatype, 'did not get expected datatype'

        ds = gdal.Open('tmp/ovr39.tif')
        cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
        ds = None

        if gdal.DataTypeIsComplex(datatype):
            expected_cs = 1171
        else:
            expected_cs = 1087

        assert cs == expected_cs, \
            ('did not get expected checksum for datatype %s' % gdal.GetDataTypeName(datatype))


###############################################################################
# Test external overviews on 1 bit datasets with AVERAGE_BIT2GRAYSCALE (similar to tiff_ovr_4)


def test_tiff_ovr_40(both_endian):

    shutil.copyfile('data/oddsize_1bit2b.tif', 'tmp/ovr40.tif')

    wrk_ds = gdal.Open('tmp/ovr40.tif')

    assert wrk_ds is not None, 'Failed to open test dataset.'

    wrk_ds.BuildOverviews('AVERAGE_BIT2GRAYSCALE', overviewlist=[2, 4])
    wrk_ds = None

    wrk_ds = gdal.Open('tmp/ovr40.tif')

    ovband = wrk_ds.GetRasterBand(1).GetOverview(1)
    md = ovband.GetMetadata()
    assert 'RESAMPLING' in md and md['RESAMPLING'] == 'AVERAGE_BIT2GRAYSCALE', \
        'Did not get expected RESAMPLING metadata.'

    # compute average value of overview band image data.
    ovimage = ovband.ReadRaster(0, 0, ovband.XSize, ovband.YSize)

    pix_count = ovband.XSize * ovband.YSize
    total = float(sum(ovimage))
    average = total / pix_count
    exp_average = 154.0992
    assert average == pytest.approx(exp_average, abs=0.1), 'got wrong average for overview image'

    # Read base band as overview resolution and verify we aren't getting
    # the grayscale image.

    frband = wrk_ds.GetRasterBand(1)
    ovimage = frband.ReadRaster(0, 0, frband.XSize, frband.YSize,
                                ovband.XSize, ovband.YSize)

    pix_count = ovband.XSize * ovband.YSize
    total = float(sum(ovimage))
    average = total / pix_count
    exp_average = 0.6096

    assert average == pytest.approx(exp_average, abs=0.01), 'got wrong average for downsampled image'

    wrk_ds = None

###############################################################################
# Test external overviews on 1 bit datasets with NEAREST


def test_tiff_ovr_41(both_endian):

    shutil.copyfile('data/oddsize_1bit2b.tif', 'tmp/ovr41.tif')

    ds = gdal.Open('tmp/ovr41.tif')
    # data = wrk_ds.GetRasterBand(1).ReadRaster(0,0,99,99,50,50)
    ds.BuildOverviews('NEAREST', overviewlist=[2])
    ds = None

    # ds = gdaltest.tiff_drv.Create('tmp/ovr41.tif.handmade.ovr',50,50,1,options=['NBITS=1'])
    # ds.GetRasterBand(1).WriteRaster(0,0,50,50,data)
    # ds = None

    ds = gdal.Open('tmp/ovr41.tif')
    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    ds = None

    assert cs == 1496, 'did not get expected checksum'

###############################################################################
# Test external overviews on dataset with color table


def test_tiff_ovr_42(both_endian):

    ct_data = [(255, 0, 0), (0, 255, 0), (0, 0, 255), (255, 255, 255)]

    ct = gdal.ColorTable()
    for i, data in enumerate(ct_data):
        ct.SetColorEntry(i, data)

    ds = gdaltest.tiff_drv.Create('tmp/ovr42.tif', 1, 1)
    ds.GetRasterBand(1).SetRasterColorTable(ct)
    ds = None

    ds = gdal.Open('tmp/ovr42.tif')
    ds.BuildOverviews('NEAREST', overviewlist=[2])
    ds = None

    ds = gdal.Open('tmp/ovr42.tif.ovr')
    ct2 = ds.GetRasterBand(1).GetRasterColorTable()

    assert (ct2.GetCount() == 256 and \
       ct2.GetColorEntry(0) == (255, 0, 0, 255) and \
       ct2.GetColorEntry(1) == (0, 255, 0, 255) and \
       ct2.GetColorEntry(2) == (0, 0, 255, 255) and \
       ct2.GetColorEntry(3) == (255, 255, 255, 255)), 'Wrong color table entry.'

    ds = None

###############################################################################
# Make sure that 16bit overviews with JPEG compression are handled using 12-bit
# jpeg-in-tiff (#3539)


def test_tiff_ovr_43(both_endian):

    md = gdaltest.tiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('JPEG') == -1:
        pytest.skip()

    with gdaltest.config_option('CPL_ACCUM_ERROR_MSG', 'ON'):
        gdal.ErrorReset()
        with gdaltest.error_handler():
            try:
                ds = gdal.Open('data/mandrilmini_12bitjpeg.tif')
                ds.GetRasterBand(1).ReadRaster(0, 0, 1, 1)
            except:
                ds = None

    if gdal.GetLastErrorMsg().find('Unsupported JPEG data precision 12') != -1:
        pytest.skip('12bit jpeg not available')

    ds = gdaltest.tiff_drv.Create('tmp/ovr43.tif', 16, 16, 1, gdal.GDT_UInt16)
    ds.GetRasterBand(1).Fill(4000)
    ds = None

    try:
        os.remove('tmp/ovr43.tif.ovr')
    except OSError:
        pass

    ds = gdal.Open('tmp/ovr43.tif')
    with gdaltest.config_option('COMPRESS_OVERVIEW', 'JPEG'):
        ds.BuildOverviews('NEAREST', overviewlist=[2])
        ds = None

    ds = gdal.Open('tmp/ovr43.tif.ovr')
    md = ds.GetRasterBand(1).GetMetadata('IMAGE_STRUCTURE')
    cs = ds.GetRasterBand(1).Checksum()
    ds = None

    assert 'NBITS' in md and md['NBITS'] == '12', 'did not get expected NBITS'

    assert cs == 642, 'did not get expected checksum'

    gdaltest.tiff_drv.Delete('tmp/ovr43.tif')

###############################################################################
# Test that we can change overview block size through GDAL_TIFF_OVR_BLOCKSIZE configuration
# option


def test_tiff_ovr_44(both_endian):

    shutil.copyfile('data/byte.tif', 'tmp/ovr44.tif')
    with gdaltest.config_option('GDAL_TIFF_OVR_BLOCKSIZE', '256'):
        ds = gdal.Open('tmp/ovr44.tif', gdal.GA_Update)
        ds.BuildOverviews(overviewlist=[2])
        ds = None

    ds = gdal.Open('tmp/ovr44.tif')
    ovr_band = ds.GetRasterBand(1).GetOverview(0)
    if 'GetBlockSize' in dir(gdal.Band):
        (blockx, blocky) = ovr_band.GetBlockSize()
        assert blockx == 256 and blocky == 256, 'did not get expected block size'
    cs = ovr_band.Checksum()
    ds = None

    gdaltest.tiff_drv.Delete('tmp/ovr44.tif')

    assert cs == 1087, 'did not get expected checksum'

###############################################################################
# Same as tiff_ovr_44, but with external overviews


def test_tiff_ovr_45(both_endian):

    shutil.copyfile('data/byte.tif', 'tmp/ovr45.tif')
    with gdaltest.config_option('GDAL_TIFF_OVR_BLOCKSIZE', '256'):
        ds = gdal.Open('tmp/ovr45.tif', gdal.GA_ReadOnly)
        ds.BuildOverviews(overviewlist=[2])
        ds = None

    ds = gdal.Open('tmp/ovr45.tif.ovr')
    ovr_band = ds.GetRasterBand(1)
    if 'GetBlockSize' in dir(gdal.Band):
        (blockx, blocky) = ovr_band.GetBlockSize()
        assert blockx == 256 and blocky == 256, 'did not get expected block size'
    cs = ovr_band.Checksum()
    ds = None

    gdaltest.tiff_drv.Delete('tmp/ovr45.tif')

    assert cs == 1087, 'did not get expected checksum'

###############################################################################
# Test overview on a dataset where width * height > 2 billion


def test_tiff_ovr_46():

    if not gdaltest.run_slow_tests():
        pytest.skip()

    # Test NEAREST
    with gdaltest.config_option('GTIFF_DONT_WRITE_BLOCKS', 'YES'):
        ds = gdaltest.tiff_drv.Create('/vsimem/tiff_ovr_46.tif', 50000, 50000, options=['SPARSE_OK=YES'])
        ds.BuildOverviews('NEAREST', overviewlist=[2])
        ds = None

    gdaltest.tiff_drv.Delete('/vsimem/tiff_ovr_46.tif')

    # Test AVERAGE in optimized case (x2 reduction)
    with gdaltest.config_option('GTIFF_DONT_WRITE_BLOCKS', 'YES'):
        ds = gdaltest.tiff_drv.Create('/vsimem/tiff_ovr_46.tif', 50000, 50000, options=['SPARSE_OK=YES'])
        ds.BuildOverviews('AVERAGE', overviewlist=[2])
        ds = None

    gdaltest.tiff_drv.Delete('/vsimem/tiff_ovr_46.tif')

    # Test AVERAGE in un-optimized case (x3 reduction)
    with gdaltest.config_option('GTIFF_DONT_WRITE_BLOCKS', 'YES'):
        ds = gdaltest.tiff_drv.Create('/vsimem/tiff_ovr_46.tif', 50000, 50000, options=['SPARSE_OK=YES'])
        ds.BuildOverviews('AVERAGE', overviewlist=[3])
        ds = None

    gdaltest.tiff_drv.Delete('/vsimem/tiff_ovr_46.tif')

    # Test AVERAGE in un-optimized case (color table)
    with gdaltest.config_option('GTIFF_DONT_WRITE_BLOCKS', 'YES'):
        ds = gdaltest.tiff_drv.Create('/vsimem/tiff_ovr_46.tif', 50000, 50000, options=['SPARSE_OK=YES'])

        ct = gdal.ColorTable()
        ct.SetColorEntry(0, (255, 0, 0))
        ds.GetRasterBand(1).SetRasterColorTable(ct)

        ds.BuildOverviews('AVERAGE', overviewlist=[2])
        ds = None

    gdaltest.tiff_drv.Delete('/vsimem/tiff_ovr_46.tif')

    # Test GAUSS
    with gdaltest.config_option('GTIFF_DONT_WRITE_BLOCKS', 'YES'):
        ds = gdaltest.tiff_drv.Create('/vsimem/tiff_ovr_46.tif', 50000, 50000, options=['SPARSE_OK=YES'])
        ds.BuildOverviews('GAUSS', overviewlist=[2])
        ds = None

    gdaltest.tiff_drv.Delete('/vsimem/tiff_ovr_46.tif')

    # Test GAUSS with color table
    with gdaltest.config_option('GTIFF_DONT_WRITE_BLOCKS', 'YES'):
        ds = gdaltest.tiff_drv.Create('/vsimem/tiff_ovr_46.tif', 50000, 50000, options=['SPARSE_OK=YES'])

        ct = gdal.ColorTable()
        ct.SetColorEntry(0, (255, 0, 0))
        ds.GetRasterBand(1).SetRasterColorTable(ct)

        ds.BuildOverviews('GAUSS', overviewlist=[2])
        ds = None

    gdaltest.tiff_drv.Delete('/vsimem/tiff_ovr_46.tif')

    # Test MODE
    with gdaltest.config_option('GTIFF_DONT_WRITE_BLOCKS', 'YES'):
        ds = gdaltest.tiff_drv.Create('/vsimem/tiff_ovr_46.tif', 50000, 50000, options=['SPARSE_OK=YES'])
        ds.BuildOverviews('MODE', overviewlist=[2])
        ds = None

    gdaltest.tiff_drv.Delete('/vsimem/tiff_ovr_46.tif')

    # Test CUBIC
    with gdaltest.config_option('GTIFF_DONT_WRITE_BLOCKS', 'YES'):
        ds = gdaltest.tiff_drv.Create('/vsimem/tiff_ovr_46.tif', 50000, 50000, options=['SPARSE_OK=YES'])
        ds.BuildOverviews('CUBIC', overviewlist=[2])
        ds = None

    gdaltest.tiff_drv.Delete('/vsimem/tiff_ovr_46.tif')

###############################################################################
# Test workaround with libtiff 3.X when creating interleaved overviews


def test_tiff_ovr_47(both_endian):
    mem_drv = gdal.GetDriverByName('MEM')
    mem_ds = mem_drv.Create('', 852, 549, 3)

    for i in range(1, 4):
        band = mem_ds.GetRasterBand(i)
        band.Fill(128)

    driver = gdal.GetDriverByName("GTIFF")
    out_ds = driver.CreateCopy("/vsimem/tiff_ovr_47.tif", mem_ds)
    mem_ds = None
    out_ds.BuildOverviews("NEAREST", [2, 4, 8, 16])
    out_ds = None

    ds = gdal.Open("/vsimem/tiff_ovr_47.tif")
    cs = ds.GetRasterBand(1).GetOverview(1).Checksum()
    ds = None

    gdal.Unlink("/vsimem/tiff_ovr_47.tif")

    assert cs == 35721, 'did not get expected checksum'

###############################################################################
# Test that we don't average 0's in alpha band


def test_tiff_ovr_48(both_endian):

    shutil.copy('data/rgba_with_alpha_0_and_255.tif', 'tmp')
    ds = gdal.Open('tmp/rgba_with_alpha_0_and_255.tif')
    ds.BuildOverviews('AVERAGE', [2])
    ds = None

    ds = gdal.Open('tmp/rgba_with_alpha_0_and_255.tif.ovr')
    for i in range(4):
        cs = ds.GetRasterBand(i + 1).Checksum()
        assert cs == 3, i

    # But if we define GDAL_OVR_PROPAGATE_NODATA, a nodata value in source
    # samples will cause the target pixel to be zeroed.
    shutil.copy('data/rgba_with_alpha_0_and_255.tif', 'tmp')
    ds = gdal.Open('tmp/rgba_with_alpha_0_and_255.tif')
    with gdaltest.config_option('GDAL_OVR_PROPAGATE_NODATA', 'YES'):
        ds.BuildOverviews('AVERAGE', [2])
    ds = None

    ds = gdal.Open('tmp/rgba_with_alpha_0_and_255.tif.ovr')
    for i in range(4):
        cs = ds.GetRasterBand(i + 1).Checksum()
        assert cs == 0, i


###############################################################################
# Test possible stride computation issue in GDALRegenerateOverviewsMultiBand (#5653)


def test_tiff_ovr_49(both_endian):

    ds = gdal.GetDriverByName('GTiff').Create('/vsimem/tiff_ovr_49.tif', 1023, 1023, 1)
    ds.GetRasterBand(1).Fill(0)
    c = '\xFF'
    # Fails on 1.11.1 with col = 255 or col = 1019
    col = 1019
    ds.GetRasterBand(1).WriteRaster(col, 0, 1, 1023, c, 1, 1)
    ds = None
    ds = gdal.Open('/vsimem/tiff_ovr_49.tif')
    with gdaltest.config_option('COMPRESS_OVERVIEW', 'DEFLATE'):
        ds.BuildOverviews('AVERAGE', overviewlist=[2])
    ds = None
    ds = gdal.Open('/vsimem/tiff_ovr_49.tif.ovr')
    assert ds.GetRasterBand(1).Checksum() != 0
    ds = None
    gdal.GetDriverByName('GTiff').Delete('/vsimem/tiff_ovr_49.tif')

###############################################################################
# Test overviews when X dimension is smaller than Y (#5794)


def test_tiff_ovr_50(both_endian):

    ds = gdal.GetDriverByName('GTiff').Create('/vsimem/tiff_ovr_50.tif', 6, 8192, 3,
                                              options=['COMPRESS=DEFLATE'])
    ds.GetRasterBand(1).Fill(255)
    # We just check that it doesn't crash
    ds.BuildOverviews('AVERAGE', overviewlist=[2, 4, 8, 16, 32])
    ds.BuildOverviews('AVERAGE', overviewlist=[2, 4, 8, 16, 32])
    ds = None
    gdal.GetDriverByName('GTiff').Delete('/vsimem/tiff_ovr_50.tif')

###############################################################################
# Test average overview on a color palette with nodata values (#6371)


def test_tiff_ovr_51():

    src_ds = gdal.Open('data/stefan_full_rgba_pct32.png')
    if src_ds is None:
        pytest.skip()

    ds = gdal.GetDriverByName('PNG').CreateCopy('/vsimem/tiff_ovr_51.png', src_ds)
    ds.BuildOverviews('AVERAGE', [2])
    ds = None

    ds = gdal.Open('/vsimem/tiff_ovr_51.png.ovr')
    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 24518
    ds = None

    gdal.GetDriverByName('PNG').Delete('/vsimem/tiff_ovr_51.png')

###############################################################################
# Test unsorted external overview building (#6617)


def test_tiff_ovr_52():

    src_ds = gdal.Open('data/byte.tif')
    if src_ds is None:
        pytest.skip()

    gdal.GetDriverByName('GTiff').CreateCopy('/vsimem/tiff_ovr_52.tif', src_ds)
    with gdaltest.config_option('COMPRESS_OVERVIEW', 'DEFLATE'):
        with gdaltest.config_option('INTERLEAVE_OVERVIEW', 'PIXEL'):
            ds = gdal.Open('/vsimem/tiff_ovr_52.tif')
            ds.BuildOverviews('NEAR', [4])
            ds = None
            ds = gdal.Open('/vsimem/tiff_ovr_52.tif')
            ds.BuildOverviews('NEAR', [2])
            ds = None

    ds = gdal.Open('/vsimem/tiff_ovr_52.tif')
    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    assert cs == 328
    cs = ds.GetRasterBand(1).GetOverview(1).Checksum()
    assert cs == 1087
    ds = None

    gdal.GetDriverByName('GTiff').Delete('/vsimem/tiff_ovr_52.tif')

    gdal.GetDriverByName('GTiff').CreateCopy('/vsimem/tiff_ovr_52.tif', src_ds)
    with gdaltest.config_option('COMPRESS_OVERVIEW', 'DEFLATE'):
        with gdaltest.config_option('INTERLEAVE_OVERVIEW', 'PIXEL'):
            ds = gdal.Open('/vsimem/tiff_ovr_52.tif')
            ds.BuildOverviews('NEAR', [4, 2])
            ds = None

    ds = gdal.Open('/vsimem/tiff_ovr_52.tif')
    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    assert cs == 328
    cs = ds.GetRasterBand(1).GetOverview(1).Checksum()
    assert cs == 1087
    ds = None

    gdal.GetDriverByName('GTiff').Delete('/vsimem/tiff_ovr_52.tif')

###############################################################################
# Test external overviews building in several steps


def test_tiff_ovr_53():

    src_ds = gdal.Open('data/byte.tif')
    if src_ds is None:
        pytest.skip()

    gdal.GetDriverByName('GTiff').CreateCopy('/vsimem/tiff_ovr_53.tif', src_ds)
    ds = gdal.Open('/vsimem/tiff_ovr_53.tif')
    ds.BuildOverviews('NEAR', [2])
    ds = None
    # Note: currently this will compute it from the base raster and not
    # ov_factor=2 !
    ds = gdal.Open('/vsimem/tiff_ovr_53.tif')
    ds.BuildOverviews('NEAR', [4])
    ds = None

    ds = gdal.Open('/vsimem/tiff_ovr_53.tif')
    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    assert cs == 1087
    cs = ds.GetRasterBand(1).GetOverview(1).Checksum()
    assert cs == 328
    ds = None

    gdal.GetDriverByName('GTiff').Delete('/vsimem/tiff_ovr_53.tif')

    # Compressed code path
    gdal.GetDriverByName('GTiff').CreateCopy('/vsimem/tiff_ovr_53.tif', src_ds)
    with gdaltest.config_option('COMPRESS_OVERVIEW', 'DEFLATE'):
        with gdaltest.config_option('INTERLEAVE_OVERVIEW', 'PIXEL'):
            ds = gdal.Open('/vsimem/tiff_ovr_53.tif')
            ds.BuildOverviews('NEAR', [2])
            ds = None
            # Note: currently this will compute it from the base raster and not
            # ov_factor=2 !
            ds = gdal.Open('/vsimem/tiff_ovr_53.tif')
            ds.BuildOverviews('NEAR', [4])
            ds = None

    ds = gdal.Open('/vsimem/tiff_ovr_53.tif')
    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    assert cs == 1087
    cs = ds.GetRasterBand(1).GetOverview(1).Checksum()
    assert cs == 328
    ds = None

    gdal.GetDriverByName('GTiff').Delete('/vsimem/tiff_ovr_53.tif')

###############################################################################
# Test external overviews building in several steps with jpeg compression


def test_tiff_ovr_54():

    drv = gdal.GetDriverByName('GTiff')
    md = drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('JPEG') == -1:
        pytest.skip()

    src_ds = gdal.Open('../gdrivers/data/small_world.tif')
    gdal.GetDriverByName('GTiff').CreateCopy('/vsimem/tiff_ovr_54.tif', src_ds)

    with gdaltest.config_options({'COMPRESS_OVERVIEW': 'JPEG',
                                  'PHOTOMETRIC_OVERVIEW': 'YCBCR',
                                  'INTERLEAVE_OVERVIEW': 'PIXEL'}):
        ds = gdal.Open('/vsimem/tiff_ovr_54.tif')
        ds.BuildOverviews('AVERAGE', [2])
        ds = None
        ds = gdal.Open('/vsimem/tiff_ovr_54.tif')
        ds.BuildOverviews('AVERAGE', [4])
        ds = None

    ds = gdal.Open('/vsimem/tiff_ovr_54.tif')
    cs0 = ds.GetRasterBand(1).GetOverview(0).Checksum()
    cs1 = ds.GetRasterBand(1).GetOverview(1).Checksum()
    ds = None

    gdal.GetDriverByName('GTiff').Delete('/vsimem/tiff_ovr_54.tif')

    assert not (cs0 == 0 or cs1 == 0)

###############################################################################
# Test average overview generation with nodata.

def test_tiff_ovr_55(both_endian):

    src_ds = gdal.Open('../gdrivers/data/int16.tif')
    gdal.GetDriverByName('GTiff').CreateCopy('/vsimem/tiff_ovr_55.tif', src_ds)

    wrk_ds = gdal.Open('/vsimem/tiff_ovr_55.tif')
    assert wrk_ds is not None, 'Failed to open test dataset.'

    wrk_ds.BuildOverviews('RMS', overviewlist=[2])
    wrk_ds = None

    wrk_ds = gdal.Open('/vsimem/tiff_ovr_55.tif')
    cs = wrk_ds.GetRasterBand(1).GetOverview(0).Checksum()
    exp_cs = 1172

    assert cs == exp_cs, 'got wrong overview checksum.'


###############################################################################


def test_tiff_ovr_too_many_levels_contig():

    src_ds = gdal.Open('data/byte.tif')
    tmpfilename = '/vsimem/tiff_ovr_too_many_levels_contig.tif'
    ds = gdal.GetDriverByName('GTiff').CreateCopy(tmpfilename, src_ds)
    ds.BuildOverviews('AVERAGE', [2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096])
    assert ds.GetRasterBand(1).GetOverviewCount() == 5
    ds.BuildOverviews('AVERAGE', [2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096])
    assert ds.GetRasterBand(1).GetOverviewCount() == 5
    ds = None
    gdal.GetDriverByName('GTiff').Delete(tmpfilename)

###############################################################################


def test_tiff_ovr_too_many_levels_separate():

    src_ds = gdal.Open('data/separate_tiled.tif')
    tmpfilename = '/vsimem/tiff_ovr_too_many_levels_separate.tif'
    ds = gdal.GetDriverByName('GTiff').CreateCopy(tmpfilename, src_ds)
    ds.BuildOverviews('AVERAGE', [2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096])
    assert ds.GetRasterBand(1).GetOverviewCount() == 6
    assert ds.GetRasterBand(1).GetOverviewCount() == 6
    ds = None
    gdal.GetDriverByName('GTiff').Delete(tmpfilename)

###############################################################################


def test_tiff_ovr_too_many_levels_external():

    src_ds = gdal.Open('data/byte.tif')
    tmpfilename = '/vsimem/tiff_ovr_too_many_levels_contig.tif'
    gdal.GetDriverByName('GTiff').CreateCopy(tmpfilename, src_ds)
    ds = gdal.Open(tmpfilename)
    ds.BuildOverviews('AVERAGE', [2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096])
    assert ds.GetRasterBand(1).GetOverviewCount() == 5
    ds.BuildOverviews('AVERAGE', [2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096])
    assert ds.GetRasterBand(1).GetOverviewCount() == 5
    ds = None
    gdal.GetDriverByName('GTiff').Delete(tmpfilename)

###############################################################################


def test_tiff_ovr_average_multiband_vs_singleband():

    gdal.Translate('/vsimem/tiff_ovr_average_multiband_band.tif', 'data/reproduce_average_issue.tif', creationOptions=['INTERLEAVE=BAND'])
    gdal.Translate('/vsimem/tiff_ovr_average_multiband_pixel.tif', 'data/reproduce_average_issue.tif', creationOptions=['INTERLEAVE=PIXEL'])

    ds = gdal.Open('/vsimem/tiff_ovr_average_multiband_band.tif', gdal.GA_Update)
    ds.BuildOverviews('AVERAGE', [2])
    cs_band = [ds.GetRasterBand(i+1).GetOverview(0).Checksum() for i in range(3)]
    ds = None

    ds = gdal.Open('/vsimem/tiff_ovr_average_multiband_pixel.tif', gdal.GA_Update)
    ds.BuildOverviews('AVERAGE', [2])
    cs_pixel = [ds.GetRasterBand(i+1).GetOverview(0).Checksum() for i in range(3)]
    ds = None

    gdal.GetDriverByName('GTiff').Delete('/vsimem/tiff_ovr_average_multiband_band.tif')
    gdal.GetDriverByName('GTiff').Delete('/vsimem/tiff_ovr_average_multiband_pixel.tif')

    assert cs_band == cs_pixel

###############################################################################


def test_tiff_ovr_multithreading_multiband():

    # Test multithreading through GDALRegenerateOverviewsMultiBand
    ds = gdal.Translate('/vsimem/test.tif', 'data/stefan_full_rgba.tif',
                        creationOptions=['COMPRESS=LZW', 'TILED=YES',
                                         'BLOCKXSIZE=16', 'BLOCKYSIZE=16'])
    with gdaltest.config_options({'GDAL_NUM_THREADS': '8',
                                  'GDAL_OVR_CHUNK_MAX_SIZE': '100'}):
        ds.BuildOverviews('AVERAGE', [2])
    ds = None
    ds = gdal.Open('/vsimem/test.tif')
    assert [ds.GetRasterBand(i+1).Checksum() for i in range(4)] == [12603, 58561, 36064, 10807]
    ds = None
    gdal.Unlink('/vsimem/test.tif')

###############################################################################


def test_tiff_ovr_multithreading_singleband():

    # Test multithreading through GDALRegenerateOverviews
    ds = gdal.Translate('/vsimem/test.tif', 'data/stefan_full_rgba.tif',
                        creationOptions=['INTERLEAVE=BAND'])
    with gdaltest.config_options({'GDAL_NUM_THREADS': '8',
                                  'GDAL_OVR_CHUNKYSIZE': '1'}):
        ds.BuildOverviews('AVERAGE', [2, 4])
    ds = None
    ds = gdal.Open('/vsimem/test.tif')
    assert [ds.GetRasterBand(i+1).Checksum() for i in range(4)] == [12603, 58561, 36064, 10807]
    ds = None
    gdal.Unlink('/vsimem/test.tif')

###############################################################################


def test_tiff_ovr_multiband_code_path_degenerate():

    temp_path = '/vsimem/test.tif'
    ds = gdal.GetDriverByName('GTiff').Create(temp_path, 5, 6)
    ds.GetRasterBand(1).Fill(255)
    del ds
    ds = gdal.OpenEx(temp_path, gdal.GA_ReadOnly)
    with gdaltest.config_option('COMPRESS_OVERVIEW', 'LZW'):
        ds.BuildOverviews('nearest', overviewlist=[2, 4, 8])
    assert ds.GetRasterBand(1).GetOverview(0).Checksum() != 0
    assert ds.GetRasterBand(1).GetOverview(1).Checksum() != 0
    assert ds.GetRasterBand(1).GetOverview(2).Checksum() != 0
    del ds
    gdal.GetDriverByName('GTiff').Delete(temp_path)

###############################################################################


def test_tiff_ovr_color_table_bug_3336():

    temp_path = '/vsimem/test.tif'
    ds = gdal.GetDriverByName('GTiff').Create(temp_path, 242, 10442)
    ct = gdal.ColorTable()
    ct.SetColorEntry(255, (255,2552,55))
    ds.GetRasterBand(1).SetRasterColorTable(ct)
    del ds
    ds = gdal.OpenEx(temp_path, gdal.GA_ReadOnly)
    assert ds.BuildOverviews('nearest', overviewlist=[32]) == 0
    del ds
    gdal.GetDriverByName('GTiff').Delete(temp_path)

###############################################################################


def test_tiff_ovr_color_table_bug_3336_bis():

    temp_path = '/vsimem/test.tif'
    ds = gdal.GetDriverByName('GTiff').Create(temp_path, 128, 12531)
    ct = gdal.ColorTable()
    ct.SetColorEntry(255, (255,2552,55))
    ds.GetRasterBand(1).SetRasterColorTable(ct)
    del ds
    ds = gdal.OpenEx(temp_path, gdal.GA_ReadOnly)
    assert ds.BuildOverviews('nearest', overviewlist=[128]) == 0
    del ds
    gdal.GetDriverByName('GTiff').Delete(temp_path)


###############################################################################
# Cleanup

def test_tiff_ovr_cleanup():
    gdaltest.tiff_drv.Delete('tmp/mfloat32.tif')
    gdaltest.tiff_drv.Delete('tmp/ovr4.tif')
    gdaltest.tiff_drv.Delete('tmp/ovr5.tif')
    gdaltest.tiff_drv.Delete('tmp/ovr6.tif')
    gdaltest.tiff_drv.Delete('tmp/test_average_palette.tif')
    gdaltest.tiff_drv.Delete('tmp/ovr9.tif')
    gdaltest.tiff_drv.Delete('tmp/ovr10.tif')
    gdaltest.tiff_drv.Delete('tmp/ovr11.tif')
    gdaltest.tiff_drv.Delete('tmp/ovr12.tif')
    gdaltest.tiff_drv.Delete('tmp/test_gauss_palette.tif')
    gdaltest.tiff_drv.Delete('tmp/ovr15.tif')
    gdaltest.tiff_drv.Delete('tmp/ovr16.tif')
    gdaltest.tiff_drv.Delete('tmp/ovr17.tif')
    gdaltest.tiff_drv.Delete('tmp/ovr18.tif')
    gdaltest.tiff_drv.Delete('tmp/ovr19.tif')
    gdaltest.tiff_drv.Delete('tmp/ovr20.tif')
    gdaltest.tiff_drv.Delete('tmp/ovr21.tif')
    gdaltest.tiff_drv.Delete('tmp/ovr22.tif')
    gdaltest.tiff_drv.Delete('tmp/ovr23.tif')
    gdaltest.tiff_drv.Delete('tmp/ovr24.tif')
    gdaltest.tiff_drv.Delete('tmp/ovr25.tif')
    gdaltest.tiff_drv.Delete('tmp/ovr26.tif')
    gdaltest.tiff_drv.Delete('tmp/ovr27.tif')
    gdaltest.tiff_drv.Delete('tmp/ovr30.tif')
    gdaltest.tiff_drv.Delete('tmp/ovr31.tif')
    gdaltest.tiff_drv.Delete('tmp/ovr37.dt0')
    gdaltest.tiff_drv.Delete('tmp/ovr38.tif')
    gdaltest.tiff_drv.Delete('tmp/ovr39.tif')
    gdaltest.tiff_drv.Delete('tmp/ovr40.tif')
    gdaltest.tiff_drv.Delete('tmp/ovr41.tif')
    gdaltest.tiff_drv.Delete('tmp/ovr42.tif')
    gdaltest.tiff_drv.Delete('tmp/rgba_with_alpha_0_and_255.tif')
