#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdalwarp testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2008-2013, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import os
import shutil
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


def test_gdalwarp_1(gdalwarp_path, tmp_path):

    dst_tif = str(tmp_path / "testgdalwarp1.tif")

    _, err = gdaltest.runexternal_out_and_err(
        f"{gdalwarp_path} ../gcore/data/byte.tif {dst_tif}"
    )
    assert err is None or err == "", "got error/warning"

    ds = gdal.Open(dst_tif)
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 4672, "Bad checksum"

    ds = None


###############################################################################
# Test -of option


def test_gdalwarp_2(gdalwarp_path, tmp_path):

    dst_tif = str(tmp_path / "testgdalwarp2.tif")

    gdaltest.runexternal(f"{gdalwarp_path} -of GTiff ../gcore/data/byte.tif {dst_tif}")

    ds = gdal.Open(dst_tif)
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 4672, "Bad checksum"

    ds = None


###############################################################################
# Test -ot option


def test_gdalwarp_3(gdalwarp_path, tmp_path):

    dst_tif = str(tmp_path / "testgdalwarp3.tif")

    gdaltest.runexternal(f"{gdalwarp_path} -ot Int16 ../gcore/data/byte.tif {dst_tif}")

    ds = gdal.Open(dst_tif)
    assert ds is not None

    assert ds.GetRasterBand(1).DataType == gdal.GDT_Int16, "Bad data type"

    assert ds.GetRasterBand(1).Checksum() == 4672, "Bad checksum"

    ds = None


###############################################################################
# Test -t_srs option


def test_gdalwarp_4(gdalwarp_path, tmp_path):

    dst_tif = str(tmp_path / "testgdalwarp4.tif")

    gdaltest.runexternal(
        f"{gdalwarp_path} -t_srs EPSG:32611 ../gcore/data/byte.tif {dst_tif}"
    )

    ds = gdal.Open(dst_tif)
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 4672, "Bad checksum"

    ds = None


###############################################################################
# Test warping from GCPs without any explicit option


@pytest.fixture(scope="module")
def testgdalwarp_gcp_tif(tmp_path_factory):

    testgdalwarp_gcp_tif_fname = str(
        tmp_path_factory.mktemp("tmp") / "testgdalwarp_gcp.tif"
    )

    if test_cli_utilities.get_gdal_translate_path() is None:
        pytest.skip("gdal_translate missing")

    gdaltest.runexternal(
        test_cli_utilities.get_gdal_translate_path()
        + f" -a_srs EPSG:26711 -gcp 0 0  440720.000 3751320.000 -gcp 20 0 441920.000 3751320.000 -gcp 20 20 441920.000 3750120.000 0 -gcp 0 20 440720.000 3750120.000 ../gcore/data/byte.tif {testgdalwarp_gcp_tif_fname}"
    )

    yield testgdalwarp_gcp_tif_fname


def test_gdalwarp_5(gdalwarp_path, testgdalwarp_gcp_tif, tmp_path):

    dst_tif = str(tmp_path / "testgdalwarp5.tif")

    gdaltest.runexternal(f"{gdalwarp_path} {testgdalwarp_gcp_tif} {dst_tif}")

    ds = gdal.Open(dst_tif)
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


def test_gdalwarp_6(gdalwarp_path, testgdalwarp_gcp_tif, tmp_path):

    dst_tif = str(tmp_path / "testgdalwarp6.tif")

    gdaltest.runexternal(f"{gdalwarp_path} -tps {testgdalwarp_gcp_tif} {dst_tif}")

    ds = gdal.Open(dst_tif)
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


def test_gdalwarp_7(gdalwarp_path, testgdalwarp_gcp_tif, tmp_path):

    dst_tif = str(tmp_path / "testgdalwarp7.tif")

    gdaltest.runexternal(
        f"{gdalwarp_path} -tr 120 120 {testgdalwarp_gcp_tif} {dst_tif}"
    )

    ds = gdal.Open(dst_tif)
    assert ds is not None

    expected_gt = (440720.0, 120.0, 0.0, 3751320.0, 0.0, -120.0)
    gdaltest.check_geotransform(expected_gt, ds.GetGeoTransform(), 1e-9)

    ds = None


###############################################################################
# Test -ts


def test_gdalwarp_8(gdalwarp_path, testgdalwarp_gcp_tif, tmp_path):

    dst_tif = str(tmp_path / "testgdalwarp8.tif")

    gdaltest.runexternal(f"{gdalwarp_path} -ts 10 10 {testgdalwarp_gcp_tif} {dst_tif}")

    ds = gdal.Open(dst_tif)
    assert ds is not None

    expected_gt = (440720.0, 120.0, 0.0, 3751320.0, 0.0, -120.0)
    gdaltest.check_geotransform(expected_gt, ds.GetGeoTransform(), 1e-9)

    ds = None


###############################################################################
# Test -te


def test_gdalwarp_9(gdalwarp_path, testgdalwarp_gcp_tif, tmp_path):

    dst_tif = str(tmp_path / "testgdalwarp9.tif")

    gdaltest.runexternal(
        f"{gdalwarp_path} -te 440720.000 3750120.000 441920.000 3751320.000 {testgdalwarp_gcp_tif} {dst_tif}"
    )

    ds = gdal.Open(dst_tif)
    assert ds is not None

    gdaltest.check_geotransform(
        gdal.Open("../gcore/data/byte.tif").GetGeoTransform(),
        ds.GetGeoTransform(),
        1e-9,
    )

    ds = None


###############################################################################
# Test -rn


def test_gdalwarp_10(gdalwarp_path, testgdalwarp_gcp_tif, tmp_path):

    dst_tif = str(tmp_path / "testgdalwarp10.tif")

    gdaltest.runexternal(
        f"{gdalwarp_path} -ts 40 40 -rn {testgdalwarp_gcp_tif} {dst_tif}"
    )

    ds = gdal.Open(dst_tif)
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 18784, "Bad checksum"

    ds = None


