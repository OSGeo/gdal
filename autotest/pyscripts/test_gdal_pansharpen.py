#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdal_pansharpen testing
# Author:   Even Rouault <even.rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2015, Even Rouault <even.rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import gdaltest
import pytest
import test_py_scripts

from osgeo import gdal

pytestmark = pytest.mark.skipif(
    test_py_scripts.get_py_script("gdal_pansharpen") is None,
    reason="gdal_pansharpen not available",
)


@pytest.fixture()
def script_path():
    return test_py_scripts.get_py_script("gdal_pansharpen")


@pytest.fixture(scope="module")
def small_world_pan_tif(tmp_path_factory):

    small_world_pan_tif = str(tmp_path_factory.mktemp("tmp") / "small_world_pan.tif")

    with gdal.Open(
        test_py_scripts.get_data_path("gdrivers") + "small_world.tif"
    ) as src_ds:
        src_data = src_ds.GetRasterBand(1).ReadRaster()
        gt = src_ds.GetGeoTransform()
        wkt = src_ds.GetProjectionRef()

    with gdal.GetDriverByName("GTiff").Create(small_world_pan_tif, 800, 400) as pan_ds:
        gt = [gt[i] for i in range(len(gt))]
        gt[1] *= 0.5
        gt[5] *= 0.5
        pan_ds.SetGeoTransform(gt)
        pan_ds.SetProjection(wkt)
        pan_ds.GetRasterBand(1).WriteRaster(0, 0, 800, 400, src_data, 400, 200)

    return small_world_pan_tif


###############################################################################
#


def test_gdal_pansharpen_help(script_path):

    assert "ERROR" not in test_py_scripts.run_py_script(
        script_path, "gdal_pansharpen", "--help"
    )


###############################################################################
#


def test_gdal_pansharpen_version(script_path):

    assert "ERROR" not in test_py_scripts.run_py_script(
        script_path, "gdal_pansharpen", "--version"
    )


###############################################################################
# Simple test


@pytest.mark.skipif(
    not gdaltest.vrt_has_open_support(),
    reason="VRT driver open missing",
)
def test_gdal_pansharpen_1(script_path, tmp_path, small_world_pan_tif):

    out_tif = str(tmp_path / "out.tif")

    _, err = test_py_scripts.run_py_script(
        script_path,
        "gdal_pansharpen",
        f" {small_world_pan_tif} "
        + test_py_scripts.get_data_path("gdrivers")
        + "small_world.tif "
        + out_tif,
        return_stderr=True,
    )
    assert "UseExceptions" not in err

    with gdal.Open(out_tif) as ds:
        cs = [ds.GetRasterBand(i + 1).Checksum() for i in range(ds.RasterCount)]

    assert cs in ([4735, 10000, 9742], [4731, 9991, 9734])  # s390x or graviton2


###############################################################################
# Full options


@pytest.mark.skipif(
    not gdaltest.vrt_has_open_support(),
    reason="VRT driver open missing",
)
def test_gdal_pansharpen_2(script_path, tmp_path, small_world_pan_tif):

    out_vrt = str(tmp_path / "out.vrt")

    test_py_scripts.run_py_script(
        script_path,
        "gdal_pansharpen",
        " -q -b 3 -b 1 -bitdepth 8 -threads ALL_CPUS -spat_adjust union -w 0.33333333333333333 -w 0.33333333333333333 -w 0.33333333333333333 -of VRT -r cubic "
        + small_world_pan_tif
        + " "
        + test_py_scripts.get_data_path("gdrivers")
        + "small_world.tif,band=1 "
        + test_py_scripts.get_data_path("gdrivers")
        + "small_world.tif,band=2 "
        + test_py_scripts.get_data_path("gdrivers")
        + "small_world.tif,band=3 "
        + out_vrt,
    )

    with gdal.Open(out_vrt) as ds:
        cs = [ds.GetRasterBand(i + 1).Checksum() for i in range(ds.RasterCount)]

    assert cs in (
        [9742, 4735],
        [9734, 4731],  # s390x or graviton2
        [9727, 4726],  # ICC 2004.0.2 in -O3
    )
