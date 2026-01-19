#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal raster viewshed' testing
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


def get_alg():
    return gdal.GetGlobalAlgorithmRegistry()["raster"]["viewshed"]


@pytest.fixture()
def viewshed_input(tmp_path):

    fname = str(tmp_path / "test_gdal_viewshed_in.tif")

    ds = gdal.Warp(fname, "../gdrivers/data/n43.tif", dstSRS="EPSG:32617")
    ds.GetRasterBand(1).DeleteNoDataValue()
    ds.Close()

    return fname


@pytest.fixture()
def viewshed_sd_input(tmp_path):
    fname = str(tmp_path / "test_gdal_viewshed_sd_in.tif")

    ds = gdal.Warp(fname, "../gdrivers/data/n43d-sd.tif", dstSRS="EPSG:32617")
    ds.GetRasterBand(1).DeleteNoDataValue()
    ds.Close()
    return fname


VIEWSHED_NOMINAL_CHECKSUM = 14695


def test_gdalalg_raster_viewshed(viewshed_input):

    last_pct = [0]

    def my_progress(pct, msg, user_data):
        last_pct[0] = pct
        return True

    alg = get_alg()
    alg["input"] = viewshed_input
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["position"] = [621528, 4817617, 100]
    assert alg.Run(my_progress)
    assert last_pct[0] == 1.0
    ds = alg["output"].GetDataset()
    assert ds.GetRasterBand(1).GetNoDataValue() is None
    assert ds.GetRasterBand(1).Checksum() == VIEWSHED_NOMINAL_CHECKSUM


def test_gdalalg_raster_viewshed_overwrite_and_creation_option(
    viewshed_input, tmp_vsimem
):

    out_filename = tmp_vsimem / "out.tif"

    alg = get_alg()
    alg["input"] = viewshed_input
    alg["output"] = out_filename
    alg["position"] = [621528, 4817617, 100]
    alg["creation-option"] = {"COMPRESS": "LZW"}
    assert alg.Run()
    assert alg.Finalize()

    with gdal.Open(out_filename, gdal.GA_Update) as ds:
        assert ds.GetMetadataItem("COMPRESSION", "IMAGE_STRUCTURE") == "LZW"
        ds.GetRasterBand(1).Fill(0)

    alg = get_alg()
    alg["input"] = viewshed_input
    alg["output"] = out_filename
    alg["position"] = [621528, 4817617, 100]
    with pytest.raises(
        Exception,
        match="already exists",
    ):
        alg.Run()

    alg = get_alg()
    alg["input"] = viewshed_input
    alg["output"] = out_filename
    alg["position"] = [621528, 4817617, 100]
    alg["overwrite"] = True
    assert alg.Run()
    ds = alg["output"].GetDataset()
    assert ds.GetRasterBand(1).Checksum() == VIEWSHED_NOMINAL_CHECKSUM


def test_gdalalg_raster_viewshed_target_height(viewshed_input):

    alg = get_alg()
    alg["input"] = viewshed_input
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["position"] = [621528, 4817617, 100]
    alg["target-height"] = 20
    assert alg.Run()
    ds = alg["output"].GetDataset()
    assert ds.GetRasterBand(1).Checksum() not in (-1, 0, VIEWSHED_NOMINAL_CHECKSUM)


def test_gdalalg_raster_viewshed_max_distance_and_out_of_range_value(viewshed_input):

    alg = get_alg()
    alg["input"] = viewshed_input
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["position"] = [621528, 4817617, 100]
    alg["max-distance"] = 2000
    assert alg.Run()
    ds = alg["output"].GetDataset()
    cs = ds.GetRasterBand(1).Checksum()
    assert cs not in (-1, 0, VIEWSHED_NOMINAL_CHECKSUM)

    alg = get_alg()
    alg["input"] = viewshed_input
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["position"] = [621528, 4817617, 100]
    alg["max-distance"] = 2000
    alg["out-of-range-value"] = 1
    assert alg.Run()
    ds = alg["output"].GetDataset()
    assert ds.GetRasterBand(1).Checksum() not in (-1, 0, VIEWSHED_NOMINAL_CHECKSUM, cs)


def test_gdalalg_raster_viewshed_curvature_coefficient(viewshed_input):

    alg = get_alg()
    alg["input"] = viewshed_input
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["position"] = [621528, 4817617, 100]
    alg["curvature-coefficient"] = 1
    assert alg.Run()
    ds = alg["output"].GetDataset()
    assert ds.GetRasterBand(1).Checksum() not in (-1, 0, VIEWSHED_NOMINAL_CHECKSUM)


def test_gdalalg_raster_viewshed_band(viewshed_input):

    alg = get_alg()
    alg["input"] = viewshed_input
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["position"] = [621528, 4817617, 100]
    alg["band"] = 2
    with pytest.raises(
        Exception,
        match="Value of 'band' should be greater or equal than 1 and less or equal than 1",
    ):
        alg.Run()