###############################################################################
# Test -rb


def test_gdalwarp_11(gdalwarp_path, testgdalwarp_gcp_tif, tmp_path):

    dst_tif = str(tmp_path / "testgdalwarp11.tif")

    gdaltest.runexternal(
        f"{gdalwarp_path} -ts 40 40 -rb {testgdalwarp_gcp_tif} {dst_tif}"
    )

    ds = gdal.Open(dst_tif)
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


def test_gdalwarp_12(gdalwarp_path, testgdalwarp_gcp_tif, tmp_path):

    dst_tif = str(tmp_path / "testgdalwarp12.tif")

    gdaltest.runexternal(
        f"{gdalwarp_path} -ts 40 40 -rc {testgdalwarp_gcp_tif} {dst_tif}"
    )

    ds = gdal.Open(dst_tif)
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


def test_gdalwarp_13(gdalwarp_path, testgdalwarp_gcp_tif, tmp_path):

    dst_tif = str(tmp_path / "testgdalwarp13.tif")

    gdaltest.runexternal(
        f"{gdalwarp_path} -ts 40 40 -rcs {testgdalwarp_gcp_tif} {dst_tif}"
    )

    ds = gdal.Open(dst_tif)
    assert ds is not None

    ref_ds = gdal.Open("ref_data/testgdalwarp13.tif")
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ref_ds = None

    assert maxdiff <= 1, "Image too different from reference"

    ds = None


###############################################################################
# Test -r lanczos


def test_gdalwarp_14(gdalwarp_path, testgdalwarp_gcp_tif, tmp_path):

    dst_tif = str(tmp_path / "testgdalwarp14.tif")

    gdaltest.runexternal(
        f"{gdalwarp_path} -ts 40 40 -r lanczos {testgdalwarp_gcp_tif} {dst_tif}"
    )

    ds = gdal.Open(dst_tif)
    assert ds is not None

    ref_ds = gdal.Open("ref_data/testgdalwarp14.tif")
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ref_ds = None

    assert maxdiff <= 1, "Image too different from reference"

    ds = None


###############################################################################
# Test -of VRT which is a special case


@pytest.mark.skipif(
    not gdaltest.vrt_has_open_support(),
    reason="VRT driver open missing",
)
def test_gdalwarp_16(gdalwarp_path, testgdalwarp_gcp_tif, tmp_path):

    dst_vrt = str(tmp_path / "testgdalwarp16.vrt")

    gdaltest.runexternal(f"{gdalwarp_path} -of VRT {testgdalwarp_gcp_tif} {dst_vrt}")

    ds = gdal.Open(dst_vrt)
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 4672, "Bad checksum"

    ds = None


###############################################################################
# Test -dstalpha


def test_gdalwarp_17(gdalwarp_path, tmp_path):

    dst_tif = str(tmp_path / "testgdalwarp17.tif")

    gdaltest.runexternal(
        f"{gdalwarp_path} -dstalpha ../gcore/data/rgbsmall.tif {dst_tif}"
    )

    ds = gdal.Open(dst_tif)
    assert ds is not None

    assert ds.GetRasterBand(4) is not None, "No alpha band generated"

    ds = None


###############################################################################
# Test -wm -multi


def test_gdalwarp_18(gdalwarp_path, tmp_path):

    dst_tif = str(tmp_path / "testgdalwarp18.tif")

    _, ret_stderr = gdaltest.runexternal_out_and_err(
        f"{gdalwarp_path} -wm 20MB -multi ../gcore/data/byte.tif {dst_tif}"
    )

    # This error will be returned if GDAL is not compiled with thread support
    if ret_stderr.find("CPLCreateThread() failed in ChunkAndWarpMulti()") != -1:
        pytest.skip("GDAL not compiled with thread support")
    assert not ret_stderr

    ds = gdal.Open(dst_tif)
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 4672, "Bad checksum"

    ds = None


###############################################################################
# Test -et 0 which is a special case


def test_gdalwarp_19(gdalwarp_path, testgdalwarp_gcp_tif, tmp_path):

    dst_tif = str(tmp_path / "testgdalwarp19.tif")

    gdaltest.runexternal(f"{gdalwarp_path} -et 0 {testgdalwarp_gcp_tif} {dst_tif}")

    ds = gdal.Open(dst_tif)
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 4672, "Bad checksum"

    ds = None


###############################################################################
# Test -of VRT -et 0 which is a special case


@pytest.mark.skipif(
    not gdaltest.vrt_has_open_support(),
    reason="VRT driver open missing",
)
def test_gdalwarp_20(gdalwarp_path, testgdalwarp_gcp_tif, tmp_path):

    dst_vrt = str(tmp_path / "testgdalwarp20.vrt")

    gdaltest.runexternal(
        f"{gdalwarp_path} -of VRT -et 0 {testgdalwarp_gcp_tif} {dst_vrt}"
    )

    ds = gdal.Open(dst_vrt)
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 4672, "Bad checksum"

    ds = None


###############################################################################
# Test cutline from OGR datasource.


@pytest.mark.require_driver("CSV")
def test_gdalwarp_21(gdalwarp_path, tmp_path):

    dst_tif = str(tmp_path / "testgdalwarp21.tif")

    gdaltest.runexternal(
        f"{gdalwarp_path} ../gcore/data/utmsmall.tif {dst_tif} -cutline data/cutline.vrt -cl cutline"
    )

    ds = gdal.Open(dst_tif)
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 19139, "Bad checksum"

    ds = None


###############################################################################
# Test with a cutline and an output at a different resolution.


@pytest.mark.require_driver("CSV")
def test_gdalwarp_22(gdalwarp_path, tmp_path):

    dst_tif = str(tmp_path / "testgdalwarp22.tif")

    gdaltest.runexternal(
        f"{gdalwarp_path} ../gcore/data/utmsmall.tif {dst_tif} -cutline data/cutline.vrt -cl cutline -tr 30 30"
    )

    ds = gdal.Open(dst_tif)
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 14047, "Bad checksum"

    ds = None


