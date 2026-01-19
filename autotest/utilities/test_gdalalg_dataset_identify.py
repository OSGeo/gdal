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

import json

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


def test_gdalalg_dataset_identify_recursive():

    with gdal.Run(
        "dataset", "identify", filename="../utilities/data", recursive=True
    ) as alg:
        assert alg.Output() == [
            {"name": "../utilities/data", "driver": "ESRI Shapefile"}
        ]


def test_gdalalg_dataset_identify_force_recursive():

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


def test_gdalalg_dataset_identify_json_output_file(tmp_vsimem):

    gdal.Run(
        "dataset",
        "identify",
        filename="../gcore/data/byte.tif",
        format="json",
        output=tmp_vsimem / "out.json",
    )
    with gdal.VSIFile(tmp_vsimem / "out.json", "rb") as f:
        j = json.loads(f.read())
        assert j == [
            {
                "driver": "GTiff",
                "name": "../gcore/data/byte.tif",
            },
        ]


def test_gdalalg_dataset_identify_text_output_file(tmp_vsimem):

    with pytest.raises(
        Exception, match="identify: Cannot create '/i_do/not/exist.json'"
    ):
        gdal.Run(
            "dataset",
            "identify",
            filename="../gcore/data/byte.tif",
            format="text",
            output="/i_do/not/exist.json",
        )

    gdal.Run(
        "dataset",
        "identify",
        filename="../gcore/data/byte.tif",
        format="text",
        output=tmp_vsimem / "out.json",
    )
    with gdal.VSIFile(tmp_vsimem / "out.json", "rb") as f:
        assert f.read() == b"../gcore/data/byte.tif: GTiff\n"


def test_gdalalg_dataset_identify_json_output_detailed():

    with gdal.Run(
        "dataset",
        "identify",
        filename="../gcore/data/gtiff/byte_DEFLATE_tiled.tif",
        format="json",
        detailed=True,
    ) as alg:
        assert alg.Output() == [
            {
                "name": "../gcore/data/gtiff/byte_DEFLATE_tiled.tif",
                "driver": "GTiff",
                "layout": "COG",
                "file_list": ["../gcore/data/gtiff/byte_DEFLATE_tiled.tif"],
                "has_crs": True,
                "has_geotransform": True,
            }
        ]


def test_gdalalg_dataset_identify_text_output_detailed():

    with gdal.Run(
        "dataset",
        "identify",
        filename="../gcore/data/gtiff/byte_DEFLATE_tiled.tif",
        format="text",
        detailed=True,
    ) as alg:
        assert (
            alg.Output()
            == "../gcore/data/gtiff/byte_DEFLATE_tiled.tif: GTiff, layout=COG, has CRS, has geotransform\n"
        )


@pytest.mark.require_driver("CSV")
def test_gdalalg_dataset_identify_csv_output_output(tmp_vsimem):

    with pytest.raises(
        Exception,
        match="identify: 'output' argument must be specified for non-text or non-json output",
    ):
        gdal.Run(
            "dataset",
            "identify",
            filename="../gcore/data/gtiff/byte_DEFLATE_tiled.tif",
            format="CSV",
        )

    with pytest.raises(Exception):
        gdal.Run(
            "dataset",
            "identify",
            filename="../gcore/data/gtiff/byte_DEFLATE_tiled.tif",
            format="CSV",
            output="/i_do/not/exist.csv",
        )

    with gdal.Run(
        "dataset",
        "identify",
        filename="../gcore/data/gtiff/byte_DEFLATE_tiled.tif",
        output=tmp_vsimem / "out.csv",
    ) as alg:
        ds = alg.Output()
        lyr = ds.GetLayer(0)
        f = lyr.GetNextFeature()
        assert f["filename"] == "../gcore/data/gtiff/byte_DEFLATE_tiled.tif"
        assert f["driver"] == "GTiff"

    with pytest.raises(
        Exception, match="already exists. You may specify the --overwrite option"
    ):
        gdal.Run(
            "dataset",
            "identify",
            filename="../gcore/data/gtiff/byte_DEFLATE_tiled.tif",
            format="CSV",
            output=tmp_vsimem / "out.csv",
        )

    with gdal.Run(
        "dataset",
        "identify",
        filename="../gcore/data/gtiff/byte_DEFLATE_tiled.tif",
        format="CSV",
        output=tmp_vsimem / "out.csv",
        overwrite=True,
    ) as alg:
        ds = alg.Output()
        lyr = ds.GetLayer(0)
        f = lyr.GetNextFeature()
        assert f["filename"] == "../gcore/data/gtiff/byte_DEFLATE_tiled.tif"
        assert f["driver"] == "GTiff"


def test_gdalalg_dataset_identify_output_cannot_guess_fromat(tmp_vsimem):

    with pytest.raises(Exception, match="Cannot guess driver"):
        gdal.Run(
            "dataset",
            "identify",
            filename="../gcore/data/gtiff/byte_DEFLATE_tiled.tif",
            output=tmp_vsimem / "unknown.extension",
        )


def test_gdalalg_dataset_identify_mem_output_detailed():

    with gdal.Run(
        "dataset",
        "identify",
        filename="../gcore/data/gtiff/byte_DEFLATE_tiled.tif",
        format="MEM",
        detailed=True,
    ) as alg:
        ds = alg.Output()
        lyr = ds.GetLayer(0)
        f = lyr.GetNextFeature()
        assert f["filename"] == "../gcore/data/gtiff/byte_DEFLATE_tiled.tif"
        assert f["driver"] == "GTiff"
        assert f["file_list"] == ["../gcore/data/gtiff/byte_DEFLATE_tiled.tif"]
        assert f["layout"] == "COG"
        assert f["has_crs"] is True
        assert f["has_geotransform"] is True
        assert f["has_overview"] is False


def test_gdalalg_dataset_identify_mem_output_report_failures():

    with gdal.Run(
        "dataset",
        "identify",
        filename="/i_do/not/exist",
        format="MEM",
        report_failures=True,
    ) as alg:
        ds = alg.Output()
        lyr = ds.GetLayer(0)
        f = lyr.GetNextFeature()
        assert f["filename"] == "/i_do/not/exist"
        assert f["driver"] is None
