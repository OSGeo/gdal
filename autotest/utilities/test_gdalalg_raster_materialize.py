#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  Testing of 'materialize' step in 'gdal raster pipeline'
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import json
import sys

import pytest

from osgeo import gdal


def _get_cleaned_list(lst):
    if not lst:
        return []
    ret = []
    for elt in lst:
        if elt != "." and elt != "..":
            ret.append(elt)
    return ret


def test_gdalalg_raster_materialize_temp_output_tif(tmp_path):

    tab_pct = [0]

    def my_progress(pct, msg, user_data):
        assert pct >= tab_pct[0]
        tab_pct[0] = pct
        return True

    with gdal.config_option("CPL_TMPDIR", str(tmp_path)):
        with gdal.Run(
            "raster",
            "pipeline",
            pipeline="read ../gcore/data/byte.tif ! materialize ! write --of stream streamed_dataset",
            progress=my_progress,
        ) as alg:
            if sys.platform != "win32":
                assert _get_cleaned_list(gdal.ReadDir(tmp_path)) == []
            assert tab_pct[0] == 1.0
            with alg.Output() as ds:
                assert ds.GetRasterBand(1).Checksum() == 4672

    assert _get_cleaned_list(gdal.ReadDir(tmp_path)) == []


def test_gdalalg_raster_materialize_temp_output_mem(tmp_path):

    with gdal.config_option("CPL_TMPDIR", str(tmp_path)):
        with gdal.Run(
            "raster",
            "pipeline",
            pipeline="read ../gcore/data/byte.tif ! materialize --format MEM ! write --of stream streamed_dataset",
        ) as alg:
            assert _get_cleaned_list(gdal.ReadDir(tmp_path)) == []
            with alg.Output() as ds:
                assert ds.GetDriver().GetDescription() == "MEM"
                assert ds.GetRasterBand(1).Checksum() == 4672

    assert _get_cleaned_list(gdal.ReadDir(tmp_path)) == []


@pytest.mark.require_driver("COG")
def test_gdalalg_raster_materialize_temp_output_cog(tmp_path):

    with gdal.config_option("CPL_TMPDIR", str(tmp_path)):
        with gdal.Run(
            "raster",
            "pipeline",
            pipeline="read ../gcore/data/byte.tif ! materialize --format COG ! write --of stream streamed_dataset",
        ) as alg:
            if sys.platform != "win32":
                assert _get_cleaned_list(gdal.ReadDir(tmp_path)) == []
            with alg.Output() as ds:
                assert ds.GetRasterBand(1).Checksum() == 4672

    assert _get_cleaned_list(gdal.ReadDir(tmp_path)) == []


@pytest.mark.require_driver("GPKG")
def test_gdalalg_raster_materialize_temp_output_gpkg(tmp_path):

    with gdal.config_option("CPL_TMPDIR", str(tmp_path)):
        with gdal.Run(
            "raster",
            "pipeline",
            pipeline="read ../gcore/data/byte.tif ! materialize --format GPKG ! write --of stream streamed_dataset",
        ) as alg:
            with alg.Output() as ds:
                assert ds.GetDriver().GetDescription() == "GPKG"
                assert ds.GetRasterBand(1).Checksum() == 4672

    assert _get_cleaned_list(gdal.ReadDir(tmp_path)) == []


def test_gdalalg_raster_materialize_manual_output(tmp_path):

    with gdal.Run(
        "raster",
        "pipeline",
        pipeline=f"read ../gcore/data/byte.tif ! materialize --output={tmp_path}/tmp.tif ! write --of stream streamed_dataset",
    ) as alg:
        with alg.Output() as ds:
            assert ds.GetRasterBand(1).Checksum() == 4672

    with gdal.Open(tmp_path / "tmp.tif") as ds:
        assert ds.GetRasterBand(1).Checksum() == 4672


@pytest.mark.require_driver("GDALG")
def test_gdalalg_raster_materialize_read_from_gdalg():
    with gdal.Open(
        json.dumps(
            {
                "type": "gdal_streamed_alg",
                "command_line": "gdal pipeline read ../gcore/data/byte.tif ! materialize",
            }
        )
    ) as ds:
        assert ds.GetRasterBand(1).Checksum() == 4672


@pytest.mark.require_driver("GDALG")
def test_gdalalg_raster_materialize_read_from_gdalg_error():
    with pytest.raises(
        Exception,
        match="Step 'materialize' not allowed in stream execution, unless the GDAL_ALGORITHM_ALLOW_WRITES_IN_STREAM configuration option is set",
    ):
        gdal.Open(
            json.dumps(
                {
                    "type": "gdal_streamed_alg",
                    "command_line": "gdal pipeline read ../gcore/data/byte.tif ! materialize --output=illegal",
                }
            )
        )