###############################################################################
# Test cutline with ALL_TOUCHED enabled.


@pytest.mark.require_driver("CSV")
def test_gdalwarp_23(gdalwarp_path, tmp_path):

    dst_tif = str(tmp_path / "testgdalwarp23.tif")

    gdaltest.runexternal(
        f"{gdalwarp_path} -wo CUTLINE_ALL_TOUCHED=TRUE ../gcore/data/utmsmall.tif {dst_tif} -cutline data/cutline.vrt -cl cutline"
    )

    ds = gdal.Open(dst_tif)
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 20123, "Bad checksum"

    ds = None


###############################################################################
# Test warping an image crossing the 180E/180W longitude (#3206)


def test_gdalwarp_24(gdalwarp_path, tmp_path):

    src_tif = str(tmp_path / "testgdalwarp24src.tif")
    dst_tif = str(tmp_path / "testgdalwarp24dst.tif")

    ds = gdal.GetDriverByName("GTiff").Create(src_tif, 100, 100)
    ds.SetGeoTransform([179.5, 0.01, 0, 45, 0, -0.01])
    ds.SetProjection(
        'GEOGCS["GCS_WGS_1984",DATUM["D_WGS_1984",SPHEROID["WGS_1984",6378137.0,298.257223563]],PRIMEM["Greenwich",0.0],UNIT["Degree",0.0174532925199433]]'
    )
    ds.GetRasterBand(1).Fill(255)
    ds = None

    gdaltest.runexternal(f"{gdalwarp_path} -t_srs EPSG:32660 {src_tif} {dst_tif}")

    ds = gdal.Open(dst_tif)
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 50683, "Bad checksum"

    ds = None


###############################################################################
# Test warping a full EPSG:4326 extent to +proj=sinu (#2305)


@pytest.mark.require_creation_option("GTiff", "JPEG")
def test_gdalwarp_25(gdalwarp_path, tmp_path):

    dst_tif = str(tmp_path / "testgdalwarp25.tif")

    gdaltest.runexternal(
        f'{gdalwarp_path} -t_srs "+proj=sinu" data/w_jpeg.tiff {dst_tif}'
    )

    ds = gdal.Open(dst_tif)
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
def test_gdalwarp_26(gdalwarp_path, tmp_path):

    dst_tif = str(tmp_path / "testgdalwarp26.tif")

    gdaltest.runexternal(
        f'{gdalwarp_path} -t_srs "+proj=eck4" data/w_jpeg.tiff {dst_tif}'
    )

    ds = gdal.Open(dst_tif)
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
def test_gdalwarp_27(gdalwarp_path, tmp_path):

    dst_tif = str(tmp_path / "testgdalwarp27.tif")

    gdaltest.runexternal(
        f'{gdalwarp_path} -t_srs "+proj=vandg" data/w_jpeg.tiff {dst_tif} -overwrite'
    )

    ds = gdal.Open(dst_tif)
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
def test_gdalwarp_28(gdalwarp_path, tmp_path):

    dst_tif = str(tmp_path / "testgdalwarp28.tif")

    gdaltest.runexternal(
        f'{gdalwarp_path} -t_srs "+proj=aeqd +lat_0=45 +lon_0=90" data/w_jpeg.tiff {dst_tif}'
    )

    ds = gdal.Open(dst_tif)
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
@pytest.mark.xfail()
def test_gdalwarp_29(gdalwarp_path, tmp_path):

    # This test has been disabled since PROJ 8 will reproject a coordinates at
    # lat=90 to a finite value, due to 90deg being < PI/2 due to numerical
    # accuracy

    dst_tif = str(tmp_path / "testgdalwarp29.tif")

    gdaltest.runexternal(f"{gdalwarp_path} -t_srs EPSG:3785 data/w_jpeg.tiff {dst_tif}")

    ds = gdal.Open(dst_tif)
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
def test_gdalwarp_30(gdalwarp_path, tmp_path):

    dst1_tif = str(tmp_path / "testgdalwarp30_1.tif")
    dst2_tif = str(tmp_path / "testgdalwarp30_2.tif")
    dst3_tif = str(tmp_path / "testgdalwarp30_3.tif")

    te = " -te -20037508.343 -16206629.152 20036845.112 16213801.068"

    # First run : no parameter
    gdaltest.runexternal(
        gdalwarp_path
        + f" data/w_jpeg.tiff {dst1_tif}  -t_srs EPSG:3785 -co COMPRESS=LZW -wm 500000  --config GDAL_CACHEMAX 1 -ts 1000 500 -co TILED=YES"
        + te
    )

    # Second run : with  -wo OPTIMIZE_SIZE=TRUE
    gdaltest.runexternal(
        gdalwarp_path
        + f" data/w_jpeg.tiff {dst2_tif}  -t_srs EPSG:3785 -co COMPRESS=LZW -wm 500000 -wo OPTIMIZE_SIZE=TRUE  --config GDAL_CACHEMAX 1 -ts 1000 500 -co TILED=YES"
        + te
    )

    # Third run : with  -wo STREAMABLE_OUTPUT=TRUE
    gdaltest.runexternal(
        gdalwarp_path
        + f" data/w_jpeg.tiff {dst3_tif}  -t_srs EPSG:3785 -co COMPRESS=LZW -wm 500000 -wo STREAMABLE_OUTPUT=TRUE  --config GDAL_CACHEMAX 1 -ts 1000 500 -co TILED=YES"
        + te
    )

    file_size1 = os.stat(dst1_tif)[stat.ST_SIZE]
    file_size2 = os.stat(dst2_tif)[stat.ST_SIZE]
    file_size3 = os.stat(dst3_tif)[stat.ST_SIZE]

    ds = gdal.Open(dst1_tif)
    assert ds is not None

    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 64629 or cs == 1302, "Bad checksum on testgdalwarp30_1"

    ds = None

    ds = gdal.Open(dst2_tif)
    assert ds is not None

    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 64629 or cs == 1302, "Bad checksum on testgdalwarp30_2"

    ds = None

    ds = gdal.Open(dst3_tif)
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


