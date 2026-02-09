#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal raster contour' testing
# Author:   Alessandro Pasotti, <elpaso at itopen dot it>
#
###############################################################################
# Copyright (c) 2025, Alessandro Pasotti <elpaso at itopen dot it>
#
# SPDX-License-Identifier: MIT
###############################################################################

import gdaltest
import pytest

from osgeo import gdal, ogr


def get_contour_alg():
    reg = gdal.GetGlobalAlgorithmRegistry()
    raster = reg.InstantiateAlg("raster")
    return raster.InstantiateSubAlgorithm("contour")


@pytest.mark.require_driver("AAIGRID")
@pytest.mark.parametrize(
    "options, polygonize, expected_elev_values",
    [
        (
            ["--interval", "10", "--min-name", "ELEV_MIN", "--max-name", "ELEV_MAX"],
            True,
            [(4, 10), (10, 20), (20, 30), (30, 36)],
        ),
        (["--interval", "10", "--elevation-name", "ELEV"], False, [10.0, 20.0, 30.0]),
        (
            ["--interval", "10", "--elevation-name", "ELEV", "--offset", "5"],
            False,
            [5.0, 15.0, 25.0, 35.0],
        ),
        (
            ["--interval", "10", "--elevation-name", "ELEV", "--src-nodata", "4"],
            False,
            [20.0, 30.0],
        ),
        (
            ["--levels", "10", "--levels", "20", "--elevation-name", "ELEV"],
            False,
            [10.0, 20.0],
        ),
        (
            ["--levels", "10,20", "--elevation-name", "ELEV"],
            False,
            [10.0, 20.0],
        ),
        (
            ["--levels", "10", "--min-name", "ELEV_MIN", "--max-name", "ELEV_MAX"],
            True,
            [(10, 20)],
        ),
        (
            ["--exp-base", "10", "--min-name", "ELEV_MIN", "--max-name", "ELEV_MAX"],
            True,
            [(4, 10), (10, 36)],
        ),
        (
            [
                "--levels",
                "MIN,15,MAX",
                "--min-name",
                "ELEV_MIN",
                "--max-name",
                "ELEV_MAX",
            ],
            True,
            [(4, 15), (15, 36)],
        ),
        (
            [
                "--exp-base",
                "10",
                "--levels",
                "10",
                "--min-name",
                "ELEV_MIN",
                "--max-name",
                "ELEV_MAX",
                "--group-transactions",
                "1",
            ],
            True,
            "contour: Argument 'exp-base' is mutually exclusive with 'levels'.",
        ),
        (
            [
                "--interval",
                "10",
                "--levels",
                "10",
                "--min-name",
                "ELEV_MIN",
                "--max-name",
                "ELEV_MAX",
                "--group-transactions",
                "1",
            ],
            True,
            "contour: Argument 'levels' is mutually exclusive with 'interval'.",
        ),
        (
            [],
            True,
            "contour: One of 'interval', 'levels', 'exp-base' must be specified.",
        ),
        (
            ["--interval", "-10"],
            True,
            "Value of argument 'interval' is -10, but should be > 0",
        ),
    ],
)
def test_gdalalg_raster_contour(tmp_vsimem, options, polygonize, expected_elev_values):

    tmp_out_filename = str(tmp_vsimem / "out.shp")
    tmp_filename = str(tmp_vsimem / "tmp.asc")
    dem = """ncols        2
nrows        2
xllcorner    0
yllcorner    0
cellsize     1
4 15
25 36"""

    gdal.FileFromMemBuffer(tmp_filename, dem.encode("ascii"))

    alg = get_contour_alg()
    alg_options = [
        tmp_filename,
        tmp_out_filename,
    ]
    alg_options.extend(options)

    if polygonize:
        alg_options.append("--polygonize")

    if isinstance(expected_elev_values, str):
        with pytest.raises(RuntimeError):
            assert alg.ParseRunAndFinalize(alg_options)
        assert gdal.GetLastErrorMsg() == expected_elev_values

    else:
        assert alg.ParseRunAndFinalize(alg_options)

        with gdal.OpenEx(tmp_out_filename) as ds:
            lyr = ds.GetLayer()
            for i, feat in enumerate(lyr):
                geom = feat.GetGeometryRef()
                if polygonize:
                    assert geom.GetGeometryType() == ogr.wkbPolygon
                    assert feat.GetField("ELEV_MIN") == expected_elev_values[i][0]
                    assert feat.GetField("ELEV_MAX") == expected_elev_values[i][1]
                else:
                    assert geom.GetGeometryType() == ogr.wkbLineString
                    assert feat.GetField("ELEV") == expected_elev_values[i]
            lyr = None


@pytest.mark.require_driver("AAIGRID")
def test_gdalalg_raster_contour_overwrite(tmp_vsimem):

    tmp_out_filename = str(tmp_vsimem / "out.shp")
    tmp_filename = str(tmp_vsimem / "tmp.asc")
    dem = """ncols        2
nrows        2
xllcorner    0
yllcorner    0
cellsize     1
4 15
25 36"""

    gdal.FileFromMemBuffer(tmp_filename, dem.encode("ascii"))

    alg = get_contour_alg()
    alg_options = [
        tmp_filename,
        tmp_out_filename,
        "--interval",
        "10",
        "--min-name",
        "ELEV_MIN",
        "--max-name",
        "ELEV_MAX",
    ]

    assert alg.ParseRunAndFinalize(alg_options)

    # Run it again without --overwrite
    alg = get_contour_alg()
    with pytest.raises(RuntimeError, match="already exists"):
        alg.ParseRunAndFinalize(alg_options)

    # Run it again with --overwrite
    alg = get_contour_alg()
    alg_options.append("--overwrite")
    assert alg.ParseRunAndFinalize(alg_options)


@pytest.mark.require_driver("GPKG")
def test_gdalalg_raster_contour_creation_options(tmp_vsimem):

    out_filename = tmp_vsimem / "out.gpkg"

    alg = get_contour_alg()
    alg["input"] = "../gcore/data/byte.tif"
    alg["output"] = out_filename
    alg["interval"] = 10
    alg["creation-option"] = {"METADATA_TABLES": "YES"}
    alg["layer-creation-option"] = {"DESCRIPTION": "my_desc"}
    assert alg.Run()
    assert alg.Finalize()
    with ogr.Open(out_filename) as ds:
        with ds.ExecuteSQL(
            "SELECT * FROM sqlite_master WHERE name LIKE '%metadata%'"
        ) as sql_lyr:
            assert sql_lyr.GetFeatureCount() == 2
        assert ds.GetLayer(0).GetMetadata_Dict() == {"DESCRIPTION": "my_desc"}


@pytest.mark.require_driver("GPKG")
def test_gdalalg_raster_contour_all_nodata(tmp_vsimem):

    src_ds = gdal.GetDriverByName("MEM").Create("", 10, 10)
    src_ds.SetGeoTransform([0, 1, 0, 0, 0, -1])
    src_ds.GetRasterBand(1).SetNoDataValue(0)

    out_filename = tmp_vsimem / "out.gpkg"
    alg = get_contour_alg()
    alg["input"] = src_ds
    alg["output"] = out_filename
    alg["interval"] = 10
    with gdaltest.error_raised(gdal.CE_None):
        alg.Run()

    ds = alg.Output()
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 0


def test_gdalalg_raster_contour_pipeline_output_layer(tmp_vsimem):

    with gdal.alg.pipeline(
        pipeline="read ../gcore/data/byte.tif ! contour --interval 10 --output-layer foo"
    ) as alg:
        ds = alg.Output()
        assert ds.GetLayer(0).GetName() == "foo"
