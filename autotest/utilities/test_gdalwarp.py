#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdalwarp testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
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
import stat

import gdaltest
import pytest
import test_cli_utilities

from osgeo import gdal

pytestmark = pytest.mark.skipif(
    test_cli_utilities.get_gdalwarp_path() is None, reason="gdalwarp not available"
)


@pytest.fixture()
def gdalwarp_path():
    return test_cli_utilities.get_gdalwarp_path()


###############################################################################
# Simple test


def test_gdalwarp_1(gdalwarp_path):

    (_, err) = gdaltest.runexternal_out_and_err(
        gdalwarp_path + " ../gcore/data/byte.tif tmp/testgdalwarp1.tif"
    )
    assert err is None or err == "", "got error/warning"

    ds = gdal.Open("tmp/testgdalwarp1.tif")
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 4672, "Bad checksum"

    ds = None


###############################################################################
# Test -of option


def test_gdalwarp_2(gdalwarp_path):

    gdaltest.runexternal(
        gdalwarp_path + " -of GTiff ../gcore/data/byte.tif tmp/testgdalwarp2.tif"
    )

    ds = gdal.Open("tmp/testgdalwarp2.tif")
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 4672, "Bad checksum"

    ds = None


###############################################################################
# Test -ot option


def test_gdalwarp_3(gdalwarp_path):

    gdaltest.runexternal(
        gdalwarp_path + " -ot Int16 ../gcore/data/byte.tif tmp/testgdalwarp3.tif"
    )

    ds = gdal.Open("tmp/testgdalwarp3.tif")
    assert ds is not None

    assert ds.GetRasterBand(1).DataType == gdal.GDT_Int16, "Bad data type"

    assert ds.GetRasterBand(1).Checksum() == 4672, "Bad checksum"

    ds = None


###############################################################################
# Test -t_srs option


def test_gdalwarp_4(gdalwarp_path):

    gdaltest.runexternal(
        gdalwarp_path
        + " -t_srs EPSG:32611 ../gcore/data/byte.tif tmp/testgdalwarp4.tif"
    )

    ds = gdal.Open("tmp/testgdalwarp4.tif")
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 4672, "Bad checksum"

    ds = None


###############################################################################
# Test warping from GCPs without any explicit option


def test_gdalwarp_5(gdalwarp_path):

    if test_cli_utilities.get_gdal_translate_path() is None:
        pytest.skip("gdal_translate missing")

    gdaltest.runexternal(
        test_cli_utilities.get_gdal_translate_path()
        + " -a_srs EPSG:26711 -gcp 0 0  440720.000 3751320.000 -gcp 20 0 441920.000 3751320.000 -gcp 20 20 441920.000 3750120.000 0 -gcp 0 20 440720.000 3750120.000 ../gcore/data/byte.tif tmp/testgdalwarp_gcp.tif"
    )

    gdaltest.runexternal(
        gdalwarp_path + " tmp/testgdalwarp_gcp.tif tmp/testgdalwarp5.tif"
    )

    ds = gdal.Open("tmp/testgdalwarp5.tif")
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 4672, "Bad checksum"

    gdaltest.check_geotransform(
        gdal.Open("../gcore/data/byte.tif").GetGeoTransform(),
        ds.GetGeoTransform(),
        1e-9,
    )

    ds = None


###############################################################################
# Test warping from GCPs with -tps


def test_gdalwarp_6(gdalwarp_path):

    gdaltest.runexternal(
        gdalwarp_path + " -tps tmp/testgdalwarp_gcp.tif tmp/testgdalwarp6.tif"
    )

    ds = gdal.Open("tmp/testgdalwarp6.tif")
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 4672, "Bad checksum"

    gdaltest.check_geotransform(
        gdal.Open("../gcore/data/byte.tif").GetGeoTransform(),
        ds.GetGeoTransform(),
        1e-9,
    )

    ds = None


###############################################################################
# Test -tr


def test_gdalwarp_7(gdalwarp_path):

    gdaltest.runexternal(
        gdalwarp_path + " -tr 120 120 tmp/testgdalwarp_gcp.tif tmp/testgdalwarp7.tif"
    )

    ds = gdal.Open("tmp/testgdalwarp7.tif")
    assert ds is not None

    expected_gt = (440720.0, 120.0, 0.0, 3751320.0, 0.0, -120.0)
    gdaltest.check_geotransform(expected_gt, ds.GetGeoTransform(), 1e-9)

    ds = None


###############################################################################
# Test -ts


def test_gdalwarp_8(gdalwarp_path):

    gdaltest.runexternal(
        gdalwarp_path + " -ts 10 10 tmp/testgdalwarp_gcp.tif tmp/testgdalwarp8.tif"
    )

    ds = gdal.Open("tmp/testgdalwarp8.tif")
    assert ds is not None

    expected_gt = (440720.0, 120.0, 0.0, 3751320.0, 0.0, -120.0)
    gdaltest.check_geotransform(expected_gt, ds.GetGeoTransform(), 1e-9)

    ds = None


###############################################################################
# Test -te


def test_gdalwarp_9(gdalwarp_path):

    gdaltest.runexternal(
        gdalwarp_path
        + " -te 440720.000 3750120.000 441920.000 3751320.000 tmp/testgdalwarp_gcp.tif tmp/testgdalwarp9.tif"
    )

    ds = gdal.Open("tmp/testgdalwarp9.tif")
    assert ds is not None

    gdaltest.check_geotransform(
        gdal.Open("../gcore/data/byte.tif").GetGeoTransform(),
        ds.GetGeoTransform(),
        1e-9,
    )

    ds = None


###############################################################################
# Test -rn


def test_gdalwarp_10(gdalwarp_path):

    gdaltest.runexternal(
        gdalwarp_path + " -ts 40 40 -rn tmp/testgdalwarp_gcp.tif tmp/testgdalwarp10.tif"
    )

    ds = gdal.Open("tmp/testgdalwarp10.tif")
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 18784, "Bad checksum"

    ds = None


###############################################################################
# Test -rb


def test_gdalwarp_11(gdalwarp_path):

    gdaltest.runexternal(
        gdalwarp_path + " -ts 40 40 -rb tmp/testgdalwarp_gcp.tif tmp/testgdalwarp11.tif"
    )

    ds = gdal.Open("tmp/testgdalwarp11.tif")
    assert ds is not None

    ref_ds = gdal.Open("ref_data/testgdalwarp11.tif")
    maxdiff = gdaltest.compare_ds(ds, ref_ds, verbose=0)
    ref_ds = None

    if maxdiff > 1:
        gdaltest.compare_ds(ds, ref_ds, verbose=1)
        pytest.fail("Image too different from reference")

    ds = None