def test_gdalwarp_31(gdalwarp_path, tmp_path):

    dst_tif = str(tmp_path / "testgdalwarp31.tif")

    gdaltest.runexternal(f"{gdalwarp_path} ../gcore/data/byte.tif {dst_tif}")

    ds = gdal.Open(dst_tif)
    cs1 = ds.GetRasterBand(1).Checksum()
    ds = None

    _, err = gdaltest.runexternal_out_and_err(
        f"{gdalwarp_path} ../gcore/data/byte.tif {dst_tif} -t_srs EPSG:4326"
    )

    ds = gdal.Open(dst_tif)
    cs2 = ds.GetRasterBand(1).Checksum()
    ds = None

    _, err2 = gdaltest.runexternal_out_and_err(
        f"{gdalwarp_path} ../gcore/data/byte.tif {dst_tif} -t_srs EPSG:4326 -overwrite"
    )

    ds = gdal.Open(dst_tif)
    cs3 = ds.GetRasterBand(1).Checksum()
    ds = None

    assert cs1 == 4672 and cs2 == 4672 and cs3 == 4727 and err != "" and err2 == ""


###############################################################################
# Test warping a JPEG compressed image with a mask into a RGBA image


@pytest.mark.require_creation_option("GTiff", "JPEG")
def test_gdalwarp_33(gdalwarp_path, tmp_path):

    if test_cli_utilities.get_gdal_translate_path() is None:
        pytest.skip("gdal_translate missing")

    dst_tif = str(tmp_path / "testgdalwarp33.tif")
    mask_tif = str(tmp_path / "testgdalwarp33_mask.tif")

    gdaltest.runexternal(
        f"{gdalwarp_path} -dstalpha ../gcore/data/ycbcr_with_mask.tif {dst_tif}"
    )

    src_ds = gdal.Open("../gcore/data/ycbcr_with_mask.tif")
    ds = gdal.Open(dst_tif)
    assert ds is not None

    # There are expected diffs because of the artifacts due to JPEG compression in 8x8 blocks
    # that are partially masked. gdalwarp will remove those artifacts
    max_diff = gdaltest.compare_ds(src_ds, ds)
    assert max_diff <= 40

    src_ds = None

    gdaltest.runexternal(
        test_cli_utilities.get_gdal_translate_path()
        + f" -expand gray GTIFF_DIR:2:../gcore/data/ycbcr_with_mask.tif {mask_tif}"
    )

    mask_ds = gdal.Open(mask_tif)
    expected_cs = mask_ds.GetRasterBand(1).Checksum()
    mask_ds = None

    cs = ds.GetRasterBand(4).Checksum()

    ds = None

    assert cs == expected_cs, "did not get expected checksum on alpha band"


###############################################################################
# Test warping multiple sources


def test_gdalwarp_34(gdalwarp_path, tmp_path):

    if test_cli_utilities.get_gdal_translate_path() is None:
        pytest.skip("gdal_translate missing")

    src1_tif = str(tmp_path / "testgdalwarp34src_1.tif")
    src2_tif = str(tmp_path / "testgdalwarp34src_2.tif")
    dst_tif = str(tmp_path / "testgdalwarp34.tif")

    gdaltest.runexternal(
        test_cli_utilities.get_gdal_translate_path()
        + f" ../gcore/data/byte.tif {src1_tif} -srcwin 0 0 10 20"
    )
    gdaltest.runexternal(
        test_cli_utilities.get_gdal_translate_path()
        + f" ../gcore/data/byte.tif {src2_tif} -srcwin 10 0 10 20"
    )
    gdaltest.runexternal(f"{gdalwarp_path} {src1_tif} {src2_tif} {dst_tif}")

    ds = gdal.Open(dst_tif)
    cs = ds.GetRasterBand(1).Checksum()
    gt = ds.GetGeoTransform()
    xsize = ds.RasterXSize
    ysize = ds.RasterYSize
    ds = None

    assert xsize == 20 and ysize == 20, "bad dimensions"

    assert cs == 4672, "bad checksum"

    expected_gt = (440720.0, 60.0, 0.0, 3751320.0, 0.0, -60.0)
    for i in range(6):
        assert gt[i] == pytest.approx(expected_gt[i], abs=1e-5), "bad gt"


###############################################################################
# Test -ts and -te optimization (doesn't need calling GDALSuggestedWarpOutput2, #4804)


def test_gdalwarp_35(gdalwarp_path, tmp_path):

    dst_tif = str(tmp_path / "testgdalwarp35.tif")

    gdaltest.runexternal(
        f"{gdalwarp_path} -ts 20 20 -te 440720.000 3750120.000 441920.000 3751320.000 ../gcore/data/byte.tif {dst_tif}"
    )

    ds = gdal.Open(dst_tif)
    assert ds is not None

    gdaltest.check_geotransform(
        gdal.Open("../gcore/data/byte.tif").GetGeoTransform(),
        ds.GetGeoTransform(),
        1e-9,
    )

    ds = None


###############################################################################
# Test -tr and -te optimization (doesn't need calling GDALSuggestedWarpOutput2, #4804)


def test_gdalwarp_36(gdalwarp_path, tmp_path):

    dst_tif = str(tmp_path / "testgdalwarp36.tif")

    gdaltest.runexternal(
        f"{gdalwarp_path} -tr 60 60 -te 440720.000 3750120.000 441920.000 3751320.000 ../gcore/data/byte.tif {dst_tif}"
    )

    ds = gdal.Open(dst_tif)
    assert ds is not None

    gdaltest.check_geotransform(
        gdal.Open("../gcore/data/byte.tif").GetGeoTransform(),
        ds.GetGeoTransform(),
        1e-9,
    )

    ds = None


