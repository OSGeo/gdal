#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test the image reprojection functions. Try to test as many
#           resamplers as possible (we have optimized resamplers for some
#           data types, test them too).
# Author:   Andrey Kiselev, dron16@ak4719.spb.edu
#
###############################################################################
# Copyright (c) 2008, Andrey Kiselev <dron16@ak4719.spb.edu>
# Copyright (c) 2008-2014, Even Rouault <even dot rouault at spatialys.com>
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
import sys

from osgeo import gdal, osr

import gdaltest
import pytest
import struct

###############################################################################
# Verify that we always getting the same image when warping.
# Warp the image using the VRT file and compare result with reference image

# Upsampling


def test_warp_1():

    gdaltest.tiff_drv = gdal.GetDriverByName('GTiff')
    if gdaltest.tiff_drv is None:
        pytest.skip()

    ds = gdal.Open('data/utmsmall_near.vrt')
    ref_ds = gdal.Open('data/utmsmall_near.tiff')
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None

    assert maxdiff <= 1, 'Image too different from reference'


def test_warp_1_short():

    gdaltest.tiff_drv = gdal.GetDriverByName('GTiff')
    if gdaltest.tiff_drv is None:
        pytest.skip()

    ds = gdal.Open('data/utmsmall_near_short.vrt')
    ref_ds = gdal.Open('data/utmsmall_near.tiff')
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None

    assert maxdiff <= 1, 'Image too different from reference'


def test_warp_1_ushort():

    gdaltest.tiff_drv = gdal.GetDriverByName('GTiff')
    if gdaltest.tiff_drv is None:
        pytest.skip()

    ds = gdal.Open('data/utmsmall_near_ushort.vrt')
    ref_ds = gdal.Open('data/utmsmall_near.tiff')
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None

    assert maxdiff <= 1, 'Image too different from reference'


def test_warp_1_float():

    gdaltest.tiff_drv = gdal.GetDriverByName('GTiff')
    if gdaltest.tiff_drv is None:
        pytest.skip()

    ds = gdal.Open('data/utmsmall_near_float.vrt')
    ref_ds = gdal.Open('data/utmsmall_near.tiff')
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None

    assert maxdiff <= 1, 'Image too different from reference'


def test_warp_2():

    gdaltest.tiff_drv = gdal.GetDriverByName('GTiff')
    if gdaltest.tiff_drv is None:
        pytest.skip()

    ds = gdal.Open('data/utmsmall_blinear.vrt')
    ref_ds = gdal.Open('data/utmsmall_blinear.tiff')
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None

    assert maxdiff <= 1, 'Image too different from reference'


def test_warp_2_short():

    gdaltest.tiff_drv = gdal.GetDriverByName('GTiff')
    if gdaltest.tiff_drv is None:
        pytest.skip()

    ds = gdal.Open('data/utmsmall_blinear_short.vrt')
    ref_ds = gdal.Open('data/utmsmall_blinear.tiff')
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None

    assert maxdiff <= 1, 'Image too different from reference'


def test_warp_2_ushort():

    gdaltest.tiff_drv = gdal.GetDriverByName('GTiff')
    if gdaltest.tiff_drv is None:
        pytest.skip()

    ds = gdal.Open('data/utmsmall_blinear_ushort.vrt')
    ref_ds = gdal.Open('data/utmsmall_blinear.tiff')
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None

    assert maxdiff <= 1, 'Image too different from reference'


def test_warp_2_downsize():

    gdaltest.tiff_drv = gdal.GetDriverByName('GTiff')
    if gdaltest.tiff_drv is None:
        pytest.skip()

    ds = gdal.Open('data/utmsmall_bilinear_2.vrt')
    ref_ds = gdal.Open('data/utmsmall_bilinear_2.tif')
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None

    assert maxdiff <= 1, 'Image too different from reference'


def test_warp_3():

    gdaltest.tiff_drv = gdal.GetDriverByName('GTiff')
    if gdaltest.tiff_drv is None:
        pytest.skip()

    ds = gdal.Open('data/utmsmall_cubic.vrt')
    ref_ds = gdal.Open('data/utmsmall_cubic.tiff')
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None

    assert maxdiff <= 1, 'Image too different from reference'


def test_warp_3_short():

    gdaltest.tiff_drv = gdal.GetDriverByName('GTiff')
    if gdaltest.tiff_drv is None:
        pytest.skip()

    ds = gdal.Open('data/utmsmall_cubic_short.vrt')
    ref_ds = gdal.Open('data/utmsmall_cubic.tiff')
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None

    assert maxdiff <= 1, 'Image too different from reference'


def test_warp_3_ushort():

    gdaltest.tiff_drv = gdal.GetDriverByName('GTiff')
    if gdaltest.tiff_drv is None:
        pytest.skip()

    ds = gdal.Open('data/utmsmall_cubic_ushort.vrt')
    ref_ds = gdal.Open('data/utmsmall_cubic.tiff')
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None

    assert maxdiff <= 1, 'Image too different from reference'


def test_warp_3_downsize():

    gdaltest.tiff_drv = gdal.GetDriverByName('GTiff')
    if gdaltest.tiff_drv is None:
        pytest.skip()

    ds = gdal.Open('data/utmsmall_cubic_2.vrt')
    ref_ds = gdal.Open('data/utmsmall_cubic_2.tif')
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None

    assert maxdiff <= 1, 'Image too different from reference'


def test_warp_3_float_downsize():

    gdaltest.tiff_drv = gdal.GetDriverByName('GTiff')
    if gdaltest.tiff_drv is None:
        pytest.skip()

    ds = gdal.Open('data/utmsmall_cubic_2_float.vrt')
    ref_ds = gdal.Open('data/utmsmall_cubic_2.tif')
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None

    assert maxdiff <= 1, 'Image too different from reference'


def test_warp_4():

    gdaltest.tiff_drv = gdal.GetDriverByName('GTiff')
    if gdaltest.tiff_drv is None:
        pytest.skip()

    ds = gdal.Open('data/utmsmall_cubicspline.vrt')
    ref_ds = gdal.Open('data/utmsmall_cubicspline.tiff')
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None

    assert maxdiff <= 1, 'Image too different from reference'


def test_warp_4_short():

    gdaltest.tiff_drv = gdal.GetDriverByName('GTiff')
    if gdaltest.tiff_drv is None:
        pytest.skip()

    ds = gdal.Open('data/utmsmall_cubicspline_short.vrt')
    ref_ds = gdal.Open('data/utmsmall_cubicspline.tiff')
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None

    assert maxdiff <= 1, 'Image too different from reference'


def test_warp_4_ushort():

    gdaltest.tiff_drv = gdal.GetDriverByName('GTiff')
    if gdaltest.tiff_drv is None:
        pytest.skip()

    ds = gdal.Open('data/utmsmall_cubicspline_ushort.vrt')
    ref_ds = gdal.Open('data/utmsmall_cubicspline.tiff')
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None

    assert maxdiff <= 1, 'Image too different from reference'


def test_warp_4_downsize():

    gdaltest.tiff_drv = gdal.GetDriverByName('GTiff')
    if gdaltest.tiff_drv is None:
        pytest.skip()

    ds = gdal.Open('data/utmsmall_cubicspline_2.vrt')
    ref_ds = gdal.Open('data/utmsmall_cubicspline_2.tif')
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None

    assert maxdiff <= 1, 'Image too different from reference'


