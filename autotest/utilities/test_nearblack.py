#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  nearblack testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2010-2013, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import shutil

import gdaltest
import pytest
import test_cli_utilities

from osgeo import gdal

pytestmark = pytest.mark.skipif(
    test_cli_utilities.get_nearblack_path() is None, reason="nearblack not available"
)


@pytest.fixture()
def nearblack_path():
    return test_cli_utilities.get_nearblack_path()


###############################################################################
# Basic test


def test_nearblack_1(nearblack_path, tmp_path):

    output_tif = str(tmp_path / "nearblack1.tif")

    _, err = gdaltest.runexternal_out_and_err(
        f"{nearblack_path} ../gdrivers/data/rgbsmall.tif -nb 0 -of GTiff -o {output_tif}"
    )
    assert err is None or err == "", "got error/warning"

    src_ds = gdal.Open("../gdrivers/data/rgbsmall.tif")
    ds = gdal.Open(output_tif)
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 21106, "Bad checksum band 1"

    assert ds.GetRasterBand(2).Checksum() == 20736, "Bad checksum band 2"

    assert ds.GetRasterBand(3).Checksum() == 21309, "Bad checksum band 3"

    src_gt = src_ds.GetGeoTransform()
    dst_gt = ds.GetGeoTransform()
    for i in range(6):
        assert src_gt[i] == pytest.approx(dst_gt[i], abs=1e-10), "Bad geotransform"

    dst_wkt = ds.GetProjectionRef()
    assert dst_wkt.find('AUTHORITY["EPSG","4326"]') != -1, "Bad projection"

    src_ds = None
    ds = None


###############################################################################
# Add alpha band


def test_nearblack_2(nearblack_path, tmp_path):

    output_tif = str(tmp_path / "nearblack2.tif")
    output2_tif = str(tmp_path / "nearblack3.tif")

    gdaltest.runexternal(
        f"{nearblack_path} ../gdrivers/data/rgbsmall.tif -setalpha -nb 0 -of GTiff -o {output_tif} -co TILED=YES"
    )

    ds = gdal.Open(output_tif)
    assert ds is not None

    assert ds.GetRasterBand(4).Checksum() == 22002, "Bad checksum band 0"

    ds = None

    # Set existing alpha band

    shutil.copy(output_tif, output2_tif)
    gdaltest.runexternal(f"{nearblack_path} -setalpha -nb 0 -of GTiff {output2_tif}")

    ds = gdal.Open(output2_tif)
    assert ds is not None

    assert ds.GetRasterBand(4).Checksum() == 22002, "Bad checksum band 0"

    ds = None


###############################################################################
# Test -white


def test_nearblack_4(nearblack_path, tmp_path):
    if test_cli_utilities.get_gdalwarp_path() is None:
        pytest.skip()

    src_tif = str(tmp_path / "nearblack4_src.tif")
    output_tif = str(tmp_path / "nearblack4.tif")

    gdaltest.runexternal(
        test_cli_utilities.get_gdalwarp_path()
        + f' -wo "INIT_DEST=255" ../gdrivers/data/rgbsmall.tif  {src_tif} -srcnodata 0'
    )
    gdaltest.runexternal(
        nearblack_path
        + f" -q -setalpha -white -nb 0 -of GTiff {src_tif} -o {output_tif}"
    )

    ds = gdal.Open(output_tif)
    assert ds is not None

    assert ds.GetRasterBand(4).Checksum() == 24151, "Bad checksum band 0"

    ds = None


###############################################################################
# Add mask band


def test_nearblack_5(nearblack_path, tmp_path):

    output_tif = str(tmp_path / "nearblack5.tif")
    output2_tif = str(tmp_path / "nearblack6.tif")

    gdaltest.runexternal(
        f"{nearblack_path} ../gdrivers/data/rgbsmall.tif --config GDAL_TIFF_INTERNAL_MASK NO -setmask -nb 0 -of GTiff -o {output_tif} -co TILED=YES"
    )

    ds = gdal.Open(output_tif)
    assert ds is not None

    assert (
        ds.GetRasterBand(1).GetMaskBand().Checksum() == 22002
    ), "Bad checksum mask band"

    ds = None

    # Set existing mask band

    shutil.copy(output_tif, output2_tif)
    shutil.copy(output_tif + ".msk", output2_tif + ".msk")

    gdaltest.runexternal(f"{nearblack_path} -setmask -nb 0 -of GTiff {output2_tif}")

    ds = gdal.Open(output2_tif)
    assert ds is not None

    assert (
        ds.GetRasterBand(1).GetMaskBand().Checksum() == 22002
    ), "Bad checksum mask band"

    ds = None


###############################################################################
# Test -color


def test_nearblack_7(nearblack_path, tmp_path):

    output_tif = str(tmp_path / "nearblack7.tif")

    gdaltest.runexternal(
        f"{nearblack_path} data/whiteblackred.tif -o {output_tif} -color 0,0,0 -color 255,255,255 -of GTiff"
    )

    ds = gdal.Open(output_tif)
    assert ds is not None

    assert (
        ds.GetRasterBand(1).Checksum() == 418
        and ds.GetRasterBand(2).Checksum() == 0
        and ds.GetRasterBand(3).Checksum() == 0
    ), "Bad checksum"

    ds = None


###############################################################################
# Test in-place update


def test_nearblack_8(nearblack_path, tmp_path):

    output_tif = str(tmp_path / "nearblack8.tif")

    src_ds = gdal.Open("../gdrivers/data/rgbsmall.tif")
    gdal.GetDriverByName("GTiff").CreateCopy(output_tif, src_ds)
    src_ds = None

    _, err = gdaltest.runexternal_out_and_err(f"{nearblack_path} {output_tif} -nb 0")
    assert err is None or err == "", "got error/warning"

    ds = gdal.Open(output_tif)
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 21106, "Bad checksum band 1"

    assert ds.GetRasterBand(2).Checksum() == 20736, "Bad checksum band 2"

    assert ds.GetRasterBand(3).Checksum() == 21309, "Bad checksum band 3"
