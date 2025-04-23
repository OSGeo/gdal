#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Benchmarking of gdalwarp
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2023, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import gdaltest
import pytest

from osgeo import gdal, osr

# Must be set to run the test_XXX functions under the benchmark fixture
pytestmark = pytest.mark.usefixtures("decorate_with_benchmark")


@pytest.fixture()
def source_ds_filename(tmp_vsimem):
    filename = str(tmp_vsimem / "source.tif")
    if "debug" in gdal.VersionInfo(""):
        size = 1024
    else:
        size = 4096
    ds = gdal.GetDriverByName("GTiff").Create(
        filename, size, size, 3, options=["TILED=YES"]
    )
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(32631)
    ds.SetSpatialRef(srs)
    ds.SetGeoTransform([400000, 1, 0, 4500000, 0, -1])
    ds.GetRasterBand(1).Fill(1)
    ds.GetRasterBand(2).Fill(2)
    ds.GetRasterBand(3).Fill(3)
    ds = None
    return filename


@pytest.mark.parametrize("num_threads", ["1", "ALL_CPUS"])
@pytest.mark.parametrize("resample_alg", ["near", "cubic"])
def test_gdalwarp(tmp_vsimem, source_ds_filename, num_threads, resample_alg):
    filename = str(tmp_vsimem / "test_gdalwarp.tif")
    if gdal.VSIStatL(filename):
        gdal.Unlink(filename)
    with gdaltest.config_option("GDAL_NUM_THREADS", num_threads):
        gdal.Warp(
            filename,
            source_ds_filename,
            options=f"-co TILED=YES -r {resample_alg} -t_srs EPSG:4326",
        )