###############################################################################
# Test -rc


def test_gdalwarp_12(gdalwarp_path):

    gdaltest.runexternal(
        gdalwarp_path + " -ts 40 40 -rc tmp/testgdalwarp_gcp.tif tmp/testgdalwarp12.tif"
    )

    ds = gdal.Open("tmp/testgdalwarp12.tif")
    assert ds is not None

    ref_ds = gdal.Open("ref_data/testgdalwarp12.tif")
    maxdiff = gdaltest.compare_ds(ds, ref_ds, verbose=0)

    if maxdiff > 1:
        gdaltest.compare_ds(ds, ref_ds, verbose=1)
        ref_ds = None
        pytest.fail("Image too different from reference")

    ds = None
    ref_ds = None


###############################################################################
# Test -rcs


def test_gdalwarp_13(gdalwarp_path):

    gdaltest.runexternal(
        gdalwarp_path
        + " -ts 40 40 -rcs tmp/testgdalwarp_gcp.tif tmp/testgdalwarp13.tif"
    )

    ds = gdal.Open("tmp/testgdalwarp13.tif")
    assert ds is not None

    ref_ds = gdal.Open("ref_data/testgdalwarp13.tif")
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ref_ds = None

    assert maxdiff <= 1, "Image too different from reference"

    ds = None


###############################################################################
# Test -r lanczos


def test_gdalwarp_14(gdalwarp_path):

    gdaltest.runexternal(
        gdalwarp_path
        + " -ts 40 40 -r lanczos tmp/testgdalwarp_gcp.tif tmp/testgdalwarp14.tif"
    )

    ds = gdal.Open("tmp/testgdalwarp14.tif")
    assert ds is not None

    ref_ds = gdal.Open("ref_data/testgdalwarp14.tif")
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ref_ds = None

    assert maxdiff <= 1, "Image too different from reference"

    ds = None


###############################################################################
# Test -of VRT which is a special case


def test_gdalwarp_16(gdalwarp_path):

    gdaltest.runexternal(
        gdalwarp_path + " -of VRT tmp/testgdalwarp_gcp.tif tmp/testgdalwarp16.vrt"
    )

    ds = gdal.Open("tmp/testgdalwarp16.vrt")
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 4672, "Bad checksum"

    ds = None


###############################################################################
# Test -dstalpha


def test_gdalwarp_17(gdalwarp_path):

    gdaltest.runexternal(
        gdalwarp_path + " -dstalpha ../gcore/data/rgbsmall.tif tmp/testgdalwarp17.tif"
    )

    ds = gdal.Open("tmp/testgdalwarp17.tif")
    assert ds is not None

    assert ds.GetRasterBand(4) is not None, "No alpha band generated"

    ds = None


###############################################################################
# Test -wm -multi


def test_gdalwarp_18(gdalwarp_path):

    (_, ret_stderr) = gdaltest.runexternal_out_and_err(
        gdalwarp_path + " -wm 20 -multi ../gcore/data/byte.tif tmp/testgdalwarp18.tif"
    )

    # This error will be returned if GDAL is not compiled with thread support
    if ret_stderr.find("CPLCreateThread() failed in ChunkAndWarpMulti()") != -1:
        pytest.skip("GDAL not compiled with thread support")

    ds = gdal.Open("tmp/testgdalwarp18.tif")
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 4672, "Bad checksum"

    ds = None


###############################################################################
# Test -et 0 which is a special case


def test_gdalwarp_19(gdalwarp_path):

    gdaltest.runexternal(
        gdalwarp_path + " -et 0 tmp/testgdalwarp_gcp.tif tmp/testgdalwarp19.tif"
    )

    ds = gdal.Open("tmp/testgdalwarp19.tif")
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 4672, "Bad checksum"

    ds = None


###############################################################################
# Test -of VRT -et 0 which is a special case


def test_gdalwarp_20(gdalwarp_path):

    gdaltest.runexternal(
        gdalwarp_path + " -of VRT -et 0 tmp/testgdalwarp_gcp.tif tmp/testgdalwarp20.vrt"
    )

    ds = gdal.Open("tmp/testgdalwarp20.vrt")
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 4672, "Bad checksum"

    ds = None


###############################################################################
# Test cutline from OGR datasource.


@pytest.mark.require_driver("CSV")
def test_gdalwarp_21(gdalwarp_path):

    gdaltest.runexternal(
        gdalwarp_path
        + " ../gcore/data/utmsmall.tif tmp/testgdalwarp21.tif -cutline data/cutline.vrt -cl cutline"
    )

    ds = gdal.Open("tmp/testgdalwarp21.tif")
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 19139, "Bad checksum"

    ds = None


###############################################################################
# Test with a cutline and an output at a different resolution.


@pytest.mark.require_driver("CSV")
def test_gdalwarp_22(gdalwarp_path):

    gdaltest.runexternal(
        gdalwarp_path
        + " ../gcore/data/utmsmall.tif tmp/testgdalwarp22.tif -cutline data/cutline.vrt -cl cutline -tr 30 30"
    )

    ds = gdal.Open("tmp/testgdalwarp22.tif")
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 14047, "Bad checksum"

    ds = None


###############################################################################
# Test cutline with ALL_TOUCHED enabled.


@pytest.mark.require_driver("CSV")
def test_gdalwarp_23(gdalwarp_path):

    gdaltest.runexternal(
        gdalwarp_path
        + " -wo CUTLINE_ALL_TOUCHED=TRUE ../gcore/data/utmsmall.tif tmp/testgdalwarp23.tif -cutline data/cutline.vrt -cl cutline"
    )

    ds = gdal.Open("tmp/testgdalwarp23.tif")
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 20123, "Bad checksum"

    ds = None


###############################################################################
# Test warping an image crossing the 180E/180W longitude (#3206)


def test_gdalwarp_24(gdalwarp_path):

    ds = gdal.GetDriverByName("GTiff").Create("tmp/testgdalwarp24src.tif", 100, 100)
    ds.SetGeoTransform([179.5, 0.01, 0, 45, 0, -0.01])
    ds.SetProjection(
        'GEOGCS["GCS_WGS_1984",DATUM["D_WGS_1984",SPHEROID["WGS_1984",6378137.0,298.257223563]],PRIMEM["Greenwich",0.0],UNIT["Degree",0.0174532925199433]]'
    )
    ds.GetRasterBand(1).Fill(255)
    ds = None

    gdaltest.runexternal(
        gdalwarp_path
        + " -t_srs EPSG:32660 tmp/testgdalwarp24src.tif tmp/testgdalwarp24dst.tif"
    )

    ds = gdal.Open("tmp/testgdalwarp24dst.tif")
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 50683, "Bad checksum"

    ds = None