def test_warp_4_short_downsize():

    gdaltest.tiff_drv = gdal.GetDriverByName('GTiff')
    if gdaltest.tiff_drv is None:
        pytest.skip()

    ds = gdal.Open('data/utmsmall_cubicspline_wt_short.vrt')
    ref_ds = gdal.Open('data/utmsmall_cubicspline_2.tif')
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None

    assert maxdiff <= 1, 'Image too different from reference'


def test_warp_4_float_downsize():

    gdaltest.tiff_drv = gdal.GetDriverByName('GTiff')
    if gdaltest.tiff_drv is None:
        pytest.skip()

    ds = gdal.Open('data/utmsmall_cubicspline_wt_float32.vrt')
    ref_ds = gdal.Open('data/utmsmall_cubicspline_2.tif')
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None

    assert maxdiff <= 1, 'Image too different from reference'


def test_warp_5():

    gdaltest.tiff_drv = gdal.GetDriverByName('GTiff')
    if gdaltest.tiff_drv is None:
        pytest.skip()

    ds = gdal.Open('data/utmsmall_lanczos.vrt')
    ref_ds = gdal.Open('data/utmsmall_lanczos.tiff')
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None

    assert maxdiff <= 1, 'Image too different from reference'


def test_warp_5_downsize():

    gdaltest.tiff_drv = gdal.GetDriverByName('GTiff')
    if gdaltest.tiff_drv is None:
        pytest.skip()

    ds = gdal.Open('data/utmsmall_lanczos_2.vrt')
    ref_ds = gdal.Open('data/utmsmall_lanczos_2.tif')
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None

    assert maxdiff <= 1, 'Image too different from reference'

# Downsampling


def test_warp_6():

    gdaltest.tiff_drv = gdal.GetDriverByName('GTiff')
    if gdaltest.tiff_drv is None:
        pytest.skip()

    tst = gdaltest.GDALTest('VRT', 'utmsmall_ds_near.vrt', 1, 4770)

    return tst.testOpen()


def test_warp_7():

    gdaltest.tiff_drv = gdal.GetDriverByName('GTiff')
    if gdaltest.tiff_drv is None:
        pytest.skip()

    tst = gdaltest.GDALTest('VRT', 'utmsmall_ds_blinear.vrt', 1, 4755)

    return tst.testOpen()


def test_warp_8():

    gdaltest.tiff_drv = gdal.GetDriverByName('GTiff')
    if gdaltest.tiff_drv is None:
        pytest.skip()

    tst = gdaltest.GDALTest('VRT', 'utmsmall_ds_cubic.vrt', 1, 4833)

    return tst.testOpen()


def test_warp_9():

    gdaltest.tiff_drv = gdal.GetDriverByName('GTiff')
    if gdaltest.tiff_drv is None:
        pytest.skip()

    ds = gdal.Open('data/utmsmall_ds_cubicspline.vrt')
    ref_ds = gdal.Open('data/utmsmall_ds_cubicspline.tiff')
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None

    assert maxdiff <= 1, 'Image too different from reference'


def test_warp_10():

    gdaltest.tiff_drv = gdal.GetDriverByName('GTiff')
    if gdaltest.tiff_drv is None:
        pytest.skip()

    ds = gdal.Open('data/utmsmall_ds_lanczos.vrt')
    ref_ds = gdal.Open('data/utmsmall_ds_lanczos.tiff')
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None

    assert maxdiff <= 1, 'Image too different from reference'


def test_warp_11():

    gdaltest.tiff_drv = gdal.GetDriverByName('GTiff')
    if gdaltest.tiff_drv is None:
        pytest.skip()

    tst = gdaltest.GDALTest('VRT', 'rgbsmall_dstalpha.vrt', 4, 30658)

    return tst.testOpen()

# Test warping an empty RGBA with bilinear resampling


def test_warp_12():

    gdaltest.tiff_drv = gdal.GetDriverByName('GTiff')
    if gdaltest.tiff_drv is None:
        pytest.skip()

    ds = gdaltest.tiff_drv.Create('tmp/empty.tif', 20, 20, 4)
    ds.GetRasterBand(1).Fill(0)
    ds.GetRasterBand(2).Fill(0)
    ds.GetRasterBand(3).Fill(0)
    ds.GetRasterBand(4).Fill(0)
    ds = None

    # The alpha channel must be empty
    tst = gdaltest.GDALTest('VRT', 'empty_rb.vrt', 4, 0)

    ret = tst.testOpen()

    gdaltest.tiff_drv.Delete('tmp/empty.tif')

    return ret

# Test warping an empty RGBA with cubic resampling


def test_warp_13():

    gdaltest.tiff_drv = gdal.GetDriverByName('GTiff')
    if gdaltest.tiff_drv is None:
        pytest.skip()

    ds = gdaltest.tiff_drv.Create('tmp/empty.tif', 20, 20, 4)
    ds.GetRasterBand(1).Fill(0)
    ds.GetRasterBand(2).Fill(0)
    ds.GetRasterBand(3).Fill(0)
    ds.GetRasterBand(4).Fill(0)
    ds = None

    # The alpha channel must be empty
    tst = gdaltest.GDALTest('VRT', 'empty_rc.vrt', 4, 0)

    ret = tst.testOpen()

    gdaltest.tiff_drv.Delete('tmp/empty.tif')

    return ret

# Test warping an empty RGBA with cubic spline resampling


def test_warp_14():

    gdaltest.tiff_drv = gdal.GetDriverByName('GTiff')
    if gdaltest.tiff_drv is None:
        pytest.skip()

    ds = gdaltest.tiff_drv.Create('tmp/empty.tif', 20, 20, 4)
    ds.GetRasterBand(1).Fill(0)
    ds.GetRasterBand(2).Fill(0)
    ds.GetRasterBand(3).Fill(0)
    ds.GetRasterBand(4).Fill(0)
    ds = None

    # The alpha channel must be empty
    tst = gdaltest.GDALTest('VRT', 'empty_rcs.vrt', 4, 0)

    ret = tst.testOpen()

    gdaltest.tiff_drv.Delete('tmp/empty.tif')

    return ret

# Test GWKNearestFloat with transparent source alpha band


def test_warp_15():

    gdaltest.tiff_drv = gdal.GetDriverByName('GTiff')
    if gdaltest.tiff_drv is None:
        pytest.skip()

    ds = gdaltest.tiff_drv.Create('tmp/test.tif', 20, 20, 4)
    ds.GetRasterBand(1).Fill(0)
    ds.GetRasterBand(2).Fill(0)
    ds.GetRasterBand(3).Fill(0)
    ds.GetRasterBand(4).Fill(0)
    ds = None

    # The alpha channel must be empty
    tst = gdaltest.GDALTest('VRT', 'test_nearest_float.vrt', 4, 0)

    ret = tst.testOpen()

    gdaltest.tiff_drv.Delete('tmp/test.tif')

    return ret

# Test GWKNearestFloat with opaque source alpha band


def test_warp_16():

    gdaltest.tiff_drv = gdal.GetDriverByName('GTiff')
    if gdaltest.tiff_drv is None:
        pytest.skip()

    ds = gdaltest.tiff_drv.Create('tmp/test.tif', 20, 20, 4)
    ds.GetRasterBand(1).Fill(255)
    ds.GetRasterBand(2).Fill(0)
    ds.GetRasterBand(3).Fill(0)
    ds.GetRasterBand(4).Fill(255)
    ds = None

    # The alpha channel must be empty
    tst = gdaltest.GDALTest('VRT', 'test_nearest_float.vrt', 4, 4921)

    ret = tst.testOpen()

    gdaltest.tiff_drv.Delete('tmp/test.tif')

    return ret

