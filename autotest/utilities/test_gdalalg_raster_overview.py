#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal raster iveview' testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import pytest

from osgeo import gdal


def get_overview_alg():
    reg = gdal.GetGlobalAlgorithmRegistry()
    raster = reg.InstantiateAlg("raster")
    return raster.InstantiateSubAlgorithm("overview")


def get_overview_add_alg():
    return get_overview_alg().InstantiateSubAlgorithm("add")


def get_overview_delete_alg():
    return get_overview_alg().InstantiateSubAlgorithm("delete")


def test_gdalalg_overview_invalid_arguments():

    add = get_overview_add_alg()
    with pytest.raises(Exception):
        add.GetArg("levels").Set([1])

    add = get_overview_add_alg()
    with pytest.raises(Exception):
        add.GetArg("min-size").Set(0)


def test_gdalalg_overview_explicit_level():

    ds = gdal.Translate("", "../gcore/data/byte.tif", format="MEM")

    add = get_overview_add_alg()
    add.GetArg("dataset").Get().SetDataset(ds)
    add.GetArg("levels").Set([2])
    assert add.Run()

    assert ds.GetRasterBand(1).Checksum() == 4672
    assert ds.GetRasterBand(1).GetOverviewCount() == 1
    assert ds.GetRasterBand(1).GetOverview(0).Checksum() == 1087


def test_gdalalg_overview_minsize_and_resampling():

    ds = gdal.Translate("", "../gcore/data/byte.tif", format="MEM")

    add = get_overview_add_alg()
    add.GetArg("dataset").Get().SetDataset(ds)
    add.GetArg("resampling").Set("average")
    add.GetArg("min-size").Set(10)
    assert add.Run()

    assert ds.GetRasterBand(1).Checksum() == 4672
    assert ds.GetRasterBand(1).GetOverviewCount() == 1
    assert ds.GetRasterBand(1).GetOverview(0).Checksum() == 1152


def test_gdalalg_overview_reuse_resampling_and_levels(tmp_vsimem):

    tmp_filename = str(tmp_vsimem / "tmp.tif")
    ds = gdal.Translate(tmp_filename, "../gcore/data/byte.tif")

    add = get_overview_add_alg()
    add.GetArg("dataset").Get().SetDataset(ds)
    add.GetArg("resampling").Set("average")
    add.GetArg("min-size").Set(10)
    assert add.Run()

    assert ds.GetRasterBand(1).Checksum() == 4672
    assert ds.GetRasterBand(1).GetOverviewCount() == 1
    assert ds.GetRasterBand(1).GetOverview(0).GetMetadataItem("RESAMPLING") == "AVERAGE"
    assert ds.GetRasterBand(1).GetOverview(0).Checksum() == 1152

    ds.GetRasterBand(1).GetOverview(0).Fill(0)

    add = get_overview_add_alg()
    add.GetArg("dataset").Get().SetDataset(ds)
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
    add.GetArg("dataset").Get().SetDataset(ds)
    add.GetArg("resampling").Set("average")
    add.GetArg("min-size").Set(10)
    assert add.Run()

    assert ds.GetRasterBand(1).GetOverviewCount() == 1

    delete = get_overview_delete_alg()
    delete.GetArg("dataset").Get().SetDataset(ds)
    assert delete.Run()

    assert ds.GetRasterBand(1).GetOverviewCount() == 0