###############################################################################
# Test warping a full EPSG:4326 extent to +proj=sinu (#2305)


@pytest.mark.require_creation_option("GTiff", "JPEG")
def test_gdalwarp_25(gdalwarp_path):

    gdaltest.runexternal(
        gdalwarp_path + ' -t_srs "+proj=sinu" data/w_jpeg.tiff tmp/testgdalwarp25.tif'
    )

    ds = gdal.Open("tmp/testgdalwarp25.tif")
    assert ds is not None

    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 8016 or cs == 6157, "Bad checksum"

    gt = ds.GetGeoTransform()
    expected_gt = [
        -20037508.342789248,
        78245.302611923355,
        0.0,
        10001965.729313632,
        0.0,
        -77939.656898595524,
    ]
    for i in range(6):
        assert gt[i] == pytest.approx(expected_gt[i], abs=1), "Bad gt"

    ds = None


###############################################################################
# Test warping a full EPSG:4326 extent to +proj=eck4 (#2305)


@pytest.mark.require_creation_option("GTiff", "JPEG")
def test_gdalwarp_26(gdalwarp_path):

    gdaltest.runexternal(
        gdalwarp_path + ' -t_srs "+proj=eck4" data/w_jpeg.tiff tmp/testgdalwarp26.tif'
    )

    ds = gdal.Open("tmp/testgdalwarp26.tif")
    assert ds is not None

    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 8582 or cs == 3938, "Bad checksum"

    gt = ds.GetGeoTransform()
    expected_gt = [
        -16921202.922943164,
        41752.719393322564,
        0.0,
        8460601.4614715818,
        0.0,
        -41701.109109770863,
    ]
    for i in range(6):
        assert gt[i] == pytest.approx(expected_gt[i], abs=1), "Bad gt"

    ds = None


###############################################################################
# Test warping a full EPSG:4326 extent to +proj=vandg (#2305)


@pytest.mark.require_creation_option("GTiff", "JPEG")
def test_gdalwarp_27(gdalwarp_path):

    gdaltest.runexternal(
        gdalwarp_path
        + ' -t_srs "+proj=vandg" data/w_jpeg.tiff tmp/testgdalwarp27.tif -overwrite'
    )

    ds = gdal.Open("tmp/testgdalwarp27.tif")
    assert ds is not None

    cs = ds.GetRasterBand(1).Checksum()
    # 22615 for MacOSX
    assert cs == 22006 or cs == 22615, "Bad checksum"

    gt = ds.GetGeoTransform()
    expected_gt = [
        -20015109.356056381,
        98651.645855415176,
        0.0,
        20015109.356056374,
        0.0,
        -98651.645855415176,
    ]
    for i in range(6):
        assert gt[i] == pytest.approx(expected_gt[i], abs=1), "Bad gt"

    ds = None


###############################################################################
# Test warping a full EPSG:4326 extent to +proj=aeqd +lat_0=45 +lon_0=90 (#2305)


@pytest.mark.require_creation_option("GTiff", "JPEG")
def test_gdalwarp_28(gdalwarp_path):

    gdaltest.runexternal(
        gdalwarp_path
        + ' -t_srs "+proj=aeqd +lat_0=45 +lon_0=90" data/w_jpeg.tiff tmp/testgdalwarp28.tif'
    )

    ds = gdal.Open("tmp/testgdalwarp28.tif")
    assert ds is not None

    # Check that there is no hole at the south pole location
    cs = ds.GetRasterBand(1).Checksum()
    # 1219 for MacOSX
    assert cs in (1794, 1219), "Bad checksum"

    gt = ds.GetGeoTransform()
    expected_gt1 = [
        -18494092.97555049,
        93907.15126464187,
        0.0,
        20003931.458625447,
        0.0,
        -93907.15126464187,
    ]
    for i in range(6):
        assert gt[i] == pytest.approx(expected_gt1[i], abs=1), ("Bad gt", gt)

    ds = None


###############################################################################
# Test warping a full EPSG:4326 extent to EPSG:3785 (#2305)


@pytest.mark.require_creation_option("GTiff", "JPEG")
def DISABLED_test_gdalwarp_29(gdalwarp_path):

    # This test has been disabled since PROJ 8 will reproject a coordinates at
    # lat=90 to a finite value, due to 90deg being < PI/2 due to numerical
    # accuracy

    gdaltest.runexternal(
        gdalwarp_path + " -t_srs EPSG:3785 data/w_jpeg.tiff tmp/testgdalwarp29.tif"
    )

    ds = gdal.Open("tmp/testgdalwarp29.tif")
    assert ds is not None

    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 55149 or cs == 56054, "Bad checksum"

    gt = ds.GetGeoTransform()
    expected_gt = [
        -20037508.342789248,
        90054.726863985939,
        0.0,
        16213801.067583967,
        0.0,
        -90056.750611190684,
    ]
    for i in range(6):
        assert gt[i] == pytest.approx(expected_gt[i], abs=1), "Bad gt"

    ds = None


###############################################################################
# Test the effect of the -wo OPTIMIZE_SIZE=TRUE and -wo STREAMABLE_OUTPUT=TRUE options (#3459, #1866)


@pytest.mark.require_creation_option("GTiff", "JPEG")
def test_gdalwarp_30(gdalwarp_path):

    te = " -te -20037508.343 -16206629.152 20036845.112 16213801.068"

    # First run : no parameter
    gdaltest.runexternal(
        gdalwarp_path
        + " data/w_jpeg.tiff tmp/testgdalwarp30_1.tif  -t_srs EPSG:3785 -co COMPRESS=LZW -wm 500000  --config GDAL_CACHEMAX 1 -ts 1000 500 -co TILED=YES"
        + te
    )

    # Second run : with  -wo OPTIMIZE_SIZE=TRUE
    gdaltest.runexternal(
        gdalwarp_path
        + " data/w_jpeg.tiff tmp/testgdalwarp30_2.tif  -t_srs EPSG:3785 -co COMPRESS=LZW -wm 500000 -wo OPTIMIZE_SIZE=TRUE  --config GDAL_CACHEMAX 1 -ts 1000 500 -co TILED=YES"
        + te
    )

    # Third run : with  -wo STREAMABLE_OUTPUT=TRUE
    gdaltest.runexternal(
        gdalwarp_path
        + " data/w_jpeg.tiff tmp/testgdalwarp30_3.tif  -t_srs EPSG:3785 -co COMPRESS=LZW -wm 500000 -wo STREAMABLE_OUTPUT=TRUE  --config GDAL_CACHEMAX 1 -ts 1000 500 -co TILED=YES"
        + te
    )

    file_size1 = os.stat("tmp/testgdalwarp30_1.tif")[stat.ST_SIZE]
    file_size2 = os.stat("tmp/testgdalwarp30_2.tif")[stat.ST_SIZE]
    file_size3 = os.stat("tmp/testgdalwarp30_3.tif")[stat.ST_SIZE]

    ds = gdal.Open("tmp/testgdalwarp30_1.tif")
    assert ds is not None

    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 64629 or cs == 1302, "Bad checksum on testgdalwarp30_1"

    ds = None

    ds = gdal.Open("tmp/testgdalwarp30_2.tif")
    assert ds is not None

    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 64629 or cs == 1302, "Bad checksum on testgdalwarp30_2"

    ds = None

    ds = gdal.Open("tmp/testgdalwarp30_3.tif")
    assert ds is not None

    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 64629 or cs == 1302, "Bad checksum on testgdalwarp30_3"

    ds = None

    assert (
        file_size1 > file_size2
    ), "Size with -wo OPTIMIZE_SIZE=TRUE larger than without !"

    assert (
        file_size1 > file_size3
    ), "Size with -wo STREAMABLE_OUTPUT=TRUE larger than without !"