# Test GWKNearestShort with transparent source alpha band


def test_warp_17():

    gdaltest.tiff_drv = gdal.GetDriverByName('GTiff')
    if gdaltest.tiff_drv is None:
        pytest.skip()

    ds = gdaltest.tiff_drv.Create('tmp/test.tif', 20, 20, 4)
    ds.GetRasterBand(1).Fill(0)
    ds.GetRasterBand(2).Fill(0)
    ds.GetRasterBand(3).Fill(0)
    ds.GetRasterBand(4).Fill(0)
    ds = None

    # The alpha channel must be empty
    tst = gdaltest.GDALTest('VRT', 'test_nearest_short.vrt', 4, 0)

    ret = tst.testOpen()

    gdaltest.tiff_drv.Delete('tmp/test.tif')

    return ret

# Test GWKNearestShort with opaque source alpha band


def test_warp_18():

    gdaltest.tiff_drv = gdal.GetDriverByName('GTiff')
    if gdaltest.tiff_drv is None:
        pytest.skip()

    ds = gdaltest.tiff_drv.Create('tmp/test.tif', 20, 20, 4)
    ds.GetRasterBand(1).Fill(255)
    ds.GetRasterBand(2).Fill(0)
    ds.GetRasterBand(3).Fill(0)
    ds.GetRasterBand(4).Fill(255)
    ds = None

    # The alpha channel must be empty
    tst = gdaltest.GDALTest('VRT', 'test_nearest_short.vrt', 4, 4921)

    ret = tst.testOpen()

    gdaltest.tiff_drv.Delete('tmp/test.tif')

    return ret


def warp_19_internal(size, datatype, resampling_string):

    ds = gdaltest.tiff_drv.Create('tmp/test.tif', size, size, 1, datatype)
    ds.SetGeoTransform((10, 5, 0, 30, 0, -5))
    ds.SetProjection('GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433]]')
    ds.GetRasterBand(1).Fill(10.1, 20.1)
    ds = None

    gdal.Warp('tmp/testwarp.tif', 'tmp/test.tif', options='-r ' + resampling_string)

    ref_ds = gdal.Open('tmp/test.tif')
    ds = gdal.Open('tmp/testwarp.tif')
    checksum = ds.GetRasterBand(1).Checksum()
    checksum_ref = ref_ds.GetRasterBand(1).Checksum()
    ds = None
    ref_ds = None

    gdaltest.tiff_drv.Delete('tmp/testwarp.tif')

    assert checksum == checksum_ref

    gdaltest.tiff_drv.Delete('tmp/test.tif')


# Test all data types and resampling methods for very small images
# to test edge behaviour
def test_warp_19():

    gdaltest.tiff_drv = gdal.GetDriverByName('GTiff')
    if gdaltest.tiff_drv is None:
        pytest.skip()

    datatypes = [gdal.GDT_Byte,
                 gdal.GDT_Int16,
                 gdal.GDT_CInt16,
                 gdal.GDT_UInt16,
                 gdal.GDT_Int32,
                 gdal.GDT_CInt32,
                 gdal.GDT_UInt32,
                 gdal.GDT_Float32,
                 gdal.GDT_CFloat32,
                 gdal.GDT_Float64,
                 gdal.GDT_CFloat64]

    methods = ['near', 'bilinear', 'cubic', 'cubicspline', 'lanczos', 'average']

    sizes = [1, 2, 3, 7]

    for size in sizes:
        print('Testing size = %d ...' % size)
        for method in methods:
            for datatype in datatypes:
                warp_19_internal(size, datatype, method)


# Test fix for #2724 (initialization of destination area to nodata in warped VRT)
def test_warp_20():

    gdaltest.tiff_drv = gdal.GetDriverByName('GTiff')
    if gdaltest.tiff_drv is None:
        pytest.skip()

    tst = gdaltest.GDALTest('VRT', 'white_nodata.vrt', 1, 1705)

    return tst.testOpen()

###############################################################################
# Test overviews on warped VRT files


def test_warp_21():

    shutil.copy('data/utmsmall_near.vrt', 'tmp/warp_21.vrt')

    ds = gdal.Open('tmp/warp_21.vrt', gdal.GA_Update)
    ds.BuildOverviews('NEAR', overviewlist=[2])
    ds = None

    ds = gdal.Open('tmp/warp_21.vrt')
    if ds.GetRasterBand(1).GetOverviewCount() != 1:
        pytest.skip()

    ds.GetRasterBand(1).GetOverview(0).Checksum()

    ds = None

    os.remove('tmp/warp_21.vrt')

###############################################################################
# Test warping with datasets which are "bigger" than the wm parameter.
# Would have detected issue of #3458


def test_warp_22():

    # Generate source image with non uniform data
    w = 1001
    h = 1001
    ds = gdal.GetDriverByName('GTiff').Create("tmp/warp_22_src.tif", w, h, 1)
    ds.SetGeoTransform([2, 0.01, 0, 49, 0, -0.01])
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(4326)
    ds.SetProjection(sr.ExportToWkt())

    for j in range(h):
        line = ''
        for i in range(w):
            line = line + '%c' % int((i * i + h * j / (i + 1)) % 256)
        ds.GetRasterBand(1).WriteRaster(0, j, w, 1, line)

    expected_cs = ds.GetRasterBand(1).Checksum()
    ds = None

    ret = 'success'
    failures = ''

    # warp with various options
    for option1 in ['', '-wo OPTIMIZE_SIZE=TRUE']:
        for option2 in ['', '-co TILED=YES', '-co TILED=YES -co BLOCKXSIZE=16 -co BLOCKYSIZE=16']:
            option = option1 + ' ' + option2
            try:
                os.remove('tmp/warp_22_dst.tif')
            except OSError:
                pass
            # -wm should not be greater than 2 * w * h. Let's put it at its minimum value
            gdal.Warp('tmp/warp_22_dst.tif', 'tmp/warp_22_src.tif', options='-wm 100000 ' + option)
            ds = gdal.Open('tmp/warp_22_dst.tif')
            cs = ds.GetRasterBand(1).Checksum()
            if cs != expected_cs:
                if failures != '':
                    failures = failures + '\n'
                failures = failures + 'failed for %s. Checksum : got %d, expected %d' % (option, cs, expected_cs)
                ret = 'fail'
            ds = None

    if failures != '':
        gdaltest.post_reason(failures)

    os.remove('tmp/warp_22_src.tif')
    os.remove('tmp/warp_22_dst.tif')

    return ret

###############################################################################
# Test warping with datasets where some RasterIO() requests involve nBufXSize == 0 (#3582)