###############################################################################
# Test metadata copying - stats should not be copied (#5319)


def test_gdalwarp_37(gdalwarp_path, tmp_path):

    dst_tif = str(tmp_path / "testgdalwarp37.tif")

    gdaltest.runexternal(f"{gdalwarp_path} -tr 60 60 ./data/utmsmall.tif {dst_tif}")

    ds = gdal.Open(dst_tif)
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
def test_gdalwarp_38(gdalwarp_path, tmp_path):

    dst_tif = str(tmp_path / "testgdalwarp38.tif")

    gdaltest.runexternal(f"{gdalwarp_path} data/withnodata.asc {dst_tif}")

    ds = gdal.Open(dst_tif)
    assert ds.GetRasterBand(1).Checksum() == 65531
    assert ds.GetRasterBand(1).GetNoDataValue() == -999
    ds = None


###############################################################################
# Test -oo


@pytest.mark.require_driver("AAIGRID")
def test_gdalwarp_39(gdalwarp_path, tmp_path):

    dst_tif = str(tmp_path / "testgdalwarp39.tif")

    gdaltest.runexternal(
        f"{gdalwarp_path} ../gdrivers/data/aaigrid/float64.asc {dst_tif} -oo DATATYPE=Float64 -overwrite"
    )

    ds = gdal.Open(dst_tif)
    assert ds.GetRasterBand(1).DataType == gdal.GDT_Float64
    ds = None


###############################################################################
# Test -ovr


