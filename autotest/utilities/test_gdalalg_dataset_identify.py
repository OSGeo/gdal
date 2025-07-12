#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal dataset identify' testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import gdaltest
import pytest
import test_cli_utilities

from osgeo import gdal


def test_gdalalg_dataset_identify_json():

    with gdal.Run("dataset", "identify", filename="../gcore/data/byte.tif") as alg:
        assert alg.Output() == [{"driver": "GTiff", "name": "../gcore/data/byte.tif"}]


def test_gdalalg_dataset_identify_several_files_json():

    tab_pct = [0]

    def my_progress(pct, msg, user_data):
        assert pct >= tab_pct[0]
        tab_pct[0] = pct
        return True

    with gdal.Run(
        "dataset",
        "identify",
        filename=["../gcore/data/byte.tif", "../gcore/data/uint16.tif"],
        progress=my_progress,
    ) as alg:
        assert alg.Output() == [
            {"driver": "GTiff", "name": "../gcore/data/byte.tif"},
            {"driver": "GTiff", "name": "../gcore/data/uint16.tif"},
        ]

    assert tab_pct[0] == 1


def test_gdalalg_dataset_identify_stdout():

    gdal_path = test_cli_utilities.get_gdal_path()
    if gdal_path is None:
        pytest.skip("gdal binary missing")
    out = gdaltest.runexternal(f"{gdal_path} dataset identify data/utmsmall.tif")
    assert out.startswith("data/utmsmall.tif: GTiff")


def test_gdalalg_dataset_identify_complete():

    gdal_path = test_cli_utilities.get_gdal_path()
    if gdal_path is None:
        pytest.skip("gdal binary missing")
    out = gdaltest.runexternal(
        f"{gdal_path} completion gdal dataset identify data/whiteblackred"
    )
    assert out.replace("\\", "/") == "data/whiteblackred.tif"


def test_gdalalg_dataset_identify_text():

    with gdal.Run(
        "dataset", "identify", filename="../gcore/data/byte.tif", format="text"
    ) as alg:
        assert alg.Output() == "../gcore/data/byte.tif: GTiff\n"


def test_gdalalg_dataset_identify_report_failures_json():

    with gdal.Run(
        "dataset", "identify", filename="/i_do/not/exist", report_failures=True
    ) as alg:
        assert alg.Output() == [{"driver": None, "name": "/i_do/not/exist"}]


def test_gdalalg_dataset_identify_report_failures_text():

    with gdal.Run(
        "dataset",
        "identify",
        filename="/i_do/not/exist",
        report_failures=True,
        format="text",
    ) as alg:
        assert alg.Output() == "/i_do/not/exist: unrecognized\n"


def test_gdalalg_dataset_recursive():

    with gdal.Run(
        "dataset", "identify", filename="../utilities/data", recursive=True
    ) as alg:
        assert alg.Output() == [
            {"name": "../utilities/data", "driver": "ESRI Shapefile"}
        ]


def test_gdalalg_dataset_force_recursive():

    tab_pct = [0]

    def my_progress(pct, msg, user_data):
        assert pct >= tab_pct[0]
        tab_pct[0] = pct
        return True

    with gdal.Run(
        "dataset",
        "identify",
        filename="../utilities/data",
        force_recursive=True,
        progress=my_progress,
    ) as alg:
        assert len(alg.Output()) > 1

    assert tab_pct[0] == 1