###############################################################################
# Test -overwrite (#3759)


def test_gdalwarp_31(gdalwarp_path):

    gdaltest.runexternal(
        gdalwarp_path + " ../gcore/data/byte.tif tmp/testgdalwarp31.tif"
    )

    ds = gdal.Open("tmp/testgdalwarp31.tif")
    cs1 = ds.GetRasterBand(1).Checksum()
    ds = None

    (_, err) = gdaltest.runexternal_out_and_err(
        gdalwarp_path
        + " ../gcore/data/byte.tif tmp/testgdalwarp31.tif -t_srs EPSG:4326"
    )

    ds = gdal.Open("tmp/testgdalwarp31.tif")
    cs2 = ds.GetRasterBand(1).Checksum()
    ds = None

    (_, err2) = gdaltest.runexternal_out_and_err(
        gdalwarp_path
        + " ../gcore/data/byte.tif tmp/testgdalwarp31.tif -t_srs EPSG:4326 -overwrite"
    )

    ds = gdal.Open("tmp/testgdalwarp31.tif")
    cs3 = ds.GetRasterBand(1).Checksum()
    ds = None

    assert cs1 == 4672 and cs2 == 4672 and cs3 == 4727 and err != "" and err2 == ""


###############################################################################
# Test warping a JPEG compressed image with a mask into a RGBA image


@pytest.mark.require_creation_option("GTiff", "JPEG")
def test_gdalwarp_33(gdalwarp_path):

    if test_cli_utilities.get_gdal_translate_path() is None:
        pytest.skip("gdal_translate missing")

    gdaltest.runexternal(
        gdalwarp_path
        + " -dstalpha ../gcore/data/ycbcr_with_mask.tif tmp/testgdalwarp33.tif"
    )

    src_ds = gdal.Open("../gcore/data/ycbcr_with_mask.tif")
    ds = gdal.Open("tmp/testgdalwarp33.tif")
    assert ds is not None

    # There are expected diffs because of the artifacts due to JPEG compression in 8x8 blocks
    # that are partially masked. gdalwarp will remove those artifacts
    max_diff = gdaltest.compare_ds(src_ds, ds)
    assert max_diff <= 40

    src_ds = None

    gdaltest.runexternal(
        test_cli_utilities.get_gdal_translate_path()
        + " -expand gray GTIFF_DIR:2:../gcore/data/ycbcr_with_mask.tif tmp/testgdalwarp33_mask.tif"
    )

    mask_ds = gdal.Open("tmp/testgdalwarp33_mask.tif")
    expected_cs = mask_ds.GetRasterBand(1).Checksum()
    mask_ds = None

    cs = ds.GetRasterBand(4).Checksum()

    ds = None

    assert cs == expected_cs, "did not get expected checksum on alpha band"


###############################################################################
# Test warping multiple sources


def test_gdalwarp_34(gdalwarp_path):

    if test_cli_utilities.get_gdal_translate_path() is None:
        pytest.skip("gdal_translate missing")

    try:
        os.remove("tmp/testgdalwarp34.tif")
    except OSError:
        pass

    gdaltest.runexternal(
        test_cli_utilities.get_gdal_translate_path()
        + " ../gcore/data/byte.tif tmp/testgdalwarp34src_1.tif -srcwin 0 0 10 20"
    )
    gdaltest.runexternal(
        test_cli_utilities.get_gdal_translate_path()
        + " ../gcore/data/byte.tif tmp/testgdalwarp34src_2.tif -srcwin 10 0 10 20"
    )
    gdaltest.runexternal(
        gdalwarp_path
        + " tmp/testgdalwarp34src_1.tif tmp/testgdalwarp34src_2.tif tmp/testgdalwarp34.tif"
    )
    os.remove("tmp/testgdalwarp34src_1.tif")
    os.remove("tmp/testgdalwarp34src_2.tif")

    ds = gdal.Open("tmp/testgdalwarp34.tif")
    cs = ds.GetRasterBand(1).Checksum()
    gt = ds.GetGeoTransform()
    xsize = ds.RasterXSize
    ysize = ds.RasterYSize
    ds = None

    os.remove("tmp/testgdalwarp34.tif")

    assert xsize == 20 and ysize == 20, "bad dimensions"

    assert cs == 4672, "bad checksum"

    expected_gt = (440720.0, 60.0, 0.0, 3751320.0, 0.0, -60.0)
    for i in range(6):
        assert gt[i] == pytest.approx(expected_gt[i], abs=1e-5), "bad gt"


###############################################################################
# Test -ts and -te optimization (doesn't need calling GDALSuggestedWarpOutput2, #4804)


def test_gdalwarp_35(gdalwarp_path):

    gdaltest.runexternal(
        gdalwarp_path
        + " -ts 20 20 -te 440720.000 3750120.000 441920.000 3751320.000 ../gcore/data/byte.tif tmp/testgdalwarp35.tif"
    )

    ds = gdal.Open("tmp/testgdalwarp35.tif")
    assert ds is not None

    gdaltest.check_geotransform(
        gdal.Open("../gcore/data/byte.tif").GetGeoTransform(),
        ds.GetGeoTransform(),
        1e-9,
    )

    ds = None


###############################################################################
# Test -tr and -te optimization (doesn't need calling GDALSuggestedWarpOutput2, #4804)