@pytest.mark.skipif(
    not gdaltest.vrt_has_open_support(),
    reason="VRT driver open missing",
)
def test_gdalwarp_40(gdalwarp_path, tmp_path):

    src_tif = str(tmp_path / "test_gdalwarp_40_src.tif")
    src_tif_copy = str(tmp_path / "test_gdalwarp_40_src_copy.tif")
    dst_tif = str(tmp_path / "test_gdalwarp_40.tif")
    dst_vrt = str(tmp_path / "test_gdalwarp_40.vrt")

    src_ds = gdal.Open("../gcore/data/byte.tif")
    out_ds = gdal.GetDriverByName("GTiff").CreateCopy(src_tif, src_ds)
    cs_main = out_ds.GetRasterBand(1).Checksum()
    out_ds.BuildOverviews("NONE", overviewlist=[2, 4])
    out_ds.GetRasterBand(1).GetOverview(0).Fill(127)
    cs_ov0 = out_ds.GetRasterBand(1).GetOverview(0).Checksum()
    out_ds.GetRasterBand(1).GetOverview(1).Fill(255)
    cs_ov1 = out_ds.GetRasterBand(1).GetOverview(1).Checksum()

    out_ds = None

    shutil.copy(src_tif, src_tif_copy)

    # Should select main resolution
    gdaltest.runexternal(f"{gdalwarp_path} {src_tif} {dst_tif} -overwrite")

    ds = gdal.Open(dst_tif)
    assert ds.GetRasterBand(1).Checksum() == cs_main
    ds = None

    # Test -ovr AUTO. Should select main resolution
    gdaltest.runexternal(f"{gdalwarp_path} {src_tif} {dst_tif} -overwrite -ovr AUTO")

    ds = gdal.Open(dst_tif)
    assert ds.GetRasterBand(1).Checksum() == cs_main
    ds = None

    gdaltest.runexternal(
        f"{gdalwarp_path} ../gcore/data/byte.tif {dst_tif} -overwrite -ts 5 5"
    )
    ds = gdal.Open(dst_tif)
    expected_cs = ds.GetRasterBand(1).Checksum()
    ds = None

    # Test -ovr NONE. Should select main resolution too
    gdaltest.runexternal(
        f"{gdalwarp_path} {src_tif} {dst_tif} -overwrite -ovr NONE -ts 5 5"
    )

    ds = gdal.Open(dst_tif)
    assert ds.GetRasterBand(1).Checksum() == expected_cs
    ds = None

    gdaltest.runexternal(
        f"{gdalwarp_path} ../gcore/data/byte.tif {dst_tif} -overwrite -ts 15 15"
    )
    ds = gdal.Open(dst_tif)
    expected_cs = ds.GetRasterBand(1).Checksum()
    ds = None

    # Should select main resolution too
    gdaltest.runexternal(f"{gdalwarp_path} {src_tif} {dst_tif} -overwrite -ts 15 15")

    ds = gdal.Open(dst_tif)
    assert ds.GetRasterBand(1).Checksum() == expected_cs
    ds = None

    # Should select overview 0
    gdaltest.runexternal(f"{gdalwarp_path} {src_tif} {dst_tif} -overwrite -ts 10 10")

    ds = gdal.Open(dst_tif)
    assert ds.GetRasterBand(1).Checksum() == cs_ov0
    ds = None

    # Should select overview 0
    gdaltest.runexternal(f"{gdalwarp_path} {src_tif} {dst_tif} -overwrite -ovr 0")

    ds = gdal.Open(dst_tif)
    assert ds.GetRasterBand(1).Checksum() == cs_ov0
    ds = None

    # Should select overview 0 (no overwrite)
    gdaltest.runexternal(f"{gdalwarp_path} {src_tif} {dst_tif} -ovr 0")

    # Repeat with no output file and no overwrite (takes a different code path)
    os.unlink(dst_tif)
    gdaltest.runexternal(f"{gdalwarp_path} {src_tif} {dst_tif} -ovr 0")

    # Should not crash (actually it never did)
    os.unlink(dst_tif)
    gdaltest.runexternal(f"{gdalwarp_path} {src_tif} {src_tif_copy} {dst_tif} -ovr 0")
    ds = gdal.Open(dst_tif)
    assert ds.GetRasterBand(1).Checksum() == cs_ov0
    ds = None

    # Should select overview 0 through VRT
    gdaltest.runexternal(
        f"{gdalwarp_path} {src_tif} {dst_vrt} -overwrite -ts 10 10 -of VRT"
    )

    ds = gdal.Open(dst_vrt)
    assert ds.GetRasterBand(1).Checksum() == cs_ov0
    ds = None

    # Should select overview 0 through VRT
    gdaltest.runexternal(
        f"{gdalwarp_path} {src_tif} {dst_vrt} -overwrite -ts 10 10 -te 440720 3750120 441920 3751320 -of VRT"
    )

    ds = gdal.Open(dst_vrt)
    assert ds.GetRasterBand(1).Checksum() == cs_ov0
    ds = None

    gdaltest.runexternal(
        f"{gdalwarp_path} {src_tif} -oo OVERVIEW_LEVEL=0 {dst_tif} -overwrite -ts 7 7"
    )
    ds = gdal.Open(dst_tif)
    expected_cs = ds.GetRasterBand(1).Checksum()
    ds = None

    # Test that tiny variations in -te that result in a target resampling factor
    # very close to the one of overview 0 lead to overview 0 been selected

    gdaltest.runexternal(
        f"{gdalwarp_path} {src_tif} {dst_vrt} -overwrite -ts 10 10 -te 440721 3750120 441920 3751320 -of VRT"
    )

    ds = gdal.Open(dst_vrt)
    assert ds.GetRasterBand(1).Checksum() == cs_ov0
    ds = None

    gdaltest.runexternal(
        f"{gdalwarp_path} {src_tif} {dst_vrt} -overwrite -ts 10 10 -te 440719 3750120 441920 3751320 -of VRT"
    )

    ds = gdal.Open(dst_vrt)
    assert ds.GetRasterBand(1).Checksum() == cs_ov0
    ds = None

    # Should select overview 0 too
    gdaltest.runexternal(f"{gdalwarp_path} {src_tif} {dst_tif} -overwrite -ts 7 7")

    ds = gdal.Open(dst_tif)
    assert ds.GetRasterBand(1).Checksum() == expected_cs
    ds = None

    gdaltest.runexternal(
        f"{gdalwarp_path} {src_tif} -ovr NONE -oo OVERVIEW_LEVEL=0 {dst_tif} -overwrite -ts 5 5"
    )
    ds = gdal.Open(dst_tif)
    expected_cs = ds.GetRasterBand(1).Checksum()
    ds = None

    # Test AUTO-n. Should select overview 0 too
    gdaltest.runexternal(
        f"{gdalwarp_path} {src_tif} {dst_tif} -overwrite -ts 5 5 -ovr AUTO-1"
    )

    ds = gdal.Open(dst_tif)
    assert ds.GetRasterBand(1).Checksum() == expected_cs
    ds = None

    # Should select overview 1
    gdaltest.runexternal(f"{gdalwarp_path} {src_tif} {dst_tif} -overwrite -ts 5 5")

    ds = gdal.Open(dst_tif)
    assert ds.GetRasterBand(1).Checksum() == cs_ov1
    ds = None

    gdaltest.runexternal(
        f"{gdalwarp_path} {src_tif} -oo OVERVIEW_LEVEL=1 {dst_tif} -overwrite -ts 3 3"
    )
    ds = gdal.Open(dst_tif)
    expected_cs = ds.GetRasterBand(1).Checksum()
    ds = None

    # Should select overview 1 too
    gdaltest.runexternal(f"{gdalwarp_path} {src_tif} {dst_tif} -overwrite -ts 3 3")

    ds = gdal.Open(dst_tif)
    assert ds.GetRasterBand(1).Checksum() == expected_cs
    ds = None

    gdaltest.runexternal(
        f"{gdalwarp_path} {src_tif} -oo OVERVIEW_LEVEL=1 {dst_tif} -overwrite -ts 20 20"
    )
    ds = gdal.Open(dst_tif)
    expected_cs = ds.GetRasterBand(1).Checksum()
    ds = None

    # Specify a level >= number of overviews. Should select overview 1 too
    gdaltest.runexternal(f"{gdalwarp_path} {src_tif} {dst_tif} -overwrite -ovr 5")

    ds = gdal.Open(dst_tif)
    assert ds.GetRasterBand(1).Checksum() == expected_cs
    ds = None


###############################################################################
# Test source fill ratio heuristics (#3120)
# Also check that we guess a reasonable resolution (#2754), from the source
# dataset and target extent


def test_gdalwarp_41(gdalwarp_path, tmp_path):

    src_tif = str(tmp_path / "test_gdalwarp_41_src.tif")
    dst_tif = str(tmp_path / "test_gdalwarp_41.tif")

    src_ds = gdal.GetDriverByName("GTiff").Create(src_tif, 666, 666)
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
    src_ds.SetProjection("""PROJCS["WGS_1984_Stereographic_South_Pole",
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
        AUTHORITY["EPSG","9001"]]]""")
    src_ds.GetRasterBand(1).Fill(255)
    src_ds = None

    # Check when source fill ratio heuristics is ON
    gdaltest.runexternal(
        f"{gdalwarp_path} {src_tif} {dst_tif} -overwrite  -t_srs EPSG:4326 -te -180 -90 180 90  -wo INIT_DEST=127 -wo SKIP_NOSOURCE=YES"
    )

    ds = gdal.Open(dst_tif)
    assert ds.RasterXSize == 2052
    assert ds.RasterYSize == 1026
    assert ds.GetRasterBand(1).Checksum() == 57091
    ds = None

    # Check when source fill ratio heuristics is OFF
    gdaltest.runexternal(
        f"{gdalwarp_path} {src_tif} {dst_tif} -overwrite  -t_srs EPSG:4326 -te -180 -90 180 90  -wo INIT_DEST=127 -wo SKIP_NOSOURCE=YES -wo SRC_FILL_RATIO_HEURISTICS=NO"
    )

    ds = gdal.Open(dst_tif)
    assert ds.GetRasterBand(1).Checksum() == 31890
    ds = None