def test_warp_23():

    gcp1 = gdal.GCP()
    gcp1.GCPPixel = 3213
    gcp1.GCPLine = 2225
    gcp1.GCPX = -88.834495
    gcp1.GCPY = 29.979959

    gcp2 = gdal.GCP()
    gcp2.GCPPixel = 2804
    gcp2.GCPLine = 2236
    gcp2.GCPX = -88.836706
    gcp2.GCPY = 29.979516

    gcp3 = gdal.GCP()
    gcp3.GCPPixel = 3157
    gcp3.GCPLine = 4344
    gcp3.GCPX = -88.833389
    gcp3.GCPY = 29.969519

    gcp4 = gdal.GCP()
    gcp4.GCPPixel = 3768
    gcp4.GCPLine = 5247
    gcp4.GCPX = -88.830168
    gcp4.GCPY = 29.964958

    gcp5 = gdal.GCP()
    gcp5.GCPPixel = 2697
    gcp5.GCPLine = 9225
    gcp5.GCPX = -88.83516
    gcp5.GCPY = 29.945386

    gcp6 = gdal.GCP()
    gcp6.GCPPixel = 4087
    gcp6.GCPLine = 12360
    gcp6.GCPX = -88.827899
    gcp6.GCPY = 29.929807

    gcp7 = gdal.GCP()
    gcp7.GCPPixel = 4629
    gcp7.GCPLine = 11258
    gcp7.GCPX = -88.825102
    gcp7.GCPY = 29.93527

    gcp8 = gdal.GCP()
    gcp8.GCPPixel = 4480
    gcp8.GCPLine = 7602
    gcp8.GCPX = -88.826733
    gcp8.GCPY = 29.95304

    gcps = [gcp1, gcp2, gcp3, gcp4, gcp5, gcp6, gcp7, gcp8]
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(4326)

    ds = gdal.GetDriverByName('GTiff').Create('tmp/test3582.tif', 70, 170, 4, options=['SPARSE_OK=YES'])
    for i, gcp in enumerate(gcps):
        gcps[i].GCPPixel = gcp.GCPPixel / 10
        gcps[i].GCPLine = gcp.GCPLine / 10
    ds.SetGCPs(gcps, sr.ExportToWkt())
    ds = None

    ds = gdal.Warp('', 'tmp/test3582.tif', format='MEM')
    ret = 'success'
    if ds is None:
        gdaltest.post_reason('could not open output dataset')
        ret = 'fail'
    ds = None

    os.remove('tmp/test3582.tif')

    return ret

###############################################################################
# Test fix for #3658 (numerical imprecision with Ubuntu 8.10 GCC 4.4.3 -O2 leading to upper
# left pixel being not set in GWKBilinearResample() case)


def test_warp_24():

    ds_ref = gdal.Open('data/test3658.tif')
    cs_ref = ds_ref.GetRasterBand(1).Checksum()
    ds = gdal.Warp('', ds_ref, options='-of MEM -r bilinear')
    cs = ds.GetRasterBand(1).Checksum()

    assert cs == cs_ref, 'did not get expected checksum'

###############################################################################
# Test -refine_gcps (#4143)


def test_warp_25():

    ds = gdal.Open('data/refine_gcps.vrt')
    cs = ds.GetRasterBand(1).Checksum()
    ds = None

    assert cs == 4672, 'did not get expected checksum'

###############################################################################
# Test serializing and deserializing TPS transformer


def test_warp_26():

    gdal.Translate('tmp/warp_25_gcp.vrt', '../gcore/data/byte.tif',
                   options='-of VRT -gcp 0 0 0 20 -gcp 0 20 0  0 '
                   '-gcp 20 0 20 20 -gcp 20 20 20 0')
    gdal.Warp('tmp/warp_25_warp.vrt', 'tmp/warp_25_gcp.vrt',
              options='-of VRT -tps')

    ds = gdal.Open('tmp/warp_25_warp.vrt')
    cs = ds.GetRasterBand(1).Checksum()
    ds = None

    assert cs == 4672, 'did not get expected checksum'

    os.unlink('tmp/warp_25_gcp.vrt')
    os.unlink('tmp/warp_25_warp.vrt')

###############################################################################
# Pure Python reprojection example. Nothing particular, just make use of existing
# API.


def warp_27_progress_callback(pct, message, user_data):
    # pylint: disable=unused-argument
    return 1  # 1 to continue, 0 to stop


def test_warp_27():

    # Open source dataset
    src_ds = gdal.Open('../gcore/data/byte.tif')

    # Desfine target SRS
    dst_srs = osr.SpatialReference()
    dst_srs.ImportFromEPSG(4326)
    dst_wkt = dst_srs.ExportToWkt()

    error_threshold = 0.125  # error threshold --> use same value as in gdalwarp
    resampling = gdal.GRA_Bilinear

    # Call AutoCreateWarpedVRT() to fetch default values for target raster dimensions and geotransform
    tmp_ds = gdal.AutoCreateWarpedVRT(src_ds,
                                      None,  # src_wkt : left to default value --> will use the one from source \
                                      dst_wkt, \
                                      resampling, \
                                      error_threshold)
    dst_xsize = tmp_ds.RasterXSize
    dst_ysize = tmp_ds.RasterYSize
    dst_gt = tmp_ds.GetGeoTransform()
    tmp_ds = None

    # Now create the true target dataset
    dst_ds = gdal.GetDriverByName('GTiff').Create('tmp/warp_27.tif', dst_xsize, dst_ysize,
                                                  src_ds.RasterCount)
    dst_ds.SetProjection(dst_wkt)
    dst_ds.SetGeoTransform(dst_gt)

    # And run the reprojection

    cbk = warp_27_progress_callback
    cbk_user_data = None  # value for last parameter of above warp_27_progress_callback

    gdal.ReprojectImage(src_ds,
                        dst_ds,
                        None,  # src_wkt : left to default value --> will use the one from source \
                        None,  # dst_wkt : left to default value --> will use the one from destination \
                        resampling, \
                        0,  # WarpMemoryLimit : left to default value \
                        error_threshold,
                        cbk,  # Progress callback : could be left to None or unspecified for silent progress
                        cbk_user_data)  # Progress callback user data

    # Done !
    dst_ds = None

    # Check that we have the same result as produced by 'gdalwarp -rb -t_srs EPSG:4326 ../gcore/data/byte.tif tmp/warp_27.tif'
    ds = gdal.Open('tmp/warp_27.tif')
    cs = ds.GetRasterBand(1).Checksum()
    ds = None

    ds = gdal.Warp('tmp/warp_27_ref.tif', '../gcore/data/byte.tif', options='-rb -t_srs EPSG:4326')
    ref_cs = ds.GetRasterBand(1).Checksum()
    ds = None

    assert cs == ref_cs

    gdal.Unlink('tmp/warp_27.tif')
    gdal.Unlink('tmp/warp_27_ref.tif')

###############################################################################
# Test reading a VRT with a destination alpha band, but no explicit
# INIT_DEST setting


def test_warp_28():

    ds = gdal.Open('data/utm_alpha_noinit.vrt')
    cs1 = ds.GetRasterBand(1).Checksum()
    cs2 = ds.GetRasterBand(2).Checksum()
    assert not (cs1 == 0 or cs2 == 0), 'bad checksum'
    ds = None

###############################################################################
# Test multi-thread computations