def test_gdalalg_raster_viewshed_visible_value(viewshed_input):

    alg = get_alg()
    alg["input"] = viewshed_input
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["position"] = [621528, 4817617, 100]
    alg["visible-value"] = 1
    assert alg.Run()
    ds = alg["output"].GetDataset()
    assert ds.GetRasterBand(1).Checksum() not in (-1, 0, VIEWSHED_NOMINAL_CHECKSUM)


def test_gdalalg_raster_viewshed_invisible_value(viewshed_input):

    alg = get_alg()
    alg["input"] = viewshed_input
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["position"] = [621528, 4817617, 100]
    alg["invisible-value"] = 1
    assert alg.Run()
    ds = alg["output"].GetDataset()
    assert ds.GetRasterBand(1).Checksum() not in (-1, 0, VIEWSHED_NOMINAL_CHECKSUM)


def test_gdalalg_raster_viewshed_nodata(viewshed_input):

    alg = get_alg()
    alg["input"] = viewshed_input
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["position"] = [621528, 4817617, 100]
    alg["dst-nodata"] = 0
    assert alg.Run()
    ds = alg["output"].GetDataset()
    assert ds.GetRasterBand(1).Checksum() == VIEWSHED_NOMINAL_CHECKSUM
    assert ds.GetRasterBand(1).GetNoDataValue() == 0


def test_gdalalg_raster_mode_cumulative(viewshed_input, tmp_vsimem):

    tmp_filename = tmp_vsimem / "out.tif"

    alg = get_alg()
    alg["input"] = viewshed_input
    alg["output"] = tmp_filename
    alg["height"] = 100
    alg["mode"] = "cumulative"
    assert alg.Run()
    with gdal.Open(tmp_filename) as ds:
        # approx for ARM64 and Intel compiler
        assert ds.GetRasterBand(1).Checksum() == pytest.approx(36560, abs=1)

    alg = get_alg()
    alg["input"] = viewshed_input
    alg["output"] = tmp_filename
    alg["height"] = 100
    alg["mode"] = "cumulative"
    alg["overwrite"] = True
    alg["observer-spacing"] = 20
    assert alg.Run()
    with gdal.Open(tmp_filename) as ds:
        # approx for ARM64 and Intel compiler
        assert ds.GetRasterBand(1).Checksum() == pytest.approx(30941, abs=10)

    alg = get_alg()
    alg["input"] = gdal.GetDriverByName("MEM").Create("", 1, 1)
    alg["output"] = tmp_filename
    alg["height"] = 100
    alg["mode"] = "cumulative"
    alg["overwrite"] = True
    with pytest.raises(
        Exception, match="In cumulative mode, the input dataset must be opened by name"
    ):
        alg.Run()


def test_gdalalg_raster_mode_dem(viewshed_input):

    alg = get_alg()
    alg["input"] = viewshed_input
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["position"] = [621528, 4817617, 100]
    alg["mode"] = "DEM"
    assert alg.Run()
    ds = alg["output"].GetDataset()
    assert ds.GetRasterBand(1).Checksum() == 47060


def test_gdalalg_raster_mode_ground(viewshed_input):

    alg = get_alg()
    alg["input"] = viewshed_input
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["position"] = [621528, 4817617, 100]
    alg["mode"] = "ground"
    assert alg.Run()
    ds = alg["output"].GetDataset()
    assert ds.GetRasterBand(1).Checksum() == 8381


def test_gdalalg_raster_sd(viewshed_input, viewshed_sd_input):

    # Some test configurations don't support numpy. This will skip those
    # configurations.
    np = pytest.importorskip("numpy")
    gdaltest.importorskip_gdal_array()

    alg = get_alg()
    alg["input"] = viewshed_input
    alg["sd-filename"] = viewshed_sd_input
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["position"] = [621528, 4817617, 100]
    alg["mode"] = "normal"
    alg["maybe-visible-value"] = 3
    assert alg.Run()
    ds = alg["output"].GetDataset()
    array = ds.ReadAsArray()
    values, counts = np.unique(array, return_counts=True)
    assert len(values) == 3
    assert values[0] == 0
    assert values[1] == 3
    assert values[2] == 255
    assert counts[0] == 5815
    assert counts[1] == 2020
    assert counts[2] == 6585
    assert ds.GetRasterBand(1).Checksum() == 20755


def test_gdalalg_raster_wrong_sd(viewshed_input):

    alg = get_alg()
    alg["input"] = viewshed_input
    alg["sd-filename"] = gdal.GetDriverByName("MEM").Create("", 0, 0, 0)
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["position"] = [621528, 4817617, 100]
    alg["mode"] = "normal"
    alg["maybe-visible-value"] = 3
    with pytest.raises(
        Exception, match="The standard deviation dataset must have one raster band"
    ):
        alg.Run()