def test_gdalwarp_36(gdalwarp_path):

    gdaltest.runexternal(
        gdalwarp_path
        + " -tr 60 60 -te 440720.000 3750120.000 441920.000 3751320.000 ../gcore/data/byte.tif tmp/testgdalwarp36.tif"
    )

    ds = gdal.Open("tmp/testgdalwarp36.tif")
    assert ds is not None

    gdaltest.check_geotransform(
        gdal.Open("../gcore/data/byte.tif").GetGeoTransform(),
        ds.GetGeoTransform(),
        1e-9,
    )

    ds = None


###############################################################################
# Test metadata copying - stats should not be copied (#5319)


def test_gdalwarp_37(gdalwarp_path):

    gdaltest.runexternal(
        gdalwarp_path + " -tr 60 60 ./data/utmsmall.tif tmp/testgdalwarp37.tif"
    )

    ds = gdal.Open("tmp/testgdalwarp37.tif")
    assert ds is not None

    md = ds.GetRasterBand(1).GetMetadata()

    # basic metadata test
    assert "testkey" in md and md["testkey"] == "test value", (
        "Output file metadata is wrong : { %s }" % md
    )

    # make sure stats not copied
    assert "STATISTICS_MEAN" not in md, "Output file contains statistics metadata"

    assert ds.GetRasterBand(1).GetMinimum() is None, "Output file has statistics"

    ds = None


###############################################################################
# Test implicit nodata setting (#5675)


@pytest.mark.require_driver("AAIGRID")
def test_gdalwarp_38(gdalwarp_path):

    gdaltest.runexternal(gdalwarp_path + " data/withnodata.asc tmp/testgdalwarp38.tif")

    ds = gdal.Open("tmp/testgdalwarp38.tif")
    assert ds.GetRasterBand(1).Checksum() == 65531
    assert ds.GetRasterBand(1).GetNoDataValue() == -999
    ds = None


###############################################################################
# Test -oo


@pytest.mark.require_driver("AAIGRID")
def test_gdalwarp_39(gdalwarp_path):

    gdaltest.runexternal(
        gdalwarp_path
        + " ../gdrivers/data/aaigrid/float64.asc tmp/test_gdalwarp_39.tif -oo DATATYPE=Float64 -overwrite"
    )

    ds = gdal.Open("tmp/test_gdalwarp_39.tif")
    assert ds.GetRasterBand(1).DataType == gdal.GDT_Float64
    ds = None


###############################################################################
# Test -ovr


def test_gdalwarp_40(gdalwarp_path):

    src_ds = gdal.Open("../gcore/data/byte.tif")
    out_ds = gdal.GetDriverByName("GTiff").CreateCopy(
        "tmp/test_gdalwarp_40_src.tif", src_ds
    )
    cs_main = out_ds.GetRasterBand(1).Checksum()
    out_ds.BuildOverviews("NONE", overviewlist=[2, 4])
    out_ds.GetRasterBand(1).GetOverview(0).Fill(127)
    cs_ov0 = out_ds.GetRasterBand(1).GetOverview(0).Checksum()
    out_ds.GetRasterBand(1).GetOverview(1).Fill(255)
    cs_ov1 = out_ds.GetRasterBand(1).GetOverview(1).Checksum()
    out_ds = None

    # Should select main resolution
    gdaltest.runexternal(
        gdalwarp_path
        + " tmp/test_gdalwarp_40_src.tif tmp/test_gdalwarp_40.tif -overwrite"
    )

    ds = gdal.Open("tmp/test_gdalwarp_40.tif")
    assert ds.GetRasterBand(1).Checksum() == cs_main
    ds = None

    # Test -ovr AUTO. Should select main resolution
    gdaltest.runexternal(
        gdalwarp_path
        + " tmp/test_gdalwarp_40_src.tif tmp/test_gdalwarp_40.tif -overwrite -ovr AUTO"
    )

    ds = gdal.Open("tmp/test_gdalwarp_40.tif")
    assert ds.GetRasterBand(1).Checksum() == cs_main
    ds = None

    gdaltest.runexternal(
        gdalwarp_path
        + " ../gcore/data/byte.tif tmp/test_gdalwarp_40.tif -overwrite -ts 5 5"
    )
    ds = gdal.Open("tmp/test_gdalwarp_40.tif")
    expected_cs = ds.GetRasterBand(1).Checksum()
    ds = None

    # Test -ovr NONE. Should select main resolution too
    gdaltest.runexternal(
        gdalwarp_path
        + " tmp/test_gdalwarp_40_src.tif tmp/test_gdalwarp_40.tif -overwrite -ovr NONE -ts 5 5"
    )

    ds = gdal.Open("tmp/test_gdalwarp_40.tif")
    assert ds.GetRasterBand(1).Checksum() == expected_cs
    ds = None

    gdaltest.runexternal(
        gdalwarp_path
        + " ../gcore/data/byte.tif tmp/test_gdalwarp_40.tif -overwrite -ts 15 15"
    )
    ds = gdal.Open("tmp/test_gdalwarp_40.tif")
    expected_cs = ds.GetRasterBand(1).Checksum()
    ds = None

    # Should select main resolution too
    gdaltest.runexternal(
        gdalwarp_path
        + " tmp/test_gdalwarp_40_src.tif tmp/test_gdalwarp_40.tif -overwrite -ts 15 15"
    )

    ds = gdal.Open("tmp/test_gdalwarp_40.tif")
    assert ds.GetRasterBand(1).Checksum() == expected_cs
    ds = None

    # Should select overview 0
    gdaltest.runexternal(
        gdalwarp_path
        + " tmp/test_gdalwarp_40_src.tif tmp/test_gdalwarp_40.tif -overwrite -ts 10 10"
    )

    ds = gdal.Open("tmp/test_gdalwarp_40.tif")
    assert ds.GetRasterBand(1).Checksum() == cs_ov0
    ds = None

    # Should select overview 0 through VRT
    gdaltest.runexternal(
        gdalwarp_path
        + " tmp/test_gdalwarp_40_src.tif tmp/test_gdalwarp_40.vrt -overwrite -ts 10 10 -of VRT"
    )

    ds = gdal.Open("tmp/test_gdalwarp_40.vrt")
    assert ds.GetRasterBand(1).Checksum() == cs_ov0
    ds = None

    # Should select overview 0 through VRT
    gdaltest.runexternal(
        gdalwarp_path
        + " tmp/test_gdalwarp_40_src.tif tmp/test_gdalwarp_40.vrt -overwrite -ts 10 10 -te 440720 3750120 441920 3751320 -of VRT"
    )

    ds = gdal.Open("tmp/test_gdalwarp_40.vrt")
    assert ds.GetRasterBand(1).Checksum() == cs_ov0
    ds = None

    gdaltest.runexternal(
        gdalwarp_path
        + " tmp/test_gdalwarp_40_src.tif -oo OVERVIEW_LEVEL=0 tmp/test_gdalwarp_40.tif -overwrite -ts 7 7"
    )
    ds = gdal.Open("tmp/test_gdalwarp_40.tif")
    expected_cs = ds.GetRasterBand(1).Checksum()
    ds = None

    # Should select overview 0 too
    gdaltest.runexternal(
        gdalwarp_path
        + " tmp/test_gdalwarp_40_src.tif tmp/test_gdalwarp_40.tif -overwrite -ts 7 7"
    )

    ds = gdal.Open("tmp/test_gdalwarp_40.tif")
    assert ds.GetRasterBand(1).Checksum() == expected_cs
    ds = None

    gdaltest.runexternal(
        gdalwarp_path
        + " tmp/test_gdalwarp_40_src.tif -ovr NONE -oo OVERVIEW_LEVEL=0 tmp/test_gdalwarp_40.tif -overwrite -ts 5 5"
    )
    ds = gdal.Open("tmp/test_gdalwarp_40.tif")
    expected_cs = ds.GetRasterBand(1).Checksum()
    ds = None

    # Test AUTO-n. Should select overview 0 too
    gdaltest.runexternal(
        gdalwarp_path
        + " tmp/test_gdalwarp_40_src.tif tmp/test_gdalwarp_40.tif -overwrite -ts 5 5 -ovr AUTO-1"
    )

    ds = gdal.Open("tmp/test_gdalwarp_40.tif")
    assert ds.GetRasterBand(1).Checksum() == expected_cs
    ds = None

    # Should select overview 1
    gdaltest.runexternal(
        gdalwarp_path
        + " tmp/test_gdalwarp_40_src.tif tmp/test_gdalwarp_40.tif -overwrite -ts 5 5"
    )

    ds = gdal.Open("tmp/test_gdalwarp_40.tif")
    assert ds.GetRasterBand(1).Checksum() == cs_ov1
    ds = None

    gdaltest.runexternal(
        gdalwarp_path
        + " tmp/test_gdalwarp_40_src.tif -oo OVERVIEW_LEVEL=1 tmp/test_gdalwarp_40.tif -overwrite -ts 3 3"
    )
    ds = gdal.Open("tmp/test_gdalwarp_40.tif")
    expected_cs = ds.GetRasterBand(1).Checksum()
    ds = None

    # Should select overview 1 too
    gdaltest.runexternal(
        gdalwarp_path
        + " tmp/test_gdalwarp_40_src.tif tmp/test_gdalwarp_40.tif -overwrite -ts 3 3"
    )

    ds = gdal.Open("tmp/test_gdalwarp_40.tif")
    assert ds.GetRasterBand(1).Checksum() == expected_cs
    ds = None

    gdaltest.runexternal(
        gdalwarp_path
        + " tmp/test_gdalwarp_40_src.tif -oo OVERVIEW_LEVEL=1 tmp/test_gdalwarp_40.tif -overwrite -ts 20 20"
    )
    ds = gdal.Open("tmp/test_gdalwarp_40.tif")
    expected_cs = ds.GetRasterBand(1).Checksum()
    ds = None

    # Specify a level >= number of overviews. Should select overview 1 too
    gdaltest.runexternal(
        gdalwarp_path
        + " tmp/test_gdalwarp_40_src.tif tmp/test_gdalwarp_40.tif -overwrite -ovr 5"
    )

    ds = gdal.Open("tmp/test_gdalwarp_40.tif")
    assert ds.GetRasterBand(1).Checksum() == expected_cs
    ds = None