def test_warp_29():

    ds = gdal.Open('data/white_nodata.vrt')
    cs_monothread = ds.GetRasterBand(1).Checksum()
    ds = None

    old_val = gdal.GetConfigOption('GDAL_NUM_THREADS')
    gdal.SetConfigOption('GDAL_NUM_THREADS', 'ALL_CPUS')
    gdal.SetConfigOption('WARP_THREAD_CHUNK_SIZE', '0')
    ds = gdal.Open('data/white_nodata.vrt')
    cs_multithread = ds.GetRasterBand(1).Checksum()
    ds = None
    gdal.SetConfigOption('GDAL_NUM_THREADS', old_val)
    gdal.SetConfigOption('WARP_THREAD_CHUNK_SIZE', None)

    assert cs_monothread == cs_multithread

    old_val = gdal.GetConfigOption('GDAL_NUM_THREADS')
    gdal.SetConfigOption('GDAL_NUM_THREADS', '2')
    gdal.SetConfigOption('WARP_THREAD_CHUNK_SIZE', '0')
    ds = gdal.Open('data/white_nodata.vrt')
    cs_multithread = ds.GetRasterBand(1).Checksum()
    ds = None
    gdal.SetConfigOption('GDAL_NUM_THREADS', old_val)
    gdal.SetConfigOption('WARP_THREAD_CHUNK_SIZE', None)

    assert cs_monothread == cs_multithread

    src_ds = gdal.Open('../gcore/data/byte.tif')

    ds = gdal.Open('data/byte_gcp.vrt')
    old_val = gdal.GetConfigOption('GDAL_NUM_THREADS')
    gdal.SetConfigOption('GDAL_NUM_THREADS', '2')
    gdal.SetConfigOption('WARP_THREAD_CHUNK_SIZE', '0')
    got_cs = ds.GetRasterBand(1).Checksum()
    gdal.SetConfigOption('GDAL_NUM_THREADS', old_val)
    gdal.SetConfigOption('WARP_THREAD_CHUNK_SIZE', None)
    ds = None

    assert got_cs == src_ds.GetRasterBand(1).Checksum()

    ds = gdal.Open('data/byte_tps.vrt')
    old_val = gdal.GetConfigOption('GDAL_NUM_THREADS')
    gdal.SetConfigOption('GDAL_NUM_THREADS', '2')
    gdal.SetConfigOption('WARP_THREAD_CHUNK_SIZE', '0')
    got_cs = ds.GetRasterBand(1).Checksum()
    gdal.SetConfigOption('GDAL_NUM_THREADS', old_val)
    gdal.SetConfigOption('WARP_THREAD_CHUNK_SIZE', None)
    ds = None

    assert got_cs == src_ds.GetRasterBand(1).Checksum()

    src_ds = None

###############################################################################
# Test warping interruption


def warp_30_progress_callback(pct, message, user_data):
    # pylint: disable=unused-argument
    return bool(pct <= 0.2)


def test_warp_30():

    # Open source dataset
    src_ds = gdal.Open('../gcore/data/byte.tif')

    # Desfine target SRS
    dst_srs = osr.SpatialReference()
    dst_srs.ImportFromEPSG(4326)
    dst_wkt = dst_srs.ExportToWkt()

    error_threshold = 0.125  # error threshold --> use same value as in gdalwarp
    resampling = gdal.GRA_Bilinear

    # Call AutoCreateWarpedVRT() to fetch default values for target raster dimensions and geotransform
    tmp_ds = gdal.AutoCreateWarpedVRT(src_ds,
                                      None,  # src_wkt : left to default value --> will use the one from source \
                                      dst_wkt, \
                                      resampling, \
                                      error_threshold)
    dst_xsize = tmp_ds.RasterXSize
    dst_ysize = tmp_ds.RasterYSize
    dst_gt = tmp_ds.GetGeoTransform()
    tmp_ds = None

    # Now create the true target dataset
    dst_ds = gdal.GetDriverByName('GTiff').Create('/vsimem/warp_30.tif', dst_xsize, dst_ysize,
                                                  src_ds.RasterCount)
    dst_ds.SetProjection(dst_wkt)
    dst_ds.SetGeoTransform(dst_gt)

    # And run the reprojection

    cbk = warp_30_progress_callback
    cbk_user_data = None  # value for last parameter of above warp_27_progress_callback

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ret = gdal.ReprojectImage(src_ds,
                              dst_ds,
                              None,  # src_wkt : left to default value --> will use the one from source \
                              None,  # dst_wkt : left to default value --> will use the one from destination \
                              resampling, \
                              0,  # WarpMemoryLimit : left to default value \
                              error_threshold,
                              cbk,  # Progress callback : could be left to None or unspecified for silent progress
                              cbk_user_data)  # Progress callback user data
    gdal.PopErrorHandler()

    assert ret != 0

    old_val = gdal.GetConfigOption('GDAL_NUM_THREADS')
    gdal.SetConfigOption('GDAL_NUM_THREADS', '2')
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ret = gdal.ReprojectImage(src_ds,
                              dst_ds,
                              None,  # src_wkt : left to default value --> will use the one from source \
                              None,  # dst_wkt : left to default value --> will use the one from destination \
                              resampling, \
                              0,  # WarpMemoryLimit : left to default value \
                              error_threshold,
                              cbk,  # Progress callback : could be left to None or unspecified for silent progress
                              cbk_user_data)  # Progress callback user data
    gdal.PopErrorHandler()
    gdal.SetConfigOption('GDAL_NUM_THREADS', old_val)

    assert ret != 0

    gdal.Unlink('/vsimem/warp_30.tif')

# Average (Byte)


def test_warp_31():

    gdaltest.tiff_drv = gdal.GetDriverByName('GTiff')
    if gdaltest.tiff_drv is None:
        pytest.skip()

    ds = gdal.Open('data/utmsmall_average.vrt')
    ref_ds = gdal.Open('data/utmsmall_average.tiff')
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None

    assert maxdiff <= 1, 'Image too different from reference'

# Average (Float)


def test_warp_32():

    gdaltest.tiff_drv = gdal.GetDriverByName('GTiff')
    if gdaltest.tiff_drv is None:
        pytest.skip()

    ds = gdal.Open('data/utmsmall_average_float.vrt')
    ref_ds = gdal.Open('data/utmsmall_average_float.tiff')
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None

    assert maxdiff <= 1, 'Image too different from reference'

# Mode (Byte)


def test_warp_33():

    gdaltest.tiff_drv = gdal.GetDriverByName('GTiff')
    if gdaltest.tiff_drv is None:
        pytest.skip()

    ds = gdal.Open('data/utmsmall_mode.vrt')
    ref_ds = gdal.Open('data/utmsmall_mode.tiff')
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None

    assert maxdiff <= 1, 'Image too different from reference'

# Mode (Int16)


def test_warp_34():

    gdaltest.tiff_drv = gdal.GetDriverByName('GTiff')
    if gdaltest.tiff_drv is None:
        pytest.skip()

    ds = gdal.Open('data/utmsmall_mode_int16.vrt')
    ref_ds = gdal.Open('data/utmsmall_mode_int16.tiff')
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None

    assert maxdiff <= 1, 'Image too different from reference'

# Mode (Int16 - signed with negative values)


def test_warp_35():

    gdaltest.tiff_drv = gdal.GetDriverByName('GTiff')
    if gdaltest.tiff_drv is None:
        pytest.skip()

    ds = gdal.Open('data/utmsmall-int16-neg_mode.vrt')
    ref_ds = gdal.Open('data/utmsmall-int16-neg_mode.tiff')
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None

    assert maxdiff <= 1, 'Image too different from reference'

# Mode (Int32) - this uses algorithm 2 (inefficient)


