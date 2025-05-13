#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal raster overview' testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import gdaltest
import pytest

from osgeo import gdal


def get_overview_alg():
    return gdal.GetGlobalAlgorithmRegistry()["raster"]["overview"]


def get_overview_add_alg():
    return get_overview_alg()["add"]


def get_overview_delete_alg():
    return get_overview_alg()["delete"]


def test_gdalalg_overview_invalid_arguments():

    add = get_overview_add_alg()
    with pytest.raises(Exception):
        add["levels"] = [1]

    add = get_overview_add_alg()
    with pytest.raises(Exception):
        add["min-size"] = 0


def test_gdalalg_overview_explicit_level():

    ds = gdal.Translate("", "../gcore/data/byte.tif", format="MEM")

    add = get_overview_add_alg()
    add["dataset"] = ds
    add["levels"] = [2]
    assert add.Run()

    assert ds.GetRasterBand(1).Checksum() == 4672
    assert ds.GetRasterBand(1).GetOverviewCount() == 1
    assert ds.GetRasterBand(1).GetOverview(0).Checksum() == 1087


def test_gdalalg_overview_minsize_and_resampling():

    ds = gdal.Translate("", "../gcore/data/byte.tif", format="MEM")

    add = get_overview_add_alg()
    add["dataset"] = ds
    add["resampling"] = "average"
    add["min-size"] = 10
    assert add.Run()

    assert ds.GetRasterBand(1).Checksum() == 4672
    assert ds.GetRasterBand(1).GetOverviewCount() == 1
    assert ds.GetRasterBand(1).GetOverview(0).Checksum() == 1152


def test_gdalalg_overview_reuse_resampling_and_levels(tmp_vsimem):

    tmp_filename = str(tmp_vsimem / "tmp.tif")
    ds = gdal.Translate(tmp_filename, "../gcore/data/byte.tif")

    add = get_overview_add_alg()
    add["dataset"] = ds
    add["resampling"] = "average"
    add["min-size"] = 10
    assert add.Run()

    assert ds.GetRasterBand(1).Checksum() == 4672
    assert ds.GetRasterBand(1).GetOverviewCount() == 1
    assert ds.GetRasterBand(1).GetOverview(0).GetMetadataItem("RESAMPLING") == "AVERAGE"
    assert ds.GetRasterBand(1).GetOverview(0).Checksum() == 1152

    ds.GetRasterBand(1).GetOverview(0).Fill(0)

    add = get_overview_add_alg()
    add["dataset"] = ds
    assert add.Run()

    assert ds.GetRasterBand(1).Checksum() == 4672
    assert ds.GetRasterBand(1).GetOverviewCount() == 1
    assert ds.GetRasterBand(1).GetOverview(0).GetMetadataItem("RESAMPLING") == "AVERAGE"
    assert ds.GetRasterBand(1).GetOverview(0).Checksum() == 1152


def test_gdalalg_overview_in_plae(tmp_vsimem):

    tmp_filename = str(tmp_vsimem / "tmp.tif")
    gdal.Translate(tmp_filename, "../gcore/data/byte.tif")

    add = get_overview_add_alg()
    assert add.ParseRunAndFinalize([tmp_filename, "--levels=2"])

    assert gdal.VSIStatL(tmp_filename + ".ovr") is None

    with gdal.Open(tmp_filename) as ds:
        assert ds.GetRasterBand(1).GetOverviewCount() == 1


def test_gdalalg_overview_external(tmp_vsimem):

    tmp_filename = str(tmp_vsimem / "tmp.tif")
    gdal.Translate(tmp_filename, "../gcore/data/byte.tif")

    add = get_overview_add_alg()
    assert add.ParseRunAndFinalize([tmp_filename, "--levels=2", "--external"])

    assert gdal.VSIStatL(tmp_filename + ".ovr") is not None

    with gdal.Open(tmp_filename) as ds:
        assert ds.GetRasterBand(1).GetOverviewCount() == 1

    delete = get_overview_delete_alg()
    assert delete.ParseRunAndFinalize([tmp_filename, "--external"])

    assert gdal.VSIStatL(tmp_filename + ".ovr") is None


def test_gdalalg_overview_delete():

    ds = gdal.Translate("", "../gcore/data/byte.tif", format="MEM")

    add = get_overview_add_alg()
    add["dataset"] = ds
    add["resampling"] = "average"
    add["min-size"] = 10
    assert add.Run()

    assert ds.GetRasterBand(1).GetOverviewCount() == 1

    delete = get_overview_delete_alg()
    delete["dataset"] = ds
    assert delete.Run()

    assert ds.GetRasterBand(1).GetOverviewCount() == 0


@pytest.mark.require_driver("COG")
def test_gdalalg_overview_cog(tmp_vsimem):

    src_ds = gdal.GetDriverByName("MEM").Create("", 1024, 1024)
    filename = tmp_vsimem / "my_cog.tif"
    gdal.GetDriverByName("COG").CreateCopy(filename, src_ds)

    with gdal.Open(filename) as ds:
        assert ds.GetRasterBand(1).GetOverviewCount() > 0

    delete = get_overview_delete_alg()
    delete["dataset"] = filename
    assert delete.Run()
    assert delete.Finalize()

    with gdal.Open(filename) as ds:
        assert ds.GetRasterBand(1).GetOverviewCount() == 0

    add = get_overview_add_alg()
    add["dataset"] = filename
    with pytest.raises(
        Exception, match=r"has C\(loud\) O\(ptimized\) G\(eoTIFF\) layout"
    ):
        add.Run()

    add = get_overview_add_alg()
    add["dataset"] = filename
    add["open-option"] = {"IGNORE_COG_LAYOUT_BREAK": "YES"}
    with gdaltest.error_raised(gdal.CE_Warning):
        assert add.Run()
    assert delete.Finalize()

    with gdal.Open(filename) as ds:
        assert ds.GetRasterBand(1).GetOverviewCount() > 0