###############################################################################
# Test source fill ratio heuristics (#3120)
# Also check that we guess a reasonable resolution (#2754), from the source
# dataset and target extent


def test_gdalwarp_41(gdalwarp_path):

    src_ds = gdal.GetDriverByName("GTiff").Create(
        "tmp/test_gdalwarp_41_src.tif", 666, 666
    )
    src_ds.SetGeoTransform(
        [
            -3333500,
            10010.510510510510358,
            0,
            3333500.000000000000000,
            0,
            -10010.510510510510358,
        ]
    )
    src_ds.SetProjection(
        """PROJCS["WGS_1984_Stereographic_South_Pole",
    GEOGCS["WGS 84",
        DATUM["WGS_1984",
            SPHEROID["WGS 84",6378137,298.257223563,
                AUTHORITY["EPSG","7030"]],
            AUTHORITY["EPSG","6326"]],
        PRIMEM["Greenwich",0],
        UNIT["degree",0.0174532925199433],
        AUTHORITY["EPSG","4326"]],
    PROJECTION["Polar_Stereographic"],
    PARAMETER["latitude_of_origin",-71],
    PARAMETER["central_meridian",0],
    PARAMETER["scale_factor",1],
    PARAMETER["false_easting",0],
    PARAMETER["false_northing",0],
    UNIT["metre",1,
        AUTHORITY["EPSG","9001"]]]"""
    )
    src_ds.GetRasterBand(1).Fill(255)
    src_ds = None

    # Check when source fill ratio heuristics is ON
    gdaltest.runexternal(
        gdalwarp_path
        + " tmp/test_gdalwarp_41_src.tif tmp/test_gdalwarp_41.tif -overwrite  -t_srs EPSG:4326 -te -180 -90 180 90  -wo INIT_DEST=127 -wo SKIP_NOSOURCE=YES"
    )

    ds = gdal.Open("tmp/test_gdalwarp_41.tif")
    assert ds.RasterXSize == 2052
    assert ds.RasterYSize == 1026
    assert ds.GetRasterBand(1).Checksum() == 57091
    ds = None

    # Check when source fill ratio heuristics is OFF
    gdaltest.runexternal(
        gdalwarp_path
        + " tmp/test_gdalwarp_41_src.tif tmp/test_gdalwarp_41.tif -overwrite  -t_srs EPSG:4326 -te -180 -90 180 90  -wo INIT_DEST=127 -wo SKIP_NOSOURCE=YES -wo SRC_FILL_RATIO_HEURISTICS=NO"
    )

    ds = gdal.Open("tmp/test_gdalwarp_41.tif")
    assert ds.GetRasterBand(1).Checksum() == 31890
    ds = None


###############################################################################
# Test warping multiple source images, in one step or several, with INIT_DEST/nodata (#5909, #5387)