def test_warp_36():

    gdaltest.tiff_drv = gdal.GetDriverByName('GTiff')
    if gdaltest.tiff_drv is None:
        pytest.skip()

    ds = gdal.Open('data/utmsmall_mode_int32.vrt')
    ref_ds = gdal.Open('data/utmsmall_mode_int32.tiff')
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None

    assert maxdiff <= 1, 'Image too different from reference'

###############################################################################
# Test a few error cases


def test_warp_37():

    # Open source dataset
    src_ds = gdal.Open('../gcore/data/byte.tif')

    # Dummy proj.4 method
    sr = osr.SpatialReference()
    sr.ImportFromWkt("""PROJCS["unnamed",
    GEOGCS["unnamed ellipse",
        DATUM["unknown",
            SPHEROID["unnamed",6378137,298.257223563]],
        PRIMEM["Greenwich",0],
        UNIT["degree",0.0174532925199433]],
    PROJECTION["custom_proj4"],
    UNIT["Meter",1],
    EXTENSION["PROJ4","+proj=dummy_method +units=m +wktext"]]""")
    dst_wkt = sr.ExportToWkt()

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    tmp_ds = gdal.AutoCreateWarpedVRT(src_ds, None, dst_wkt)
    gdal.PopErrorHandler()
    gdal.ErrorReset()
    assert tmp_ds is None

###############################################################################
# Test a warp with GCPs on the *destination* image.


def test_warp_38():

    # Create an output file with GCPs.
    out_file = 'tmp/warp_38.tif'
    ds = gdal.GetDriverByName('GTiff').Create(out_file, 50, 50, 3)

    gcp_list = [
        gdal.GCP(397000, 5642000, 0, 0, 0),
        gdal.GCP(397000, 5641990, 0, 0, 50),
        gdal.GCP(397010, 5642000, 0, 50, 0),
        gdal.GCP(397010, 5641990, 0, 50, 50),
        gdal.GCP(397005, 5641995, 0, 25, 25),
    ]
    ds.SetGCPs(gcp_list, gdaltest.user_srs_to_wkt('EPSG:32632'))
    ds = None

    gdal.Warp(out_file, 'data/test3658.tif', options='-to DST_METHOD=GCP_POLYNOMIAL')

    ds = gdal.Open(out_file)
    cs = ds.GetRasterBand(1).Checksum()
    ds = None

    # Should exactly match the source file.
    exp_cs = 30546
    assert cs == exp_cs

    os.unlink(out_file)

###############################################################################
# Test a warp with GCPs for TPS on the *destination* image.


def test_warp_39():

    # Create an output file with GCPs.
    out_file = 'tmp/warp_39.tif'
    ds = gdal.GetDriverByName('GTiff').Create(out_file, 50, 50, 3)

    gcp_list = [
        gdal.GCP(397000, 5642000, 0, 0, 0),
        gdal.GCP(397000, 5641990, 0, 0, 50),
        gdal.GCP(397010, 5642000, 0, 50, 0),
        gdal.GCP(397010, 5641990, 0, 50, 50),
        gdal.GCP(397005, 5641995, 0, 25, 25),
    ]
    ds.SetGCPs(gcp_list, gdaltest.user_srs_to_wkt('EPSG:32632'))
    ds = None

    gdal.Warp(out_file, 'data/test3658.tif', options='-to DST_METHOD=GCP_TPS')

    ds = gdal.Open(out_file)
    cs = ds.GetRasterBand(1).Checksum()
    ds = None

    # Should exactly match the source file.
    exp_cs = 30546
    assert cs == exp_cs

    os.unlink(out_file)

###############################################################################
# test average (#5311)


def test_warp_40():

    gdaltest.tiff_drv = gdal.GetDriverByName('GTiff')
    if gdaltest.tiff_drv is None:
        pytest.skip()

    ds = gdal.Open('data/2by2.vrt')
    ref_ds = gdal.Open('data/2by2.tif')
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None

    assert maxdiff <= 1, 'Image too different from reference'

###############################################################################
# test weighted average


def test_warp_weighted_average():

    gdaltest.tiff_drv = gdal.GetDriverByName('GTiff')
    if gdaltest.tiff_drv is None:
        pytest.skip()

    ds = gdal.Open('data/3by3_average.vrt')
    ref_ds = gdal.Open('data/3by3_average.tif')
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None

    assert maxdiff <= 1, 'Image too different from reference'

###############################################################################
# test weighted average, with src offset (fix for #2665)


def test_warp_weighted_average_with_srcoffset():

    ds = gdal.Open('data/3by3_average_with_srcoffset.vrt')
    val = struct.unpack('d', ds.ReadRaster(0, 0, 1, 1))[0]
    assert val == pytest.approx(8.5, abs=1e-5)

###############################################################################
# test sum


def test_warp_sum():

    gdaltest.tiff_drv = gdal.GetDriverByName('GTiff')
    if gdaltest.tiff_drv is None:
        pytest.skip()

    ds = gdal.Open('data/3by3_sum.vrt')
    ref_ds = gdal.Open('data/3by3_sum.tif')
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None

    assert maxdiff <= 1, 'Image too different from reference'

###############################################################################
# test GDALSuggestedWarpOutput (#5693)


def test_warp_41():

    src_ds = gdal.Open("""<VRTDataset rasterXSize="67108864" rasterYSize="67108864">
  <GeoTransform> -2.0037508340000000e+07,  5.9716428339481353e-01,  0.0000000000000000e+00,  2.0037508340000000e+07,  0.0000000000000000e+00, -5.9716428339481353e-01</GeoTransform>
  <VRTRasterBand dataType="Byte" band="1">
    <SimpleSource>
      <SourceFilename relativeToVRT="0">dummy</SourceFilename>
      <SourceBand>1</SourceBand>
      <SourceProperties RasterXSize="67108864" RasterYSize="67108864" DataType="Byte" BlockXSize="256" BlockYSize="256" />
      <SrcRect xOff="0" yOff="0" xSize="67108864" ySize="67108864" />
      <DstRect xOff="0" yOff="0" xSize="67108864" ySize="67108864" />
    </SimpleSource>
  </VRTRasterBand>
</VRTDataset>""")

    vrt_ds = gdal.AutoCreateWarpedVRT(src_ds, None, None, gdal.GRA_NearestNeighbour, 0.3)
    assert vrt_ds.RasterXSize == src_ds.RasterXSize
    assert vrt_ds.RasterYSize == src_ds.RasterYSize
    src_gt = src_ds.GetGeoTransform()
    vrt_gt = vrt_ds.GetGeoTransform()
    for i in range(6):
        assert src_gt[i] == pytest.approx(vrt_gt[i], abs=1e-5)


###############################################################################

# Maximum


def test_warp_42():

    gdaltest.tiff_drv = gdal.GetDriverByName('GTiff')
    if gdaltest.tiff_drv is None:
        pytest.skip()

    ds = gdal.Open('data/utmsmall_max.vrt')
    ref_ds = gdal.Open('data/utmsmall_max.tif')
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None

    assert maxdiff <= 1, 'Image too different from reference'

# Minimum


def test_warp_43():

    gdaltest.tiff_drv = gdal.GetDriverByName('GTiff')
    if gdaltest.tiff_drv is None:
        pytest.skip()

    ds = gdal.Open('data/utmsmall_min.vrt')
    ref_ds = gdal.Open('data/utmsmall_min.tif')
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None

    assert maxdiff <= 1, 'Image too different from reference'

