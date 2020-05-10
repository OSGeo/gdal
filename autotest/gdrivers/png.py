#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read/write functionality for PNG driver.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2008-2012, Even Rouault <even dot rouault at spatialys.com>
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
from osgeo import gdal


import gdaltest
import pytest

###############################################################################
# Read test of simple byte reference data.


def test_png_1():

    tst = gdaltest.GDALTest('PNG', 'png/test.png', 1, 57921)
    return tst.testOpen()

###############################################################################
# Test lossless copying.


def test_png_2():

    tst = gdaltest.GDALTest('PNG', 'png/test.png', 1, 57921)

    return tst.testCreateCopy()

###############################################################################
# Verify the geotransform, colormap, and nodata setting for test file.


def test_png_3():

    ds = gdal.Open('data/png/test.png')
    cm = ds.GetRasterBand(1).GetRasterColorTable()
    assert cm.GetCount() == 16 and cm.GetColorEntry(0) == (255, 255, 255, 0) and cm.GetColorEntry(1) == (255, 255, 208, 255), \
        'Wrong colormap entries'

    cm = None

    assert int(ds.GetRasterBand(1).GetNoDataValue()) == 0, 'Wrong nodata value.'

    # This geotransform test is also verifying the fix for bug 1414, as
    # the world file is in a mixture of numeric representations for the
    # numbers.  (mixture of "," and "." in file)

    gt_expected = (700000.305, 0.38, 0.01, 4287500.695, -0.01, -0.38)

    gt = ds.GetGeoTransform()
    for i in range(6):
        if gt[i] != pytest.approx(gt_expected[i], abs=0.0001):
            print('expected:', gt_expected)
            print('got:', gt)
            pytest.fail('Mixed locale world file read improperly.')

    
###############################################################################
# Test RGB mode creation and reading.


def test_png_4():

    tst = gdaltest.GDALTest('PNG', 'rgbsmall.tif', 3, 21349)

    return tst.testCreateCopy()

###############################################################################
# Test RGBA 16bit read support.


def test_png_5():

    tst = gdaltest.GDALTest('PNG', 'png/rgba16.png', 3, 1815)
    return tst.testOpen()

###############################################################################
# Test RGBA 16bit mode creation and reading.


def test_png_6():

    tst = gdaltest.GDALTest('PNG', 'png/rgba16.png', 4, 4873)

    return tst.testCreateCopy()

###############################################################################
# Test RGB NODATA_VALUES metadata write (and read) support.
# This is handled via the tRNS block in PNG.


def test_png_7():

    drv = gdal.GetDriverByName('PNG')
    srcds = gdal.Open('data/png/tbbn2c16.png')

    dstds = drv.CreateCopy('tmp/png7.png', srcds)
    srcds = None

    dstds = gdal.Open('tmp/png7.png')
    md = dstds.GetMetadata()
    dstds = None

    assert md['NODATA_VALUES'] == '32639 32639 32639', 'NODATA_VALUES wrong'

    dstds = None

    drv.Delete('tmp/png7.png')

###############################################################################
# Test PNG file with broken IDAT chunk. This poor man test of clean
# recovery from errors caused by reading broken file..


def test_png_8():

    drv = gdal.GetDriverByName('PNG')
    ds_src = gdal.Open('data/png/idat_broken.png')

    md = ds_src.GetMetadata()
    assert not md, 'metadata list not expected'

    # Number of bands has been preserved
    assert ds_src.RasterCount == 4, 'wrong number of bands'

    # No reading is performed, so we expect valid reference
    b = ds_src.GetRasterBand(1)
    assert b is not None, 'band 1 is missing'

    # We're not interested in returned value but internal state of GDAL.
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    b.ComputeBandStats()
    err = gdal.GetLastErrorNo()
    gdal.PopErrorHandler()

    assert err != 0, 'error condition expected'

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds_dst = drv.CreateCopy('tmp/idat_broken.png', ds_src)
    err = gdal.GetLastErrorNo()
    gdal.PopErrorHandler()
    ds_src = None

    assert err != 0, 'error condition expected'

    assert ds_dst is None, 'dataset not expected'

    os.remove('tmp/idat_broken.png')


###############################################################################
# Test creating an in memory copy.

def test_png_9():

    tst = gdaltest.GDALTest('PNG', 'byte.tif', 1, 4672)

    return tst.testCreateCopy(vsimem=1)

###############################################################################
# Test writing to /vsistdout/


def test_png_10():

    src_ds = gdal.Open('data/byte.tif')
    ds = gdal.GetDriverByName('PNG').CreateCopy('/vsistdout_redirect//vsimem/tmp.png', src_ds)
    assert ds.GetRasterBand(1).Checksum() == 0
    src_ds = None
    ds = None

    ds = gdal.Open('/vsimem/tmp.png')
    assert ds is not None
    assert ds.GetRasterBand(1).Checksum() == 4672

    gdal.Unlink('/vsimem/tmp.png')

###############################################################################
# Test CreateCopy() interruption