###############################################################################
# Test warping multiple source images, in one step or several, with INIT_DEST/nodata (#5909, #5387)


def test_gdalwarp_42(gdalwarp_path, tmp_path):

    if test_cli_utilities.get_gdal_translate_path() is None:
        pytest.skip("gdal_translate missing")

    left_tif = str(tmp_path / "small_world_left.tif")
    right_tif = str(tmp_path / "small_world_right.tif")
    dst_tif = str(tmp_path / "test_gdalwarp_42.tif")

    gdaltest.runexternal(
        test_cli_utilities.get_gdal_translate_path()
        + f" ../gdrivers/data/small_world.tif {left_tif} -srcwin 0 0 200 200 -a_nodata 255"
    )
    gdaltest.runexternal(
        test_cli_utilities.get_gdal_translate_path()
        + f" ../gdrivers/data/small_world.tif {right_tif} -srcwin 200 0 200 200  -a_nodata 255"
    )

    gdaltest.runexternal(
        f"{gdalwarp_path} {left_tif} {dst_tif} -overwrite -te -180 -90 180 90 -dstalpha -wo UNIFIED_SRC_NODATA=YES"
    )
    gdaltest.runexternal(
        f"{gdalwarp_path} {right_tif} {dst_tif} -wo UNIFIED_SRC_NODATA=YES"
    )

    ds = gdal.Open(dst_tif)
    got_cs = [ds.GetRasterBand(i + 1).Checksum() for i in range(4)]
    expected_cs = [25382, 27573, 35297, 59540]
    assert got_cs == expected_cs
    ds = None

    # In one step
    gdaltest.runexternal(
        f"{gdalwarp_path} {left_tif} {right_tif} {dst_tif} -overwrite -te -180 -90 180 90 -dstalpha -wo UNIFIED_SRC_NODATA=YES"
    )

    ds = gdal.Open(dst_tif)
    got_cs = [ds.GetRasterBand(i + 1).Checksum() for i in range(4)]
    expected_cs = [25382, 27573, 35297, 59540]
    assert got_cs == expected_cs
    ds = None

    # In one step with -wo INIT_DEST=255,255,255,0
    gdaltest.runexternal(
        f"{gdalwarp_path} {left_tif} {right_tif} {dst_tif} -wo INIT_DEST=255,255,255,0 -overwrite -te -180 -90 180 90 -dstalpha -wo UNIFIED_SRC_NODATA=YES"
    )

    ds = gdal.Open(dst_tif)
    got_cs = [ds.GetRasterBand(i + 1).Checksum() for i in range(4)]
    expected_cs = [30111, 32302, 40026, 59540]
    assert got_cs == expected_cs
    ds = None

    # In one step with -wo INIT_DEST=0,0,0,0
    # Different checksum since there are source pixels at 255, so they get remap to 0
    gdaltest.runexternal(
        f"{gdalwarp_path} {left_tif} {right_tif} {dst_tif} -wo INIT_DEST=0,0,0,0 -overwrite -te -180 -90 180 90 -dstalpha -wo UNIFIED_SRC_NODATA=YES"
    )

    ds = gdal.Open(dst_tif)
    got_cs = [ds.GetRasterBand(i + 1).Checksum() for i in range(4)]
    expected_cs = [25382, 27573, 35297, 59540]
    assert got_cs == expected_cs
    ds = None


###############################################################################
# Test that NODATA_VALUES is honoured, but not transferred when adding an alpha channel.


def test_gdalwarp_43(gdalwarp_path, tmp_path):
    if test_cli_utilities.get_gdal_translate_path() is None:
        pytest.skip("gdal_translate missing")

    src_tif = str(tmp_path / "small_world.tif")
    dst_tif = str(tmp_path / "test_gdalwarp_43.tif")

    gdaltest.runexternal(
        test_cli_utilities.get_gdal_translate_path()
        + f' ../gdrivers/data/small_world.tif {src_tif} -mo "FOO=BAR" -mo "NODATA_VALUES=62 93 23"'
    )

    gdaltest.runexternal(f"{gdalwarp_path} {src_tif} {dst_tif} -overwrite -dstalpha")

    ds = gdal.Open(dst_tif)
    assert ds.GetMetadataItem("NODATA_VALUES") is None
    assert ds.GetMetadataItem("FOO") == "BAR"
    got_cs = [ds.GetRasterBand(i + 1).Checksum() for i in range(4)]
    expected_cs = [30106, 32285, 40022, 64261]
    assert got_cs == expected_cs


###############################################################################
# Test effect of -wo SRC_COORD_PRECISION


def test_gdalwarp_44(gdalwarp_path, tmp_path):

    dst_tif = str(tmp_path / "out.tif")

    # Without  -wo SRC_COORD_PRECISION
    gdaltest.runexternal(
        f"{gdalwarp_path} -q ../gcore/data/byte.tif {dst_tif} -wm 10 -overwrite -ts 500 500 -r cubic -ot float32 -t_srs EPSG:4326"
    )
    ds = gdal.Open(dst_tif)
    cs1 = ds.GetRasterBand(1).Checksum()
    ds = None

    gdaltest.runexternal(
        f"{gdalwarp_path} -q ../gcore/data/byte.tif {dst_tif} -wm 0.1 -overwrite -ts 500 500 -r cubic -ot float32 -t_srs EPSG:4326"
    )
    ds = gdal.Open(dst_tif)
    cs2 = ds.GetRasterBand(1).Checksum()
    ds = None

    if cs1 == cs2:
        print("Unexpected cs1 == cs2")

    # With  -wo SRC_COORD_PRECISION
    gdaltest.runexternal(
        f"{gdalwarp_path} -q ../gcore/data/byte.tif {dst_tif} -wm 10 -et 0.01 -wo SRC_COORD_PRECISION=0.1 -overwrite -ts 500 500 -r cubic -ot float32 -t_srs EPSG:4326"
    )
    ds = gdal.Open(dst_tif)
    cs3 = ds.GetRasterBand(1).Checksum()
    ds = None

    gdaltest.runexternal(
        f"{gdalwarp_path} -q ../gcore/data/byte.tif {dst_tif} -wm 0.1 -et 0.01 -wo SRC_COORD_PRECISION=0.1 -overwrite -ts 500 500 -r cubic -ot float32 -t_srs EPSG:4326"
    )
    ds = gdal.Open(dst_tif)
    cs4 = ds.GetRasterBand(1).Checksum()
    ds = None

    assert cs3 == cs4