# Median


def test_warp_44():

    gdaltest.tiff_drv = gdal.GetDriverByName('GTiff')
    if gdaltest.tiff_drv is None:
        pytest.skip()

    ds = gdal.Open('data/utmsmall_med.vrt')
    ref_ds = gdal.Open('data/utmsmall_med.tif')
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None

    assert maxdiff <= 1, 'Image too different from reference'

# Quartile 1


def test_warp_45():

    gdaltest.tiff_drv = gdal.GetDriverByName('GTiff')
    if gdaltest.tiff_drv is None:
        pytest.skip()

    ds = gdal.Open('data/utmsmall_Q1.vrt')
    ref_ds = gdal.Open('data/utmsmall_Q1.tif')
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None

    assert maxdiff <= 1, 'Image too different from reference'

# Quartile 3


def test_warp_46():

    gdaltest.tiff_drv = gdal.GetDriverByName('GTiff')
    if gdaltest.tiff_drv is None:
        pytest.skip()

    ds = gdal.Open('data/utmsmall_Q3.vrt')
    ref_ds = gdal.Open('data/utmsmall_Q3.tif')
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None

    assert maxdiff <= 1, 'Image too different from reference'

# Maximum (Int16 - signed with negative values)


def test_warp_47():

    gdaltest.tiff_drv = gdal.GetDriverByName('GTiff')
    if gdaltest.tiff_drv is None:
        pytest.skip()

    ds = gdal.Open('data/utmsmall-int16-neg_max.vrt')
    ref_ds = gdal.Open('data/utmsmall-int16-neg_max.tif')
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None

    assert maxdiff <= 1, 'Image too different from reference'

# Minimum (Int16 - signed with negative values)


def test_warp_48():

    gdaltest.tiff_drv = gdal.GetDriverByName('GTiff')
    if gdaltest.tiff_drv is None:
        pytest.skip()

    ds = gdal.Open('data/utmsmall-int16-neg_min.vrt')
    ref_ds = gdal.Open('data/utmsmall-int16-neg_min.tif')
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None

    assert maxdiff <= 1, 'Image too different from reference'

# Median (Int16 - signed with negative values)


def test_warp_49():

    gdaltest.tiff_drv = gdal.GetDriverByName('GTiff')
    if gdaltest.tiff_drv is None:
        pytest.skip()

    ds = gdal.Open('data/utmsmall-int16-neg_med.vrt')
    ref_ds = gdal.Open('data/utmsmall-int16-neg_med.tif')
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None

    assert maxdiff <= 1, 'Image too different from reference'

# Quartile 1 (Int16 - signed with negative values)


def test_warp_50():

    gdaltest.tiff_drv = gdal.GetDriverByName('GTiff')
    if gdaltest.tiff_drv is None:
        pytest.skip()

    ds = gdal.Open('data/utmsmall-int16-neg_Q1.vrt')
    ref_ds = gdal.Open('data/utmsmall-int16-neg_Q1.tif')
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None

    assert maxdiff <= 1, 'Image too different from reference'

# Quartile 3 (Int16 - signed with negative values)


def test_warp_51():

    gdaltest.tiff_drv = gdal.GetDriverByName('GTiff')
    if gdaltest.tiff_drv is None:
        pytest.skip()

    ds = gdal.Open('data/utmsmall-int16-neg_Q3.vrt')
    ref_ds = gdal.Open('data/utmsmall-int16-neg_Q3.tif')
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None

    assert maxdiff <= 1, 'Image too different from reference'

###############################################################################
# Test fix for #6182


def test_warp_52():

    src_ds = gdal.GetDriverByName('MEM').Create('', 4096, 4096, 3, gdal.GDT_UInt16)
    rpc = [
        "HEIGHT_OFF=1466.05894327379",
        "HEIGHT_SCALE=144.837606185489",
        "LAT_OFF=38.9266809014185",
        "LAT_SCALE=-0.108324009570885",
        "LINE_DEN_COEFF=1 -0.000392404256440504 -0.0027925489381758 0.000501819414812054 0.00216726134806561 -0.00185617059201599 0.000183834173326118 -0.00290342803717354 -0.00207181007131322 -0.000900223247894285 -0.00132518281680544 0.00165598132063197 0.00681015244696305 0.000547865679631528 0.00516214646283021 0.00795287690785699 -0.000705040639059332 -0.00254360623317078 -0.000291154885056484 0.00070943440010757",
        "LINE_NUM_COEFF=-0.000951099635749339 1.41709976082781 -0.939591985038569 -0.00186609235173885 0.00196881101098923 0.00361741523740639 -0.00282449434932066 0.0115361898794214 -0.00276027843825304 9.37913944402154e-05 -0.00160950221565737 0.00754053609977256 0.00461831968713819 0.00274991122620312 0.000689605203796422 -0.0042482778732957 -0.000123966494595151 0.00307976709897974 -0.000563274426174409 0.00049981716767074",
        "LINE_OFF=2199.50159296339",
        "LINE_SCALE=2195.852519621",
        "LONG_OFF=76.0381768085136",
        "LONG_SCALE=0.130066683772651",
        "SAMP_DEN_COEFF=1 -0.000632078047521022 -0.000544107268758971 0.000172438016778527 -0.00206391734870399 -0.00204445747536872 -0.000715754551621987 -0.00195545265530244 -0.00168532972557267 -0.00114709980708329 -0.00699131177532728 0.0038551339822296 0.00283631282133365 -0.00436885468926666 -0.00381335885955994 0.0018742043611019 -0.0027263909314293 -0.00237054119407013 0.00246374716379501 -0.00121074576302219",
        "SAMP_NUM_COEFF=0.00249293151551852 -0.581492592442025 -1.00947448466175 0.00121597346320039 -0.00552825219917498 -0.00194683170765094 -0.00166012459012905 -0.00338315804553888 -0.00152062885009498 -0.000214562164393127 -0.00219914905535387 -0.000662800177832777 -0.00118644828432841 -0.00180061222825708 -0.00364756875260519 -0.00287273485650089 -0.000540077934728493 -0.00166800463003749 0.000201057249109451 -8.49620129025469e-05",
        "SAMP_OFF=3300.34602166792",
        "SAMP_SCALE=3297.51222987611"
    ]
    src_ds.SetMetadata(rpc, "RPC")

    import time
    start = time.time()

    out_ds = gdal.Warp('', src_ds, format='MEM',
                       outputBounds=[8453323.83095, 4676723.13796, 8472891.71018, 4696291.0172],
                       xRes=4.77731426716,
                       yRes=4.77731426716,
                       dstSRS='EPSG:3857',
                       warpOptions=['SKIP_NOSOURCE=YES', 'DST_ALPHA_MAX=255'],
                       transformerOptions=['RPC_DEM=data/warp_52_dem.tif'],
                       dstAlpha=True,
                       errorThreshold=0,
                       resampleAlg=gdal.GRA_Cubic)

    end = time.time()
    assert end - start <= 10, 'processing time was way too long'

    cs = out_ds.GetRasterBand(4).Checksum()
    assert cs == 3188

###############################################################################
# Test Grey+Alpha