def test_png_11():

    tst = gdaltest.GDALTest('PNG', 'byte.tif', 1, 4672)

    ret = tst.testCreateCopy(vsimem=1, interrupt_during_copy=True)
    gdal.Unlink('/vsimem/byte.tif.tst')
    return ret

###############################################################################
# Test optimized IRasterIO


def test_png_12():
    ds = gdal.Open('../gcore/data/stefan_full_rgba.png')
    cs = [ds.GetRasterBand(i + 1).Checksum() for i in range(ds.RasterCount)]

    # Band interleaved
    data = ds.ReadRaster(0, 0, ds.RasterXSize, ds.RasterYSize)
    tmp_ds = gdal.GetDriverByName('Mem').Create('', ds.RasterXSize, ds.RasterYSize, ds.RasterCount)
    tmp_ds.WriteRaster(0, 0, ds.RasterXSize, ds.RasterYSize, data)
    got_cs = [tmp_ds.GetRasterBand(i + 1).Checksum() for i in range(ds.RasterCount)]
    assert cs == got_cs

    # Pixel interleaved
    data = ds.ReadRaster(0, 0, ds.RasterXSize, ds.RasterYSize, buf_pixel_space=ds.RasterCount, buf_band_space=1)
    tmp_ds = gdal.GetDriverByName('Mem').Create('', ds.RasterXSize, ds.RasterYSize, ds.RasterCount)
    tmp_ds.WriteRaster(0, 0, ds.RasterXSize, ds.RasterYSize, data, buf_pixel_space=ds.RasterCount, buf_band_space=1)
    got_cs = [tmp_ds.GetRasterBand(i + 1).Checksum() for i in range(ds.RasterCount)]
    assert cs == got_cs

    # Pixel interleaved with padding
    data = ds.ReadRaster(0, 0, ds.RasterXSize, ds.RasterYSize, buf_pixel_space=5, buf_band_space=1)
    tmp_ds = gdal.GetDriverByName('Mem').Create('', ds.RasterXSize, ds.RasterYSize, ds.RasterCount)
    tmp_ds.WriteRaster(0, 0, ds.RasterXSize, ds.RasterYSize, data, buf_pixel_space=5, buf_band_space=1)
    got_cs = [tmp_ds.GetRasterBand(i + 1).Checksum() for i in range(ds.RasterCount)]
    assert cs == got_cs

###############################################################################
# Test metadata


def test_png_13():

    src_ds = gdal.GetDriverByName('MEM').Create('', 1, 1)
    src_ds.SetMetadataItem('foo', 'bar')
    src_ds.SetMetadataItem('COPYRIGHT', 'copyright value')
    src_ds.SetMetadataItem('DESCRIPTION', 'will be overridden by creation option')
    out_ds = gdal.GetDriverByName('PNG').CreateCopy('/vsimem/tmp.png', src_ds, options=['WRITE_METADATA_AS_TEXT=YES', 'DESCRIPTION=my desc'])
    md = out_ds.GetMetadata()
    assert len(md) == 3 and md['foo'] == 'bar' and md['Copyright'] == 'copyright value' and md['Description'] == 'my desc'
    out_ds = None
    # check that no PAM file is created
    assert gdal.VSIStatL('/vsimem/tmp.png.aux.xml') != 0
    gdal.Unlink('/vsimem/tmp.png')

###############################################################################
# Test support for nbits < 8


def test_png_14():

    src_ds = gdal.Open('../gcore/data/oddsize1bit.tif')
    expected_cs = src_ds.GetRasterBand(1).Checksum()
    gdal.GetDriverByName('PNG').CreateCopy('/vsimem/tmp.png', src_ds)
    out_ds = gdal.Open('/vsimem/tmp.png')
    cs = out_ds.GetRasterBand(1).Checksum()
    nbits = out_ds.GetRasterBand(1).GetMetadataItem('NBITS', 'IMAGE_STRUCTURE')
    gdal.Unlink('/vsimem/tmp.png')

    assert cs == expected_cs

    assert nbits == '1'

    # check that no PAM file is created
    assert gdal.VSIStatL('/vsimem/tmp.png.aux.xml') != 0

    # Test explicit NBITS
    gdal.GetDriverByName('PNG').CreateCopy('/vsimem/tmp.png', src_ds, options=['NBITS=2'])
    out_ds = gdal.Open('/vsimem/tmp.png')
    nbits = out_ds.GetRasterBand(1).GetMetadataItem('NBITS', 'IMAGE_STRUCTURE')
    gdal.Unlink('/vsimem/tmp.png')
    assert nbits == '2'

    # Test (wrong) explicit NBITS
    with gdaltest.error_handler():
        gdal.GetDriverByName('PNG').CreateCopy('/vsimem/tmp.png', src_ds, options=['NBITS=7'])
    out_ds = gdal.Open('/vsimem/tmp.png')
    nbits = out_ds.GetRasterBand(1).GetMetadataItem('NBITS', 'IMAGE_STRUCTURE')
    gdal.Unlink('/vsimem/tmp.png')
    assert nbits is None