def test_gdalwarp_42(gdalwarp_path):
    if test_cli_utilities.get_gdal_translate_path() is None:
        pytest.skip("gdal_translate missing")

    gdaltest.runexternal(
        test_cli_utilities.get_gdal_translate_path()
        + " ../gdrivers/data/small_world.tif tmp/small_world_left.tif -srcwin 0 0 200 200 -a_nodata 255"
    )
    gdaltest.runexternal(
        test_cli_utilities.get_gdal_translate_path()
        + " ../gdrivers/data/small_world.tif tmp/small_world_right.tif -srcwin 200 0 200 200  -a_nodata 255"
    )

    gdaltest.runexternal(
        gdalwarp_path
        + " tmp/small_world_left.tif tmp/test_gdalwarp_42.tif -overwrite -te -180 -90 180 90 -dstalpha -wo UNIFIED_SRC_NODATA=YES"
    )
    gdaltest.runexternal(
        gdalwarp_path
        + " tmp/small_world_right.tif tmp/test_gdalwarp_42.tif -wo UNIFIED_SRC_NODATA=YES"
    )

    ds = gdal.Open("tmp/test_gdalwarp_42.tif")
    got_cs = [ds.GetRasterBand(i + 1).Checksum() for i in range(4)]
    expected_cs = [25382, 27573, 35297, 59540]
    assert got_cs == expected_cs
    ds = None

    # In one step
    gdaltest.runexternal(
        gdalwarp_path
        + " tmp/small_world_left.tif tmp/small_world_right.tif tmp/test_gdalwarp_42.tif -overwrite -te -180 -90 180 90 -dstalpha -wo UNIFIED_SRC_NODATA=YES"
    )

    ds = gdal.Open("tmp/test_gdalwarp_42.tif")
    got_cs = [ds.GetRasterBand(i + 1).Checksum() for i in range(4)]
    expected_cs = [25382, 27573, 35297, 59540]
    assert got_cs == expected_cs
    ds = None

    # In one step with -wo INIT_DEST=255,255,255,0
    gdaltest.runexternal(
        gdalwarp_path
        + " tmp/small_world_left.tif tmp/small_world_right.tif tmp/test_gdalwarp_42.tif -wo INIT_DEST=255,255,255,0 -overwrite -te -180 -90 180 90 -dstalpha -wo UNIFIED_SRC_NODATA=YES"
    )

    ds = gdal.Open("tmp/test_gdalwarp_42.tif")
    got_cs = [ds.GetRasterBand(i + 1).Checksum() for i in range(4)]
    expected_cs = [30111, 32302, 40026, 59540]
    assert got_cs == expected_cs
    ds = None

    # In one step with -wo INIT_DEST=0,0,0,0
    # Different checksum since there are source pixels at 255, so they get remap to 0
    gdaltest.runexternal(
        gdalwarp_path
        + " tmp/small_world_left.tif tmp/small_world_right.tif tmp/test_gdalwarp_42.tif -wo INIT_DEST=0,0,0,0 -overwrite -te -180 -90 180 90 -dstalpha -wo UNIFIED_SRC_NODATA=YES"
    )

    ds = gdal.Open("tmp/test_gdalwarp_42.tif")
    got_cs = [ds.GetRasterBand(i + 1).Checksum() for i in range(4)]
    expected_cs = [25382, 27573, 35297, 59540]
    assert got_cs == expected_cs
    ds = None


###############################################################################
# Test that NODATA_VALUES is honoured, but not transferred when adding an alpha channel.


def test_gdalwarp_43(gdalwarp_path):
    if test_cli_utilities.get_gdal_translate_path() is None:
        pytest.skip("gdal_translate missing")

    gdaltest.runexternal(
        test_cli_utilities.get_gdal_translate_path()
        + ' ../gdrivers/data/small_world.tif tmp/small_world.tif -mo "FOO=BAR" -mo "NODATA_VALUES=62 93 23"'
    )

    gdaltest.runexternal(
        gdalwarp_path
        + " tmp/small_world.tif tmp/test_gdalwarp_43.tif -overwrite -dstalpha"
    )

    ds = gdal.Open("tmp/test_gdalwarp_43.tif")
    assert ds.GetMetadataItem("NODATA_VALUES") is None
    assert ds.GetMetadataItem("FOO") == "BAR"
    got_cs = [ds.GetRasterBand(i + 1).Checksum() for i in range(4)]
    expected_cs = [30106, 32285, 40022, 64261]
    assert got_cs == expected_cs


###############################################################################
# Test effect of -wo SRC_COORD_PRECISION


def test_gdalwarp_44(gdalwarp_path):

    # Without  -wo SRC_COORD_PRECISION
    gdaltest.runexternal(
        gdalwarp_path
        + " -q ../gcore/data/byte.tif tmp/test_gdalwarp_44.tif -wm 10 -overwrite -ts 500 500 -r cubic -ot float32 -t_srs EPSG:4326"
    )
    ds = gdal.Open("tmp/test_gdalwarp_44.tif")
    cs1 = ds.GetRasterBand(1).Checksum()
    ds = None

    gdaltest.runexternal(
        gdalwarp_path
        + " -q ../gcore/data/byte.tif tmp/test_gdalwarp_44.tif -wm 0.1 -overwrite -ts 500 500 -r cubic -ot float32 -t_srs EPSG:4326"
    )
    ds = gdal.Open("tmp/test_gdalwarp_44.tif")
    cs2 = ds.GetRasterBand(1).Checksum()
    ds = None

    if cs1 == cs2:
        print("Unexpected cs1 == cs2")

    # With  -wo SRC_COORD_PRECISION
    gdaltest.runexternal(
        gdalwarp_path
        + " -q ../gcore/data/byte.tif tmp/test_gdalwarp_44.tif -wm 10 -et 0.01 -wo SRC_COORD_PRECISION=0.1 -overwrite -ts 500 500 -r cubic -ot float32 -t_srs EPSG:4326"
    )
    ds = gdal.Open("tmp/test_gdalwarp_44.tif")
    cs3 = ds.GetRasterBand(1).Checksum()
    ds = None

    gdaltest.runexternal(
        gdalwarp_path
        + " -q ../gcore/data/byte.tif tmp/test_gdalwarp_44.tif -wm 0.1 -et 0.01 -wo SRC_COORD_PRECISION=0.1 -overwrite -ts 500 500 -r cubic -ot float32 -t_srs EPSG:4326"
    )
    ds = gdal.Open("tmp/test_gdalwarp_44.tif")
    cs4 = ds.GetRasterBand(1).Checksum()
    ds = None

    assert cs3 == cs4


###############################################################################
# Test -te_srs