def test_warp_53():

    if sys.platform == 'darwin':
        pytest.skip("Expected checksum should be updated for Mac")

    for typestr in ('Byte', 'UInt16', 'Int16'):
        src_ds = gdal.Translate('', '../gcore/data/byte.tif',
                                options='-of MEM -b 1 -b 1 -ot ' + typestr)
        src_ds.GetRasterBand(2).SetColorInterpretation(gdal.GCI_AlphaBand)
        src_ds.GetRasterBand(2).Fill(255)
        zero = struct.pack('B' * 1, 0)
        src_ds.GetRasterBand(2).WriteRaster(10, 10, 1, 1, zero,
                                            buf_type=gdal.GDT_Byte)
        dst_ds = gdal.Translate('', src_ds,
                                options='-of MEM -a_srs EPSG:32611')

        for option in ('-wo USE_GENERAL_CASE=TRUE', ''):
            # First checksum is proj 4.8, second proj 4.9.2
            for alg_name, expected_cs in (('near', [3781, 3843]),
                                          ('cubic', [3942, 4133]),
                                          ('cubicspline', [3874, 4076]),
                                          ('bilinear', [4019, 3991])):
                dst_ds.GetRasterBand(1).Fill(0)
                dst_ds.GetRasterBand(2).Fill(0)
                gdal.Warp(dst_ds, src_ds,
                          options='-r ' + alg_name + ' ' + option)
                cs1 = dst_ds.GetRasterBand(1).Checksum()
                cs2 = dst_ds.GetRasterBand(2).Checksum()
                if cs1 not in expected_cs or cs2 not in [3903, 4138]:
                    print(typestr)
                    print(option)
                    pytest.fail(alg_name)


###############################################################################
# Test Alpha on UInt16/Int16


def test_warp_54():

    # UInt16
    src_ds = gdal.Translate('', '../gcore/data/stefan_full_rgba.tif',
                                options='-of MEM -scale 0 255 0 65535 -ot UInt16 -a_ullr -162 150 0 0')
    dst_ds = gdal.Warp('', src_ds, format='MEM')
    for i in range(4):
        expected_cs = src_ds.GetRasterBand(i + 1).Checksum()
        got_cs = dst_ds.GetRasterBand(i + 1).Checksum()
        assert expected_cs == got_cs, i

    # Int16
    src_ds = gdal.Translate('', '../gcore/data/stefan_full_rgba.tif',
                                options='-of MEM -scale 0 255 0 32767 -ot Int16 -a_ullr -162 150 0 0')
    dst_ds = gdal.Warp('', src_ds, format='MEM')
    for i in range(4):
        expected_cs = src_ds.GetRasterBand(i + 1).Checksum()
        got_cs = dst_ds.GetRasterBand(i + 1).Checksum()
        assert expected_cs == got_cs, i

    # Test NBITS
    src_ds = gdal.Translate('', '../gcore/data/stefan_full_rgba.tif',
                                options='-of MEM -scale 0 255 0 32767 -ot UInt16 -a_ullr -162 150 0 0')
    for i in range(4):
        src_ds.GetRasterBand(i + 1).SetMetadataItem('NBITS', '15', 'IMAGE_STRUCTURE')
    dst_ds = gdal.Warp('/vsimem/warp_54.tif', src_ds, options='-co NBITS=15')
    for i in range(4):
        expected_cs = src_ds.GetRasterBand(i + 1).Checksum()
        got_cs = dst_ds.GetRasterBand(i + 1).Checksum()
        assert expected_cs == got_cs, i
    dst_ds = None

    gdal.Unlink('/vsimem/warp_54.tif')

###############################################################################
# Test warped VRT with source overview, target GT != GenImgProjetion target GT
# and subsampling (#6972)


def test_warp_55():

    ds = gdal.Open('data/warpedvrt_with_ovr.vrt')
    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    assert cs == 25128
    ds = None

###############################################################################
# Test bilinear interpolation when warping into same coordinate system (and
# same size). This test crops a single pixel out of a 3-by-3 image.


def test_warp_56():

    try:
        import numpy
    except (ImportError, AttributeError):
        pytest.skip()

    pix_ds = gdal.GetDriverByName('MEM').Create('', 1, 1)
    src_ds = gdal.GetDriverByName('MEM').Create('', 3, 3)
    src_ds.GetRasterBand(1).WriteArray(numpy.array([[0, 0, 0],
                                                    [0, 0, 0],
                                                    [0, 0, 100]]))
    src_ds.SetGeoTransform([1, 1, 0,
                            1, 0, 1])

    for off in numpy.linspace(0, 2, 21):
        pix_ds.SetGeoTransform([off + 1, 1, 0,
                                off + 1, 0, 1])
        gdal.Warp(pix_ds, src_ds, resampleAlg='bilinear')

        exp = 0 if off < 1 else 100 * (off - 1)**2
        warped = pix_ds.GetRasterBand(1).ReadAsArray()[0, 0]
        assert warped == pytest.approx(exp, abs=0.6), \
            'offset: {}, expected: {:.0f}, got: {}'.format(off, exp, warped)


###############################################################################
# Test bugfix for #1656

def test_warp_nearest_real_nodata_multiple_band():

    src_ds = gdal.GetDriverByName('MEM').Create('', 2, 1, 2, gdal.GDT_Float64)
    src_ds.GetRasterBand(1).SetNoDataValue(65535)
    src_ds.GetRasterBand(1).WriteRaster(0,0,1,1,struct.pack('d', 65535))
    src_ds.GetRasterBand(2).SetNoDataValue(65535)
    src_ds.GetRasterBand(2).WriteRaster(0,0,1,1,struct.pack('d', 65535))
    src_ds.SetGeoTransform([1, 1, 0,
                            1, 0, 1])
    out_ds = gdal.Warp('', src_ds, options = '-of MEM')
    assert struct.unpack('d' * 4, out_ds.ReadRaster()) == struct.unpack('d' * 4, src_ds.ReadRaster())


###############################################################################
# Test bugfix for #2365

def test_warp_med_out_of_bounds_src_pixels():

    ds = gdal.Open('data/test_bug_2365_wraped_med.vrt')
    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 0
    ds = None

###############################################################################
# Test fix for #2460


def test_warp_rpc_source_has_geotransform():

    out_ds = gdal.Warp('', 'data/test_rpc_with_gt_bug_2460.tif',
                       format='MEM',
                       transformerOptions=['METHOD=RPC', 'RPC_HEIGHT=1118'])
    cs = out_ds.GetRasterBand(1).Checksum()
    assert cs == 60397

###############################################################################
# Test RMS resampling

def test_warp_ds_rms():

    gdaltest.tiff_drv = gdal.GetDriverByName('GTiff')
    if gdaltest.tiff_drv is None:
        pytest.skip()

    tst = gdaltest.GDALTest('VRT', 'utmsmall_ds_rms.vrt', 1, 4770)

    return tst.testOpen()

def test_warp_rms_1():

    gdaltest.tiff_drv = gdal.GetDriverByName('GTiff')
    if gdaltest.tiff_drv is None:
        pytest.skip()

    tst = gdaltest.GDALTest('VRT', 'utmsmall_rms_float.vrt', 1, 30396)

    return tst.testOpen()

def test_warp_rms_2():

    gdaltest.tiff_drv = gdal.GetDriverByName('GTiff')
    if gdaltest.tiff_drv is None:
        pytest.skip()

    tst = gdaltest.GDALTest('VRT', 'utmsmall_rms.vrt', 1, 30396)

    return tst.testOpen()