###############################################################################
# Test -te_srs


def test_gdalwarp_45(gdalwarp_path, tmp_path):

    dst_tif = str(tmp_path / "test_gdalwarp_45.tif")

    gdaltest.runexternal(
        f"{gdalwarp_path} -te_srs EPSG:4267 -te -117.641087629972 33.8915301685897 -117.628190189534 33.9024195619201 ../gcore/data/byte.tif {dst_tif} -overwrite"
    )

    ds = gdal.Open(dst_tif)
    assert ds.GetRasterBand(1).Checksum() == 4672

    ds = None


def test_gdalwarp_45bis(gdalwarp_path, tmp_path):

    dst_tif = str(tmp_path / "test_gdalwarp_45bis.tif")

    gdaltest.runexternal(
        f"{gdalwarp_path} -te_srs EPSG:4267 -te -117.641087629972 33.8915301685897 -117.628190189534 33.9024195619201 -t_srs EPSG:32611 ../gcore/data/byte.tif {dst_tif} -overwrite"
    )

    ds = gdal.Open(dst_tif)
    assert ds.GetRasterBand(1).Checksum() == 4672

    ds = None


###############################################################################
# Test -crop_to_cutline


@pytest.mark.require_driver("CSV")
def test_gdalwarp_46(gdalwarp_path, tmp_path):
    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip("ogr2ogr missing")

    dst_tif = str(tmp_path / "testgdalwarp46.tif")

    gdaltest.runexternal(
        f"{gdalwarp_path} ../gcore/data/utmsmall.tif {dst_tif} -cutline data/cutline.vrt -crop_to_cutline -overwrite"
    )

    ds = gdal.Open(dst_tif)
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 18837, "Bad checksum"

    ds = None


@pytest.mark.require_driver("CSV")
def test_gdalwarp_46bis(gdalwarp_path, tmp_path):

    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip("ogr2ogr missing")

    dst_tif = str(tmp_path / "testgdalwarp46bis.tif")

    # With explicit -s_srs and -t_srs
    gdaltest.runexternal(
        f"{gdalwarp_path} ../gcore/data/utmsmall.tif {dst_tif} -cutline data/cutline.vrt -crop_to_cutline -overwrite -s_srs EPSG:26711 -t_srs EPSG:26711"
    )

    ds = gdal.Open(dst_tif)
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 18837, "Bad checksum"

    ds = None


@pytest.mark.require_driver("CSV")
def test_gdalwarp_46ter(gdalwarp_path, tmp_path):

    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip("ogr2ogr missing")

    dst_tif = str(tmp_path / "testgdalwarp46ter.tif")
    cutline_shp = str(tmp_path / "cutline_4326.shp")

    # With cutline in another SRS
    gdaltest.runexternal(
        test_cli_utilities.get_ogr2ogr_path()
        + f" {cutline_shp} data/cutline.vrt -s_srs EPSG:26711 -t_srs EPSG:4326"
    )
    gdaltest.runexternal(
        f"{gdalwarp_path} ../gcore/data/utmsmall.tif {dst_tif} -cutline {cutline_shp} -crop_to_cutline -overwrite -t_srs EPSG:32711"
    )

    ds = gdal.Open(dst_tif)
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 19582, "Bad checksum"

    ds = None


###############################################################################
# Test gdalwarp -co APPEND_SUBDATASET=YES


def test_gdalwarp_47_append_subdataset(gdalwarp_path, tmp_path):

    tmpfilename = str(tmp_path / "test_gdalwarp_47_append_subdataset.tif")
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


def test_gdalwarp_if_option(gdalwarp_path, tmp_vsimem):

    ret, err = gdaltest.runexternal_out_and_err(
        f"{gdalwarp_path} -if GTiff ../gcore/data/byte.tif {tmp_vsimem}/out.tif"
    )
    assert err is None or err == ""

    _, err = gdaltest.runexternal_out_and_err(
        f"{gdalwarp_path} -if invalid_driver_name ../gcore/data/byte.tif {tmp_vsimem}/out.tif"
    )
    assert err is not None
    assert "invalid_driver_name" in err

    _, err = gdaltest.runexternal_out_and_err(
        f"{gdalwarp_path} -if HFA ../gcore/data/byte.tif {tmp_vsimem}/out.tif"
    )
    assert err is not None


###############################################################################
# Test invalid -wm


def test_gdalwarp_invalid_wm(gdalwarp_path, tmp_vsimem):

    ret, err = gdaltest.runexternal_out_and_err(
        f"{gdalwarp_path} ../gcore/data/byte.tif {tmp_vsimem}/out.tif -wm maximum"
    )
    assert "non-numeric" in err
    assert "Failed to parse value of -wm" in err

    ret, err = gdaltest.runexternal_out_and_err(
        f"{gdalwarp_path} ../gcore/data/byte.tif {tmp_vsimem}/out.tif -wm 200%"
    )
    assert "Memory percentage" in err
    assert "Failed to parse value of -wm" in err