def test_gdalwarp_45(gdalwarp_path):

    gdaltest.runexternal(
        gdalwarp_path
        + " -te_srs EPSG:4267 -te -117.641087629972 33.8915301685897 -117.628190189534 33.9024195619201 ../gcore/data/byte.tif tmp/test_gdalwarp_45.tif -overwrite"
    )

    ds = gdal.Open("tmp/test_gdalwarp_45.tif")
    assert ds.GetRasterBand(1).Checksum() == 4672

    ds = None

    gdaltest.runexternal(
        gdalwarp_path
        + " -te_srs EPSG:4267 -te -117.641087629972 33.8915301685897 -117.628190189534 33.9024195619201 -t_srs EPSG:32611 ../gcore/data/byte.tif tmp/test_gdalwarp_45.tif -overwrite"
    )

    ds = gdal.Open("tmp/test_gdalwarp_45.tif")
    assert ds.GetRasterBand(1).Checksum() == 4672

    ds = None


###############################################################################
# Test -crop_to_cutline


@pytest.mark.require_driver("CSV")
def test_gdalwarp_46(gdalwarp_path):
    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip("ogr2ogr missing")

    gdaltest.runexternal(
        gdalwarp_path
        + " ../gcore/data/utmsmall.tif tmp/test_gdalwarp_46.tif -cutline data/cutline.vrt -crop_to_cutline -overwrite"
    )

    ds = gdal.Open("tmp/test_gdalwarp_46.tif")
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 18837, "Bad checksum"

    ds = None

    # With explicit -s_srs and -t_srs
    gdaltest.runexternal(
        gdalwarp_path
        + " ../gcore/data/utmsmall.tif tmp/test_gdalwarp_46.tif -cutline data/cutline.vrt -crop_to_cutline -overwrite -s_srs EPSG:26711 -t_srs EPSG:26711"
    )

    ds = gdal.Open("tmp/test_gdalwarp_46.tif")
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 18837, "Bad checksum"

    ds = None

    # With cutline in another SRS
    gdaltest.runexternal(
        test_cli_utilities.get_ogr2ogr_path()
        + " tmp/cutline_4326.shp data/cutline.vrt -s_srs EPSG:26711 -t_srs EPSG:4326"
    )
    gdaltest.runexternal(
        gdalwarp_path
        + " ../gcore/data/utmsmall.tif tmp/test_gdalwarp_46.tif -cutline tmp/cutline_4326.shp -crop_to_cutline -overwrite -t_srs EPSG:32711"
    )

    ds = gdal.Open("tmp/test_gdalwarp_46.tif")
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 19582, "Bad checksum"

    ds = None


###############################################################################
# Test gdalwarp -co APPEND_SUBDATASET=YES


def test_gdalwarp_47_append_subdataset(gdalwarp_path):

    tmpfilename = "tmp/test_gdalwarp_47_append_subdataset.tif"
    gdal.Translate(tmpfilename, "../gcore/data/byte.tif")
    gdaltest.runexternal(
        gdalwarp_path
        + " -co APPEND_SUBDATASET=YES ../gcore/data/utmsmall.tif "
        + tmpfilename
    )

    ds = gdal.Open("GTIFF_DIR:2:" + tmpfilename)
    assert ds.GetRasterBand(1).Checksum() == 50054

    ds = None
    gdal.Unlink(tmpfilename)


###############################################################################
# Test -if option


def test_gdalwarp_if_option(gdalwarp_path):

    ret, err = gdaltest.runexternal_out_and_err(
        gdalwarp_path + " -if GTiff ../gcore/data/byte.tif /vsimem/out.tif"
    )
    assert err is None or err == ""

    _, err = gdaltest.runexternal_out_and_err(
        gdalwarp_path
        + " -if invalid_driver_name ../gcore/data/byte.tif /vsimem/out.tif"
    )
    assert err is not None
    assert "invalid_driver_name" in err

    _, err = gdaltest.runexternal_out_and_err(
        gdalwarp_path + " -if HFA ../gcore/data/byte.tif /vsimem/out.tif"
    )
    assert err is not None


###############################################################################
# Cleanup


def test_gdalwarp_cleanup():

    # We don't clean up when run in debug mode.
    if gdal.GetConfigOption("CPL_DEBUG", "OFF") == "ON":
        return

    for i in range(37):
        try:
            os.remove("tmp/testgdalwarp" + str(i + 1) + ".tif")
        except OSError:
            pass
        try:
            os.remove("tmp/testgdalwarp" + str(i + 1) + ".vrt")
        except OSError:
            pass
        try:
            os.remove("tmp/testgdalwarp" + str(i + 1) + ".tif.aux.xml")
        except OSError:
            pass
    try:
        os.remove("tmp/testgdalwarp_gcp.tif")
    except OSError:
        pass
    try:
        os.remove("tmp/testgdalwarp24src.tif")
    except OSError:
        pass
    try:
        os.remove("tmp/testgdalwarp24dst.tif")
    except OSError:
        pass
    try:
        os.remove("tmp/testgdalwarp30_1.tif")
    except OSError:
        pass
    try:
        os.remove("tmp/testgdalwarp30_2.tif")
    except OSError:
        pass
    try:
        os.remove("tmp/testgdalwarp30_3.tif")
    except OSError:
        pass
    try:
        os.remove("tmp/testgdalwarp33_mask.tif")
    except OSError:
        pass
    try:
        os.remove("tmp/testgdalwarp37.tif")
    except OSError:
        pass
    try:
        os.remove("tmp/testgdalwarp38.tif")
    except OSError:
        pass
    try:
        os.remove("tmp/test_gdalwarp_39.tif")
    except OSError:
        pass
    try:
        os.remove("tmp/test_gdalwarp_40_src.tif")
        os.remove("tmp/test_gdalwarp_40.tif")
        os.remove("tmp/test_gdalwarp_40.vrt")
    except OSError:
        pass
    try:
        os.remove("tmp/test_gdalwarp_41_src.tif")
        os.remove("tmp/test_gdalwarp_41.tif")
    except OSError:
        pass
    try:
        os.remove("tmp/small_world_left.tif")
        os.remove("tmp/small_world_right.tif")
        os.remove("tmp/test_gdalwarp_42.tif")
    except OSError:
        pass
    try:
        os.remove("tmp/small_world.tif")
        os.remove("tmp/test_gdalwarp_43.tif")
    except OSError:
        pass
    try:
        os.remove("tmp/test_gdalwarp_44.tif")
    except OSError:
        pass
    try:
        os.remove("tmp/test_gdalwarp_45.tif")
    except OSError:
        pass
    try:
        os.remove("tmp/test_gdalwarp_46.tif")
    except OSError:
        pass
    try:
        os.remove("tmp/cutline_4326.shp")
        os.remove("tmp/cutline_4326.shx")
        os.remove("tmp/cutline_4326.dbf")
        os.remove("tmp/cutline_4326.prj")
    except OSError:
        pass
